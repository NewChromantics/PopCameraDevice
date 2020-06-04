#include "AvfVideoCapture.h"
#import <CoreVideo/CoreVideo.h>
#import <AVFoundation/AVFoundation.h>
//#import <Accelerate/Accelerate.h>
#include "SoyDebug.h"
#include "SoyScope.h"
#include "SoyString.h"
#include "RemoteArray.h"
#include "SoyPixels.h"
#include "AvfPixelBuffer.h"
#include "SoyAvf.h"
#include "SoyOpenglContext.h"
#include "SoyOpenglContext.h"
#include "Avf.h"
#include "SoyFourcc.h"


namespace Avf
{
	TCaptureFormatMeta	GetMeta(AVCaptureDeviceFormat* Format);
	vec2f				GetMinMaxFrameRate(AVCaptureDeviceFormat* Format);
}



@class VideoCaptureProxy;
@class DepthCaptureProxy;



@interface VideoCaptureProxy : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
	AvfVideoCapture*	mParent;
	size_t				mStreamIndex;
}

- (id)initWithVideoCapturePrivate:(AvfVideoCapture*)parent;

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;

- (void)onVideoError:(NSNotification *)notification;

@end




@implementation VideoCaptureProxy

- (void)onVideoError:(NSNotification *)notification
{
	//	gr: handle this properly - got it when disconnecting USB hub
	/*
	Exception Name: NSInvalidArgumentException
	Description: -[NSError UTF8String]: unrecognized selector sent to instance 0x618000847350
	User Info: (null)
	*/
	try
	{
		NSString* Error = notification.userInfo[AVCaptureSessionErrorKey];
		auto ErrorStr = Soy::NSStringToString( Error );
		std::Debug << "Video error: "  << ErrorStr << std::endl;
	}
	catch(...)
	{
		std::Debug << "Some video error" << std::endl;
	}
}

- (id)initWithVideoCapturePrivate:(AvfVideoCapture*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
		mStreamIndex = 0;
	}
	return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	static bool DoRetain = true;
	mParent->OnSampleBuffer( sampleBuffer, mStreamIndex, DoRetain );
}


- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	std::Debug << "dropped sample" << std::endl;
}

@end



AvfVideoCapture::AvfVideoCapture(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext) :
	AvfMediaExtractor		( Params, OpenglContext ),
	mQueue					( nullptr ),
	mDiscardOldFrames		( Params.mDiscardOldFrames ),
	mForceNonPlanarOutput	( Params.mForceNonPlanarOutput )
{
	Run( Params.mFilename, TVideoQuality::High );
	StartStream();
}

AvfVideoCapture::~AvfVideoCapture()
{
	//	stop avf
	StopStream();
	
	//	call parent to clear existing data
	Shutdown();

	//	release queue and proxy from output
#if !defined(TARGET_OSX)
	if ( mOutputDepth )
	{
		[mOutputDepth setDelegate:nil callbackQueue:nil];
		mOutputDepth.Release();
	}
#endif
	
	if ( mOutputColour )
	{
		[mOutputColour setSampleBufferDelegate:nil queue: nil];
		mOutputColour.Release();
	}
	
	//	delete queue
	if ( mQueue )
	{
#if !defined(ARC_ENABLED)
		dispatch_release( mQueue );
#endif
		mQueue = nullptr;
	}
	
	mSession.Release();
	mProxyColour.Release();
	mProxyDepth.Release();
	
}



void AvfVideoCapture::Shutdown()
{
	//	gr: shutdown the parent (frames) first as the renderer will dangle and then get freed up in a frame
	//	todo: Better link between them so the CFPixelBuffer's never have ownership
	{
		//ReleaseFrames();
		std::lock_guard<std::mutex> Lock( mPacketQueueLock );
		mPacketQueue.Clear();
	}
	
	//	deffer all opengl shutdown to detach the renderer
	PopWorker::DeferDelete( mOpenglContext, mRenderer );
	
	//	no longer need the context
	mOpenglContext.reset();
}



