#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include "Input.h"
#include <cassert>
#include <wrl.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {

    HRESULT result;

    // DirectInputのインスタンス生成
    result = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()),
        nullptr
    );
    assert(SUCCEEDED(result));

    // キーボードデバイス生成
    result = directInput_->CreateDevice(
        GUID_SysKeyboard,
        keyboard_.GetAddressOf(),
        nullptr
    );
    assert(SUCCEEDED(result));

    // 入力データ形式のセット
    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    // 排他制御レベルのセット
    result = keyboard_->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND |
        DISCL_NONEXCLUSIVE |
        DISCL_NOWINKEY
    );
    assert(SUCCEEDED(result));
}