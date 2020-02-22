#include "PopCameraDevice.h"
#include <exception>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>
#include "SoyLib/src/HeapArray.hpp"
#include "TestDevice.h"



namespace PopCameraDevice
{
	const Soy::TVersion	Version(2, 0, 0);
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
	bool			PopFrame(TDevice& Device, ArrayBridge<uint8_t>&& Plane0, ArrayBridge<uint8_t>&& Plane1, ArrayBridge<uint8_t>&& Plane2,std::string& Meta);

	uint32_t		CreateCameraDevice(const std::string& Name,const std::string& Format);

	void			Shutdown();

	std::mutex				InstancesLock;
	Array<TDeviceInstance>	Instances;
	uint32_t				InstancesCounter = 1;

	std::string				EnumDevicesJson();
	void					EnumDevices(ArrayBridge< TDeviceAndFormats>&& DeviceAndFormats);
}


#if defined(TARGET_WINDOWS)
BOOL APIENTRY DllMain(HMODULE /* hModule */, DWORD ul_reason_for_call, LPVOID /* lpReserved */)
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
	//if (ul_reason_for_call == DLL_THREAD_DETACH || ul_reason_for_call == DLL_PROCESS_DETACH)
	if ( ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		PopCameraDevice_Cleanup();
	}

	return TRUE;
}
#endif



class PopCameraDevice::TDeviceInstance
{
public:
	std::shared_ptr<TDevice>	mDevice;
	uint32_t					mInstanceId = 0;

	bool						operator==(const uint32_t& InstanceId) const	{	return mInstanceId == InstanceId;	}
};


