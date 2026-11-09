#include "Input.h"
#include <cassert>
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(WinApp* winApp) {
    // 借りてきたWinAppのインスタンスを記録
    winApp_ = winApp;
    assert(winApp_ != nullptr);

    HRESULT result;
    result = DirectInput8Create(
        winApp_->GetHInstance(),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()),
        nullptr
    );
    assert(SUCCEEDED(result));

    result = directInput_->CreateDevice(
        GUID_SysKeyboard,
        keyboard_.GetAddressOf(),
        nullptr
    );
    assert(SUCCEEDED(result));

    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    result = keyboard_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY
    );
    assert(SUCCEEDED(result));
}

void Input::Update() {
    // 前フレームの入力を保持し、現在の入力を読み直す
    std::memcpy(previousKey_, key_, sizeof(key_));
    std::memset(key_, 0, sizeof(key_));

    if (keyboard_ == nullptr) {
        return;
    }

    // フォーカスがない場合は取得できないことがある
    HRESULT result = keyboard_->Acquire();
    if (FAILED(result)) {
        return;
    }

    result = keyboard_->GetDeviceState(sizeof(key_), key_);
    if (FAILED(result)) {
        // 一時的なデバイスの取得失敗から復帰を試す
        if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
            HRESULT acquireResult = keyboard_->Acquire();
            if (SUCCEEDED(acquireResult)) {
                result = keyboard_->GetDeviceState(sizeof(key_), key_);
            }
        }
        if (FAILED(result)) {
            std::memset(key_, 0, sizeof(key_));
        }
    }
}

const BYTE* Input::GetKey() const {
    return key_;
}

bool Input::PushKey(BYTE key) const {
    return (key_[key] & 0x80) != 0;
}

bool Input::TriggerKey(BYTE key) const {
    return (key_[key] & 0x80) != 0 &&
        (previousKey_[key] & 0x80) == 0;
}

Input::~Input() {
    if (keyboard_ != nullptr) {
        keyboard_->Unacquire();
    }
    // ComPtrでkeyboard_とdirectInput_を自動解放する
}
