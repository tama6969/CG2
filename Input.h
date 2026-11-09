#pragma once

#include <Windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>
#include <wrl.h>

class Input {
private:
   
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // DirectInputの初期化（既存の引数は変更しない）
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    // 毎フレーム、キーボード情報を取得する
    void Update();

    const BYTE* GetKey() const;

    bool PushKey(BYTE key) const;

    // 押した瞬間のフレームだけtrue
    bool TriggerKey(BYTE key) const;

    ~Input();

private:

    ComPtr<IDirectInput8> directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    BYTE key_[256] = {};
    BYTE previousKey_[256] = {};
};
