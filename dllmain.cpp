// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MinHook.h"
#include <cstdint>
#include <vector>
#include "GameConfig.h"
#pragma comment(lib, "libMinHook.x86.lib")
#include "include\BlingMenu_public.h"
#include <chrono> 

#include <string>
using namespace GameConfig;
struct vector3 {
	float x;
	float y;
	float z;
};
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
bool m_GTASA_heightIncreaseMult_enabled = true;
//BYTE switch_shoulder = false;
bool m_halved_base_pitch = true;
void patchJmp(void* addr, void* func) {
	DWORD oldProtect;

	VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(uint8_t*)addr = 0xE9;
	*(uint32_t*)((uint8_t*)addr + 1) = (uint32_t)func - (uint32_t)addr - 5;
	VirtualProtect(addr, 5, oldProtect, &oldProtect);
}
BYTE havok_pasued() {
	return (*(BYTE*)0x2526D28 || *(BYTE*)0x2527CB6);
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


inline float timestep() {
	return *(float*)0x00E84380;
}

// GTA's timestep does 0.8f at 60fps and 1.f at 30fps, wtf?? this isn't that. but works.
float GTA_timestep() {
	return timestep() / (1.f / 30.f);
}

float DK_timestep() {
	return  (1.f / 30.f) / timestep();
}

typedef uintptr_t(__fastcall* GetPointerT)(uintptr_t VehiclePointer);
GetPointerT GetPointer = (GetPointerT)0x00AE28F0;

typedef int (__thiscall* vehicle_get_speed_in_mphT)(uintptr_t FinalVehiclePointer);
vehicle_get_speed_in_mphT vehicle_get_speed_in_mph = (vehicle_get_speed_in_mphT)0xAA6820;

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
			//printf("pointer 0x%X \n", GetPointer(value));
			//printf("Speed %d \n", vehicle_get_speed_in_mph(GetPointer(value)));
			return GetPointer(value);
		}
	}
	return NULL;
}

// Inlined function - kind of.
// address 0x00AA7340 - actual num_seats, gets called like 3 times but all other instances are inlined??????
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

inline bool getVehicleType(uintptr_t vehicle_pointer, int type) {
	if (vehicle_pointer == NULL)
		return false;
	return vehicle_pointer && *(uintptr_t*)(*(uintptr_t*)(vehicle_pointer + 0x84E4) + 0x9C) == type;
}

// not inlined, address: 0x00AB51B0 for call but cba for an asm caller
bool isBike(uintptr_t vehicle_pointer) {
	return getVehicleType(vehicle_pointer, 1);
}

bool isFlyAble(uintptr_t vehicle_pointer) {
	// 3 for helicopter, 2 airplane? hopefully
	return getVehicleType(vehicle_pointer, 3) || getVehicleType(vehicle_pointer, 2);
}

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
bool canWeSRTTRShoulder(camera_free_submodes status) {
	return !(status == CFSM_FINE_AIM ||
		status == CFSM_FINE_AIM_CROUCH ||
		status == CFSM_HELICOPTER_FINE_AIM ||
		status == CFSM_FINE_AIM_VEHICLE || status == CFSM_HUMAN_SHIELD || status == CFSM_ZOOM || status == CFSM_AIRPLANE_DRIVER || status == CFSM_HELICOPTER_DRIVER || status == CFSM_WATERCRAFT_DRIVER ||
		status == CFSM_VEHICLE_DRIVER_ALT || status == CFSM_VEHICLE_DRIVER);
}
bool isAnyFineAim(camera_free_submodes status) {
	return status == CFSM_FINE_AIM ||
		status == CFSM_FINE_AIM_CROUCH ||
		status == CFSM_HELICOPTER_FINE_AIM ||
		status == CFSM_FINE_AIM_VEHICLE || status == CFSM_HUMAN_SHIELD;
}
float EditedZoomMod = 0.f;
float ShoulderTarget = 1.f;
float zoom_values[] = { 2.0f,-1.5f, -1.f, 0.f, 1.f, 1.5f,2.f };
float heightIncreaseMult = 0.f;
float ShoulderSpeedMult = 6.f;
float ShoulderSRTTR = 0.0f;
char keySwitch = 'B';
char keySwitchShoulder = VK_XBUTTON1;
char keySwitchShoulderLeft = 0x0;
char keySwitchShoulderRight = 0x0;
#define ZOOM_MID_INDEX (sizeof(zoom_values) / sizeof(zoom_values[0]) / 2)
BYTE onfootIndex = ZOOM_MID_INDEX;
BYTE inVehicleIndex = ZOOM_MID_INDEX;
BYTE fineAimIndex = ZOOM_MID_INDEX;
BYTE SRTTR_ShoulderIndex;
volatile bool switch_shoulder;

