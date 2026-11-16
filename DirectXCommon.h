#pragma once
#include <array>
#include <cstdint>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include "externals/DirectXTex/DirectXTex.h"
#include "WinApp.h"

class DirectXCommon {
public:
    // DirectXの初期化
    void Initialize(WinApp* winAppParam);

    // 描画前処理
    void PreDraw();
    // 描画後処理
    void PostDraw();

    // getter
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
    // 既存ParticleとImGuiへの接続に必要なSRV情報
    ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap.Get(); }
    uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV; }

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

    /// <summary>
    /// バッファリソースの生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    /// <summary>
    /// テクスチャリソースの生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

    /// <summary>
    /// テクスチャデータの転送
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    /// <summary>
    /// テクスチャファイルの読み込み
    /// </summary>
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // SRVの指定番号のCPUディスクリプタハンドルを取得する
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    // SRVの指定番号のGPUディスクリプタハンドルを取得する
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

private:
    // FPS固定初期化
    void InitializeFixFPS();
    // FPS固定更新
    void UpdateFixFPS();

    // デバイスの初期化
    void InitializeDevice();
    // コマンド関連の初期化
    void InitializeCommand();
    // スワップチェーンの生成
    void CreateSwapChain();
    // 深度バッファの生成
    void CreateDepthStencilResource();
    // 各種デスクリプタヒープの生成
    void CreateDescriptorHeaps();
    // レンダーターゲットビューの初期化
    void InitializeRenderTargetView();
    // 深度ステンシルビューの初期化
    void InitializeDepthStencilView();
    // フェンスの初期化
    void InitializeFence();
    // ビューポート矩形の初期化
    void InitializeViewport();
    // シザリング矩形の初期化
    void InitializeScissorRect();
    // DXCコンパイラの生成
    void InitializeDXC();
    // ImGuiの初期化
    void InitializeImGui();

    // ディスクリプタヒープを生成する
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

    // 指定番号のCPUディスクリプタハンドルを取得する
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);
    // 指定番号のGPUディスクリプタハンドルを取得する
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);

private:
    // 記録時間（FPS固定用）
    std::chrono::steady_clock::time_point reference_;

    // WindowsAPI
    WinApp* winApp = nullptr;
    // DirectX12デバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    // DXGIファクトリ
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
    // コマンド関連
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    // スワップチェーン
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
    // ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
    uint32_t descriptorSizeRTV = 0;
    uint32_t descriptorSizeSRV = 0;
    uint32_t descriptorSizeDSV = 0;
    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;
    // スワップチェーンリソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;
    // RTVハンドル
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtvHandles{};
    // フェンス
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    uint64_t fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    // ビューポート・シザリング矩形
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissorRect{};
    // DXC
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
};
