#pragma once

#include <mutex>
#include <SoyPixels.h>
#include "Json11/json11.hpp"

class TPixelBuffer;


namespace PopCameraDevice
{
	class TDevice;
	class TCaptureParams;
	class TInvalidNameException;
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
class PopCameraDevice::TCaptureParams
{
public:
	bool			Read(json11::Json& Options,const char* Name,size_t& Value);
	bool			Read(json11::Json& Options,const char* Name,bool& Value);
	bool			Read(json11::Json& Options,const char* Name,std::string& Value);
	bool			Read(json11::Json& Options,const char* Name,SoyPixelsFormat::Type& Value);

	//	include some params they all use here
	//std::string		mSerial;
	bool			mVerboseDebug = true;	
};


class PopCameraDevice::TFrame
{
public:
	//	on nvidia/linux, this seems to have some problems (seg fault) being copied
	std::string						mMeta;
	SoyTime							mFrameTime;
	std::shared_ptr<TPixelBuffer>	mPixelBuffer;

	json11::Json::object			GetMetaJson();
};

class PopCameraDevice::TDevice
{
public:
	TDevice(json11::Json& Params);
	TDevice()	{};
	
	bool							GetNextFrame(TFrame& Frame,bool DeleteFrame);

	virtual void					EnableFeature(TFeature::Type Feature,bool Enable)=0;	//	throws if unsupported
	virtual void					ReadNativeHandle(void* Handle);
	
	//	get additional meta for output (debug mostly?)
	virtual void					GetDeviceMeta(json11::Json::object& Meta);

protected:
	virtual void					PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyTime FrameTime,json11::Json::object& FrameMeta);

public:
	Array<std::function<void()>>	mOnNewFrameCallbacks;

public:
	//	some generic properties from params
	bool			mSplitPlanes = true;

private:
	size_t			mCulledFrames = 0;	//	debug - running total of culled frames
	std::mutex		mFramesLock;
	Array<std::shared_ptr<TFrame>>	mFrames;		//	might be expensive to copy atm
	size_t			mMaxFrameBuffers = 13;
};

