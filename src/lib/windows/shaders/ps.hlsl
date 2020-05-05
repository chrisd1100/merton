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
};

SamplerState ss {
};

Texture2D frame : register(t0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 rgba;

	// Gaussian Sharp
	if (filter == 3) {
		float2 res = float2(width, height);
		float2 p = input.texcoord * res;
		float2 c = floor(p) + 0.5;
		float2 dist = p - c;
		dist = 16.0 * dist * dist * dist * dist * dist;
		p = c + dist;

		rgba = frame.Sample(ss, p / res);

	// No filter
	} else {
		rgba = frame.Sample(ss, input.texcoord);
	}

	// Scanlines
	if (effect == 1) {
		if (fmod(floor(input.texcoord.y * constrain_h), 2.0) == 1.0)
			rgba = float4(rgba.r * 0.85, rgba.g * 0.85, rgba.b * 0.85, rgba.a * 0.85);
	}

	return float4(rgba.b, rgba.g, rgba.r, rgba.a);
}
