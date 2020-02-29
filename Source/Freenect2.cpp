#include "Freenect2.h"
#include <libfreenect2/libfreenect2.hpp>
#include "SoyAssert.h"

//	osx
//	brew install libusb
//	cd Libs/libfreenect2/
//	mkdir build
//	cd build
//	cmake .. -DENABLE_OPENCL:BOOL=OFF -DBUILD_OPENNI2_DRIVER:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_OPENGL:BOOL=OFF
//	make
//	builds .a

namespace Freenect2
{
	std::shared_ptr<TContext> gContext;
	const auto DeviceNamePrefix = "Kinect2:";
}


class Freenect2::TContext
{
public:
	TContext();
	~TContext();
	
	void			EnumDevices(std::function<void(const std::string&)>& Enum);
	
	std::shared_ptr<libfreenect2::Freenect2>	mFreenect;
};



Freenect2::TContext& Freenect2::GetContext()
{
	if ( !gContext )
	{
		gContext.reset( new TContext() );
	}
	return *gContext;
}


Freenect2::TContext::TContext()
{
	mFreenect.reset( new libfreenect2::Freenect2() );
}


Freenect2::TContext::~TContext()
{
	//	may need to stop threads here
	mFreenect.reset();
}

void Freenect2::TContext::EnumDevices(std::function<void(const std::string&)>& Enum)
{
	auto DeviceCount = mFreenect->enumerateDevices();
	for ( auto d=0;	d<DeviceCount;	d++ )
	{
		auto DeviceSerial = mFreenect->getDeviceSerialNumber(d);
		std::stringstream Name;
		Name << DeviceNamePrefix << DeviceSerial;
		Enum( Name.str() );
	}
}



void Freenect2::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	auto& Context = GetContext();
	Context.EnumDevices( Enum );
}


Freenect2::TDevice::TDevice(const std::string& Serial)
{
	if( !Soy::StringBeginsWith(Serial, DeviceNamePrefix, true ) )
		throw PopCameraDevice::TInvalidNameException();

}

void Freenect2::TDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	throw Soy_AssertException("Feature not supported");
}






