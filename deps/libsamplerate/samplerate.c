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

#include "fastest_coeffs.h"
// #include "mid_qual_coeffs.h"

#define SRC_MAX_RATIO            256
#define MAX(a,b)                 (((a) > (b)) ? (a) : (b))
#define MIN(a,b)                 (((a) < (b)) ? (a) : (b))
#define ARRAY_LEN(x)             ((int32_t) (sizeof (x) / sizeof ((x) [0])))
#define MAKE_INCREMENT_T(x)      ((int32_t) (x))
#define SHIFT_BITS               12
#define FP_ONE                   ((double) (((int32_t) 1) << SHIFT_BITS))
#define INV_FP_ONE               (1.0 / FP_ONE)

#define fp_fraction_part(x) \
	((x) & ((((int32_t) 1) << SHIFT_BITS) - 1))

#define fp_to_double(x) \
	(fp_fraction_part(x) * INV_FP_ONE)

#define int_to_fp(x) \
	(((int32_t) (x)) << SHIFT_BITS)

#define double_to_fp(x) \
	(lrint ((x) * FP_ONE))

#define fp_to_int(x) \
	(((x) >> SHIFT_BITS))

enum {
	SRC_SINC_MEDIUM_QUALITY		= 1,
	SRC_SINC_FASTEST			= 2,
};

enum {
	SRC_ERR_NO_ERROR = 0,
	SRC_ERR_BAD_SRC_RATIO,
	SRC_ERR_SINC_PREPARE_DATA_BAD_LEN,
	SRC_ERR_BAD_INTERNAL_STATE,
} ;

typedef struct {
	int32_t in_count, in_used ;
	int32_t out_count, out_gen ;
	int32_t coeff_half_len, index_inc ;
	int32_t b_current, b_end, b_real_end, b_len ;
	double last_ratio, last_position ;
	double src_ratio, input_index ;
	double const *coeffs;
	float *buffer;
} SRC_STATE ;

static double fmod_one (double x)
{
	double res = x - lrint(x);

	if (res < 0.0)
		return res + 1.0 ;

	return res;
}

static int32_t is_bad_src_ratio (double ratio)
{
	return (ratio < (1.0 / SRC_MAX_RATIO) || ratio > (1.0 * SRC_MAX_RATIO)) ;
}

static int16_t double_to_short(double in)
{
	return in >= (double) INT16_MAX ? INT16_MAX : in <= INT16_MIN ? INT16_MIN : (int16_t) lrint(in);
}

static int32_t prepare_data (SRC_STATE *ctx, const int16_t *in, int32_t half_filter_chan_len)
{
	int32_t len = 0 ;

	if (ctx->b_real_end >= 0)
		return 0 ;	/* Should be terminating. Just return. */

	if (ctx->b_current == 0)
	{	/* Initial state. Set up zeros at the start of the buffer and
		** then load new data after that.
		*/
		len = ctx->b_len - 2 * half_filter_chan_len ;

		ctx->b_current = ctx->b_end = half_filter_chan_len ;
		}
	else if (ctx->b_end + half_filter_chan_len + 2 < ctx->b_len)
	{	/*  Load data at current end position. */
		len = MAX (ctx->b_len - ctx->b_current - half_filter_chan_len, 0) ;
		}
	else
	{	/* Move data at end of buffer back to the start of the buffer. */
		len = ctx->b_end - ctx->b_current ;
		memmove (ctx->buffer, ctx->buffer + ctx->b_current - half_filter_chan_len,
						(half_filter_chan_len + len) * sizeof (ctx->buffer [0])) ;

		ctx->b_current = half_filter_chan_len ;
		ctx->b_end = ctx->b_current + len ;

		/* Now load data at current end of buffer. */
		len = MAX (ctx->b_len - ctx->b_current - half_filter_chan_len, 0) ;
		} ;

	len = MIN (ctx->in_count - ctx->in_used, len) ;
	len -= (len % 2) ;

	if (len < 0 || ctx->b_end + len > ctx->b_len)
		return SRC_ERR_SINC_PREPARE_DATA_BAD_LEN ;

	for (int32_t x = 0; x < len; x++)
		ctx->buffer[ctx->b_end + x] = (float) in[ctx->in_used + x];

	ctx->b_end += len ;
	ctx->in_used += len ;

	return 0 ;
}

