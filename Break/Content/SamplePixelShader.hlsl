struct PS_INPUT
{
    float4 inPosition : SV_POSITION;
    float3 inTexCoord : TEXCOORD; // Changed to float3 for 3D texture coordinates
};

Texture3D objTexture : register(t0); // Changed to Texture3D
SamplerState objSamplerState : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 pixelColor = objTexture.Sample(objSamplerState, input.inTexCoord); // Sampling with float3
    return float4(pixelColor.rgba); // Ensure alpha is set to 1.0f
}