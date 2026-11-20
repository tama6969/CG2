

#include <cstdint>
#include <string>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <cmath> 
#include <wrl.h>
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
#include <xaudio2.h>
#include "MatrixMath.h"
#include "SphereMesh.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "D3DResourceLeakChecker.h"
#include "Logger.h"
#include "StringUtility.h"
#include "DebugCamera.h"
#include "Particle.h"
#include "BlendMode.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"                         
#include "externals/imgui/imgui_impl_dx12.h"       
#include "externals/imgui/imgui_impl_win32.h"      
#endif

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib,"xaudio2.lib")
using Microsoft::WRL::ComPtr;
class ResourceObject {
public:
	// コンストラクタ
	ResourceObject(ID3D12Resource* resource)
		: resource_(resource) {
	}
	// デストラクタ
	~ResourceObject() {
		if (resource_) {
			resource_->Release();
		}
	}
	// 中のResourceを取得
	ID3D12Resource* Get() {
		return resource_;
	}

private:
	ResourceObject(const ResourceObject&) = delete;
	ResourceObject& operator=(const ResourceObject&) = delete;
	ID3D12Resource* resource_;
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};
//チャンクデータ
struct ChunkHeader {
	char id[4];
	uint32_t size;
};
//RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};
//フォーマットチャンク
struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};
//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの戦闘サイズ
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};
SoundData SoundLoadWave(const char* filename) {
	//HRESULT hr;
	//ファイルを開く
	std::ifstream file;
	//.wavファイルをバイナリーモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイル開くのを失敗を検出
	assert(file.is_open());

	//.wavデータの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	//ファイルかRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	//タイプかWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}
	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}
	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	//データチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	//JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再読み込み
		file.read((char*)&data, sizeof(data));
	}
	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}
	//Dataチャンクのデータ部の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	//Waveファイルを閉じる
	file.close();

	//読み込んだ音声データをreturn
	//returnするための音声データ
	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;
	return soundData;
}
//音声データの解放
void SoundUnload(SoundData* soundData) {
	//バッファの解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}
//音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT hr;
	//波形フォーマットをもとにSourceVoiceを作成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(hr));
	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	//波形データの再生
	hr = pSourceVoice->SubmitSourceBuffer(&buf);
	hr = pSourceVoice->Start();

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





[[nodiscard]]


ID3D12Resource* CreateDepthStencilTextureResource(const ComPtr<ID3D12Device>& device, int32_t width, int32_t height) {
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






;

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// WinMain内のComPtrがすべて解放されたあと、最後にリークチェックする

	D3DResourceLeakChecker leakCheck;

	// Windows APIの初期化
	HRESULT hr;
	WinApp* winApp = new WinApp();
	winApp->Initialize();
	Logger::Log("Hello,DirectX!!\n");


	// DirectXの初期化
	DirectXCommon* dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);
	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// XAudio2の初期化
	ComPtr<IXAudio2> xaudio2;
	IXAudio2MasteringVoice* masterVoice;

	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// Particleの頂点シェーダー(Instancing用)
	ComPtr<IDxcBlob> particleVertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(particleVertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> particlePixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(particlePixelShaderBlob != nullptr);

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

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
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
	// ブレンドステートの生成
	BlendState blendState;
	// ブレンドステートの初期化5
	blendState.Initialize();

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendState.GetBlendDesc(BlendMode::kNone);
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
	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(graphicsPipelineState.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// Particle専用RootSignature。Object3d用のRootSignatureは変更しない。
	// VSのt0: 粒ごとのTransformationMatrix (StructuredBuffer)
	D3D12_DESCRIPTOR_RANGE particleInstanceRange[1] = {};
	particleInstanceRange[0].BaseShaderRegister = 0;
	particleInstanceRange[0].NumDescriptors = 1;
	particleInstanceRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleInstanceRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// PSのt0: Texture2D。VS側とregister(t0)が同じでもShaderVisibilityが異なる。
	D3D12_DESCRIPTOR_RANGE particleTextureRange[1] = {};
	particleTextureRange[0].BaseShaderRegister = 0;
	particleTextureRange[0].NumDescriptors = 1;
	particleTextureRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleTextureRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER particleRootParameters[3] = {};
	// [0] PSのb0: ParticleのMaterial
	particleRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	particleRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[0].Descriptor.ShaderRegister = 0;

	// [1] VSのt0: 10粒分の行列を参照するSRV
	particleRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	particleRootParameters[1].DescriptorTable.pDescriptorRanges = particleInstanceRange;
	particleRootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(particleInstanceRange);

	// [2] PSのt0: Texture2D用SRV
	particleRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[2].DescriptorTable.pDescriptorRanges = particleTextureRange;
	particleRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(particleTextureRange);

	D3D12_ROOT_SIGNATURE_DESC particleRootSignatureDesc = descriptionRootSignature;
	particleRootSignatureDesc.pParameters = particleRootParameters;
	particleRootSignatureDesc.NumParameters = _countof(particleRootParameters);
	ComPtr<ID3DBlob> particleSignatureBlob;
	ComPtr<ID3DBlob> particleErrorBlob;
	hr = D3D12SerializeRootSignature(
		&particleRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&particleSignatureBlob,
		&particleErrorBlob
	);
	if (FAILED(hr)) {
		if (particleErrorBlob != nullptr) {
			Logger::Log(reinterpret_cast<const char*>(particleErrorBlob->GetBufferPointer()));
		}
		assert(false);
	}
	ComPtr<ID3D12RootSignature> particleRootSignature;
	hr = device->CreateRootSignature(
		0,
		particleSignatureBlob->GetBufferPointer(),
		particleSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(particleRootSignature.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePipelineDesc = graphicsPipelineStateDesc;
	particlePipelineDesc.pRootSignature = particleRootSignature.Get();
	particlePipelineDesc.VS = {
		particleVertexShaderBlob->GetBufferPointer(), particleVertexShaderBlob->GetBufferSize()
	};
	particlePipelineDesc.PS = {
		particlePixelShaderBlob->GetBufferPointer(), particlePixelShaderBlob->GetBufferSize()
	};
	particlePipelineDesc.BlendState = blendState.GetBlendDesc(BlendMode::kNormal);
	particlePipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	particlePipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// ImGuiからブレンドを切り替えるため、Particle用のPSOを3種類作成する。
	ComPtr<ID3D12PipelineState> particlePipelineStates[3];
	BlendMode particleBlendModes[3] = { BlendMode::kNone, BlendMode::kNormal, BlendMode::kAdd };
	for (int32_t index = 0; index < 3; ++index) {
		particlePipelineDesc.BlendState = blendState.GetBlendDesc(particleBlendModes[index]);
		hr = device->CreateGraphicsPipelineState(
			&particlePipelineDesc, IID_PPV_ARGS(particlePipelineStates[index].GetAddressOf())
		);
		assert(SUCCEEDED(hr));
	}

	// 3Dオブジェクト用 頂点バッファ
	//モデル読み込み
	ModelData modelData = LoadObjFile("resources/fence", "fence.obj");
	//頂点リソースを作る
	ComPtr<ID3D12Resource> vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
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

	ComPtr<ID3D12Resource> vertexResourceSprite = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
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
	ComPtr<ID3D12Resource> indexResourceSprite = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);
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
	ComPtr<ID3D12Resource> sphereVertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * sphereVertices.size());
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
	ComPtr<ID3D12Resource> sphereIndexResource = dxCommon->CreateBufferResource(sizeof(uint32_t) * sphereIndices.size());
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
	ComPtr<ID3D12Resource> sphereWvpResource = dxCommon->CreateBufferResource(sphereWvpBufferSize);
	assert(sphereWvpResource != nullptr);

	TransformationMatrix* sphereWvpData = nullptr;
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereWvpData));

	ComPtr<ID3D12Resource> indexResource = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);
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
	ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(materialBufferSize);
	assert(materialResource != nullptr);

	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true;
	materialData->uvTransform = MakeIdentity4x4();
	materialResource->Unmap(0, nullptr);

	size_t materialSpriteBufferSize = (sizeof(Material) + 255) & ~255;
	ComPtr<ID3D12Resource> materialResourceSprite = dxCommon->CreateBufferResource(materialSpriteBufferSize);
	assert(materialResourceSprite != nullptr);

	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();
	materialResourceSprite->Unmap(0, nullptr);

	// Particle専用Material: 色変更が既存Spriteへ影響しないように分ける。
	size_t particleMaterialBufferSize = (sizeof(Material) + 255) & ~255;
	ComPtr<ID3D12Resource> particleMaterialResource = dxCommon->CreateBufferResource(particleMaterialBufferSize);
	assert(particleMaterialResource != nullptr);
	Material* particleMaterialData = nullptr;
	hr = particleMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&particleMaterialData));
	assert(SUCCEEDED(hr));
	particleMaterialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	particleMaterialData->enableLighting = false;
	particleMaterialData->uvTransform = MakeIdentity4x4();

	// 平行光源用CBufferリソースの作成
	size_t directionalLightBufferSize = (sizeof(DirectionalLight) + 255) & ~255;
	ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(directionalLightBufferSize);
	assert(directionalLightResource != nullptr);

	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// 3Dオブジェクト WVP用CBufferリソースの作成
	size_t wvpBufferSize = (sizeof(TransformationMatrix) + 255) & ~255;
	ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(wvpBufferSize);
	assert(wvpResource != nullptr);

	TransformationMatrix* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	size_t spriteWvpBufferSize = (sizeof(TransformationMatrix) + 255) & ~255;
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite = dxCommon->CreateBufferResource(spriteWvpBufferSize);
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
	// 1枚目のテクスチャ読み込みとSRV生成
	DirectX::ScratchImage mipImages = DirectXCommon::LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource = dxCommon->CreateTextureResource(metadata);
	ComPtr<ID3D12Resource> intermediateResource =
		dxCommon->UploadTextureData(textureResource, mipImages);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	DirectX::ScratchImage mipImages2 = DirectXCommon::LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ComPtr<ID3D12Resource> textureResource2 = dxCommon->CreateTextureResource(metadata2);
	ComPtr<ID3D12Resource> intermediateResource2 =
		dxCommon->UploadTextureData(textureResource2, mipImages2);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(2);
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	Transform transform{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	bool useMonsterBall = true;
	// XAudioエンジンのインスタンスを生成
	hr = XAudio2Create(&xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	hr = xaudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(hr));
	SoundData soundData1 = SoundLoadWave("Resources/fanfare.wav");
	SoundPlayWave(xaudio2.Get(), soundData1);

	// キーボード入力クラスの初期化
	Input* input = new Input();
	input->Initialize(winApp);

	// SRVヒープの3番を粒ごとのStructuredBufferに使用する(0=ImGui, 1/2=テクスチャ)
	Particle particle;
	particle.Initialize(device, dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 3);

	DebugCamera debugCamera;
	debugCamera.Initialize();

	while (true)
	{
		// Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			break;
		}
		// 毎フレームのキーボード入力を更新
		input->Update();

		// DebugCameraは今までどおりBYTE配列を受け取る
		debugCamera.Update(input->GetKey());

		// 数字の0キーが押されていたら
		if (input->PushKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");
		}

#ifdef USE_IMGUI
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("FPS");
		ImGui::Text("FPS : %.1f", ImGui::GetIO().Framerate);
		ImGui::End();
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

		// 録画のSettingsウィンドウと同様に、描画中にパラメータを操作する。
		ImGui::SetNextWindowPos(ImVec2(895.0f, 45.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(345.0f, 415.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Particle Settings");
		Particle::Settings& particleSettings = particle.GetSettings();
		ImGui::Checkbox("Enable##Particle", &particleSettings.enabled);
		ImGui::Checkbox("Update##Particle", &particleSettings.update);
		ImGui::SliderInt("Count##Particle", &particleSettings.count, 0, static_cast<int>(Particle::kNumMaxInstance));
		ImGui::SliderFloat("Size##Particle", &particleSettings.size, 0.1f, 3.0f);
		ImGui::SliderFloat("Speed##Particle", &particleSettings.speed, 0.0f, 5.0f);
		ImGui::DragFloat3("Position##Particle", &particleSettings.position.x, 0.05f, -20.0f, 20.0f);
		ImGui::Separator();
		ImGui::ColorEdit4("Color##Particle", &particleMaterialData->color.x);
		const char* blendItems[] = { "None", "Normal", "Add" };
		ImGui::Combo("Blend##Particle", &particleSettings.blendMode, blendItems, 3);
		ImGui::Separator();
		ImGui::SliderFloat("Spawn Range##Particle", &particleSettings.spawnRange, 0.0f, 10.0f);
		ImGui::SliderFloat("Velocity Range##Particle", &particleSettings.velocityRange, 0.0f, 3.0f);
		if (ImGui::Button("Randomize / Reset")) {
			particle.ResetParticles();
		}
		ImGui::TextDisabled("Spawn and velocity ranges apply on Reset.");
		ImGui::End();
#endif
		transform.rotate.y += 0.03f;
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

		Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
		Matrix4x4 projectionMatrix = debugCamera.GetProjectionMatrix();
		Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix4x4(worldMatrix, MultiplyMatrix4x4(viewMatrix, projectionMatrix));
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;

		// 粒10個ぶんのWVP行列を更新
		particle.Update(viewMatrix, projectionMatrix);

		// 球体の行列更新
		Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
		Matrix4x4 sphereWvpMatrix = MultiplyMatrix4x4(sphereWorldMatrix, MultiplyMatrix4x4(viewMatrix, projectionMatrix));
		sphereWvpData->WVP = sphereWvpMatrix;
		sphereWvpData->World = sphereWorldMatrix;

		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
		Matrix4x4 orthographicMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
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

		// 描画前処理
		dxCommon->PreDraw();

		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(graphicsPipelineState.Get());

		// ImGuiで既存のDescriptorHeapを再指定するために残す
		ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
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

		// 10粒を一度のDrawInstancedで描画
		// Particle Settingsに応じてBlend PSOと専用Materialを使う。
		int32_t particleBlendIndex = particle.GetSettings().blendMode;
		if (particleBlendIndex < 0 || particleBlendIndex > 2) {
			particleBlendIndex = 1;
		}
		particle.Draw(commandList, particleRootSignature.Get(), particlePipelineStates[particleBlendIndex].Get(),
			particleMaterialResource->GetGPUVirtualAddress(), textureSrvHandleGPU);

		// 以降は既存3D用RootSignature/PSOへ戻す(スプライト描画用)
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(graphicsPipelineState.Get());
		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

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

		// 描画後処理
		dxCommon->PostDraw();

		// テクスチャ転送の実行完了後、中間リソースを解放する
		intermediateResource.Reset();
		intermediateResource2.Reset();

	}
	// XAudio2解放
	xaudio2.Reset();

	// 音声データ解放
	SoundUnload(&soundData1);


	// Particle専用マテリアルを解放
	particleMaterialResource->Unmap(0, nullptr);

	// 球体用解放
	sphereWvpResource->Unmap(0, nullptr);

	transformationMatrixResourceSprite->Unmap(0, nullptr);
	wvpResource->Unmap(0, nullptr);
	directionalLightResource->Unmap(0, nullptr);


#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
#ifdef _DEBUG
	// debugControllerもComPtrが自動解放するk
#endif

	// DirectX解放
	delete dxCommon;

	// ウィンドウ破棄前にDirectInputを解放
	delete input;
	input = nullptr;

	// WindowsAPIの終了処理
	winApp->Finalize();
	delete winApp;
	return 0;
}