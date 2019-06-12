#include "Freenect.h"
#include "SoyJson.h"


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
	const std::string			DeviceName_Colour_Suffix = "_Colour";
	const std::string			DeviceName_Depth_Suffix = "_Depth";
	
	
	namespace TStream
	{
		enum TYPE
		{
			Depth,
			Colour,
		};
	}
}


std::ostream &operator<<(std::ostream &out,const Freenect::TStream::TYPE& Type)
{
	switch ( Type )
	{
		case Freenect::TStream::Depth:	out << Freenect::DeviceName_Depth_Suffix;	break;
		case Freenect::TStream::Colour:	out << Freenect::DeviceName_Colour_Suffix;	break;
		default:
			out << "<?" << static_cast<int>(Type) << ">";
			break;
	}
	return out;
}


//	NOT RAII atm
class Freenect::TDevice
{
public:
	TDevice()
	{
		mDepthMode.is_valid = false;
	}
	TDevice(freenect_device& Device,const std::string& Serial)
	{
		mDevice = &Device;
		mSerial = Serial;
		mDepthMode.is_valid = false;
	}
	
	void			EnableDepthStream(SoyPixelsMeta Format);
	void			EnableColourStream(SoyPixelsMeta Format);
	void			Close();
	SoyPixelsRemote	GetDepthPixels(const uint8_t* Pixels);
	SoyPixelsRemote	GetColourPixels(const uint8_t* Pixels);

	//	todo; can we compare two device pointers?
	bool			operator==(const std::string& Serial) const	{	return mSerial == Serial;	}
	bool			operator!=(const std::string& Serial) const	{	return mSerial != Serial;	}
	bool			operator==(const TDevice& Device) const		{	return mSerial == Device.mSerial;	}
	bool			operator!=(const TDevice& Device) const		{	return mSerial != Device.mSerial;	}
	bool			operator==(const freenect_device& Device) const		{	return mDevice == &Device;	}
	bool			operator!=(const freenect_device& Device) const		{	return mDevice != &Device;	}

	
	freenect_frame_mode	mDepthMode;
	freenect_frame_mode	mColourMode;
	freenect_device*	mDevice = nullptr;
	std::string			mSerial;
};

class Freenect::TFrameListener
{
public:
	std::string		mSerial;
	TStream::TYPE	mStream;
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
	
	void				EnumDevices(std::function<void(const std::string&)>& Enum);
	TDevice&			OpenDevice(const std::string& Serial);
	void				OnDepthFrame(freenect_device& Device,const uint8_t* Bytes,SoyTime Timestamp);
	void				OnColourFrame(freenect_device& Device,const uint8_t* Bytes,SoyTime Timestamp);
	TDevice&			GetDevice(freenect_device& Device);

private:
	virtual void		Thread() override;
	
public:
	std::function<void(TDevice&,const SoyPixelsImpl&,SoyTime)>	mOnDepthFrame;
	std::function<void(TDevice&,const SoyPixelsImpl&,SoyTime)>	mOnColourFrame;

private:
	Array<TDevice>		mDevices;	//	devices we've opened
	freenect_context*	mContext = nullptr;
};


class Freenect::TContext
{
public:
	TFreenect&						GetFreenect();
	
	std::shared_ptr<TFrameListener>	CreateListener(const std::string& Serial,SoyPixelsMeta Format);
	
private:
	void							OnFrame(TDevice& Device,TStream::TYPE Stream,const SoyPixelsImpl& Pixels,SoyTime Timestamp);

public:
	//	 lock this!
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


void Freenect::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	auto& Context = GetContext();
	auto& Freenect = Context.GetFreenect();
	
	std::function<void(const std::string&)> EnumDeviceSerial = [&](const std::string& Serial)
	{
		//	would be good to know what capabilities it has...
		{
			std::stringstream Name;
			Name << DeviceName_Prefix << Serial << TStream::Depth;
			Enum( Name.str() );
		}
		{
			std::stringstream Name;
			Name << DeviceName_Prefix << Serial << TStream::Colour;
			Enum( Name.str() );
		}
	};
	
	Freenect.EnumDevices( EnumDeviceSerial );
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


Freenect::TFreenect& Freenect::TContext::GetFreenect()
{
	if ( !mFreenect )
	{
		mFreenect.reset( new TFreenect() );
		mFreenect->mOnDepthFrame = std::bind( &TContext::OnFrame, this, std::placeholders::_1, TStream::Depth, std::placeholders::_2, std::placeholders::_3 );
		mFreenect->mOnColourFrame = std::bind( &TContext::OnFrame, this, std::placeholders::_1, TStream::Colour, std::placeholders::_2, std::placeholders::_3 );
	}
	return *mFreenect;
}

bool IsDepthFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::FreenectDepthmm:
		case SoyPixelsFormat::KinectDepth:
		case SoyPixelsFormat::FreenectDepth10bit:
		case SoyPixelsFormat::FreenectDepth11bit:
			return true;
			
		default:
			return false;
	}
}

