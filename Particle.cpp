#include "Particle.h"
#include <cassert>
#include <cstring>

namespace {

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateParticleBuffer(
        ID3D12Device* device,
        size_t sizeInBytes
    ) {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = sizeInBytes;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(resource.GetAddressOf())
        );
        assert(SUCCEEDED(hr));
        return resource;
    }

} // namespace

void Particle::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    uint32_t srvIndex
) {
    assert(device != nullptr);
    assert(srvDescriptorHeap != nullptr);

    // 資料の四角形: 6頂点(三角形2枚)。頂点情報は全粒で共通
    const VertexData vertices[6] = {
        { {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    };

    vertexResource_ = CreateParticleBuffer(device, sizeof(vertices));
    VertexData* mappedVertices = nullptr;
    HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    assert(SUCCEEDED(hr));
    std::memcpy(mappedVertices, vertices, sizeof(vertices));
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(vertices));
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 粒ごとのTransform(WVP + World)をStructuredBufferとしてGPUに渡す
    instancingResource_ = CreateParticleBuffer(device, sizeof(ParticleForGPU) * kNumMaxInstance);
    hr = instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
    assert(SUCCEEDED(hr));

    for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
        instancingData_[index].WVP = MakeIdentity4x4();
        instancingData_[index].World = MakeIdentity4x4();
        instancingData_[index].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    // StructuredBuffer用SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kNumMaxInstance;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = srvDescriptorHeap->GetDesc();
    assert(srvIndex < heapDesc.NumDescriptors);
    assert((heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0);

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandleCPU.ptr += static_cast<SIZE_T>(descriptorSizeSRV) * srvIndex;
    instancingSrvHandleGPU_ = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    instancingSrvHandleGPU_.ptr += static_cast<UINT64>(descriptorSizeSRV) * srvIndex;
    device->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, srvHandleCPU);

    // 乱数生成器を1度だけ初期化する。
    std::random_device seedGenerator;
    randomEngine_.seed(seedGenerator());
    ResetParticles();
}

// 授業資料のMakeNewParticle関数。
// Particleは既存クラス名なので、粒データはParticleDataとして定義している。
Particle::ParticleData Particle::MakeNewParticle(std::mt19937& randomEngine) const {
    const float spawnRange = settings_.spawnRange < 0.0f ? 0.0f : settings_.spawnRange;
    const float velocityRange = settings_.velocityRange < 0.0f ? 0.0f : settings_.velocityRange;
    std::uniform_real_distribution<float> positionDistribution(-spawnRange, spawnRange);
    std::uniform_real_distribution<float> velocityDistribution(-velocityRange, velocityRange);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    ParticleData particle{};
    particle.transform.scale = { 1.0f, 1.0f, 1.0f };
    particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
    particle.transform.translate = {
        positionDistribution(randomEngine),
        positionDistribution(randomEngine),
        positionDistribution(randomEngine)
    };
    particle.velocity = {
        velocityDistribution(randomEngine),
        velocityDistribution(randomEngine),
        velocityDistribution(randomEngine)
    };
    particle.color = {
        distColor(randomEngine),
        distColor(randomEngine),
        distColor(randomEngine),
        1.0f
    };
    particle.lifeTime = distTime(randomEngine);
    particle.currentTime = 0.0f;
    return particle;
}

// 初期位置・速度・色を再抽選。ImGuiのRandomize / Resetからも呼び出せる。
void Particle::ResetParticles() {
    particles_.clear();
    particles_.reserve(kNumMaxInstance);
    for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
        particles_.push_back(MakeNewParticle(randomEngine_));
    }
    const int clampedCount = settings_.count < 0 ? 0 :
        (settings_.count > static_cast<int>(kNumMaxInstance) ? static_cast<int>(kNumMaxInstance) : settings_.count);
    numInstance_ = static_cast<uint32_t>(clampedCount);
}

void Particle::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
    // 授業資料の固定60fps想定。将来は実測deltaTimeに置き換える。
    const float kDeltaTime = 1.0f / 60.0f;
    const Matrix4x4 viewProjectionMatrix = MultiplyMatrix4x4(viewMatrix, projectionMatrix);
    const int clampedCount = settings_.count < 0 ? 0 :
        (settings_.count > static_cast<int>(kNumMaxInstance) ? static_cast<int>(kNumMaxInstance) : settings_.count);
    numInstance_ = 0;

    for (uint32_t index = 0; index < static_cast<uint32_t>(clampedCount); ++index) {
        // 寿命を過ぎた粒は更新も描画も行わない。
        if (particles_[index].lifeTime <= particles_[index].currentTime) {
            continue;
        }

        if (settings_.update) {
            particles_[index].transform.translate.x += particles_[index].velocity.x * kDeltaTime * settings_.speed;
            particles_[index].transform.translate.y += particles_[index].velocity.y * kDeltaTime * settings_.speed;
            particles_[index].transform.translate.z += particles_[index].velocity.z * kDeltaTime * settings_.speed;
            particles_[index].currentTime += kDeltaTime;
        }

        Transform transform = particles_[index].transform;
        transform.scale = { settings_.size, settings_.size, settings_.size };
        transform.translate.x += settings_.position.x;
        transform.translate.y += settings_.position.y;
        transform.translate.z += settings_.position.z;

        Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
        Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix4x4(worldMatrix, viewProjectionMatrix);
        instancingData_[numInstance_].WVP = worldViewProjectionMatrix;
        instancingData_[numInstance_].World = worldMatrix;
        instancingData_[numInstance_].color = particles_[index].color;

        // 経過時間が生存可能時間に近づくにつれて透明にする。
        float alpha = 1.0f - (particles_[index].currentTime / particles_[index].lifeTime);
        instancingData_[numInstance_].color.w = alpha;
        ++numInstance_;
    }
}

void Particle::Draw(
    ID3D12GraphicsCommandList* commandList,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState,
    D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSRV
) const {
    if (!settings_.enabled || numInstance_ == 0) {
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->SetGraphicsRootConstantBufferView(0, materialCBV);
    commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);
    commandList->SetGraphicsRootDescriptorTable(2, textureSRV);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(6, numInstance_, 0, 0);
}

Particle::~Particle() {
    if (instancingResource_ != nullptr && instancingData_ != nullptr) {
        instancingResource_->Unmap(0, nullptr);
        instancingData_ = nullptr;
    }
}
