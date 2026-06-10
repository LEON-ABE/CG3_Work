#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <cmath>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxcapi.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

struct Vector4 {
    float x, y, z, w;
};

struct Vector3 {
    float x, y, z;
};

struct Matrix4x4 {
    float matrix[4][4];
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

// 単位行列
Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 result{};
    result.matrix[0][0] = 1.0f;
    result.matrix[1][1] = 1.0f;
    result.matrix[2][2] = 1.0f;
    result.matrix[3][3] = 1.0f;
    return result;
}

// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.matrix[i][j] += m1.matrix[i][k] * m2.matrix[k][j];
            }
        }
    }
    return result;
}

// アフィン変換行列（SRT）
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
    // スケール
    Matrix4x4 scaleMatrix{};
    scaleMatrix.matrix[0][0] = scale.x;
    scaleMatrix.matrix[1][1] = scale.y;
    scaleMatrix.matrix[2][2] = scale.z;
    scaleMatrix.matrix[3][3] = 1.0f;

    // 回転X
    Matrix4x4 rotX{};
    rotX.matrix[0][0] = 1.0f;
    rotX.matrix[1][1] = cosf(rotate.x);  rotX.matrix[1][2] = sinf(rotate.x);
    rotX.matrix[2][1] = -sinf(rotate.x); rotX.matrix[2][2] = cosf(rotate.x);
    rotX.matrix[3][3] = 1.0f;

    // 回転Y
    Matrix4x4 rotY{};
    rotY.matrix[0][0] = cosf(rotate.y);  rotY.matrix[0][2] = -sinf(rotate.y);
    rotY.matrix[1][1] = 1.0f;
    rotY.matrix[2][0] = sinf(rotate.y);  rotY.matrix[2][2] = cosf(rotate.y);
    rotY.matrix[3][3] = 1.0f;

    // 回転Z
    Matrix4x4 rotZ{};
    rotZ.matrix[0][0] = cosf(rotate.z);  rotZ.matrix[0][1] = sinf(rotate.z);
    rotZ.matrix[1][0] = -sinf(rotate.z); rotZ.matrix[1][1] = cosf(rotate.z);
    rotZ.matrix[2][2] = 1.0f;
    rotZ.matrix[3][3] = 1.0f;

    // 平行移動
    Matrix4x4 translateMatrix = MakeIdentity4x4();
    translateMatrix.matrix[3][0] = translate.x;
    translateMatrix.matrix[3][1] = translate.y;
    translateMatrix.matrix[3][2] = translate.z;

    return Multiply(scaleMatrix, Multiply(rotX, Multiply(rotY, Multiply(rotZ, translateMatrix))));
}

