#include "Freenect.h"
#include "libusb.h"
#include "libfreenect.h"
#include "PopCameraDevice.h"	//	params keys

#if defined(TARGET_WINDOWS)
//	libusb uses some stdio functions which are now inlined. This library provides a function to link to
//	https://stackoverflow.com/a/32418900
#pragma comment(lib,"legacy_stdio_definitions.lib")
//	this define apparently might resolve the same thing, but doesnt seem to work 
//	_NO_CRT_STDIO_INLINE

//	linking with libusb also is missing "iob"
//	so here's a replacement
//https://stackoverflow.com/a/32449318
//	this gives warning LNK4049: locally defined symbol _iob imported
//	maybe I can get around that with dllspec(IMPORT) ?
extern "C" FILE _iob[] = { *stdin, *stdout, *stderr };
#endif


namespace Freenect
{
	//	we need to recreate the library interface sometimes, so it's wrapped in this
	class TFreenect;
	class TContext;
	
	class TFrameListener;
	class TDevice;
	
	bool					IsOkay(libusb_error Result,const std::string& Context,bool Throw=true);
	std::string				GetErrorString(libusb_error Result);
	freenect_video_format	GetColourFormat(SoyPixelsFormat::Type Format);
	freenect_depth_format	GetDepthFormat(SoyPixelsFormat::Type Format);
	SoyPixelsFormat::Type	GetFormat(freenect_video_format Format);
	SoyPixelsFormat::Type	GetFormat(freenect_depth_format Format);
	void					OnColourFrame(freenect_device *dev, void *rgb, uint32_t timestamp);
	void					OnDepthFrame(freenect_device *dev, void *rgb, uint32_t timestamp);
	void					LogCallback(freenect_context *dev, freenect_loglevel level, const char *msg);
	const char*				LogLevelToString(freenect_loglevel level);
	SoyTime					TimestampToMs(uint32_t Timestamp);
	
	bool		IsOkay(int Result,const std::string& Context,bool Throw=true);
	std::string	GetErrorString(int Result);

	//	unfortunetly the libfreenect usr data (void*) seems to mangle addresses... I'm assuming 32bit address internally, not 64. so store an id instead
	uint32		AllocDeviceId();
	
	
	
	TContext&					GetContext();
	std::shared_ptr<TContext>	gContext;
	
	const std::string			DeviceName_Prefix = "Freenect:";
	auto 						DefaultColourFormat = SoyPixelsFormat::uyvy_8888;
	auto 						DefaultDepthFormat = SoyPixelsFormat::Depth16mm;
	auto 						DepthStreamName = "Depth";
	auto 						ColourStreamName = "Colour";

	freenect_resolution			GetResolution(SoyPixelsMeta Meta,bool AllowHighResolution);

	auto LOW_WIDTH = 320;
	auto LOW_HEIGHT = 240;
	auto MEDIUM_WIDTH = 640;
	auto MEDIUM_HEIGHT = 480;
	auto HIGH_WIDTH = 1280;
	auto HIGH_HEIGHT = 1024;
}


Freenect::TCaptureParams::TCaptureParams(json11::Json& Options)
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

	SetPixelFormat( POPCAMERADEVICE_KEY_DEPTHFORMAT, mDepthFormat );
	SetPixelFormat( POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
	
	//	temp force colour format as its not a usual format
	if ( mColourFormat == SoyPixelsFormat::Invalid )
		mColourFormat = DefaultColourFormat;
}

//	NOT RAII atm
class Freenect::TDevice
{
public:
	TDevice()
	{
		mDepthMode.is_valid = false;
	}
	/*
	TDevice(freenect_device& Device,const std::string& Serial)
	{
		mDevice = &Device;
		mSerial = Serial;
		mDepthMode.is_valid = false;
		mColourMode.is_valid = false;
	}
	*/
	TDevice(std::function<freenect_device*(const std::string&)> OpenFunc,const std::string& Serial)
	{
		mOpenFunc = OpenFunc;
		mSerial = Serial;
		mDepthMode.is_valid = false;
		mColourMode.is_valid = false;
		Open();
	}
	
	void			EnableDepthStream(SoyPixelsMeta Format);
	void			EnableColourStream(SoyPixelsMeta Format);
	void			Close();
	void			Open();			//	if there's no device, re-open given our known serial
	SoyPixelsRemote	GetDepthPixels(const uint8_t* Pixels);
	SoyPixelsRemote	GetColourPixels(const uint8_t* Pixels);

	//	todo; can we compare two device pointers?
	bool			operator==(const std::string& Serial) const	{	return mSerial == Serial;	}
	bool			operator!=(const std::string& Serial) const	{	return mSerial != Serial;	}
	bool			operator==(const TDevice& Device) const		{	return mSerial == Device.mSerial;	}
	bool			operator!=(const TDevice& Device) const		{	return mSerial != Device.mSerial;	}
	bool			operator==(const freenect_device& Device) const		{	return mDevice == &Device;	}
	bool			operator!=(const freenect_device& Device) const		{	return mDevice != &Device;	}

	
	std::function<freenect_device*(const std::string&)>	mOpenFunc;
	freenect_frame_mode	mDepthMode;
	freenect_frame_mode	mColourMode;
	freenect_device*	mDevice = nullptr;
	std::string			mSerial;

private:
	//	these are for re-opening
	SoyPixelsMeta		mDepthFormat;
	SoyPixelsMeta		mColourFormat;
};

