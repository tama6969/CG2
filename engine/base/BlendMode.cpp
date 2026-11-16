
#include "BlendMode.h"

void BlendState::Initialize() {

    blendDesc_ = {};

    // 共通設定
    blendDesc_.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    blendDesc_.RenderTarget[0].BlendEnable = TRUE;

    // Alphaの計算
    blendDesc_.RenderTarget[0].SrcBlendAlpha =
        D3D12_BLEND_ONE;

    blendDesc_.RenderTarget[0].BlendOpAlpha =
        D3D12_BLEND_OP_ADD;

    blendDesc_.RenderTarget[0].DestBlendAlpha =
        D3D12_BLEND_ZERO;
}

D3D12_BLEND_DESC BlendState::GetBlendDesc(BlendMode mode) const {

    D3D12_BLEND_DESC desc = blendDesc_;

    switch (mode) {

    case BlendMode::kNone:
        // ブレンドなし
        desc.RenderTarget[0].BlendEnable = FALSE;
        break;

    case BlendMode::kNormal:
        // 通常ブレンド
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_SRC_ALPHA;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_ADD;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_INV_SRC_ALPHA;
        break;

    case BlendMode::kAdd:
        // 加算合成
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_SRC_ALPHA;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_ADD;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_ONE;
        break;

    case BlendMode::kSubtract:
        // 減算合成（逆減算）
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_SRC_ALPHA;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_REV_SUBTRACT;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_ONE;
        break;

    case BlendMode::kMultiply:
        // 乗算合成
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_ZERO;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_ADD;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_SRC_COLOR;
        break;

    case BlendMode::kScreen:
        // スクリーン合成
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_INV_DEST_COLOR;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_ADD;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_ONE;
        break;

    default:
        // 不正な値の場合は通常ブレンド
        desc.RenderTarget[0].SrcBlend =
            D3D12_BLEND_SRC_ALPHA;

        desc.RenderTarget[0].BlendOp =
            D3D12_BLEND_OP_ADD;

        desc.RenderTarget[0].DestBlend =
            D3D12_BLEND_INV_SRC_ALPHA;
        break;
    }

    return desc;
}
