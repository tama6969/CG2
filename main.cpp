#include <windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <cmath> 
#include <chrono>
#include <cstring>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>	
#include "externals/DirectXTex/DirectXTex.h"
#include "MatrixMath.h"
#include "SphereMesh.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"                         
#include "externals/imgui/imgui_impl_dx12.h"       
#include "externals/imgui/imgui_impl_win32.h"      
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")

IDxcUtils* g_dxcUtils = nullptr;
IDxcCompiler3* g_dxcCompiler = nullptr;
IDxcIncludeHandler* g_includeHandler = nullptr;

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};


std::string ConvertString(const std::wstring& str) {
	if (str.empty()) return {};
	int size = WideCharToMultiByte(CP_UTF8, 0, str.data(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), -1, result.data(), size, nullptr, nullptr);
	result.pop_back();
	return result;
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}
// mtlファイルを読む関数
MaterialData LoadMaterialTemplateFile(
	const std::string& directoryPath,
	const std::string& filename) {

	MaterialData materialData; // 構築するMaterialData
	std::string line;          // ファイルから読んだ1行を格納する

	// mtlファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	// ファイルを1行ずつ読む
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// map_Kdにはテクスチャのファイル名が書かれている
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;

			// ディレクトリ名とファイル名を連結してパスにする
			materialData.textureFilePath =
				directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

//objファイルの読む関数
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {

	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	// ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open()); // 開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		if (identifier == "v") { // 頂点座標
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;

			// 右手系から左手系に変換するため、xを反転
			position.x *= -1.0f;

			positions.push_back(position);
		}
		else if (identifier == "vt") { // テクスチャ座標
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;

			// OBJは左下原点、DirectX側は左上原点なのでY座標を反転
			texcoord.y = 1.0f - texcoord.y;

			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") { // 法線ベクトル
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;

			// 法線もxを反転
			normal.x *= -1.0f;

			normals.push_back(normal);
		}
		else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;

			// OBJファイルと同じディレクトリにあるMTLファイルを読む
			modelData.material =
				LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
		else if (identifier == "f") { // 面情報

			VertexData triangle[3];

			// 面は三角形限定
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				// 頂点要素を分割しIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				// 要素Indexから実際の要素の値を取得して頂点を構築
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				triangle[faceVertex] = { position, texcoord, normal };
			}

			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
	}

	return modelData;
}

// ハンドル取得用関数群
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

// DescriptorHeapの作成関数
ID3D12DescriptorHeap* CreateDescriptorHeap(
	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;
}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,
			img->pixels,
			UINT(img->rowPitch),
			UINT(img->slicePitch)
		);
		assert(SUCCEEDED(hr));
	}
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi", L"-Qembed_debug",
		L"-Od",
		L"-Zpr"
	};

	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includeHandler, IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		assert(false);
	}

	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	shaderSource->Release();
	shaderResult->Release();

	return shaderBlob;
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	CoInitializeEx(0, COINIT_MULTITHREADED);
	Log("Hello,DirectX!!\n");

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	IDXGIFactory7* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	IDXGIAdapter4* useAdapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 desc{};
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));
		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			break;
		}
		useAdapter = nullptr;
	}

	ID3D12Device* device = nullptr;
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
	for (int i = 0; i < 3; i++) {
		hr = D3D12CreateDevice(useAdapter, levels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			break;
		}
	}
	assert(device != nullptr);

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID denyIds[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
		infoQueue->Release();
	}
#endif

	// DescriptorSizeの取得
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wc.hInstance, nullptr);

	ShowWindow(hwnd, SW_SHOW);

	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device,
		2,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvDescriptorHeap,
		GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 0),
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 0)
	);
