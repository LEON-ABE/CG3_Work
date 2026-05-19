#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>

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
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    
    Log(L"Hello, DirectX!\n");

    const int32_t windowWidth = 1280;
    const int32_t windowHeight = 720;

    
    std::wstring windowSizeLog = std::format(L"Window size: {}x{}\n", windowWidth, windowHeight);
    Log(windowSizeLog);

    
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

    ShowWindow(hwnd, SW_SHOW);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}