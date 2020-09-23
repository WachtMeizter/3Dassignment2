#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "bth_image.h"
#include <crtdbg.h>//For checking memory leaks

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

#define VERTEXCOUNT 6
#define PI 3.141592f

using namespace DirectX;

//Setting up Direct3D
HWND InitWindow(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT CreateDirect3DContext(HWND wndHandle);
IDXGISwapChain* gSwapChain = nullptr;
ID3D11Device* gDevice = nullptr;
ID3D11DeviceContext* gDeviceContext = nullptr;
ID3D11RenderTargetView* gBackbufferRTV = nullptr;
//Vertex Shader
ID3D11Buffer* gVertexBuffer = nullptr;
ID3D11InputLayout* gVertexLayout = nullptr;
ID3D11VertexShader* gVertexShader = nullptr;
//Fragment Shader
ID3D11PixelShader* gPixelShader = nullptr;
//Geometry Shader
ID3D11GeometryShader* gGeometryShader = nullptr;
//Textures
ID3D11Texture2D *gTexture = nullptr;
ID3D11ShaderResourceView * gTexView = nullptr;
ID3D11SamplerState* gSamplerState = nullptr;
//Z-buffer
ID3D11Texture2D* gDepthStencilBuffer = nullptr;
ID3D11DepthStencilView * gDepthStencilView = nullptr;
//Constant Buffer
ID3D11Buffer* gCBuffer = nullptr; // NEW

//Rotationvariable
float rot = 0.0f;

//define matrices
struct Matrices {
	XMFLOAT4X4	world;
	XMFLOAT4X4  view;
	XMFLOAT4X4	project;
};
Matrices wvp;

//Define a triangle
struct TriangleVertex
{
	float x, y, z;
	float u, v;
};

void CreateConstantBuffer() //Constant buffer to supply matrices to the Geo Shader
{
	// initialize the description of the buffer.
	D3D11_BUFFER_DESC CBuffer;
	CBuffer.Usage = D3D11_USAGE_DYNAMIC;
	CBuffer.ByteWidth = sizeof(Matrices);
	CBuffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBuffer.MiscFlags = 0;
	CBuffer.StructureByteStride = 0;

	// check if the creation failed for any reason
	HRESULT hr = 0;
	hr = gDevice->CreateBuffer(&CBuffer, nullptr, &gCBuffer);
	if (FAILED(hr))
		exit(-1);
}

void InitTexture() //(code lifted straight from texture lexture slides)
{
	//Texture Description
	D3D11_TEXTURE2D_DESC bthTexDesc;
	ZeroMemory(&bthTexDesc, sizeof(bthTexDesc));
	bthTexDesc.Width = BTH_IMAGE_WIDTH;
	bthTexDesc.Height = BTH_IMAGE_HEIGHT;
	bthTexDesc.MipLevels = bthTexDesc.ArraySize = 1;
	bthTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bthTexDesc.SampleDesc.Count = 1;
	bthTexDesc.SampleDesc.Quality = 0;
	bthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	bthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bthTexDesc.MiscFlags = 0;
	bthTexDesc.CPUAccessFlags = 0;

	//Create the texture from raw data (float*) 

	D3D11_SUBRESOURCE_DATA texData;
	ZeroMemory(&texData, sizeof(texData));
	texData.pSysMem = (void*)BTH_IMAGE_DATA;
	texData.SysMemPitch = BTH_IMAGE_WIDTH * 4 * sizeof(char);
	texData.SysMemSlicePitch = 0;

	HRESULT hr = gDevice->CreateTexture2D(&bthTexDesc, &texData, &gTexture);
	if (FAILED(hr))
		exit(-1);

	//Resource view description
	D3D11_SHADER_RESOURCE_VIEW_DESC resViewDesc;
	ZeroMemory(&resViewDesc, sizeof(resViewDesc));
	resViewDesc.Format = bthTexDesc.Format;
	resViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	resViewDesc.Texture2D.MipLevels = bthTexDesc.MipLevels;
	resViewDesc.Texture2D.MostDetailedMip = 0;

	hr = gDevice->CreateShaderResourceView(gTexture, &resViewDesc, &gTexView);
	if (FAILED(hr))
		exit(-1);
	gTexture->Release();


	//Define sampler
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//Create Sampler
	hr = gDevice->CreateSamplerState(&samplerDesc, &gSamplerState);
	if (FAILED(hr))
		exit(-1);
	


	//Enable depth stencil
	D3D11_TEXTURE2D_DESC gDepthBufferDesc;
	ZeroMemory(&gDepthBufferDesc, sizeof(gDepthBufferDesc));
	gDepthBufferDesc.Width = 640; //size of viewport, not image.
	gDepthBufferDesc.Height = 480;
	gDepthBufferDesc.MipLevels = 1;
	gDepthBufferDesc.ArraySize = 1;
	gDepthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	gDepthBufferDesc.SampleDesc.Count = 4; //Same as in Direct3DContext
	gDepthBufferDesc.SampleDesc.Quality = 0;
	gDepthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	gDepthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	gDepthBufferDesc.CPUAccessFlags = 0;
	gDepthBufferDesc.MiscFlags = 0;

	hr = gDevice->CreateTexture2D(&gDepthBufferDesc, nullptr, &gDepthStencilBuffer);
	if (FAILED(hr))
		exit(-1);


	//Create a view of the depth stencil buffer.
	D3D11_DEPTH_STENCIL_VIEW_DESC gDepthStencilViewDesc;
	ZeroMemory(&gDepthStencilViewDesc, sizeof(gDepthStencilViewDesc));
	gDepthStencilViewDesc.Format = gDepthBufferDesc.Format;
	gDepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

	hr = gDevice->CreateDepthStencilView(gDepthStencilBuffer, &gDepthStencilViewDesc, &gDepthStencilView);
	if (FAILED(hr))
		exit(-1);
	gDepthStencilBuffer->Release();
	// Set render target to back buffer
	gDeviceContext->OMSetRenderTargets(1, &gBackbufferRTV, gDepthStencilView);

	D3D11_DEPTH_STENCIL_DESC gDepthStencilDesc;
	//// Depth test parameters
	gDepthStencilDesc.DepthEnable = true;
	gDepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	gDepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	// Stencil test parameters
	gDepthStencilDesc.StencilEnable = true;
	gDepthStencilDesc.StencilReadMask = 0xFF;
	gDepthStencilDesc.StencilWriteMask = 0xFF;
	// Stencil operations if pixel is front-facing
	gDepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	gDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	gDepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	gDepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	// Stencil operations if pixel is back-facing
	gDepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	gDepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	gDepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	gDepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	ID3D11DepthStencilState * gDepthStencilState;
	hr = gDevice->CreateDepthStencilState(&gDepthStencilDesc, &gDepthStencilState);
	if (FAILED(hr))
		exit(-1);
	gDepthStencilState->Release();

};

void CreateMatrices()
{
	//world
	XMStoreFloat4x4(&wvp.world, XMMatrixTranspose(XMMatrixRotationY(0))); //Define world as a rotationmatrix of 0 rads.
	//view
	XMStoreFloat4x4(&wvp.view,
		(
			XMMatrixTranspose(XMMatrixLookAtLH(
				{ 0.0f, 0.0f,-2.0f, 1.0f },	//Eye Position
				{ 0.0f, 0.0f, 0.0f, 1.0f },	//Look at position
				{ 0.0f, 1.0f, 0.0f, 1.0f }  //Up
			)))
	);
	//proj
	XMStoreFloat4x4(&wvp.project,
		(
			XMMatrixTranspose(XMMatrixPerspectiveFovLH(
				PI*0.45f,		//FOV
				640.f / 480.f,  //Aspect Ratio
				0.1f,			//NearZ
				20				//FarZ
			)
			)
			)
	);


}

void CreateShaders()
{
	//create vertex shader
	ID3DBlob* pVS = nullptr;

	D3DCompileFromFile(
		L"Vertex.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"VS_main",		// entry point
		"vs_5_0",		// shader model (target)
		0,				// shader compile options			// here DEBUGGING OPTIONS
		0,				// effect compile options
		&pVS,			// double pointer to ID3DBlob		
		nullptr			// pointer for Error Blob messages.
	);

	HRESULT hr = gDevice->CreateVertexShader(pVS->GetBufferPointer(), pVS->GetBufferSize(), nullptr, &gVertexShader);
	if (FAILED(hr))
		exit(-1);

	//create input layout (verified using vertex shader)
	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = gDevice->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), pVS->GetBufferPointer(), pVS->GetBufferSize(), &gVertexLayout);
	// we do not need this COM object anymore, so we release it.
	pVS->Release();

	//create pixel shader
	ID3DBlob* pPS = nullptr;
	D3DCompileFromFile(
		L"Fragment.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"PS_main",		// entry point
		"ps_5_0",		// shader model (target)
		0,				// shader compile options
		0,				// effect compile options
		&pPS,			// double pointer to ID3DBlob		
		nullptr			// pointer for Error Blob messages.
	);

	hr = gDevice->CreatePixelShader(pPS->GetBufferPointer(), pPS->GetBufferSize(), nullptr, &gPixelShader);
	if (FAILED(hr))
		exit(-1);
	// we do not need this COM object anymore, so we release it.
	pPS->Release();

	ID3DBlob* pGS = nullptr;
	D3DCompileFromFile(
		L"GeometryShader.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"GS_main",		// entry point
		"gs_5_0",		// shader model (target)
		0,				// shader compile options
		0,				// effect compile options
		&pGS,			// double pointer to ID3DBlob		
		nullptr			// pointer for Error Blob messages.
	);

	hr = gDevice->CreateGeometryShader(pGS->GetBufferPointer(), pGS->GetBufferSize(), nullptr, &gGeometryShader);
	if (FAILED(hr))
		exit(-1);
	pGS->Release();
}