class Freenect::TFrameListener
{
public:
	std::string		mSerial;
	std::function<void(const SoyPixelsImpl&,SoyTime)>	mOnFrame;
};



//	because the API needs to sometimes recreate the "hardware" (freenect)
//	we still need to manage devices that existed etc
//	so the context, controls the freenect context
class Freenect::TFreenect : public SoyThread
{
public:
	TFreenect();
	~TFreenect();
	
	void				EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&& Formats)> Enum);
	TDevice&			OpenDevice(const std::string& Serial);
	void				OnDepthFrame(freenect_device& Device,const uint8_t* Bytes,SoyTime Timestamp);
	void				OnColourFrame(freenect_device& Device,const uint8_t* Bytes,SoyTime Timestamp);
	TDevice&			GetDevice(freenect_device& Device);

private:
	virtual bool		ThreadIteration() override;
	void				ReacquireDevices();
	
public:
	std::function<void(TDevice&,const SoyPixelsImpl&,SoyTime)>	mOnDepthFrame;
	std::function<void(TDevice&,const SoyPixelsImpl&,SoyTime)>	mOnColourFrame;

private:
	std::recursive_mutex	mDeviceLock;	//	we were getting deadlock from depthcallback, whilst opening a device, so set to recursive
	Array<TDevice>			mDevices;		//	devices we've opened
	freenect_context*		mContext = nullptr;
};


class Freenect::TContext
{
public:
	TFreenect&						GetFreenect();
	
	std::shared_ptr<TFrameListener>	CreateListener(const std::string& Serial,SoyPixelsMeta ColourFormat,SoyPixelsMeta DepthFormat);
	
	void							FailRunningThreads();	//	for process exit, threads have gone, but we are unaware

private:
	void							OnFrame(TDevice& Device,const SoyPixelsImpl& Pixels,SoyTime Timestamp);

public:
	//	 lock this!
	std::mutex						mListenersLock;
	Array<std::shared_ptr<TFrameListener>>	mListeners;
	
	std::shared_ptr<TFreenect>	mFreenect;	//	library
};


Freenect::TContext& Freenect::GetContext()
{
	if ( !gContext )
	{
		gContext.reset( new Freenect::TContext() );
	}
	return *gContext;
}


void Freenect::OnDepthFrame(freenect_device* dev, void *rgb, uint32_t timestamp)
{
	if ( !dev )
	{
		std::Debug << "OnDepthFrame with null device" << std::endl;
		return;
	}
	
	auto* FreenectPtr = freenect_get_user(dev);
	auto& Freenect = *reinterpret_cast<TFreenect*>( FreenectPtr );
	SoyTime Timecode = Freenect::TimestampToMs( timestamp );

	auto* rgb8 = static_cast<uint8_t*>(rgb);
	Freenect.OnDepthFrame( *dev, rgb8, Timecode );
}


void Freenect::OnColourFrame(freenect_device* dev, void *rgb, uint32_t timestamp)
{
	if ( !dev )
	{
		std::Debug << "OnDepthFrame with null device" << std::endl;
		return;
	}
	
	auto* FreenectPtr = freenect_get_user(dev);
	auto& Freenect = *reinterpret_cast<TFreenect*>( FreenectPtr );
	SoyTime Timecode = Freenect::TimestampToMs( timestamp );
	
	auto* rgb8 = static_cast<uint8_t*>(rgb);
	Freenect.OnColourFrame( *dev, rgb8, Timecode );
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

void Freenect::Shutdown(bool ProcessExit)
{
	if (gContext)
	{
		gContext->FailRunningThreads();
	}
	//	if this thread is the freenect context thread, we need to tell the thread it's already shutdown
	gContext.reset();
}


void Freenect::EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&& Formats)> Enum)
{
	auto& Context = GetContext();
	auto& Freenect = Context.GetFreenect();
	
	std::function<void(const std::string&,ArrayBridge<std::string>&& Formats)> EnumDevice = [&](const std::string& Serial,ArrayBridge<std::string>&& Formats)
	{
		Enum( DeviceName_Prefix+Serial, GetArrayBridge(Formats) );
	};
	
	Freenect.EnumDevices( EnumDevice );
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
		
	}
	
	std::Debug << "Freenect: " << Freenect::LogLevelToString(level) << ": " << Message << std::endl;
}



std::string	Freenect::GetErrorString(libusb_error Result)
{
	return libusb_strerror(Result);
}


std::string	Freenect::GetErrorString(int Result)
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
	return Freenect::IsOkay( static_cast<int>( Result ), Context, Throw );
}

bool Freenect::IsOkay(int Result,const std::string& Context,bool Throw)
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

