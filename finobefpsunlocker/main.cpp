#include <Windows.h>

#include <string>
#include <iostream>
#include <vector>

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "sigscan.h"

void WINAPI DllInit();
void WINAPI DllExit();

bool WriteMemory(void* address, const void* patch, size_t sz)
{
	DWORD protect;

	if (!VirtualProtect(address, sz, PAGE_EXECUTE_READWRITE, &protect)) return false;
	memcpy((void*)address, patch, sz);
	if (!VirtualProtect(address, sz, protect, (PDWORD)&protect)) return false;

	return true;
}

void* HookVFT(void* object, int index, void* targetf)
{
	int* vftable = *(int**)(object);
	void* previous = (void*)vftable[index];

	WriteMemory(vftable + index, &targetf, sizeof(void*));

	return previous;
}

typedef HRESULT(_stdcall *IDXGISwapChainPresentFn)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
IDXGISwapChainPresentFn IDXGISwapChainPresent;

HRESULT __stdcall IDXGISwapChainPresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	/* 
	 * https://msdn.microsoft.com/en-us/library/windows/desktop/bb174576(v=vs.85).aspx
	 * https://i.imgur.com/BH2lY12.png
	 * 
	 * Enabling DebugGraphicsVsync disables throttling but enables VSync via Present calls
	 * Solution: Hook Present and set SyncInterval to 0
	 */
	return IDXGISwapChainPresent(pSwapChain, 0, Flags);
}

void WINAPI DllInit()
{
	if (GetModuleHandleA("dxgi.dll"))
	{
		/* Scan for DebugGraphicsVsync flag */
		uintptr_t Flag = (uintptr_t)sigscan::scan("FinobePlayer.exe", "\x74\x10\x80\x3D\x00\x00\x00\x00\x00\xC7", "xxxx????xx"); // 74 10 80 3D ?? ?? ?? ?? 00 C7
		if (!Flag)
		{
			MessageBoxA(NULL, "Scan failed", "Error", MB_OK);
			DllExit();
		}
		Flag = *(uint32_t*)(Flag + 4);

		/* Create dummy ID3D11Device to grab its vftable */
		ID3D11Device* Device = 0;
		ID3D11DeviceContext* DeviceContext = 0;
		IDXGISwapChain* SwapChain = 0;
		D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
		DXGI_SWAP_CHAIN_DESC SwapChainDesc;

		ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));
		SwapChainDesc.BufferCount = 1;
		SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.OutputWindow = FindWindowW(NULL, L"Finobe"); // finobe hooks FindWindowA lol
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.Windowed = TRUE;
		SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_NULL, NULL, NULL, &FeatureLevel, 1, D3D11_SDK_VERSION, &SwapChainDesc, &SwapChain, &Device, NULL, &DeviceContext)))
		{
			/* Hook IDXGISwapChain::Present & enable flag */
			IDXGISwapChainPresent = (IDXGISwapChainPresentFn)HookVFT(SwapChain, 8, IDXGISwapChainPresentHook);
			*(unsigned char*)Flag = 1;

			/* Free objects */
			Device->Release();
			DeviceContext->Release();
			SwapChain->Release();
		}
		else
		{
			MessageBoxA(NULL, "Unable to create D3D11 device", "Error", MB_OK);
			DllExit();
		}
	}
}

void WINAPI DllExit()
{
	FreeLibraryAndExitThread(GetModuleHandleA("finobefpsunlocker.dll"), 0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)DllInit, 0, 0, 0);
	}

	return TRUE;
}


