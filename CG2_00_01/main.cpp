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
    Vector3 normal;
};

struct Matrix4x4 {
    float m[4][4];
};

struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform; 
};

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) result.m[i][i] = 1.0f;
    return result;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

Matrix4x4 MakeRotateZMatrix(float theta) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = std::cos(theta);
    result.m[0][1] = std::sin(theta);
    result.m[1][0] = -std::sin(theta);
    result.m[1][1] = std::cos(theta);
    return result;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;
    return result;
}

Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
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

// テクスチャ読み込みヘルパー関数
DirectX::ScratchImage LoadTexture(const std::wstring& filePath) {
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);

    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 4, mipImages);
    if (SUCCEEDED(hr)) {
        return mipImages;
    }
    return image;
}

// リソース作成ヘルパー関数
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

// テクスチャデータのアップロード
void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    UINT numSubresources = static_cast<UINT>(metadata.mipLevels * metadata.arraySize);

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
    std::vector<UINT> numRows(numSubresources);
    std::vector<UINT64> rowSizesInBytes(numSubresources);
    UINT64 totalBytes = 0;

    D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();
    device->GetCopyableFootprints(&textureDesc, 0, numSubresources, 0, layouts.data(), numRows.data(), rowSizesInBytes.data(), &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc{};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = totalBytes;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* uploadBuffer = nullptr;
    HRESULT hr = device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
    assert(SUCCEEDED(hr));

    void* mappedData = nullptr;
    hr = uploadBuffer->Map(0, nullptr, &mappedData);
    assert(SUCCEEDED(hr));

    for (UINT i = 0; i < numSubresources; ++i) {
        const DirectX::Image* img = mipImages.GetImage(i % metadata.mipLevels, i / metadata.mipLevels, 0);
        assert(img != nullptr);

        BYTE* destSlice = reinterpret_cast<BYTE*>(mappedData) + layouts[i].Offset;
        const BYTE* srcSlice = img->pixels;

        for (UINT row = 0; row < numRows[i]; ++row) {
            memcpy(destSlice + (row * layouts[i].Footprint.RowPitch), srcSlice + (row * img->rowPitch), rowSizesInBytes[i]);
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    for (UINT i = 0; i < numSubresources; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = texture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = i;

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = uploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layouts[i];

        commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    assert(SUCCEEDED(hr));

    ID3D12CommandList* cmdLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, cmdLists);

    ID3D12Fence* syncFence = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&syncFence));
    assert(SUCCEEDED(hr));

    HANDLE syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(syncEvent != nullptr);

    commandQueue->Signal(syncFence, 1);
    if (syncFence->GetCompletedValue() < 1) {
        syncFence->SetEventOnCompletion(1, syncEvent);
        WaitForSingleObject(syncEvent, INFINITE);
    }

    CloseHandle(syncEvent);
    syncFence->Release();
    commandQueue->Release();
    uploadBuffer->Release();
}

// DXCコンパイルヘルパー関数
IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler) {
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderBuffer{};
    shaderBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderBuffer.Size = shaderSource->GetBufferSize();
    shaderBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Od",
        L"-Zpr"
    };

    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(&shaderBuffer, arguments, _countof(arguments), includeHandler, IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        OutputDebugStringA(shaderError->GetStringPointer());
        assert(false && "Shader Compilation Failed");
    }

    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    shaderSource->Release();
    shaderResult->Release();
    if (shaderError) shaderError->Release();

    return shaderBlob;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

#ifdef _DEBUG
    ID3D12Debug* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->Release();
    }
