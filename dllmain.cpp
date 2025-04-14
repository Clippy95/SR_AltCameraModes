// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MinHook.h"
#include <cstdint>
#include <vector>
#include "GameConfig.h"
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
float zoom_values[] = { 2.f, -1.f, 0.f, 1.f, 2.f };
#define ZOOM_MID_INDEX (sizeof(zoom_values) / sizeof(zoom_values[0]) / 2)
BYTE onfootIndex = ZOOM_MID_INDEX;
BYTE inVehicleIndex = ZOOM_MID_INDEX;
BYTE fineAimIndex = ZOOM_MID_INDEX;
void PerformCustomMemoryCopy() {
	UpdateKeys();
	enum camera_free_submodes : BYTE
	{
		CFSM_EXTERIOR_CLOSE = 0x0,
		CFSM_INTERIOR_CLOSE = 0x1,
		CFSM_INTERIOR_SPRINT = 0x2,
		CFSM_VEHICLE_DRIVER = 0x3,
		CFSM_VEHICLE_DRIVER_ALT = 0x4,
		CFSM_WATERCRAFT_DRIVER = 0x5,
		CFSM_HELICOPTER_DRIVER = 0x6,
		CFSM_HELICOPTER_FINE_AIM = 0x7,
		CFSM_AIRPLANE_DRIVER = 0x8,
		CFSM_ZOOM = 0x9,
		CFSM_SWIMMING = 0xA,
		CFSM_SPECTATOR = 0xB,
		CFSM_FENCE = 0xC,
		CFSM_RAGDOLL = 0xD,
		CFSM_FALLING = 0xE,
		CFSM_LEAPING = 0xF,
		CFSM_FINE_AIM = 0x10,
		CFSM_FINE_AIM_CROUCH = 0x11,
		CFSM_MELEE_LOCK = 0x12,
		CFSM_FINE_AIM_VEHICLE = 0x13,
		CFSM_FREEFALL = 0x14,
		CFSM_PARACHUTE = 0x15,
		CFSM_HUMAN_SHIELD = 0x16,
		CFSM_SPRINT = 0x17,
	};
		BYTE* activeIndex = &onfootIndex;
		camera_free_submodes player_status = *(camera_free_submodes*)0x00E9A5BC;
		switch (player_status) {
		case CFSM_VEHICLE_DRIVER:
		case CFSM_WATERCRAFT_DRIVER:
		case CFSM_HELICOPTER_DRIVER:
		case CFSM_AIRPLANE_DRIVER:
		case CFSM_VEHICLE_DRIVER_ALT: // a what now?
			activeIndex = &inVehicleIndex;
			break;
		case CFSM_FINE_AIM_CROUCH:
		case CFSM_FINE_AIM:
		case CFSM_HELICOPTER_FINE_AIM:
			activeIndex = &fineAimIndex;
			break;
		default:
			activeIndex = &onfootIndex;
			break;
		}
		float target = zoom_values[*activeIndex];
		switch (player_status) {
		case CFSM_VEHICLE_DRIVER:
		case CFSM_WATERCRAFT_DRIVER:
		case CFSM_HELICOPTER_DRIVER:
		case CFSM_AIRPLANE_DRIVER:
		case CFSM_VEHICLE_DRIVER_ALT: // a what now?
			if (target < 0.f)
				target *= 0.8f;
			else
				target *= 0.75f;
			break;
		case CFSM_FINE_AIM_CROUCH:
		case CFSM_FINE_AIM:
			//if(target > 0.f)
			//target *= 0.35f;
			//break;
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
				dest[i] = (*(float*)0x00E9A654) * powf(2.f, EditedZoomMod / 2.0f);
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
	GameConfig::Initialize();
	onfootIndex = GameConfig::GetValue("Index", "onfoot", ZOOM_MID_INDEX);
	inVehicleIndex = GameConfig::GetValue("Index", "inVehicle", ZOOM_MID_INDEX);
	fineAimIndex = GameConfig::GetValue("Index", "fineAim", ZOOM_MID_INDEX);
	patchJmp((void*)0x0049A4CB, HookedRepMovsd);
}
void safeconfig() {
	GameConfig::SetValue("Index", "onfoot", onfootIndex);
	GameConfig::SetValue("Index", "inVehicle", inVehicleIndex);
	GameConfig::SetValue("Index", "fineAim", fineAimIndex);
}
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        setupHook();
        break;
    case DLL_PROCESS_DETACH:
		safeconfig();
		FreeLibraryAndExitThread(hModule, TRUE);
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