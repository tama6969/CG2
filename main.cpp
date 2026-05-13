#include <windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <format>
#include <fstream>
#include <chrono>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")




LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:

		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
std::string ConvertString(const std::wstring& str) {
	if (str.empty()) return {};

	int size = WideCharToMultiByte(
		CP_UTF8, 0,
		str.data(), -1,
		nullptr, 0,
		nullptr, nullptr
	);

	std::string result(size, 0);

	WideCharToMultiByte(
		CP_UTF8, 0,
		str.data(), -1,
		result.data(), size,
		nullptr, nullptr
	);

	result.pop_back();
	return result;
}



void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

void Log(const std::wstring& message) {
	Log(ConvertString(message));
}
//*****************************************************
//
//メイン
// 
//*****************************************************
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {

	
	
	
	
	
	SYSTEMTIME time;
	GetLocalTime(&time);

	wchar_t filePath[MAX_PATH]{};

	CreateDirectory(L"./Dumps", nullptr);

	StringCchPrintfW(
		filePath,
		MAX_PATH,
		L"./Dumps/%04d%02d%02d_%02d%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond
	);

	HANDLE file = CreateFile(
		filePath,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	MINIDUMP_EXCEPTION_INFORMATION info{};
	info.ThreadId = threadId;
	info.ExceptionPointers = exception;
	info.ClientPointers = TRUE;

	MiniDumpWriteDump(
		GetCurrentProcess(),
		processId,
		file,
		MiniDumpNormal,
		&info,
		nullptr,
		nullptr
	);

	return EXCEPTION_EXECUTE_HANDLER;
}
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	SetUnhandledExceptionFilter(ExportDump);

	Log("Hello,DirectX!!\n");

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	IDXGIFactory7* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	IDXGIAdapter4* useAdapter = nullptr;

	for (UINT i = 0;
		dxgiFactory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&useAdapter)
		) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		DXGI_ADAPTER_DESC3 desc{};
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));

		// ソフトウェアGPUは除外
		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(std::format("Use Adapter: {}\n",
				ConvertString(desc.Description)));
			break;
		}

		useAdapter = nullptr;
	}


	ID3D12Device* device = nullptr;

	// 試す機能レベル（高い順）
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};

	const char* levelStrings[] = { "12.2", "12.1", "12.0" };

	for (int i = 0; i < 3; i++) {
		hr = D3D12CreateDevice(
			useAdapter,
			levels[i],
			IID_PPV_ARGS(&device)
		);

		if (SUCCEEDED(hr)) {
			Log(std::format("FeatureLevel : {}\n", levelStrings[i]));
			break;
		}
	}

	assert(device != nullptr);
	Log("Complete create D3D12Device!!\n");

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) 
	{
		// ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		
		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ

			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
		
		//解放
		infoQueue->Release();
	}
#endif
	assert(useAdapter != nullptr);

	ID3D12CommandQueue* commandQueue = nullptr;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	hr = device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue)
	);

	assert(SUCCEEDED(hr));

	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);

	assert(SUCCEEDED(hr));

	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator,
		nullptr,
		IID_PPV_ARGS(&commandList)
	);
	assert(SUCCEEDED(hr));

	std::string str0{ "STRING!!" };
	std::string str1_{ std::to_string(10) };

	std::filesystem::create_directory("logs");

	auto now = std::chrono::system_clock::now();
	auto nowSec = std::chrono::time_point_cast<std::chrono::seconds>(now);

	auto localTime = std::chrono::zoned_time{ std::chrono::current_zone(), nowSec };

	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);

	std::string logFilePath = ("logs/") + dateString + ".log";

	std::ofstream logStream(logFilePath);

	Log(std::format("str0 = {}", str0));
	Log(std::format("str1 = {}", str1_));
	Log(std::format("value = {}", 10));
	// =========================
   // ウィンドウクラス登録
   // =========================
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


	HWND hwnd = CreateWindow(wc.lpszClassName, L"CG2",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left, wrc.bottom - wrc.top,
		nullptr, nullptr, wc.hInstance, nullptr);





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

	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(&swapChain)
	);

	assert(SUCCEEDED(hr));

	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = 2;

	hr = device->CreateDescriptorHeap(
		&rtvDescriptorHeapDesc,
		IID_PPV_ARGS(&rtvDescriptorHeap)
	);

	assert(SUCCEEDED(hr));

	//Fenceの作成
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
	

	ID3D12Resource* swapChainResources[2] = {};

	hr = swapChain->GetBuffer(
		0,
		IID_PPV_ARGS(&swapChainResources[0])
	);
	assert(SUCCEEDED(hr));

	hr = swapChain->GetBuffer(
		1,
		IID_PPV_ARGS(&swapChainResources[1])
	);
	assert(SUCCEEDED(hr));

	// ========================================
// RTVを作成する
// ========================================
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// Heapの先頭を取得
	rtvHandles[0] =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// Descriptor1個分のサイズを取得
	UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV
		);

	// 2つ目のHandle
	rtvHandles[1].ptr =
		rtvHandles[0].ptr + descriptorSize;

	// RTV作成
	device->CreateRenderTargetView(
		swapChainResources[0],
		&rtvDesc,
		rtvHandles[0]
	);

	device->CreateRenderTargetView(
		swapChainResources[1],
		&rtvDesc,
		rtvHandles[1]
	);

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


			// 更新 描画
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// TransitionBarrier


			D3D12_RESOURCE_BARRIER barrier{}; // これで全体をゼロ初期化
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			// Transitionの中身を正しく埋める
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &barrier);

			
			// RenderTarget設定
			commandList->OMSetRenderTargets(
				1,
				&rtvHandles[backBufferIndex],
				FALSE,
				nullptr
			);

			// 画面クリア
			FLOAT clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

			commandList->ClearRenderTargetView(
				rtvHandles[backBufferIndex],
				clearColor,
				0,
				nullptr
			);
			// RenderTarget → Present
			
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			commandList->ResourceBarrier(1, &barrier);
			// コマンド確定
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUへ送信
			ID3D12CommandList* commandLists[] = { commandList };

			commandQueue->ExecuteCommandLists(1,commandLists);

			// 画面交換
			swapChain->Present(1, 0);
			fenceValue++;
			// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence, fenceValue);

			
			// Fenceの値が指定したSignal値にたどり着いているか確認する
			// GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue)
			{
				// 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				// イベント待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}
			// 次フレーム用にReset
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));

			hr = commandList->Reset(
				commandAllocator,
				nullptr
			);
			assert(SUCCEEDED(hr));

		}
	}
	

	CloseHandle(fenceEvent);

	fence->Release();

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
	debugController->Release();
#endif

	DestroyWindow(hwnd);

#ifdef _DEBUG
	// リソースリークチェック
	IDXGIDebug1* debug = nullptr;

	if (SUCCEEDED(DXGIGetDebugInterface1(0,IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL,DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
#endif
	return 0;
}