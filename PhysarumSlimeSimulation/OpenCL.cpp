#pragma warning(disable: 4312)

#include "OpenCL.h"

#include <stdlib.h>
#include <stdio.h>

//#pragma comment(lib, "cuda_runtime_api.lib")
//#pragma comment(lib, "cuda_d3d11_interop.lib")
//#pragma comment(lib, "helper_cuda.lib")
//#pragma comment(lib, "helper_string.lib")
//#pragma comment(lib, "libloaderapi.lib")
//#pragma comment(lib, "d3dcompiler.lib")
//#pragma comment(lib, "rendercheck_d3d11.lib")

#include <cuda_runtime_api.h>
#include <cuda_d3d11_interop.h>
#include <helper_cuda.h>
#include <helper_string.h>
#include <libloaderapi.h>
#include <d3dcompiler.h>
#include <rendercheck_d3d11.h>

void OpenCL::Setup(char* executionFile)
{
    this->executionFile = executionFile;

    char device_name[256];
    char* ref_file = NULL;

    printf("[%s] - Starting...\n", SDK_name);

    if (!findCUDADevice())                   // Search for CUDA GPU
    {
        printf("> CUDA Device NOT found on \"%s\".. Exiting.\n", device_name);
        exit(EXIT_SUCCESS);
    }

    if (!dynlinkLoadD3D11API())                  // Search for D3D API (locate drivers, does not mean device is found)
    {
        printf("> D3D11 API libraries NOT found on.. Exiting.\n");
        dynlinkUnloadD3D11API();
        exit(EXIT_SUCCESS);
    }

    if (!findDXDevice(device_name))           // Search for D3D Hardware Device
    {
        printf("> D3D11 Graphics Device NOT found.. Exiting.\n");
        dynlinkUnloadD3D11API();
        exit(EXIT_SUCCESS);
    }

    CreateOpenCLWindow();
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);
}
void Cleanup() {};
void OpenCL::Run()
{
    RegisterSimulation();

    char* ref_file = NULL;

    //
    // the main loop
    //
    while (g_bDone == false)
    {
        Render();

        //
        // handle I/O
        //
        MSG msg;
        ZeroMemory(&msg, sizeof(msg));

        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                Render();

                if (ref_file)
                {
                    for (int count = 0; count < g_iFrameToCompare; count++)
                    {
                        Render();
                    }

                    const char* cur_image_path = "simpleD3D11Texture.ppm";

                    // Save a reference of our current test run image
                    CheckRenderD3D11::ActiveRenderTargetToPPM(g_pd3dDevice, cur_image_path);

                    // compare to offical reference image, printing PASS or FAIL.
                    g_bPassed = CheckRenderD3D11::PPMvsPPM(cur_image_path, ref_file, executionFile, MAX_EPSILON, 0.15f);

                    g_bDone = true;

                    Cleanup();

                    PostQuitMessage(0);
                }
                else
                {
                    g_bPassed = true;
                }
            }
        }
    }
}

bool OpenCL::findCUDADevice()
{
    int nGraphicsGPU = 0;
    int deviceCount = 0;
    bool bFoundGraphics = false;
    char devname[NAME_LEN];

    // This function call returns 0 if there are no CUDA capable devices.
    cudaError_t error_id = cudaGetDeviceCount(&deviceCount);

    if (error_id != cudaSuccess)
    {
        printf("cudaGetDeviceCount returned %d\n-> %s\n", (int)error_id, cudaGetErrorString(error_id));
        exit(EXIT_FAILURE);
    }

    if (deviceCount == 0)
    {
        printf("> There are no device(s) supporting CUDA\n");
        return false;
    }
    else
    {
        printf("> Found %d CUDA Capable Device(s)\n", deviceCount);
    }

    // Get CUDA device properties
    cudaDeviceProp deviceProp;

    for (int dev = 0; dev < deviceCount; ++dev)
    {
        cudaGetDeviceProperties(&deviceProp, dev);
        STRCPY(devname, NAME_LEN, deviceProp.name);
        printf("> GPU %d: %s\n", dev, devname);
    }

    return true;
}

