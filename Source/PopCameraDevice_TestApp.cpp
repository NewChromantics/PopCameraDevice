#include <iostream>
#include "PopCameraDevice.h"
#pragma comment(lib, "PopCameraDevice.lib")


int main()
{
	std::cout << "Hello World!\n";
	
	char EnumJson[1024];
	PopCameraDevice_EnumCameraDevicesJson(EnumJson, std::size(EnumJson));

	std::cout << "Devices and formats:" << EnumJson << std::endl;
	
	PopCameraDevice_Cleanup();
	return 0;
}

