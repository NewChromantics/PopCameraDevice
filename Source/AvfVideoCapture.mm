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
#include <magic_enum/include/magic_enum.hpp>
#include "PopCameraDevice.h"

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



#if defined(TARGET_IOS)
@interface DepthCaptureProxy : NSObject <AVCaptureDepthDataOutputDelegate>
#elif defined(TARGET_OSX)
@interface DepthCaptureProxy : NSObject
#endif
{
	AvfVideoCapture*	mParent;
	size_t				mStreamIndex;
}

- (id)initWithVideoCapturePrivate:(AvfVideoCapture*)parent;

#if defined(TARGET_IOS)
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)output didOutputDepthData:(AVDepthData *)depthData timestamp:(CMTime)timestamp connection:(AVCaptureConnection *)connection;
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)output didDropDepthData:(AVDepthData *)depthData timestamp:(CMTime)timestamp connection:(AVCaptureConnection *)connection reason:(AVCaptureOutputDataDroppedReason)reason;
//- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
//- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (void)onVideoError:(NSNotification *)notification;
#endif

@end




@implementation DepthCaptureProxy

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

#if defined(TARGET_IOS)
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)captureOutput didOutputDepthData:(AVDepthData*)depthData timestamp:(CMTime)timestamp connection:(AVCaptureConnection *)connection
{
	static bool DoRetain = true;
	mParent->OnDepthFrame( depthData, timestamp, mStreamIndex, DoRetain );
}
#endif

#if defined(TARGET_IOS)
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)output didDropDepthData:(AVDepthData *)depthData timestamp:(CMTime)timestamp connection:(AVCaptureConnection *)connection reason:(AVCaptureOutputDataDroppedReason)reason;
{
	std::Debug << "dropped depth sample" << std::endl;
}
#endif

@end


Avf::TCaptureParams::TCaptureParams(json11::Json& Options)
{
	auto SetInt = [&](const char* Name,size_t& ValueUnsigned)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_number() )
			return false;
		auto Value = Handle.int_value();
		if ( Value < 0 )
		{
			std::stringstream Error;
			Error << "Value for " << Name << " is " << Value << ", not expecting negative";
			throw Soy::AssertException(Error);
		}
		ValueUnsigned = Value;
		return true;
	};
	auto SetBool = [&](const char* Name,bool& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_bool() )
			return false;
		Value = Handle.bool_value();
		return true;
	};
	auto SetString = [&](const char* Name,std::string& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_string() )
			return false;
		Value = Handle.string_value();
		return true;
	};

	auto SetPixelFormat = [&](const char* Name,SoyPixelsFormat::Type& Value)
	{
		std::string EnumString;
		if ( !SetString(Name,EnumString) )
			return false;
		
		Value = SoyPixelsFormat::Validate(EnumString);
		return true;
	};

	SetBool( POPCAMERADEVICE_KEY_SKIPFRAMES, mDiscardOldFrames );
	SetInt( POPCAMERADEVICE_KEY_FRAMERATE, mFrameRate );
	SetPixelFormat( POPCAMERADEVICE_KEY_DEPTHFORMAT, mDepthFormat );

	//	format could be string codec, or pixel format
	try
	{
		SetPixelFormat( POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
	}
	catch(...)
	{
		SetString(POPCAMERADEVICE_KEY_FORMAT, mCodecFormat);
	}
}


AvfVideoCapture::AvfVideoCapture(const std::string& Serial,const Avf::TCaptureParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext) :
	AvfMediaExtractor		( OpenglContext ),
	mQueue					( nullptr )
{
	@try
	{
		CreateDevice(Serial);
		CreateStream(Params);
	
		if ( Params.mFrameRate != 0 )
		{
			SetFrameRate(Params.mFrameRate);
		}
	
		StartStream();
	}
	@catch(NSException* e)
	{
		throw Soy::AssertException( Soy::NSErrorToString(e) );
	}
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

void AvfVideoCapture::CreateAndAddOutputDepth(AVCaptureSession* Session,const Avf::TCaptureParams& Params)
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
	
	bool KeepOldFrames = !Params.mDiscardOldFrames;
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
	
	auto& Proxy = mProxyDepth.mObject;
	[Output setDelegate:Proxy callbackQueue: mQueue];
#endif
}