bool OpenCL::findDXDevice(char* dev_name)
{
    HRESULT hr = S_OK;
    cudaError cuStatus;

    // Iterate through the candidate adapters
    IDXGIFactory* pFactory;
    hr = sFnPtr_CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&pFactory));

    if (!SUCCEEDED(hr))
    {
        printf("> No DXGI Factory created.\n");
        return false;
    }

    UINT adapter = 0;

    for (; !g_pCudaCapableAdapter; ++adapter)
    {
        // Get a candidate DXGI adapter
        IDXGIAdapter* pAdapter = NULL;
        hr = pFactory->EnumAdapters(adapter, &pAdapter);

        if (FAILED(hr))
        {
            break;    // no compatible adapters found
        }

        // Query to see if there exists a corresponding compute device
        int cuDevice;
        cuStatus = cudaD3D11GetDevice(&cuDevice, pAdapter);
        printLastCudaError("cudaD3D11GetDevice failed"); //This prints and resets the cudaError to cudaSuccess

        if (cudaSuccess == cuStatus)
        {
            // If so, mark it as the one against which to create our d3d10 device
            g_pCudaCapableAdapter = pAdapter;
            g_pCudaCapableAdapter->AddRef();
        }

        pAdapter->Release();
    }

    printf("> Found %d D3D11 Adapater(s).\n", (int)adapter);

    pFactory->Release();

    if (!g_pCudaCapableAdapter)
    {
        printf("> Found 0 D3D11 Adapater(s) /w Compute capability.\n");
        return false;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    g_pCudaCapableAdapter->GetDesc(&adapterDesc);
    wcstombs(dev_name, adapterDesc.Description, 128);

    printf("> Found 1 D3D11 Adapater(s) /w Compute capability.\n");
    printf("> %s\n", dev_name);

    return true;
}

bool g_bDone = false;
LRESULT __stdcall MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            g_bDone = true;
            Cleanup();
            PostQuitMessage(0);
            return 0;
        }

        break;

    case WM_DESTROY:
        g_bDone = true;
        Cleanup();
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        ValidateRect(hWnd, NULL);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void OpenCL::CreateOpenCLWindow()
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "CUDA SDK", NULL
    };
    RegisterClassEx(&wc);

    // Create the application's window
    int xBorder = ::GetSystemMetrics(SM_CXSIZEFRAME);
    int yMenu = ::GetSystemMetrics(SM_CYMENU);
    int yBorder = ::GetSystemMetrics(SM_CYSIZEFRAME);
    hWnd = CreateWindow(wc.lpszClassName, "CUDA/D3D11 Texture InterOP",
        WS_OVERLAPPEDWINDOW, 0, 0, g_WindowWidth + 2 * xBorder, g_WindowHeight + 2 * yBorder + yMenu,
        NULL, NULL, wc.hInstance, NULL);
}

void OpenCL::RegisterSimulation()
{
    if (SUCCEEDED(InitD3D(hWnd)) &&
        SUCCEEDED(InitTextures()))
    {
        // 2D
        // register the Direct3D resources that we'll use
        // we'll read to and write from g_texture_2d, so don't set any special map flags for it
        cudaGraphicsD3D11RegisterResource(&g_texture_2d.cudaResource, g_texture_2d.pTexture, cudaGraphicsRegisterFlagsNone);
        getLastCudaError("cudaGraphicsD3D11RegisterResource (g_texture_2d) failed");
        // cuda cannot write into the texture directly : the texture is seen as a cudaArray and can only be mapped as a texture
        // Create a buffer so that cuda can write into it
        // pixel fmt is DXGI_FORMAT_R32G32B32A32_FLOAT
        cudaMallocPitch(&g_texture_2d.cudaLinearMemory, &g_texture_2d.pitch, g_texture_2d.width * sizeof(float) * 4, g_texture_2d.height);
        getLastCudaError("cudaMallocPitch (g_texture_2d) failed");
        cudaMemset(g_texture_2d.cudaLinearMemory, 1, g_texture_2d.pitch * g_texture_2d.height);
    }
}

void OpenCL::Render()
{
    //
    // map the resources we've registered so we can access them in Cuda
    // - it is most efficient to map and unmap all resources in a single call,
    //   and to have the map/unmap calls be the boundary between using the GPU
    //   for Direct3D and Cuda
    //
    static bool doit = true;

    if (doit)
    {
        doit = true;
        cudaStream_t    stream = 0;
        const int nbResources = 3;
        cudaGraphicsResource* ppResources[nbResources] =
        {
            g_texture_2d.cudaResource
        };
        cudaGraphicsMapResources(nbResources, ppResources, stream);
        getLastCudaError("cudaGraphicsMapResources(3) failed");

        //
        // run kernels which will populate the contents of those textures
        //
        RunKernels();

        //
        // unmap the resources
        //
        cudaGraphicsUnmapResources(nbResources, ppResources, stream);
        getLastCudaError("cudaGraphicsUnmapResources(3) failed");
    }

    //
    // draw the scene using them
    //
    DrawScene();
}


