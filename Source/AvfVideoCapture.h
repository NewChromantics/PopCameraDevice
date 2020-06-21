#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>

#include "AvfPixelBuffer.h"


#if defined(__OBJC__)
@class VideoCaptureProxy;
@class DepthCaptureProxy;
#endif

namespace Avf
{
	class TCaptureParams;
}

namespace TVideoQuality
{
	enum Type
	{
		Low,
		Medium,
		High,
	};
};


class Avf::TCaptureParams
{
public:
	bool			mDiscardOldFrames = true;
	size_t			mFrameRate = 0;	//	0 = any
	SoyPixelsMeta	mPixelFormat;	//	invalid = any
};


#if defined(__OBJC__)
class AvfMediaExtractor
{
public:
	AvfMediaExtractor(std::shared_ptr<Opengl::TContext>& OpenglContext);
	
	void			OnSampleBuffer(CMSampleBufferRef SampleBufferRef,size_t StreamIndex,bool DoRetain);
	void			OnSampleBuffer(CVPixelBufferRef PixelBufferRef,SoyTime Timestamp,size_t StreamIndex,bool DoRetain);
	void			OnDepthFrame(AVDepthData* DepthData,CMTime Timestamp,size_t StreamIndex,bool DoRetain);

	std::shared_ptr<TMediaPacket>	PopPacket(size_t StreamIndex);
	
protected:
	TStreamMeta		GetFrameMeta(CMSampleBufferRef sampleBufferRef,size_t StreamIndex);
	TStreamMeta		GetFrameMeta(CVPixelBufferRef sampleBufferRef,size_t StreamIndex);
	void			QueuePacket(std::shared_ptr<TMediaPacket>& Packet);
	
public:
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<AvfDecoderRenderer>	mRenderer;	//	persistent rendering data

	std::function<void(const SoyTime,size_t)>	mOnPacketQueued;
	std::mutex								mPacketQueueLock;
	Array<std::shared_ptr<TMediaPacket>>	mPacketQueue;	//	extracted frames
};
#endif
	
#if defined(__OBJC__)
class AvfVideoCapture : public AvfMediaExtractor
{
public:
	friend class AVCaptureSessionWrapper;
	
public:
	AvfVideoCapture(const std::string& Serial,const Avf::TCaptureParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext);
	virtual ~AvfVideoCapture();

	//	configure funcs
	void 		SetFrameRate(size_t FramesPerSec);

	
protected:
	void					StartStream();
	void					StopStream();
	
private:
	void		Shutdown();
	void		CreateDevice(const std::string& Serial);
	void		CreateStream(const Avf::TCaptureParams& Params);
	
	void	CreateAndAddOutputDepth(AVCaptureSession* Session,const Avf::TCaptureParams& Params);
	void	CreateAndAddOutputColour(AVCaptureSession* Session,const Avf::TCaptureParams& Params);

	
public:
	ObjcPtr<AVCaptureDevice>			mDevice;
	ObjcPtr<AVCaptureSession>			mSession;
	ObjcPtr<VideoCaptureProxy>			mProxyColour;
	ObjcPtr<DepthCaptureProxy>			mProxyDepth;
	ObjcPtr<AVCaptureVideoDataOutput>	mOutputColour;
#if !defined(TARGET_OSX)
	ObjcPtr<AVCaptureDepthDataOutput>	mOutputDepth;
#endif
	dispatch_queue_t					mQueue = nullptr;
};
#endif
