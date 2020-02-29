#include "TestDevice.h"
#include "SoyLib/src/SoyMedia.h"



void TestDevice::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	Enum(TestDevice::DeviceName);
}

TestDevice::TestDevice(const std::string& Format)
{
	//if (!Soy::StringBeginsWith(Serial, TestDevice::DeviceName, true))
	//	throw PopCameraDevice::TInvalidNameException();

	PopCameraDevice::DecodeFormatString(Format, mMeta, mFrameRate);

	GenerateFrame();

	//	todo: start a thread!
}


void TestDevice::GenerateFrame()
{
	std::shared_ptr<TPixelBuffer> pPixelBuffer(new TDumbPixelBuffer());
	auto& PixelBuffer = dynamic_cast<TDumbPixelBuffer&>(*pPixelBuffer);
	auto& Pixels = PixelBuffer.mPixels;

	SoyTime FrameTime(true);

	//	set the type, alloc pixels, then fill the test planes
	Pixels.mMeta = mMeta;
	Pixels.mArray.SetSize(Pixels.mMeta.GetDataSize());

	BufferArray<std::shared_ptr<SoyPixelsImpl>,3> Planes;
	Pixels.SplitPlanes(GetArrayBridge(Planes));

	BufferArray<uint8_t, 4> Components;
	Components.PushBack(128);
	Components.PushBack(0);
	Components.PushBack(255);
	Components.PushBack(255);
	for (auto p=0;	p<Planes.GetSize();	p++ )
	{
		auto& Plane = *Planes[p];
		Plane.SetPixels(GetArrayBridge(Components));
	}

	this->PushFrame(pPixelBuffer, Pixels.mMeta, FrameTime, std::string() );
}

void TestDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Test device doesn't support feature " << Feature;
	throw Soy::AssertException(Error.str());
}


