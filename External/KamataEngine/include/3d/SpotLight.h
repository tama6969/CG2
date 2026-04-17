#pragma once

#include <math\Vector2.h>
#include <math\Vector3.h>
#include <cmath>

namespace KamataEngine {

/// <summary>
/// スポットライト
/// </summary>
class SpotLight {
public: // サブクラス
	// 定数バッファ用データ構造体
	struct ConstBufferData {
		Vector3 color;     // ライトの色(RGB)
		float intensity;   // 輝度
		Vector3 position;  // ライト座標
		float radius;      // 最大距離
		Vector3 direction; // ライト方向
		float decay;       // 減衰率
		Vector2 cosAngle;  // 角度減衰のコサイン (x:開始, y:終了)
		unsigned int active; // 有効フラグ
		float pad[1];
	};

public: // メンバ関数
	/// <summary>
	/// 方向をセット
	/// </summary>
	/// <param name="direction">方向</param>
	void SetDirection(const Vector3& direction);

	/// <summary>
	/// 方向を取得
	/// </summary>
	/// <returns>方向</returns>
	inline const Vector3& GetDirection() const { return direction_; }

	/// <summary>
	/// 座標をセット
	/// </summary>
	/// <param name="position">座標</param>
	inline void SetPosition(const Vector3& position) { this->position_ = position; }

	/// <summary>
	/// 座標を取得
	/// </summary>
	/// <returns>座標</returns>
	inline const Vector3& GetPosition() const { return position_; }

	/// <summary>
	/// 色をセット
	/// </summary>
	/// <param name="color">色</param>
	inline void SetColor(const Vector3& color) { this->color_ = color; }

	/// <summary>
	/// 色を取得
	/// </summary>
	/// <returns>色</returns>
	inline const Vector3& GetColor() const { return color_; }

	/// <summary>
	/// 輝度をセット
	/// </summary>
	/// <param name="intensity">輝度</param>
	inline void SetIntensity(float intensity) { intensity_ = intensity; }

	/// <summary>
	/// 輝度を取得
	/// </summary>
	/// <returns>輝度</returns>
	inline float GetIntensity() const { return intensity_; }

	/// <summary>
	/// 半径をセット
	/// </summary>
	/// <param name="radius">半径</param>
	inline void SetRadius(float radius) { radius_ = radius; }

	/// <summary>
	/// 半径を取得
	/// </summary>
	/// <returns>半径</returns>
	inline float GetRadius() const { return radius_; }

	/// <summary>
	/// 減衰率をセット
	/// </summary>
	/// <param name="decay">減衰率</param>
	inline void SetDecay(float decay) { decay_ = decay; }

	/// <summary>
	/// 減衰率を取得
	/// </summary>
	/// <returns>減衰率</returns>
	inline float GetDecay() const { return decay_; }

	/// <summary>
	/// 角度をセット
	/// </summary>
	/// <param name="cosAngle">x:減衰開始角度 y:減衰終了角度[radian]</param>
	inline void SetCosAngle(const Vector2& angle) {
		this->cosAngle_.x = std::cos(angle.x);
		this->cosAngle_.y = std::cos(angle.y);
	}

	/// <summary>
	/// 角度を取得
	/// </summary>
	/// <returns>角度</returns>
	inline const Vector2& GetCosAngle() const { return cosAngle_; }

	/// <summary>
	/// 有効フラグをセット
	/// </summary>
	/// <param name="active">有効フラグ</param>
	inline void SetActive(bool active) { this->active_ = active; }

	/// <summary>
	/// 有効チェック
	/// </summary>
	/// <returns>有効フラグ</returns>
	inline bool IsActive() const { return active_; }

private: // メンバ変数
	// 色
	Vector3 color_ = {1, 1, 1};
	// 座標（ワールド座標系）
	Vector3 position_ = {0, 0, 0};
	// 方向（単位ベクトル）
	Vector3 direction_ = {1, 0, 0};
	// 輝度
	float intensity_ = 1.0f;
	// ライト力届く最大距離
	float radius_ = 20.0f;
	// 減衰率
	float decay_ = 1.0f;
	// 角度
	Vector2 cosAngle_ = {0.2f, 0.5f};
	// 有効フラグ
	bool active_ = false;
};

} // namespace KamataEngine