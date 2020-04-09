#include "TOutputDevice.h"
#include <exception>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>
#include "SoyLib/src/HeapArray.hpp"
#include "TestDevice.h"


#if defined(TARGET_OSX) || defined(TARGET_IOS)
#include "AvfCapture.h"
#endif


namespace PopOutputDevice
{
	class TDeviceInstance;

	TDevice&		GetDevice(int32_t Instance);
	TDevice&		GetDevice(uint32_t Instance);
	uint32_t		CreateInstance(std::shared_ptr<TDevice> Device);
	void			FreeInstance(uint32_t Instance);
	void			PushFrame(int32_t Instance,const ArrayBridge<uint8_t>&& Pixels,const std::string& MetaJson);

	uint32_t		CreateDevice(const std::string& Name,const std::string& Format);

	//	to deal with windows exiting a process but silently destroying/detaching threads	
	//	we do a hail mary shutdown
	void			Shutdown(bool ProcessExit);

	std::mutex				InstancesLock;
	Array<TDeviceInstance>	Instances;
	uint32_t				InstancesCounter = 1;
}


class PopOutputDevice::TDeviceInstance
{
public:
	std::shared_ptr<TDevice>	mDevice;
	uint32_t					mInstanceId = 0;

	bool						operator==(const uint32_t& InstanceId) const	{	return mInstanceId == InstanceId;	}
};


uint32_t PopOutputDevice::CreateDevice(const std::string& Name,const std::string& Format)
{
#if defined(TARGET_WINDOWS)
	std::shared_ptr<TDevice> Device(new MediaFoundation::TOutputDevice(Name));
#elif defined(TARGET_OSX) || defined(TARGET_IOS)
	std::shared_ptr<TDevice> Device(new Avf::TOutputDevice(Name));
#endif
	if (Device)
		return PopOutputDevice::CreateInstance(Device);
	
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

__export int32_t PopOutputDevice_CreateDevice(const char* Name,const char* Format, char* ErrorBuffer, int32_t ErrorBufferLength)
{
	try
	{
		Format = Format ? Format : "";
		auto InstanceId = PopOutputDevice::CreateDevice( Name, Format );
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


PopOutputDevice::TDevice& PopOutputDevice::GetDevice(int32_t Instance)
{
	if ( Instance < 0 )
	{
		std::stringstream Error;
		Error << "Invalid instance identifier " << Instance;
		throw Soy::AssertException(Error.str());
	}
	return GetDevice( static_cast<uint32_t>( Instance ) );
}

PopOutputDevice::TDevice& PopOutputDevice::GetDevice(uint32_t Instance)
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


uint32_t PopOutputDevice::CreateInstance(std::shared_ptr<TDevice> Device)
{
	std::lock_guard<std::mutex> Lock(InstancesLock);
	
	TDeviceInstance Instance;
	Instance.mInstanceId = InstancesCounter;
	Instance.mDevice = Device;
	Instances.PushBack(Instance);

	InstancesCounter++;
	return Instance.mInstanceId;
}


void PopOutputDevice::FreeInstance(uint32_t Instance)
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


void PopOutputDevice::PushFrame(int32_t Instance,const ArrayBridge<uint8_t>&& Pixels,const std::string& MetaJson)
{
	auto& Device = GetCameraDevice(Instance);
	Device.PushFrame( Pixels, MetaJson );
}

__export void PopOutputDevice_FreeCameraDevice(int32_t Instance)
{
	auto Function = [&]()
	{
		PopOutputDevice::FreeInstance(Instance);
		return 0;
	};
	SafeCall(Function, __func__, 0 );
}


__export void PopOutputDevice_PushFrame(int32_t Instance,uint8_t* PixelBuffer,size_t PixelBufferSize,const char* MetaJson,char* ErrorBuffer, int32_t ErrorBufferLength)
{
	try
	{
		auto PixelArray = GetRemoteArray( PixelBuffer, PixelBufferSize );
		std::string Meta( MetaJson ? MetaJson : "" );
		PopOutputDevice::PushFrame( Instance, GetArrayBridge(PixelArray), MetaJson );
	}
	catch(std::exception& e)
	{
		Soy::StringToBuffer( e.what(), ErrorBuffer, ErrorBufferLength );
	}
	catch(...)
	{
		auto Error = "Unknown exception";
		Soy::StringToBuffer( Error, ErrorBuffer, ErrorBufferLength );
	}
}
