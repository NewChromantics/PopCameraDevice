#pragma once

#include <mutex>
#include <SoyPixels.h>
#include "Json11/json11.hpp"

class TPixelBuffer;


namespace PopCameraDevice
{
	class TDevice;
	class TInvalidNameException;
	class TParams;
	class TFrame;
	
	std::string	GetFormatString(SoyPixelsMeta Meta, size_t FrameRate = 0);
	void		DecodeFormatString(std::string FormatString, SoyPixelsMeta& Meta, size_t& FrameRate);
	void		DecodeFormatString_UnitTests();
	void		ReadNativeHandle(int32_t Instance,void* Handle);

	//	these features are currently all on/off.
	//	but some cameras have options like ISO levels, which we should allow specific numbers of
	namespace TFeature
	{
		enum Type
		{
			AutoFocus,
			AutoWhiteBalance,
		};
	}
}


class PopCameraDevice::TInvalidNameException : public std::exception
{
public:
	virtual const char* what() const __noexcept { return "Invalid device name"; }
};


//	move towards params with json. like Poph264
class PopCameraDevice::TParams
{
public:
	std::string		mSerial;
	std::string		mFormat;
	bool			mVerboseDebug = true;
};

class PopCameraDevice::TFrame
{
public:
	json11::Json::object			mMeta;
	SoyTime							mFrameTime;
	std::shared_ptr<TPixelBuffer>	mPixelBuffer;
};

class PopCameraDevice::TDevice
{
public:
	bool							GetNextFrame(TFrame& Frame,bool DeleteFrame);

	virtual void					EnableFeature(TFeature::Type Feature,bool Enable)=0;	//	throws if unsupported
	virtual void					ReadNativeHandle(void* Handle);
	
	//	get additional meta for output (debug mostly?)
	virtual void					GetDeviceMeta(json11::Json::object& Meta);

protected:
	virtual void					PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyTime FrameTime,json11::Json::object& FrameMeta);

public:
	Array<std::function<void()>>	mOnNewFrameCallbacks;

private:
	size_t			mCulledFrames = 0;	//	debug - running total of culled frames
	std::mutex		mFramesLock;
	Array<TFrame>	mFrames;		//	might be expensive to copy atm
	size_t			mMaxFrameBuffers = 10;
};

