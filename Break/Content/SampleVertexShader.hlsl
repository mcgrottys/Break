cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
    matrix model;
    matrix view;
    matrix projection;
};

struct VertexShaderInput
{
    float3 inPos : POSITION;
    float3 inTexCoord : TEXCOORD; // Changed from float2 to float3
};

struct PixelShaderInput
{
    float4 outPosition : SV_POSITION;
    float3 outTexCoord : TEXCOORD; // Changed from float2 to float3
};

PixelShaderInput main(VertexShaderInput input)
{
    PixelShaderInput output;
    float4 pos = float4(input.inPos, 1.0f);

    pos = mul(pos, model);
    pos = mul(pos, view);
    pos = mul(pos, projection);
    output.outPosition = pos;
	
    output.outTexCoord = input.inTexCoord; // Pass the 3D texture coordinate

    return output;
}