static void calc_output_stereo (SRC_STATE *ctx, int32_t increment, int32_t start_filter_index,
	double scale, int16_t * output)
{
	double		fraction, left [2], right [2], icoeff ;
	int32_t	filter_index, max_filter_index ;
	int32_t			data_index, coeff_count, indx ;

	/* Convert input parameters into fixed point. */
	max_filter_index = int_to_fp (ctx->coeff_half_len) ;

	/* First apply the left half of the filter. */
	filter_index = start_filter_index ;
	coeff_count = (max_filter_index - filter_index) / increment ;
	filter_index = filter_index + coeff_count * increment ;
	data_index = ctx->b_current - 2 * coeff_count ;

	left [0] = left [1] = 0.0 ;
	do
	{	fraction = fp_to_double (filter_index) ;
		indx = fp_to_int (filter_index) ;

		icoeff = ctx->coeffs [indx] + fraction * (ctx->coeffs [indx + 1] - ctx->coeffs [indx]) ;

		left [0] += icoeff * ctx->buffer [data_index] ;
		left [1] += icoeff * ctx->buffer [data_index + 1] ;

		filter_index -= increment ;
		data_index = data_index + 2 ;
		}
	while (filter_index >= MAKE_INCREMENT_T (0)) ;

	/* Now apply the right half of the filter. */
	filter_index = increment - start_filter_index ;
	coeff_count = (max_filter_index - filter_index) / increment ;
	filter_index = filter_index + coeff_count * increment ;
	data_index = ctx->b_current + 2 * (1 + coeff_count) ;

	right [0] = right [1] = 0.0 ;
	do
	{	fraction = fp_to_double (filter_index) ;
		indx = fp_to_int (filter_index) ;

		icoeff = ctx->coeffs [indx] + fraction * (ctx->coeffs [indx + 1] - ctx->coeffs [indx]) ;

		right [0] += icoeff * ctx->buffer [data_index] ;
		right [1] += icoeff * ctx->buffer [data_index + 1] ;

		filter_index -= increment ;
		data_index = data_index - 2 ;
		}
	while (filter_index > MAKE_INCREMENT_T (0)) ;

	output [0] = double_to_short(scale * (left [0] + right [0])) ;
	output [1] = double_to_short(scale * (left [1] + right [1])) ;
}

static int32_t sinc_stereo_vari_process (SRC_STATE *ctx, const int16_t *in, size_t in_frames,
	int16_t *out, size_t out_frames, double ratio, size_t *out_written)
{
	double		input_index, src_ratio, count, float_increment, terminate, rem ;
	int32_t	increment, start_filter_index ;
	int32_t			half_filter_chan_len, samples_in_hand ;

	ctx->in_count = (int32_t) in_frames * 2 ;
	ctx->out_count = (int32_t) out_frames * 2;
	ctx->in_used = ctx->out_gen = 0 ;

	src_ratio = ctx->last_ratio ;

	if (is_bad_src_ratio (src_ratio))
		return SRC_ERR_BAD_INTERNAL_STATE ;

	/* Check the sample rate ratio wrt the buffer len. */
	count = (ctx->coeff_half_len + 2.0) / ctx->index_inc ;
	if (MIN (ctx->last_ratio, ratio) < 1.0)
		count /= MIN (ctx->last_ratio, ratio) ;

	/* Maximum coefficientson either side of center point. */
	half_filter_chan_len = 2 * (lrint (count) + 1) ;

	input_index = ctx->last_position ;
	float_increment = ctx->index_inc ;

	rem = fmod_one (input_index) ;
	ctx->b_current = (ctx->b_current + 2 * lrint (input_index - rem)) % ctx->b_len ;
	input_index = rem ;

	terminate = 1.0 / src_ratio + 1e-20 ;

	/* Main processing loop. */
	while (ctx->out_gen < ctx->out_count)
	{
		/* Need to reload buffer? */
		samples_in_hand = (ctx->b_end - ctx->b_current + ctx->b_len) % ctx->b_len ;

		if (samples_in_hand <= half_filter_chan_len)
		{
			int32_t error = prepare_data (ctx, in, half_filter_chan_len);
			if (error != 0)
				return error;

			samples_in_hand = (ctx->b_end - ctx->b_current + ctx->b_len) % ctx->b_len ;
			if (samples_in_hand <= half_filter_chan_len)
				break ;
			} ;

		/* This is the termination condition. */
		if (ctx->b_real_end >= 0)
		{	if (ctx->b_current + input_index + terminate >= ctx->b_real_end)
				break ;
			} ;

		if (ctx->out_count > 0 && fabs (ctx->last_ratio - ratio) > 1e-10)
			src_ratio = ctx->last_ratio + ctx->out_gen * (ratio - ctx->last_ratio) / ctx->out_count ;

		float_increment = ctx->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
		increment = double_to_fp (float_increment) ;

		start_filter_index = double_to_fp (input_index * float_increment) ;

		calc_output_stereo (ctx, increment, start_filter_index, float_increment / ctx->index_inc, out + ctx->out_gen) ;
		ctx->out_gen += 2 ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		rem = fmod_one (input_index) ;

		ctx->b_current = (ctx->b_current + 2 * lrint (input_index - rem)) % ctx->b_len ;
		input_index = rem ;
		} ;

	ctx->last_position = input_index ;

	/* Save current ratio rather then target ratio. */
	ctx->last_ratio = src_ratio ;

	*out_written = ctx->out_gen / 2 ;

	return SRC_ERR_NO_ERROR ;
}

