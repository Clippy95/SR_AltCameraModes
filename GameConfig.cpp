#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <windows.h>
#include <ctime>
#include <cstdint>
#include "GameConfig.h"
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")
extern "C" IMAGE_DOS_HEADER __ImageBase;

#if !RELOADED
char ini_name[] = "SR_AltCameraModes.ini";
#else
char ini_name[] = "reloaded.ini";
#endif
namespace GameConfig
{
	static char inipath[MAX_PATH];
	//---------------------------------
	// Set up INI path
	//---------------------------------
	void Initialize()
	{
		char modulePath[MAX_PATH];
		GetModuleFileNameA((HMODULE)&__ImageBase, modulePath, MAX_PATH);

		// Strip the filename to get the directory
		PathRemoveFileSpecA(modulePath);

		// Append the ini name
		snprintf(inipath, MAX_PATH, "%s\\%s", modulePath, ini_name);

		// Check if the INI file exists, create it if it doesn't
		DWORD fileAttrib = GetFileAttributesA(inipath);
		if (fileAttrib == INVALID_FILE_ATTRIBUTES)
		{
			HANDLE hFile = CreateFileA(inipath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFile);
			}
		}
	}

	//---------------------------------
	// Get INI path
	//---------------------------------
	char* GetINIPath() { return (char*)inipath; }
	//---------------------------------
	// Get a value from the INI
	//---------------------------------
	uint32_t GetValue(const char* appName, const char* keyName, uint32_t def)
	{
		// Check if the value exists
		char tempBuffer[32];
		DWORD result = GetPrivateProfileStringA(appName, keyName, "", tempBuffer, sizeof(tempBuffer), inipath);

		// If the value doesn't exist, create it with the default value
		if (result == 0)
		{
			SetValue(appName, keyName, def);
		}

		return GetPrivateProfileIntA(appName, keyName, def, inipath);
	}

	void SetDoubleValue(const char* appName, const char* keyName, double new_value)
	{
		char new_string[32];
		sprintf_s(new_string, "%f", new_value);
		WritePrivateProfileStringA(appName, keyName, new_string, inipath);
	}
	//---------------------------------
	// Set a value from the INI
	//---------------------------------
	void SetValue(const char* appName, const char* keyName, uint32_t new_value)
	{
		char new_string[32];
		sprintf_s(new_string, "%d", new_value);
		WritePrivateProfileStringA(appName, keyName, new_string, inipath);
	}
	//---------------------------------
	// Get a string value from the INI
	//---------------------------------
	void GetStringValue(const char* appName, const char* keyName, const char* def, char* buffer)
	{
		// Check if the value exists
		char tempBuffer[MAX_PATH];
		DWORD result = GetPrivateProfileStringA(appName, keyName, "", tempBuffer, MAX_PATH, inipath);

		// If the value doesn't exist, create it with the default value
		if (result == 0 && def && strlen(def) > 0)
		{
			SetStringValue(appName, keyName, (char*)def);
		}

		GetPrivateProfileStringA(appName, keyName, def, buffer, MAX_PATH, inipath);
	}
	//---------------------------------
	// Get a signed value from the INI
	//---------------------------------
	int32_t GetSignedValue(const char* appName, const char* keyName, int32_t def)
	{
		char returned[32];
		GetStringValue(appName, keyName, "", returned);
		if (strlen(returned))
		{
			int32_t string_value = atoi(returned);
			return string_value;
		}
		else
		{
			// Value doesn't exist, create it with default
			char defStr[32];
			sprintf_s(defStr, "%d", def);
			SetStringValue(appName, keyName, defStr);
			return def;
		}
	}

	double GetDoubleValue(const char* appName, const char* keyName, double def)
	{
		char returned[32];
		GetStringValue(appName, keyName, "", returned);
		if (strlen(returned))
		{
			double string_value = atof(returned);
			return string_value;
		}
		else
		{
			// Value doesn't exist, create it with default
			SetDoubleValue(appName, keyName, def);
			return def;
		}
	}
	//---------------------------------
	// Set a string value from the INI
	//---------------------------------
	void SetStringValue(const char* appName, const char* keyName, char* buffer)
	{
		WritePrivateProfileStringA(appName, keyName, buffer, inipath);
	}

}