#endif

	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxcCompiler));
	assert(SUCCEEDED(hr));
	hr = g_dxcUtils->CreateDefaultIncludeHandler(&g_includeHandler);
	assert(SUCCEEDED(hr));

	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", g_dxcUtils, g_dxcCompiler, g_includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", g_dxcUtils, g_dxcCompiler, g_includeHandler);
	assert(pixelShaderBlob != nullptr);

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// 3Dオブジェクト用 頂点バッファ
	//モデル読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");
	//頂点リソースを作る
	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());
	//頂点バッファービューを作る
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書きこむ
	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	vertexResource->Unmap(0, nullptr);

	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
	assert(vertexResourceSprite != nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 2つの三角形
	vertexDataSprite[0].position = { 0.0f,   360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

	vertexDataSprite[1].position = { 0.0f,   0.0f,   0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

	vertexDataSprite[3].position = { 640.0f, 0.0f,   0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	for (uint32_t i = 0; i < 4; ++i) {
		vertexDataSprite[i].normal = { 0.0f, 0.0f, -1.0f };
	}
	vertexResourceSprite->Unmap(0, nullptr);
	ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
	assert(indexResourceSprite != nullptr);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;
	indexResourceSprite->Unmap(0, nullptr);
	// 球体用メッシュデータの生成とバッファ確保
	std::vector<VertexData> sphereVertices;
	std::vector<uint32_t> sphereIndices;
	const uint32_t kSphereSubdivision = 16;
	GenerateSphereMesh(1.0f, kSphereSubdivision, sphereVertices, sphereIndices);

	// 球体用 頂点バッファ
	ID3D12Resource* sphereVertexResource = CreateBufferResource(device, sizeof(VertexData) * sphereVertices.size());
	assert(sphereVertexResource != nullptr);

	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	sphereVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * sphereVertices.size());
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* sphereVertexData = nullptr;
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));
	std::memcpy(sphereVertexData, sphereVertices.data(), sizeof(VertexData) * sphereVertices.size());
	sphereVertexResource->Unmap(0, nullptr);

	// 球体用 インデックスバッファ
	ID3D12Resource* sphereIndexResource = CreateBufferResource(device, sizeof(uint32_t) * sphereIndices.size());
	assert(sphereIndexResource != nullptr);

	D3D12_INDEX_BUFFER_VIEW sphereIndexBufferView{};
	sphereIndexBufferView.BufferLocation = sphereIndexResource->GetGPUVirtualAddress();
	sphereIndexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * sphereIndices.size());
	sphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* sphereIndexData = nullptr;
	sphereIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereIndexData));
	std::memcpy(sphereIndexData, sphereIndices.data(), sizeof(uint32_t) * sphereIndices.size());
	sphereIndexResource->Unmap(0, nullptr);

	// 球体用の位置情報
	Transform sphereTransform{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 1.5f, 0.0f, 0.0f }
	};

	size_t sphereWvpBufferSize = (sizeof(TransformationMatrix) + 255) & ~255;
	ID3D12Resource* sphereWvpResource = CreateBufferResource(device, sphereWvpBufferSize);
	assert(sphereWvpResource != nullptr);

	TransformationMatrix* sphereWvpData = nullptr;
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereWvpData));

	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);
	assert(depthStencilResource != nullptr);
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	assert(dsvDescriptorHeap != nullptr);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, GetCPUDescriptorHandle(dsvDescriptorHeap, descriptorSizeDSV, 0));

	ID3D12Resource* indexResource = CreateBufferResource(device, sizeof(uint32_t) * 6);
	assert(indexResource != nullptr);

	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexData = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0; indexData[1] = 1; indexData[2] = 2;
	indexData[3] = 1; indexData[4] = 3; indexData[5] = 2;
	indexResource->Unmap(0, nullptr);

	// マテリアル用CBufferリソースの作成
	size_t materialBufferSize = (sizeof(Material) + 255) & ~255;
	ID3D12Resource* materialResource = CreateBufferResource(device, materialBufferSize);
	assert(materialResource != nullptr);

	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true;
	materialData->uvTransform = MakeIdentity4x4();
	materialResource->Unmap(0, nullptr);

	size_t materialSpriteBufferSize = (sizeof(Material) + 255) & ~255;
	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, materialSpriteBufferSize);
	assert(materialResourceSprite != nullptr);

	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();
	materialResourceSprite->Unmap(0, nullptr);

	// 平行光源用CBufferリソースの作成
	size_t directionalLightBufferSize = (sizeof(DirectionalLight) + 255) & ~255;
	ID3D12Resource* directionalLightResource = CreateBufferResource(device, directionalLightBufferSize);
	assert(directionalLightResource != nullptr);

	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// 3Dオブジェクト WVP用CBufferリソースの作成
	size_t wvpBufferSize = (sizeof(TransformationMatrix) + 255) & ~255;
	ID3D12Resource* wvpResource = CreateBufferResource(device, wvpBufferSize);
	assert(wvpResource != nullptr);

	TransformationMatrix* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	size_t spriteWvpBufferSize = (sizeof(TransformationMatrix) + 255) & ~255;
	ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, spriteWvpBufferSize);
	assert(transformationMatrixResourceSprite != nullptr);

	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();

	Transform transformSprite{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	// SpriteのUVTransform用パラメータ
	Transform uvTransformSprite{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};
	// 1枚目のテクスチャ読み込みとSRV生成 (index = 1)
	DirectX::ScratchImage mipImages = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource, mipImages);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

	DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

	Transform transform{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	D3D12_VIEWPORT viewport{};
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	ID3D12Resource* swapChainResources[2] = {};
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 0);
	rtvHandles[1] = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 1);

	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);
	bool useMonsterBall = true;
	MSG msg{};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::ShowDemoWindow();
			ImGui::Begin("Window");
			ImGui::SliderFloat3("Scale", &transform.scale.x, 0.0f, 1.0f);
			ImGui::SliderFloat3("Rotate", &transform.rotate.x, 0.0f, 1.0f);
			ImGui::SliderFloat3("Translate", &transform.translate.x, 0.0f, 1.0f);
			ImGui::Separator();
			ImGui::Text("Sprite Transform");

			ImGui::SliderFloat3("Sprite Scale", &transformSprite.scale.x, 0.0f, 5.0f);
			ImGui::SliderFloat("Sprite Rotate Z", &transformSprite.rotate.z, -3.1415f, 3.1415f);
			ImGui::SliderFloat2("Sprite Translate", &transformSprite.translate.x, 0.0f, 1280.0f);

			ImGui::Separator();
			ImGui::Text("Sprite UV Transform");
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);

			ImGui::Separator();
			ImGui::Text("Sphere Transform");
			ImGui::SliderFloat("Sphere Radius", &sphereTransform.scale.x, 0.1f, 3.0f);
			sphereTransform.scale.y = sphereTransform.scale.x;
			sphereTransform.scale.z = sphereTransform.scale.x;
			ImGui::SliderFloat3("Sphere Rotate", &sphereTransform.rotate.x, 0.0f, 6.28f);
			ImGui::SliderFloat3("Sphere Translate", &sphereTransform.translate.x, -5.0f, 5.0f);

			ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			Material* currentMaterialData = nullptr;
			materialResource->Map(0, nullptr, reinterpret_cast<void**>(&currentMaterialData));
			ImGui::ColorEdit4("Color", &currentMaterialData->color.x);
			bool enableLighting = currentMaterialData->enableLighting != 0;
			if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
				currentMaterialData->enableLighting = enableLighting;
			}
			materialResource->Unmap(0, nullptr);

			ImGui::Separator();
			ImGui::Text("Directional Light");
			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
			ImGui::SliderFloat3("Light Direction", &directionalLightData->direction.x, -1.0f, 1.0f);
			ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);
			float length = std::sqrt(
				directionalLightData->direction.x * directionalLightData->direction.x +
				directionalLightData->direction.y * directionalLightData->direction.y +
				directionalLightData->direction.z * directionalLightData->direction.z);
			if (length != 0.0f) {
				directionalLightData->direction.x /= length;
				directionalLightData->direction.y /= length;
				directionalLightData->direction.z /= length;
			}
			ImGui::End();
