#include <iostream>
#include "PopCameraDevice.h"
#include "SoyDebug.h"
#include "HeapArray.hpp"
#include "SoyString.h"

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
	
	//	todo: alloc each one, then free it
	return 0;
}
