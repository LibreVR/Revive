float4 main( in uint vI : SV_VERTEXID, out float2 tex : TEXCOORD0) : SV_POSITION
{
	tex = float2(vI & 1,vI >> 1);
	return float4(tex * float2(2, -2) + float2(-1, 1), 0, 1);
}
