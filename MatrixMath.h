#pragma once
#include <cmath>

// 構造体の定義
struct Vector4 {
	float x, y, z, w;
};
struct Vector3 {
	float x, y, z;
};
struct Vector2 {
	float x, y;
};
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
};
struct Material {
	Vector4 color;
};
struct Matrix4x4 {
	float m[4][4];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
};

// 関数の宣言
Matrix4x4 MultiplyMatrix4x4(const Matrix4x4& m1, const Matrix4x4& m2);
Matrix4x4 MakeIdentity4x4();
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 Inverse(const Matrix4x4& m);
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearClip, float farClip);
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);