#include "Kinect.h"



namespace Freenect
{
	bool					IsOkay(libusb_error Result,const std::string& Context,bool Throw=true);
	std::string				GetErrorString(libusb_error Result);
	freenect_video_format	GetVideoFormat(SoyPixelsFormat::Type Format);
	freenect_depth_format	GetDepthFormat(SoyPixelsFormat::Type Format);
	SoyPixelsFormat::Type	GetFormat(freenect_video_format Format);
	SoyPixelsFormat::Type	GetFormat(freenect_depth_format Format);
	void					OnVideoFrame(freenect_device *dev, void *rgb, uint32_t timestamp);
	void					OnDepthFrame(freenect_device *dev, void *rgb, uint32_t timestamp);
	void					LogCallback(freenect_context *dev, freenect_loglevel level, const char *msg);
	const char*				LogLevelToString(freenect_loglevel level);
	SoyTime					TimestampToMs(uint32_t Timestamp);
	
	const size_t			VideoStreamIndex=0;
	const size_t			DepthStreamIndex=1;
	//const size_t			AudioStreamIndex=2;
}


namespace Kinect
{
	std::shared_ptr<TContext>	gContext;
	std::shared_ptr<TContext>	AllocContext();
	void						FreeContext();
	
	bool		IsOkay(int Result,const std::string& Context,bool Throw=true);
	std::string	GetErrorString(int Result);

	//	unfortunetly the libfreenect usr data (void*) seems to mangle addresses... I'm assuming 32bit address internally, not 64. so store an id instead
	uint32		AllocDeviceId();
}


uint32 Kinect::AllocDeviceId()
{
	static uint32 Id = 1001;
	return Id++;
}


SoyTime Freenect::TimestampToMs(uint32_t Timestamp)
{
	//	according to here, timestamp is in video clock cycles at 60(59.xx)... or 24hz?
	//	https://groups.google.com/forum/#!topic/openkinect/dQVaksXrNQ0
	int HzToMs = (59952460/1000);
	uint64 TimestampMs = Timestamp / HzToMs;
	return SoyTime( std::chrono::milliseconds(TimestampMs) );
}


const char* Freenect::LogLevelToString(freenect_loglevel level)
{
	switch ( level )
	{
		case FREENECT_LOG_FATAL:	return "Fatal";
		case FREENECT_LOG_ERROR:	return "Error";
		case FREENECT_LOG_WARNING:	return "Warning";
		case FREENECT_LOG_NOTICE:	return "Notice";
		case FREENECT_LOG_INFO:		return "Info";
		case FREENECT_LOG_DEBUG:	return "Debug";
		case FREENECT_LOG_SPEW:		return "Spew";
		case FREENECT_LOG_FLOOD:	return "Flood";
		default:					return "Libfreenect";
	}
}


void Kinect::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	
}


void Freenect::LogCallback(freenect_context *dev, freenect_loglevel level, const char *msg)
{
	std::string Message( msg );
	BufferArray<char,10> Trims;
	Trims.PushBack('\n');
	Trims.PushBack('\r');
	Trims.PushBack(' ');
	Soy::StringTrimRight( Message, GetArrayBridge(Trims) );

	//	save errors in the context
	if ( level == FREENECT_LOG_ERROR || level == FREENECT_LOG_FATAL )
	{
		if ( Kinect::gContext )
		{
			//Kinect::gContext->OnLibError( Message );
			return;
		}
	}
	
	std::Debug << "Freenect: " << Freenect::LogLevelToString(level) << ": " << Message << std::endl;
}



std::string	Freenect::GetErrorString(libusb_error Result)
{
	return libusb_strerror(Result);
}


std::string	Kinect::GetErrorString(int Result)
{
	if ( (Result >= 0 && Result < LIBUSB_ERROR_COUNT) || Result == LIBUSB_ERROR_OTHER )
	{
		return Freenect::GetErrorString( static_cast<libusb_error>(Result) );
	}

	std::stringstream Error;
	Error << "#" << Result << " ";
	
	//if ( gContext )
	//	gContext->FlushLibError( Error );
	
	return Error.str();
}


