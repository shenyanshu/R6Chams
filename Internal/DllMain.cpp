#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>
#include <D3DX11tex.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "winmm.lib")
#include <d3dcommon.h>
#include "MinHook/include/MinHook.h"
#include <DirectXMath.h>
#pragma warning( disable : 4244 )
#include <string>
#include <stdio.h>
#include <memoryapi.h>
#include <stdint.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <ntstatus.h>
#include <assert.h>
#include <cstdint>
#include <type_traits>
#include <winternl.h>
#include <map>
#include <wtypes.h>
#include <mutex>
#include <charconv>
#include <random>


typedef void(__stdcall* D3D11DrawIndexedInstancedIndirectHook) (ID3D11DeviceContext* pContext, ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);


D3D11DrawIndexedInstancedIndirectHook phookD3D11DrawIndexedInstancedIndirect = NULL;
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef uintptr_t PTR;

//create texture
ID3D11Texture2D* texGreen = nullptr;
ID3D11Texture2D* texRed = nullptr;

//create shaderresourcevew
ID3D11ShaderResourceView* texSRVg;
ID3D11ShaderResourceView* texSRVr;

//create samplerstate
ID3D11SamplerState* pSamplerState;

class Model {
private:
	UINT ModelStride;
	UINT ModelvscWidth;
	UINT ModelpscWidth;
	UINT ModelpsDescrFORMAT;
public:
	Model(UINT Stride, UINT vscWidth, UINT pscWidth, UINT psDescrFORMAT) { ModelStride = Stride; ModelvscWidth = vscWidth; ModelpscWidth = pscWidth; ModelpsDescrFORMAT = psDescrFORMAT; };

	bool IsModel(UINT Stride, UINT vscWidth, UINT pscWidth) { if (Stride == ModelStride && vscWidth == ModelvscWidth && pscWidth == ModelpscWidth)return true; else return false; };
	bool IsModel(UINT Stride, UINT vscWidth, UINT pscWidth, UINT psDescrFORMAT) { if (Stride == ModelStride && vscWidth == ModelvscWidth && pscWidth == ModelpscWidth && psDescrFORMAT == ModelpsDescrFORMAT)return true; else return false; };
};


namespace Models {
	Model Sky(8, 11, 11, 95);

	Model Eyes(8, 60, 4, 71);
	Model Arms(8, 60, 4, 77);
	Model Body1(8, 60, 12, NULL);
	Model Body2(8, 60, 14, NULL);

	Model DroneWorldOverlay(8, 49, 3, NULL);
	Model Barricades(8, 49, 16, NULL);
	Model WorldOverlay1(8, 49, 4, NULL);
	Model WorldOverlay2(8, 49, 12, NULL);
	Model WorldOverlay3(8, 49, 14, NULL);
	Model WorldOverlay4(8, 49, 49, NULL);
	Model Scope(8, 49, 49, 0);
}