void Freenect::TContext::FailRunningThreads()
{
	if (!mFreenect)
		return;

	mFreenect->OnDetatchedExternally();
}

Freenect::TFreenect& Freenect::TContext::GetFreenect()
{
	if ( !mFreenect )
	{
		mFreenect.reset( new TFreenect() );
		mFreenect->mOnDepthFrame = std::bind( &TContext::OnFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 );
		mFreenect->mOnColourFrame = std::bind( &TContext::OnFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 );
	}
	return *mFreenect;
}


std::shared_ptr<Freenect::TFrameListener> Freenect::TContext::CreateListener(const std::string& Serial,SoyPixelsMeta ColourFormat,SoyPixelsMeta DepthFormat)
{
	//	try and open the device first, so this will fail the caller
	auto& Freenect = GetFreenect();
	auto& Device = Freenect.OpenDevice( Serial );

	if ( ColourFormat.GetFormat() != SoyPixelsFormat::Invalid )
	{
		Device.EnableColourStream( ColourFormat );
	}
	
	if ( DepthFormat.GetFormat() != SoyPixelsFormat::Invalid )
	{
		Device.EnableDepthStream( DepthFormat );
	}

	//	create the listener
	std::shared_ptr<TFrameListener> Listener( new TFrameListener );
	Listener->mSerial = Serial;
	
	mListeners.PushBack( Listener );
	return Listener;
}

void Freenect::TDevice::Close()
{
	if ( !mDevice )
		return;
	
	//	stop streams
	auto Result = freenect_stop_depth( mDevice );
	mDepthMode.is_valid = false;
	
	Result = freenect_stop_video( mDevice );
	mColourMode.is_valid = false;

	Result = freenect_close_device( mDevice );
	IsOkay( Result, "freenect_close_device" );
	
	mDevice = nullptr;
}


void Freenect::TDevice::Open()
{
	//	already open
	if (mDevice)
		return;

	mDevice = mOpenFunc(mSerial);

	//	re-enable streaming
	if (mDepthFormat.IsValid())
		EnableDepthStream(mDepthFormat);

	if (mColourFormat.IsValid())
		EnableColourStream(mColourFormat);
}


freenect_resolution Freenect::GetResolution(SoyPixelsMeta Meta,bool AllowHighResolution)
{
	if ( Meta.GetWidth() == LOW_WIDTH && Meta.GetHeight() == LOW_HEIGHT )
		return FREENECT_RESOLUTION_LOW;

	if ( Meta.GetWidth() == MEDIUM_WIDTH && Meta.GetHeight() == MEDIUM_HEIGHT )
		return FREENECT_RESOLUTION_MEDIUM;

	if ( AllowHighResolution )
		if ( Meta.GetWidth() == HIGH_WIDTH && Meta.GetHeight() == HIGH_HEIGHT )
			return FREENECT_RESOLUTION_HIGH;
	
	std::stringstream Error;
	Error << "Resolution " << Meta.GetWidth() << "x" << Meta.GetHeight() << " not supported. ";
	Error << " Low=" << LOW_WIDTH << "x" << LOW_HEIGHT;
	Error << " Medium=" << MEDIUM_WIDTH << "x" << MEDIUM_HEIGHT;
	if ( AllowHighResolution )
		Error << " High=" << HIGH_WIDTH << "x" << HIGH_HEIGHT;
	throw std::runtime_error(Error.str());
}

void Freenect::TDevice::EnableDepthStream(SoyPixelsMeta Meta)
{
	//	if already open, see if we can reconfigure, or doesn't need it
	if ( mDepthMode.is_valid )
	{
		//throw Soy::AssertException("Depth stream already open on device; todo: reopen/skip etc");
		std::Debug << "Depth stream already open on device" << std::endl;
		return;
	}
	
	auto Format = Freenect::GetDepthFormat( Meta.GetFormat() );
	//	high for depth will fail!
	auto Resolution = GetResolution( Meta, false );

	auto FrameMode = freenect_find_depth_mode( Resolution, Format );
	auto Result = freenect_set_depth_mode( mDevice, FrameMode );
	Freenect::IsOkay( Result, "freenect_set_depth_mode" );
	
	mDepthMode = FrameMode;

	freenect_set_depth_callback( mDevice, Freenect::OnDepthFrame );

	//	stop current mode
	//	gr: this had problems, just start
	//freenect_stop_depth( &Device );
	Result = freenect_start_depth( mDevice );
	Freenect::IsOkay( Result, "freenect_start_depth" );
	std::Debug << __PRETTY_FUNCTION__ << " Started depth stream" << std::endl;

	mDepthFormat = Meta;
}


