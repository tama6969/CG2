#pragma once

#include <cstdint>
#include <random>
#include "MatrixMath.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

// 四角形を1回のDrawInstancedで複数描画するパーティクル
class Particle {
public:
    static constexpr uint32_t kNumMaxInstance = 10;

    // ImGuiから調整する値。描画・移動に毎フレーム反映される。
    struct Settings {
        bool enabled = true;
        bool update = true;
        int count = 10;
        float size = 1.0f;
        float speed = 1.0f;
        float spawnRange = 1.0f;
        float velocityRange = 1.0f;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        int blendMode = 1; // 0:None, 1:Normal, 2:Add
    };

    Settings& GetSettings() { return settings_; }
    const Settings& GetSettings() const { return settings_; }
    void ResetParticles();

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
 
    struct ParticleForGPU {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };
    struct ParticleData {
        Transform transform;
        Vector3 velocity;
    };

    Settings settings_{};
    std::vector<ParticleData> particles_;
    uint32_t numInstance_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    std::mt19937 randomEngine_;
};
