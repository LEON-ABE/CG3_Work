#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <cmath>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxgidebug.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxcapi.h>
#include "./externals/DirectXTex/DirectXTex.h"

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

struct TransformationMatrix {
    Matrix4x4 WVP;
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

// 既存のテクスチャ読み込みヘルパー関数
DirectX::ScratchImage LoadTexture(const std::wstring& filePath) {
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    assert(SUCCEEDED(hr));

    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 4, mipImages);
    if (SUCCEEDED(hr)) {
        return mipImages;
    }
    return image;
}

// 既存のリソース作成ヘルパー関数
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
    HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT64>(metadata.width);
    resourceDesc.Height = static_cast<UINT>(metadata.height);
    resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    (void)texture; (void)mipImages; (void)device; (void)commandList;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

    // 1. ウィンドウの生成
    WNDCLASS wc{};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"CG2WindowClass";
    wc.hInstance = hInstance;
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, hInstance, nullptr);
    assert(hWnd != nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. DirectX12の初期化
    HRESULT hr = S_OK;
    IDXGIFactory7* dxgiFactory = nullptr;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr) && dxgiFactory != nullptr);

    IDXGIAdapter4* useAdapter = nullptr;
    hr = dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter));
    assert(SUCCEEDED(hr) && useAdapter != nullptr);

    ID3D12Device* device = nullptr;
    hr = D3D12CreateDevice(useAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));
    assert(SUCCEEDED(hr) && device != nullptr);

    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    assert(SUCCEEDED(hr) && commandQueue != nullptr);

    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    assert(SUCCEEDED(hr) && commandAllocator != nullptr);

    ID3D12GraphicsCommandList* commandList = nullptr;
   
    assert(commandAllocator != nullptr);
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
    assert(SUCCEEDED(hr) && commandList != nullptr);

    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = 1280;
    swapChainDesc.Height = 720;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* swapChain1 = nullptr;

    // commandQueue
    assert(commandQueue != nullptr);
    hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    assert(SUCCEEDED(hr) && swapChain1 != nullptr);
    hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
    assert(SUCCEEDED(hr) && swapChain != nullptr);
    swapChain1->Release();

    ID3D12Resource* swapChainResources[2] = {};
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
    assert(SUCCEEDED(hr) && swapChainResources[0] != nullptr);
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr) && swapChainResources[1] != nullptr);

    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    assert(SUCCEEDED(hr) && rtvDescriptorHeap != nullptr);

    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 128;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap));
    assert(SUCCEEDED(hr) && srvDescriptorHeap != nullptr);

    ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap));
    assert(SUCCEEDED(hr) && dsvDescriptorHeap != nullptr);

    ID3D12Resource* depthStencilResource = nullptr;
    D3D12_RESOURCE_DESC depthStencilDesc{};
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Width = 1280;
    depthStencilDesc.Height = 720;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES depthHeapProp{};
    depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    hr = device->CreateCommittedResource(&depthHeapProp, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&depthStencilResource));
    assert(SUCCEEDED(hr) && depthStencilResource != nullptr);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    ID3D12Fence* fence = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr) && fence != nullptr);

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent != nullptr); 

    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    IDxcIncludeHandler* includeHandler = nullptr;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(dxcUtils != nullptr && dxcCompiler != nullptr);
    dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(includeHandler != nullptr);

    ID3DBlob* vertexShaderBlob = nullptr;
    ID3DBlob* pixelShaderBlob = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* graphicsPipelineState = nullptr;
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 1024);
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
    ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
    ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

    UINT descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    DirectX::ScratchImage mipImages = LoadTexture(L"resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
    UploadTextureData(textureResource, mipImages, device, commandList);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandleCPU.ptr += descriptorSizeSRV * 1;
    device->CreateShaderResourceView(textureResource, &srvDesc, srvHandleCPU);


    DirectX::ScratchImage mipImages2 = LoadTexture(L"resources/monsterBall.png");
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
    UploadTextureData(textureResource2, mipImages2, device, commandList);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = static_cast<UINT>(metadata2.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU2 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandleCPU2.ptr += descriptorSizeSRV * 2;
    device->CreateShaderResourceView(textureResource2, &srvDesc2, srvHandleCPU2);


    // GPUハンドルの取得
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    textureSrvHandleGPU.ptr += descriptorSizeSRV * 1;

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    textureSrvHandleGPU2.ptr += descriptorSizeSRV * 2;


#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM, srvDescriptorHeap, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
#endif

    bool useMonsterBall = false;

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

            ImGui::Begin("Window");
            ImGui::Checkbox("useMonsterBall", &useMonsterBall);
            ImGui::End();

            ImGui::Render();
#endif

            assert(commandAllocator != nullptr);
            commandAllocator->Reset();

            assert(commandList != nullptr);
            commandList->Reset(commandAllocator, nullptr);

            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

            if (rootSignature != nullptr) {
                commandList->SetGraphicsRootSignature(rootSignature);
            }

            if (graphicsPipelineState != nullptr) {
                commandList->SetPipelineState(graphicsPipelineState);
            }

            if (useMonsterBall) {
                commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
            }
            else {
                commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
            }

#ifdef USE_IMGUI
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

            commandList->Close();
            ID3D12CommandList* cmdLists[] = { commandList };

            assert(commandQueue != nullptr);
            commandQueue->ExecuteCommandLists(1, cmdLists);
            swapChain->Present(1, 0);

            UINT64 fenceValue = 1;
            commandQueue->Signal(fence, fenceValue);
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                // fenceEventが有効であることをコンパイラに確定させて呼び出し
                assert(fenceEvent != nullptr);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    // 各種リソース解放（非ヌル確認を行って安全に解放）
    if (depthStencilResource) depthStencilResource->Release();
    if (dsvDescriptorHeap) dsvDescriptorHeap->Release();
    if (transformationMatrixResourceSprite) transformationMatrixResourceSprite->Release();
    if (textureResource2) textureResource2->Release();
    if (textureResource) textureResource->Release();
    if (wvpResource) wvpResource->Release();
    if (materialResource) materialResource->Release();
    if (vertexResource) vertexResource->Release();

    if (graphicsPipelineState) graphicsPipelineState->Release();
    if (signatureBlob) signatureBlob->Release();
    if (errorBlob) errorBlob->Release();
    if (rootSignature) rootSignature->Release();
    if (pixelShaderBlob) pixelShaderBlob->Release();
    if (vertexShaderBlob) vertexShaderBlob->Release();

    if (includeHandler) includeHandler->Release();
    if (dxcCompiler) dxcCompiler->Release();
    if (dxcUtils) dxcUtils->Release();

    if (fenceEvent != nullptr) {
        CloseHandle(fenceEvent);
    }
    if (fence) fence->Release();
    if (srvDescriptorHeap) srvDescriptorHeap->Release();
    if (rtvDescriptorHeap) rtvDescriptorHeap->Release();
    if (swapChainResources[0]) swapChainResources[0]->Release();
    if (swapChainResources[1]) swapChainResources[1]->Release();
    if (swapChain) swapChain->Release();
    if (commandList) commandList->Release();
    if (commandAllocator) commandAllocator->Release();
    if (commandQueue) commandQueue->Release();

    if (useAdapter) useAdapter->Release();
    if (device) device->Release();
    if (dxgiFactory) dxgiFactory->Release();

    return 0;
}