#endif
			transform.rotate.y += 0.03f;
			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

			Transform cameraTransform{
				{ 1.0f, 1.0f, 1.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, -5.0f }
			};
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix4x4(worldMatrix, MultiplyMatrix4x4(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			// 球体の行列更新
			Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
			Matrix4x4 sphereWvpMatrix = MultiplyMatrix4x4(sphereWorldMatrix, MultiplyMatrix4x4(viewMatrix, projectionMatrix));
			sphereWvpData->WVP = sphereWvpMatrix;
			sphereWvpData->World = sphereWorldMatrix;

			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 orthographicMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 wvpMatrixSprite = MultiplyMatrix4x4(worldMatrixSprite, MultiplyMatrix4x4(viewMatrixSprite, orthographicMatrixSprite));
			transformationMatrixDataSprite->WVP = wvpMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			// SpriteのUVTransform行列を作成してMaterialへ反映
			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = MultiplyMatrix4x4(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = MultiplyMatrix4x4(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			Material* currentMaterialDataSprite = nullptr;
			materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&currentMaterialDataSprite));
			currentMaterialDataSprite->uvTransform = uvTransformMatrix;
			materialResourceSprite->Unmap(0, nullptr);

			// ---- 描画の処理 ----
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &barrier);

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap, descriptorSizeDSV, 0);
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], FALSE, &dsvHandle);

			// カラーバッファのクリア
			FLOAT clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicsPipelineState);

			// 共通記述ヒープの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

			// 3D三角形を描画
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

			//  球体オブジェクトを描画
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			commandList->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress());
			commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView);
			commandList->IASetIndexBuffer(&sphereIndexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(UINT(sphereIndices.size()), 1, 0, 0, 0);

			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			commandList->IASetIndexBuffer(&indexBufferViewSprite);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

#ifdef USE_IMGUI
			ImGui::Render();
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);

			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);

			swapChain->Present(1, 0);
			fenceValue++;
			commandQueue->Signal(fence, fenceValue);

			if (fence->GetCompletedValue() < fenceValue)
			{
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, nullptr);
			assert(SUCCEEDED(hr));
		}
	}

	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue)
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
	CloseHandle(fenceEvent);

	// 球体用解放
	sphereWvpResource->Unmap(0, nullptr);
	sphereWvpResource->Release();
	sphereIndexResource->Release();
	sphereVertexResource->Release();

	transformationMatrixResourceSprite->Unmap(0, nullptr);
	transformationMatrixResourceSprite->Release();
	indexResourceSprite->Release();
	vertexResourceSprite->Release();
	wvpResource->Unmap(0, nullptr);
	wvpResource->Release();
	indexResource->Release();
	directionalLightResource->Unmap(0, nullptr);
	directionalLightResource->Release();
	materialResourceSprite->Release();
	materialResource->Release();
	vertexResource->Release();
	depthStencilResource->Release();
	dsvDescriptorHeap->Release();

	textureResource2->Release();
	textureResource->Release();

	graphicsPipelineState->Release();
	signatureBlob->Release();
	rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();
	fence->Release();
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	srvDescriptorHeap->Release();
	rtvDescriptorHeap->Release();
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();
	commandList->Release();
	commandAllocator->Release();
	commandQueue->Release();
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();
#ifdef _DEBUG
	if (debugController) {
		debugController->Release();
	}
#endif

	DestroyWindow(hwnd);

#ifdef _DEBUG
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
#endif
	CoUninitialize();
	return 0;
}