std::shared_ptr<Freenect::TFrameListener> Freenect::TContext::CreateListener(const std::string& Serial,SoyPixelsMeta Format)
{
	//	try and open the device first, so this will fail the caller
	auto& Freenect = GetFreenect();
	auto& Device = Freenect.OpenDevice( Serial );

	TStream::TYPE Stream;
	if ( IsDepthFormat( Format.GetFormat() ) )
	{
		Device.EnableDepthStream( Format );
		Stream = TStream::Depth;
	}
	else
	{
		Device.EnableColourStream( Format );
		Stream = TStream::Colour;
	}

	//	create the listener
	std::shared_ptr<TFrameListener> Listener( new TFrameListener );
	Listener->mSerial = Serial;
	Listener->mStream = Stream;
	
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

	Result = freenect_close_device( mDevice );
	IsOkay( Result, "freenect_close_device" );
	
	mDevice = nullptr;
}

freenect_resolution GetResolution(SoyPixelsMeta Meta,bool AllowHighResolution)
{
#define LOW_WIDTH		320
#define LOW_HEIGHT		240
#define MEDIUM_WIDTH	640
#define MEDIUM_HEIGHT	480
#define HIGH_WIDTH		1280
#define HIGH_HEIGHT		1024

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
	throw Soy::AssertException(Error);
}

void Freenect::TDevice::EnableDepthStream(SoyPixelsMeta Meta)
{
	//	if already open, see if we can reconfigure, or doesn't need it
	if ( mDepthMode.is_valid )
		throw Soy::AssertException("Depth stream already open on device; todo: reopen/skip etc");
	
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
}


void Freenect::TDevice::EnableColourStream(SoyPixelsMeta Meta)
{
	//	if already open, see if we can reconfigure, or doesn't need it
	if ( mColourMode.is_valid )
		throw Soy::AssertException("Colour stream already open on device; todo: reopen/skip etc");
	
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
}

SoyPixelsRemote Freenect::TDevice::GetDepthPixels(const uint8_t* PixelBytes_const)
{
	auto* PixelBytes = const_cast<uint8_t*>( PixelBytes_const );
	
	if ( !mDepthMode.is_valid )
		throw Soy_AssertException("Invalid depth mode");
	
	auto PixelFormat = GetFormat( mDepthMode.depth_format );
	auto Width = mDepthMode.width;
	auto Height = mDepthMode.height;
	
	//	data size not provided!
	auto Size = SoyPixelsMeta( Width, Height, PixelFormat ).GetDataSize();
	
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
	
	auto PixelFormat = GetFormat( mColourMode.depth_format );
	auto Width = mColourMode.width;
	auto Height = mColourMode.height;
	
	//	data size not provided!
	auto Size = SoyPixelsMeta( Width, Height, PixelFormat ).GetDataSize();
	
	//	gr: make a transform for this
	//	mDepthMode.padding_bits_per_pixel
	SoyPixelsRemote Pixels( PixelBytes, Width, Height, Size, PixelFormat );
	return Pixels;
}