volatile float debug_SRTT_mult = 1.74f;

static bool bCamShake = true;
static float fShakeIntensity = 0.105000;
static int nShakeStartSpeed = 40; // MPH to start shaking
static float fShakeMaxIntensity = 4.5f; // Max intensity cap
static float fShakeFreqX = 15.0f;
static float fShakeFreqY = 12.3f;
static float fShakeFreqZ = 18.7f;
static float fShakeMultX = 0.110000f;
static float fShakeMultY = 0.100000f;
static float fShakeMultZ = 0.100000f;

static bool bSmoothShake = true; // true = smooth sine waves, false = random jerky
static float x_offset = -0.45f;
static float y_offset = 0.f;
static float z_offset = 0.f;
static bool bGTAIV_vehicle_camera = false;
static float fGTAIV_LerpSpeed = 2.0f;
static float currentXOffset = 0.0f;
static float currentYOffset = 0.0f;
static float currentZOffset = 0.0f;
static bool bSmoothShake_useGTATimestep = false;
static bool bAllowGTAIV_vehicle_camera_for_flyable = false;
static bool bAllowGTAIV_vehicle_camera_for_bike = false;
void GTAIV_Camera_Offset(vector3* look_at_offset) {


	// Determine target offset based on whether we're in a vehicle
	float targetOffset = 0.0f;
	if (bGTAIV_vehicle_camera && FindPlayersVehicle() && (!isFlyAble(FindPlayersVehicle()) || bAllowGTAIV_vehicle_camera_for_flyable) && (!isBike(FindPlayersVehicle()) || bAllowGTAIV_vehicle_camera_for_bike)) {
		targetOffset = x_offset;
	}


	float deltaTime = timestep();
	currentXOffset = currentXOffset + (targetOffset - currentXOffset) * fGTAIV_LerpSpeed * deltaTime;


	look_at_offset->x += currentXOffset;
}

void CameraShake(vector3* look_at_offset) {
	if (!bCamShake || !FindPlayersVehicle() || look_at_offset == NULL)
		return;
	if (isFlyAble(FindPlayersVehicle()))
		return;
	int mph_vehicle = vehicle_get_speed_in_mph(FindPlayersVehicle());
	if (mph_vehicle < nShakeStartSpeed)
		return;
	float intensity = (mph_vehicle - nShakeStartSpeed) / 100.0f;
	intensity = min(intensity, fShakeMaxIntensity);

	float shakeX, shakeY, shakeZ;

	if (bSmoothShake) {
		// Smooth sine wave shake
		static float time = 0.0f;
		if (!bSmoothShake_useGTATimestep)
			time += timestep();
		else time += GTA_timestep();
		if (time > 360.0f) time = 0.0f; // Prevent overflow

		shakeX = sin(time * fShakeFreqX) * intensity * fShakeMultX * fShakeIntensity;
		shakeY = sin(time * fShakeFreqY) * intensity * fShakeMultY * fShakeIntensity;
		shakeZ = cos(time * fShakeFreqZ) * intensity * fShakeMultZ * fShakeIntensity;
	}
	else {
		int shakeRand = rand();
		float shakeOffset = intensity * fShakeIntensity * 0.1f;

		shakeX = shakeOffset * ((shakeRand & 0xF) - 7) * fShakeMultX;
		shakeY = shakeOffset * (((shakeRand & 0xF0) >> 4) - 7) * fShakeMultY;
		shakeZ = shakeOffset * (((shakeRand & 0xF00) >> 8) - 7) * fShakeMultZ;
	}

	look_at_offset->x += shakeX;
	look_at_offset->y += shakeY;
	look_at_offset->z += shakeZ;
}


