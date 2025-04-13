// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MinHook.h"
#include <cstdint>
#include <vector>

#pragma comment(lib, "libMinHook.x86.lib")

#include <chrono> 


static float EditedCameraPosition = 0.f;
static std::chrono::steady_clock::time_point TimerStart; // Start time of the timer
static const std::chrono::seconds InterpolationDuration(2); // Interpolation duration (2 seconds)
static bool IsTimerActive = false; // Flag to indicate if the timer is active

static int onfootIndex = 2; // Default index in the array (0 position)
static int inVehicleIndex = 2;
static int fineAimIndex = 2;
static const std::vector<float> cameraAdjustments = { -2.f, -1.f, 0.f, 1.f, 2.f };

float getDeltaTime() {
    static DWORD lastTime = GetTickCount64();
    DWORD currentTime = GetTickCount64();
    DWORD elapsedMillis = currentTime - lastTime;
    lastTime = currentTime;
    return elapsedMillis / 1000.0f;
}

bool inline isplayerstatus(uint8_t status) {
    return (*(BYTE*)0x00E9A5BC == status);
}

uint8_t inline static playerstatus() {
    return *(BYTE*)0x00E9A5BC;
}

enum playerstatus {
    vehicle = 3,
    boat = 5,
    helicopter = 6,
    plane = 9,
    sniper = 9,
    fineaimcrouch = 16,
    fineaim = 17,
};

float OriginalCameraPosition;

bool IsInterpolationActive() {
    if (!IsTimerActive)
        return false;

    auto elapsed = std::chrono::steady_clock::now() - TimerStart;
    if (elapsed >= InterpolationDuration) {
        IsTimerActive = false;
        return false;
    }
    return true;
}

void ActivateInterpolationTimer() {
    TimerStart = std::chrono::steady_clock::now();
    IsTimerActive = true;
}

void EditCameraPosition(float newzoom) {
    OriginalCameraPosition = *(float*)0x00E9A654;

    if (OriginalCameraPosition <= 1.f) {
        EditedCameraPosition = OriginalCameraPosition;
        return;
    }

    if (IsInterpolationActive()) {
        float targetPosition = OriginalCameraPosition + newzoom;
        float smoothingfactor = 5.f;

        if (isplayerstatus(playerstatus::fineaimcrouch) || isplayerstatus(playerstatus::fineaim)) {
            if (newzoom != 0)
                targetPosition = OriginalCameraPosition + (newzoom / 5.f);
            smoothingfactor *= 2.f;
        }

        EditedCameraPosition += (targetPosition - EditedCameraPosition) * (getDeltaTime() * smoothingfactor);
    }
    else {
        EditedCameraPosition = OriginalCameraPosition + newzoom;
    }
}

void HandleKeyPress() {
    static bool keyPressed = false;
    uint8_t status = playerstatus();
    int* activeIndex = nullptr;

    switch (status) {
    case playerstatus::fineaimcrouch:
    case playerstatus::fineaim:
        activeIndex = &fineAimIndex;
        break;
    case playerstatus::vehicle:
    case playerstatus::boat:
    case playerstatus::helicopter:
    case playerstatus::plane:
        activeIndex = &inVehicleIndex;
        break;
    default:
        activeIndex = &onfootIndex;
        break;
    }

    if (*(uint8_t*)0x252A406) {
        if (GetAsyncKeyState('B') & 0x8000) {
            if (!keyPressed) {
                keyPressed = true;

                *activeIndex = (*activeIndex + 1) % cameraAdjustments.size();
                *(bool*)(0x252740E) = 1;

                ActivateInterpolationTimer();
            }
        }
        else {
            keyPressed = false;
        }
    }

    EditCameraPosition(cameraAdjustments[*activeIndex]);
}

void PerformCustomMemoryCopy() {
    HandleKeyPress();
    uint8_t* src = reinterpret_cast<uint8_t*>(0x00E9A638);
    uint8_t* dest = reinterpret_cast<uint8_t*>(0x00E9A60C);
    size_t size = 0x2C;

    for (size_t i = 0; i < size; ++i) {
        if (reinterpret_cast<uintptr_t>(src) == 0x00E9A654 && OriginalCameraPosition > 1.f) {
            src = reinterpret_cast<uint8_t*>(&EditedCameraPosition);
        }
        dest[i] = *src++;
    }
}

void __declspec(naked) HookedRepMovsd() {
    __asm {
        pushad
        pushfd
    }

    __asm {
        call PerformCustomMemoryCopy
    }

    __asm {
        popfd
        popad

        mov eax, 0x0049A4DC
        jmp eax
    }
}

typedef void(__stdcall* RepMovsdFunc)();
RepMovsdFunc originalRepMovsd = nullptr;

void setupHook() {
    if (MH_Initialize() != MH_OK) {
        MessageBoxW(NULL, L"FAILED TO INITIALIZE", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    if (MH_CreateHook((LPVOID)0x0049A4D0, &HookedRepMovsd, (LPVOID*)&originalRepMovsd) != MH_OK) {
        MessageBoxW(NULL, L"FAILED TO HOOK", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MessageBoxW(NULL, L"FAILED TO ENABLE", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
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