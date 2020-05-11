#include "apu.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define APU_CLOCK 1789773

// Global timer shift value for changing frequency (emulator hack for overclocking)
static uint8_t OC_SHIFT;

// Length Counter

struct length {
	bool enabled;
	bool next_enabled;
	bool skip_clock;
	uint8_t value;
};

static void apu_step_length(struct length *len)
{
	if (len->skip_clock) {
		len->skip_clock = false;
		return;
	}

	if (len->enabled && len->value > 0)
		len->value--;
}


// Envelope

struct envelope {
	bool constant_volume;
	bool start;
	bool loop;
	uint8_t v;
	uint8_t divider_period;
	uint8_t decay_level;
};

static void apu_step_envelope(struct envelope *env)
{
	if (!env->start) {
		if (env->divider_period == 0) {
			env->divider_period = env->v << OC_SHIFT;

			if (env->decay_level == 0) {
				if (env->loop)
					env->decay_level = 15;
			} else {
				env->decay_level--;
			}
		} else {
			env->divider_period--;
		}

	} else {
		env->start = false;
		env->decay_level = 15;
		env->divider_period = env->v << OC_SHIFT;
	}
}


// Pulse Channels

struct timer {
	uint16_t period;
	uint16_t value;
};

struct pulse {
	bool enabled;
	uint8_t output;

	struct timer timer;
	struct length len;
	struct envelope env;

	struct {
		bool reload;
		bool enabled;
		bool negate;
		uint8_t shift;
		uint8_t period;
		uint8_t value;
	} sweep;

	uint8_t duty_mode;
	uint8_t duty_value;
};

