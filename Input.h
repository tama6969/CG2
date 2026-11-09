#pragma once

#include <Windows.h>

// DirectInputのバージョンはdinput.hより前に指定する
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>
#include <wrl.h>
#include "WinApp.h"

class Input {
private:

    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // 初期化
    void Initialize(WinApp* winApp);

    // 毎フレーム、キーボード情報を取得する
    void Update();

    const BYTE* GetKey() const;

    // 押している間true
    bool PushKey(BYTE key) const;

    // 押した瞬間のフレームだけtrue
    bool TriggerKey(BYTE key) const;

    ~Input();

private:
    // DirectInputとキーボードデバイスをクラスで所有する
    ComPtr<IDirectInput8> directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    BYTE key_[256] = {};
    BYTE previousKey_[256] = {};

    // WindowsAPI
    WinApp* winApp_ = nullptr;
};
