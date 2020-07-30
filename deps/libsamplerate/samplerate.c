/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/erikd/libsamplerate/blob/master/COPYING
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef int32_t increment_t ;
typedef double	coeff_t ;

#include "fastest_coeffs.h"
// #include "mid_qual_coeffs.h"

#define SRC_MAX_RATIO            256

#define MAX(a,b)                 (((a) > (b)) ? (a) : (b))
#define MIN(a,b)                 (((a) < (b)) ? (a) : (b))

#define ARRAY_LEN(x)             ((int) (sizeof (x) / sizeof ((x) [0])))

#define MAKE_INCREMENT_T(x)       ((increment_t) (x))

#define SHIFT_BITS                12
#define FP_ONE                    ((double) (((increment_t) 1) << SHIFT_BITS))
#define INV_FP_ONE                (1.0 / FP_ONE)

/* Opaque data type SRC_STATE. */
typedef struct SRC_STATE SRC_STATE ;

enum
{
	SRC_SINC_MEDIUM_QUALITY		= 1,
	SRC_SINC_FASTEST			= 2,
} ;

enum
{	SRC_ERR_NO_ERROR = 0,

	SRC_ERR_MALLOC_FAILED,
	SRC_ERR_BAD_STATE,
	SRC_ERR_BAD_DATA,
	SRC_ERR_BAD_DATA_PTR,
	SRC_ERR_NO_PRIVATE,
	SRC_ERR_BAD_SRC_RATIO,
	SRC_ERR_BAD_PROC_PTR,
	SRC_ERR_FILTER_LEN,
	SRC_ERR_BAD_CONVERTER,
	SRC_ERR_BAD_CHANNEL_COUNT,
	SRC_ERR_DATA_OVERLAP,
	SRC_ERR_SINC_PREPARE_DATA_BAD_LEN,
	SRC_ERR_BAD_INTERNAL_STATE,
} ;

typedef struct
{	const float	*data_in ;
	float	 *data_out ;

	long	input_frames, output_frames ;
	long	input_frames_used, output_frames_gen ;

	int		end_of_input ;

	double	src_ratio ;
} SRC_DATA ;

typedef struct SRC_PRIVATE {
	double	last_ratio, last_position ;

	int		error ;
	int		channels ;

	/* Pointer to data to converter specific data. */
	void	*private_data ;

	/* Varispeed process function. */
	int		(*vari_process) (struct SRC_PRIVATE *psrc, SRC_DATA *data) ;

	/* Constant speed process function. */
	int		(*const_process) (struct SRC_PRIVATE *psrc, SRC_DATA *data) ;

	/* State reset. */
	void	(*reset) (struct SRC_PRIVATE *psrc) ;
} SRC_PRIVATE ;

typedef struct {
	int		channels ;
	long	in_count, in_used ;
	long	out_count, out_gen ;

	int		coeff_half_len, index_inc ;

	double	src_ratio, input_index ;

	coeff_t const	*coeffs ;

	int		b_current, b_end, b_real_end, b_len ;

	/* Sure hope noone does more than 128 channels at once. */
	double left_calc [128], right_calc [128] ;

	/* C99 struct flexible array. */
	float	buffer [1] ;
} SINC_FILTER ;

static increment_t
double_to_fp (double x)
{	return (lrint ((x) * FP_ONE)) ;
} /* double_to_fp */

static increment_t
int_to_fp (int x)
{	return (((increment_t) (x)) << SHIFT_BITS) ;
} /* int_to_fp */

static int
fp_to_int (increment_t x)
{	return (((x) >> SHIFT_BITS)) ;
} /* fp_to_int */

static increment_t
fp_fraction_part (increment_t x)
{	return ((x) & ((((increment_t) 1) << SHIFT_BITS) - 1)) ;
} /* fp_fraction_part */

static double
fp_to_double (increment_t x)
{	return fp_fraction_part (x) * INV_FP_ONE ;
} /* fp_to_double */

static double
fmod_one (double x)
{	double res ;

	res = x - lrint (x) ;
	if (res < 0.0)
		return res + 1.0 ;

	return res ;
} /* fmod_one */