// 逆行列
Matrix4x4 Inverse(const Matrix4x4& m) {
    Matrix4x4 result{};
    float det =
        m.matrix[0][0] * (m.matrix[1][1] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[1][2] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) + m.matrix[1][3] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]))
        - m.matrix[0][1] * (m.matrix[1][0] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[1][2] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[1][3] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]))
        + m.matrix[0][2] * (m.matrix[1][0] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) - m.matrix[1][1] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[1][3] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]))
        - m.matrix[0][3] * (m.matrix[1][0] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]) - m.matrix[1][1] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]) + m.matrix[1][2] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]));

    float invDet = 1.0f / det;
    result.matrix[0][0] = invDet * (m.matrix[1][1] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[1][2] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) + m.matrix[1][3] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]));
    result.matrix[0][1] = -invDet * (m.matrix[0][1] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[0][2] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) + m.matrix[0][3] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]));
    result.matrix[0][2] = invDet * (m.matrix[0][1] * (m.matrix[1][2] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][2]) - m.matrix[0][2] * (m.matrix[1][1] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][1]) + m.matrix[0][3] * (m.matrix[1][1] * m.matrix[3][2] - m.matrix[1][2] * m.matrix[3][1]));
    result.matrix[0][3] = -invDet * (m.matrix[0][1] * (m.matrix[1][2] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][2]) - m.matrix[0][2] * (m.matrix[1][1] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][1]) + m.matrix[0][3] * (m.matrix[1][1] * m.matrix[2][2] - m.matrix[1][2] * m.matrix[2][1]));

    result.matrix[1][0] = -invDet * (m.matrix[1][0] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[1][2] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[1][3] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]));
    result.matrix[1][1] = invDet * (m.matrix[0][0] * (m.matrix[2][2] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][2]) - m.matrix[0][2] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[0][3] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]));
    result.matrix[1][2] = -invDet * (m.matrix[0][0] * (m.matrix[1][2] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][2]) - m.matrix[0][2] * (m.matrix[1][0] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][0]) + m.matrix[0][3] * (m.matrix[1][0] * m.matrix[3][2] - m.matrix[1][2] * m.matrix[3][0]));
    result.matrix[1][3] = invDet * (m.matrix[0][0] * (m.matrix[1][2] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][2]) - m.matrix[0][2] * (m.matrix[1][0] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][0]) + m.matrix[0][3] * (m.matrix[1][0] * m.matrix[2][2] - m.matrix[1][2] * m.matrix[2][0]));

    result.matrix[2][0] = invDet * (m.matrix[1][0] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) - m.matrix[1][1] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[1][3] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]));
    result.matrix[2][1] = -invDet * (m.matrix[0][0] * (m.matrix[2][1] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][1]) - m.matrix[0][1] * (m.matrix[2][0] * m.matrix[3][3] - m.matrix[2][3] * m.matrix[3][0]) + m.matrix[0][3] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]));
    result.matrix[2][2] = invDet * (m.matrix[0][0] * (m.matrix[1][1] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][1]) - m.matrix[0][1] * (m.matrix[1][0] * m.matrix[3][3] - m.matrix[1][3] * m.matrix[3][0]) + m.matrix[0][3] * (m.matrix[1][0] * m.matrix[3][1] - m.matrix[1][1] * m.matrix[3][0]));
    result.matrix[2][3] = -invDet * (m.matrix[0][0] * (m.matrix[1][1] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][1]) - m.matrix[0][1] * (m.matrix[1][0] * m.matrix[2][3] - m.matrix[1][3] * m.matrix[2][0]) + m.matrix[0][3] * (m.matrix[1][0] * m.matrix[2][1] - m.matrix[1][1] * m.matrix[2][0]));

    result.matrix[3][0] = -invDet * (m.matrix[1][0] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]) - m.matrix[1][1] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]) + m.matrix[1][2] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]));
    result.matrix[3][1] = invDet * (m.matrix[0][0] * (m.matrix[2][1] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][1]) - m.matrix[0][1] * (m.matrix[2][0] * m.matrix[3][2] - m.matrix[2][2] * m.matrix[3][0]) + m.matrix[0][2] * (m.matrix[2][0] * m.matrix[3][1] - m.matrix[2][1] * m.matrix[3][0]));
    result.matrix[3][2] = -invDet * (m.matrix[0][0] * (m.matrix[1][1] * m.matrix[3][2] - m.matrix[1][2] * m.matrix[3][1]) - m.matrix[0][1] * (m.matrix[1][0] * m.matrix[3][2] - m.matrix[1][2] * m.matrix[3][0]) + m.matrix[0][2] * (m.matrix[1][0] * m.matrix[3][1] - m.matrix[1][1] * m.matrix[3][0]));
    result.matrix[3][3] = invDet * (m.matrix[0][0] * (m.matrix[1][1] * m.matrix[2][2] - m.matrix[1][2] * m.matrix[2][1]) - m.matrix[0][1] * (m.matrix[1][0] * m.matrix[2][2] - m.matrix[1][2] * m.matrix[2][0]) + m.matrix[0][2] * (m.matrix[1][0] * m.matrix[2][1] - m.matrix[1][1] * m.matrix[2][0]));

    return result;
}

// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
    Matrix4x4 result{};
    float f = 1.0f / tanf(fovY / 2.0f);
    result.matrix[0][0] = f / aspectRatio;
    result.matrix[1][1] = f;
    result.matrix[2][2] = farClip / (farClip - nearClip);
    result.matrix[2][3] = 1.0f;
    result.matrix[3][2] = -nearClip * farClip / (farClip - nearClip);
    return result;
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

    return EXCEPTION_EXECUTE_HANDLER;
}

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), sizeNeeded);
    result.resize(sizeNeeded - 1);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(sizeNeeded, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, result.data(), sizeNeeded, nullptr, nullptr);
    result.resize(sizeNeeded - 1);
    return result;
}

void Log(const std::string& message) {
    OutputDebugStringA(message.c_str());
}

void Log(const std::wstring& message) {
    Log(ConvertString(message));
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
#endif
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// CreateBufferResource関数
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));
    return resource;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}

// CompileShader関数
IDxcBlob* CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler)
{
    // hlslファイル
    Log(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile));
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    //Compile
    LPCWSTR arguments[] = {
        filePath.c_str(),        // コンパイル対象のhlslファイル名
        L"-E", L"main",          // エントリーポイント
        L"-T", profile,          // ShaderProfileの設定
        L"-Zi", L"-Qembed_debug",// デバッグ情報を埋め込む
        L"-Od",                  // 最最適化を外しておく
        L"-Zpr"                  // メモリレイアウトは行優先
    };

    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler,
        IID_PPV_ARGS(&shaderResult)
    );
    assert(SUCCEEDED(hr));

    // 3. 警告・エラーがでていないか確認する
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(shaderError->GetStringPointer());
        assert(false);
    }

    // 4. Compile結果を受け取って返す
    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
    shaderSource->Release();
    shaderResult->Release();

    return shaderBlob;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    SetUnhandledExceptionFilter(ExportDump);

    const int32_t windowWidth = 1280;
    const int32_t windowHeight = 720;

    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"CG2WindowClass";
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    RECT wrc = { 0, 0, windowWidth, windowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    HWND hwnd = CreateWindow(
        wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top,
        nullptr, nullptr, wc.hInstance, nullptr
    );

#ifdef _DEBUG
    ID3D12Debug1* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    ShowWindow(hwnd, SW_SHOW);

    // DXGIファクトリー生成
    IDXGIFactory7* dxgiFactory = nullptr;
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr));

    // アダプターの選択
    IDXGIAdapter4* useAdapter = nullptr;
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            Log(std::format(L"Use Adapter: {}\n", adapterDesc.Description));
            break;
        }
        useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    // デバイス生成
    ID3D12Device* device = nullptr;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };
    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) {
            Log(std::format("FeatureLevel: {}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device != nullptr);
    Log("Complete create D3D12Device!!!\n");

#ifdef _DEBUG
    ID3D12InfoQueue* infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        infoQueue->PushStorageFilter(&filter);
        infoQueue->Release();
    }
#endif

    // コマンドキュー作成
    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
    assert(SUCCEEDED(hr));

    // スワップチェーン作成
    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = windowWidth;
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* swapChain1 = nullptr;
    hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    assert(SUCCEEDED(hr));
    hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
    assert(SUCCEEDED(hr));
    swapChain1->Release();

    ID3D12Resource* swapChainResources[2] = { nullptr };
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
    assert(SUCCEEDED(hr));
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr));

    // RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
    ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    // SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
    ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
    rtvHandles[1].ptr = rtvHandles[0].ptr + rtvDescriptorSize;
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    assert(SUCCEEDED(hr));

    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
    assert(SUCCEEDED(hr));

    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);

    //DXCの初期化
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    IDxcIncludeHandler* includeHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));