bool Freenect::IsOkay(libusb_error Result,const std::string& Context,bool Throw)
{
	return Kinect::IsOkay( static_cast<int>( Result ), Context, Throw );
}

bool Kinect::IsOkay(int Result,const std::string& Context,bool Throw)
{
	if ( Result == 0 )
		return true;
	
	std::stringstream Error;
	Error << "Kinect/libusb error in " << Context << ": " << GetErrorString(Result);
	
	if ( Throw )
	{
		throw Soy::AssertException( Error.str() );
	}
	else
	{
		std::Debug << Error.str() << std::endl;
		return false;
	}
}



std::shared_ptr<Kinect::TContext> Kinect::AllocContext()
{
	if ( !gContext )
	{
		gContext.reset( new TContext() );
	}
	return gContext;
}



void Kinect::FreeContext()
{
	//	last one, free it
	if ( gContext.unique() )
		gContext.reset();
}


std::shared_ptr<Kinect::TContext>	gAutoFreeingContext;
std::mutex							gAutoFreeingContextLock;
Kinect::TContext&					LockAutoFreeContext();
void								ReleaseAutoFreeContext();

Kinect::TContext& LockAutoFreeContext()
{
	gAutoFreeingContextLock.lock();
	if ( !gAutoFreeingContext )
		gAutoFreeingContext = Kinect::AllocContext();

	return *gAutoFreeingContext;
}

/*
class TDefferedReleaseThread
{
public:
	std::shared_ptr<std::thread>	mThread;
	std::atomic<bool>				mDelay;
	
	void				DelayDeath()
	{
		mDelay = true;
	}
	
	static void			AutoReleaseFunc()
	{
		StartAgain:
		do
		{
			static auto SleepMs = 3000;
			mDelay = false;
			std::this_thread::sleep_for( std::chrono::milliseconds( SleepMs ) );
		}
		while ( mDelay );

		if ( !gAutoFreeingContextLock.try_lock() )
		{
			//	someone has locked the context since sleep
			goto StartAgain;
		}
		
		gAutoFreeingContext.reset();
		gAutoFreeingContextLock.unlock();
		
	}
};
std::shared_ptr<TDefferedReleaseThread>	gAutoReleaseThread;
*/
void ReleaseAutoFreeContext()
{
	auto pContext = gAutoFreeingContext;
	
	/*
	//	startup a thread that will free the context
	if ( gAutoReleaseThread )
		gAutoReleaseThread->DelayDeath();
	else
		gAutoReleaseThread.reset( new std::thread( TDefferedReleaseThread::AutoReleaseFunc) );
	*/
	gAutoFreeingContext.reset();
	gAutoFreeingContextLock.unlock();

	Kinect::FreeContext();
}


void Kinect::EnumDevices(std::function<void(const std::string&)> Append)
{
	try
	{
		auto& Context = LockAutoFreeContext();
/*
		Array<Kinect::TDeviceMeta> Metas;
		Context.EnumDevices( GetArrayBridge(Metas), false );
		
		for ( int i=0;	i<Metas.GetSize();	i++ )
		{
			auto& Meta = Metas[i];
			Append( Meta.mName );
		}
		*/
		ReleaseAutoFreeContext();
	}
	catch(std::exception& e)
	{
		ReleaseAutoFreeContext();
		//	don't rethrow
	}

}





Kinect::TContext::TContext() /*:
#if defined(LIBFREENECT)
	mContext	( nullptr ),
#endif
	SoyThread	( "Kinect Context" )*/
{
	//	init context
#if defined(LIBFREENECT)
	CreateContext();
#endif
}