void AvfVideoCapture::CreateAndAddOutputColour(AVCaptureSession* Session,SoyPixelsFormat::Type Format,const Avf::TCaptureParams& Params)
{
	mOutputColour.Retain( [[AVCaptureVideoDataOutput alloc] init] );
	if ( !mOutputColour )
		throw Soy::AssertException("Failed to allocate AVCaptureVideoDataOutput");

	auto& Output = mOutputColour.mObject;
	
	//	todo: get output codecs
	//	gr: debug all these
	Array<std::string> Codecs;
	GetOutputCodecs(Output,GetArrayBridge(Codecs));
	std::Debug << "Availible Codecs: " << Soy::StringJoin(GetArrayBridge(Codecs),",") << std::endl;
	
	Array<Soy::TFourcc> Formats;
	GetOutputFormats(Output,GetArrayBridge(Formats));
	std::Debug << "Availible Formats: " << Soy::StringJoin(GetArrayBridge(Formats),",") << std::endl;

	
	OSType AvfFormat = Avf::GetPlatformPixelFormat(Format);

	/*
	//	loop through formats for ones we can handle that are accepted
	//	https://developer.apple.com/library/mac/documentation/AVFoundation/Reference/AVCaptureVideoDataOutput_Class/#//apple_ref/occ/instp/AVCaptureVideoDataOutput/availableVideoCVPixelFormatTypes
	//	The first format in the returned list is the most efficient output format.
	Array<OSType> TryPixelFormats;
	{
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
				
			Debug << ", ";
			TryPixelFormats.PushBack( Format );
		}
		std::Debug << Debug.str() << std::endl;
	}
	*/
	bool KeepOldFrames = !Params.mDiscardOldFrames;
	Output.alwaysDiscardsLateVideoFrames = KeepOldFrames ? NO : YES;
	
	//	gr: wrong format here didnt error, just gave me infinite dropped samples.
	Output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
							[NSNumber numberWithInt:AvfFormat], kCVPixelBufferPixelFormatTypeKey, nil];
	/*
	Output.videoSettings = @{
							   (NSString *)kCVPixelBufferPixelFormatTypeKey : [NSNumber numberWithInt:Format],
								};
	 */
	if ( ![Session canAddOutput:Output] )
	{
		std::stringstream Error;
		Error << "Device does not support pixel format " << Soy::TFourcc(AvfFormat) << "/" << Format << std::endl;
		throw Soy::AssertException(Error);
	}

	//	compatible, add
	[Session addOutput:Output];
	std::Debug << "Device added " << Soy::TFourcc(AvfFormat) << "/" << Format << " output" << std::endl;
	
	auto& Proxy = mProxyColour.mObject;
	[Output setSampleBufferDelegate:Proxy queue: mQueue];
	
	
	//	register for notifications from errors
	NSNotificationCenter *notify = [NSNotificationCenter defaultCenter];
	[notify addObserver: Proxy
			   selector: @selector(onVideoError:)
				   name: AVCaptureSessionRuntimeErrorNotification
				 object: Session];

}


void AvfVideoCapture::CreateAndAddOutputCodec(AVCaptureSession* Session,const std::string& Codec,const Avf::TCaptureParams& Params)
{
	mOutputColour.Retain( [[AVCaptureVideoDataOutput alloc] init] );
	if ( !mOutputColour )
		throw Soy::AssertException("Failed to allocate AVCaptureVideoDataOutput");
	
	auto& Output = mOutputColour.mObject;
	
	//	todo: get output codecs
	//	gr: debug all these
	Array<std::string> Codecs;
	GetOutputCodecs(Output,GetArrayBridge(Codecs));
	std::Debug << "Availible Codecs: " << Soy::StringJoin(GetArrayBridge(Codecs),",") << std::endl;
	
	Array<Soy::TFourcc> Formats;
	GetOutputFormats(Output,GetArrayBridge(Formats));
	std::Debug << "Availible Formats: " << Soy::StringJoin(GetArrayBridge(Formats),",") << std::endl;
	
	
	bool KeepOldFrames = !Params.mDiscardOldFrames;
	NSString* CodecNs = Soy::StringToNSString(Codec);
	Output.alwaysDiscardsLateVideoFrames = KeepOldFrames ? NO : YES;
	
	Output.videoSettings = @{
							 (NSString *)AVVideoCodecKey : CodecNs
							 };

	if ( ![Session canAddOutput:Output] )
	{
		std::stringstream Error;
		Error << "Device does not support codec " << Codec << std::endl;
		throw Soy::AssertException(Error);
	}
	
	//	compatible, add
	[Session addOutput:Output];
	std::Debug << "Device added " << Codec << " output" << std::endl;
	
	auto& Proxy = mProxyColour.mObject;
	[Output setSampleBufferDelegate:Proxy queue: mQueue];
	
	
	//	register for notifications from errors
	NSNotificationCenter *notify = [NSNotificationCenter defaultCenter];
	[notify addObserver: Proxy
			   selector: @selector(onVideoError:)
				   name: AVCaptureSessionRuntimeErrorNotification
				 object: Session];
	
}

