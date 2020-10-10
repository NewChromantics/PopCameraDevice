#include "TestDevice.h"
#include "SoyLib/src/SoyMedia.h"
#include "PopCameraDevice.h"


TTestDeviceParams::TTestDeviceParams(json11::Json& Options)
{
	Read( Options, POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
	Read( Options, POPCAMERADEVICE_KEY_FRAMERATE, mFrameRate );
	Read( Options, "Width", mWidth );
	Read( Options, "Height", mHeight );
	Read( Options, POPCAMERADEVICE_KEY_DEBUG, mVerboseDebug );
};


void TestDevice::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	Enum(TestDevice::DeviceName);
}

TestDevice::TestDevice(json11::Json& Options) :
	mParams	( Options )
{
	GenerateFrame();

	auto Iteration = [this]()
	{
		this->GenerateFrame();
		
		auto Sleepf = 1000.0f / static_cast<float>(this->mParams.mFrameRate);
		auto Sleepms = static_cast<int>(Sleepf);
		std::this_thread::sleep_for( std::chrono::milliseconds(Sleepms));
		
		return this->mRunning;
	};

	mThread.reset( new SoyThreadLambda( std::string(DeviceName), Iteration ) );
}

TestDevice::~TestDevice()
{
	mRunning = false;
	if ( mThread )
	{
		mThread->Stop(true);
		mThread.reset();
	}
}

void TestDevice::GenerateFrame()
{
	std::shared_ptr<TPixelBuffer> pPixelBuffer(new TDumbPixelBuffer());
	auto& PixelBuffer = dynamic_cast<TDumbPixelBuffer&>(*pPixelBuffer);
	auto& Pixels = PixelBuffer.mPixels;

	SoyTime FrameTime = SoyTime::UpTime();

	//	set the type, alloc pixels, then fill the test planes
	Pixels.mMeta = SoyPixelsMeta(mParams.mWidth,mParams.mHeight,mParams.mColourFormat);
	Pixels.mArray.SetSize(Pixels.mMeta.GetDataSize());

	BufferArray<std::shared_ptr<SoyPixelsImpl>,3> Planes;
	Pixels.SplitPlanes(GetArrayBridge(Planes));

	BufferArray<uint8_t, 4> Components;
	Components.PushBack( mFrameNumber % 256 );
	Components.PushBack(0);
	Components.PushBack(255);
	Components.PushBack(255);
	for (auto p=0;	p<Planes.GetSize();	p++ )
	{
		auto& Plane = *Planes[p];
		Plane.SetPixels(GetArrayBridge(Components));
	}

	json11::Json::object Meta;
	Meta["Hello"] = "World";
	Meta["GeneratedFrameNumer"] = static_cast<int>(mFrameNumber);

	this->PushFrame(pPixelBuffer, FrameTime, Meta);
	mFrameNumber++;
}

void TestDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Test device doesn't support feature " << Feature;
	throw Soy::AssertException(Error.str());
}


