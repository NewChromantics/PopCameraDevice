#include "Kinect2.h"
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

namespace Kinect2
{
	std::shared_ptr<TContext> gContext;
	const auto DeviceNamePrefix = "Kinect2:";
}


class Kinect2::TContext
{
public:
	TContext();
	~TContext();
	
	void			EnumDevices(std::function<void(const std::string&)>& Enum);
	
	std::shared_ptr<libfreenect2::Freenect2>	mFreenect;
};



Kinect2::TContext& Kinect2::GetContext()
{
	if ( !gContext )
	{
		gContext.reset( new TContext() );
	}
	return *gContext;
}


Kinect2::TContext::TContext()
{
	mFreenect.reset( new libfreenect2::Freenect2() );
}


Kinect2::TContext::~TContext()
{
	//	may need to stop threads here
	mFreenect.reset();
}

void Kinect2::TContext::EnumDevices(std::function<void(const std::string&)>& Enum)
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



void Kinect2::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	auto& Context = GetContext();
	Context.EnumDevices( Enum );
}


Kinect2::TDevice::TDevice(const std::string& Serial)
{
	
}

void Kinect2::TDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	throw Soy_AssertException("Feature not supported");
}






