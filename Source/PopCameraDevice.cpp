#include "PopCameraDevice.h"
#include <exception>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>
#include "SoyLib/src/HeapArray.hpp"
#include "TestDevice.h"


#if defined(TARGET_WINDOWS)
#include "MfCapture.h"
#endif

#if defined(TARGET_OSX)
#include "AvfCapture.h"
#endif


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
	return TRUE;
}
#endif


namespace PopCameraDevice
{
	class TDeviceInstance;

	TDevice&		GetCameraDevice(uint32_t Instance);
	uint32_t		CreateInstance(std::shared_ptr<TDevice> Device);
	void			FreeInstance(uint32_t Instance);
	bool			PopFrame(TDevice& Device, ArrayBridge<uint8_t>&& Plane0, ArrayBridge<uint8_t>&& Plane1, ArrayBridge<uint8_t>&& Plane2);

	uint32_t		CreateCameraDevice(const std::string& Name);

	std::mutex				InstancesLock;
	Array<TDeviceInstance>	Instances;
	uint32_t				InstancesCounter = 1;
}


class PopCameraDevice::TDeviceInstance
{
public:
	std::shared_ptr<TDevice>	mDevice;
	uint32_t					mInstanceId = 0;

	bool						operator==(const uint32_t& InstanceId) const	{	return mInstanceId == InstanceId;	}
};


//	gr: make this safe!
__export void PopCameraDevice_EnumCameraDevices(char* StringBuffer,int32_t StringBufferLength)
{
	//	first char is delin
	//	£ gives Illegal character encoding in string literal
	const char PossibleDelin[] = ",;:#!@+=_-&^%*$?|/ ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopoqrstuvwzy0123456789";
	auto PossibleDelinArray = GetRemoteArray( PossibleDelin );

	Array<std::string> DeviceNames;
	auto EnumDevice = [&](const std::string& Name)
	{
		DeviceNames.PushBack(Name);
	};
	TestDevice::EnumDeviceNames(EnumDevice);
#if defined(TARGET_WINDOWS)
	MediaFoundation::EnumCaptureDevices(EnumDevice);
#elif defined(TARGET_OSX)
	Avf::EnumCaptureDevices(EnumDevice);
#endif

	auto IsCharUsed = [&](char Char)
	{
		for ( int d=0;	d<DeviceNames.GetSize();	d++ )
		{
			auto& DeviceName = DeviceNames[d];
			auto Index = DeviceName.find_first_of(Char);
			if ( Index != DeviceName.npos )
				return true;
		}
		return false;
	};

	char Delin = 0;
	for ( auto pd=0;	pd<PossibleDelinArray.GetSize();	pd++ )
	{
		Delin = PossibleDelin[pd];
		bool Used = IsCharUsed(Delin);
		if ( !Used )
			break;		
	}
	//	todo! handle no unused chars!

	//	build output
	//	first char is delin, then each device seperated by that delin
	std::stringstream OutputString;
	OutputString << Delin;
	for ( int d=0;	d<DeviceNames.GetSize();	d++ )
	{
		auto& DeviceName = DeviceNames[d];
		OutputString << DeviceName << Delin;
	}

	auto OutputStringStr = OutputString.str();
	Soy::StringToBuffer( OutputStringStr, StringBuffer, StringBufferLength );
}



uint32_t PopCameraDevice::CreateCameraDevice(const std::string& Name)
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


#if defined(TARGET_WINDOWS)
	try
	{
		std::shared_ptr<TCameraDevice> Device(new MediaFoundation::TCamera(Name));
//#elif defined(TARGET_OSX)
//		std::shared_ptr<TCameraDevice> Device(new Avf::TCamera(Name));
		if ( Device )
			return PopCameraDevice::CreateInstance(Device);
	}
	catch(std::exception& e)
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

__export int32_t PopCameraDevice_CreateCameraDevice(const char* Name)
{
	auto Function = [&]()
	{
		auto InstanceId = PopCameraDevice::CreateCameraDevice( Name );
		return InstanceId;
	};
	return SafeCall( Function, __func__, -1 );
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


__export void PopCameraDevice_GetMeta(int32_t Instance, int32_t* pMetaValues, int32_t MetaValuesCount)
{
	auto Function = [&]()
	{
		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		auto Meta = Device.GetMeta();

		size_t MetaValuesCounter = 0;
		auto MetaValues = GetRemoteArray(pMetaValues, MetaValuesCount, MetaValuesCounter);

		BufferArray<SoyPixelsMeta, 3> PlaneMetas;
		Meta.GetPlanes(GetArrayBridge(PlaneMetas));
		MetaValues.PushBack(PlaneMetas.GetSize());

		for ( auto p=0;	p<PlaneMetas.GetSize();	p++ )
		{
			auto& PlaneMeta = PlaneMetas[p];
			MetaValues.PushBack(PlaneMeta.GetWidth());
			MetaValues.PushBack(PlaneMeta.GetHeight());
			MetaValues.PushBack(PlaneMeta.GetChannels());
			MetaValues.PushBack(PlaneMeta.GetFormat());
			MetaValues.PushBack(PlaneMeta.GetDataSize());
		}

		return 0;
	};
	 SafeCall(Function, __func__, 0 );
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

__export EXPORTCLASS* PopCameraDevice_GetDevicePtr(int32_t Instance)
{
	auto Function = [&]()
	{
		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		return &Device;
	};
	return SafeCall<EXPORTCLASS*>( Function, __func__, nullptr );
}


bool PopCameraDevice::PopFrame(TDevice& Device,ArrayBridge<uint8_t>&& Plane0,ArrayBridge<uint8_t>&& Plane1,ArrayBridge<uint8_t>&& Plane2)
{
	if ( !Device.PopLastFrame(Plane0, Plane1, Plane2) )
		return false;

	return true;
}

__export int32_t PopCameraDevice_PopFrame(int32_t Instance,uint8_t* Plane0,int32_t Plane0Size,uint8_t* Plane1,int32_t Plane1Size,uint8_t* Plane2,int32_t Plane2Size)
{
	auto Function = [&]()
	{
		auto& Device = PopCameraDevice::GetCameraDevice(Instance);
		auto Plane0Array = GetRemoteArray(Plane0, Plane0Size);
		auto Plane1Array = GetRemoteArray(Plane1, Plane1Size);
		auto Plane2Array = GetRemoteArray(Plane2, Plane2Size);
		auto Result = PopCameraDevice::PopFrame(Device, GetArrayBridge(Plane0Array), GetArrayBridge(Plane1Array), GetArrayBridge(Plane2Array));
		return Result ? 1 : 0;
	};
	return SafeCall(Function, __func__, 0 );
}

