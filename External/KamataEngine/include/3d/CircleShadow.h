#pragma once

#include <math\Vector2.h>
#include <math\Vector3.h>
#include <cmath>

namespace KamataEngine {

/// <summary>
/// 丸影
/// </summary>
class CircleShadow {
public: // サブクラス
	// 定数バッファ用データ構造体
	struct ConstBufferData {
		Vector3 direction; // 投影方向の逆ベクトル
		float pad1;
		Vector3 position;  // キャスター座標
		float distanceCasterLight; // キャスターとライトの距離
		Vector3 atten;     // 距離減衰係数
		float pad2;
		Vector2 cosAngle;  // 減衰角度のコサイン (x:開始, y:終了)
		unsigned int active; // 有効フラグ
		float pad3;
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
	inline void SetPosition(const Vector3& position) { position_ = position; }

	/// <summary>
	/// 座標を取得
	/// </summary>
	/// <returns>座標</returns>
	inline const Vector3& GetPosition() const { return position_; }

	/// <summary>
	/// キャスターとライトの距離をセット
	/// </summary>
	/// <param name="lightpos">キャスターとライトの距離</param>
	inline void SetDistanceCasterLight(float distanceCasterLight) { distanceCasterLight_ = distanceCasterLight; }

	/// <summary>
	/// キャスターとライトの距離を取得
	/// </summary>
	/// <returns>キャスターとライトの距離</returns>
	inline float GetDistanceCasterLight() const { return distanceCasterLight_; }

	/// <summary>
	/// 距離減衰係数をセット
	/// </summary>
	/// <param name="lightatten">ライト距離減衰係数</param>
	inline void SetAtten(const Vector3& atten) { atten_ = atten; }

	/// <summary>
	/// 距離減衰係数を取得
	/// </summary>
	/// <returns>ライト距離減衰係数</returns>
	inline const Vector3& GetAtten() const { return atten_; }

	/// <summary>
	/// 減衰角度をセット
	/// </summary>
	/// <param name="angle">x:減衰開始角度 y:減衰終了角度[radian]</param>
	inline void SetCosAngle(const Vector2& angle) {
		cosAngle_.x = std::cos(angle.x);
		cosAngle_.y = std::cos(angle.y);
	}

	/// <summary>
	/// 減衰角度を取得
	/// </summary>
	/// <returns>減衰角度</returns>
	inline const Vector2& GetCosAngle() const { return cosAngle_; }

	/// <summary>
	/// 有効フラグをセット
	/// </summary>
	/// <param name="active">有効フラグ</param>
	inline void SetActive(bool active) { active_ = active; }

	/// <summary>
	/// 有効チェック
	/// </summary>
	/// <returns>有効フラグ</returns>
	inline bool IsActive() const { return active_; }

private: // メンバ変数
	// 方向（単位ベクトル）
	Vector3 direction_ = {1, 0, 0};
	// キャスターとライトの距離
	float distanceCasterLight_ = 100.0f;
	// 座標（ワールド座標系）
	Vector3 position_ = {0, 0, 0};
	// 距離減衰係数
	Vector3 atten_ = {0.5f, 0.6f, 0.0f};
	// 減衰角度
	Vector2 cosAngle_ = {0.2f, 0.5f};
	// 有効フラグ
	bool active_ = false;
};

} // namespace KamataEngine