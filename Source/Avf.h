#pragma once

#include <functional>
#include <string>
#include "SoyLib/src/Array.hpp"
#include "SoyLib/src/HeapArray.hpp"
#include "SoyPixels.h"
#include "SoyFourcc.h"

#include <CoreVideo/CVPixelBuffer.h>

//	forward declare some classes that aren't in every sdk version,
//	plus for use in c++
#if defined(__OBJC__)
@class AVDepthData;
@class ARFrame;
@class ARSession;
@class ARDepthData;
@class ARAnchor;
#else
class AVDepthData;
class ARFrame;
class ARSession;
class ARDepthData;
class ARAnchor;
#endif


namespace Avf
{
	class TDeviceMeta;
	class TCaptureFormatMeta;
	
	void				EnumCaptureDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> Enum);
	void				EnumCaptureDevices(std::function<void(const TDeviceMeta&)> Enum);
	
	CVPixelBufferRef	GetDepthPixelBuffer(AVDepthData* DepthData,SoyPixelsFormat::Type OutputFormat=SoyPixelsFormat::DepthFloatMetres);

	std::ostream& operator<<(std::ostream& out,const TCaptureFormatMeta& in);
}


//	this can be generic
class Avf::TCaptureFormatMeta
{
public:
	size_t	mMaxFps = 0;
	size_t	mMinFps = 0;
	
	SoyPixelsMeta	mPixelMeta;
	Soy::TFourcc	mCodec;
};

//	this can probably be generic device meta...
class Avf::TDeviceMeta
{
public:
	//	unique identifier
	//	luckily, OSX makes all names unique with #2 etc on the end
	//	so for now the struct can
	std::string&		mCookie = mName;	//	unique identifier
	std::string			mName;
	std::string			mSerial;
	std::string			mVendor;
	bool				mIsConnected = false;
	bool				mIsSuspended = false;
	bool				mHasAudio = false;
	bool				mHasVideo = false;
	Array<TCaptureFormatMeta>	mFormats;
};
