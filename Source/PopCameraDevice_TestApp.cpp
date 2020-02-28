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
	OutputDebugStringA("\n");
#endif
	std::cout << Message.c_str() << std::endl;
}

int main()
{
	DebugPrint("PopCameraDevice_UnitTests");
	PopCameraDevice_UnitTests();

	DebugPrint("PopCameraDevice_EnumCameraDevicesJson");	
	char EnumJson[1024*2];
	PopCameraDevice_EnumCameraDevicesJson(EnumJson, std::size(EnumJson));
	
	//	at least one of these should be the test device
	DebugPrint("Devices and formats:");
	DebugPrint(EnumJson);

	//	create the test device
	//	and grab a frame to test
	DebugPrint("Creating test device");
	auto Format = "RGBA^100x100@30";
	char ErrorBuffer[1024];
	auto Instance = PopCameraDevice_CreateCameraDeviceWithFormat("Test", Format, ErrorBuffer, std::size(ErrorBuffer));
	if (Instance <= 0 )
		throw std::runtime_error( std::string("Device failed to be created; ") + ErrorBuffer);

	//	we assume the test device instantly creates a first frame
	DebugPrint("Peek first frame");
	char MetaJson[1024];
	auto FirstFrameTime = PopCameraDevice_PeekNextFrame(Instance, MetaJson, std::size(MetaJson));
	if (FirstFrameTime < 0)
		throw std::runtime_error("First test frame invalid time");
	DebugPrint("First Frame:");
	DebugPrint(MetaJson);

	//	todo: check meta by regex/parsing the json
	DebugPrint("Pop Frame:");
	uint8_t Plane0[100 * 100 * 4];
	auto FrameTime = PopCameraDevice_PopNextFrame(Instance, nullptr, 0, Plane0, std::size(Plane0), nullptr, 0, nullptr, 0);
	if (FrameTime != FirstFrameTime )
		throw std::runtime_error("Frame time doesn't match first frame time");

	//	todo: verify pixels

	//PopCameraDevice_Cleanup();
	return 0;
}

