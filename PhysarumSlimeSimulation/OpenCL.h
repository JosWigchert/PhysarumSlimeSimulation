#pragma once

#include <dynlink_d3d11.h>
#include <driver_types.h>

class OpenCL
{
public:
	~OpenCL();

	void Setup(char* executionFile);
	void Run();

private:
	bool findCUDADevice();
	bool findDXDevice(char* dev_name);
	void CreateOpenCLWindow();

	void RegisterSimulation();
	HRESULT InitD3D(HWND hWnd);
	HRESULT InitTextures();
	void Render();
	void RunKernels();
	void DrawScene();


	//LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	//void Cleanup();

private:
	IDXGIAdapter* g_pCudaCapableAdapter = NULL;  // Adapter to use
	ID3D11Device* g_pd3dDevice = NULL; // Our rendering device
	ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
	IDXGISwapChain* g_pSwapChain = NULL; // The swap chain of the window
	ID3D11RenderTargetView* g_pSwapChainRTV = NULL; //The Render target view on the swap chain ( used for clear)
	ID3D11RasterizerState* g_pRasterState = NULL;

	ID3D11InputLayout* g_pInputLayout = NULL;

	ID3D11VertexShader* g_pVertexShader;
	ID3D11PixelShader* g_pPixelShader;
	ID3D11Buffer* g_pConstantBuffer;
	ID3D11SamplerState* g_pSamplerState;

	bool g_bDone = false;
	bool g_bPassed = true;
	char* executionFile;

	int g_iFrameToCompare = 10;

	HWND hWnd;

	char* g_simpleShaders;



protected:
	static constexpr int NAME_LEN = 512;
	static constexpr int MAX_EPSILON = 10;
	const char* SDK_name = "PhasarumSlimeSimulation";

	const unsigned int g_WindowWidth = 1600;
	const unsigned int g_WindowHeight = 900;

	struct ConstantBuffer
	{
		float   vQuadRect[4];
		int     UseCase;
	};

	// Data structure for 2D texture shared between DX10 and CUDA
	struct texture_2d
	{
		ID3D11Texture2D* pTexture;
		ID3D11ShaderResourceView* pSRView;
		cudaGraphicsResource* cudaResource;
		void* cudaLinearMemory;
		size_t                  pitch;
		int                     width;
		int                     height;
		int                     offsetInShader;
	} g_texture_2d;
};

// testing/tracing function used pervasively in tests.  if the condition is unsatisfied
// then spew and fail the function immediately (doing no cleanup)
#define AssertOrQuit(x) \
    if (!(x)) \
    { \
        fprintf(stdout, "Assert unsatisfied in %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
        return 1; \
    }

// The CUDA kernel launchers that get called
extern "C"
{
	bool cuda_texture_2d(void* surface, size_t width, size_t height, size_t pitch, float t);
}