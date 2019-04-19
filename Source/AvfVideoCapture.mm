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



//	this can be generic
class TCaptureFormatMeta
{
public:
	size_t	mMaxFps = 0;
	size_t	mMinFps = 0;
	
	SoyPixelsMeta	mPixelMeta;
	std::string	mCodec;
};


namespace Avf
{
	TCaptureFormatMeta GetMeta(AVCaptureDeviceFormat* Format);
}





vec2f GetMinMaxFrameRate(AVCaptureDeviceFormat* Format)
{
	if ( !Format )
		throw Soy::AssertException("GetMinMaxFrameRate(): Format null");
	
	//	these are in pairs, but seems okay to just get min & max of both
	Array<float> MinFrameRates;
	Array<float> MaxFrameRates;
	auto* FrameRateRanges = Format.videoSupportedFrameRateRanges;
	auto EnumRange = [&](AVFrameRateRange* Range)
	{
		MinFrameRates.PushBack(Range.maxFrameRate);
		MaxFrameRates.PushBack(Range.minFrameRate);
	};
	Platform::NSArray_ForEach<AVFrameRateRange*>(FrameRateRanges,EnumRange);
	
	if ( MinFrameRates.IsEmpty() )
		throw Soy::AssertException("Got no frame rates from camera format");
	
	//std::Debug << "Camera fps min: " << Soy::StringJoin(GetArrayBridge(MinFrameRates),",") << " max: " << Soy::StringJoin(GetArrayBridge(MaxFrameRates),",") << std::endl;
	
	float Min = MinFrameRates[0];
	float Max = MaxFrameRates[0];
	for ( auto i=0;	i<MinFrameRates.GetSize();	i++ )
	{
		Min = std::min( Min, MinFrameRates[i] );
		Max = std::max( Max, MaxFrameRates[i] );
	}
	
	return vec2f( Min, Max );
}



@class VideoCaptureProxy;



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
	bool KeepOldFrames = !mDiscardOldFrames;
	Run( Params.mFilename, TVideoQuality::High, KeepOldFrames );
	StartStream();
}

