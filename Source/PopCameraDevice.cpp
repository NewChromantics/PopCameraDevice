#include "PopCameraDevice.h"
#include <exception>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>
#include <HeapArray.hpp>
#include "TestDevice.h"
#include <SoyMedia.h>




namespace PopCameraDevice
{
	//	2.2.14	Arkit stream now skipping depth if frame is 0 (no new data), or if repeat frame. 
	//			Colour streams from Arkit now named RearColour/FrontColour/Colour
	//			Arkit now skipping segmentation creation by default. Option to use smooth depth
	//	2.2.15	Arkit smoothed scene depthmap wasn't being turned on. Now works & has SceneDepthSmoothed stream name
	//	2.2.16	Visual studio project outputting kinect DLLs in NoKinect builds, just to keep PopEngine build happy
	//	2.2.17	osx/ios set to use link compatibility version 1 so we don't get 1.0.0 can't use PopCameraDevice 0.0.0 errors
	//	2.2.18	Linux builds have been renamed
	//	2.2.19	"" for JSON params in CreateDevice now caught as "no-params"
	const Soy::TVersion	Version(2, 2, 19);
	const int32_t		NoFrame = -1;
	const int32_t		Error = -2;
}

__export int32_t PopCameraDevice_GetVersion()
{
	return PopCameraDevice::Version.GetMillion();
}


#if defined(ENABLE_KINECT2)
#include "Kinect2.h"
#endif

#if defined(ENABLE_FREENECT)
#include "Freenect.h"
#endif

#if defined(ENABLE_KINECTAZURE)
#include "KinectAzure.h"
#endif

#if defined(TARGET_WINDOWS)
#include "MfCapture.h"
#endif

#if defined(TARGET_OSX) || defined(TARGET_IOS)
#include "AvfCapture.h"
#endif

#if defined(TARGET_IOS)
#include "ArkitCapture.h"
#endif




class TDeviceAndFormats
{
public:
	std::string			mSerial;
	Array<std::string>	mFormats;
};

namespace PopCameraDevice
{
	class TDeviceInstance;

	TDevice&		GetCameraDevice(int32_t Instance);
	TDevice&		GetCameraDevice(uint32_t Instance);
	uint32_t		CreateInstance(std::shared_ptr<TDevice> Device);
	void			FreeInstance(uint32_t Instance);
	int32_t			GetNextFrame(int32_t Instance, char* JsonBuffer, int32_t JsonBufferSize, ArrayBridge<ArrayBridge<uint8_t>*>&& Planes, bool DeleteFrame);
	void			AddOnNewFrameCallback(int32_t Instance,std::function<void()> Callback);

	uint32_t		CreateCameraDevice(const std::string& Name,json11::Json& Options);

	//	to deal with windows exiting a process but silently destroying/detaching threads	
	//	we do a hail mary shutdown
	void			Shutdown(bool ProcessExit);

	std::mutex				InstancesLock;
	Array<TDeviceInstance>	Instances;
	uint32_t				InstancesCounter = 1;

	std::string				EnumDevicesJson();
	void					EnumDevices(ArrayBridge< TDeviceAndFormats>&& DeviceAndFormats);
}


#if defined(TARGET_WINDOWS)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	//	need to mark threads as exited which may have been shutdown from owner
	//	mostly used to clean up TLS
	//	on process detatch, threads are already gone...
	if (ul_reason_for_call == DLL_THREAD_DETACH)
	{
		//	gr: this seems to only be called for the main thread?
	}

	if ( ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		PopCameraDevice::Shutdown(true);
	}

	return TRUE;
}
#endif

#if !defined(TARGET_WINDOWS)
void __attribute__((destructor)) DllExit()
{
	PopCameraDevice::Shutdown(true);
}
#endif

class PopCameraDevice::TDeviceInstance
{
public:
	std::shared_ptr<TDevice>	mDevice;
	uint32_t					mInstanceId = 0;

	bool						operator==(const uint32_t& InstanceId) const	{	return mInstanceId == InstanceId;	}
};



void PushJsonKey(std::stringstream& Json, const char* Key, int PreTab = 1)
{
	for (auto t = 0; t < PreTab; t++)
	Json << '\t';
	Json << '"' << Key << '"' << ':';
}