static const uint8_t LENGTH_TABLE[] = {
	10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

static const uint8_t DUTY_TABLE[4][8] = {
	{0, 1, 0, 0, 0, 0, 0, 0},
	{0, 1, 1, 0, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{1, 0, 0, 1, 1, 1, 1, 1},
};

static bool apu_sweep_mute(struct pulse *p, enum extaudio ext)
{
	return
		(ext == EXT_NONE && p->timer.period < 8) ||
		(!p->sweep.negate && ((p->timer.period + (p->timer.period >> p->sweep.shift)) & 0x0800));
}

static void apu_pulse_step_sweep(struct pulse *p, uint8_t channel, enum extaudio ext)
{
	if (p->sweep.value == 0 && p->sweep.enabled && !apu_sweep_mute(p, ext) && p->sweep.shift > 0) {
		int32_t delta = (p->timer.period >> p->sweep.shift) >> OC_SHIFT;

		if (p->sweep.negate) {
			delta = -delta;

			if (channel == 0)
				delta--;
		}

		p->timer.period = (uint16_t) ((int32_t) p->timer.period + delta);
	}

	if (p->sweep.value == 0 || p->sweep.reload) {
		p->sweep.value = p->sweep.period << OC_SHIFT;
		p->sweep.reload = false;

	} else {
		p->sweep.value--;
	}
}

static void apu_pulse_step_timer(struct pulse *p, enum extaudio ext)
{
	if (p->timer.value == 0) {
		p->timer.value = p->timer.period << OC_SHIFT;
		p->duty_value = (p->duty_value + 1) % 8;

		p->output = (p->len.value == 0 || apu_sweep_mute(p, ext) ||
			DUTY_TABLE[p->duty_mode][p->duty_value] == 0) ? 0 :
			p->env.constant_volume ? p->env.v : p->env.decay_level;

	} else {
		p->timer.value--;
	}
}


// Triangle Channel

struct triangle {
	bool enabled;
	uint8_t output;
	bool pop;

	struct timer timer;
	struct length len;

	struct {
		bool reload;
		uint8_t period;
		uint8_t value;
	} counter;

	uint8_t duty_value;
};

static const uint8_t TRIANGLE_TABLE[] = {
	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static void apu_triangle_step_timer(struct triangle *t)
{
	if (t->timer.value == 0) {
		t->timer.value = t->timer.period << OC_SHIFT;

		// timer.period of 0 cause high pitched tones
		if (t->len.value > 0 && t->counter.value > 0 && t->timer.period > 0)
			t->duty_value = (t->duty_value + 1) % 32;

		if (!t->pop && t->duty_value >= 15)
			t->pop = true;

		t->output = t->pop ? TRIANGLE_TABLE[t->duty_value] : 0;;

	} else {
		t->timer.value--;
	}
}

static void apu_triangle_step_counter(struct triangle *t)
{
	if (t->counter.reload) {
		t->counter.value = t->counter.period << OC_SHIFT;

	} else if (t->counter.value > 0) {
		t->counter.value--;
	}

	if (t->len.enabled)
		t->counter.reload = false;
}


// Noise Channel

struct noise {
	bool enabled;
	uint8_t output;

	struct timer timer;
	struct length len;
	struct envelope env;

	bool mode;
	uint16_t shift_register;
};

static const uint16_t NOISE_TABLE[] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

static void apu_noise_step_timer(struct noise *n)
{
	if (n->timer.value > 0)
		n->timer.value--;

	if (n->timer.value == 0) {
		n->timer.value = n->timer.period << OC_SHIFT;

		uint16_t feedback = (n->shift_register & 0x0001) ^ ((n->shift_register >> (n->mode ? 6 : 1)) & 0x0001);
		n->shift_register = (n->shift_register >> 1) | (feedback << 14);

		n->output = (n->len.value == 0 || (n->shift_register & 0x0001)) ? 0 :
			n->env.constant_volume ? n->env.v : n->env.decay_level;
	}
}


// DMC Channel

struct dmc {
	bool enabled;
	uint8_t output;

	struct timer timer;

	struct {
		uint8_t shift_register;
		uint8_t bits_remaining;
		bool silence;
	} out;

	struct {
		bool sample_buffer_empty;
		uint8_t sample_buffer;
	} reader;

	uint16_t sample_address;
	uint16_t sample_length;
	uint16_t current_address;
	uint16_t current_length;
	bool loop;
	bool irq;
	bool irq_flag;
};

static const uint8_t DMC_TABLE[] = {
	214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27,
};

static void apu_dmc_restart(struct dmc *d)
{
	d->current_address = d->sample_address;
	d->current_length = d->sample_length;
}

static void apu_dmc_fill_sample_buffer(struct dmc *d, NES *nes)
{
	if (d->reader.sample_buffer_empty && d->current_length > 0) {
		sys_dma_dmc_begin(nes, d->current_address);

		d->current_address = (d->current_address == 0xFFFF) ? 0x8000 : d->current_address + 1;
		d->current_length--;

		if (d->current_length == 0) {
			if (d->loop) {
				apu_dmc_restart(d);

			} else if (d->irq) {
				d->irq_flag = true;
			}
		}

		d->reader.sample_buffer_empty = false;
	}
}

static void apu_dmc_step_timer(struct dmc *d, NES *nes)
{
	if (d->timer.value > 0)
		d->timer.value--;

	if (d->timer.value == 0) {
		d->timer.value = d->timer.period << OC_SHIFT;

		if (!d->out.silence) {
			if (d->out.shift_register & 0x01 && d->output <= 125) {
				d->output += 2;

			} else if (d->output >= 2) {
				d->output -= 2;
			}
		}

		d->out.shift_register >>= 1;

		// Out cycle has ended
		if (d->out.bits_remaining == 0) {
			d->out.bits_remaining = 8;

			if (d->reader.sample_buffer_empty) {
				d->out.silence = true;

			} else {
				d->out.silence = false;
				d->out.shift_register = d->reader.sample_buffer;
				d->reader.sample_buffer_empty = true;
				apu_dmc_fill_sample_buffer(d, nes);
			}
		}

		d->out.bits_remaining--;
	}
}


// VRC6

struct pulse6 {
	bool enabled;
	bool mode;
	uint8_t volume;
	uint8_t duty_value;
	uint8_t duty_cycle;
	uint8_t output;
	uint16_t divider;
	uint16_t frequency;
};

struct saw {
	bool enabled;
	uint8_t clock;
	uint8_t accum_rate;
	uint8_t accumulator;
	uint8_t output;
	uint16_t divider;
	uint16_t frequency;
};

static void apu_vrc6_pulse_step_timer(struct pulse6 *p)
{
	if (p->divider == 0) {
		p->divider = p->frequency << OC_SHIFT;

		if (p->duty_value == 0) {
			p->duty_value = 15;
		} else {
			p->duty_value--;
		}

		p->output = p->enabled && (p->duty_value <= p->duty_cycle || p->mode) ?
			p->volume : 0;

	} else {
		p->divider--;
	}
}

static void apu_vrc6_saw_step_timer(struct saw *s)
{
	if (s->divider == 0) {
		s->divider = s->frequency << OC_SHIFT;

		if (s->clock == 0) {
			s->accumulator = 0;

		} else if ((s->clock & 1) == 0) {
			s->accumulator += s->accum_rate;
			s->output = s->enabled ? (s->accumulator & 0xF8) >> 3 : 0;
		}

		if (++s->clock == 14)
			s->clock = 0;

	} else {
		s->divider--;
	}
}


// Sunsoft 5B

struct ss5b {
	bool disable;
	uint8_t output;

	bool flip;
	uint8_t volume;
	uint16_t frequency;
	uint16_t counter;
	uint16_t divider;
};

static void apu_ss5b_channel_step_timer(struct ss5b *c)
{
	if (++c->divider == 16) {
		if (++c->counter >= c->frequency << OC_SHIFT) {
			c->flip = !c->flip;
			c->output = c->flip && !c->disable ? (c->volume << 1) + (c->volume > 0 ? 1 : 0) : 0;
			c->counter = 0;
		}

		c->divider = 0;
	}
}


// DAC

#define TIME_BITS   20
#define TIME_UNIT   (1 << TIME_BITS)
#define BASS_SHIFT  14
#define DELTA_BITS  15
#define PHASE_COUNT 32
#define OUTPUT_SIZE 1024
#define BUF_SIZE    ((44800 * sizeof(int16_t) * 2) / 30)

struct dac {
	NES_Config cfg;

	int16_t pvol[31];
	int16_t cvol[32];
	int16_t tndvol[203];
	int16_t sinc[PHASE_COUNT + 1][16];
	uint32_t frame_samples;
	uint32_t factor;
	uint32_t offset;
	uint32_t cycle;
	int16_t prev_sample[2];
	int32_t integrator[2];
	int32_t samples[2][2048];
	int16_t output[OUTPUT_SIZE];
	int16_t buf[BUF_SIZE];
	size_t buf_offset;
};

static const int16_t SINC[PHASE_COUNT + 1][8] = {
	{43, -115,  350, -488, 1136, -914,  5861, 21022},
	{44, -118,  348, -473, 1076, -799,  5274, 21001},
	{45, -121,  344, -454, 1011, -677,  4706, 20936},
	{46, -122,  336, -431,  942, -549,  4156, 20829},
	{47, -123,  327, -404,  868, -418,  3629, 20679},
	{47, -122,  316, -375,  792, -285,  3124, 20488},
	{47, -120,  303, -344,  714, -151,  2644, 20256},
	{46, -117,  289, -310,  634,  -17,  2188, 19985},
	{46, -114,  273, -275,  553,  117,  1758, 19675},
	{44, -108,  255, -237,  471,  247,  1356, 19327},
	{43, -103,  237, -199,  390,  373,   981, 18944},
	{42,  -98,  218, -160,  310,  495,   633, 18527},
	{40,  -91,  198, -121,  231,  611,   314, 18078},
	{38,  -84,  178,  -81,  153,  722,    22, 17599},
	{36,  -76,  157,  -43,   80,  824,  -241, 17092},
	{34,  -68,  135,   -3,    8,  919,  -476, 16558},
	{32,  -61,  115,   34,  -60, 1006,  -683, 16001},
	{29,  -52,   94,   70, -123, 1083,  -862, 15422},
	{27,  -44,   73,  106, -184, 1152, -1015, 14824},
	{25,  -36,   53,  139, -239, 1211, -1142, 14210},
	{22,  -27,   34,  170, -290, 1261, -1244, 13582},
	{20,  -20,   16,  199, -335, 1301, -1322, 12942},
	{18,  -12,   -3,  226, -375, 1331, -1376, 12293},
	{15,   -4,  -19,  250, -410, 1351, -1408, 11638},
	{13,    3,  -35,  272, -439, 1361, -1419, 10979},
	{11,    9,  -49,  292, -464, 1362, -1410, 10319},
	{ 9,   16,  -63,  309, -483, 1354, -1383,  9660},
	{ 7,   22,  -75,  322, -496, 1337, -1339,  9005},
	{ 6,   26,  -85,  333, -504, 1312, -1280,  8355},
	{ 4,   31,  -94,  341, -507, 1278, -1205,  7713},
	{ 3,   35, -102,  347, -506, 1238, -1119,  7082},
	{ 1,   40, -110,  350, -499, 1190, -1021,  6464},
	{ 0,   43, -115,  350, -488, 1136,  -914,  5861},
};

static void apu_dac_clock_math(struct dac *dac, uint32_t clock)
{
	dac->factor = (uint32_t) ceil(TIME_UNIT * (double) dac->cfg.sampleRate / (double) clock);
	dac->frame_samples = (clock / dac->cfg.sampleRate) * (dac->cfg.sampleRate / 100);
}

static int16_t apu_clampi32(int32_t pcmi32)
{
	return pcmi32 < -32768 ? -32768 : pcmi32 > 32767 ? 32767 : (int16_t) pcmi32;
}

static int16_t apu_clampf64(double pcm)
{
	return apu_clampi32(lrint(pcm * 32768.0));
}

static void apu_dac_create(struct dac *dac)
{
	for (int32_t x = 0; x < 31; x++)
		dac->pvol[x] = apu_clampf64(95.52 / (8128.0 / (double) x + 100.0));

	for (int32_t x = 0; x < 203; x++)
		dac->tndvol[x] = apu_clampf64(163.67 / (24329.0 / (double) x + 100.0));

	for (int32_t x = 0; x < 32; x++)
		dac->cvol[x] = apu_clampf64(x == 0 ? 0.0 : 1.0 / pow(1.6, 1.0 / 2 * (31 - x)));

	for (int32_t x = 0; x < PHASE_COUNT + 1; x++) {
		for (int32_t y = 0; y < 8; y++) {
			dac->sinc[x][y] = SINC[x][y];
			dac->sinc[PHASE_COUNT - x][15 - y] = dac->sinc[x][y];
		}
	}
}

static void apu_dac_add_sample(struct dac *dac, uint32_t offset, uint8_t chan, int16_t sample, bool fast)
{
	if (sample == dac->prev_sample[chan])
		return;

	int32_t delta = sample - dac->prev_sample[chan];
	int32_t *out = dac->samples[chan] + (offset >> TIME_BITS);

	if (fast) {
		int32_t interp = (offset >> 5) & 0x7FFF;
		int32_t delta2 = delta * interp;

		out[7] += delta * 0x8000 - delta2;
		out[8] += delta2;

	} else {
		int32_t phase = (offset >> 15) & 0x1F;
		int32_t interp = offset & 0x7FFF;
		int32_t delta2 = (delta * interp) >> DELTA_BITS;
		delta -= delta2;

		for (uint8_t x = 0; x < 16; x++)
			out[x] += dac->sinc[phase][x] * delta + dac->sinc[phase + 1][x] * delta2;
	}

	dac->prev_sample[chan] = sample;
}

static void apu_dac_output_channel(struct dac *dac, uint8_t chan, int32_t offset)
{
	int16_t s = apu_clampi32(dac->integrator[chan] >> DELTA_BITS);
	dac->output[offset * 2 + chan] = s;
	dac->integrator[chan] += dac->samples[chan][offset];
	dac->integrator[chan] -= s << (DELTA_BITS - BASS_SHIFT); // High pass filter
}

static void apu_dac_spatialize(struct dac *dac, int32_t x)
{
	int16_t *l = &dac->output[x * 2];
	int16_t *r = &dac->output[x * 2 + 1];

	if (dac->cfg.stereo) {
		int16_t stereo_l = (int16_t) lrint((*l * 0.65 + *r * 0.35) * 1.65);
		int16_t stereo_r = (int16_t) lrint((*r * 0.65 + *l * 0.35) * 1.65);

		*l = stereo_l;
		*r = stereo_r;

	} else {
		*r = *l;
	}
}

static void apu_dac_generate_output(struct dac *dac, uint32_t offset)
{
	int32_t samples = offset >> TIME_BITS;
	dac->offset = offset & (TIME_UNIT - 1);
	dac->cycle = 0;

	for (int32_t x = 0; x < samples; x++) {
		apu_dac_output_channel(dac, 0, x);
		apu_dac_output_channel(dac, 1, x);
		apu_dac_spatialize(dac, x);

		if (x < 18) {
			dac->samples[0][x] = dac->samples[0][samples + x];
			dac->samples[1][x] = dac->samples[1][samples + x];
			dac->samples[0][samples + x]  = dac->samples[1][samples + x] = 0;
		} else {
			dac->samples[0][x]  = dac->samples[1][x] = 0;
		}
	}

	size_t size = samples * sizeof(int16_t) * 2;
	memcpy(dac->buf + dac->buf_offset, dac->output, size);
	dac->buf_offset += size / 2;
}

static void apu_dac_step(struct dac *dac, int16_t l, int16_t r)
{
	uint32_t offset = dac->cycle * dac->factor + dac->offset;

	if (!dac->cfg.stereo)
		l += r;

	apu_dac_add_sample(dac, offset, 0, l, false);
	apu_dac_add_sample(dac, offset, 1, r, false);

	if (dac->cycle++ > dac->frame_samples)
		apu_dac_generate_output(dac, offset);
}

static void apu_dac_mix(struct dac *dac, uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3,
	uint8_t p6_0, uint8_t p6_1, uint8_t c0, uint8_t c1, uint8_t c2, uint8_t s, uint8_t t,
	uint8_t n, uint8_t d)
{
	if (!(dac->cfg.channels & NES_CHANNEL_PULSE_0))
		p0 = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_PULSE_1))
		p1 = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_TRIANGLE))
		t = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_NOISE))
		n = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_DMC))
		d = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_EXT_0))
		p2 = p6_0 = c0 = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_EXT_1))
		p3 = p6_1 = c1 = 0;

	if (!(dac->cfg.channels & NES_CHANNEL_EXT_2))
		s = c2 = 0;

	if (dac->cfg.stereo) {
		int16_t l = dac->tndvol[3 * t + 2 * n] + dac->pvol[p0] - dac->pvol[p2] - dac->pvol[p6_0] +
			dac->cvol[c0] - dac->pvol[s] - dac->cvol[c2];
		int16_t r = dac->tndvol[d] + dac->pvol[p1] - dac->pvol[p3] - dac->pvol[p6_1] - dac->cvol[c1];

		apu_dac_step(dac, l, r);
	} else {
		int16_t m = dac->tndvol[3 * t + 2 * n + d] + dac->pvol[p0 + p1] - dac->pvol[p2] -
			dac->pvol[p6_0] - dac->cvol[c0] - dac->pvol[s] - dac->cvol[c2] - dac->pvol[p3] -
			dac->pvol[p6_1] - dac->cvol[c1];

		apu_dac_step(dac, m, 0);
	}
}