static int
is_bad_src_ratio (double ratio)
{	return (ratio < (1.0 / SRC_MAX_RATIO) || ratio > (1.0 * SRC_MAX_RATIO)) ;
} /* is_bad_src_ratio */

static void
sinc_reset (SRC_PRIVATE *psrc)
{	SINC_FILTER *filter ;

	filter = (SINC_FILTER*) psrc->private_data ;
	if (filter == NULL)
		return ;

	filter->b_current = filter->b_end = 0 ;
	filter->b_real_end = -1 ;

	filter->src_ratio = filter->input_index = 0.0 ;

	memset (filter->buffer, 0, filter->b_len * sizeof (filter->buffer [0])) ;

	/* Set this for a sanity check */
	memset (filter->buffer + filter->b_len, 0xAA, filter->channels * sizeof (filter->buffer [0])) ;
} /* sinc_reset */

static int
prepare_data (SINC_FILTER *filter, SRC_DATA *data, int half_filter_chan_len)
{	int len = 0 ;

	if (filter->b_real_end >= 0)
		return 0 ;	/* Should be terminating. Just return. */

	if (filter->b_current == 0)
	{	/* Initial state. Set up zeros at the start of the buffer and
		** then load new data after that.
		*/
		len = filter->b_len - 2 * half_filter_chan_len ;

		filter->b_current = filter->b_end = half_filter_chan_len ;
		}
	else if (filter->b_end + half_filter_chan_len + filter->channels < filter->b_len)
	{	/*  Load data at current end position. */
		len = MAX (filter->b_len - filter->b_current - half_filter_chan_len, 0) ;
		}
	else
	{	/* Move data at end of buffer back to the start of the buffer. */
		len = filter->b_end - filter->b_current ;
		memmove (filter->buffer, filter->buffer + filter->b_current - half_filter_chan_len,
						(half_filter_chan_len + len) * sizeof (filter->buffer [0])) ;

		filter->b_current = half_filter_chan_len ;
		filter->b_end = filter->b_current + len ;

		/* Now load data at current end of buffer. */
		len = MAX (filter->b_len - filter->b_current - half_filter_chan_len, 0) ;
		} ;

	len = MIN (filter->in_count - filter->in_used, len) ;
	len -= (len % filter->channels) ;

	if (len < 0 || filter->b_end + len > filter->b_len)
		return SRC_ERR_SINC_PREPARE_DATA_BAD_LEN ;

	memcpy (filter->buffer + filter->b_end, data->data_in + filter->in_used,
						len * sizeof (filter->buffer [0])) ;

	filter->b_end += len ;
	filter->in_used += len ;

	if (filter->in_used == filter->in_count &&
			filter->b_end - filter->b_current < 2 * half_filter_chan_len && data->end_of_input)
	{	/* Handle the case where all data in the current buffer has been
		** consumed and this is the last buffer.
		*/

		if (filter->b_len - filter->b_end < half_filter_chan_len + 5)
		{	/* If necessary, move data down to the start of the buffer. */
			len = filter->b_end - filter->b_current ;
			memmove (filter->buffer, filter->buffer + filter->b_current - half_filter_chan_len,
							(half_filter_chan_len + len) * sizeof (filter->buffer [0])) ;

			filter->b_current = half_filter_chan_len ;
			filter->b_end = filter->b_current + len ;
			} ;

		filter->b_real_end = filter->b_end ;
		len = half_filter_chan_len + 5 ;

		if (len < 0 || filter->b_end + len > filter->b_len)
			len = filter->b_len - filter->b_end ;

		memset (filter->buffer + filter->b_end, 0, len * sizeof (filter->buffer [0])) ;
		filter->b_end += len ;
		} ;

	return 0 ;
} /* prepare_data */