AvfVideoCapture::~AvfVideoCapture()
{
	//	stop avf
	StopStream();
	
	//	wait for everything to release
	TMediaExtractor::WaitToFinish();
	
	//	call parent to clear existing data
	Shutdown();

	//	release queue and proxy from output
	if ( mOutput )
	{
		//[mOutput setSampleBufferDelegate:mProxy queue: nil];
		[mOutput setSampleBufferDelegate:nil queue: nil];
		mOutput.Release();
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
	mProxy.Release();
	
	
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



//	gr: make this generic across platforms!
class TSerialMatch
{
public:
	TSerialMatch() :
		mSerial				( "<invalid TSerialMatch>" ),
		mNameExact			( false ),
		mNameStarts			( false ),
		mNameContains		( false ),
		mVendorExact		( false ),
		mVendorStarts		( false ),
		mVendorContains		( false ),
		mSerialExact		( false ),
		mSerialStarts		( false ),
		mSerialContains		( false )
	{
	}
	
	TSerialMatch(const std::string& PartSerial,const std::string& Serial,const std::string& Name,const std::string& Vendor) :
		mSerial				( Serial ),
		mNameExact			( Soy::StringMatches( Name, PartSerial, false ) ),
		mNameStarts			( Soy::StringBeginsWith( Name, PartSerial, false ) ),
		mNameContains		( Soy::StringContains( Name, PartSerial, false ) ),
		mVendorExact		( Soy::StringMatches( Vendor, PartSerial, false ) ),
		mVendorStarts		( Soy::StringBeginsWith( Vendor, PartSerial, false ) ),
		mVendorContains		( Soy::StringContains( Vendor, PartSerial, false ) ),
		mSerialExact		( Soy::StringMatches( Serial, PartSerial, false ) ),
		mSerialStarts		( Soy::StringBeginsWith( Serial, PartSerial, false ) ),
		mSerialContains		( Soy::StringContains( Serial, PartSerial, false ) )
	{
		//	allow matching all serials
		if ( PartSerial == "*" )
			mNameContains = true;
	}
	
	size_t		GetScore()
	{
		size_t Score = 0;
		//	shift = priority
		Score |= mSerialExact << 9;
		Score |= mNameExact << 8;

		Score |= mSerialStarts << 7;
		Score |= mNameStarts << 6;
		Score |= mVendorExact << 5;
	
		Score |= mSerialContains << 4;
		Score |= mNameContains << 3;
		Score |= mVendorStarts << 2;
		Score |= mVendorContains << 1;
		return Score;
	}

	std::string	mSerial;
	bool		mNameExact;
	bool		mNameStarts;
	bool		mNameContains;
	bool		mVendorExact;
	bool		mVendorStarts;
	bool		mVendorContains;
	bool		mSerialExact;
	bool		mSerialStarts;
	bool		mSerialContains;
};



std::ostream& operator<<(std::ostream& out,const TCaptureFormatMeta& in)
{
	out << "MaxSize=" << in.mPixelMeta << ", ";
	out << "Fps=" << in.mMinFps << "..." << in.mMaxFps << ", ";
	
	return out;
}


TCaptureFormatMeta Avf::GetMeta(AVCaptureDeviceFormat* Format)
{
	if ( !Format )
		throw Soy::AssertException("GetMeta AVCaptureDeviceFormat null format");
	
	auto FrameRateRange = GetMinMaxFrameRate( Format );

	//	lots of work already in here
	auto StreamMeta = Avf::GetStreamMeta( Format.formatDescription );

	
	TCaptureFormatMeta Meta;
	Meta.mMinFps = FrameRateRange.x;
	Meta.mMaxFps = FrameRateRange.y;
	Meta.mPixelMeta = StreamMeta.mPixelMeta;
	
	return Meta;
}


void AvfVideoCapture::Run(const std::string& Serial,TVideoQuality::Type DesiredQuality,bool KeepOldFrames)
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
		Array<TCaptureFormatMeta> Metas;
		auto EnumFormat = [&](AVCaptureDeviceFormat* Format)
		{
			auto Meta = Avf::GetMeta(Format);
			Metas.PushBack( Meta );
			std::Debug << "Camera format: " << Meta << std::endl;
		};
		Platform::NSArray_ForEach<AVCaptureDeviceFormat*>( [Device formats], EnumFormat );
	}
		
	//	make proxy
	mProxy.Retain( [[VideoCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	
	
	

	//	make session
	mSession.Retain( [[AVCaptureSession alloc] init] );
	auto& Session = mSession.mObject;
	auto& Proxy = mProxy.mObject;
	
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
		//auto QualityString = GetAVCaptureSessionQuality(Quality);
		auto QualityString = AVCaptureSessionPreset1280x720;
		
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
	
	mOutput.Retain( [[AVCaptureVideoDataOutput alloc] init] );
	auto& Output = mOutput.mObject;
	
	
	
	//	todo: get output codecs
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
	
	
	//	register for notifications from errors
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
		mQueue = dispatch_queue_create("camera_queue", NULL);
	
	if ( MarkBeginConfig )
		[Session commitConfiguration];
	
	[Output setSampleBufferDelegate:Proxy queue: mQueue];
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


void AvfMediaExtractor::GetStreams(ArrayBridge<TStreamMeta>&& StreamMetas)
{
	for ( auto& Meta : mStreamMeta )
	{
		StreamMetas.PushBack( Meta.second );
	}
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CMSampleBufferRef SampleBuffer,size_t StreamIndex)
{
	auto Desc = CMSampleBufferGetFormatDescription( SampleBuffer );
	auto Meta = Avf::GetStreamMeta( Desc );
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	
	//	new stream!
	if ( mStreamMeta.find(StreamIndex) == mStreamMeta.end() )
	{
		try
		{
			mStreamMeta[StreamIndex] = Meta;
			OnStreamsChanged();
		}
		catch(...)
		{
			mStreamMeta.erase( mStreamMeta.find(StreamIndex) );
			throw;
		}
	}
	
	return Meta;
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CVPixelBufferRef SampleBuffer,size_t StreamIndex)
{
	TStreamMeta Meta;
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	
	//	new stream!
	if ( mStreamMeta.find(StreamIndex) == mStreamMeta.end() )
	{
		try
		{
			mStreamMeta[StreamIndex] = Meta;
			OnStreamsChanged();
		}
		catch(...)
		{
			mStreamMeta.erase( mStreamMeta.find(StreamIndex) );
			throw;
		}
	}
	
	return Meta;
}


void AvfMediaExtractor::QueuePacket(std::shared_ptr<TMediaPacket>& Packet)
{
	OnPacketExtracted( Packet->mTimecode, Packet->mMeta.mStreamIndex );
	
	{
		std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	
		//	only save latest
		//	gr: check in case this causes too much stuttering and maybe keep 2
		if ( mParams.mDiscardOldFrames )
			mPacketQueue.Clear();

		mPacketQueue.PushBack( Packet );
	}
	
	//	wake up the extractor as we want ReadNextPacket to try again
	Wake();
}

std::shared_ptr<TMediaPacket> AvfMediaExtractor::ReadNextPacket()
{
	if ( mPacketQueue.IsEmpty() )
		return nullptr;
	
	std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	return mPacketQueue.PopAt(0);
}

AvfMediaExtractor::AvfMediaExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	TMediaExtractor	( Params ),
	mOpenglContext	( OpenglContext ),
	mRenderer		( new AvfDecoderRenderer() )
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
		auto MinMax = GetMinMaxFrameRate(ActiveFormat);
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