void GetObjectJson(const TDeviceAndFormats& DeviceAndFormats, std::stringstream& Json)
{
	Json << "{\n";
	Json << "\t\"Serial\":\"" << DeviceAndFormats.mSerial << "\",\n";
	Json << "\t\"Formats\":\"[\n";
	for (auto f = 0; f < DeviceAndFormats.mFormats.GetSize(); f++)
	{
		auto& Format = DeviceAndFormats.mFormats[f];
		Json << "\t\t\"" << Format << "\"";
		if (f != DeviceAndFormats.mFormats.GetSize() - 1)
			Json << ",";
		Json << "\n";
	}
	Json << "\t]\n";
	Json << "}\n";
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
	MediaFoundation::EnumCaptureDevices(EnumDevice);
#elif defined(TARGET_OSX)
	Avf::EnumCaptureDevices(EnumDeviceAndFormats);
#endif
	//Freenect2::EnumDeviceNames(EnumDevice);
	//Kinect::EnumDeviceNames(EnumDevice);

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
	
	for (auto d = 0; d < DeviceAndFormats.GetSize(); d++)
	{
		auto& DeviceAndFormat = DeviceAndFormats[d];
		GetObjectJson(DeviceAndFormat, Json);

		if (d != DeviceAndFormats.GetSize()-1)
		{
			Json << ",\n";
		}
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



uint32_t PopCameraDevice::CreateCameraDevice(const std::string& Name,const std::string& Format)
{
	//	alloc device
	try
	{
		auto Device = TestDevice::CreateDevice(Name);
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}


	try
	{
#if defined(TARGET_WINDOWS)
		std::shared_ptr<TDevice> Device(new MediaFoundation::TCamera(Name));
#elif defined(TARGET_OSX) || defined(TARGET_IOS)
		std::shared_ptr<TDevice> Device(new Avf::TCamera(Name));
#endif
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}

#if defined(ENABLE_KINECT2)
	try
	{
		std::shared_ptr<TDevice> Device(new Kinect2::TDevice(Name));
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif

	
#if defined(ENABLE_FREENECT)
	try
	{
		std::shared_ptr<TDevice> Device(new Freenect::TSource(Name));
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif


#if defined(ENABLE_KINECTAZURE)
	try
	{
		std::shared_ptr<TDevice> Device(new KinectAzure::TCameraDevice(Name));
		if (Device)
			return PopCameraDevice::CreateInstance(Device);
	}
	catch (std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif

	throw Soy::AssertException("Failed to create device");
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

__export int32_t PopCameraDevice_CreateCameraDeviceWithFormat(const char* Name,const char* Format, char* ErrorBuffer, int32_t ErrorBufferLength)
{
	try
	{
		Format = Format ? Format : "";
		auto InstanceId = PopCameraDevice::CreateCameraDevice( Name, Format );
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
	std::lock_guard<std::mutex> Lock(InstancesLock);

	auto InstanceIndex = Instances.FindIndex(Instance);
	if ( InstanceIndex < 0 )
	{
		std::Debug << "No instance " << Instance << " to free" << std::endl;
		return;
	}

	Instances.RemoveBlock(InstanceIndex, 1);
}


void PushJsonKey(std::stringstream& Json,const char* Key,int PreTab=1)
{
	for ( auto t=0;	t<PreTab;	t++ )
		Json << '\t';
	Json << '"' << Key << '"' << ':';
}

template<typename TYPE>
void PushJson(std::stringstream& Json,const char* Key,const TYPE& Value,int PreTab=1,bool TrailingComma=false)
{
	PushJsonKey(Json,Key,PreTab);

	//	todo: escape string
	Json << '"' << Value << '"';

	if ( TrailingComma )
		Json << ',';
	Json << '\n';
}


void PushJson(std::stringstream& Json,const char* Key,const char* String,int PreTab=1,bool TrailingComma=false)
{
	PushJsonKey(Json,Key,PreTab);

	//	todo: escape string!
	Json << '"' << String << '"';

	if ( TrailingComma )
		Json << ',';
	Json << '\n';
}

void PushJson(std::stringstream& Json,const char* Key,bool Boolean,int PreTab=1,bool TrailingComma=false)
{
	PushJsonKey(Json,Key,PreTab);

	Json << Boolean ? "true" : "false";

	if ( TrailingComma )
		Json << ',';
	Json << '\n';
}

void PushJson(std::stringstream& Json,const char* Key,int Number,int PreTab=1,bool TrailingComma=false)
{
	PushJsonKey(Json,Key,PreTab);

	Json << Number;

	if ( TrailingComma )
		Json << ',';
	Json << '\n';
}

void PushJson(std::stringstream& Json,const char* Key,float Number,int PreTab=1,bool TrailingComma=false)
{
	PushJsonKey(Json,Key,PreTab);

	//	todo: handle special cases
	Json << Number;

	if ( TrailingComma )
		Json << ',';
	Json << '\n';
}


__export void PopCameraDevice_GetNextFrameMeta(int32_t Instance, char* JsonBuffer, int32_t JsonBufferSize)
{
	try
	{
		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		auto Meta = Device.GetMeta();

		BufferArray<SoyPixelsMeta, 3> PlaneMetas;
		Meta.GetPlanes(GetArrayBridge(PlaneMetas));

		bool HasNewFrame = Device.HasNewFrame();
		
		std::stringstream Json;
		Json << "{\n";
		{
			PushJsonKey(Json,"Planes",1);
			Json << "[\n";

			for ( auto p=0;	p<PlaneMetas.GetSize();	p++ )
			{
				auto Tabs = 2;
				auto& PlaneMeta = PlaneMetas[p];
				PushJson(Json,"Width",PlaneMeta.GetWidth(),Tabs,true);
				PushJson(Json,"Height",PlaneMeta.GetHeight(),Tabs,true);
				PushJson(Json,"Format",PlaneMeta.GetFormat(),Tabs,true);
				//	gr: don't show size if no next frame
				if ( HasNewFrame )
					PushJson(Json,"DataSize",PlaneMeta.GetDataSize(),Tabs,true);
				PushJson(Json,"Channels",PlaneMeta.GetChannels(),Tabs,false);
			}
			Json << "]\n";
		}
		Json << "}\n";
		
		Soy::StringToBuffer( Json.str(), JsonBuffer, JsonBufferSize );
	}
	catch(std::exception& e)
	{
		std::stringstream Json;
		Json << "{\"Exception\":\"" << e.what() << "\"}";
		Soy::StringToBuffer( Json.str(), JsonBuffer, JsonBufferSize );
	}
	catch(...)
	{
		std::string Json = "{\"Exception\":\"Unknown\"}";
		Soy::StringToBuffer( Json, JsonBuffer, JsonBufferSize );
	}
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


bool PopCameraDevice::PopFrame(TDevice& Device,ArrayBridge<uint8_t>&& Plane0,ArrayBridge<uint8_t>&& Plane1,ArrayBridge<uint8_t>&& Plane2, std::string& Meta)
{
	if ( !Device.PopLastFrame(Plane0, Plane1, Plane2, Meta) )
		return false;

	return true;
}


__export int32_t PopCameraDevice_PopFrameAndMeta(int32_t Instance, uint8_t* Plane0, int32_t Plane0Size, uint8_t* Plane1, int32_t Plane1Size, uint8_t* Plane2, int32_t Plane2Size,char* MetaBuffer,int32_t MetaBufferLength)
{
	auto Function = [&]()
	{
		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		auto Plane0Array = GetRemoteArray(Plane0, Plane0Size);
		auto Plane1Array = GetRemoteArray(Plane1, Plane1Size);
		auto Plane2Array = GetRemoteArray(Plane2, Plane2Size);

		std::string MetaString;
		auto Result = PopCameraDevice::PopFrame(Device, GetArrayBridge(Plane0Array), GetArrayBridge(Plane1Array), GetArrayBridge(Plane2Array), MetaString );

		//	copy out meta
		if (MetaBuffer)
		{
			for (auto i = 0; i < MetaBufferLength; i++)
			{
				auto Char = (i < MetaString.length()) ? MetaString[i] : '\0';
				MetaBuffer[i] = Char;
			}
		}

		return Result ? 1 : 0;
	};
	return SafeCall(Function, __func__, 0);
}


__export int32_t PopCameraDevice_PopFrame(int32_t Instance, uint8_t* Plane0, int32_t Plane0Size, uint8_t* Plane1, int32_t Plane1Size, uint8_t* Plane2, int32_t Plane2Size)
{
	return PopCameraDevice_PopFrameAndMeta(Instance, Plane0, Plane0Size, Plane1, Plane1Size, Plane2, Plane2Size, nullptr, 0);
}

void PopCameraDevice::Shutdown()
{
#if defined(ENABLE_FREENECT)
	Freenect::Shutdown();
#endif
}

__export void PopCameraDevice_Cleanup()
{
	PopCameraDevice::Shutdown();
}
