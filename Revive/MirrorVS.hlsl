float4 main( in uint vI : SV_VERTEXID, out float2 tex : TEXCOORD0) : SV_POSITION
{
	tex = float2(vI & 1,vI >> 1);
	return float4((tex.x - 0.5f) * 2.0f, -(tex.y - 0.5f) * 2.0f, 0.0f, 1.0f);
}
