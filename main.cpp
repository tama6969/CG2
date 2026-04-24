#include <windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <format>
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
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	Log("Hello,DirectX!!\n");

	std::string str0{ "STRING!!" };
	std::string str1_{ std::to_string(10) };

	std::filesystem::create_directory("logs");

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