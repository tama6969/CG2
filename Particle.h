#pragma once

#include <cstdint>
#include "MatrixMath.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

// 四角形を1回のDrawInstancedで複数描画するパーティクル
class Particle {
public:
    static constexpr uint32_t kNumMaxInstance = 10;

    void Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        uint32_t srvIndex
    );

    void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState,
        D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSRV
    ) const;

    ~Particle();

private:
    // C++のデータレイアウトとParticle.VS.hlslの構造体を合わせる
    struct ParticleForGPU {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // 粒ごとのTransform。今回は授業資料に合わせて10個を固定配置する。
    std::vector<Transform> transforms_;
    uint32_t numInstance_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};
};
