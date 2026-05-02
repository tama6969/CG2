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
	assert(useAdapter != nullptr);





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
		}
	}

	return 0;
}