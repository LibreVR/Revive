float4 main( in float2 pos : POSITION, in float2 uv : TEXCOORD0, out float2 tex : TEXCOORD0) : SV_POSITION
{
	tex = uv;
	return float4(pos, 0.0, 1.0);
}
