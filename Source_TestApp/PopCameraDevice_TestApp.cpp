#include <iostream>
#include <thread>
#include <sstream>
#include "../Source/PopCameraDevice.h"
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


void TestDeviceInstance(const std::string& Name,const std::string& OptionsJson,size_t GrabFrameCount)
{
	//	create the test device
	//	and grab a frame to test
	DebugPrint("Creating test device");
	DebugPrint(Name);
	DebugPrint(OptionsJson);
	
	char ErrorBuffer[1024] = {};
	int Instance = PopCameraDevice_CreateCameraDevice(Name.c_str(), OptionsJson.c_str(), ErrorBuffer, std::size(ErrorBuffer));
	if (Instance <= 0)
		throw std::runtime_error(std::string("Device failed to be created; ") + ErrorBuffer);

	//	test callback
	auto OnNewFrame = [](void* Meta)
	{
	/*
		auto* pInstance = (int*)Meta;
		DebugPrint("New frame callback");
		char MetaJson[1024];
		auto NextFrame = PopCameraDevice_PeekNextFrame(*pInstance, MetaJson, std::size(MetaJson));
		DebugPrint(std::string("New frame meta (") + std::to_string(NextFrame) + "): ");
		DebugPrint(MetaJson);
		*/
	};
	PopCameraDevice_AddOnNewFrameCallback(Instance,OnNewFrame,&Instance);


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
		char MetaJson[1024*10];
		auto FrameTime = PopCameraDevice_PopNextFrame(Instance, MetaJson, std::size(MetaJson), Plane0, std::size(Plane0), nullptr, 0, nullptr, 0);
		if (FrameTime == -1)
		{
			//DebugPrint("No frame, waiting...");
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			//i--;
			continue;
		}
		/*
		if ( i==0)
			if (FrameTime != FirstFrameTime && FirstFrameTime != -1)
				throw std::runtime_error("Frame time doesn't match first frame time");
		*/
	
		std::stringstream Debug;
		Debug << "Got frame " << FrameTime << "(" << static_cast<uint32_t>(FrameTime) << ") (first=" << FirstFrameTime << ") Meta=" << MetaJson;
		//	todo: verify pixels
		DebugPrint(Debug.str());
	}

	PopCameraDevice_FreeCameraDevice(Instance);
}



int main()
{
	DebugPrint("PopCameraDevice_UnitTests");
	PopCameraDevice_UnitTests();

	DebugPrint("PopCameraDevice_EnumCameraDevicesJson");	
	char EnumJson[1024*10];
	PopCameraDevice_EnumCameraDevicesJson(EnumJson, std::size(EnumJson));
	
	//	at least one of these should be the test device
	DebugPrint("Devices and formats:");
	DebugPrint(EnumJson);

	//	test device currently only pumps out one frame
	//ReadFrameToPng("Test", "{\"Format\":\"RGBA\"}", "Test.png");
	TestDeviceInstance("Test", "{\"SphereZ\":2}", 1);
		
	auto TestFrameCount = 20000;
	TestDeviceInstance("Freenect:A22595W00862214A_Depth", "{\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	//TestDeviceInstance("Front TrueDepth Camera", "{\"Format\":\"Depth16mm\"}", TestFrameCount);
	//TestDeviceInstance("Front TrueDepth Camera", "{\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	//TestDeviceInstance("Front Camera", "{\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	TestDeviceInstance("Arkit Rear Depth", "{\"BodyTracking\":false,\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\",\"DepthSmooth\":true,\"DepthConfidence\":true}", TestFrameCount);
	TestDeviceInstance("Arkit Rear Depth", "{\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	TestDeviceInstance("KinectAzure_000023201312", "{\"BodyTracking\":false,\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	
	
	for ( auto i=0;	i<1000; i++ )
		TestDeviceInstance("KinectAzure_000396300112", "{\"Format\":\"Yuv_8_88\",\"SplitPlanes\":false,\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	//TestDeviceInstance("Front TrueDepth Camera", "{\"Format\":\"Yuv_8_88\",\"SplitPlanes\":false,\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	return 0;
	TestDeviceInstance("Back Camera", "{\"Format\":\"avc1\"}", TestFrameCount);
	TestDeviceInstance("Back Camera", "{\"Format\":\"Yuv_8_88\"}", TestFrameCount);
	TestDeviceInstance("Front TrueDepth Camera", "{\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	TestDeviceInstance("FaceTime HD Camera (Built-in)", "{\"Format\":\"avc1\"}", TestFrameCount);
	TestDeviceInstance("FaceTime HD Camera (Built-in)", "{\"Format\":\"Uvy_8_88\"}", TestFrameCount);
	TestDeviceInstance("FaceTime HD Camera (Built-in)", "{\"Format\":\"Yuv_8_8_8\"}", TestFrameCount);
	
	TestDeviceInstance("KinectAzure_000396300112", "{\"Format\":\"Depth16mm\"}", TestFrameCount);
	TestDeviceInstance("KinectAzure_000396300112", "{\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	TestDeviceInstance("KinectAzure_000396300112", "{\"Format\":\"Yuv_8_88\"}", TestFrameCount);
	TestDeviceInstance("KinectAzure_000396300112", "{\"Format\":\"Yuv_8_88\",\"DepthFormat\":\"Depth16mm\"}", TestFrameCount);
	//TestDeviceInstance("KinectAzure_000396300112", "BGRA_Depth16^2560x1440@30", 4);
	//TestDeviceInstance("KinectAzure_000396300112", "BGRA_Depth16^2560x1440@30", 4);
	//TestDeviceInstance("KinectAzure_000396300112","BGRA_Depth16^2560x1440@30", 4);
	
	//PopCameraDevice_Cleanup();
	return 0;
}

