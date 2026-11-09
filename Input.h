#pragma once

#include <Windows.h>
#include <dinput.h>
#include <wrl.h>


class Input {
public:
    // 初期化
    void Initialize(HINSTANCE hInstance, HWND hwnd);

private:
    // DirectInput
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

    // キーボード
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
};