#endif

    WNDCLASS wc{};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"CG2WindowClass";
    wc.hInstance = hInstance;
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, hInstance, nullptr);
    assert(hWnd != nullptr);
    ShowWindow(hWnd, nCmdShow);

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

    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleStart = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; ++i) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHandleStart;
        rtvHandle.ptr += rtvDescriptorSize * i;
        device->CreateRenderTargetView(swapChainResources[i], nullptr, rtvHandle);
    }

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
    dxcUtils->CreateDefaultIncludeHandler(&includeHandler);

    IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
    IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);

    D3D12_DESCRIPTOR_RANGE descriptorRanges[1]{};
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRanges;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
    rootSignatureDesc.pStaticSamplers = staticSamplers;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        assert(false);
    }

    ID3D12RootSignature* rootSignature = nullptr;
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature;
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    ID3D12PipelineState* graphicsPipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

  
    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 4);
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
    ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
    ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
    ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));

    ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
    indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;
    indexResourceSprite->Unmap(0, nullptr);

    VertexData* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    vertexData[0].position = { -0.5f, -0.5f, 0.0f, 1.0f }; vertexData[0].texcoord = { 0.0f, 1.0f }; vertexData[0].normal = { 0.0f, 0.0f, -1.0f };
    vertexData[1].position = { -0.5f,  0.5f, 0.0f, 1.0f }; vertexData[1].texcoord = { 0.0f, 0.0f }; vertexData[1].normal = { 0.0f, 0.0f, -1.0f };
    vertexData[2].position = { 0.5f, -0.5f, 0.0f, 1.0f }; vertexData[2].texcoord = { 1.0f, 1.0f }; vertexData[2].normal = { 0.0f, 0.0f, -1.0f };
    vertexData[3].position = { 0.5f,  0.5f, 0.0f, 1.0f }; vertexData[3].texcoord = { 1.0f, 0.0f }; vertexData[3].normal = { 0.0f, 0.0f, -1.0f };
    vertexResource->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = Vector3{ 0.0f, -1.0f, 0.0f };
    directionalLightData->intensity = 1.0f;
    directionalLightResource->Unmap(0, nullptr);

  
    Material* materialData = nullptr;
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    materialData->enableLighting = 1;
    materialData->uvTransform = MakeIdentity4x4(); 
    materialResource->Unmap(0, nullptr);

    TransformationMatrix* wvpData = nullptr;
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            wvpData->WVP.m[i][j] = (i == j) ? 1.0f : 0.0f;
            wvpData->World.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    wvpResource->Unmap(0, nullptr);

    UINT descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    DirectX::ScratchImage mipImages = LoadTexture(L"resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = CreateTextureResource(device, metadata);

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);
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

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);
    UploadTextureData(textureResource2, mipImages2, device, commandList);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = static_cast<UINT>(metadata2.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU2 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandleCPU2.ptr += descriptorSizeSRV * 2;
    device->CreateShaderResourceView(textureResource2, &srvDesc2, srvHandleCPU2);

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
    UINT64 fenceValue = 0;

    Transform uvTransformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

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

            ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
            ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
            ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);

            ImGui::End();

            ImGui::Render();
#endif

            commandAllocator->Reset();
            commandList->Reset(commandAllocator, nullptr);

            Matrix4x4 matScale = MakeScaleMatrix(uvTransformSprite.scale);
            Matrix4x4 matRotate = MakeRotateZMatrix(uvTransformSprite.rotate.z);
            Matrix4x4 matTranslate = MakeTranslateMatrix(uvTransformSprite.translate);

            Matrix4x4 uvTransformMatrix = Multiply(matScale, matRotate);
            uvTransformMatrix = Multiply(uvTransformMatrix, matTranslate);

            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

            commandList->SetGraphicsRootSignature(rootSignature);
            commandList->SetPipelineState(graphicsPipelineState);

            D3D12_VIEWPORT viewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
            D3D12_RECT scissorRect{ 0, 0, 1280, 720 };
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);

            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
            D3D12_RESOURCE_BARRIER barrierToRenderTarget{};
            barrierToRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierToRenderTarget.Transition.pResource = swapChainResources[backBufferIndex];
            barrierToRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrierToRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrierToRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrierToRenderTarget);

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += backBufferIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

         
            Material* matMap = nullptr;
            if (SUCCEEDED(materialResource->Map(0, nullptr, reinterpret_cast<void**>(&matMap)))) {
                matMap->color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
                matMap->enableLighting = 1;
                matMap->uvTransform = uvTransformMatrix; 
                materialResource->Unmap(0, nullptr);
            }

            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource->GetGPUVirtualAddress());

            if (useMonsterBall) {
                commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU2);
            }
            else {
                commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU);
            }

            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

            commandList->IASetIndexBuffer(&indexBufferViewSprite);
            commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

#ifdef USE_IMGUI
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

            D3D12_RESOURCE_BARRIER barrierToPresent{};
            barrierToPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierToPresent.Transition.pResource = swapChainResources[backBufferIndex];
            barrierToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrierToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrierToPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrierToPresent);

            commandList->Close();
            ID3D12CommandList* cmdLists[] = { commandList };

            commandQueue->ExecuteCommandLists(1, cmdLists);
            swapChain->Present(1, 0);

            fenceValue++;
            commandQueue->Signal(fence, fenceValue);
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

    fenceValue++;
    commandQueue->Signal(fence, fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    if (indexResourceSprite) indexResourceSprite->Release();
    if (directionalLightResource) directionalLightResource->Release();
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