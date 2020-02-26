#include <iostream>
#include "PopCameraDevice.h"
#pragma comment(lib, "PopCameraDevice.lib")

#if defined(_MSC_VER)
#define TARGET_WINDOWS
#endif

#if defined(TARGET_WINDOWS)
#include <Windows.h>
#endif

void DebugPrint(const std::string& Message)
{
#if defined(TARGET_WINDOWS)
	OutputDebugStringA(Message.c_str());
#endif
	std::cout << Message.c_str() << std::endl;
}

int main()
{
	DebugPrint("PopCameraDevice_EnumCameraDevicesJson");	
	char EnumJson[1024*2];
	PopCameraDevice_EnumCameraDevicesJson(EnumJson, std::size(EnumJson));


	DebugPrint("Devices and formats:");
	DebugPrint(EnumJson);

	PopCameraDevice_Cleanup();
	return 0;
}