void Freenect::TDevice::EnableColourStream(SoyPixelsMeta Meta)
{
	//	if already open, see if we can reconfigure, or doesn't need it
	if ( mColourMode.is_valid )
	{
		std::Debug << "Colour stream already open on device;" << std::endl;
		//throw Soy::AssertException("Colour stream already open on device; todo: reopen/skip etc");
		return;
	}
	
	auto Format = Freenect::GetColourFormat( Meta.GetFormat() );
	//	high for depth will fail!
	auto Resolution = GetResolution( Meta, true );
	
	auto FrameMode = freenect_find_video_mode( Resolution, Format );
	auto Result = freenect_set_video_mode( mDevice, FrameMode );
	Freenect::IsOkay( Result, "freenect_set_color_mode" );
	
	mColourMode = FrameMode;
	
	freenect_set_video_callback( mDevice, Freenect::OnColourFrame );
	
	//	stop current mode
	//	gr: this had problems, just start
	//freenect_stop_depth( &Device );
	Result = freenect_start_video( mDevice );
	Freenect::IsOkay( Result, "freenect_start_color" );
	std::Debug << __PRETTY_FUNCTION__ << " Started colour stream" << std::endl;
	
	mColourFormat = Meta;
}

SoyPixelsRemote Freenect::TDevice::GetDepthPixels(const uint8_t* PixelBytes_const)
{
	auto* PixelBytes = const_cast<uint8_t*>( PixelBytes_const );
	
	if ( !mDepthMode.is_valid )
		throw Soy_AssertException("Invalid depth mode");
	
	auto PixelFormat = GetFormat( mDepthMode.depth_format );
	auto Width = mDepthMode.width;
	auto Height = mDepthMode.height;
	auto Size = mDepthMode.bytes;
	
	//	gr: make a transform for this
	//	mDepthMode.padding_bits_per_pixel
	SoyPixelsRemote Pixels( PixelBytes, Width, Height, Size, PixelFormat );
	return Pixels;
}

SoyPixelsRemote Freenect::TDevice::GetColourPixels(const uint8_t* PixelBytes_const)
{
	auto* PixelBytes = const_cast<uint8_t*>( PixelBytes_const );
	
	if ( !mColourMode.is_valid )
		throw Soy_AssertException("Invalid colour mode");
	
	auto PixelFormat = GetFormat( mColourMode.video_format );
	auto Width = mColourMode.width;
	auto Height = mColourMode.height;
	auto Size = mColourMode.bytes;

	//	gr: make a transform for this
	//	mDepthMode.padding_bits_per_pixel
	SoyPixelsRemote Pixels( PixelBytes, Width, Height, Size, PixelFormat );
	return Pixels;
}







Freenect::TFreenect::TFreenect() :
	SoyThread	("Freenect")
{
	//	gr: windows requires libusb 1.0.22 minimum. Check this!

	freenect_usb_context* UsbContext = nullptr;
	auto Result = freenect_init( &mContext, UsbContext );
	Freenect::IsOkay( Result, "freenect_init" );
	
	freenect_set_log_level( mContext, FREENECT_LOG_NOTICE );
	freenect_set_log_callback( mContext, Freenect::LogCallback );
	
	freenect_device_flags flags = (freenect_device_flags)( FREENECT_DEVICE_CAMERA );
	freenect_select_subdevices( mContext, flags );
	
	//	start thread
	Start();
}

Freenect::TFreenect::~TFreenect()
{
	try
	{
		Stop(true);
	}
	catch (std::exception&e)
	{
		std::Debug << "Freenect thread cleanup error: " << e.what() << std::endl;
	}

	try
	{
		std::lock_guard<std::recursive_mutex> Lock(mDeviceLock);

		//	close all devices
		for ( auto d=0;	d<mDevices.GetSize();	d++ )
		{
			auto& Device = mDevices[d];
			Device.Close();
		}
		
		auto Error = freenect_shutdown( mContext );
		IsOkay( Error, "freenect_shutdown" );
	}
	catch(std::exception& e)
	{
		std::Debug << "Freenect cleanup error: "  << e.what() << std::endl;
	}
}

bool Freenect::TFreenect::ThreadIteration()
{
	if (mDeviceLock.try_lock())
	{
		try
		{
			//	re-open any devices we need to
			for (auto d = 0; d < mDevices.GetSize(); d++)
			{
				auto& Device = mDevices[d];
				Device.Open();
			}
		}
		catch (std::exception& e)
		{
			std::Debug << e.what() << std::endl;
		}
		mDeviceLock.unlock();
	}

	//	include timeout so we can shutdown without blocking thread
	int TimeoutMs = 30;
	int TimeoutSecs = 1;
	int TimeoutMicroSecs = TimeoutMs*1000;
	timeval Timeout = {TimeoutSecs,TimeoutMicroSecs};
	libusb_error Result = static_cast<libusb_error>( freenect_process_events_timeout( mContext, &Timeout ) );
	
	//	should we sleep here
	if ( Result == LIBUSB_SUCCESS )
		return true;
	

	if (Result == LIBUSB_ERROR_IO)
	{
		//	close and re-open devices
		ReacquireDevices();
	}


	//	handle error
	std::Debug << Result << std::endl;
	/*

		//	gr: -1 (also LIB_IO_USB_ERROR) can mean we've lost a device...
		//	https://github.com/OpenKinect/libfreenect/issues/229
		if ( Result == -1 )
		{
			ReacquireDevices();
		}
		else
		{
			Freenect::IsOkay( Result, "freenect_process_events_timeout" );
		}
	*/
	
	//	let thread breath?
	static size_t SleepMs = 10;
	std::this_thread::sleep_for( std::chrono::milliseconds(SleepMs) );
	return true;
}