template<typename TYPE>
void PushJson(std::stringstream& Json, const char* Key, const TYPE& Value, int PreTab = 1, bool TrailingComma = false)
{
	PushJsonKey(Json, Key, PreTab);
	
	//	todo: escape string
	Json << '"' << Value << '"';
	
	if (TrailingComma)
	Json << ',';
	Json << '\n';
}


void PushJson(std::stringstream& Json, const char* Key, const char* String, int PreTab = 1, bool TrailingComma = false)
{
	PushJsonKey(Json, Key, PreTab);
	
	//	todo: escape string!
	Json << '"' << String << '"';
	
	if (TrailingComma)
	Json << ',';
	Json << '\n';
}

void PushJson(std::stringstream& Json, const char* Key, bool Boolean, int PreTab = 1, bool TrailingComma = false)
{
	PushJsonKey(Json, Key, PreTab);
	
	Json << (Boolean ? "true" : "false");
	
	if (TrailingComma)
	Json << ',';
	Json << '\n';
}

void PushJson(std::stringstream& Json, const char* Key, int Number, int PreTab = 1, bool TrailingComma = false)
{
	PushJsonKey(Json, Key, PreTab);
	
	Json << Number;
	
	if (TrailingComma)
	Json << ',';
	Json << '\n';
}

//	help the compiler
void PushJson(std::stringstream& Json, const char* Key, size_t Number, int PreTab = 1, bool TrailingComma = false)
{
	PushJson(Json, Key, static_cast<int>(Number), PreTab, TrailingComma);
}

void PushJson(std::stringstream& Json, const char* Key, uint8_t Number, int PreTab = 1, bool TrailingComma = false)
{
	PushJson(Json, Key, static_cast<int>(Number), PreTab, TrailingComma);
}


void PushJson(std::stringstream& Json, const char* Key, float Number, int PreTab = 1, bool TrailingComma = false)
{
	PushJsonKey(Json, Key, PreTab);
	
	//	todo: handle special cases
	Json << Number;
	
	if (TrailingComma)
	Json << ',';
	Json << '\n';
}




const char* GetTabString(int TabCount)
{
	switch (TabCount)
	{
	case 0:	return "";
	case 1:	return "\t";
	case 2:	return "\t\t";
	case 3:	return "\t\t\t";
	case 4:	return "\t\t\t\t";
	case 5:	return "\t\t\t\t\t";
	default:
	case 6:	return "\t\t\t\t\t\t";
	}
}

void GetObjectJson(const TDeviceAndFormats& DeviceAndFormats, std::stringstream& Json,int Tabs)
{
	Json << GetTabString(Tabs) << "{\n";
	PushJson(Json, "Serial", DeviceAndFormats.mSerial, Tabs+1, true);

	PushJsonKey(Json, "Formats", Tabs + 1);
	Json << "[\n";
	for (auto f = 0; f < DeviceAndFormats.mFormats.GetSize(); f++)
	{
		auto& Format = DeviceAndFormats.mFormats[f];
		Json << GetTabString(Tabs+2) << "\"" << Format << "\"";
		if (f != DeviceAndFormats.mFormats.GetSize() - 1)
			Json << ",";
		Json << "\n";
	}
	Json << GetTabString(Tabs+2) << "]\n";
	Json << GetTabString(Tabs) << "}";
}


void GetObjectJson(json11::Json::object& Json,const SoyPixelsMeta& PlaneMeta)
{
	Json["Width"] = static_cast<int>(PlaneMeta.GetWidth());
	Json["Height"] = static_cast<int>(PlaneMeta.GetHeight());
	Json["Format"] = SoyPixelsFormat::ToString(PlaneMeta.GetFormat());
	Json["DataSize"] = static_cast<int>(PlaneMeta.GetDataSize());
	Json["Channels"] = PlaneMeta.GetChannels();
}

