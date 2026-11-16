#pragma once

#include "MatrixMath.h"
#include <dinput.h>

class DebugCamera {
public:
	/// 初期化
	void Initialize();

	/// 更新
	void Update(const BYTE* key);

	/// ビュー行列を取得
	const Matrix4x4& GetViewMatrix() const {
		return matView_;
	}

	/// 射影行列を取得
	const Matrix4x4& GetProjectionMatrix() const {
		return matProjection_;
	}

private:
	
	Matrix4x4 matRot_;

	// ローカル座標
	Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

	// ビュー行列
	Matrix4x4 matView_;

	// 射影行列
	Matrix4x4 matProjection_;
};