void AvfVideoCapture::CreateDevice(const std::string& Serial)
{
	// Find a suitable AVCaptureDevice
	auto SerialString = Soy::StringToNSString( Serial );
	mDevice.Retain( [AVCaptureDevice deviceWithUniqueID:SerialString] );
	if ( !mDevice )
	{
		std::stringstream Error;
		Error << "Failed to get AVCapture Device with serial " << Serial;
		throw Soy::AssertException( Error.str() );
	}

	//	make session
	{
		auto& Device = mDevice.mObject;

		mSession.Retain( [[AVCaptureSession alloc] init] );
		auto& Session = mSession.mObject;
		
		//	make our own queue, not the main queue
		if ( !mQueue )
			mQueue = dispatch_queue_create("AvfVideoCapture_Queue", NULL);
		
		NSError* Error = nil;
		//	assign intput to session
		AVCaptureDeviceInput* _input = [AVCaptureDeviceInput deviceInputWithDevice:Device error:&Error];
		if ( !_input || ![Session canAddInput:_input])
		{
			throw Soy::AssertException("Cannot add AVCaptureDeviceInput");
		}
		[Session addInput:_input];
	}
}

void AvfVideoCapture::CreateStream(const Avf::TCaptureParams& Params)
{
	if ( !mDevice )
		throw Soy::AssertException("CreateStream with no device");
	
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
		
	//	make proxys (even if they're not used)
	mProxyColour.Retain( [[VideoCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	mProxyDepth.Retain( [[DepthCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	
	auto& Session = mSession.mObject;

	
	static bool MarkBeginConfig = false;
	if ( MarkBeginConfig )
		[Session beginConfiguration];


	//	try and make all the format outputs asked for
	if ( Params.mColourFormat != SoyPixelsFormat::Invalid )
		CreateAndAddOutputColour(Session,Params.mColourFormat,Params);
	
	if ( Params.mDepthFormat != SoyPixelsFormat::Invalid )
		CreateAndAddOutputDepth(Session,Params);

	if ( !Params.mCodecFormat.empty() )
		CreateAndAddOutputCodec(Session,Params.mCodecFormat,Params);

	/*
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
	*/
/*
	//auto QualityString = AVCaptureSessionPreset1280x720;
	auto QualityString = AVCaptureSessionPresetHigh;

	if ( [mSession canSetSessionPreset:QualityString] )
	{
		Session.sessionPreset = QualityString;
	}
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
	auto PixelBuffer = SampleBuffer;
	TStreamMeta Meta;
	Meta.mStreamIndex = StreamIndex;
	
	//	gr: code duplicated from  AvfPixelBuffer::Lock
	//	if this is multiple planes, maybe we need to merge it back in again
	auto PlaneCount = CVPixelBufferGetPlaneCount( PixelBuffer );
	if ( PlaneCount >= 1 )
	{
		std::Debug << __PRETTY_FUNCTION__ << " needs to handle multiple plane data" << std::endl;
	/*
		BufferArray<SoyPixelsFormat::Type,2> PlaneFormats;
		auto Format = CVPixelBufferGetPixelFormatType( PixelBuffer );
		auto SoyFormat = Avf::GetPixelFormat( Format );
		SoyPixelsFormat::GetFormatPlanes( SoyFormat, GetArrayBridge(PlaneFormats) );
		auto PixelBufferDataSize = CVPixelBufferGetDataSize(PixelBuffer);
		for ( size_t PlaneIndex=0;	PlaneIndex<PlaneCount;	PlaneIndex++ )
		{
			//	gr: although the blitter can split this for us, I assume there MAY be a case where planes are not contiguous, so for this platform handle it explicitly
			auto Width = CVPixelBufferGetWidthOfPlane( PixelBuffer, PlaneIndex );
			auto Height = CVPixelBufferGetHeightOfPlane( PixelBuffer, PlaneIndex );
			auto* Pixels = CVPixelBufferGetBaseAddressOfPlane( PixelBuffer, PlaneIndex );
			auto BytesPerRow = CVPixelBufferGetBytesPerRowOfPlane( PixelBuffer, PlaneIndex );
			auto PlaneFormat = PlaneFormats[PlaneIndex];
	 
			//	gr: should this throw?
			if ( !Pixels )
			{
				std::Debug << "Image plane #" << PlaneIndex << "/" << PlaneCount << " " << Width << "x" << Height << " return null" << std::endl;
				continue;
			}
	 
			//	data size here is for the whole image, so we need to calculate (ie. ASSUME) it ourselves.
			SoyPixelsMeta PlaneMeta( Width, Height, PlaneFormat );
	 
			//	should be LESS as there are multiple plaens in the total buffer, but we'll do = just for the sake of the safety
			Soy::Assert( PlaneMeta.GetDataSize() <= PixelBufferDataSize, "Plane's calcualted data size exceeds the total buffer size" );
	 
			//	gr: currently we only have one transform... so... only apply to main plane (and hope they're the same)
			float3x3 DummyTransform;
			float3x3& PlaneTransform = (PlaneIndex == 0) ? Transform : DummyTransform;
	 
			LockPixels( Planes, Pixels, BytesPerRow, PlaneMeta, PlaneTransform );
		}
	 */
	}
	else
	{
		//	get the "non-planar" image
		auto Height = CVPixelBufferGetHeight( PixelBuffer );
		auto Width = CVPixelBufferGetWidth( PixelBuffer );
		//auto* Pixels = CVPixelBufferGetBaseAddress(PixelBuffer);
		auto Format = CVPixelBufferGetPixelFormatType( PixelBuffer );
		//Soy::TFourcc FormatFourcc(Format);
		//auto DataSize = CVPixelBufferGetDataSize(PixelBuffer);
		auto SoyFormat = Avf::GetPixelFormat( Format );
		/*
		auto BytesPerRow = CVPixelBufferGetBytesPerRow( PixelBuffer );
		if ( SoyFormat == SoyPixelsFormat::Invalid )
		{
			std::stringstream Error;
			Error << "Trying to lock plane but pixel format(" << Format << "/" << FormatFourcc <<") is unsupported(" << SoyFormat << ")";
			throw Soy::AssertException(Error.str());
		}
		
		if ( !Pixels )
		throw Soy::AssertException("Failed to get pixel buffer address");
		
		SoyPixelsMeta Meta( Width, Height, SoyFormat );
		LockPixels( Planes, Pixels, BytesPerRow, Meta, Transform, DataSize );
		*/
		Meta.mPixelMeta = SoyPixelsMeta( Width, Height, SoyFormat );
	}
	
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
	
		mPacketQueue.PushBack( Packet );
	}

	//	callback AFTER packet is queued
	if ( mOnPacketQueued )
		mOnPacketQueued( Packet->mTimecode, Packet->mMeta.mStreamIndex );
}


AvfMediaExtractor::AvfMediaExtractor(std::shared_ptr<Opengl::TContext>& OpenglContext) :
	mOpenglContext		( OpenglContext ),
	mRenderer			( new AvfDecoderRenderer() )
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

void AvfMediaExtractor::OnDepthFrame(AVDepthData* DepthData,CMTime CmTimestamp,size_t StreamIndex,bool DoRetain)
{
	//	gr: I think stalling too long here can make USB bus crash (killing bluetooth, hid, audio etc)
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,5);
	
	//Soy::Assert( sampleBufferRef != nullptr, "Expected sample buffer ref");
	if ( !DepthData )
	{
		std::Debug << __PRETTY_FUNCTION__ << " null depth data" << std::endl;
		return;
	}

	//	convert to the format we want, then call again
	Soy::TFourcc DepthFormat(DepthData.depthDataType);
	auto DepthPixelFormat = Avf::GetPixelFormat(DepthFormat.mFourcc32);
	SoyPixelsFormat::Type OutputFormat = SoyPixelsFormat::DepthFloatMetres;
	if ( DepthPixelFormat != OutputFormat )
	{
		auto OutputFourcc = Avf::GetPlatformPixelFormat(OutputFormat);
		auto NewDepthData = [DepthData depthDataByConvertingToDepthDataType:OutputFourcc];
		if ( !NewDepthData )
			throw Soy::AssertException("Failed to convert depth data to desired format");
		//	gr: need to be careful about recursion here
		OnDepthFrame(NewDepthData,CmTimestamp, StreamIndex, DoRetain);
		return;
	}

	
	//	convert format
	SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	auto DepthPixels = DepthData.depthDataMap;
	//Soy::TFourcc DepthFormat(DepthData.depthDataType);
	auto Quality = magic_enum::enum_name(DepthData.depthDataQuality);
	auto Accuracy = magic_enum::enum_name(DepthData.depthDataAccuracy);
	auto IsFiltered = DepthData.depthDataFiltered;
	AVCameraCalibrationData* CameraCalibration = DepthData.cameraCalibrationData;
		
	//std::Debug << "Depth format " << DepthFormat << " quality=" << Quality << " Accuracy=" << Accuracy << " IsFiltered=" << IsFiltered << std::endl;
	OnSampleBuffer( DepthPixels, Timestamp, StreamIndex, DoRetain );
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

	//[Session beginConfiguration];
	
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

	//[Session commitConfiguration];
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