NSString* GetAVCaptureSessionQuality(TVideoQuality::Type Quality)
{
	switch ( Quality )
	{
		case TVideoQuality::Low:
			return AVCaptureSessionPresetLow;
			
		case TVideoQuality::Medium:
			return AVCaptureSessionPresetMedium;
			
		case TVideoQuality::High:
			return AVCaptureSessionPresetHigh;
			
		default:
			Soy::Assert( false, "Unhandled TVideoQuality Type" );
			return AVCaptureSessionPresetHigh;
	}
}


void GetOutputFormats(AVCaptureVideoDataOutput* Output,ArrayBridge<Soy::TFourcc>&& Formats)
{
	auto EnumFormat = [&](NSNumber* FormatNumber)
	{
		auto FormatInt = [FormatNumber integerValue];
		OSType FormatOSType = static_cast<OSType>( FormatInt );
		Soy::TFourcc Fourcc(FormatOSType);
		Formats.PushBack(Fourcc);
	};

	NSArray<NSNumber*>* AvailibleFormats = [Output availableVideoCVPixelFormatTypes];
	Platform::NSArray_ForEach<NSNumber*>(AvailibleFormats,EnumFormat);
}


void GetOutputCodecs(AVCaptureVideoDataOutput* Output,ArrayBridge<std::string>&& Codecs)
{
	auto EnumCodec = [&](AVVideoCodecType CodecType)
	{
		auto CodecName = Soy::NSStringToString(CodecType);
		Codecs.PushBack(CodecName);
	};
	
	NSArray<AVVideoCodecType>* Availible = [Output availableVideoCodecTypes];
	Platform::NSArray_ForEach<AVVideoCodecType>(Availible,EnumCodec);
}

void AvfVideoCapture::CreateAndAddOutputDepth(AVCaptureSession* Session,SoyPixelsFormat::Type RequestedFormat)
{
#if defined(TARGET_OSX)
	throw Soy::AssertException("No depth output on osx");
#else
	mOutputDepth.Retain( [[AVCaptureDepthDataOutput alloc] init] );
	if ( !mOutputDepth )
		throw Soy::AssertException("Failed to allocate AVCaptureDepthDataOutput");

	auto& Output = mOutputDepth.mObject;
	//	gr: debug all these
	//Array<std::string> Codecs;
	//GetOutputCodecs(Output,GetArrayBridge(Codecs));
	//Array<Soy::TFourcc> Formats;
	//GetOutputFormats(Output,GetArrayBridge(Formats));
	
	bool KeepOldFrames = !mDiscardOldFrames;
	Output.alwaysDiscardsLateDepthData = KeepOldFrames ? NO : YES;
	/*
	Output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
							[NSNumber numberWithInt:Format], kCVPixelBufferPixelFormatTypeKey, nil];
	*/
	if ( ![Session canAddOutput:Output] )
		throw Soy::AssertException("Cannot add depth output");
	
	//	compatible, add
	[Session addOutput:Output];
	std::Debug << "Added depth output" << std::endl;

	//	todo:
	//auto& Proxy = mProxy.mObject;
	//[Output setDelegate:Proxy queue: mQueue];
#endif
}