// IO

struct apu {
	bool mode;
	bool next_mode;
	bool irq_disabled;
	bool frame_irq;
	uint8_t delayed_reset;
	int64_t frame_counter;

	struct pulse p[4];
	struct pulse6 p6[2];
	struct ss5b c[3];
	struct saw s;
	struct triangle t;
	struct noise n;
	struct dmc d;

	enum extaudio ext;
	struct dac dac;
};

void apu_dma_dmc_finish(struct apu *apu, uint8_t v)
{
	apu->d.reader.sample_buffer = v;
}

static void apu_reload_length(struct apu *apu, struct length *len, bool channel_enabled, uint8_t v)
{
	bool in_length_cycle = apu->frame_counter == 14913 || apu->frame_counter == (apu->mode ? 37281 : 29828);

	len->skip_clock = len->value == 0 && in_length_cycle;
	bool ignore_reload = len->value != 0 && in_length_cycle;

	if (channel_enabled && !ignore_reload)
		len->value = LENGTH_TABLE[v >> 3] << OC_SHIFT;
}

uint8_t apu_read_status(struct apu *apu, enum extaudio ext)
{
	uint8_t r = 0;
	struct pulse *p = ext == EXT_MMC5 ? apu->p + 2 : apu->p;

	if (p[0].len.value > 0) r |= 0x01;
	if (p[1].len.value > 0) r |= 0x02;

	if (ext == EXT_NONE) {
		if (apu->t.len.value > 0)      r |= 0x04;
		if (apu->n.len.value > 0)      r |= 0x08;
		if (apu->d.current_length > 0) r |= 0x10;
		if (apu->frame_irq)            r |= 0x40;
		if (apu->d.irq_flag)           r |= 0x80;

		apu->frame_irq = false;
	}

	return r;
}

