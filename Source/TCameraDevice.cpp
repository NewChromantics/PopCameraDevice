#include "TCameraDevice.h"
#include <SoyMedia.h>
#include <magic_enum/include/magic_enum.hpp>


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


void PopCameraDevice::TDevice::PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyPixelsMeta PixelMeta,SoyTime FrameTime,const std::string& FrameMeta)
{
	{
		Soy::TScopeTimerPrint Timer("PopCameraDevice::TDevice::PushFrame Lock",5);
		std::lock_guard<std::mutex> Lock(mLastPixelBufferLock);
		mLastPixelBuffer = FramePixelBuffer;
		mLastPixelsMeta = PixelMeta;
		mLastFrameMeta = FrameMeta;
		mLastFrameTime = FrameTime;
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



std::shared_ptr<TPixelBuffer> PopCameraDevice::TDevice::GetNextFrame(SoyPixelsMeta& PixelMeta, SoyTime& FrameTime, std::string& FrameMeta, bool DeleteFrame)
{
	std::lock_guard<std::mutex> Lock(mLastPixelBufferLock);
	auto PixelBuffer = mLastPixelBuffer;
	PixelMeta = mLastPixelsMeta;
	FrameTime = mLastFrameTime;
	FrameMeta = mLastFrameMeta;

	if (DeleteFrame)
	{
		mLastFrameMeta.clear();
		mLastPixelBuffer.reset();
		mLastFrameTime = SoyTime();
		FrameMeta = std::string();
	}

	return PixelBuffer;

}

void PopCameraDevice::TDevice::ReadNativeHandle(void* Handle)
{
	throw Soy::AssertException("This device doesn't support ReadNativeHandle");
}
