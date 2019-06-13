#include <iostream>
#include "PopCameraDevice.h"
#include "SoyDebug.h"
#include "HeapArray.hpp"
#include "SoyString.h"
#include <thread>



int main(int argc, const char * argv[])
{
	//	get devices
	std::Debug << "PopCameraDevice_EnumCameraDevices()..." << std::endl;
	char DeviceNameBuffer[1024*10];
	PopCameraDevice_EnumCameraDevices( DeviceNameBuffer, sizeof(DeviceNameBuffer) );
	Array<std::string> DeviceNames;
	auto Delin = std::string() + DeviceNameBuffer[0];
	Soy::StringSplitByString( GetArrayBridge(DeviceNames), DeviceNameBuffer, Delin, false );
	
	//std::Debug << "Devices: " << DeviceNameBuffer << std::endl;
	std::Debug << "Found x" << DeviceNames.GetSize() << ": " << std::endl;
	for ( auto d=0;	d<DeviceNames.GetSize();	d++ )
	{
		std::Debug << DeviceNames[d] << std::endl;
	}
	
	auto Name = "Freenect:0000000000000000_Depth";
	//auto Name = "Freenect:A22595W00862214A_Depth";
	//auto Name = "Freenect:A22595W00862214A_Colour";
	
	//auto Name = "Kinect2:Default_Depth";
	auto KinectDevice = PopCameraDevice_CreateCameraDevice(Name);

	//	get 10 frames
	auto FrameCount = 0;
	while ( FrameCount < 10 )
	{
		char Meta[1000];
		auto Result = PopCameraDevice_PopFrameAndMeta(KinectDevice, nullptr, 0, nullptr, 0, nullptr, 0, Meta, sizeof(Meta) );
		if ( Result > 0 )
		{
			std::Debug << "Got frame, meta=" << Meta << std::endl;
			FrameCount++;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	PopCameraDevice_FreeCameraDevice(KinectDevice);

	//	todo: alloc each one, then free it
	return 0;
}
