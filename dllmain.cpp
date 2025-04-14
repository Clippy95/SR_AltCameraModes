// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MinHook.h"
#include <cstdint>
#include <vector>
#include "GameConfig.h"
#pragma comment(lib, "libMinHook.x86.lib")
#include "include\BlingMenu_public.h"
#include <chrono> 
#if _DEBUG
#include <string>
#endif
bool m_GTASA_heightIncreaseMult_enabled = true;
void patchJmp(void* addr, void* func) {
	DWORD oldProtect;

	VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(uint8_t*)addr = 0xE9;
	*(uint32_t*)((uint8_t*)addr + 1) = (uint32_t)func - (uint32_t)addr - 5;
	VirtualProtect(addr, 5, oldProtect, &oldProtect);
}

void patchDWord(void* addr, uint32_t val) {
	DWORD oldProtect;

	VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(uint32_t*)addr = val;
	VirtualProtect(addr, 1, oldProtect, &oldProtect);
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

// GTA's timestep does 0.8f at 60fps and 1.f at 30fps, wtf?? this isn't that. but works.
float timestep() {
	float frametime = *(float*)0x00E84380;
	return frametime / (1.f / 30.f);
}

typedef uintptr_t(__fastcall* GetPointerT)(uintptr_t VehiclePointer);
GetPointerT GetPointer = (GetPointerT)0x00AE28F0;

uintptr_t FindPlayer() {
	return *(uintptr_t*)(0x21703D4);
}

uintptr_t FindPlayersVehicle() {
	if (!FindPlayer())
		return NULL;
	auto players_vehicle_handle = FindPlayer() + 0xD74;
	if (players_vehicle_handle) {
		uintptr_t value = *(uintptr_t*)players_vehicle_handle;
		if (value) {
			return GetPointer(value);
		}
	}
	return NULL;
}

// Inlined function.
int vehicle_get_num_seats(uintptr_t vehicle_pointer) {
	if (!vehicle_pointer)
		return 0;
	if (*(bool*)(vehicle_pointer + 0x84D8)) // vp->m_srdi.m_loaded
		return *(int*)(vehicle_pointer + 0x6CE8); // p_m_srdi->m_data.m_num_seats

	return 8;
}
// what no symbols does to a mf, this probably isn't an inlined function but I couldn't find it, so let's recreate it.
int vehicle_get_num_passengers(uintptr_t vehicle_pointer) {
	if (!vehicle_pointer)
		return 0;
	int num_passengers = 0;
	for (int i = 0; i < vehicle_get_num_seats(vehicle_pointer); ++i) {
		int character_handle = *(int*)(vehicle_pointer + 0x8ABC + (i * 0x24));
		if (character_handle)
			++num_passengers;
	}
	return num_passengers;

}
// not inlined, address: 0x	w00AB51B0 for call but cba for an asm caller
bool isBike(uintptr_t vehicle_pointer) {
	return vehicle_pointer && *(uintptr_t*)(*(uintptr_t*)(vehicle_pointer + 0x84E4) + 0x9C) == 1;
}

float EditedZoomMod = 0.f;
float zoom_values[] = { -2.f, -1.f, 0.f, 1.f, 2.f };
float heightIncreaseMult = 0.f;
char keySwitch = 'B';
#define ZOOM_MID_INDEX (sizeof(zoom_values) / sizeof(zoom_values[0]) / 2)
BYTE onfootIndex = ZOOM_MID_INDEX;
BYTE inVehicleIndex = ZOOM_MID_INDEX;
BYTE fineAimIndex = ZOOM_MID_INDEX;
void cf_lookat_position_process_midhook() {
	UpdateKeys();
	//printf("is Bike: 0x%X \n", isBike(FindPlayersVehicle()));
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
		if (m_GTASA_heightIncreaseMult_enabled && player_status == CFSM_VEHICLE_DRIVER && isBike(FindPlayersVehicle())) {
			if (vehicle_get_num_passengers(FindPlayersVehicle()) > 1) {
				if (heightIncreaseMult < 1.0f) {
					heightIncreaseMult = min(1.0f,  (0.02f * timestep()) + heightIncreaseMult);
				}
			}
			else {
				if (heightIncreaseMult > 0.0f) {
					heightIncreaseMult = max(0.0f, heightIncreaseMult - (0.02f * timestep()));
				}
			}
		}
		else
			heightIncreaseMult = 0.f;
		EditedZoomMod = lerp(EditedZoomMod, target, t);
		if (IsKeyPressed(keySwitch, false)) {
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
			else if (m_GTASA_heightIncreaseMult_enabled && reinterpret_cast<uintptr_t>(src) == 0x00E9A63C) {
				dest[i] = (*(float*)0x00E9A63C) + (0.4f * heightIncreaseMult);
				*src++;
			}
			else
				dest[i] = *src++;
		}
	}

void __declspec(naked) cf_lookat_position_process_midhook_ASM() {
    __asm {
        pushad
        pushfd
    }

	cf_lookat_position_process_midhook();
    

    __asm {
        popfd
        popad

        mov eax, 0x0049A4DC
        jmp eax
    }
}
#if _DEBUG
void OpenConsole()
{

	AllocConsole();

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	SetConsoleTitleA("Debug Console");

}

#endif
__declspec(noinline) void loadKeys() {
	keySwitch = GameConfig::GetValue("Binds", "ChangeCameraZoom", 'B');
}
void setupHook() {
	patchDWord((void*)(0x00499B9F + 1), 0x00E9A60C);
	OpenConsole();
	GameConfig::Initialize();
	onfootIndex = GameConfig::GetValue("Index", "onfoot", ZOOM_MID_INDEX);
	inVehicleIndex = GameConfig::GetValue("Index", "inVehicle", ZOOM_MID_INDEX);
	fineAimIndex = GameConfig::GetValue("Index", "fineAim", ZOOM_MID_INDEX);
	loadKeys();
	patchJmp((void*)0x0049A4CB, cf_lookat_position_process_midhook_ASM);
}
void safeconfig() {
	GameConfig::SetValue("Index", "onfoot", onfootIndex);
	GameConfig::SetValue("Index", "inVehicle", inVehicleIndex);
	GameConfig::SetValue("Index", "fineAim", fineAimIndex);
}

DWORD WINAPI LateBM(LPVOID lpParameter)
{
	// Funny Tervel hooking Sleep, BlingMenu settings get added after 2450ms
	SleepEx(1499, 0);
	BlingMenuAddFunc("ClippyCamera",
#if !_DEBUG
		"version: r3"
#else
		"version: r3 : DEBUG BUILD"
#endif
		,NULL);
	BlingMenuAddBool("ClippyCamera", "GTA:SA bikes cam raise with passenger", &m_GTASA_heightIncreaseMult_enabled, nullptr);
	//BlingMenuAddFunc("ClippyCamera", "Reload binds from config", &loadKeys);
#if _DEBUG
	//BlingMenuAddInt8("ClippyCamera", "Index to mod", (signed char*)&mod_index, &DEBUG_setIndexPointers, 1, 0, sizeof(zoom_values) / sizeof(zoom_values[0]));
	//BlingMenuAddFloat("ClippyCamera", "value", ModifyValues, &DEBUG_setIndexPointers, 0.1f, -5.f, 5.f);
	for (int i = 0; i < (sizeof(zoom_values) / sizeof(zoom_values[0])); i++) {
		std::string whatever;
		whatever = "Index [" + std::to_string(i) + "]";
		BlingMenuAddFloat("ClippyCamera", whatever.c_str(), &zoom_values[i], NULL, 0.1f, -5.f, 5.f);
	}
#endif
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        setupHook();
		if (BlingMenuLoad()) {
			CreateThread(0, 0, LateBM, 0, 0, 0);
		}
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