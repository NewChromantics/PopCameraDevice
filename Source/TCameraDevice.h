#pragma once

#include <mutex>
#include <SoyPixels.h>
#include "Json11/json11.hpp"

class TPixelBuffer;


namespace PopCameraDevice
{
	class TDevice;
	class TStreamMeta;
	class TInvalidNameException;
	class TParams;
	
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

//	used as desired params when creating camera devices
//	todo: merge/cleanup soy TStreamMeta which has more info (eg. transform matrix)
class PopCameraDevice::TStreamMeta
{
public:
	SoyPixelsMeta	mPixelMeta;
	size_t			mFrameRate = 0;
};


//	move towards params with json. like Poph264
class PopCameraDevice::TParams
{
public:
	std::string		mSerial;
	std::string		mFormat;
	bool			mVerboseDebug = true;
};

class PopCameraDevice::TDevice
{
public:
	std::shared_ptr<TPixelBuffer>	GetNextFrame(SoyPixelsMeta& PixelMeta, SoyTime& FrameTime, std::string& FrameMeta,bool DeleteFrame);

	virtual void					EnableFeature(TFeature::Type Feature,bool Enable)=0;	//	throws if unsupported
	virtual void					ReadNativeHandle(void* Handle);

protected:
	virtual void					PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyPixelsMeta PixelMeta,SoyTime FrameTime,json11::Json::object FrameMeta);

public:
	Array<std::function<void()>>	mOnNewFrameCallbacks;

private:
	//	currently storing just last frame
	std::mutex						mLastPixelBufferLock;
	std::shared_ptr<TPixelBuffer>	mLastPixelBuffer;
	std::string						mLastFrameMeta;		//	maybe some key system later, but currently device outputs anything (ideally json)
	SoyPixelsMeta					mLastPixelsMeta;
	SoyTime							mLastFrameTime;
};