void OpenCL::RunKernels()
{
    static float t = 0.0f;

    // populate the 2d texture bottom left
    {
        cudaArray* cuArray;
        cudaGraphicsSubResourceGetMappedArray(&cuArray, g_texture_2d.cudaResource, 0, 0);
        getLastCudaError("cudaGraphicsSubResourceGetMappedArray (cuda_texture_2d) failed");

        // kick off the kernel and send the staging buffer cudaLinearMemory as an argument to allow the kernel to write to it
        cuda_texture_2d(g_texture_2d.cudaLinearMemory, g_texture_2d.width, g_texture_2d.height, g_texture_2d.pitch, t);
        getLastCudaError("cuda_texture_2d failed");

        // then we want to copy cudaLinearMemory to the D3D texture, via its mapped form : cudaArray
        cudaMemcpy2DToArray(
            cuArray, // dst array
            0, 0,    // offset
            g_texture_2d.cudaLinearMemory, g_texture_2d.pitch,       // src
            g_texture_2d.width * 4 * sizeof(float), g_texture_2d.height, // extent
            cudaMemcpyDeviceToDevice); // kind
        getLastCudaError("cudaMemcpy2DToArray failed");
    }

    t += 0.1f;
}

void OpenCL::DrawScene()
{

}

HRESULT OpenCL::InitD3D(HWND hWnd)
{
    HRESULT hr = S_OK;

    // Set up the structure used to create the device and swapchain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_WindowWidth;
    sd.BufferDesc.Height = g_WindowHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL tour_fl[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL flRes;
    // Create device and swapchain
    hr = sFnPtr_D3D11CreateDeviceAndSwapChain(
        g_pCudaCapableAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,//D3D_DRIVER_TYPE_HARDWARE,
        NULL, //HMODULE Software
        0, //UINT Flags
        tour_fl, // D3D_FEATURE_LEVEL* pFeatureLevels
        3, //FeatureLevels
        D3D11_SDK_VERSION, //UINT SDKVersion
        &sd, // DXGI_SWAP_CHAIN_DESC* pSwapChainDesc
        &g_pSwapChain, //IDXGISwapChain** ppSwapChain
        &g_pd3dDevice, //ID3D11Device** ppDevice
        &flRes, //D3D_FEATURE_LEVEL* pFeatureLevel
        &g_pd3dDeviceContext//ID3D11DeviceContext** ppImmediateContext
    );
    AssertOrQuit(SUCCEEDED(hr));

    g_pCudaCapableAdapter->Release();

    // Get the immediate DeviceContext
    g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

    // Create a render target view of the swapchain
    ID3D11Texture2D* pBuffer;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
    AssertOrQuit(SUCCEEDED(hr));

    hr = g_pd3dDevice->CreateRenderTargetView(pBuffer, NULL, &g_pSwapChainRTV);
    AssertOrQuit(SUCCEEDED(hr));
    pBuffer->Release();

    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pSwapChainRTV, NULL);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = g_WindowWidth;
    vp.Height = g_WindowHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &vp);

    ID3DBlob* pShader;
    ID3DBlob* pErrorMsgs;
    // Vertex shader
    {
        hr = D3DCompile(g_simpleShaders, strlen(g_simpleShaders), "Memory", NULL, NULL, "VS", "vs_4_0", 0/*Flags1*/, 0/*Flags2*/, &pShader, &pErrorMsgs);

        if (FAILED(hr))
        {
            const char* pStr = (const char*)pErrorMsgs->GetBufferPointer();
            printf(pStr);
        }

        AssertOrQuit(SUCCEEDED(hr));
        hr = g_pd3dDevice->CreateVertexShader(pShader->GetBufferPointer(), pShader->GetBufferSize(), NULL, &g_pVertexShader);
        AssertOrQuit(SUCCEEDED(hr));
        // Let's bind it now : no other vtx shader will replace it...
        g_pd3dDeviceContext->VSSetShader(g_pVertexShader, NULL, 0);
        //hr = g_pd3dDevice->CreateInputLayout(...pShader used for signature...) No need
    }
    // Pixel shader
    {
        hr = D3DCompile(g_simpleShaders, strlen(g_simpleShaders), "Memory", NULL, NULL, "PS", "ps_4_0", 0/*Flags1*/, 0/*Flags2*/, &pShader, &pErrorMsgs);

        AssertOrQuit(SUCCEEDED(hr));
        hr = g_pd3dDevice->CreatePixelShader(pShader->GetBufferPointer(), pShader->GetBufferSize(), NULL, &g_pPixelShader);
        AssertOrQuit(SUCCEEDED(hr));
        // Let's bind it now : no other pix shader will replace it...
        g_pd3dDeviceContext->PSSetShader(g_pPixelShader, NULL, 0);
    }
    // Create the constant buffer
    {
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;//D3D11_BIND_SHADER_RESOURCE;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.ByteWidth = 16 * ((sizeof(ConstantBuffer) + 16) / 16);
        //cbDesc.StructureByteStride = 0;
        hr = g_pd3dDevice->CreateBuffer(&cbDesc, NULL, &g_pConstantBuffer);
        AssertOrQuit(SUCCEEDED(hr));
        // Assign the buffer now : nothing in the code will interfere with this (very simple sample)
        g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
        g_pd3dDeviceContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    }
    // SamplerState
    {
        D3D11_SAMPLER_DESC sDesc;
        sDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sDesc.MinLOD = 0;
        sDesc.MaxLOD = 8;
        sDesc.MipLODBias = 0;
        sDesc.MaxAnisotropy = 1;
        hr = g_pd3dDevice->CreateSamplerState(&sDesc, &g_pSamplerState);
        AssertOrQuit(SUCCEEDED(hr));
        g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_pSamplerState);
    }

    // Setup  no Input Layout
    g_pd3dDeviceContext->IASetInputLayout(0);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D11_RASTERIZER_DESC rasterizerState;
    rasterizerState.FillMode = D3D11_FILL_SOLID;
    rasterizerState.CullMode = D3D11_CULL_FRONT;
    rasterizerState.FrontCounterClockwise = false;
    rasterizerState.DepthBias = false;
    rasterizerState.DepthBiasClamp = 0;
    rasterizerState.SlopeScaledDepthBias = 0;
    rasterizerState.DepthClipEnable = false;
    rasterizerState.ScissorEnable = false;
    rasterizerState.MultisampleEnable = false;
    rasterizerState.AntialiasedLineEnable = false;
    g_pd3dDevice->CreateRasterizerState(&rasterizerState, &g_pRasterState);
    g_pd3dDeviceContext->RSSetState(g_pRasterState);

    return S_OK;
}