void apu_write(struct apu *apu, NES *nes, uint16_t addr, uint8_t v, enum extaudio ext)
{
	struct pulse *p = ext == EXT_MMC5 ? apu->p + 2 : apu->p;

	if (ext != EXT_NONE)
		apu->ext = ext;

	if (ext != EXT_VRC6 && ext != EXT_SS5B && addr > 0x4017)
		return;

	switch (addr) {
		// Pulse, MMC5 Pulse
		case 0x4000:
		case 0x4004: {
			uint8_t i = addr == 0x4000 ? 0 : 1;

			p[i].duty_mode = v >> 6;
			p[i].len.next_enabled = !(v & 0x20);
			p[i].env.loop = v & 0x20;
			p[i].env.constant_volume = v & 0x10;
			p[i].env.v = v & 0x0F;
			break;
		}
		case 0x4001:
		case 0x4005: {
			uint8_t i = addr == 0x4001 ? 0 : 1;

			p[i].sweep.enabled = v & 0x80;
			p[i].sweep.period = (v >> 4) & 0x07;
			p[i].sweep.negate = v & 0x08;
			p[i].sweep.shift = v & 0x07;
			p[i].sweep.reload = true;
			break;
		}
		case 0x4002:
		case 0x4006: {
			uint8_t i = addr == 0x4002 ? 0 : 1;

			p[i].timer.period = (p[i].timer.period & 0xFF00) | (uint16_t) v;
			break;
		}
		case 0x4003:
		case 0x4007: {
			uint8_t i = addr == 0x4003 ? 0 : 1;

			apu_reload_length(apu, &p[i].len, p[i].enabled, v);
			p[i].timer.period = (p[i].timer.period & 0x00FF) | ((uint16_t) (v & 0x07) << 8);
			p[i].env.start = true;
			p[i].duty_value = 0;
			break;
		}

		// Triangle
		case 0x4008:
			apu->t.len.next_enabled = !(v & 0x80);
			apu->t.counter.period = v & 0x7F;
			break;
		case 0x4009:
			break;
		case 0x400A:
			apu->t.timer.period = (apu->t.timer.period & 0xFF00) | (uint16_t) v;
			break;
		case 0x400B:
			apu_reload_length(apu, &apu->t.len, apu->t.enabled, v);
			apu->t.timer.period = (apu->t.timer.period & 0x00FF) | ((uint16_t) (v & 0x07) << 8);
			apu->t.counter.reload = true;
			break;

		// Noise
		case 0x400C:
			apu->n.len.next_enabled = !(v & 0x20);
			apu->n.env.loop = v & 0x20;
			apu->n.env.constant_volume = v & 0x10;
			apu->n.env.v = v & 0x0F;
			break;
		case 0x400D:
			break;
		case 0x400E:
			apu->n.mode = v & 0x80;
			apu->n.timer.period = NOISE_TABLE[v & 0x0F];
			break;
		case 0x400F:
			apu_reload_length(apu, &apu->n.len, apu->n.enabled, v);
			apu->n.env.start = true;
			break;

		// DMC
		case 0x4010:
			apu->d.irq = v & 0x80;

			if (!apu->d.irq) {
				apu->d.irq_flag = false;
			}

			apu->d.loop = v & 0x40;
			apu->d.timer.period = DMC_TABLE[v & 0x0F];
			break;
		case 0x4011:
			apu->d.output = v & 0x7F;
			break;
		case 0x4012:
			apu->d.sample_address = 0xC000 | ((uint16_t) v << 6);
			break;
		case 0x4013:
			apu->d.sample_length = ((uint16_t) v << 4) | 0x0001;
			break;

		// Status
		case 0x4015:
			p[0].enabled = v & 0x01;
			p[1].enabled = v & 0x02;

			if (!p[0].enabled)
				p[0].len.value = 0;

			if (!p[1].enabled)
				p[1].len.value = 0;

			if (ext == EXT_NONE) {
				apu->t.enabled = v & 0x04;
				apu->n.enabled = v & 0x08;
				apu->d.enabled = v & 0x10;

				apu->d.irq_flag = false;

				if (!apu->t.enabled)
					apu->t.len.value = 0;

				if (!apu->n.enabled)
					apu->n.len.value = 0;

				if (!apu->d.enabled) {
					apu->d.current_length = 0;

				} else {
					if (apu->d.current_length == 0)
						apu_dmc_restart(&apu->d);

					apu_dmc_fill_sample_buffer(&apu->d, nes);
				}
			}
			break;

		// Frame Counter
		case 0x4017:
			apu->next_mode = v & 0x80;
			apu->irq_disabled = v & 0x40;
			apu->delayed_reset = sys_odd_cycle(nes) ? 3 : 4;

			if (apu->irq_disabled)
				apu->frame_irq = false;
			break;

		// VRC6 Pulse
		case 0x9000:
		case 0xA000: {
			uint8_t i = (addr >> 12) - 0x9;
			apu->p6[i].volume = v & 0x0F;
			apu->p6[i].duty_cycle = (v & 0x70) >> 4;
			apu->p6[i].mode = v >> 7;
			break;
		}
		case 0x9001:
		case 0xA001: {
			uint8_t i = (addr >> 12) - 0x9;
			apu->p6[i].frequency &= 0xFF00;
			apu->p6[i].frequency |= v;
			break;
		}
		case 0x9002:
		case 0xA002: {
			uint8_t i = (addr >> 12) - 0x9;
			apu->p6[i].frequency &= 0xF0FF;
			apu->p6[i].frequency |= (uint16_t) (v & 0x0F) << 8;
			apu->p6[i].enabled = v & 0x80;
			break;
		}
		case 0x9003:
			break;

		// VRC6 Sawtooth
		case 0xB000:
			apu->s.accum_rate = v & 0x3F;
			break;
		case 0xB001:
			apu->s.frequency &= 0xFF00;
			apu->s.frequency |= v;
			break;
		case 0xB002:
			apu->s.frequency &= 0xF0FF;
			apu->s.frequency |= (uint16_t) (v & 0x0F) << 8;
			apu->s.enabled = v & 0x80;
			break;

		// Sunsoft 5B Channel A, B, C Low Period
		case 0xE000:
		case 0xE002:
		case 0xE004: {
			struct ss5b *c = &apu->c[(addr & 0xF) / 2];
			c->frequency = (c->frequency & 0xFF00) | v;
			break;
		}

		// Sunsoft 5B Channel A, B, C High Period
		case 0xE001:
		case 0xE003:
		case 0xE005: {
			struct ss5b *c = &apu->c[(addr & 0xF) / 2];
			c->frequency = (c->frequency & 0x00FF) | ((uint16_t) v << 8);
			break;
		}

		// Sunsoft 5B Channel A, B, C Tone Disable
		case 0xE007:
			apu->c[0].disable = v & 0x1;
			apu->c[1].disable = v & 0x2;
			apu->c[2].disable = v & 0x4;
			break;

		// Sunsoft 5B Channel A, B, C, Volume
		case 0xE008:
		case 0xE009:
		case 0xE00A:
			struct ss5b *c = &apu->c[(addr & 0xF) - 0x8];
			c->volume = v & 0xF;
			break;
	}
}


