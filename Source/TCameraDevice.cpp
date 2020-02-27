#include "TCameraDevice.h"
#include "SoyLib/src/SoyMedia.h"


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


void PopCameraDevice::TDevice::PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyPixelsMeta PixelMeta,SoyTime FrameTime,const std::string& FrameMeta)
{
	{
		std::lock_guard<std::mutex> Lock(mLastPixelBufferLock);
		mLastPixelBuffer = FramePixelBuffer;
		mLastPixelsMeta = PixelMeta;
		mLastFrameMeta = FrameMeta;
		mLastFrameTime = FrameTime;
	}

	if ( mOnNewFrame )
		mOnNewFrame();
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