void PopCameraDevice::EnumDevices(ArrayBridge<TDeviceAndFormats>&& DeviceAndFormats)
{
	auto EnumDevice = [&](const std::string& Name)
	{
		auto& Device = DeviceAndFormats.PushBack();
		Device.mSerial = Name;
	};
	
	auto EnumDeviceAndFormats = [&](const std::string& Name,ArrayBridge<std::string>&& Formats)
	{
		auto& Device = DeviceAndFormats.PushBack();
		Device.mSerial = Name;
		Device.mFormats.Copy(Formats);
	};

	TestDevice::EnumDeviceNames(EnumDevice);
#if defined(TARGET_WINDOWS)
	MediaFoundation::EnumDeviceNameAndFormats(EnumDeviceAndFormats);
#elif defined(TARGET_OSX) || defined(TARGET_IOS)
	Avf::EnumCaptureDevices(EnumDeviceAndFormats);
#endif
#if defined(TARGET_IOS)
	Arkit::EnumDevices(EnumDeviceAndFormats);
#endif
	
#if defined(ENABLE_FREENECT)
	Freenect::EnumDeviceNames(EnumDevice);
#endif

#if defined(ENABLE_KINECT2)
	Kinect2::EnumDeviceNames(EnumDevice);
#endif

#if defined(ENABLE_KINECTAZURE)
	KinectAzure::EnumDeviceNameAndFormats(EnumDeviceAndFormats);
#endif
}


std::string PopCameraDevice::EnumDevicesJson()
{
	Array<TDeviceAndFormats> DeviceAndFormats;
	EnumDevices(GetArrayBridge(DeviceAndFormats));

	//	make json
	std::stringstream Json;
	Json << "{\n";
	
	PushJsonKey(Json, "Devices");
	{
		Json << "[\n";
		for (auto d = 0; d < DeviceAndFormats.GetSize(); d++)
		{
			auto Tabs = 2;
			auto& DeviceAndFormat = DeviceAndFormats[d];
			GetObjectJson(DeviceAndFormat, Json, Tabs);

			if (d != DeviceAndFormats.GetSize() - 1)
			{
				Json << ",";
			}
			Json << "\n";
		}
		Json << "\t]\n";
	}

	Json << "}\n";
	return Json.str();
}

//	gr: make this safe!
__export void PopCameraDevice_EnumCameraDevicesJson(char* StringBuffer,int32_t StringBufferLength)
{
	auto Json = PopCameraDevice::EnumDevicesJson();

	Soy::StringToBuffer(Json, StringBuffer, StringBufferLength);
}



