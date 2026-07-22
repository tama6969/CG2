#include "DebugCamera.h"

void DebugCamera::Initialize() {
	// 累積回転行列を初期化
	matRot_ = MakeIdentity4x4();

	// 平行移動行列を計算
	Matrix4x4 matTrans = MakeTranslateMatrix(translation_);

	// ワールド行列を計算
	Matrix4x4 matWorld = MultiplyMatrix4x4(matRot_, matTrans);

	// ビュー行列を計算
	matView_ = Inverse(matWorld);

	// 射影行列を計算
	matProjection_ = MakePerspectiveFovMatrix(
		0.45f,
		1280.0f / 720.0f,
		0.1f,
		100.0f
	);
}

void DebugCamera::Update(const BYTE* key) {
	const float kMoveSpeed = 0.2f;
	const float kRotateSpeed = 0.02f;

	Vector3 rotateDelta = { 0.0f, 0.0f, 0.0f };

	// 前進
	if (key[DIK_W]) {
		Vector3 move = { 0.0f, 0.0f, kMoveSpeed };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// 後退
	if (key[DIK_S]) {
		Vector3 move = { 0.0f, 0.0f, -kMoveSpeed };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// 左移動
	if (key[DIK_A]) {
		Vector3 move = { -kMoveSpeed, 0.0f, 0.0f };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// 右移動
	if (key[DIK_D]) {
		Vector3 move = { kMoveSpeed, 0.0f, 0.0f };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// 上移動
	if (key[DIK_E]) {
		Vector3 move = { 0.0f, kMoveSpeed, 0.0f };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// 下移動
	if (key[DIK_Q]) {
		Vector3 move = { 0.0f, -kMoveSpeed, 0.0f };
		move = TransformNormal(move, matRot_);

		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// X軸正方向回転
	if (key[DIK_UP]) {
		rotateDelta.x += kRotateSpeed;
	}

	// X軸負方向回転
	if (key[DIK_DOWN]) {
		rotateDelta.x -= kRotateSpeed;
	}

	// Y軸正方向回転
	if (key[DIK_RIGHT]) {
		rotateDelta.y += kRotateSpeed;
	}

	// Y軸負方向回転
	if (key[DIK_LEFT]) {
		rotateDelta.y -= kRotateSpeed;
	}

	// Z軸正方向回転
	if (key[DIK_Z]) {
		rotateDelta.z += kRotateSpeed;
	}

	// Z軸負方向回転
	if (key[DIK_X]) {
		rotateDelta.z -= kRotateSpeed;
	}

	// 追加回転分の回転行列を生成
	Matrix4x4 matRotDelta = MakeIdentity4x4();

	matRotDelta = MultiplyMatrix4x4(
		matRotDelta,
		MakeRotateXMatrix(rotateDelta.x)
	);

	matRotDelta = MultiplyMatrix4x4(
		matRotDelta,
		MakeRotateYMatrix(rotateDelta.y)
	);

	matRotDelta = MultiplyMatrix4x4(
		matRotDelta,
		MakeRotateZMatrix(rotateDelta.z)
	);

	// 累積回転行列を合成
	matRot_ = MultiplyMatrix4x4(matRotDelta, matRot_);

	// 平行移動行列を計算
	Matrix4x4 matTrans = MakeTranslateMatrix(translation_);

	// 累積回転行列と平行移動行列からワールド行列を計算
	Matrix4x4 matWorld = MultiplyMatrix4x4(matRot_, matTrans);

	// ワールド行列の逆行列をビュー行列に代入
	matView_ = Inverse(matWorld);
}