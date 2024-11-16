#pragma once

#include "TCameraDevice.h"
#include "SoyMedia.h"

//  OSX
//  to install libusb and libfreenect, get brew from http://brew.sh/
//  /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
//  brew install libfreenect
//  usually need to restart xcode to recognise new source tree entries
//#include "libusb.h"
//#include "libfreenect.h"

namespace Freenect
{
	class TSource;
	class TCaptureParams;
	class TFrameListener;
	
	void			EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&& Formats)> Enum);
	void			Shutdown(bool ProcessExit);
}


class Freenect::TCaptureParams : public PopCameraDevice::TCaptureParams
{
public:
	TCaptureParams(json11::Json& Options);

	SoyPixelsFormat::Type	mColourFormat = SoyPixelsFormat::Invalid;
	SoyPixelsFormat::Type	mDepthFormat = SoyPixelsFormat::Invalid;
};

class Freenect::TSource : public PopCameraDevice::TDevice
{
public:
	TSource(std::string Serial,json11::Json& Params);
	~TSource();
	
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
	
private:
	void			OnFrame(const SoyPixelsImpl& Frame,SoyTime Timestamp);
	
private:
	std::shared_ptr<TFrameListener>	mListener;
};




/*

class Kinect::TDeviceMeta
{
public:
	TDeviceMeta()	{}
	TDeviceMeta(struct freenect_device_attributes& Device);
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}
	bool				operator==(const TDeviceMeta& That) const	{	return mName == That.mName;	}
	
public:
	std::string			mName;
	
	Array<TStreamMeta>	mStreams;
};



class Kinect::TFrame
{
public:
	TFrame(SoyPixelsImpl& Pixels,SoyTime Timecode,size_t StreamIndex) :
		mPixels			( Pixels ),
		mTimecode		( Timecode ),
		mStreamIndex	( StreamIndex )
	{
	}
	
public:
	SoyPixelsImpl&	mPixels;
	SoyTime			mTimecode;
	size_t			mStreamIndex;
};

class Kinect::TStream
{
public:
#if defined(LIBFREENECT)
	TStream(freenect_device& Device,size_t StreamIndex);
#endif	
	
	const TStreamMeta&		GetStreamMeta() const	{	return mMeta;	}
	
public:
	TStreamMeta				mMeta;
#if defined(LIBFREENECT)
	freenect_device*		mDevice;
	freenect_frame_mode		mFrameMode;
#endif
};

class Kinect::TVideoStream : public TStream
{
public:
#if defined(LIBFREENECT)
	TVideoStream(freenect_device& Device,SoyPixelsFormat::Type Format,size_t StreamIndex);
#endif
	~TVideoStream();
};


class Kinect::TDepthStream : public TStream
{
public:
#if defined(LIBFREENECT)
	TDepthStream(freenect_device& Device,SoyPixelsFormat::Type Format,size_t StreamIndex);
#endif
	~TDepthStream();
};

class Kinect::TDevice
{
public:
	TDevice(const std::string& Serial);
	//TDevice(const TDeviceMeta& Meta,const TVideoDecoderParams& Params,TContext& Context);
	~TDevice();
	
#if defined(LIBFREENECT)
	void					AcquireDevice();
	void					CreateStreams(bool Video,bool Depth);
	void					ReleaseDevice();
#endif
	
	std::shared_ptr<TVideoStream>	GetVideoStream()	{	return mVideoStream;	}
	std::shared_ptr<TDepthStream>	GetDepthStream()	{	return mDepthStream;	}
	
	void					GetStreamMeta(ArrayBridge<TStreamMeta>& StreamMetas);
	bool					HasFatalError(std::string& Error)			{	Error = mFatalError;	return !mFatalError.empty();	}
	
	bool					operator==(const uint32& DeviceId) const	{	return mDeviceId == DeviceId;	}
	
public:
	TDeviceMeta				mMeta;
	std::string				mFatalError;
	
	std::function<void(const TFrame&)>	mOnNewFrame;
	
private:
	std::shared_ptr<TVideoStream>	mVideoStream;
	std::shared_ptr<TDepthStream>	mDepthStream;
	uint32							mDeviceId;
	TContext&						mContext;
#if defined(LIBFREENECT)
	freenect_device*				mDevice;
	void*							mDeviceOldUserData;
#endif
};



/*
//	gr: now we have multiple stream support, we can revert this back to 1:1 device
class Kinect::TDeviceDecoder : public TVideoDecoder
{
public:
	TDeviceDecoder(const TVideoDecoderParams& Params,std::map<size_t,std::shared_ptr<TPixelBufferManager>>& PixelBufferManagers,std::map<size_t,std::shared_ptr<TAudioBufferManager>>& AudioBufferManagers,std::shared_ptr<Opengl::TContext> OpenglContext);
	~TDeviceDecoder();
	
	virtual void			GetStreamMeta(ArrayBridge<TStreamMeta>&& StreamMetas) override;
	virtual TVideoMeta		GetMeta() override;
	virtual bool			HasFatalError(std::string& Error) override;

protected:
	void					OnNewFrame(const TFrame& Frame);
	
public:
	SoyListenerId				mDeviceOnNewFrameListener;
	std::shared_ptr<TContext>	mContext;
	std::shared_ptr<TDevice>	mDevice;	//	device, also need link to subdevice! or just specify stream[s] we're using?

	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<Opengl::TBlitter>	mOpenglBlitter;
};



class Kinect::TDepthPixelBuffer : public TPixelBuffer
{
public:
	TDepthPixelBuffer(const SoyPixelsImpl& Pixels);
	~TDepthPixelBuffer();
	
	virtual void		Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context,float3x3& Transform) override;
	virtual void		Lock(ArrayBridge<Directx::TTexture>&& Textures,Directx::TContext& Context,float3x3& Transform) override	{};
	virtual void		Lock(ArrayBridge<Metal::TTexture>&& Textures,Metal::TContext& Context,float3x3& Transform) override	{};
	virtual void		Lock(ArrayBridge<SoyPixelsImpl*>&& Textures,float3x3& Transform) override;
	virtual void		Unlock() override;

public:
	std::shared_ptr<Opengl::TTexture>	mLockedTexture;		//	output texture
	SoyPixels							mPixels;
};

 
 class Kinect::TDevice : public PopCameraDevice::TDevice
 {
 public:
 TDevice(const std::string& Serial);
 
 virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
 };
 
 */





