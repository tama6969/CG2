#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(
        float4(input.texcoord, 0.0f, 1.0f),
        gMaterial.uvTransform
    );

    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

        // RGB（色）にはライティングを適用
        output.color.rgb =
            gMaterial.color.rgb *
            textureColor.rgb *
            gDirectionalLight.color.rgb *
            cos *
            gDirectionalLight.intensity;

        // Alpha（透明度）にはライティングを適用しない
        output.color.a =
            gMaterial.color.a *
            textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    // ========================================
    // 2値抜き（Binary Alpha / αTest）
    // ========================================

    // α値が0.5以下のPixelを破棄する
    if (output.color.a <= 0.5f)
    {
        discard;
    }

    return output;
}