#pragma once

#include <memory>
#include "SoyLib/src/Array.hpp"
#include "SoyLib/src/SoyPixels.h"


#if defined(_MSC_VER) && !defined(TARGET_PS4)
	#define __export_c			extern "C" __declspec(dllexport)
#else
	#define __export_c			extern "C"
#endif


class SoyPixelsImpl;
namespace Broadway
{
	class TDecoder;
}

namespace PopH264
{

}

class TFrame
{
public:
	std::shared_ptr<SoyPixelsImpl>	mPixels;
	int32_t							mFrameNumber = -1;	//	this may be time, specified by user, so is really just Meta
	std::chrono::milliseconds		mDecodeDuration;	//	time the last packet that resulted in this picture took to decode
};

class PopH264::TDecoderInstance
{
public:
	TDecoderInstance();
	
	//	input
	void									PushData(const uint8_t* Data,size_t DataSize,int32_t FrameNumber);
	
	//	output
	void									PopFrame(int32_t& FrameNumber,ArrayBridge<uint8_t>&& Plane0,ArrayBridge<uint8_t>&& Plane1,ArrayBridge<uint8_t>&& Plane2);
	bool									PopFrame(TFrame& Frame);
	void									PushFrame(const SoyPixelsImpl& Frame,int32_t FrameNumber,std::chrono::milliseconds DecodeDurationMs);
	const SoyPixelsMeta&					GetMeta() const	{	return mMeta;	}
	
public:
	std::function<void()>					mOnNewFrame;	//	called when a new frame is pushed
	
private:
	std::shared_ptr<Broadway::TDecoder>		mDecoder;
	std::mutex								mFramesLock;
	Array<TFrame>							mFrames;
	SoyPixelsMeta							mMeta;
};