void Freenect::TFreenect::ReacquireDevices()
{
	std::lock_guard<std::recursive_mutex> Lock(mDeviceLock);

	//	don't delete devices, but close them... make thread try and re-open
	for (auto d = 0; d < mDevices.GetSize(); d++)
	{
		auto& Device = mDevices[d];
		Device.Close();
	}
}

Freenect::TDevice& Freenect::TFreenect::GetDevice(freenect_device& DeviceRef)
{
	std::lock_guard<std::recursive_mutex> Lock(mDeviceLock);

	for ( auto d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = mDevices[d];
		if ( Device != DeviceRef )
			continue;
		return Device;
	}
	
	throw Soy_AssertException("Couldn't find opened device matching ref");
}

void Freenect::TFreenect::EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&& Formats)> Enum)
{
	struct freenect_device_attributes* DeviceList = nullptr;
	//	returns <0 on error, or number of devices.
	auto CountOrError = freenect_list_device_attributes( mContext, &DeviceList );
	if ( CountOrError < 0 )
		Freenect::IsOkay( CountOrError, "freenect_list_device_attributes" );
	
	try
	{
		auto Device = DeviceList;
		Array<std::string> Formats;
		
		//	should be a proper function doing this
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(HIGH_WIDTH,HIGH_HEIGHT,GetFormat(FREENECT_VIDEO_YUV_RGB)), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(HIGH_WIDTH,HIGH_HEIGHT,GetFormat(FREENECT_VIDEO_YUV_RAW)), 30 ) );
		//Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(LOW_WIDTH,LOW_HEIGHT,GetFormat(FREENECT_DEPTH_MM)), 30 );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(MEDIUM_WIDTH,MEDIUM_HEIGHT,GetFormat(FREENECT_VIDEO_YUV_RGB)), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(MEDIUM_WIDTH,MEDIUM_HEIGHT,GetFormat(FREENECT_VIDEO_YUV_RAW)), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(MEDIUM_WIDTH,MEDIUM_HEIGHT,GetFormat(FREENECT_DEPTH_MM)), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(LOW_WIDTH,LOW_HEIGHT, GetFormat(FREENECT_VIDEO_YUV_RGB) ), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(LOW_WIDTH,LOW_HEIGHT, GetFormat(FREENECT_VIDEO_YUV_RAW) ), 30 ) );
		Formats.PushBack( PopCameraDevice::GetFormatString( SoyPixelsMeta(LOW_WIDTH,LOW_HEIGHT, GetFormat(FREENECT_DEPTH_MM) ), 30 ) );

		while ( Device )
		{
			std::string Serial = Device->camera_serial;
			Enum( Serial, GetArrayBridge(Formats) );
			Device = Device->next;
		}
		freenect_free_device_attributes( DeviceList );
	}
	catch(...)
	{
		freenect_free_device_attributes( DeviceList );
		throw;
	}
}


//	gr: this function is returning an object that moves too easily!
//		switch to RAII TDevice as soon as this goes wrong
Freenect::TDevice& Freenect::TFreenect::OpenDevice(const std::string& Serial)
{
	std::lock_guard<std::recursive_mutex> Lock(mDeviceLock);
	
	//	return existing
	for ( auto d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = mDevices[d];
		if ( Device != Serial )
			continue;
		
		Device.Open();
		return Device;
	}
	
	auto OpenFunc = [this](const std::string& Serial)
	{
		freenect_device* Device = nullptr;

		auto Error = freenect_open_device_by_camera_serial( mContext, &Device, Serial.c_str());
		IsOkay(Error, "freenect_open_device_by_camera_serial");
		if (!Device)
			throw Soy::AssertException("freenect_open_device_by_camera_serial success, but null device");

		//	assign user data back to its owner
		freenect_set_user(Device, this);
		return Device;
	};

	//	make a new device, if it doesn't throw, keep it
	TDevice NewDevice( OpenFunc, Serial );
	auto& NewDevicePtr = mDevices.PushBack( NewDevice );
	
	std::Debug << "opened device " << Serial << std::endl;
	return NewDevicePtr;
}



void Freenect::TFreenect::OnDepthFrame(freenect_device& DevicePtr,const uint8_t* Bytes,SoyTime Timestamp)
{
	auto& Device = GetDevice( DevicePtr );
	
	//	format the bytes
	auto Pixels = Device.GetDepthPixels( Bytes );
	
	//	do callback
	if ( this->mOnDepthFrame )
		this->mOnDepthFrame( Device, Pixels, Timestamp );
}


void Freenect::TFreenect::OnColourFrame(freenect_device& DevicePtr,const uint8_t* Bytes,SoyTime Timestamp)
{
	auto& Device = GetDevice( DevicePtr );
	
	//	format the bytes
	auto Pixels = Device.GetColourPixels( Bytes );
	
	//	do callback
	if ( this->mOnColourFrame )
		this->mOnColourFrame( Device, Pixels, Timestamp );
}