void cf_lookat_position_process_midhook() {
	UpdateKeys();
	//printf("is Bike: 0x%X \n", isBike(FindPlayersVehicle()));
		BYTE* activeIndex = &onfootIndex;
		camera_free_submodes player_status = *(camera_free_submodes*)0x00E9A5BC;
		float speed = 5.0f;
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
		case CFSM_FINE_AIM_VEHICLE:
			activeIndex = &fineAimIndex;
			//speed *= 6.f;
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
		default:
			break;
		}
		

		float t = speed * timestep();
		if (t > 1.0f) t = 1.0f;
		// Code from SACarCam by erorcun, 
		// I assume this is original GTA:SA logic as it acts like it even here. 
		// https://github.com/erorcun/SACarCam/blob/25a43fcaaf321d9d4969ab6f68fd65fa627ecac1/SACarCam/SACarCam.cpp#L393
		if (m_GTASA_heightIncreaseMult_enabled && player_status == CFSM_VEHICLE_DRIVER && isBike(FindPlayersVehicle())) {
			// This function might be overkill for this as bikes only have 2 seats so we can just check the handle of the second seat? either ways took me too long to figure this passenger out lol
			if (vehicle_get_num_passengers(FindPlayersVehicle()) > 1) {
				if (heightIncreaseMult < 1.0f) {
					heightIncreaseMult = min(1.0f,  (0.02f * GTA_timestep()) + heightIncreaseMult);
				}
			}
			else {
				if (heightIncreaseMult > 0.0f) {
					// SACarCam does 
					// heightIncreaseMult = max(0.0f, heightIncreaseMult - timestep() * 0.02f);
					// that won't work here for some reason, it'll just insta go to 0.f or break the camera.
					heightIncreaseMult = max(0.0f, heightIncreaseMult - (0.02f * GTA_timestep()));
				}
			}
		}
		else
			heightIncreaseMult = 0.f;
		EditedZoomMod = lerp(EditedZoomMod, target, t);
		float shoulderTarget = 0.f;
		if (isAnyFineAim(player_status)) {
			shoulderTarget = switch_shoulder ? -1.f : 1.f;
		}
		else
			shoulderTarget = 0.f;
		float SRTTR_Shoulder_Target = 0.f;{}
		if (canWeSRTTRShoulder(player_status) && !isAnyFineAim(player_status)) {
			if (IsKeyPressed(keySwitchShoulder, false)) {
				SRTTR_ShoulderIndex = (SRTTR_ShoulderIndex + 1) % 3;
			}
			else if (IsKeyPressed(keySwitchShoulderRight, false)) {
				if (SRTTR_ShoulderIndex == 1) {
					SRTTR_ShoulderIndex = 0;
				}
				else {
					SRTTR_ShoulderIndex = 1;
				}
			}
			else if (IsKeyPressed(keySwitchShoulderLeft, false)) {
				if (SRTTR_ShoulderIndex == 2) {
					SRTTR_ShoulderIndex = 0;
				}
				else {
					SRTTR_ShoulderIndex = 2;
				}
			}
		
		

			switch (SRTTR_ShoulderIndex) {
			case 0:
				SRTTR_Shoulder_Target = 0.f;
				break;
			case 1:
				SRTTR_Shoulder_Target = 0.6f;
				break;
			case 2:
				SRTTR_Shoulder_Target = -0.6f;
				break;
			}
		}
 

		float shoulderT = t * ShoulderSpeedMult;
		if (shoulderT > 1.0f) shoulderT = 1.0f;

		float srttrT = t * debug_SRTT_mult;
		if (srttrT > 1.0f) srttrT = 1.0f;

		ShoulderTarget = lerp(ShoulderTarget, shoulderTarget, shoulderT);
		ShoulderSRTTR = lerp(ShoulderSRTTR, SRTTR_Shoulder_Target, srttrT);
		if (IsKeyPressed(keySwitch, false)) {
			*activeIndex = (*activeIndex + 5) % 6;
		}
		else if (isAnyFineAim(player_status) && IsKeyPressed(keySwitchShoulder, false)) {
			switch_shoulder = !switch_shoulder;
		}
		float* src = reinterpret_cast<float*>(0x00E9A638);
		float* dest = reinterpret_cast<float*>(0x00E9A60C);
		size_t size = 0x2C / 4;

		for (size_t i = 0; i < size; ++i) {
			if (reinterpret_cast<uintptr_t>(src) == 0x00E9A654) { // camera_free_base_distance
				dest[i] = (*(float*)0x00E9A654) * powf(2.f, EditedZoomMod / 2.0f);
				*src++;
			}
			else if (m_GTASA_heightIncreaseMult_enabled && reinterpret_cast<uintptr_t>(src) == 0x00E9A63C) { // camera_free_look_at_offset.y \\ in GTA height is Z?
				dest[i] = (*(float*)0x00E9A63C) + (0.4f * heightIncreaseMult);
				*src++;
			}
			else if (reinterpret_cast<uintptr_t>(src) == 0x00E9A660) { // camera_free_x_shift
				*(float*)0x00E9A634 = ((*(float*)0x00E9A660) * ShoulderTarget) + ShoulderSRTTR;
				*src++;
			}
			else if (reinterpret_cast<uintptr_t>(src) == 0x00E9A638) { // camera_free_look_at_offset Vector3
				vector3* originalOffset = (vector3*)0x00E9A638;

				vector3 modifiedOffset = *originalOffset;
				
				CameraShake(&modifiedOffset);
				GTAIV_Camera_Offset(&modifiedOffset);
				*(vector3*)0x00E9A60C = modifiedOffset;

				// Skip the next 2 iterations since we're handling the full Vector3 (3 floats)
				dest[i] = modifiedOffset.x;
				dest[i + 1] = modifiedOffset.y;
				dest[i + 2] = modifiedOffset.z;
				src += 3; // Skip 3 floats
				i += 2;   // We'll increment by 1 more in the loop
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
void cf_submode_params_set_by_lerp_midhook() {
	camera_free_submodes player_status = *(camera_free_submodes*)0x00E9A5BC;
	switch (player_status) {
	case CFSM_VEHICLE_DRIVER:
	case CFSM_WATERCRAFT_DRIVER:
	case CFSM_VEHICLE_DRIVER_ALT: // a what now?
		*(float*)0x00E9A650 /= 2.f;
		break;
	default:
		break;
	}

	
}
void __declspec(naked) cf_submode_params_set_by_lerp_midhookASM() {
	static int jmp_continue = 0x00499AD8;
	__asm {
		fstp dword ptr[edx + 0x18]
		fld dword ptr[ecx + 0x1C]
		pushad
		pushfd
	}

	cf_submode_params_set_by_lerp_midhook();


	__asm {
		popfd
		popad

		jmp jmp_continue
	}
}
__declspec(noinline) void loadKeys() {
	keySwitch = GameConfig::GetValue("Binds", "ChangeCameraZoom", 'B');
	keySwitchShoulder = GameConfig::GetValue("Binds", "ChangeShoulder", VK_XBUTTON1);
	keySwitchShoulderLeft = GameConfig::GetValue("Binds", "ChangeShoulderLeft", 0x0);
	keySwitchShoulderRight = GameConfig::GetValue("Binds", "ChangeShoulderRight", 0x0);
}
void setupHook() {
	if (GameConfig::GetValue("Hooks", "Allow_for_GTASA_heightIncreaseMult", 1)) {
		patchDWord((void*)(0x00499B9F + 1), 0x00E9A60C); // so bike height works.
	}
	else
		m_GTASA_heightIncreaseMult_enabled = false;
#if _DEBUG
	OpenConsole();
#endif
	GameConfig::Initialize();
	onfootIndex = GameConfig::GetValue("Index", "onfoot", ZOOM_MID_INDEX);
	inVehicleIndex = GameConfig::GetValue("Index", "inVehicle", ZOOM_MID_INDEX);
	fineAimIndex = GameConfig::GetValue("Index", "fineAim", ZOOM_MID_INDEX);
	SRTTR_ShoulderIndex = GameConfig::GetValue("Index", "SRTTR_ShoulderIndex", 0);
	switch_shoulder = GameConfig::GetValue("Index", "switch_shoulder", 0);
	debug_SRTT_mult = (float)GameConfig::GetDoubleValue("EXPERIMENTAL", "debug_SRTT_mult_lerptime", 2.5f);
	loadKeys();
	patchJmp((void*)0x0049A4CB, cf_lookat_position_process_midhook_ASM);
	if(GameConfig::GetValue("EXPERIMENTAL","Halve_base_pitch_for_cars",0)) // this is probably in an xtbl somewhere (vehicles_cameras) for each vehicle?, and it acts weird on foot thus EXPERIMENTAL, probably better to change in xtbl
	patchJmp((void*)0x00499AD2, cf_submode_params_set_by_lerp_midhookASM);

	if (GameConfig::GetValue("EXPERIMENTAL", "EnableShoulderSwap", 1)) {
		patchDWord((void*)(0x0049C8EB + 2), 0x00E9A634);
		patchDWord((void*)(0x0049CF45 + 2), 0x00E9A634);
		ShoulderSpeedMult = (float)GameConfig::GetDoubleValue("EXPERIMENTAL", "ShoulderSwapSpeedMult", 6.0f);
	}

}
void safeconfig() {
	GameConfig::SetValue("Index", "onfoot", onfootIndex);
	GameConfig::SetValue("Index", "inVehicle", inVehicleIndex);
	GameConfig::SetValue("Index", "fineAim", fineAimIndex);
	GameConfig::SetValue("Index", "SRTTR_ShoulderIndex", SRTTR_ShoulderIndex);
	GameConfig::SetValue("Index", "switch_shoulder", switch_shoulder);
}

void loadnewconfig() {
	// Basic shake settings
	bCamShake = GetValue("Values", "bCamShake", bCamShake);
	fShakeIntensity = GetDoubleValue("Values", "fShakeIntensity", fShakeIntensity);
	nShakeStartSpeed = GetValue("Values", "nShakeStartSpeed", nShakeStartSpeed);
	fShakeMaxIntensity = GetDoubleValue("Values", "fShakeMaxIntensity", fShakeMaxIntensity);

	// Shake frequency settings
	fShakeFreqX = GetDoubleValue("Values", "fShakeFreqX", fShakeFreqX);
	fShakeFreqY = GetDoubleValue("Values", "fShakeFreqY", fShakeFreqY);
	fShakeFreqZ = GetDoubleValue("Values", "fShakeFreqZ", fShakeFreqZ);

	// Shake multiplier settings
	fShakeMultX = GetDoubleValue("Values", "fShakeMultX", fShakeMultX);
	fShakeMultY = GetDoubleValue("Values", "fShakeMultY", fShakeMultY);
	fShakeMultZ = GetDoubleValue("Values", "fShakeMultZ", fShakeMultZ);

	// Additional settings
	bSmoothShake = GetValue("Values", "bSmoothShake", bSmoothShake);
	bSmoothShake_useGTATimestep = GetValue("Values", "bSmoothShake_useGTATimestep", bSmoothShake_useGTATimestep);
	x_offset = GetDoubleValue("Values", "x_offset", x_offset);
	bGTAIV_vehicle_camera = GetValue("Values", "bGTAIV_vehicle_camera", bGTAIV_vehicle_camera);

	bAllowGTAIV_vehicle_camera_for_flyable = GetValue("Values", "bAllowGTAIV_vehicle_camera_for_flyable", bAllowGTAIV_vehicle_camera_for_flyable);
	bAllowGTAIV_vehicle_camera_for_bike = GetValue("Values", "bAllowGTAIV_vehicle_camera_for_flyable", bAllowGTAIV_vehicle_camera_for_bike);

}

DWORD WINAPI LateBM(LPVOID lpParameter)
{
	// Funny Tervel hooking Sleep, BlingMenu settings get added after 2450ms
	SleepEx(1200, 0);
	BlingMenuAddFunc("ClippyCamera",
#if !_DEBUG
		"version: r4"
#else
		"version: r4 : DEBUG BUILD"
#endif
		,NULL);
	if (GameConfig::GetValue("Hooks", "Allow_for_GTASA_heightIncreaseMult", 1))
	BlingMenuAddBool("ClippyCamera", "GTA:SA bikes cam raise with passenger", &m_GTASA_heightIncreaseMult_enabled, nullptr);
	BlingMenuAddFloat("ClippyCamera", "ShoulderSwapSpeedMult", &ShoulderSpeedMult, NULL, 0.25f, 1.f, 50.f);
	BlingMenuAddFloat("ClippyCamera", "debug_SRTT_mult_lerptime", (float*)&debug_SRTT_mult, NULL, 0.01f, 0.01f, 50.f);
	//BlingMenuAddFunc("ClippyCamera", "Reload binds from config", &loadKeys);

	//BlingMenuAddInt8("ClippyCamera", "Index to mod", (signed char*)&mod_index, &DEBUG_setIndexPointers, 1, 0, sizeof(zoom_values) / sizeof(zoom_values[0]));
	//BlingMenuAddFloat("ClippyCamera", "value", ModifyValues, &DEBUG_setIndexPointers, 0.1f, -5.f, 5.f);
	BlingMenuAddInt8("ClippyCamera", "&inVehicleIndex", (signed char*)&inVehicleIndex, NULL, 1, 0, sizeof(zoom_values) / sizeof(zoom_values[0] - 1));
	BlingMenuAddInt8("ClippyCamera", "&onfootIndex", (signed char*)&onfootIndex, NULL, 1, 0, sizeof(zoom_values) / sizeof(zoom_values[0] - 1));
	BlingMenuAddInt8("ClippyCamera", "&fineAimIndex", (signed char*)&fineAimIndex, NULL, 1, 0, sizeof(zoom_values) / sizeof(zoom_values[0] - 1));
	for (int i = 0; i < (sizeof(zoom_values) / sizeof(zoom_values[0])); i++) {
		std::string whatever;
		whatever = "Index [" + std::to_string(i) + "]";
		BlingMenuAddFloat("ClippyCamera", whatever.c_str(), &zoom_values[i], NULL, 0.1f, -5.f, 5.f);
	}



	BlingMenuAddBool("ClippyCamera", "Enable Camera Shake", &bCamShake, nullptr);
	BlingMenuAddFloat("ClippyCamera", "Shake Intensity", &fShakeIntensity, NULL, 0.01f, 0.0f, 2.5f);
	BlingMenuAddInt("ClippyCamera", "Shake Start Speed (MPH)", &nShakeStartSpeed, NULL, 5, 20, 100);
	BlingMenuAddFloat("ClippyCamera", "Max Shake Intensity", &fShakeMaxIntensity, NULL, 0.01f, 1.0f, 10.0f);

	// Shake frequency settings
	BlingMenuAddFloat("ClippyCamera", "Shake Freq X", &fShakeFreqX, NULL, 0.5f, 5.0f, 50.0f);
	BlingMenuAddFloat("ClippyCamera", "Shake Freq Y", &fShakeFreqY, NULL, 0.5f, 5.0f, 50.0f);
	BlingMenuAddFloat("ClippyCamera", "Shake Freq Z", &fShakeFreqZ, NULL, 0.5f, 5.0f, 50.0f);

	// Shake multiplier settings
	BlingMenuAddFloat("ClippyCamera", "Shake Mult X", &fShakeMultX, NULL, 0.01f, 0.0f, 0.2f);
	BlingMenuAddFloat("ClippyCamera", "Shake Mult Y", &fShakeMultY, NULL, 0.01f, 0.0f, 0.2f);
	BlingMenuAddFloat("ClippyCamera", "Shake Mult Z", &fShakeMultZ, NULL, 0.01f, 0.0f, 0.2f);
	BlingMenuAddBool("ClippyCamera", "Smooth Shake (vs Random)", &bSmoothShake, nullptr);
	BlingMenuAddBool("ClippyCamera", "bSmoothShake_useGTATimestep", &bSmoothShake_useGTATimestep,NULL);
	BlingMenuAddFloat("ClippyCamera", "ShoulderSwapSpeedMult", &ShoulderSpeedMult, NULL, 0.25f, 1.f, 50.f);
	BlingMenuAddFloat("ClippyCamera", "x_offset_car", &x_offset, NULL, 0.05f, -5.f, 5.f);
	BlingMenuAddBool("ClippyCamera", "bGTAIV_vehicle_camera", &bGTAIV_vehicle_camera, NULL);
	BlingMenuAddBool("ClippyCamera", "bAllowGTAIV_vehicle_camera_for_flyable", &bAllowGTAIV_vehicle_camera_for_flyable, NULL);
	BlingMenuAddBool("ClippyCamera", "bAllowGTAIV_vehicle_camera_for_bike", &bAllowGTAIV_vehicle_camera_for_bike, NULL);
	BlingMenuAddFunc("ClippyCamera", "Save (some) to .ini", []() {
		// Basic shake settings
		SetValue("Values", "bCamShake", bCamShake);
		SetDoubleValue("Values", "fShakeIntensity", fShakeIntensity);
		SetValue("Values", "nShakeStartSpeed", nShakeStartSpeed);
		SetDoubleValue("Values", "fShakeMaxIntensity", fShakeMaxIntensity);

		// Shake frequency settings
		SetDoubleValue("Values", "fShakeFreqX", fShakeFreqX);
		SetDoubleValue("Values", "fShakeFreqY", fShakeFreqY);
		SetDoubleValue("Values", "fShakeFreqZ", fShakeFreqZ);

		// Shake multiplier settings
		SetDoubleValue("Values", "fShakeMultX", fShakeMultX);
		SetDoubleValue("Values", "fShakeMultY", fShakeMultY);
		SetDoubleValue("Values", "fShakeMultZ", fShakeMultZ);

		// Additional settings
		SetValue("Values", "bSmoothShake", bSmoothShake);
		SetValue("Values", "bSmoothShake_useGTATimestep", bSmoothShake_useGTATimestep);
		SetDoubleValue("Values", "x_offset", x_offset);
		SetValue("Values", "bGTAIV_vehicle_camera", bGTAIV_vehicle_camera);
		SetValue("Values", "bAllowGTAIV_vehicle_camera_for_flyable", bAllowGTAIV_vehicle_camera_for_flyable);
		SetValue("Values", "bAllowGTAIV_vehicle_camera_for_bike", bAllowGTAIV_vehicle_camera_for_bike);
		});

	BlingMenuAddFunc("ClippyCamera", "Load from .ini", &loadnewconfig);


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
		loadnewconfig();
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