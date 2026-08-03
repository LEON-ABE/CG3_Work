#pragma once
#include <cmath>

struct Vector4 {
    float x, y, z, w;
};

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float x, y;
};

struct Matrix4x4 {
    float m[4][4];
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

inline Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) result.m[i][i] = 1.0f;
    return result;
}

inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

inline Matrix4x4 MakeRotateZMatrix(float theta) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = std::cos(theta);
    result.m[0][1] = std::sin(theta);
    result.m[1][0] = -std::sin(theta);
    result.m[1][1] = std::cos(theta);
    return result;
}

inline Matrix4x4 MakeRotateXMatrix(float theta) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[1][1] = std::cos(theta);
    result.m[1][2] = std::sin(theta);
    result.m[2][1] = -std::sin(theta);
    result.m[2][2] = std::cos(theta);
    return result;
}

inline Matrix4x4 MakeRotateYMatrix(float theta) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = std::cos(theta);
    result.m[0][2] = -std::sin(theta);
    result.m[2][0] = std::sin(theta);
    result.m[2][2] = std::cos(theta);
    return result;
}

inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;
    return result;
}

inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = m1.m[i][0] * m2.m[0][j] +
                m1.m[i][1] * m2.m[1][j] +
                m1.m[i][2] * m2.m[2][j] +
                m1.m[i][3] * m2.m[3][j];
        }
    }
    return result;
}

inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
    Matrix4x4 rotateMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
    return Multiply(scaleMatrix, Multiply(rotateMatrix, translateMatrix));
}

inline Matrix4x4 Inverse(const Matrix4x4& matrix) {
    Matrix4x4 result = MakeIdentity4x4();
    float a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            a[i][j] = matrix.m[i][j];
            a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (int i = 0; i < 4; ++i) {
        float pivot = a[i][i];
        for (int j = 0; j < 8; ++j) {
            a[i][j] /= pivot;
        }
        for (int k = 0; k < 4; ++k) {
            if (k != i) {
                float factor = a[k][i];
                for (int j = 0; j < 8; ++j) {
                    a[k][j] -= factor * a[i][j];
                }
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a[i][j + 4];
        }
    }
    return result;
}

inline Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
    Matrix4x4 result{};
    float cot = 1.0f / std::tan(fovY / 2.0f);
    result.m[0][0] = (1.0f / aspectRatio) * cot;
    result.m[1][1] = cot;
    result.m[2][2] = farClip / (farClip - nearClip);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
    return result;
}

// ベクトルを行列の回転成分(3x3)のみで変換する(平行移動成分を無視する)
inline Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) {
    Vector3 result;
    result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0];
    result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1];
    result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2];
    return result;
}