uint32_t PopCameraDevice::CreateCameraDevice(const std::string& Name,json11::Json& Options)
{
	//	alloc device
	if (Name == TestDevice::DeviceName)
	{
		try
		{
			std::shared_ptr<TDevice> Device(new TestDevice(Options));
			if (Device)
				return PopCameraDevice::CreateInstance(Device);
		}
		catch (TInvalidNameException& e)
		{
			//std::Debug << e.what() << std::endl;
		}
	}


#if defined(ENABLE_KINECT2)
	try
	{
		std::shared_ptr<TDevice> Device(new Kinect2::TDevice(Name));
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(TInvalidNameException& e)
	{
		//std::Debug << e.what() << std::endl;
	}
#endif

	
#if defined(ENABLE_FREENECT)
	try
	{
		std::shared_ptr<TDevice> Device(new Freenect::TSource(Name,Options));
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(TInvalidNameException& e)
	{
		//std::Debug << e.what() << std::endl;
	}
#endif


#if defined(ENABLE_KINECTAZURE)
	try
	{
		std::shared_ptr<TDevice> Device(new KinectAzure::TCameraDevice(Name,Options));
		if (Device)
			return PopCameraDevice::CreateInstance(Device);
	}
	catch (TInvalidNameException& e)
	{
		//std::Debug << e.what() << std::endl;
	}
#endif

#if defined(TARGET_IOS)
	if ( Name == Arkit::TFrameProxyDevice::DeviceName )
	{
		std::shared_ptr<TDevice> Device(new Arkit::TFrameProxyDevice(Options));
		if (Device)
			return PopCameraDevice::CreateInstance(Device);
	}
#endif

#if defined(TARGET_IOS)
	if ( Name == Arkit::TSessionCamera::DeviceName_SceneDepth || Name == Arkit::TSessionCamera::DeviceName_FrontDepth )
	{
		std::shared_ptr<TDevice> Device(new Arkit::TSessionCamera(Name,Options));
		if (Device)
			return PopCameraDevice::CreateInstance(Device);
	}
#endif
	
	//	do generic, non-prefixed names LAST
	try
	{
#if defined(TARGET_WINDOWS)
		std::shared_ptr<TDevice> Device(new MediaFoundation::TCamera(Name));
#elif defined(TARGET_OSX) || defined(TARGET_IOS)
		std::shared_ptr<TDevice> Device(new Avf::TCamera(Name,Options));
#else
		std::shared_ptr<TDevice> Device;
#endif
		if (Device)
			return PopCameraDevice::CreateInstance(Device);
	}
	catch (TInvalidNameException& e)
	{
		//std::Debug << e.what() << std::endl;
	}

	//	this shouldn't REALLY occur any more as the generic capture device should fail to find the name
	throw Soy::AssertException("Failed to create device, name/serial didn't match any systems");
}



template<typename RETURN,typename FUNC>
RETURN SafeCall(FUNC Function,const char* FunctionName,RETURN ErrorReturn)
{
	try
	{
		return Function();
	}
	catch(std::exception& e)
	{
		std::Debug << FunctionName << " exception: " << e.what() << std::endl;
		return ErrorReturn;
	}
	catch(...)
	{
		std::Debug << FunctionName << " unknown exception." << std::endl;
		return ErrorReturn;
	}
}


__export int32_t PopCameraDevice_CreateCameraDevice(const char* Name,const char* OptionsJson, char* ErrorBuffer, int32_t ErrorBufferLength)
{
	try
	{
		if ( !OptionsJson )
			OptionsJson = "{}";
		if ( strlen(OptionsJson) == 0 )
			OptionsJson = "{}";
			
		std::string ParseError;
		json11::Json Options = json11::Json::parse( OptionsJson, ParseError );
		if ( !ParseError.empty() )
		{
			ParseError = std::string("PopCameraDevice_CreateCameraDevice parse json error; ") + ParseError;
			throw Soy::AssertException(ParseError);
		}
		auto InstanceId = PopCameraDevice::CreateCameraDevice( Name, Options );
		return InstanceId;
	}
	catch(std::exception& e)
	{
		Soy::StringToBuffer(e.what(), ErrorBuffer, ErrorBufferLength);
		return 0;
	}
	catch(...)
	{
		Soy::StringToBuffer("Unknown exception", ErrorBuffer, ErrorBufferLength);
		return 0;
	}
}


PopCameraDevice::TDevice& PopCameraDevice::GetCameraDevice(int32_t Instance)
{
	if ( Instance < 0 )
	{
		std::stringstream Error;
		Error << "Invalid instance identifier " << Instance;
		throw Soy::AssertException(Error.str());
	}
	return GetCameraDevice( static_cast<uint32_t>( Instance ) );
}

PopCameraDevice::TDevice& PopCameraDevice::GetCameraDevice(uint32_t Instance)
{
	std::lock_guard<std::mutex> Lock(InstancesLock);
	auto pInstance = Instances.Find(Instance);
	auto* Device = pInstance ? pInstance->mDevice.get() : nullptr;
	if ( !Device )
	{
		std::stringstream Error;
		Error << "No instance/device matching " << Instance;
		throw Soy::AssertException(Error.str());
	}

	return *Device;
}


uint32_t PopCameraDevice::CreateInstance(std::shared_ptr<TDevice> Device)
{
	std::lock_guard<std::mutex> Lock(InstancesLock);
	
	TDeviceInstance Instance;
	Instance.mInstanceId = InstancesCounter;
	Instance.mDevice = Device;
	Instances.PushBack(Instance);

	InstancesCounter++;
	return Instance.mInstanceId;
}


void PopCameraDevice::FreeInstance(uint32_t Instance)
{
	//	lock, pop, and destroy outside lock
	TDeviceInstance InstanceCopy;
	{
		std::lock_guard<std::mutex> Lock(InstancesLock);

		auto InstanceIndex = Instances.FindIndex(Instance);
		if (InstanceIndex < 0)
		{
			std::Debug << "No instance " << Instance << " to free" << std::endl;
			return;
		}
		InstanceCopy = Instances.PopAt(InstanceIndex);
	}

	//	actual destroy outside lock
	//	gr: this line isn't needed, scope of Instance does it. But to aid debugging
	InstanceCopy.mDevice.reset();
}


void PopCameraDevice::ReadNativeHandle(int32_t Instance,void* Handle)
{
	auto& Device = GetCameraDevice(Instance);
	Device.ReadNativeHandle(Handle);
}



void GetJson(json11::Json::object& Json,SoyPixelsMeta PixelMeta)
{
	//	output all plane info
	BufferArray<SoyPixelsMeta, 3> PlaneMetas;
	PixelMeta.GetPlanes(GetArrayBridge(PlaneMetas));

	json11::Json::array Planes;
	for (auto p = 0; p < PlaneMetas.GetSize(); p++)
	{
		auto& PlaneMeta = PlaneMetas[p];
		json11::Json::object PlaneMetaObject;
		GetObjectJson(PlaneMetaObject, PlaneMeta);
		Planes.push_back(PlaneMetaObject);
	}

	Json["Planes"] = Planes;
}


void CopyPlanes(ArrayBridge<SoyPixelsImpl*>&& PlaneSrcs,ArrayBridge<ArrayBridge<uint8_t>*>& PlaneDsts,json11::Json::object& JsonMeta)
{
	json11::Json::array PlaneMetas;
	
	for (auto p = 0; p <PlaneSrcs.GetSize(); p++)
	{
		auto& PlaneSrc = *PlaneSrcs[p];
		//	gr: get meta first, even if there's no buffer (for peek!)
		auto PlaneMeta = PlaneSrc.GetMeta();
		json11::Json::object PlaneMetaObject;
		GetObjectJson(PlaneMetaObject, PlaneMeta);
		PlaneMetas.push_back(PlaneMetaObject);
	
		//	is there a buffer for this plane?
		if (p >= PlaneDsts.GetSize())
			continue;
		auto* pPlaneDstArray = PlaneDsts[p];
		if (!pPlaneDstArray)
			continue;
		
		auto& PlaneSrcArray = PlaneSrc.GetPixelsArray();
		auto& PlaneDstArray = *pPlaneDstArray;
		
		auto MaxSize = std::min(PlaneDstArray.GetDataSize(), PlaneSrcArray.GetDataSize());
		//	copy as much as possible
		auto PlaneSrcPixelsMin = GetRemoteArray(PlaneSrcArray.GetArray(), MaxSize);
		PlaneDstArray.Copy(PlaneSrcPixelsMin);
	}
	JsonMeta["Planes"] = PlaneMetas;
}

void CopyPlanes(TPixelBuffer& PixelBuffer,ArrayBridge<ArrayBridge<uint8_t>*>& PlaneBuffers,json11::Json::object& JsonMeta,bool SplitPlanes)
{
	//	gr: we're losing this transform. go back through PixelBuffer implementations
	//		to see if we explicitly sometimes reveal this transform ONLY on locking the 
	//		pixel buffer, or if it always comes from preexisting meta
	float3x3 Transform;
	BufferArray<SoyPixelsImpl*, 10> Textures;
	PixelBuffer.Lock(GetArrayBridge(Textures), Transform);
	try
	{
		//	split planes of planes
		BufferArray<std::shared_ptr<SoyPixelsImpl>, 10> Planes;
		BufferArray<SoyPixelsImpl*,10> PlanePtrs;
		if ( SplitPlanes )
		{
			for (auto t = 0; t < Textures.GetSize(); t++)
			{
				auto& Texture = *Textures[t];
				Texture.SplitPlanes(GetArrayBridge(Planes));
			}
			for ( auto p=0;	p<Planes.GetSize();	p++ )
				PlanePtrs.PushBack( Planes[p].get() );
		}
		else
		{
			//	just store originals (assuming theyre not split)
			for (auto t = 0; t < Textures.GetSize(); t++)
			{
				auto& Texture = *Textures[t];
				PlanePtrs.PushBack(&Texture);
			}
		}

		CopyPlanes( GetArrayBridge(PlanePtrs), PlaneBuffers, JsonMeta );
		PixelBuffer.Unlock();
	}
	catch (...)
	{
		PixelBuffer.Unlock();
		throw;
	}
}


void PopCameraDevice::AddOnNewFrameCallback(int32_t Instance, std::function<void()> Callback)
{
	auto& Device = PopCameraDevice::GetCameraDevice(Instance);
	Device.mOnNewFrameCallbacks.PushBack(Callback);
}


int32_t PopCameraDevice::GetNextFrame(int32_t Instance, char* JsonBuffer, int32_t JsonBufferSize, ArrayBridge<ArrayBridge<uint8_t>*>&& Planes,bool DeleteFrame)
{
	try
	{
		//	always clear json buffer
		if ( JsonBuffer )
			Soy::StringToBuffer("", JsonBuffer, JsonBufferSize);

		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		TFrame Frame;
		if ( !Device.GetNextFrame(Frame, DeleteFrame ) )
			return PopCameraDevice::NoFrame;

		auto Meta = Frame.GetMetaJson();

		//	get extra meta
		Device.GetDeviceMeta(Meta);

		CopyPlanes( *Frame.mPixelBuffer, Planes, Meta, Device.mSplitPlanes );

		//	copy meta out
		if (JsonBuffer)
		{
			//	copy to output
			auto JsonString = json11::Json(Meta).dump();
			Soy::StringToBuffer(JsonString, JsonBuffer, JsonBufferSize);
		}

		return Frame.mFrameTime.GetTime();
	}
	catch (std::exception& e)
	{
		std::stringstream Json;
		Json << "{\"Exception\":\"" << e.what() << "\"}";
		Soy::StringToBuffer(Json.str(), JsonBuffer, JsonBufferSize);
		throw;
	}
	catch (...)
	{
		std::string Json = "{\"Exception\":\"Unknown\"}";
		Soy::StringToBuffer(Json, JsonBuffer, JsonBufferSize);
		throw;
	}
}




__export int32_t PopCameraDevice_PeekNextFrame(int32_t Instance, char* JsonBuffer, int32_t JsonBufferSize)
{
	BufferArray<ArrayBridge<uint8_t>*,1> NoBuffers;
	auto DeleteFrame = false;
	return PopCameraDevice::GetNextFrame(Instance, JsonBuffer, JsonBufferSize, GetArrayBridge(NoBuffers), DeleteFrame);
}

__export void PopCameraDevice_AddOnNewFrameCallback(int32_t Instance, PopCameraDevice_OnNewFrame* Callback, void* Meta)
{
	auto Function = [&]()
	{
		if (!Callback)
			throw Soy::AssertException("PopCameraDevice_AddOnNewFrameCallback callback is null");

		auto Lambda = [Callback, Meta]()
		{
			Callback(Meta);
		};
		PopCameraDevice::AddOnNewFrameCallback(Instance, Lambda);
		return 0;
	};
	SafeCall(Function, __func__, 0);
}


__export void PopCameraDevice_FreeCameraDevice(int32_t Instance)
{
	auto Function = [&]()
	{
		PopCameraDevice::FreeInstance(Instance);
		return 0;
	};
	SafeCall(Function, __func__, 0 );
}



__export int32_t PopCameraDevice_PopNextFrame(int32_t Instance, char* MetaJsonBuffer, int32_t MetaJsonBufferSize, uint8_t* Plane0, int32_t Plane0Size, uint8_t* Plane1, int32_t Plane1Size, uint8_t* Plane2, int32_t Plane2Size)
{
	auto Function = [&]()
	{
		auto Plane0Array = GetRemoteArray(Plane0, Plane0Size);
		auto Plane1Array = GetRemoteArray(Plane1, Plane1Size);
		auto Plane2Array = GetRemoteArray(Plane2, Plane2Size);
		auto Plane0ArrayBridge = GetArrayBridge(Plane0Array);
		auto Plane1ArrayBridge = GetArrayBridge(Plane1Array);
		auto Plane2ArrayBridge = GetArrayBridge(Plane2Array);
		BufferArray<ArrayBridge<uint8_t>*, 3> PlaneArrays;
		PlaneArrays.PushBack(&Plane0ArrayBridge);
		PlaneArrays.PushBack(&Plane1ArrayBridge);
		PlaneArrays.PushBack(&Plane2ArrayBridge);

		auto DeleteFrame = true;
		auto Result = PopCameraDevice::GetNextFrame(Instance, MetaJsonBuffer, MetaJsonBufferSize, GetArrayBridge(PlaneArrays), DeleteFrame);
		return Result;
	};
	return SafeCall(Function, __func__, PopCameraDevice::Error);
}


void PopCameraDevice::Shutdown(bool ProcessExit)
{
#if defined(ENABLE_FREENECT)
	Freenect::Shutdown(ProcessExit);
#endif
}

__export void PopCameraDevice_Cleanup()
{
	PopCameraDevice::Shutdown(false);
}

__export void PopCameraDevice_UnitTests()
{
	PopCameraDevice::DecodeFormatString_UnitTests();
}

__export void PopCameraDevice_ReadNativeHandle(int32_t Instance,void* Handle)
{
	auto Function = [&]()
	{
		PopCameraDevice::ReadNativeHandle( Instance, Handle );
		return 0;
	};
	SafeCall(Function, __func__, 0);
}



__export void UnityPluginLoad(/*IUnityInterfaces*/void*)
{
}

__export void UnityPluginUnload()
{
	PopCameraDevice_Cleanup();
}