void AvfVideoCapture::CreateAndAddOutputColour(AVCaptureSession* Session,SoyPixelsFormat::Type RequestedFormat)
{
	auto Serial = "SomeCamera";

	mOutputColour.Retain( [[AVCaptureVideoDataOutput alloc] init] );
	if ( !mOutputColour )
		throw Soy::AssertException("Failed to allocate AVCaptureVideoDataOutput");

	auto& Output = mOutputColour.mObject;
	
	//	todo: get output codecs
	//	gr: debug all these
	Array<std::string> Codecs;
	GetOutputCodecs(Output,GetArrayBridge(Codecs));
	Array<Soy::TFourcc> Formats;
	GetOutputFormats(Output,GetArrayBridge(Formats));

	NSArray<AVVideoCodecType>* AvailibleCodecs = [Output availableVideoCodecTypes];
	
	//	loop through formats for ones we can handle that are accepted
	//	https://developer.apple.com/library/mac/documentation/AVFoundation/Reference/AVCaptureVideoDataOutput_Class/#//apple_ref/occ/instp/AVCaptureVideoDataOutput/availableVideoCVPixelFormatTypes
	//	The first format in the returned list is the most efficient output format.
	Array<OSType> TryPixelFormats;
	{
		NSArray* AvailibleFormats = [Output availableVideoCVPixelFormatTypes];
		Soy::Assert( AvailibleFormats != nullptr, "availableVideoCVPixelFormatTypes returned null array" );
		
		//	filter pixel formats
		std::stringstream Debug;
		Debug << "Device " << Serial << " supports x" << AvailibleFormats.count << " formats: ";
		
		for (NSNumber* FormatCv in AvailibleFormats)
		{
			auto FormatInt = [FormatCv integerValue];
			OSType Format = static_cast<OSType>( FormatInt );
			
			auto FormatSoy = Avf::GetPixelFormat( Format );
			
			Debug << Avf::GetPixelFormatString( FormatCv ) << '/' << FormatSoy;
			
			if ( FormatSoy == SoyPixelsFormat::Invalid )
			{
				Debug << "(Not supported by soy), ";
				continue;
			}
			
			//	don't allow YUV formats
			if ( mForceNonPlanarOutput )
			{
				Array<SoyPixelsFormat::Type> Planes;
				SoyPixelsFormat::GetFormatPlanes( FormatSoy, GetArrayBridge(Planes) );
				if ( Planes.GetSize() > 1 )
				{
					Debug << "(Skipped due to planar format), ";
					continue;
				}
			}
			
			Debug << ", ";
			TryPixelFormats.PushBack( Format );
		}
		std::Debug << Debug.str() << std::endl;
	}
	
	bool KeepOldFrames = !mDiscardOldFrames;
	bool AddedOutput = false;
	for ( int i=0;	i<TryPixelFormats.GetSize();	i++ )
	{
		OSType Format = TryPixelFormats[i];
		
		auto PixelFormat = Avf::GetPixelFormat( Format );
		
		//	should have alreayd filtered this
		if ( PixelFormat == SoyPixelsFormat::Invalid )
		continue;
		
		Output.alwaysDiscardsLateVideoFrames = KeepOldFrames ? NO : YES;
		Output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								[NSNumber numberWithInt:Format], kCVPixelBufferPixelFormatTypeKey, nil];
		if ( ![Session canAddOutput:Output] )
		{
			std::Debug << "Device " << Serial << " does not support pixel format " << PixelFormat << " (despite claiming it does)" << std::endl;
			continue;
		}
		
		//	compatible, add
		[Session addOutput:Output];
		std::Debug << "Device " << Serial << " added " << PixelFormat << " output" << std::endl;
		AddedOutput = true;
		break;
	}
	
	if ( !AddedOutput )
	throw Soy::AssertException("Could not find compatible pixel format");
	
	auto& Proxy = mProxyColour.mObject;
	[Output setSampleBufferDelegate:Proxy queue: mQueue];
}