// Step

static void apu_step_all_envelope(struct apu *apu)
{
	apu_step_envelope(&apu->p[0].env);
	apu_step_envelope(&apu->p[1].env);
	apu_triangle_step_counter(&apu->t);
	apu_step_envelope(&apu->n.env);
}

static void apu_step_all_sweep_and_length(struct apu *apu)
{
	apu_pulse_step_sweep(&apu->p[0], 0, EXT_NONE);
	apu_pulse_step_sweep(&apu->p[1], 1, EXT_NONE);

	apu_step_length(&apu->p[0].len);
	apu_step_length(&apu->p[1].len);
	apu_step_length(&apu->t.len);
	apu_step_length(&apu->n.len);
}

static void apu_step_mmc5(struct apu *apu)
{
	if (apu->ext == EXT_MMC5) {
		apu_step_envelope(&apu->p[2].env);
		apu_step_envelope(&apu->p[3].env);
		apu_step_length(&apu->p[2].len);
		apu_step_length(&apu->p[3].len);
	}
}

static void apu_delayed_length_enabled(struct apu *apu)
{
	apu->p[0].len.enabled = apu->p[0].len.next_enabled;
	apu->p[1].len.enabled = apu->p[1].len.next_enabled;
	apu->t.len.enabled = apu->t.len.next_enabled;
	apu->n.len.enabled = apu->n.len.next_enabled;

	if (apu->ext == EXT_MMC5) {
		apu->p[2].len.enabled = apu->p[2].len.next_enabled;
		apu->p[3].len.enabled = apu->p[3].len.next_enabled;
	}
}