#ifdef USE_IMGUI
    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
    // こういうもんである
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device,
        swapChainDesc.BufferCount,
        rtvDesc.Format,
        srvDescriptorHeap,
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
#endif

    //RootSignatureの生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。今回は結果1つだけなので長さ1の配列
    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    ID3D12RootSignature* rootSignature = nullptr;
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // InputLayoutの設定
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    //BlendStateの設定
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    //RasterizerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    //ShaderをCompileする
    IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
    assert(vertexShaderBlob != nullptr);

    IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
    assert(pixelShaderBlob != nullptr);

    //PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature;
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    ID3D12PipelineState* graphicsPipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

    //VertexResourceを生成する
    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(Vector4) * 3);

    // マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));

    // WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
    ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));

    //VertexBufferViewを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(Vector4) * 3;
    vertexBufferView.StrideInBytes = sizeof(Vector4);

    //Resourceにデータを書き込む
    Vector4* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    // 左下
    vertexData[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
    // 上
    vertexData[1] = { 0.0f,  0.5f, 0.0f, 1.0f };
    // 右下
    vertexData[2] = { 0.5f, -0.5f, 0.0f, 1.0f };

    // マテリアルにデータを書き込む
    Vector4* materialData = nullptr;
    // 書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は赤を書き込んでみる
    *materialData = { 1.0f, 0.0f, 0.0f, 1.0f };

    // データを書き込む
    Matrix4x4* wvpData = nullptr;
    // 書き込むためのアドレスを取得
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // 単位行列を書きこんでおく
    *wvpData = MakeIdentity4x4();

    //ViewportとScissorの設定
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(windowWidth);
    viewport.Height = static_cast<float>(windowHeight);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.right = windowWidth;
    scissorRect.top = 0;
    scissorRect.bottom = windowHeight;

    // Transform変数を作る
    Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    // カメラのTransform
    Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

    // メインループ
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
#ifdef USE_IMGUI
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
#endif

            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = swapChainResources[backBufferIndex];
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            commandList->ResourceBarrier(1, &barrier);

            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

            // ゲームの処理
#ifdef USE_IMGUI
            // 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
            ImGui::ShowDemoWindow();
#endif
            transform.rotate.y += 0.03f;
            Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
            Matrix4x4 viewMatrix = Inverse(cameraMatrix);
            Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(windowWidth) / float(windowHeight), 0.1f, 100.0f);
            Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
            *wvpData = worldViewProjectionMatrix;

#ifdef USE_IMGUI
            // ImGuiの内部コマンドを生成する
            ImGui::Render();
#endif

            //描画コマンド
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);

#ifdef USE_IMGUI
            // 描画用のDescriptorHeapの設定
            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);
#endif

            commandList->SetGraphicsRootSignature(rootSignature);
            commandList->SetPipelineState(graphicsPipelineState);

            commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // マテリアルCBufferの場所を設定
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

            // wvp用のCBufferの場所を設定
            commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

            // 描画
            commandList->DrawInstanced(3, 1, 0, 0);

#ifdef USE_IMGUI
            // 実際のcommandListのImGuiの描画コマンドを積む
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            commandList->ResourceBarrier(1, &barrier);

            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            ID3D12CommandList* commandLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, commandLists);

            swapChain->Present(1, 0);

            fenceValue++;
            commandQueue->Signal(fence, fenceValue);
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }

            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));
        }
    }

#ifdef USE_IMGUI
    // ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
    // こういうもんである。初期化と逆順に行う
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    // 解放処理
    wvpResource->Release();
    materialResource->Release();
    vertexResource->Release();
    graphicsPipelineState->Release();
    signatureBlob->Release();
    if (errorBlob) {
        errorBlob->Release();
    }
    rootSignature->Release();
    pixelShaderBlob->Release();
    vertexShaderBlob->Release();

    // DXCファクトリー周りおよびその他の初期化オブジェクトの解放
    includeHandler->Release();
    dxcCompiler->Release();
    dxcUtils->Release();

    CloseHandle(fenceEvent);
    fence->Release();
    srvDescriptorHeap->Release();
    rtvDescriptorHeap->Release();
    swapChainResources[0]->Release();
    swapChainResources[1]->Release();
    swapChain->Release();
    commandList->Release();
    commandAllocator->Release();
    commandQueue->Release();
    device->Release();
    useAdapter->Release();
    dxgiFactory->Release();
#ifdef _DEBUG
    debugController->Release();
#endif
    CloseWindow(hwnd);

    // リソースリークチェック
    IDXGIDebug1* debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }

    return 0;
}