static void
calc_output_stereo (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index, double scale, float * output)
{	double		fraction, left [2], right [2], icoeff ;
	increment_t	filter_index, max_filter_index ;
	int			data_index, coeff_count, indx ;

	/* Convert input parameters into fixed point. */
	max_filter_index = int_to_fp (filter->coeff_half_len) ;

	/* First apply the left half of the filter. */
	filter_index = start_filter_index ;
	coeff_count = (max_filter_index - filter_index) / increment ;
	filter_index = filter_index + coeff_count * increment ;
	data_index = filter->b_current - filter->channels * coeff_count ;

	left [0] = left [1] = 0.0 ;
	do
	{	fraction = fp_to_double (filter_index) ;
		indx = fp_to_int (filter_index) ;

		icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

		left [0] += icoeff * filter->buffer [data_index] ;
		left [1] += icoeff * filter->buffer [data_index + 1] ;

		filter_index -= increment ;
		data_index = data_index + 2 ;
		}
	while (filter_index >= MAKE_INCREMENT_T (0)) ;

	/* Now apply the right half of the filter. */
	filter_index = increment - start_filter_index ;
	coeff_count = (max_filter_index - filter_index) / increment ;
	filter_index = filter_index + coeff_count * increment ;
	data_index = filter->b_current + filter->channels * (1 + coeff_count) ;

	right [0] = right [1] = 0.0 ;
	do
	{	fraction = fp_to_double (filter_index) ;
		indx = fp_to_int (filter_index) ;

		icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

		right [0] += icoeff * filter->buffer [data_index] ;
		right [1] += icoeff * filter->buffer [data_index + 1] ;

		filter_index -= increment ;
		data_index = data_index - 2 ;
		}
	while (filter_index > MAKE_INCREMENT_T (0)) ;

	output [0] = (float) (scale * (left [0] + right [0])) ;
	output [1] = (float) (scale * (left [1] + right [1])) ;
} /* calc_output_stereo */

static int
sinc_stereo_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{	SINC_FILTER *filter ;
	double		input_index, src_ratio, count, float_increment, terminate, rem ;
	increment_t	increment, start_filter_index ;
	int			half_filter_chan_len, samples_in_hand ;

	if (psrc->private_data == NULL)
		return SRC_ERR_NO_PRIVATE ;

	filter = (SINC_FILTER*) psrc->private_data ;

	filter->in_count = data->input_frames * filter->channels ;
	filter->out_count = data->output_frames * filter->channels ;
	filter->in_used = filter->out_gen = 0 ;

	src_ratio = psrc->last_ratio ;

	if (is_bad_src_ratio (src_ratio))
		return SRC_ERR_BAD_INTERNAL_STATE ;

	/* Check the sample rate ratio wrt the buffer len. */
	count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
	if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
		count /= MIN (psrc->last_ratio, data->src_ratio) ;

	/* Maximum coefficientson either side of center point. */
	half_filter_chan_len = filter->channels * (lrint (count) + 1) ;

	input_index = psrc->last_position ;
	float_increment = filter->index_inc ;

	rem = fmod_one (input_index) ;
	filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
	input_index = rem ;

	terminate = 1.0 / src_ratio + 1e-20 ;

	/* Main processing loop. */
	while (filter->out_gen < filter->out_count)
	{
		/* Need to reload buffer? */
		samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

		if (samples_in_hand <= half_filter_chan_len)
		{	if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
				return psrc->error ;

			samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
			if (samples_in_hand <= half_filter_chan_len)
				break ;
			} ;

		/* This is the termination condition. */
		if (filter->b_real_end >= 0)
		{	if (filter->b_current + input_index + terminate >= filter->b_real_end)
				break ;
			} ;

		if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
			src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

		float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
		increment = double_to_fp (float_increment) ;

		start_filter_index = double_to_fp (input_index * float_increment) ;

		calc_output_stereo (filter, increment, start_filter_index, float_increment / filter->index_inc, data->data_out + filter->out_gen) ;
		filter->out_gen += 2 ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		rem = fmod_one (input_index) ;

		filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
		input_index = rem ;
		} ;

	psrc->last_position = input_index ;

	/* Save current ratio rather then target ratio. */
	psrc->last_ratio = src_ratio ;

	data->input_frames_used = filter->in_used / filter->channels ;
	data->output_frames_gen = filter->out_gen / filter->channels ;

	return SRC_ERR_NO_ERROR ;
} /* sinc_stereo_vari_process */

