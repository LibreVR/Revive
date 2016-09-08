Texture2D leftEye : register(t0);
Texture2D rightEye : register(t1);

SamplerState EyeSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float4 main(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD0) : SV_TARGET
{
	tex.x *= 2.0f;
	if (tex.x < 1.0f)
	{
		return leftEye.Sample(EyeSampler, tex);
	}
	else
	{
		tex.x -= 1.0f;
		return rightEye.Sample(EyeSampler, tex);
	}
}
