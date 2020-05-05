struct VS_OUTPUT {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

SamplerState ss {
};

Texture2D frame: register(t0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 rgba = frame.Sample(ss, input.texcoord);

	if (fmod(input.texcoord.y, 2.0) > 0.0)
		rgba.a = rgba.r = rgba.g = rgba.b = 0.0;

	return float4(rgba.b, rgba.g, rgba.r, rgba.a);
}