static int
sinc_set_converter (SRC_PRIVATE *psrc, int src_enum)
{	SINC_FILTER *filter, temp_filter ;
	increment_t count ;
	int bits ;

	if (psrc->private_data != NULL)
	{	free (psrc->private_data) ;
		psrc->private_data = NULL ;
		} ;

	memset (&temp_filter, 0, sizeof (temp_filter)) ;

	temp_filter.channels = psrc->channels ;

	if (psrc->channels != 2)
		return SRC_ERR_BAD_CHANNEL_COUNT ;
	else
	{	psrc->const_process = sinc_stereo_vari_process ;
		psrc->vari_process = sinc_stereo_vari_process ;
	}
	psrc->reset = sinc_reset ;

	switch (src_enum)
	{	case SRC_SINC_FASTEST :
				temp_filter.coeffs = fastest_coeffs.coeffs ;
				temp_filter.coeff_half_len = ARRAY_LEN (fastest_coeffs.coeffs) - 2 ;
				temp_filter.index_inc = fastest_coeffs.increment ;
				break ;

		/*
		case SRC_SINC_MEDIUM_QUALITY :
				temp_filter.coeffs = slow_mid_qual_coeffs.coeffs ;
				temp_filter.coeff_half_len = ARRAY_LEN (slow_mid_qual_coeffs.coeffs) - 2 ;
				temp_filter.index_inc = slow_mid_qual_coeffs.increment ;
				break ;
		*/

		default :
				return SRC_ERR_BAD_CONVERTER ;
		} ;

	/*
	** FIXME : This needs to be looked at more closely to see if there is
	** a better way. Need to look at prepare_data () at the same time.
	*/

	temp_filter.b_len = lrint (2.5 * temp_filter.coeff_half_len / (temp_filter.index_inc * 1.0) * SRC_MAX_RATIO) ;
	temp_filter.b_len = MAX (temp_filter.b_len, 4096) ;
	temp_filter.b_len *= temp_filter.channels ;

	if ((filter = calloc (1, sizeof (SINC_FILTER) + sizeof (filter->buffer [0]) * (temp_filter.b_len + temp_filter.channels))) == NULL)
		return SRC_ERR_MALLOC_FAILED ;

	*filter = temp_filter ;
	memset (&temp_filter, 0xEE, sizeof (temp_filter)) ;

	psrc->private_data = filter ;

	sinc_reset (psrc) ;

	count = filter->coeff_half_len ;
	for (bits = 0 ; (MAKE_INCREMENT_T (1) << bits) < count ; bits++)
		count |= (MAKE_INCREMENT_T (1) << bits) ;

	if (bits + SHIFT_BITS - 1 >= (int) (sizeof (increment_t) * 8))
		return SRC_ERR_FILTER_LEN ;

	return SRC_ERR_NO_ERROR ;
} /* sinc_set_converter */

static int
psrc_set_converter (SRC_PRIVATE	*psrc, int converter_type)
{
	if (sinc_set_converter (psrc, converter_type) == SRC_ERR_NO_ERROR)
		return SRC_ERR_NO_ERROR ;

	return SRC_ERR_BAD_CONVERTER ;
} /* psrc_set_converter */

static int
src_reset (SRC_STATE *state)
{	SRC_PRIVATE *psrc ;

	if ((psrc = (SRC_PRIVATE*) state) == NULL)
		return SRC_ERR_BAD_STATE ;

	if (psrc->reset != NULL)
		psrc->reset (psrc) ;

	psrc->last_position = 0.0 ;
	psrc->last_ratio = 0.0 ;

	psrc->error = SRC_ERR_NO_ERROR ;

	return SRC_ERR_NO_ERROR ;
} /* src_reset */


static SRC_STATE *
src_new (int converter_type, int channels, int *error)
{	SRC_PRIVATE	*psrc ;

	if (error)
		*error = SRC_ERR_NO_ERROR ;

	if (channels < 1)
	{	if (error)
			*error = SRC_ERR_BAD_CHANNEL_COUNT ;
		return NULL ;
		} ;

	if ((psrc = calloc (1, sizeof (*psrc))) == NULL)
	{	if (error)
			*error = SRC_ERR_MALLOC_FAILED ;
		return NULL ;
		} ;

	psrc->channels = channels ;

	if (psrc_set_converter (psrc, converter_type) != SRC_ERR_NO_ERROR)
	{	if (error)
			*error = SRC_ERR_BAD_CONVERTER ;
		free (psrc) ;
		psrc = NULL ;
		} ;

	src_reset ((SRC_STATE*) psrc) ;

	return (SRC_STATE*) psrc ;
} /* src_new */