void __stdcall hookD3D11DrawIndexedInstancedIndirect(ID3D11DeviceContext* pContext, ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	ID3D11Buffer* veBuffer;
	UINT veWidth;
	UINT Stride;
	UINT veBufferOffset;
	D3D11_BUFFER_DESC veDesc;

	static ID3D11RasterizerState* PlayerState = NULL;
	static ID3D11RasterizerState* BarricadeState = NULL;

	ID3D11Device* Device;
	pContext->GetDevice(&Device);

	if (texRed == nullptr) {
		//sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		Device->CreateSamplerState(&sampDesc, &pSamplerState);

		//create green texture
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //format
		static const uint32_t s_pixel = 0xff00ff00; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red, 0x00000000
		D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
		Device->CreateTexture2D(&desc, &initData, &texGreen);

		//create red texture
		static const uint32_t s_pixelr = 0xff0000ff; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red, 0x00000000
		D3D11_SUBRESOURCE_DATA initDatar = { &s_pixelr, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC descr;
		memset(&descr, 0, sizeof(descr));
		descr.Width = descr.Height = descr.MipLevels = descr.ArraySize = 1;
		descr.Format = format;
		descr.SampleDesc.Count = 1;
		descr.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		descr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
		Device->CreateTexture2D(&descr, &initDatar, &texRed);

		//create green shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset(&SRVDesc, 0, sizeof(SRVDesc));
		SRVDesc.Format = format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		Device->CreateShaderResourceView(texGreen, &SRVDesc, &texSRVg);
		texGreen->Release();

		//create red shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescr;
		memset(&SRVDescr, 0, sizeof(SRVDescr));
		SRVDescr.Format = format;
		SRVDescr.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDescr.Texture2D.MipLevels = 1;
		Device->CreateShaderResourceView(texRed, &SRVDescr, &texSRVr);
		texRed->Release();
	}

	//get models
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer) {
		veBuffer->GetDesc(&veDesc);
		veWidth = veDesc.ByteWidth;
	}
	if (NULL != veBuffer) {
		veBuffer->Release();
		veBuffer = NULL;
	}

	ID3D11Buffer* pscBuffer;
	UINT pscWidth;
	D3D11_BUFFER_DESC pscdesc;

	//get pscdesc.ByteWidth
	pContext->PSGetConstantBuffers(0, 1, &pscBuffer);
	if (pscBuffer) {
		pscBuffer->GetDesc(&pscdesc);
		pscWidth = pscdesc.ByteWidth;
	}
	if (NULL != pscBuffer) {
		pscBuffer->Release();
		pscBuffer = NULL;
	}

	ID3D11Buffer* vscBuffer;
	UINT vscWidth;
	D3D11_BUFFER_DESC vscdesc;

	pContext->VSGetConstantBuffers(0, 1, &vscBuffer);
	if (vscBuffer) {
		vscBuffer->GetDesc(&vscdesc);
		vscWidth = vscdesc.ByteWidth;
	}
	if (NULL != vscBuffer) {
		vscBuffer->Release();
		vscBuffer = NULL;
	}

	ID3D11ShaderResourceView* psShaderResourceView1;
	D3D11_SHADER_RESOURCE_VIEW_DESC psDescr;
	pContext->PSGetShaderResources(1, 1, &psShaderResourceView1);
	if (psShaderResourceView1)
	{
		psShaderResourceView1->GetDesc(&psDescr);
	}

	if (Models::Eyes.IsModel(Stride, (vscWidth / 10), (pscWidth / 10), psDescr.Format) ||
		Models::Arms.IsModel(Stride, (vscWidth / 10), (pscWidth / 10), psDescr.Format) ||
		Models::Body1.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) ||
		(Models::Body2.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) && (39 != psDescr.Format)))
	{
		ID3D11RasterizerState* OldState;
		pContext->RSGetState(&OldState);
		if (PlayerState == NULL) {
			D3D11_RASTERIZER_DESC CustomStateDesc;
			OldState->GetDesc(&CustomStateDesc);
			//CustomStateDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
			CustomStateDesc.ScissorEnable = FALSE;
			//CustomStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;
			CustomStateDesc.DepthBias = INT_MAX;
			CustomStateDesc.DepthClipEnable = TRUE;
			Device->CreateRasterizerState(&CustomStateDesc, &PlayerState);
		}
		pContext->RSSetState(PlayerState);
		phookD3D11DrawIndexedInstancedIndirect(pContext, pBufferForArgs, AlignedByteOffsetForArgs);
		pContext->RSSetState(OldState);
		/*
		for (int x1 = 0; x1 <= 10; x1++)
		{
			pContext->PSSetShaderResources(x1, 1, &texSRVr);
		}
		pContext->PSSetSamplers(0, 1, &pSamplerState);
		return phookD3D11DrawIndexedInstancedIndirect(pContext, pBufferForArgs, AlignedByteOffsetForArgs);
		*/
		return;
	}
	else if (Models::Barricades.IsModel(Stride, (vscWidth / 10), (pscWidth / 10))) {
		ID3D11RasterizerState* OldState;
		pContext->RSGetState(&OldState);
		if (BarricadeState == NULL) {
			D3D11_RASTERIZER_DESC CustomStateDesc;
			OldState->GetDesc(&CustomStateDesc);
			CustomStateDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
			CustomStateDesc.ScissorEnable = FALSE;
			CustomStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;
			Device->CreateRasterizerState(&CustomStateDesc, &BarricadeState);
		}
		pContext->RSSetState(BarricadeState);
		phookD3D11DrawIndexedInstancedIndirect(pContext, pBufferForArgs, AlignedByteOffsetForArgs);
		pContext->RSSetState(OldState);
		return;
	}
	else if (Models::DroneWorldOverlay.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) ||
		Models::WorldOverlay1.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) ||
		Models::WorldOverlay2.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) ||
		Models::WorldOverlay3.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) ||
		(Models::WorldOverlay4.IsModel(Stride, (vscWidth / 10), (pscWidth / 10)) && !Models::Scope.IsModel(Stride, (vscWidth / 10), (pscWidth / 10), psDescr.Format)))
	{
		return;
	}

	return phookD3D11DrawIndexedInstancedIndirect(pContext, pBufferForArgs, AlignedByteOffsetForArgs);
}

DWORD WINAPI InitiateChams()
{
	do
	{
		Sleep(100);
	} while (!GetModuleHandleA("d3d11.dll"));

	uintptr_t DrawIndexedFunction = uintptr_t(GetModuleHandleA("d3d11.dll")) + 0x166950;
	uintptr_t Vtable = uintptr_t(GetModuleHandleA("d3d11.dll")) + 0x1CE1A0;

	DWORD dwOld;
	VirtualProtect((void*)Vtable, 1, PAGE_EXECUTE_READWRITE, &dwOld);
	*(void**)Vtable = &hookD3D11DrawIndexedInstancedIndirect;
	VirtualProtect((void*)Vtable, 1, dwOld, &dwOld);

	phookD3D11DrawIndexedInstancedIndirect = (D3D11DrawIndexedInstancedIndirectHook)DrawIndexedFunction;

	return true;
}

bool DllMain(const HMODULE moduleInstance, const std::uint32_t callReason, void*)
{
	DisableThreadLibraryCalls(moduleInstance);
	if (callReason == DLL_PROCESS_ATTACH)
	{
		CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InitiateChams, NULL, 0, nullptr));
	}
	return true;
}