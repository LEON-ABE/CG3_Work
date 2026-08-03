#include "DebugCamera.h"
#include <Windows.h>

void DebugCamera::Initialize() {
    // 各種メンバ変数の初期化
    matRot_ = MakeIdentity4x4();
    matProjection_ = MakePerspectiveFovMatrix(0.45f, float(1280) / float(720), 0.1f, 100.0f);
}

void DebugCamera::Update() {
    const float kMoveSpeed = 0.2f;
    const float kRotateSpeed = 0.02f;
    const float kMouseRotateSpeed = 0.005f;
    const float kMousePanSpeed = 0.05f;
    const float kWheelZoomSpeed = 0.2f;

    // 現在のマウス座標を取得
    POINT mousePos;
    GetCursorPos(&mousePos);
    float mouseX = float(mousePos.x);
    float mouseY = float(mousePos.y);
    float mouseDeltaX = mouseX - prevMouseX_;
    float mouseDeltaY = mouseY - prevMouseY_;

    // X,Y,Z軸周りの今回フレーム分の回転角度
    float rotateX = 0.0f;
    float rotateY = 0.0f;
    float rotateZ = 0.0f;

    // X軸周り回転の入力があったら
    if (GetAsyncKeyState(VK_UP) & 0x8000) {
        rotateX -= kRotateSpeed;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
        rotateX += kRotateSpeed;
    }
    // Y軸周り回転の入力があったら
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
        rotateY -= kRotateSpeed;
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
        rotateY += kRotateSpeed;
    }
    // Z軸周り回転の入力があったら
    if (GetAsyncKeyState('Q') & 0x8000) {
        rotateZ -= kRotateSpeed;
    }
    if (GetAsyncKeyState('E') & 0x8000) {
        rotateZ += kRotateSpeed;
    }

    // マウス左ボタンドラッグによる回転の入力があったら
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        if (isRotating_) {
            rotateY += mouseDeltaX * kMouseRotateSpeed;
            rotateX += mouseDeltaY * kMouseRotateSpeed;
        }
        isRotating_ = true;
    } else {
        isRotating_ = false;
    }

    // 追加回転分の回転行列を生成
    Matrix4x4 matRotDelta = MakeIdentity4x4();
    matRotDelta = Multiply(matRotDelta, MakeRotateXMatrix(rotateX));
    matRotDelta = Multiply(matRotDelta, MakeRotateYMatrix(rotateY));
    matRotDelta = Multiply(matRotDelta, MakeRotateZMatrix(rotateZ));

    // 累積の回転行列を合成
    matRot_ = Multiply(matRotDelta, matRot_);

    // 前後移動の入力があったら
    if (GetAsyncKeyState('W') & 0x8000) {
        const float speed = kMoveSpeed;
        // カメラ移動ベクトル
        Vector3 move = { 0.0f, 0.0f, speed };
        // 移動ベクトルを角度分だけ回転させる
        move = TransformNormal(move, matRot_);
        // 移動ベクトル分だけ座標を加算する
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        const float speed = -kMoveSpeed;
        Vector3 move = { 0.0f, 0.0f, speed };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }

    // 左右移動の入力があったら
    if (GetAsyncKeyState('D') & 0x8000) {
        const float speed = kMoveSpeed;
        Vector3 move = { speed, 0.0f, 0.0f };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        const float speed = -kMoveSpeed;
        Vector3 move = { speed, 0.0f, 0.0f };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }

    // 上下移動の入力があったら
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        const float speed = kMoveSpeed;
        Vector3 move = { 0.0f, speed, 0.0f };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        const float speed = -kMoveSpeed;
        Vector3 move = { 0.0f, speed, 0.0f };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
    }

    // マウス中ボタンドラッグによる平行移動(パン)の入力があったら
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
        if (isPanning_) {
            Vector3 move = { -mouseDeltaX * kMousePanSpeed, mouseDeltaY * kMousePanSpeed, 0.0f };
            move = TransformNormal(move, matRot_);
            translation_.x += move.x;
            translation_.y += move.y;
            translation_.z += move.z;
        }
        isPanning_ = true;
    } else {
        isPanning_ = false;
    }

    // マウスホイールによるズーム(前後移動)の入力があったら
    if (wheelDelta_ != 0) {
        const float speed = (float(wheelDelta_) / WHEEL_DELTA) * kWheelZoomSpeed;
        Vector3 move = { 0.0f, 0.0f, speed };
        move = TransformNormal(move, matRot_);
        translation_.x += move.x;
        translation_.y += move.y;
        translation_.z += move.z;
        wheelDelta_ = 0;
    }

    // 次フレームの差分計算のためにマウス座標を保存する
    prevMouseX_ = mouseX;
    prevMouseY_ = mouseY;

    // ビュー行列の更新
    // 座標から平行移動行列を計算する
    Matrix4x4 matTranslate = MakeTranslateMatrix(translation_);
    // 累積回転行列と平行移動行列からワールド行列を計算する
    Matrix4x4 worldMatrix = Multiply(matRot_, matTranslate);
    // ワールド行列の逆行列をビュー行列に代入する
    matView_ = Inverse(worldMatrix);
}