static SRC_STATE *
src_delete (SRC_STATE *state)
{	SRC_PRIVATE *psrc ;

	psrc = (SRC_PRIVATE*) state ;
	if (psrc)
	{	if (psrc->private_data)
			free (psrc->private_data) ;
		memset (psrc, 0, sizeof (SRC_PRIVATE)) ;
		free (psrc) ;
		} ;

	return NULL ;
} /* src_state */

static int
src_process (SRC_STATE *state, SRC_DATA *data)
{	SRC_PRIVATE *psrc ;
	int error ;

	psrc = (SRC_PRIVATE*) state ;

	if (psrc == NULL)
		return SRC_ERR_BAD_STATE ;
	if (psrc->vari_process == NULL || psrc->const_process == NULL)
		return SRC_ERR_BAD_PROC_PTR ;

	/* Check for valid SRC_DATA first. */
	if (data == NULL)
		return SRC_ERR_BAD_DATA ;

	/* And that data_in and data_out are valid. */
	if (data->data_in == NULL || data->data_out == NULL)
		return SRC_ERR_BAD_DATA_PTR ;

	/* Check src_ratio is in range. */
	if (is_bad_src_ratio (data->src_ratio))
		return SRC_ERR_BAD_SRC_RATIO ;

	if (data->input_frames < 0)
		data->input_frames = 0 ;
	if (data->output_frames < 0)
		data->output_frames = 0 ;

	if (data->data_in < data->data_out)
	{	if (data->data_in + data->input_frames * psrc->channels > data->data_out)
		{	/*-printf ("\n\ndata_in: %p    data_out: %p\n",
				(void*) (data->data_in + data->input_frames * psrc->channels), (void*) data->data_out) ;-*/
			return SRC_ERR_DATA_OVERLAP ;
			} ;
		}
	else if (data->data_out + data->output_frames * psrc->channels > data->data_in)
	{	/*-printf ("\n\ndata_in : %p   ouput frames: %ld    data_out: %p\n", (void*) data->data_in, data->output_frames, (void*) data->data_out) ;

		printf ("data_out: %p (%p)    data_in: %p\n", (void*) data->data_out,
			(void*) (data->data_out + data->input_frames * psrc->channels), (void*) data->data_in) ;-*/
		return SRC_ERR_DATA_OVERLAP ;
		} ;

	/* Set the input and output counts to zero. */
	data->input_frames_used = 0 ;
	data->output_frames_gen = 0 ;

	/* Special case for when last_ratio has not been set. */
	if (psrc->last_ratio < (1.0 / SRC_MAX_RATIO))
		psrc->last_ratio = data->src_ratio ;

	/* Now process. */
	if (fabs (psrc->last_ratio - data->src_ratio) < 1e-15)
		error = psrc->const_process (psrc, data) ;
	else
		error = psrc->vari_process (psrc, data) ;

	return error ;
} /* src_process */

static void
src_short_to_float_array (const short *in, float *out, int len)
{
	while (len)
	{	len -- ;
		out [len] = (float) (in [len] / (1.0 * 0x8000)) ;
		} ;

	return ;
} /* src_short_to_float_array */

static void
src_float_to_short_array (const float *in, short *out, int len)
{	double scaled_value ;

	while (len)
	{	len -- ;

		scaled_value = in [len] * (8.0 * 0x10000000) ;
		if (scaled_value >= (1.0 * 0x7FFFFFFF))
		{	out [len] = 32767 ;
			continue ;
			} ;
		if (scaled_value <= (-8.0 * 0x10000000))
		{	out [len] = -32768 ;
			continue ;
			} ;

		out [len] = (short) (lrint (scaled_value) >> 16) ;
		} ;

} /* src_float_to_short_array */
