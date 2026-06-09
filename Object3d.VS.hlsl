#include "Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 行列を掛けて座標を変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // 4. VertexShaderOutputにtexcoordをそのまま渡す
    output.texcoord = input.texcoord;
    
    return output;
}