void Freenect::TContext::OnFrame(TDevice& Device,const SoyPixelsImpl& Pixels,SoyTime Timestamp)
{
	//	call all listeners
	//	todo: replace with enum which locks internally
	for ( auto i=0;	i<mListeners.GetSize();	i++ )
	{
		auto& Listener = *mListeners[i];
		if ( Device != Listener.mSerial )
			continue;
		
		if ( !Listener.mOnFrame )
			continue;
		
		Listener.mOnFrame( Pixels, Timestamp );
	}
}



/*
 

#if defined(LIBFREENECT)
void Freenect::TContext::ReacquireDevices()
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
void Freenect::TContext::FreeContext()
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

void Freenect::TContext::Thread()
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
				Freenect::IsOkay( Result, "freenect_process_events_timeout" );
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



std::shared_ptr<Freenect::TDevice> Freenect::TContext::GetDevice(uint32 DeviceId)
{
	for ( int i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		if ( Device == DeviceId )
			return mDevices[i];
	}
	
	return nullptr;
}


std::shared_ptr<Freenect::TDevice> Freenect::TContext::GetDevice(const std::string& Name,const TVideoDecoderParams& Params)
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

void Freenect::TContext::EnumDevices(ArrayBridge<TDeviceMeta>&& Metas,bool AllowInvalidSerial)
{
	//	always include devices we've found before
	for ( int d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = *mDevices[d];
		Metas.PushBackUnique( Device.mMeta );
	}

	
}



Freenect::TDeviceMeta::TDeviceMeta(struct freenect_device_attributes& Device)
{
#if defined(LIBFREENECT)
	mName = Device.camera_serial;
	
	//	try and fill streams here based on... context support?
#endif
}


Freenect::TDevice::TDevice(const TDeviceMeta& Meta,const TVideoDecoderParams& Params,TContext& Context) :
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
void Freenect::TDevice::AcquireDevice()
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
			Freenect::IsOkay( Result, Error.str() );
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
void Freenect::TDevice::ReleaseDevice()
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
void Freenect::TDevice::CreateStreams(bool Video, bool Depth)
{
	try
	{
		if ( Video )
			mVideoStream.reset( new TVideoStream( *mDevice, SoyPixelsFormat::RGB, Freenect::VideoStreamIndex ) );
		
		//	a pause in here seems to help USB manage to create both
		std::this_thread::sleep_for( std::chrono::milliseconds(200) );
		
		if ( Depth )
			mDepthStream.reset( new TDepthStream( *mDevice, SoyPixelsFormat::Depth16mm, Freenect::DepthStreamIndex ) );
	}
	catch (std::exception& e)
	{
		mVideoStream.reset();
		mDepthStream.reset();
		throw;
	}

}
#endif

Freenect::TDevice::~TDevice()
{
#if defined(LIBFREENECT)
	ReleaseDevice();
#endif
}

void Freenect::TDevice::GetStreamMeta(ArrayBridge<TStreamMeta>& StreamMetas)
{
	if ( mVideoStream )
		StreamMetas.PushBack( mVideoStream->GetStreamMeta() );
	
	if ( mDepthStream )
		StreamMetas.PushBack( mDepthStream->GetStreamMeta() );
}





Freenect::TDeviceDecoder::TDeviceDecoder(const TVideoDecoderParams& Params,std::map<size_t,std::shared_ptr<TPixelBufferManager>>& PixelBufferManagers,std::map<size_t,std::shared_ptr<TAudioBufferManager>>& AudioBufferManagers,std::shared_ptr<Opengl::TContext> OpenglContext) :
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

Freenect::TDeviceDecoder::~TDeviceDecoder()
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

void Freenect::TDeviceDecoder::GetStreamMeta(ArrayBridge<TStreamMeta>&& StreamMetas)
{
	if ( !mDevice )
		return;
	
	mDevice->GetStreamMeta( StreamMetas );
}


TVideoMeta Freenect::TDeviceDecoder::GetMeta()
{
	TVideoMeta Meta;
	return Meta;
}

void Freenect::TDeviceDecoder::OnNewFrame(const Freenect::TFrame& Frame)
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
		case SoyPixelsFormat::Depth16mm:
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

bool Freenect::TDeviceDecoder::HasFatalError(std::string& Error)
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

*/

freenect_video_format Freenect::GetColourFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		//case SoyPixelsFormat::RGB:		return FREENECT_VIDEO_RGB;
		case SoyPixelsFormat::RGB:			return FREENECT_VIDEO_YUV_RGB;
		case SoyPixelsFormat::uyvy_8888:	return FREENECT_VIDEO_YUV_RAW;

		default:
			break;
	}
	
	return FREENECT_VIDEO_DUMMY;
}


