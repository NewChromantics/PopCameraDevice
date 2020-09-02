#include "TCameraDevice.h"
#include <SoyMedia.h>
#include <magic_enum/include/magic_enum.hpp>
#include "PopCameraDevice.h"

std::string PopCameraDevice::GetFormatString(SoyPixelsMeta Meta, size_t FrameRate)
{
	std::stringstream Format;
	Format << Meta.GetFormat();
	Format << "^" << Meta.GetWidth();
	Format << "x" << Meta.GetHeight();

	if (FrameRate != 0)
		Format << "@" << FrameRate;

	return Format.str();
}

void PopCameraDevice::DecodeFormatString(std::string FormatString, SoyPixelsMeta& Meta, size_t& FrameRate)
{
	//	gr: regex would be good, but last time we tried, it's missing from android
	//	gr: maybe instead of popping, we should treat
	auto FormatName = Soy::StringPopUntil(FormatString, '^', false, false);
	auto WidthString = Soy::StringPopUntil(FormatString, 'x', false, false);
	auto HeightString = Soy::StringPopUntil(FormatString, '@', false, false);
	auto FrameRateString = FormatString;

	size_t Width = Meta.GetWidth();
	size_t Height = Meta.GetHeight();
	if (Soy::StringToUnsignedInteger(Width, WidthString))
		Meta.DumbSetWidth(Width);

	if ( Soy::StringToUnsignedInteger(Height, HeightString))
		Meta.DumbSetHeight(Height);

	Soy::StringToUnsignedInteger(FrameRate, FrameRateString);

	auto Format = magic_enum::enum_cast<SoyPixelsFormat::Type>(FormatName);
	if (Format.has_value())
		Meta.DumbSetFormat(*Format);
}

void PopCameraDevice::DecodeFormatString_UnitTests()
{
	auto Test = [&](const char* Format, SoyPixelsMeta Meta, size_t Rate, SoyPixelsMeta ExpectedMeta, size_t ExpectedFrameRate)
	{
		if (Meta == ExpectedMeta && Rate == ExpectedFrameRate)
			return;

		std::stringstream Error;
		Error << Format << " decoded to " << Meta << ", " << Rate;
		throw std::runtime_error(Error.str());
	};

	{
		auto Format = "RGBA^123x456@789";
		SoyPixelsMeta Meta;
		size_t FrameRate = 0;
		DecodeFormatString(Format, Meta, FrameRate);
		Test(Format, Meta, FrameRate, SoyPixelsMeta(123, 456, SoyPixelsFormat::RGBA), 789);
	}
}

PopCameraDevice::TDevice::TDevice(json11::Json& Params)
{
	if ( Params[POPCAMERADEVICE_KEY_SPLITPLANES].is_bool() )
		mSplitPlanes = Params[POPCAMERADEVICE_KEY_SPLITPLANES].bool_value();
}


void PopCameraDevice::TDevice::PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyTime FrameTime,json11::Json::object& FrameMeta)
{
	{
		Soy::TScopeTimerPrint Timer("PopCameraDevice::TDevice::PushFrame Lock",5);
		std::lock_guard<std::mutex> Lock(mFramesLock);

		std::shared_ptr<TFrame> pNewFrame( new TFrame );
		auto& NewFrame = *pNewFrame;
		NewFrame.mPixelBuffer = FramePixelBuffer;
		NewFrame.mMeta = FrameMeta;
		NewFrame.mFrameTime = FrameTime;
		mFrames.PushBack(pNewFrame);
		
		if (mFrames.GetSize() > mMaxFrameBuffers)
		{
			auto CullCount = mFrames.GetSize() - mMaxFrameBuffers;
			mCulledFrames += CullCount;
			mFrames.RemoveBlock(0,CullCount);
			std::Debug << __PRETTY_FUNCTION__ << "Culling " << CullCount << "/" << mFrames.GetSize() << " frames as over max " << mMaxFrameBuffers << " (total culled=" << mCulledFrames << ")" << std::endl;
		}
	}

	for (auto i = 0; i < mOnNewFrameCallbacks.GetSize(); i++)
	{
		try
		{
			Soy::TScopeTimerPrint Timer("PopCameraDevice::TDevice::PushFrame Callbacks",5);
			auto& Callback = mOnNewFrameCallbacks[i];
			Callback();
		}
		catch (std::exception& e)
		{
			std::Debug << "OnNewFrame callback exception; " << e.what() << std::endl;
		}
	}
}


void PopCameraDevice::TDevice::GetDeviceMeta(json11::Json::object& Meta)
{
	if (mCulledFrames > 0)
		Meta["CulledFrames"] = static_cast<int>(mCulledFrames);
}


bool PopCameraDevice::TDevice::GetNextFrame(TFrame& Frame,bool DeleteFrame)
{
	std::lock_guard<std::mutex> Lock(mFramesLock);
	if (mFrames.IsEmpty())
		return false;

	Frame = *(mFrames[0]);
	if (DeleteFrame)
	{
		mFrames.RemoveBlock(0, 1);
	}
	return true;
}

void PopCameraDevice::TDevice::ReadNativeHandle(void* Handle)
{
	throw Soy::AssertException("This device doesn't support ReadNativeHandle");
}
