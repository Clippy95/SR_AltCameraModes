

#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <windows.h>
#include <ctime>

#include <cstdint>

#if !RELOADED
char ini_name[] = "SR2.Activities.Mod.ini";
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
		// Create a buffer with the INI path
		GetCurrentDirectoryA(MAX_PATH, inipath);
		strcat_s(inipath, MAX_PATH, "\\");
		strcat_s(inipath, MAX_PATH, ini_name);
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
		//Logger::TypedLog(CHN_DLL, "trying to get value\n");
		//Logger::TypedLog(CHN_DLL, "%s\n", appName);
		//Logger::TypedLog(CHN_DLL, "%s", keyName);
		//Logger::TypedLog(CHN_DLL, "%s", def);
		//Logger::TypedLog(CHN_DLL, "%s", inipath);
		return GetPrivateProfileIntA(appName, keyName, def, inipath);
	}

	void SetDoubleValue(const char* appName, const char* keyName, double new_value)
	{
		char new_string[12];

		sprintf_s(new_string, "%f", new_value);
		WritePrivateProfileStringA(appName, keyName, new_string, inipath);
	}
	//---------------------------------
	// Set a value from the INI
	//---------------------------------

	void SetValue(const char* appName, const char* keyName, uint32_t new_value)
	{
		char new_string[12];

		sprintf_s(new_string, "%d", new_value);
		WritePrivateProfileStringA(appName, keyName, new_string, inipath);
	}

	//---------------------------------
	// Get a string value from the INI
	//---------------------------------

	void GetStringValue(const char* appName, const char* keyName, const char* def, char* buffer)
	{
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
			int32_t final_value;
			int32_t string_value = atoi(returned);
			return string_value;
		}

		return def;
	}

	double GetDoubleValue(const char* appName, const char* keyName, double def)
	{
		char returned[32];

		GetStringValue(appName, keyName, "", returned);

		if (strlen(returned))
		{
			double final_value;
			double string_value = atof(returned);
			return string_value;
		}

		return def;
	}

	//---------------------------------
	// Set a string value from the INI
	//---------------------------------

	void SetStringValue(const char* appName, const char* keyName, char* buffer)
	{
		WritePrivateProfileStringA(appName, keyName, buffer, inipath);
	}

}
