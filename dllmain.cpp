// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MinHook.h"
#include <cstdint>
#include <vector>

#pragma comment(lib, "libMinHook.x86.lib")

#include <chrono> 

void patchJmp(void* addr, void* func) {
	DWORD oldProtect;

	VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(uint8_t*)addr = 0xE9;
	*(uint32_t*)((uint8_t*)addr + 1) = (uint32_t)func - (uint32_t)addr - 5;
	VirtualProtect(addr, 5, oldProtect, &oldProtect);
}


bool wasPressedThisFrame[256];
bool lastFrameStates[256];
void UpdateKeys()
{
	for (int i = 0; i < 256; i++)
	{
		bool thisFrameState = GetKeyState(i) < 0;
		wasPressedThisFrame[i] = thisFrameState && !lastFrameStates[i];
		lastFrameStates[i] = thisFrameState;
	}
}

inline float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

__declspec(noinline) bool IsKeyPressed(unsigned char Key, bool Hold) // USE THIS FROM NOW ON
{
	if (*(uint8_t*)0x252A406)
	{
		if (Hold)
		{
			return lastFrameStates[Key];
		}
		else
		{
			return wasPressedThisFrame[Key];
		}
	}
	return false;
}

float getDeltaTime() {
    static DWORD lastTime = GetTickCount64();
    DWORD currentTime = GetTickCount64();
    DWORD elapsedMillis = currentTime - lastTime;
    lastTime = currentTime;
    return elapsedMillis / 1000.0f;
}

float EditedZoomMod = 0.f;
float zoom_values[] = { -2.f, -1.f, 0.f, 1.f, 2.f };
BYTE onfootIndex = 2;
BYTE inVehicleIndex = 2;
BYTE fineAimIndex = 2;
void PerformCustomMemoryCopy() {
	UpdateKeys();
		enum status : BYTE {
			vehicle = 3,
			boat = 5,
			helicopter = 6,
			plane = 9,
			sniper = 9,
			fineaimcrouch = 16,
			fineaim = 17,
		};
		BYTE* activeIndex = &onfootIndex;
		status player_status = *(status*)0x00E9A5BC;
		switch (player_status) {
		case vehicle:
		case boat:
		case helicopter:
		case plane:
			activeIndex = &inVehicleIndex;
			break;
		case fineaimcrouch:
		case fineaim:
			activeIndex = &fineAimIndex;
			break;
		default:
			activeIndex = &onfootIndex;
			break;
		}
		float target = zoom_values[*activeIndex];
		switch (player_status) {
		case helicopter:
		case plane:
			target *= 4.f;
			break;
		case fineaimcrouch:
		case fineaim:
			if(target > 0.f)
			target *= 0.35f;
			break;
		default:
			break;
		}
		float speed = 5.0f;

		float t = speed * getDeltaTime();
		if (t > 1.0f) t = 1.0f;

		// Update EditedZoomMod towards the target value
		EditedZoomMod = lerp(EditedZoomMod, target, t);
		if (IsKeyPressed('B', false)) {
			*activeIndex = (*activeIndex + 4) % 5;
		}
		float* src = reinterpret_cast<float*>(0x00E9A638);
		float* dest = reinterpret_cast<float*>(0x00E9A60C);
		size_t size = 0x2C / 4;

		for (size_t i = 0; i < size; ++i) {
			if (reinterpret_cast<uintptr_t>(src) == 0x00E9A654) {
				dest[i] = (*(float*)0x00E9A654) + EditedZoomMod;
				*src++;
			}
			else
				dest[i] = *src++;
		}
	}

void __declspec(naked) HookedRepMovsd() {
    __asm {
        pushad
        pushfd
    }

	PerformCustomMemoryCopy();
    

    __asm {
        popfd
        popad

        mov eax, 0x0049A4DC
        jmp eax
    }
}

void setupHook() {
	patchJmp((void*)0x0049A4CB, HookedRepMovsd);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        setupHook();
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    default:
        break;
    }
    return TRUE;
}