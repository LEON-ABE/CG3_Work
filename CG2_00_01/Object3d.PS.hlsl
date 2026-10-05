struct Material
{
    float4 color;
    int enableLighting;
    float3 padding; 
    float4x4 uvTransform; 
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

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
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    //サンプリング
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // textureのα値が0.5以下のときにPixelを棄却
    if (textureColor.a <= 0.5f)
    {
        discard;
    }

    float4 finalColor = gMaterial.color * textureColor;
    
    if (gMaterial.enableLighting != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        finalColor.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        finalColor.a = gMaterial.color.a * textureColor.a;
    }
    
    return finalColor;
}