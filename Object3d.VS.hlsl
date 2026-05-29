
struct TransformationMatrix
{
    float32_t4x4 WVP; // 4x4の行列
};
// WVP用のCBufferを register(b0) で受け取る
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

struct VertexShaderInput
{
    float4 position : POSITION;
};

// 頂点シェーダーのメイン処理
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スライドの通り、mul関数を使って頂点座標に行列を掛け算する
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    return output;
}
	
