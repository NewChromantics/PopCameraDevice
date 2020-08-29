#pragma once

//	if you're using this header to link to the DLL, you'll probbaly need the lib
//#pragma comment(lib, "PopCameraDevice.lib")

#include <stdint.h>


#if !defined(__export)

#if defined(_MSC_VER) && !defined(TARGET_PS4)	//	_MSC_VER = visual studio
#define __export			extern "C" __declspec(dllexport)
#else
#define __export			extern "C"
#endif

#endif

//	Version history (PopCameraDevice_GetVersion)
//
//	2.0.1	Added PopCameraDevice_ReadNativeHandle()
//	2.1.4	Added KinectAzure meta, now propogating properly out
//	2.1.7	PopCameraDevice_CreateCameraDevice is now the prefered instantiator with JSON options

#define POPCAMERADEVICE_KEY_SKIPFRAMES	"SkipFrames"
#define POPCAMERADEVICE_KEY_FRAMERATE	"FrameRate"
//	todo: change this format to allow an array instead of explicit "depth"
#define POPCAMERADEVICE_KEY_FORMAT		"Format"
#define POPCAMERADEVICE_KEY_DEPTHFORMAT	"DepthFormat"
#define POPCAMERADEVICE_KEY_DEBUG		"Debug"		//	extra verbose debug output

//	function pointer type for new frame callback
typedef void PopCameraDevice_OnNewFrame(void* Meta);

//	enum every availible device and their availible formats as Json
__export void				PopCameraDevice_EnumCameraDevicesJson(char* StringBuffer,int32_t StringBufferLength);

//	create a new device. Format is optional. Returns instance ID (0 on error)
__export int32_t			PopCameraDevice_CreateCameraDeviceWithFormat(const char* Name,const char* Format, char* ErrorBuffer, int32_t ErrorBufferLength);
__export int32_t			PopCameraDevice_CreateCameraDevice(const char* Name,const char* OptionsJson, char* ErrorBuffer, int32_t ErrorBufferLength);

__export void				PopCameraDevice_FreeCameraDevice(int32_t Instance);

//	register a callback function when a new frame is ready. This is expected to exist until released
__export void				PopCameraDevice_AddOnNewFrameCallback(int32_t Instance, PopCameraDevice_OnNewFrame* Callback, void* Meta);

//	returns -1 if no new frame
//	Fills in meta with JSON about frame
//	Next frame is not deleted. 
//	Meta will list other frames buffered up, so to skip to latest frame, Pop without buffers
__export int32_t			PopCameraDevice_PeekNextFrame(int32_t Instance,char* MetaJsonBuffer,int32_t MetaJsonBufferSize);

//	returns -1 if no new frame
//	Deletes frame.
__export int32_t			PopCameraDevice_PopNextFrame(int32_t Instance, char* MetaJsonBuffer, int32_t MetaJsonBufferSize, uint8_t* Plane0, int32_t Plane0Size, uint8_t* Plane1, int32_t Plane1Size, uint8_t* Plane2, int32_t Plane2Size);

//	returns	version integer as A.BBB.CCCCCC (major, minor, patch. Divide by 10's to split)
__export int32_t			PopCameraDevice_GetVersion();

//	shouldn't need this, but cleans up explicitly
__export void				PopCameraDevice_Cleanup();

//	for internal building really
__export void				PopCameraDevice_UnitTests();

//	for ARFrameProxy, you can pass in an ARFrame pointer, to read depth images immediately for queuing up
__export void				PopCameraDevice_ReadNativeHandle(int32_t Instance,void* Handle);