static void src_delete (SRC_STATE *ctx)
{
	if (ctx)
		free(ctx->buffer);

	free(ctx) ;
}

static void src_reset (SRC_STATE *ctx)
{
	ctx->b_real_end = -1 ;
	ctx->b_current = ctx->b_end = 0 ;
	ctx->last_position = ctx->last_ratio = 0.0 ;
	ctx->src_ratio = ctx->input_index = 0.0 ;
	ctx->out_count = ctx->out_gen = 0;
	ctx->in_count = ctx->in_used = 0;

	memset (ctx->buffer, 0, ctx->b_len * sizeof (float)) ;

	/* Set this for a sanity check */
	memset (ctx->buffer + ctx->b_len, 0xAA, 2 * sizeof (float)) ;
}

static SRC_STATE *src_new (int32_t converter_type)
{
	bool r = true;
	SRC_STATE *ctx = calloc(1, sizeof (SRC_STATE));

	switch (converter_type) {
		case SRC_SINC_FASTEST :
				ctx->coeffs = fastest_coeffs.coeffs ;
				ctx->coeff_half_len = ARRAY_LEN (fastest_coeffs.coeffs) - 2 ;
				ctx->index_inc = fastest_coeffs.increment ;
				break ;

		/*
		case SRC_SINC_MEDIUM_QUALITY :
				ctx->coeffs = slow_mid_qual_coeffs.coeffs ;
				ctx->coeff_half_len = ARRAY_LEN (slow_mid_qual_coeffs.coeffs) - 2 ;
				ctx->index_inc = slow_mid_qual_coeffs.increment ;
				break ;
		*/

		default:
				r = false;
				goto except;
	}

	/*
	** FIXME : This needs to be looked at more closely to see if there is
	** a better way. Need to look at prepare_data () at the same time.
	*/

	ctx->b_len = lrint (2.5 * ctx->coeff_half_len / ctx->index_inc * SRC_MAX_RATIO) ;
	ctx->b_len = MAX (ctx->b_len, 4096) ;
	ctx->b_len *= 2;
	ctx->buffer = calloc(ctx->b_len + 2, sizeof(float));

	src_reset(ctx) ;

	int32_t bits = 0;
	int32_t count = ctx->coeff_half_len ;
	for (; (MAKE_INCREMENT_T (1) << bits) < count ; bits++)
		count |= (MAKE_INCREMENT_T (1) << bits) ;

	if (bits + SHIFT_BITS - 1 >= (int32_t) (sizeof (int32_t) * 8)) {
		r = false;
		goto except;
	}

	except:

	if (!r) {
		src_delete(ctx);
		ctx = NULL;
	}

	return ctx;
}

static int32_t src_process (SRC_STATE *ctx, const int16_t *in, size_t in_frames,
	int16_t *out, size_t out_frames, double ratio, size_t *out_written)
{
	/* Check src_ratio is in range. */
	if (is_bad_src_ratio (ratio))
		return SRC_ERR_BAD_SRC_RATIO ;

	/* Special case for when last_ratio has not been set. */
	if (ctx->last_ratio < (1.0 / SRC_MAX_RATIO))
		ctx->last_ratio = ratio ;

	return sinc_stereo_vari_process (ctx, in, in_frames, out, out_frames, ratio, out_written) ;
}