static void apu_step_frame_counter(struct apu *apu)
{
	switch (apu->frame_counter) {
		case 7457:
			apu_step_all_envelope(apu);
			apu_step_mmc5(apu);
			break;
		case 14913:
			apu_step_all_sweep_and_length(apu);
			apu_step_all_envelope(apu);
			apu_step_mmc5(apu);
			break;
		case 22371:
			apu_step_all_envelope(apu);
			apu_step_mmc5(apu);
			break;
		case 29828:
			if (!apu->mode && !apu->irq_disabled)
				apu->frame_irq = true;
			break;
		case 29829:
			if (!apu->mode) {
				if (!apu->irq_disabled)
					apu->frame_irq = true;

				apu_step_all_sweep_and_length(apu);
				apu_step_all_envelope(apu);
			}
			apu_step_mmc5(apu);
			break;
		case 29830:
			if (!apu->mode) {
				if (!apu->irq_disabled)
					apu->frame_irq = true;

				apu->frame_counter = 0;
			}
			break;
		case 37281:
			if (apu->mode) {
				apu_step_all_sweep_and_length(apu);
				apu_step_all_envelope(apu);
			}
			break;
		case 37282:
			if (apu->mode)
				apu->frame_counter = 0;
			break;
	}
}

void apu_step(struct apu *apu, NES *nes)
{
	// Pulse & dmc step every other clock
	if (sys_odd_cycle(nes)) {
		apu_pulse_step_timer(&apu->p[0], EXT_NONE);
		apu_pulse_step_timer(&apu->p[1], EXT_NONE);
		apu_dmc_step_timer(&apu->d, nes);

		if (apu->ext == EXT_MMC5) {
			apu_pulse_step_timer(&apu->p[2], EXT_MMC5);
			apu_pulse_step_timer(&apu->p[3], EXT_MMC5);
		}
	}

	// Triangle & noise step every clock
	apu_triangle_step_timer(&apu->t);
	apu_noise_step_timer(&apu->n);

	// VRC6, SS5B
	if (apu->ext == EXT_VRC6) {
		apu_vrc6_pulse_step_timer(&apu->p6[0]);
		apu_vrc6_pulse_step_timer(&apu->p6[1]);
		apu_vrc6_saw_step_timer(&apu->s);

	} else if (apu->ext == EXT_SS5B) {
		apu_ss5b_channel_step_timer(&apu->c[0]);
		apu_ss5b_channel_step_timer(&apu->c[1]);
		apu_ss5b_channel_step_timer(&apu->c[2]);
	}

	// Process the frame counter
	if (!(apu->delayed_reset > 0 && apu->delayed_reset < 3 && apu->mode))
		apu_step_frame_counter(apu);

	// Enabling/disabling length happens with a 1 cycle delay
	apu_delayed_length_enabled(apu);

	// Writing to $4017 causes a delayed reset of the frame counter
	apu->mode = apu->next_mode;
	if (apu->delayed_reset > 0) {
		if (--apu->delayed_reset == 0) {
			apu->frame_counter = 0;

			// Quarter/half frame triggers up front if mode is set
			if (apu->mode) {
				apu_step_all_envelope(apu);
				apu_step_all_sweep_and_length(apu);
			}
		}
	}

	// Mix
	apu_dac_mix(&apu->dac, apu->p[0].output, apu->p[1].output, apu->p[2].output,
		apu->p[3].output, apu->p6[0].output, apu->p6[1].output, apu->c[0].output,
		apu->c[1].output, apu->c[2].output, apu->s.output, apu->t.output, apu->n.output,
		apu->d.output);

	apu->frame_counter++;
}

