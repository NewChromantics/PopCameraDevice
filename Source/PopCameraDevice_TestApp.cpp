#include <iostream>
#include <thread>
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


void TestDeviceInstance(const std::string& Name,const std::string& Format,size_t GrabFrameCount)
{
	//	create the test device
	//	and grab a frame to test
	DebugPrint("Creating test device");
	DebugPrint(Name);
	DebugPrint(Format);
	
	char ErrorBuffer[1024];
	auto Instance = PopCameraDevice_CreateCameraDeviceWithFormat(Name.c_str(), Format.c_str(), ErrorBuffer, std::size(ErrorBuffer));
	if (Instance <= 0)
		throw std::runtime_error(std::string("Device failed to be created; ") + ErrorBuffer);

	//	we assume the test device instantly creates a first frame
	int32_t FirstFrameTime = -1;
	if (Name == "Test")
	{
		DebugPrint("Peek first frame");
		char MetaJson[1024];
		FirstFrameTime = PopCameraDevice_PeekNextFrame(Instance, MetaJson, std::size(MetaJson));
		if (FirstFrameTime < 0)
			throw std::runtime_error("First test frame invalid time");
		DebugPrint("First Frame:");
		DebugPrint(MetaJson);
	}

	//	todo: check meta by regex/parsing the json
	for (auto i = 0; i < GrabFrameCount; i++)
	{
		//DebugPrint("Pop Frame:");
		uint8_t Plane0[100 * 100 * 4];
		auto FrameTime = PopCameraDevice_PopNextFrame(Instance, nullptr, 0, Plane0, std::size(Plane0), nullptr, 0, nullptr, 0);
		if (FrameTime == -1)
		{
			//DebugPrint("No frame, waiting...");
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			i--;
			continue;
		}
		if ( i==0)
			if (FrameTime != FirstFrameTime && FirstFrameTime != -1)
				throw std::runtime_error("Frame time doesn't match first frame time");
		//	todo: verify pixels
	}

	PopCameraDevice_FreeCameraDevice(Instance);
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

	//	test device currently only pumps out one frame
	TestDeviceInstance("Test", "RGBA^100x100@30", 1);
	TestDeviceInstance("KinectAzure_000396300112", "BGRA_Depth16^2560x1440@30", 4);
	TestDeviceInstance("KinectAzure_000396300112", "BGRA_Depth16^2560x1440@30", 4);
	TestDeviceInstance("KinectAzure_000396300112", "BGRA_Depth16^2560x1440@30", 4);
	TestDeviceInstance("KinectAzure_000396300112","BGRA_Depth16^2560x1440@30", 4);
	
	//PopCameraDevice_Cleanup();
	return 0;
}

