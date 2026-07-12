struct Material
{
    float4 color;
    int enableLighting;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 finalColor = gMaterial.color * textureColor;

    if (gMaterial.enableLighting != 0)
    {
       
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
 
        finalColor = gMaterial.color * gDirectionalLight.color * cos * gDirectionalLight.intensity * textureColor;
    }
    
    return finalColor;
}