#pragma once
#include "MyMath.h"

class DebugCamera {
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    const Matrix4x4& GetViewMatrix() const { return matView_; }
    const Matrix4x4& GetProjectionMatrix() const { return matProjection_; }

    /// <summary>
    /// マウスホイールの回転量を加算する(WindowProcから呼び出す)
    /// </summary>
    static void AddWheelDelta(short delta) { wheelDelta_ += delta; }

private:
    // ローカル座標
    Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

    // 累積回転行列
    Matrix4x4 matRot_;

    // ビュー行列
    Matrix4x4 matView_;
    // 射影行列
    Matrix4x4 matProjection_;

    // マウス操作用
    float prevMouseX_ = 0.0f;
    float prevMouseY_ = 0.0f;
    bool isRotating_ = false;
    bool isPanning_ = false;

    // マウスホイールの累積回転量(WindowProcから加算される)
    static inline int wheelDelta_ = 0;
};