void CreateTriangleData()
{
	//Define quad
	TriangleVertex triangleVertices[VERTEXCOUNT] =
	{
		//Triangle 1
		-0.5f, 0.5f, 0.0f,	//v0 pos
		0.0f, 0.0f,	//v0 tex
		0.5f, 0.5f, 0.0f,	//v1
		1.0f, 0.0f,	//v1 tex
		0.5f, -0.5f, 0.0f, //v2
		1.0f, 1.0f,	//v2 tex

		//Triangle 2
		-0.5f, 0.5f, 0.0f,	//v0 pos
		0.0f, 0.0f,	//v0 tex
		0.5f, -0.5f, 0.0f,	//v1 pos  
		1.0f, 1.0f,	//v1 tex
		-0.5f, -0.5f, 0.0f, //v2 pos
		0.0f, 1.0f,	//v2 tex

	};

	D3D11_BUFFER_DESC bufferDesc;
	memset(&bufferDesc, 0, sizeof(bufferDesc));
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(triangleVertices);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = triangleVertices;
	gDevice->CreateBuffer(&bufferDesc, &data, &gVertexBuffer);
}

void Render()
{
	float clearColor[] = { 0, 0, 0, 1 };
	
	//Clear screen and depthbuffer
	gDeviceContext->ClearRenderTargetView(gBackbufferRTV, clearColor);
	gDeviceContext->ClearDepthStencilView(gDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	UINT32 vertexSize = sizeof(TriangleVertex);
	UINT32 offset = 0;
	//Setting Shaders (HS and DS shaders removed for clarity)
	//VertexShader
	gDeviceContext->VSSetShader(gVertexShader, nullptr, 0);
	
	gDeviceContext->IASetVertexBuffers(0, 1, &gVertexBuffer, &vertexSize, &offset);
	gDeviceContext->IASetInputLayout(gVertexLayout);
	gDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//GeoShader
	gDeviceContext->GSSetShader(gGeometryShader, nullptr, 0);
	//Pixelshader + resources
	gDeviceContext->PSSetShader(gPixelShader, nullptr, 0);
	gDeviceContext->PSSetShaderResources(0, 1, &gTexView);
	gDeviceContext->PSSetSamplers(0, 1, &gSamplerState);

	//Mapping CBuffer
	D3D11_MAPPED_SUBRESOURCE dataPtr;
	gDeviceContext->Map(gCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
	
	//Rotation matrix
	rot += 0.0002f;
	rot /= 1.0f;
	XMStoreFloat4x4(&wvp.world, XMMatrixTranspose(XMMatrixRotationY(rot)));
	memcpy(dataPtr.pData, &wvp, sizeof(wvp));
	//Unmap CBuffer
	gDeviceContext->Unmap(gCBuffer, 0);
	//Feed constant buffer to Geometry Shader
	gDeviceContext->GSSetConstantBuffers(0, 1, &gCBuffer);
	// Draw geometry
	gDeviceContext->Draw(VERTEXCOUNT, 0);

}

void SetViewport()
{
	D3D11_VIEWPORT vp;
	vp.Width = (float)640;
	vp.Height = (float)480;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gDeviceContext->RSSetViewports(1, &vp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	MSG msg = { 0 };
	HWND wndHandle = InitWindow(hInstance); //1. Skapa fönster

	if (wndHandle)
	{
		CreateDirect3DContext(wndHandle); //2. Create and connect SwapChain, Device och Device Context

		SetViewport(); //3. Set viewport

		CreateShaders(); //4. Skapa vertex- och pixel-shaders

		CreateTriangleData(); //5. Define triangle vertices, 6. Create vertex buffer, 7. Create input layout

		CreateConstantBuffer(); //8. Create constant buffer.

		CreateMatrices(); //9. Create World, View and Projection Matrices.

		InitTexture(); //10. Initialise texture, sampler and depth stencil buffer.

		ShowWindow(wndHandle, nCmdShow);

		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				Render(); //11. Rendera

				gSwapChain->Present(0, 0); //12. Växla front- och back-buffer
			}
		}

		//Release all the buffers
		gVertexBuffer->Release();
		gVertexLayout->Release();
		gVertexShader->Release();
		gPixelShader->Release();
		gBackbufferRTV->Release();
		gSwapChain->Release();
		gDevice->Release();
		gDeviceContext->Release();
		DestroyWindow(wndHandle);
	}

	return (int)msg.wParam;
}

HWND InitWindow(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = L"BTH_D3D_DEMO";
	if (!RegisterClassEx(&wcex))
		return false;

	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	HWND handle = CreateWindow(
		L"BTH_D3D_DEMO",
		L"BTH Direct3D Demo",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	return handle;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

HRESULT CreateDirect3DContext(HWND wndHandle)
{
	// create a struct to hold information about the swap chain
	DXGI_SWAP_CHAIN_DESC scd;

	// clear out the struct for use
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	scd.BufferCount = 1;                                    // one back buffer
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	scd.OutputWindow = wndHandle;                           // the window to be used
	scd.SampleDesc.Count = 4;                               // how many multisamples
	scd.Windowed = TRUE;                                    // windowed/full-screen mode

															// create a device, device context and swap chain using the information in the scd struct
	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		NULL,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&scd,
		&gSwapChain,
		&gDevice,
		NULL,
		&gDeviceContext);

	if (SUCCEEDED(hr))
	{
		// get the address of the back buffer
		ID3D11Texture2D* pBackBuffer = nullptr;
		gSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

		// use the back buffer address to create the render target
		gDevice->CreateRenderTargetView(pBackBuffer, NULL, &gBackbufferRTV);
		pBackBuffer->Release();

		// set the render target as the back buffer
		gDeviceContext->OMSetRenderTargets(1, &gBackbufferRTV, gDepthStencilView);
	}
	return hr;
}