Freenect::TFreenect::TFreenect() :
	SoyThread	("Freenect")
{
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
	Stop( true );
	
	try
	{
		//	close all devicse
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

void Freenect::TFreenect::Thread()
{
	//	include timeout so we can shutdown without blocking thread
	int TimeoutMs = 30;
	int TimeoutSecs = 1;
	int TimeoutMicroSecs = TimeoutMs*1000;
	timeval Timeout = {TimeoutSecs,TimeoutMicroSecs};
	libusb_error Result = static_cast<libusb_error>( freenect_process_events_timeout( mContext, &Timeout ) );
	
	//	should we sleep here
	if ( Result == LIBUSB_SUCCESS )
		return;
	
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
}


Freenect::TDevice& Freenect::TFreenect::GetDevice(freenect_device& DeviceRef)
{
	for ( auto d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = mDevices[d];
		if ( Device != DeviceRef )
			continue;
		return Device;
	}
	
	throw Soy_AssertException("Couldn't find opened device matching ref");
}

void Freenect::TFreenect::EnumDevices(std::function<void (const std::string &)>& Enum)
{
	struct freenect_device_attributes* DeviceList = nullptr;
	//	returns <0 on error, or number of devices.
	auto CountOrError = freenect_list_device_attributes( mContext, &DeviceList );
	if ( CountOrError < 0 )
		Freenect::IsOkay( CountOrError, "freenect_list_device_attributes" );
	
	try
	{
		auto Device = DeviceList;
		while ( Device )
		{
			std::string Serial = Device->camera_serial;
			Enum( Serial );
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
	//	return existing
	for ( auto d=0;	d<mDevices.GetSize();	d++ )
	{
		auto& Device = mDevices[d];
		if ( Device != Serial )
			continue;
		
		return Device;
	}
	
	freenect_device* Device = nullptr;
	auto Error = freenect_open_device_by_camera_serial( mContext, &Device, Serial.c_str() );
	IsOkay( Error, "freenect_open_device_by_camera_serial");
	if ( !Device )
		throw Soy::AssertException("freenect_open_device_by_camera_serial success, but null device");
		
	//	assign user data back to its owner
	freenect_set_user( Device, this );
	
	TDevice NewDevice( *Device, Serial );
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


void Freenect::TContext::OnFrame(TDevice& Device,TStream::TYPE Stream,const SoyPixelsImpl& Pixels,SoyTime Timestamp)
{
	//	call all listeners
	//	todo: replace with enum which locks internally
	for ( auto i=0;	i<mListeners.GetSize();	i++ )
	{
		auto& Listener = *mListeners[i];
		if ( Device != Listener.mSerial )
			continue;
		if ( Listener.mStream != Stream )
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
		//case SoyPixelsFormat::RGB:	return FREENECT_VIDEO_RGB;
		case SoyPixelsFormat::RGB:	return FREENECT_VIDEO_YUV_RGB;
		case SoyPixelsFormat::uyvy:	return FREENECT_VIDEO_YUV_RAW;

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
		case SoyPixelsFormat::FreenectDepthmm:		return FREENECT_DEPTH_REGISTERED;
		//case SoyPixelsFormat::FreenectDepthmm:		return FREENECT_DEPTH_MM;
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
		case FREENECT_VIDEO_YUV_RAW:	return SoyPixelsFormat::uyvy;
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
		case FREENECT_DEPTH_REGISTERED:	return SoyPixelsFormat::FreenectDepthmm;
		case FREENECT_DEPTH_MM:			return SoyPixelsFormat::FreenectDepthmm;
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


Freenect::TSource::TSource(const std::string& DeviceName)
{
	std::string Serial = DeviceName;
	if ( !Soy::StringTrimLeft( Serial, Freenect::DeviceName_Prefix, true ) )
	{
		std::stringstream Error;
		Error << "Device name (" << DeviceName << ") doesn't begin with " << DeviceName_Prefix;
		throw Soy_AssertException(Error);
	}

	auto& Context = GetContext();

	if ( Soy::StringTrimRight( Serial, DeviceName_Colour_Suffix, true ) )
	{
		SoyPixelsMeta Meta( 640, 480, SoyPixelsFormat::uyvy );
		mListener = Context.CreateListener( Serial, Meta );
	}
	else if ( Soy::StringTrimRight( Serial, DeviceName_Depth_Suffix, true ) )
	{
		SoyPixelsMeta Meta( 640, 480, SoyPixelsFormat::FreenectDepthmm );
		mListener = Context.CreateListener( Serial, Meta );
	}
	else
	{
		std::stringstream Error;
		Error << "Device name (" << DeviceName << ") doesn't end with " << DeviceName_Colour_Suffix << " or " << DeviceName_Depth_Suffix;
		throw Soy_AssertException(Error);
	}
	
	mListener->mOnFrame = [&](const SoyPixelsImpl& Frame,SoyTime Timestamp)
	{
		this->OnFrame( Frame, Timestamp );
	};
}

Freenect::TSource::~TSource()
{
}

	
void Freenect::TSource::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	throw Soy::AssertException("Freenect device doesn't support feature");
}

void Freenect::TSource::OnFrame(const SoyPixelsImpl& Frame,SoyTime Timestamp)
{
	float3x3 Transform;
	std::shared_ptr<TPixelBuffer> PixelBuffer( new TDumbPixelBuffer( Frame, Transform ) );
	
	TJsonWriter Meta;
	Meta.Push("Time", Timestamp.GetMilliSeconds().count() );
	
	//	would like a more official source from libfreenect, but there isn't one :)
	//	http://smeenk.com/kinect-field-of-view-comparison/
	//	this source is from the offiical kinect sdk, but verify it ourselves some day
	float DepthHorzFov = 58.5f;
	float DepthVertFov = 46.6f;
	float ColourHorzFov = 62;
	float ColourVertFov = 48.6f;
	if ( Frame.GetFormat() == SoyPixelsFormat::FreenectDepthmm )
	{
		Meta.Push("DepthMax",FREENECT_DEPTH_MM_MAX_VALUE);
		Meta.Push("DepthInvalid",FREENECT_DEPTH_MM_NO_VALUE);
		Meta.Push("HorzFov",DepthHorzFov);
		Meta.Push("VertFov",DepthVertFov);
	}
	else if ( Frame.GetFormat() == SoyPixelsFormat::FreenectDepth10bit || Frame.GetFormat() == SoyPixelsFormat::FreenectDepth11bit )
	{
		Meta.Push("DepthMax",FREENECT_DEPTH_RAW_MAX_VALUE);
		Meta.Push("DepthInvalid",FREENECT_DEPTH_RAW_NO_VALUE);
		Meta.Push("HorzFov",DepthHorzFov);
		Meta.Push("VertFov",DepthVertFov);
	}
	else
	{
		Meta.Push("HorzFov",ColourHorzFov);
		Meta.Push("VertFov",ColourVertFov);
	}
	
	auto MetaString = Meta.GetString();
	
	this->PushFrame( PixelBuffer, Frame.GetMeta(), MetaString );
}



