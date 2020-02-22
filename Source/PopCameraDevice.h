#pragma once

//	if you're using this header to link to the DLL, you'll probbaly need the lib :)
//#pragma comment(lib, "PopCameraDevice.lib")

#include <stdint.h>


#if !defined(__export)

#if defined(_MSC_VER) && !defined(TARGET_PS4)
#define __export			extern "C" __declspec(dllexport)
#else
#define __export			extern "C"
#endif

#endif

//	function pointer type for new frame callback
typedef void PopCameraDevice_OnNewFrame(void* Meta);

//	enum every availible device and their availible formats as Json
__export void				PopCameraDevice_EnumCameraDevicesJson(char* StringBuffer,int32_t StringBufferLength);

//	create a new device. Format is optional. Returns instance ID (0 on error)
__export int32_t			PopCameraDevice_CreateCameraDeviceWithFormat(const char* Name,const char* Format, char* ErrorBuffer, int32_t ErrorBufferLength);

__export void				PopCameraDevice_FreeCameraDevice(int32_t Instance);

//	register a callback function when a new frame is ready. This is expected to exist until released
__export void				PopCameraDevice_AddOnNewFrameCallback(int32_t Instance, PopCameraDevice_OnNewFrame* Callback, void* Meta);

//	Next frame's info (size, format, time) as json. Sizes and time will be zero if no new frame
__export void				PopCameraDevice_GetNextFrameMeta(int32_t Instance,char* JsonBuffer,int32_t JsonBufferSize);

//	returns 0 if no new frame (and nothing updated). Otherwise fills buffers and returns timestamp (Where 1=0)
__export int32_t			PopCameraDevice_GetNextFrame(int32_t Instance, uint8_t* Plane0, int32_t Plane0Size, uint8_t* Plane1, int32_t Plane1Size, uint8_t* Plane2, int32_t Plane2Size);

//	returns	version integer as A.BBB.CCCCCC (major, minor, patch. Divide by 10's to split)
__export int32_t			PopCameraDevice_GetVersion();

//	shouldn't need this, but cleans up explicitly
__export void				PopCameraDevice_Cleanup();