freenect_depth_format Freenect::GetDepthFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::FreenectDepth11bit:	return FREENECT_DEPTH_11BIT;
		case SoyPixelsFormat::FreenectDepth10bit:	return FREENECT_DEPTH_10BIT;

		//	gr: don't use re-aligned, it's expensive
		//	use this in a shader!
		//	http://www.mindtreatstudios.com/kinect-3d-view-projection-matrix-rgb-camera/
		case SoyPixelsFormat::Depth16mm:		return FREENECT_DEPTH_REGISTERED;
		//case SoyPixelsFormat::Depth16mm:		return FREENECT_DEPTH_MM;
		default:
			break;
	}

	return FREENECT_DEPTH_DUMMY;
}


SoyPixelsFormat::Type Freenect::GetFormat(freenect_video_format Format)
{
	switch ( Format )
	{
		case FREENECT_VIDEO_RGB:		return SoyPixelsFormat::RGB;
		case FREENECT_VIDEO_YUV_RGB:	return SoyPixelsFormat::RGB;
		case FREENECT_VIDEO_YUV_RAW:	return SoyPixelsFormat::uyvy_8888;
		default:
			break;
	}
	
	return SoyPixelsFormat::Invalid;
}


SoyPixelsFormat::Type Freenect::GetFormat(freenect_depth_format Format)
{
	switch ( Format )
	{
		case FREENECT_DEPTH_11BIT:		return SoyPixelsFormat::FreenectDepth11bit;
		case FREENECT_DEPTH_10BIT:		return SoyPixelsFormat::FreenectDepth10bit;
		case FREENECT_DEPTH_REGISTERED:	return SoyPixelsFormat::Depth16mm;
		case FREENECT_DEPTH_MM:			return SoyPixelsFormat::Depth16mm;
		default:
			break;
	}
	
	return SoyPixelsFormat::Invalid;
}
/*

#if defined(LIBFREENECT)
void Freenect::OnVideoFrame(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	SoyTime Timecode = Freenect::TimestampToMs( timestamp );

	auto* usr = freenect_get_user(dev);
	auto DeviceId = reinterpret_cast<uint64>( usr );
	auto DeviceId32 = size_cast<uint32>( DeviceId );
	if ( !Freenect::gContext )
		return;
	
	auto Device = Freenect::gContext->GetDevice( DeviceId32 );
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
	
	Freenect::TFrame Frame( Pixels, Timecode, Stream->GetStreamMeta().mStreamIndex );
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
	if ( !Freenect::gContext )
		return;
	
	auto Device = Freenect::gContext->GetDevice( DeviceId32 );
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
	
	Freenect::TFrame Frame( Pixels, Timecode, Stream->GetStreamMeta().mStreamIndex );
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
Freenect::TStream::TStream(freenect_device& Device,size_t StreamIndex) :
	mDevice			( &Device )
{
	memset( &mFrameMode, 0, sizeof(mFrameMode) );
	
	mMeta.mStreamIndex = StreamIndex;
}
#endif


#if defined(LIBFREENECT)
Freenect::TVideoStream::TVideoStream(freenect_device& Device,SoyPixelsFormat::Type PixelFormat,size_t StreamIndex) :
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
	Freenect::IsOkay( Result, "freenect_set_video_mode" );
	//freenect_set_video_buffer( mDevice, rgb_back);

	//	stop current mode
	//	gr: commented out lets video start. maybe this is refcounted... so only turn off if we've started?
	//freenect_stop_video( mDevice );

	Result = freenect_start_video( &Device );
	Freenect::IsOkay( Result, "freenect_start_video" );

	freenect_set_video_callback( &Device, Freenect::OnVideoFrame );
	
	//	update meta
	SoyPixelsMeta PixelsMeta( mFrameMode.width, mFrameMode.height, Freenect::GetFormat(mFrameMode.video_format) );
	mMeta.mPixelMeta = PixelsMeta;
}
#endif

Freenect::TVideoStream::~TVideoStream()
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
Freenect::TDepthStream::TDepthStream(freenect_device& Device,SoyPixelsFormat::Type PixelFormat,size_t StreamIndex) :
	TStream	( Device, StreamIndex )
{
	auto Format = Freenect::GetDepthFormat( PixelFormat );
	//	gr: cannot use High
	auto Resolution = FREENECT_RESOLUTION_MEDIUM;
	
	mFrameMode = freenect_find_depth_mode( Resolution, Format );
	Soy::Assert( mFrameMode.is_valid, "Failed to get depth mode");

	//SoyPixelsFormat::GetFormatFromChannelCount( mVideoMode.data_bits_per_pixel/8 );

	auto Result = freenect_set_depth_mode( &Device, mFrameMode );
	Freenect::IsOkay( Result, "freenect_set_depth_mode" );

	//	stop current mode
	//freenect_stop_depth( &Device );
	Result = freenect_start_depth( &Device );
	Freenect::IsOkay( Result, "freenect_start_depth" );

	freenect_set_depth_callback( &Device, Freenect::OnDepthFrame );

	//	update meta
	SoyPixelsMeta PixelsMeta( mFrameMode.width, mFrameMode.height, Freenect::GetFormat(mFrameMode.depth_format) );
	mMeta.mPixelMeta = PixelsMeta;
}
#endif

Freenect::TDepthStream::~TDepthStream()
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



Freenect::TDepthPixelBuffer::TDepthPixelBuffer(const SoyPixelsImpl& Pixels,std::shared_ptr<Opengl::TBlitter> Blitter,std::shared_ptr<Opengl::TContext> Context) :
	mPixels			( Pixels ),
	mOpenglBlitter	( Blitter ),
	mOpenglContext	( Context )
{
}

Freenect::TDepthPixelBuffer::~TDepthPixelBuffer()
{
	PopWorker::DeferDelete( mOpenglContext, mLockedTexture, true );
	PopWorker::DeferDelete( mOpenglContext, mOpenglBlitter, false );
}


void Freenect::TDepthPixelBuffer::Lock(ArrayBridge<SoyPixelsImpl*>&& Textures,float3x3& Transform)
{
	Textures.PushBack( &mPixels );
}


void Freenect::TDepthPixelBuffer::Unlock()
{
//	mLockedTexture.reset();
}

*/