HRESULT OpenCL::InitTextures()
{
    //
    // create the D3D resources we'll be using
    //
    // 2D texture
    {
        g_texture_2d.width = g_WindowWidth;
        g_texture_2d.height = g_WindowHeight;

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = g_texture_2d.width;
        desc.Height = g_texture_2d.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, NULL, &g_texture_2d.pTexture)))
        {
            return E_FAIL;
        }

        if (FAILED(g_pd3dDevice->CreateShaderResourceView(g_texture_2d.pTexture, NULL, &g_texture_2d.pSRView)))
        {
            return E_FAIL;
        }

        g_texture_2d.offsetInShader = 0; // to be clean we should look for the offset from the shader code
        g_pd3dDeviceContext->PSSetShaderResources(g_texture_2d.offsetInShader, 1, &g_texture_2d.pSRView);
    }

    return S_OK;
}

OpenCL::~OpenCL()
{
    // unregister the Cuda resources
    /*cudaGraphicsUnregisterResource(g_texture_2d.cudaResource);
    getLastCudaError("cudaGraphicsUnregisterResource (g_texture_2d) failed");
    cudaFree(g_texture_2d.cudaLinearMemory);
    getLastCudaError("cudaFree (g_texture_2d) failed");

    cudaGraphicsUnregisterResource(g_texture_cube.cudaResource);
    getLastCudaError("cudaGraphicsUnregisterResource (g_texture_cube) failed");
    cudaFree(g_texture_cube.cudaLinearMemory);
    getLastCudaError("cudaFree (g_texture_2d) failed");

    cudaGraphicsUnregisterResource(g_texture_3d.cudaResource);
    getLastCudaError("cudaGraphicsUnregisterResource (g_texture_3d) failed");
    cudaFree(g_texture_3d.cudaLinearMemory);
    getLastCudaError("cudaFree (g_texture_2d) failed");*/

    //
    // clean up Direct3D
    //
    {
        // release the resources we created
        /*g_texture_2d.pSRView->Release();
        g_texture_2d.pTexture->Release();
        g_texture_cube.pSRView->Release();
        g_texture_cube.pTexture->Release();
        g_texture_3d.pSRView->Release();
        g_texture_3d.pTexture->Release();*/

        if (g_pInputLayout != NULL)
        {
            g_pInputLayout->Release();
        }

        /*if (g_pVertexShader)
        {
            g_pVertexShader->Release();
        }

        if (g_pPixelShader)
        {
            g_pPixelShader->Release();
        }

        if (g_pConstantBuffer)
        {
            g_pConstantBuffer->Release();
        }

        if (g_pSamplerState)
        {
            g_pSamplerState->Release();
        }*/

        if (g_pSwapChainRTV != NULL)
        {
            g_pSwapChainRTV->Release();
        }

        if (g_pSwapChain != NULL)
        {
            g_pSwapChain->Release();
        }

        if (g_pd3dDevice != NULL)
        {
            g_pd3dDevice->Release();
        }
    }
}