struct VS_OUTPUT {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

cbuffer VS_CONSTANT_BUFFER : register(b0) {
	float width;
	float height;
	float constrain_w;
	float constrain_h;
	uint filter;
	uint effect;
	uint __pad[2];
};

SamplerState ss {
};

Texture2D frame : register(t0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 rgba;

	// Gaussian Sharp
	if (filter == 3 || filter == 4) {
		float2 res = float2(width, height);
		float2 p = input.texcoord * res;
		float2 c = floor(p) + 0.5;
		float2 dist = p - c;
		if (filter == 3) {
			dist = 16.0 * dist * dist * dist * dist * dist;
		} else {
			dist = 4.0 * dist * dist * dist;
		}
		p = c + dist;

		rgba = frame.Sample(ss, p / res);

	// No filter
	} else {
		rgba = frame.Sample(ss, input.texcoord);
	}

	// Scanlines
	if (effect == 1 || effect == 2) {
		float n = (effect == 1) ? 1.0 : 2.0;
		float cycle = fmod(floor(input.texcoord.y * constrain_h), n * 2.0);

		if (cycle < n) {
			float2 res = float2(width, height);
			float2 p = input.texcoord * res;

			p.y -= 1.0;
			float4 top = frame.Sample(ss, p / res);
			p.y += 2.0;
			float4 bot = frame.Sample(ss, p / res);

			rgba = float4(
				((top.r + bot.r) / 2.0 + rgba.r * 4.0) / 5.0,
				((top.g + bot.g) / 2.0 + rgba.g * 4.0) / 5.0,
				((top.b + bot.g) / 2.0 + rgba.b * 4.0) / 5.0,
				((top.a + bot.a) / 2.0 + rgba.a * 4.0) / 5.0) * 0.80;
		}
	}

	return float4(rgba.b, rgba.g, rgba.r, rgba.a);
}