void apu_assert_irqs(struct apu *apu, struct cpu *cpu)
{
	cpu_irq(cpu, IRQ_DMC, apu->d.irq_flag);
	cpu_irq(cpu, IRQ_APU, apu->frame_irq);
}

const int16_t *apu_frames(struct apu *apu, uint32_t *count)
{
	*count = (uint32_t) (apu->dac.buf_offset / 2);
	apu->dac.buf_offset = 0;

	return apu->dac.buf;
}


// Configuration

static uint32_t apu_get_clock(struct apu *apu)
{
	return APU_CLOCK + (apu->dac.cfg.preNMI + apu->dac.cfg.postNMI) * (APU_CLOCK / 262);
}

void apu_set_config(struct apu *apu, const NES_Config *cfg)
{
	apu->dac.cfg = *cfg;

	apu_dac_clock_math(&apu->dac, apu_get_clock(apu));

	OC_SHIFT = (uint8_t) ((cfg->preNMI + cfg->postNMI) / 262);
}

void apu_clock_drift(struct apu *apu, uint32_t clock, bool over)
{
	uint32_t apu_clock = apu_get_clock(apu);

	if (abs((int32_t) clock - (int32_t) apu_clock) < 5000 * (OC_SHIFT + 1))
		apu_dac_clock_math(&apu->dac, clock + (over ? 1000 : -1000) * (OC_SHIFT + 1));
}