Kinect::TContext::~TContext()
{
	/*
	//	shut down & wait for devices
	try
	{
		WaitToFinish();
	}
	catch(std::exception& e)
	{
		std::Debug << "exception during context shutdown" << e.what() << std::endl;
	}
	
	//	kill devices
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = mDevices[i];
		Device.reset();
	}
	mDevices.Clear();
	
#if defined(LIBFREENECT)
	FreeContext();
#endif
	 */
}
/*
#if defined(LIBFREENECT)
void Kinect::TContext::CreateContext()
{
	if ( mContext )
		return;
	
	freenect_usb_context* UsbContext = nullptr;
	auto Result = freenect_init( &mContext, UsbContext );
	Kinect::IsOkay( Result, "freenect_init" );
	
	freenect_set_log_level( mContext, FREENECT_LOG_NOTICE );
	freenect_set_log_callback( mContext, Freenect::LogCallback );
	
	freenect_device_flags flags = (freenect_device_flags)( FREENECT_DEVICE_CAMERA );
	freenect_select_subdevices( mContext, flags );
	
	Start();
	
	//	re-create sub-devices
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		Device.AcquireDevice();
	}
}
#endif


#if defined(LIBFREENECT)
void Kinect::TContext::ReacquireDevices()
{
	//	re-create sub-devices
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		Device.ReleaseDevice();
		Device.AcquireDevice();
	}
}
#endif

#if defined(LIBFREENECT)
void Kinect::TContext::FreeContext()
{
	//	re-create sub-devices
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		Device.ReleaseDevice();
	}
	
	if ( mContext )
	{
		freenect_shutdown( mContext );
		mContext = nullptr;
	}
}
#endif

void Kinect::TContext::Thread()
{
#if defined(LIBFREENECT)

	try
	{
		CreateContext();
		mFatalError.clear();
	}
	catch (std::exception& e)
	{
		mFatalError = e.what();
		std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
		return;
	}
	
	if ( mContext )
	{
		int TimeoutMs = 10;
		int TimeoutSecs = 0;
		int TimeoutMicroSecs = TimeoutMs*1000;
		timeval Timeout = {TimeoutSecs,TimeoutMicroSecs};
		libusb_error Result = static_cast<libusb_error>( freenect_process_events_timeout( mContext, &Timeout ) );

		try
		{
			//	gr: -1 (also LIB_IO_USB_ERROR) can mean we've lost a device...
			//	https://github.com/OpenKinect/libfreenect/issues/229
			if ( Result == -1 )
			{
				ReacquireDevices();
			}
			else
			{
				Kinect::IsOkay( Result, "freenect_process_events_timeout" );
			}

			//	in case any devices are missing, try and acquire
			for ( int d=0;	d<mDevices.GetSize();	d++ )
			{
				auto& Device = *mDevices[d];
				Device.AcquireDevice();
			}
		
			//	after a good run, clear any fatal errors
			mFatalError.clear();
		}
		catch (std::exception& e)
		{
			mFatalError = e.what();
			
			//	gr: do we just want to re-create devices, not the context, on errors?
			//	kill context so it's re-created
			FreeContext();
		}
	}
	
	//	let thread breath?
	static size_t SleepMs = 10;
	std::this_thread::sleep_for( std::chrono::milliseconds(SleepMs) );
#endif
}



std::shared_ptr<Kinect::TDevice> Kinect::TContext::GetDevice(uint32 DeviceId)
{
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		if ( Device == DeviceId )
			return mDevices[i];
	}
	
	return nullptr;
}


std::shared_ptr<Kinect::TDevice> Kinect::TContext::GetDevice(const std::string& Name,const TVideoDecoderParams& Params)
{
	std::string MatchName = Name;

	//	see if one's already allocated
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		if ( Device.mMeta == MatchName )
			return mDevices[i];
	}
	
	bool AnyMatch = (Name == "*");
	
	//	enum devices, find matching real name
	Array<TDeviceMeta> Metas;
	EnumDevices( GetArrayBridge(Metas), AnyMatch );

	//	special names
	{
		int NameIndex = -1;
		Soy::StringToType( NameIndex, Name );
		if ( AnyMatch )
			NameIndex = 0;

		if ( NameIndex >= 0 && NameIndex < Metas.GetSize() )
			MatchName = Metas[NameIndex].mName;
	}
	
	//	search devices again
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		if ( Device.mMeta == MatchName )
			return mDevices[i];
	}
	
	std::stringstream AllocErrors;
	
	//	search metas
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		if ( Metas[i] == MatchName )
		{
			try
			{
				//	alloc if it doesn't exist
				std::shared_ptr<TDevice> NewDevice( new TDevice( Metas[i], Params, *this ) );
				mDevices.PushBack( NewDevice );
				return NewDevice;
			}
			catch(std::exception& e)
			{
				AllocErrors << "Tried to alloc device but failed: " << e.what() << ". ";
			}
		}
	}

	std::stringstream Error;
	Error << "Found 0/" << Metas.GetSize() << " devices matching " << Name << "; ";
	for ( int m=0;	m<Metas.GetSize();	m++ )
		Error << Metas[m].mName << ", ";
	Error << AllocErrors.str();
	
	throw Soy::AssertException( Error.str() );
}

void Kinect::TContext::EnumDevices(ArrayBridge<TDeviceMeta>&& Metas,bool AllowInvalidSerial)
{
	//	always include devices we've found before
	for ( int d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = *mDevices[d];
		Metas.PushBackUnique( Device.mMeta );
	}

#if defined(LIBFREENECT)
	struct freenect_device_attributes* DeviceList = nullptr;
	//	returns <0 on error, or number of devices.
	freenect_list_device_attributes( mContext, &DeviceList );
	if ( true )
	{
		auto Device = DeviceList;
		while ( Device )
		{
			//	seperate devices for seperate content
			TDeviceMeta Meta( *Device );
			
			//	ignore temporarily bad devices
			if ( AllowInvalidSerial || !Soy::StringBeginsWith( Meta.mName, "000000", true ) )
			{
				Metas.PushBackUnique( Meta );
			}
			Device = Device->next;
		}
		freenect_free_device_attributes( DeviceList );
	}
#endif
	
}



Kinect::TDeviceMeta::TDeviceMeta(struct freenect_device_attributes& Device)
{
#if defined(LIBFREENECT)
	mName = Device.camera_serial;
	
	//	try and fill streams here based on... context support?
#endif
}


Kinect::TDevice::TDevice(const TDeviceMeta& Meta,const TVideoDecoderParams& Params,TContext& Context) :
#if defined(LIBFREENECT)
	mDevice		( nullptr ),
	mDeviceOldUserData	( nullptr ),
#endif
	mMeta		( Meta ),
	mDeviceId	( 0 ),
	mContext	( Context )
{
#if defined(LIBFREENECT)
	AcquireDevice();

#else
	throw Soy::AssertException("Device not supported on this platform");
#endif
}

#if defined(LIBFREENECT)
void Kinect::TDevice::AcquireDevice()
{
	//	already acquired
	if ( mDevice )
		return;
	
	try
	{
		mDeviceId = AllocDeviceId();

		//	later maybe we only open the streams we want and re-open to add remove/streams
		static bool Video = true;
		static bool Depth = true;
		//static bool Audio = false;
	
		{
			Soy::Assert( !mDevice, "Device should be null before opening");
			auto Result = freenect_open_device_by_camera_serial( mContext.GetContext(), &mDevice, mMeta.mName.c_str() );
			std::stringstream Error;
			Error << "freenect_open_device_by_camera_serial(" << mMeta.mName << ")";
			Kinect::IsOkay( Result, Error.str() );
			Soy::Assert( mDevice, "Device missing after opening");
		}
	
		//	assign TDevice to device
		mDeviceOldUserData = freenect_get_user( mDevice );
		freenect_set_user( mDevice, reinterpret_cast<void*>(mDeviceId) );
	
		CreateStreams( Video, Depth );
	
		//	set LED based on what's enabled
		freenect_led_options LedColour = LED_BLINK_RED_YELLOW;
		if ( mVideoStream && mDepthStream )
			LedColour = LED_GREEN;
		else if ( mVideoStream )
			LedColour = LED_YELLOW;
		else if ( mDepthStream )
			LedColour = LED_RED;
	
		freenect_set_led( mDevice, LedColour );
		mFatalError.clear();
	}
	catch ( std::exception& e)
	{
		mFatalError = e.what();
		ReleaseDevice();
	}
}
#endif

#if defined(LIBFREENECT)
void Kinect::TDevice::ReleaseDevice()
{
	if ( mDevice )
	{
		mVideoStream.reset();
		mDepthStream.reset();
		freenect_set_led( mDevice, LED_RED );
		freenect_close_device( mDevice );
		mDevice = nullptr;
	}
}
#endif

#if defined(LIBFREENECT)
void Kinect::TDevice::CreateStreams(bool Video, bool Depth)
{
	try
	{
		if ( Video )
			mVideoStream.reset( new TVideoStream( *mDevice, SoyPixelsFormat::RGB, Freenect::VideoStreamIndex ) );
		
		//	a pause in here seems to help USB manage to create both
		std::this_thread::sleep_for( std::chrono::milliseconds(200) );
		
		if ( Depth )
			mDepthStream.reset( new TDepthStream( *mDevice, SoyPixelsFormat::FreenectDepthmm, Freenect::DepthStreamIndex ) );
	}
	catch (std::exception& e)
	{
		mVideoStream.reset();
		mDepthStream.reset();
		throw;
	}

}
#endif

Kinect::TDevice::~TDevice()
{
#if defined(LIBFREENECT)
	ReleaseDevice();
#endif
}

void Kinect::TDevice::GetStreamMeta(ArrayBridge<TStreamMeta>& StreamMetas)
{
	if ( mVideoStream )
		StreamMetas.PushBack( mVideoStream->GetStreamMeta() );
	
	if ( mDepthStream )
		StreamMetas.PushBack( mDepthStream->GetStreamMeta() );
}





Kinect::TDeviceDecoder::TDeviceDecoder(const TVideoDecoderParams& Params,std::map<size_t,std::shared_ptr<TPixelBufferManager>>& PixelBufferManagers,std::map<size_t,std::shared_ptr<TAudioBufferManager>>& AudioBufferManagers,std::shared_ptr<Opengl::TContext> OpenglContext) :
	TVideoDecoder		( Params, PixelBufferManagers, AudioBufferManagers ),
	mOpenglContext		( OpenglContext )
{
	//	alloc blitter
	mOpenglBlitter.reset( new Opengl::TBlitter(nullptr) );
	
	mContext = AllocContext();
	
	//	find an appropriate device
	mDevice = mContext->GetDevice( Params.mFilename, Params );

	//	alloc buffers for each device's capabilities
	{
		auto pStream = mDevice->GetVideoStream();
		if ( pStream )
		{
			AllocPixelBufferManager( pStream->GetStreamMeta() );
		}
	}
	{
		auto pStream = mDevice->GetDepthStream();
		if ( pStream )
		{
			AllocPixelBufferManager( pStream->GetStreamMeta() );
		}
	}

	//	and listen for it's new frame
	auto& Event = mDevice->mOnNewFrame;
	mDeviceOnNewFrameListener = Event.AddListener( *this, &TDeviceDecoder::OnNewFrame );
}

Kinect::TDeviceDecoder::~TDeviceDecoder()
{
	if ( mDevice )
	{
		auto& Event = mDevice->mOnNewFrame;
		Event.RemoveListener( mDeviceOnNewFrameListener );
		mDevice.reset();
	}
	
	mContext.reset();
	FreeContext();
}

void Kinect::TDeviceDecoder::GetStreamMeta(ArrayBridge<TStreamMeta>&& StreamMetas)
{
	if ( !mDevice )
		return;
	
	mDevice->GetStreamMeta( StreamMetas );
}


TVideoMeta Kinect::TDeviceDecoder::GetMeta()
{
	TVideoMeta Meta;
	return Meta;
}

void Kinect::TDeviceDecoder::OnNewFrame(const Kinect::TFrame& Frame)
{
	auto& Output = GetPixelBufferManager( Frame.mStreamIndex );
	
	TPixelBufferFrame PixelBuffer;
	PixelBuffer.mTimestamp = Frame.mTimecode;
	Output.CorrectDecodedFrameTimestamp( PixelBuffer.mTimestamp );
	float3x3 Transform;

	//	do depth processing
	switch ( Frame.mPixels.GetFormat() )
	{
		case SoyPixelsFormat::KinectDepth:
		case SoyPixelsFormat::FreenectDepth10bit:
		case SoyPixelsFormat::FreenectDepth11bit:
		case SoyPixelsFormat::FreenectDepthmm:
			//	copy pixels
			PixelBuffer.mPixels.reset( new TDepthPixelBuffer( Frame.mPixels, mOpenglBlitter, mOpenglContext ) );
			break;
			
		default:
			//	copy pixels
			PixelBuffer.mPixels.reset( new TDumbPixelBuffer( Frame.mPixels, Transform ) );
			break;
	}
	
	
	//	just drop frames
	auto Block = []
	{
		return false;
	};
	
	//	these don't apply, but call them all anyway so graphs are nice
	Output.mOnFrameExtracted.OnTriggered( PixelBuffer.mTimestamp );
	Output.mOnFrameDecodeSubmission.OnTriggered( PixelBuffer.mTimestamp );
	Output.mOnFrameDecoded.OnTriggered( PixelBuffer.mTimestamp );
	Output.PushPixelBuffer( PixelBuffer, Block );
}

bool Kinect::TDeviceDecoder::HasFatalError(std::string& Error)
{
	if ( mContext )
	{
		if ( mContext->HasFatalError( Error ) )
			return true;
	}
	
	if ( mDevice )
	{
		if ( mDevice->HasFatalError( Error ) )
			return true;
	}
	
	return false;
}


#if defined(LIBFREENECT)
freenect_video_format Freenect::GetVideoFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		//case SoyPixelsFormat::RGB:	return FREENECT_VIDEO_RGB;
		case SoyPixelsFormat::RGB:	return FREENECT_VIDEO_YUV_RGB;
		case SoyPixelsFormat::uyvy:	return FREENECT_VIDEO_YUV_RAW;

		default:
			break;
	}
	
	return FREENECT_VIDEO_DUMMY;
}
#endif

#if defined(LIBFREENECT)
freenect_depth_format Freenect::GetDepthFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::FreenectDepth11bit:	return FREENECT_DEPTH_11BIT;
		case SoyPixelsFormat::FreenectDepth10bit:	return FREENECT_DEPTH_10BIT;

		//	gr: don't use re-aligned, it's expensive
		//	use this in a shader!
		//	http://www.mindtreatstudios.com/kinect-3d-view-projection-matrix-rgb-camera/
		case SoyPixelsFormat::FreenectDepthmm:		return FREENECT_DEPTH_REGISTERED;
		//case SoyPixelsFormat::FreenectDepthmm:		return FREENECT_DEPTH_MM;
		default:
			break;
	}

	return FREENECT_DEPTH_DUMMY;
}
#endif


#if defined(LIBFREENECT)
SoyPixelsFormat::Type Freenect::GetFormat(freenect_video_format Format)
{
	switch ( Format )
	{
		case FREENECT_VIDEO_RGB:		return SoyPixelsFormat::RGB;
		case FREENECT_VIDEO_YUV_RGB:	return SoyPixelsFormat::RGB;
		case FREENECT_VIDEO_YUV_RAW:	return SoyPixelsFormat::uyvy;
		default:
			break;
	}
	
	return SoyPixelsFormat::Invalid;
}
#endif

#if defined(LIBFREENECT)
SoyPixelsFormat::Type Freenect::GetFormat(freenect_depth_format Format)
{
	switch ( Format )
	{
		case FREENECT_DEPTH_11BIT:		return SoyPixelsFormat::FreenectDepth11bit;
		case FREENECT_DEPTH_10BIT:		return SoyPixelsFormat::FreenectDepth10bit;
		case FREENECT_DEPTH_REGISTERED:	return SoyPixelsFormat::FreenectDepthmm;
		case FREENECT_DEPTH_MM:			return SoyPixelsFormat::FreenectDepthmm;
		default:
			break;
	}
	
	return SoyPixelsFormat::Invalid;
}
#endif

#if defined(LIBFREENECT)
void Freenect::OnVideoFrame(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	SoyTime Timecode = Freenect::TimestampToMs( timestamp );

	auto* usr = freenect_get_user(dev);
	auto DeviceId = reinterpret_cast<uint64>( usr );
	auto DeviceId32 = size_cast<uint32>( DeviceId );
	if ( !Kinect::gContext )
		return;
	
	auto Device = Kinect::gContext->GetDevice( DeviceId32 );
	if ( !Device )
		return;

	if ( !rgb )
	{
		std::Debug << "Video stream video callback missing rgb data " << Timecode << std::endl;
		return;
	}
	
	//	get data size
	auto Stream = Device->GetVideoStream();
	if ( !Stream )
	{
		std::Debug << "Video callback missing video stream on device " << Timecode << std::endl;
		return;
	}

	//	make pixels array
	auto* Rgb8 = reinterpret_cast<uint8*>( rgb );
	size_t RgbSize = Stream->mFrameMode.bytes;
	auto& PixelsMeta = Stream->GetStreamMeta().mPixelMeta;
	SoyPixelsRemote Pixels( Rgb8, RgbSize, PixelsMeta );
	
	Kinect::TFrame Frame( Pixels, Timecode, Stream->GetStreamMeta().mStreamIndex );
	try
	{
		Device->mOnNewFrame.OnTriggered( Frame );
	}
	catch(std::exception&e)
	{
		std::Debug << "Exception; " << e.what() << std::endl;
	}
};
#endif


#if defined(LIBFREENECT)
void Freenect::OnDepthFrame(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	SoyTime Timecode = Freenect::TimestampToMs( timestamp );

	auto* usr = freenect_get_user(dev);
	auto DeviceId = reinterpret_cast<uint64>( usr );
	auto DeviceId32 = size_cast<uint32>( DeviceId );
	if ( !Kinect::gContext )
		return;
	
	auto Device = Kinect::gContext->GetDevice( DeviceId32 );
	if ( !Device )
		return;

	if ( !rgb )
	{
		std::Debug << "Video stream video callback missing rgb data " << Timecode << std::endl;
		return;
	}
	
	//	get data size
	auto Stream = Device->GetDepthStream();
	if ( !Stream )
	{
		std::Debug << "Video callback missing video stream on device " << Timecode << std::endl;
		return;
	}
	
	//	make pixels array
	auto* Rgb8 = reinterpret_cast<uint8*>( rgb );
	size_t RgbSize = Stream->mFrameMode.bytes;
	auto& PixelsMeta = Stream->GetStreamMeta().mPixelMeta;
	SoyPixelsRemote Pixels( Rgb8, RgbSize, PixelsMeta );
	
	Kinect::TFrame Frame( Pixels, Timecode, Stream->GetStreamMeta().mStreamIndex );
	try
	{
		Device->mOnNewFrame.OnTriggered( Frame );
	}
	catch(std::exception&e)
	{
		std::Debug << "Exception; " << e.what() << std::endl;
	}
};
#endif

#if defined(LIBFREENECT)
Kinect::TStream::TStream(freenect_device& Device,size_t StreamIndex) :
	mDevice			( &Device )
{
	memset( &mFrameMode, 0, sizeof(mFrameMode) );
	
	mMeta.mStreamIndex = StreamIndex;
}
#endif


#if defined(LIBFREENECT)
Kinect::TVideoStream::TVideoStream(freenect_device& Device,SoyPixelsFormat::Type PixelFormat,size_t StreamIndex) :
	TStream	( Device, StreamIndex )
{
	//	get params
	auto Format = Freenect::GetVideoFormat( PixelFormat );
	auto Resolution = FREENECT_RESOLUTION_MEDIUM;

	//	setup format
	mFrameMode = freenect_find_video_mode( Resolution, Format );
	Soy::Assert( mFrameMode.is_valid, "Failed to get video mode");
	Soy::Assert( mFrameMode.padding_bits_per_pixel == 0, "Don't currently supported mis-aligned pixels formats");

	auto Result = freenect_set_video_mode( &Device, mFrameMode );
	Kinect::IsOkay( Result, "freenect_set_video_mode" );
	//freenect_set_video_buffer( mDevice, rgb_back);

	//	stop current mode
	//	gr: commented out lets video start. maybe this is refcounted... so only turn off if we've started?
	//freenect_stop_video( mDevice );

	Result = freenect_start_video( &Device );
	Kinect::IsOkay( Result, "freenect_start_video" );

	freenect_set_video_callback( &Device, Freenect::OnVideoFrame );
	
	//	update meta
	SoyPixelsMeta PixelsMeta( mFrameMode.width, mFrameMode.height, Freenect::GetFormat(mFrameMode.video_format) );
	mMeta.mPixelMeta = PixelsMeta;
}
#endif

Kinect::TVideoStream::~TVideoStream()
{
#if defined(LIBFREENECT)
	if ( mDevice )
	{
		freenect_stop_video( mDevice );
		freenect_set_video_callback( mDevice, nullptr );
		mDevice = nullptr;
	}
#endif
}


#if defined(LIBFREENECT)
Kinect::TDepthStream::TDepthStream(freenect_device& Device,SoyPixelsFormat::Type PixelFormat,size_t StreamIndex) :
	TStream	( Device, StreamIndex )
{
	auto Format = Freenect::GetDepthFormat( PixelFormat );
	//	gr: cannot use High
	auto Resolution = FREENECT_RESOLUTION_MEDIUM;
	
	mFrameMode = freenect_find_depth_mode( Resolution, Format );
	Soy::Assert( mFrameMode.is_valid, "Failed to get depth mode");

	//SoyPixelsFormat::GetFormatFromChannelCount( mVideoMode.data_bits_per_pixel/8 );

	auto Result = freenect_set_depth_mode( &Device, mFrameMode );
	Kinect::IsOkay( Result, "freenect_set_depth_mode" );

	//	stop current mode
	//freenect_stop_depth( &Device );
	Result = freenect_start_depth( &Device );
	Kinect::IsOkay( Result, "freenect_start_depth" );

	freenect_set_depth_callback( &Device, Freenect::OnDepthFrame );

	//	update meta
	SoyPixelsMeta PixelsMeta( mFrameMode.width, mFrameMode.height, Freenect::GetFormat(mFrameMode.depth_format) );
	mMeta.mPixelMeta = PixelsMeta;
}
#endif

Kinect::TDepthStream::~TDepthStream()
{
#if defined(LIBFREENECT)
	if ( mDevice )
	{
		freenect_stop_depth( mDevice );
		freenect_set_depth_callback( mDevice, nullptr );
		mDevice = nullptr;
	}
#endif
	
}



Kinect::TDepthPixelBuffer::TDepthPixelBuffer(const SoyPixelsImpl& Pixels,std::shared_ptr<Opengl::TBlitter> Blitter,std::shared_ptr<Opengl::TContext> Context) :
	mPixels			( Pixels ),
	mOpenglBlitter	( Blitter ),
	mOpenglContext	( Context )
{
}

Kinect::TDepthPixelBuffer::~TDepthPixelBuffer()
{
	PopWorker::DeferDelete( mOpenglContext, mLockedTexture, true );
	PopWorker::DeferDelete( mOpenglContext, mOpenglBlitter, false );
}


void Kinect::TDepthPixelBuffer::Lock(ArrayBridge<SoyPixelsImpl*>&& Textures,float3x3& Transform)
{
	Textures.PushBack( &mPixels );
}


void Kinect::TDepthPixelBuffer::Unlock()
{
//	mLockedTexture.reset();
}

*/


