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

}

void Particle::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    uint32_t srvIndex
) {
    assert(device != nullptr);
    assert(srvDescriptorHeap != nullptr);

    //四角形 6頂点 三角形2枚。
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

    //Resourceの作成 10個のWVP / Worldを単位行列で初期化
    for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
        instancingData_[i].WVP = MakeIdentity4x4();
        instancingData_[i].World = MakeIdentity4x4();
    }

    //SRVの作成
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

    // 最大10粒分を確保して少しずつずらす
    transforms_.clear();
    transforms_.reserve(kNumMaxInstance);
    for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
        Transform transform{};
        transform.scale = { 1.0f, 1.0f, 1.0f };
        transform.rotate = { 0.0f, 0.0f, 0.0f };
        float offset = static_cast<float>(index) * 0.1f;
        transform.translate = { offset, offset, offset };
        transforms_.push_back(transform);
    }
    numInstance_ = static_cast<uint32_t>(transforms_.size());
    assert(numInstance_ <= kNumMaxInstance);
}

void Particle::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
    //TransformからWorldとWVPを求めマップしたGPU用Resourceに書く
    const Matrix4x4 viewProjectionMatrix = MultiplyMatrix4x4(viewMatrix, projectionMatrix);
    for (uint32_t index = 0; index < numInstance_; ++index) {
        const Transform& transform = transforms_[index];
        Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
        Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix4x4(worldMatrix, viewProjectionMatrix);
        instancingData_[index].WVP = worldViewProjectionMatrix;
        instancingData_[index].World = worldMatrix;
    }
}

void Particle::Draw(
    ID3D12GraphicsCommandList* commandList,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState,
    D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSRV
) const {
    if (numInstance_ == 0) {
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
