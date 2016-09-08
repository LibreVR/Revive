Texture2D eye : register(t0);

SamplerState EyeSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float4 main(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD0) : SV_TARGET
{
	return eye.Sample(EyeSampler, tex);
}
