
#pragma once
#include <d3d12.h>

// ブレンドモード
enum class BlendMode {
    kNone,
    kNormal,
    kAdd,
    kSubtract,
    kMultiply,
    kScreen,
    kCount
};

class BlendState {
public:
    void Initialize();

    // 指定したブレンドモードの設定を取得
    D3D12_BLEND_DESC GetBlendDesc(BlendMode mode) const;

private:
    D3D12_BLEND_DESC blendDesc_{};
};