// Lifecycle

void apu_create(const NES_Config *cfg, struct apu **apu)
{
	struct apu *ctx = *apu = calloc(1, sizeof(struct apu));

	apu_set_config(ctx, cfg);
	apu_dac_create(&ctx->dac);
}

void apu_destroy(struct apu **apu)
{
	if (!apu || !*apu)
		return;

	free(*apu);
	*apu = NULL;
}

void apu_reset(struct apu *apu, NES *nes, bool hard)
{
	memset(apu->p, 0, sizeof(struct pulse) * 4);
	memset(apu->p6, 0, sizeof(struct pulse6) * 2);
	memset(apu->c, 0, sizeof(struct ss5b) * 3);
	memset(&apu->s, 0, sizeof(struct saw));
	memset(&apu->t, 0, sizeof(struct triangle));
	memset(&apu->n, 0, sizeof(struct noise));
	memset(&apu->d, 0, sizeof(struct dmc));

	for (uint8_t x = 0; x < 4; x++)
		apu->p[x].len.enabled = apu->p[x].len.next_enabled = true;

	apu->n.shift_register = 1;
	apu->n.len.enabled = apu->n.len.next_enabled = true;
	apu->frame_counter = 0;
	apu->t.pop = false;
	apu->frame_irq = false;
	apu->ext = EXT_NONE;

	apu_write(apu, nes, 0x4015, 0x00, EXT_NONE);

	if (hard) {
		apu->mode = apu->next_mode = false;
		apu->irq_disabled = false;
		apu->t.len.enabled = apu->t.len.next_enabled = true;
		apu_write(apu, nes, 0x4017, 0x00, EXT_NONE);
	}

	apu->delayed_reset = 0;
}

size_t apu_set_state(struct apu *apu, const void *state, size_t size)
{
	if (size >= sizeof(struct apu)) {
		*apu = *((struct apu *) state);

		return sizeof(struct apu);
	}

	return 0;
}

void *apu_get_state(struct apu *apu, size_t *size)
{
	*size = sizeof(struct apu);

	struct apu *state = malloc(*size);
	*state = *apu;

	return state;
}