void AvfVideoCapture::Run(const std::string& Serial,TVideoQuality::Type DesiredQuality)
{
	NSError* error = nil;
	
	//	grab the device and get some meta
	
	// Find a suitable AVCaptureDevice
	auto SerialString = Soy::StringToNSString( Serial );
	mDevice.Retain( [AVCaptureDevice deviceWithUniqueID:SerialString] );
	if ( !mDevice )
	{
		std::stringstream Error;
		Error << "Failed to get AVCapture Device with serial " << Serial;
		throw Soy::AssertException( Error.str() );
	}
	auto& Device = mDevice.mObject;

	
	//	enum all frame rates
	{
		Array<Avf::TCaptureFormatMeta> Metas;
		auto EnumFormat = [&](AVCaptureDeviceFormat* Format)
		{
			auto Meta = Avf::GetMeta(Format);
			Metas.PushBack( Meta );
			std::Debug << "Camera format: " << Meta << std::endl;
		};
		Platform::NSArray_ForEach<AVCaptureDeviceFormat*>( [Device formats], EnumFormat );
	}
		
	//	make proxy
	mProxyColour.Retain( [[VideoCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	//mProxyDepth.Retain( [[DepthCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	
	
	

	//	make session
	mSession.Retain( [[AVCaptureSession alloc] init] );
	auto& Session = mSession.mObject;
	
	static bool MarkBeginConfig = false;
	if ( MarkBeginConfig )
		[Session beginConfiguration];

	

	
	
	
	
	//	try all the qualitys
	Array<TVideoQuality::Type> Qualitys;
	Qualitys.PushBack( DesiredQuality );
	Qualitys.PushBack( TVideoQuality::Low );
	Qualitys.PushBack( TVideoQuality::Medium );
	Qualitys.PushBack( TVideoQuality::High );
	
	for ( int i=0;	i<Qualitys.GetSize();	i++ )
	{
		auto Quality = Qualitys[i];
		auto QualityString = GetAVCaptureSessionQuality(Quality);
		//auto QualityString = AVCaptureSessionPreset1280x720;
		
		if ( ![mSession canSetSessionPreset:QualityString] )
			continue;

		Session.sessionPreset = QualityString;
		break;
	}

	if ( !Session.sessionPreset )
		throw Soy::AssertException("Failed to set session quality");
	
	
	
	
	AVCaptureDeviceInput* _input = [AVCaptureDeviceInput deviceInputWithDevice:Device error:&error];
	if ( !_input || ![Session canAddInput:_input])
	{
		throw Soy::AssertException("Cannot add AVCaptureDeviceInput");
	}
	[Session addInput:_input];
	
	
	
	//	register for notifications from errors
	auto& Proxy = mProxyColour.mObject;
	NSNotificationCenter *notify = [NSNotificationCenter defaultCenter];
	[notify addObserver: Proxy
			   selector: @selector(onVideoError:)
				   name: AVCaptureSessionRuntimeErrorNotification
				 object: Session];
	
	//	create a queue to handle callbacks
	//	gr: can this be used for the movie decoder? should this use a thread like the movie decoder?
	//	make our own queue, not the main queue
	//[_output setSampleBufferDelegate:_proxy queue:dispatch_get_main_queue()];
	if ( !mQueue )
		mQueue = dispatch_queue_create("AvfVideoCapture_Queue", NULL);
	
	

	
	
	//	split requested format into "planes" (colour & depth)
	//	and try and add their outputs
	auto RequestedOutputDepthFormat = SoyPixelsFormat::Depth16mm;
	auto RequestedOutputColourFormat = SoyPixelsFormat::RGBA;
	bool HasDepthFormat = true;
	bool HasColourFormat = true;
	
	if ( HasDepthFormat )
	{
		CreateAndAddOutputDepth(Session,RequestedOutputDepthFormat);
	}
	
	if ( HasColourFormat )
	{
		CreateAndAddOutputColour(Session,RequestedOutputColourFormat);
	}
	
/*
	[Session beginConfiguration];
	auto FrameRate = 60;
	try
	{
		SetFrameRate( FrameRate );
	}
	catch (std::exception& e)
	{
		std::Debug << "Failed to set frame rate to " << FrameRate << ": " << e.what() << std::endl;
	}
	[Session commitConfiguration];
*/
	
	if ( MarkBeginConfig )
		[Session commitConfiguration];
	
}

void AvfVideoCapture::StartStream()
{
	if ( !mSession )
	{
		std::Debug << "Warning: tried to " << __func__ << " with no session" << std::endl;
		return;
	}

	auto& Session = mSession.mObject;
	if ( !Session.running )
		[ Session startRunning];
	
	bool IsRunning = Session.running;
	Soy::Assert( IsRunning, "Failed tostart running") ;
}

void AvfVideoCapture::StopStream()
{
	if ( !mSession )
		return;

	auto& Session = mSession.mObject;
	if ( Session.running)
		[Session stopRunning];
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CMSampleBufferRef SampleBuffer,size_t StreamIndex)
{
	auto Desc = CMSampleBufferGetFormatDescription( SampleBuffer );
	auto Meta = Avf::GetStreamMeta( Desc );
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	
	return Meta;
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CVPixelBufferRef SampleBuffer,size_t StreamIndex)
{
	TStreamMeta Meta;
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);

	return Meta;
}


std::shared_ptr<TMediaPacket> AvfMediaExtractor::PopPacket(size_t StreamIndex)
{
	if ( mPacketQueue.IsEmpty() )
		return nullptr;

	//	todo: filter stream indexes!
	std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	auto Oldest = mPacketQueue.PopAt(0);

	return Oldest;
}


void AvfMediaExtractor::QueuePacket(std::shared_ptr<TMediaPacket>& Packet)
{
	if ( !Packet )
		throw Soy::AssertException("Packet expected");
	
	{
		std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	
		//	only save latest
		//	gr: check in case this causes too much stuttering and maybe keep 2
		//	todo: filter stream indexes!
		if ( mDiscardOldFrames )
			mPacketQueue.Clear();

		mPacketQueue.PushBack( Packet );
	}

	//	callback AFTER packet is queued
	if ( mOnPacketQueued )
		mOnPacketQueued( Packet->mTimecode, Packet->mMeta.mStreamIndex );
}


AvfMediaExtractor::AvfMediaExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	mOpenglContext		( OpenglContext ),
	mRenderer			( new AvfDecoderRenderer() ),
	mDiscardOldFrames	( Params.mDiscardOldFrames )
{
	
}

void AvfMediaExtractor::OnSampleBuffer(CMSampleBufferRef sampleBufferRef,size_t StreamIndex,bool DoRetain)
{
	//	gr: I think stalling too long here can make USB bus crash (killing bluetooth, hid, audio etc)
	Soy::TScopeTimerPrint Timer("AvfMediaExtractor::OnSampleBuffer",5);
	
	//Soy::Assert( sampleBufferRef != nullptr, "Expected sample buffer ref");
	if ( !sampleBufferRef )
		return;
	
	//	callback on another thread, so need to catch exceptions
	try
	{
		CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
		SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
		
		auto pPacket = std::make_shared<TMediaPacket>();
		auto& Packet = *pPacket;
		Packet.mMeta = GetFrameMeta( sampleBufferRef, StreamIndex );
		Packet.mTimecode = Timestamp;
		Packet.mPixelBuffer = std::make_shared<CFPixelBuffer>( sampleBufferRef, DoRetain, mRenderer, Packet.mMeta.mTransform );
		
		QueuePacket( pPacket );
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " exception; " << e.what() << std::endl;
		CFPixelBuffer StackRelease(sampleBufferRef,false,mRenderer,float3x3());
		return;
	}
}


void AvfMediaExtractor::OnSampleBuffer(CVPixelBufferRef sampleBufferRef,SoyTime Timestamp,size_t StreamIndex,bool DoRetain)
{
	if ( !sampleBufferRef )
		return;
	
	//	callback on another thread, so need to catch exceptions
	try
	{
		auto pPacket = std::make_shared<TMediaPacket>();
		auto& Packet = *pPacket;
		Packet.mMeta = GetFrameMeta( sampleBufferRef, StreamIndex );
		Packet.mTimecode = Timestamp;
		Packet.mPixelBuffer = std::make_shared<CVPixelBuffer>( sampleBufferRef, DoRetain, mRenderer, Packet.mMeta.mTransform );
		
		QueuePacket( pPacket );
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " exception; " << e.what() << std::endl;
		CVPixelBuffer StackRelease(sampleBufferRef,false,mRenderer,float3x3());
		return;
	}

}



void AvfVideoCapture::SetFrameRate(size_t FramesPerSec)
{
	if ( !mDevice )
		throw Soy::AssertException("Cannot set frame rate without device");

	auto* Device = mDevice.mObject;

	//	get current format range
	auto ActiveFormat = [Device activeFormat];
	if ( ActiveFormat )
	{
		auto MinMax = Avf::GetMinMaxFrameRate(ActiveFormat);
		std::Debug << "Camera fps range " << MinMax.x << "..." << MinMax.y << std::endl;
	}

	if ( NO == [Device lockForConfiguration:NULL] )
		throw Soy::AssertException("Could not lock device for configuration");

	@try
	{
		auto Scalar = 10;
		auto FramesPerSecInt = size_cast<int>(FramesPerSec);
		auto FrameDuration = CMTimeMake(Scalar,FramesPerSecInt*Scalar);
		[Device setActiveVideoMinFrameDuration:FrameDuration];
		[Device setActiveVideoMaxFrameDuration:FrameDuration];
	}
	@catch(NSException* e)
	{
		[Device unlockForConfiguration];
		throw Soy::AssertException( Soy::NSErrorToString(e) );
	}

}

/*
bool TVideoDevice_AvFoundation::GetOption(TVideoOption::Type Option,bool Default)
{
	Soy_AssertTodo();
	return Default;
}


bool TVideoDevice_AvFoundation::SetOption(TVideoOption::Type Option, bool Enable)
{
	auto* device = mWrapper ? mWrapper->mDevice : nullptr;
	if ( !device )
		return false;

	if ( !BeginConfiguration() )
		return false;

	//	gr: is this needed???
	//if (([device hasMediaType:AVMediaTypeVideo]) && ([device position] == AVCaptureDevicePositionBack))

	NSError* error = nil;
	[device lockForConfiguration:&error];
	
	bool Supported = false;
	if ( !error )
	{
		switch ( Option )
		{
			case TVideoOption::LockedExposure:
				Supported = setExposureLocked( Enable );
				break;
			
			case TVideoOption::LockedFocus:
				Supported = setFocusLocked( Enable );
				break;
			
			case TVideoOption::LockedWhiteBalance:
				Supported = setWhiteBalanceLocked( Enable );
				break;
				
			default:
				std::Debug << "tried to set video device " << GetSerial() << " unknown option " << Option << std::endl;
				Supported = false;
				break;
		}
	}
	
	//	gr: dont unlock if error?
	[device unlockForConfiguration];
	
	EndConfiguration();

	return Supported;
}

bool TVideoDevice_AvFoundation::setFocusLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isFocusModeSupported:AVCaptureFocusModeLocked] )
			return false;
		
		device.focusMode = AVCaptureFocusModeLocked;
		return true;
	}
	else
	{
		if ( ![device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
			return false;
		
		device.focusMode = AVCaptureFocusModeContinuousAutoFocus;
		return true;
	}
}


bool TVideoDevice_AvFoundation::setWhiteBalanceLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked] )
			return false;
		
		device.whiteBalanceMode = AVCaptureWhiteBalanceModeLocked;
		return true;
	}
	else
	{
		if ( ![device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance])
			return false;
		
		device.whiteBalanceMode = AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance;
		return true;
	}
}



bool TVideoDevice_AvFoundation::setExposureLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isExposureModeSupported:AVCaptureExposureModeLocked] )
			return false;
		
		device.exposureMode = AVCaptureExposureModeLocked;
		return true;
	}
	else
	{
		if ( ![device isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure])
			return false;
		
		device.exposureMode = AVCaptureExposureModeContinuousAutoExposure;
		return true;
	}
}

bool TVideoDevice_AvFoundation::BeginConfiguration()
{
	auto* Session = mWrapper ? mWrapper->_session : nullptr;
	if ( !Soy::Assert( Session, "Expected session") )
		return false;
	
	if ( mConfigurationStackCounter == 0)
		[Session beginConfiguration];
	
	mConfigurationStackCounter++;
	return true;
}

bool TVideoDevice_AvFoundation::EndConfiguration()
{
	auto* Session = mWrapper ? mWrapper->_session : nullptr;
	if ( !Soy::Assert( Session, "Expected session") )
		return false;
	mConfigurationStackCounter--;
	
	if (mConfigurationStackCounter == 0)
		[Session commitConfiguration];
	return true;
}

TVideoDeviceMeta TVideoDevice_AvFoundation::GetMeta() const
{
	if ( !mWrapper )
		return TVideoDeviceMeta();
	
	return GetDeviceMeta( mWrapper->mDevice );
}


void SoyVideoContainer_AvFoundation::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	TVideoDevice_AvFoundation::GetDevices( Metas );
}


 */