Freenect::TSource::TSource(std::string Serial,json11::Json& Params)
{
	if ( !Soy::StringTrimLeft( Serial, Freenect::DeviceName_Prefix, true ) )
		throw PopCameraDevice::TInvalidNameException();

	TCaptureParams CaptureParams(Params);

	auto& Context = GetContext();

	//SoyPixelsMeta Meta( 640, 480, SoyPixelsFormat::uyvy_8888 );
	SoyPixelsMeta ColourFormat( 640, 480, CaptureParams.mColourFormat );
	SoyPixelsMeta DepthFormat( 640, 480, CaptureParams.mDepthFormat );
	mListener = Context.CreateListener( Serial, ColourFormat, DepthFormat );
	
	mListener->mOnFrame = [&](const SoyPixelsImpl& Frame,SoyTime Timestamp)
	{
		this->OnFrame( Frame, Timestamp );
	};
}

Freenect::TSource::~TSource()
{
	//	clean up listener (this should be atomic, or locked or something)
	mListener->mOnFrame = nullptr;
}

	
void Freenect::TSource::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	throw Soy::AssertException("Freenect device doesn't support feature");
}

void Freenect::TSource::OnFrame(const SoyPixelsImpl& Frame,SoyTime Timestamp)
{
	float3x3 Transform;
	std::shared_ptr<TPixelBuffer> PixelBuffer( new TDumbPixelBuffer( Frame, Transform ) );
	
	json11::Json::object Meta;
	json11::Json::object CameraMeta;
	
	
	//	would like a more official source from libfreenect, but there isn't one :)
	//	http://smeenk.com/kinect-field-of-view-comparison/
	//	this source is from the offiical kinect sdk, but verify it ourselves some day
	float DepthHorzFov = 58.5f;
	float DepthVertFov = 46.6f;
	float ColourHorzFov = 62;
	float ColourVertFov = 48.6f;
	
	//	intrinsics from here
	//	https://github.com/OpenKinect/libfreenect2/issues/41
	auto Depth_fx = 368.096588f;	//	atan(this / res) should = fov ~58 above 
	auto Depth_fy = 368.096588f;
	auto Depth_cx = 261.696594f;
	auto Depth_cy = 202.522202f;
	auto Depth_zmin = 0.01f;
	auto Depth_zmax = 1<<16;
	json11::Json::array DepthIntrinsics(
	{
		Depth_fx,	0,			Depth_cx,
		0,			Depth_fy,	Depth_cy,
		//	z=1 in PopCameraMeta, forced to 1000 for viewport max for ios, this should probably be set here to
		//	the unit max (1<<16)
		0,			0,			0,		
	});

	if ( Frame.GetFormat() == SoyPixelsFormat::Depth16mm)
	{
		Meta["StreamName"] = DepthStreamName;
		Meta["DepthMax"] = FREENECT_DEPTH_MM_MAX_VALUE;
		Meta["DepthInvalid"] = FREENECT_DEPTH_MM_NO_VALUE;
		CameraMeta["HorizontalFov"] = DepthHorzFov;
		CameraMeta["VerticalFov"] = DepthVertFov;
		CameraMeta["Intrinsics"] = DepthIntrinsics;
	}
	else if ( Frame.GetFormat() == SoyPixelsFormat::FreenectDepth10bit || Frame.GetFormat() == SoyPixelsFormat::FreenectDepth11bit )
	{
		Meta["StreamName"] = DepthStreamName;
		Meta["DepthMax"] = FREENECT_DEPTH_RAW_MAX_VALUE;
		Meta["DepthInvalid"] = FREENECT_DEPTH_RAW_NO_VALUE;
		CameraMeta["HorizontalFov"] = DepthHorzFov;
		CameraMeta["VerticalFov"] = DepthVertFov;
		CameraMeta["Intrinsics"] = DepthIntrinsics;
	}
	else
	{
		Meta["StreamName"] = ColourStreamName;
		CameraMeta["HorizontalFov"] = ColourHorzFov;
		CameraMeta["VerticalFov"] = ColourVertFov;
	}
	
	Meta["Camera"] = CameraMeta;
	
	this->PushFrame( PixelBuffer, Timestamp, Meta );
}



