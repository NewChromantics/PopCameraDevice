#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>

#include "AvfPixelBuffer.h"


#if defined(__OBJC__)
@class VideoCaptureProxy;
#endif


namespace TVideoQuality
{
	enum Type
	{
		Low,
		Medium,
		High,
	};
};



#if defined(__OBJC__)
class AvfMediaExtractor
{
public:
	AvfMediaExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext);
	
	void			OnSampleBuffer(CMSampleBufferRef SampleBufferRef,size_t StreamIndex,bool DoRetain);
	void			OnSampleBuffer(CVPixelBufferRef PixelBufferRef,SoyTime Timestamp,size_t StreamIndex,bool DoRetain);

	std::shared_ptr<TMediaPacket>	PopPacket(size_t StreamIndex);
	
protected:
	TStreamMeta		GetFrameMeta(CMSampleBufferRef sampleBufferRef,size_t StreamIndex);
	TStreamMeta		GetFrameMeta(CVPixelBufferRef sampleBufferRef,size_t StreamIndex);
	void			QueuePacket(std::shared_ptr<TMediaPacket>& Packet);
	
public:
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<AvfDecoderRenderer>	mRenderer;	//	persistent rendering data

	bool			mDiscardOldFrames = false;
	
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
	AvfVideoCapture(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext);
	virtual ~AvfVideoCapture();

	//	configure funcs
	void 		SetFrameRate(size_t FramesPerSec);

	
protected:
	void					StartStream();
	void					StopStream();
	
private:
	void		Shutdown();
	void		Run(const std::string& Serial,TVideoQuality::Type Quality,bool KeepOldFrames);
	
public:
	ObjcPtr<AVCaptureDevice>			mDevice;
	ObjcPtr<AVCaptureSession>			mSession;
	ObjcPtr<VideoCaptureProxy>			mProxy;
	ObjcPtr<AVCaptureVideoDataOutput>	mOutput;
	dispatch_queue_t					mQueue;
	bool								mDiscardOldFrames;
	bool								mForceNonPlanarOutput;

};
#endif