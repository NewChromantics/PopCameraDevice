#include "KinectAzure.h"
#include <k4abt.h>
#include <k4a/k4a.h>
#include "SoyDebug.h"
#include "HeapArray.hpp"
#include "MagicEnum/include/magic_enum.hpp"
#include "SoyRuntimeLibrary.h"
#include "SoyThread.h"
#include "SoyMedia.h"


#if !defined(ENABLE_KINECTAZURE)
#error Expected ENABLE_KINECTAZURE to be defined
#endif


namespace KinectAzure
{
	class TFrameReader;
	class TPixelReader;

	void		IsOkay(k4a_result_t Error, const char* Context);
	void		IsOkay(k4a_wait_result_t Error, const char* Context);
	void		IsOkay(k4a_buffer_result_t Error, const char* Context);
	void		InitDebugHandler();
	void		LoadDll();
	size_t		GetDeviceIndex(const std::string& Serial);

	SoyPixelsRemote			GetPixels(k4a_image_t Image);
	SoyPixelsFormat::Type	GetFormat(k4a_image_format_t Format);

	constexpr auto	SerialPrefix = "KinectAzure_";
}



//	a device just holds low-level handle
class KinectAzure::TDevice
{
public:
	TDevice(size_t DeviceIndex, k4a_depth_mode_t DepthMode, k4a_color_resolution_t ColourMode);
	~TDevice();

	std::string			GetSerial();

private:
	void				Shutdown();

public:
	k4a_device_t		mDevice = nullptr;
	k4a_calibration_t	mCalibration;
};


class KinectAzure::TFrameReader : public SoyThread
{
public:
	//	gr: because of the current use of the API, we have an option to keep alive here
	//		todo: pure RAII, but need to fix PopEngine first
	TFrameReader(size_t DeviceIndex,bool KeepAlive);
	~TFrameReader();

protected:
	virtual void		OnFrame(const k4a_capture_t Frame, k4a_imu_sample_t Imu, SoyTime CaptureTime) = 0;
	virtual void		OnImuMoved() {}
	virtual void		OnError(const char* Error) {}

	//	as we keep-alive, we need to know these modes
	virtual k4a_depth_mode_t		GetDepthMode() = 0;
	virtual k4a_color_resolution_t	GetColourMode() = 0;

private:
	void				Iteration(int32_t TimeoutMs);
	virtual bool		ThreadIteration() override;
	bool				HasImuMoved(k4a_imu_sample_t Imu);	//	check if the device has been moved

	void				Open();

private:
	std::shared_ptr<KinectAzure::TDevice>	mDevice;
	bool				mKeepAlive = false;
	size_t				mDeviceIndex = 0;
	vec3f				mLastAccell;			//	store accelleromter to detect movement & re-find floor plane
};



SoyPixelsMeta GetPixelMeta(k4a_depth_mode_t Mode, size_t& FrameRate)
{
	auto PixelFormat = SoyPixelsFormat::FreenectDepthmm;
	FrameRate = 30;

	switch (Mode)
	{
	case K4A_DEPTH_MODE_OFF:	return SoyPixelsMeta(0, 0, SoyPixelsFormat::Invalid);
	case K4A_DEPTH_MODE_NFOV_2X2BINNED:	return SoyPixelsMeta(320, 288, PixelFormat);
	case K4A_DEPTH_MODE_NFOV_UNBINNED:	return SoyPixelsMeta(640, 576, PixelFormat);
	case K4A_DEPTH_MODE_WFOV_2X2BINNED:	return SoyPixelsMeta(512, 512, PixelFormat);
	case K4A_DEPTH_MODE_WFOV_UNBINNED:	return SoyPixelsMeta(1024, 1024, PixelFormat);
		//case K4A_DEPTH_MODE_PASSIVE_IR: break;
	}

	std::stringstream Error;
	Error << "Unhandled K4A_COLOR_RESOLUTION_XXX " << Mode;
	throw Soy::AssertException(Error);
}

SoyPixelsMeta GetPixelMeta(k4a_color_resolution_t Mode, size_t& FrameRate, SoyPixelsFormat::Type PixelFormat = SoyPixelsFormat::Nv12)
{
	//	gr: maybe should use K4A_IMAGE_FORMAT_XXX
	FrameRate = 30;
	switch (Mode)
	{
	case K4A_COLOR_RESOLUTION_OFF:	return SoyPixelsMeta(0, 0, SoyPixelsFormat::Invalid);
	case K4A_COLOR_RESOLUTION_720P:	return SoyPixelsMeta(1280, 720, PixelFormat);
	case K4A_COLOR_RESOLUTION_1080P:	return SoyPixelsMeta(1920, 1080, PixelFormat);
	case K4A_COLOR_RESOLUTION_1440P:	return SoyPixelsMeta(2560, 1440, PixelFormat);
	case K4A_COLOR_RESOLUTION_1536P:	return SoyPixelsMeta(2048, 1536, PixelFormat);
	case K4A_COLOR_RESOLUTION_2160P:	return SoyPixelsMeta(3840, 2160, PixelFormat);
	case K4A_COLOR_RESOLUTION_3072P:	return SoyPixelsMeta(4096, 3072, PixelFormat);
	}

	std::stringstream Error;
	Error << "Unhandled K4A_COLOR_RESOLUTION_XXX " << Mode;
	throw Soy::AssertException(Error);
}


k4a_depth_mode_t GetDepthMode(SoyPixelsMeta Format)
{
	auto SameDim = [](SoyPixelsMeta a, SoyPixelsMeta b)
	{
		if (a.GetWidth() != b.GetWidth())return false;
		if (a.GetHeight() != b.GetHeight())return false;
		return true;
	};

	auto DepthFormats = 
	{ 
		K4A_DEPTH_MODE_NFOV_2X2BINNED,
		K4A_DEPTH_MODE_NFOV_UNBINNED,
		K4A_DEPTH_MODE_WFOV_2X2BINNED,
		K4A_DEPTH_MODE_WFOV_UNBINNED 
	};
	for (auto DepthFormat : DepthFormats)
	{
		//	require format to be right?
		size_t DummyFrameRate = 0;
		auto PixelFormat = GetPixelMeta(DepthFormat, DummyFrameRate);
		if (SameDim(Format, PixelFormat))
			return DepthFormat;
	}

	//	allow a way to get the no-depth mode
	if (Format == SoyPixelsMeta())
		return K4A_DEPTH_MODE_OFF;

	//	todo: if pixel format is right, return best res
	//	todo: get depth for mixed pixel format
	std::stringstream Error;
	Error << "Couldn't convert pixel meta to a depth format; " << Format;
	throw Soy::AssertException(Error);
}

k4a_color_resolution_t GetColourMode(SoyPixelsMeta Format)
{
	auto SameDim = [](SoyPixelsMeta a, SoyPixelsMeta b)
	{
		if (a.GetWidth() != b.GetWidth())return false;
		if (a.GetHeight() != b.GetHeight())return false;
		return true;
	};

	auto ColourFormats =
	{
		//K4A_COLOR_RESOLUTION_OFF,
		K4A_COLOR_RESOLUTION_720P,
		K4A_COLOR_RESOLUTION_1080P,
		K4A_COLOR_RESOLUTION_1440P,
		K4A_COLOR_RESOLUTION_1536P,
		K4A_COLOR_RESOLUTION_2160P,
		K4A_COLOR_RESOLUTION_3072P
	};
	for (auto ColourFormat : ColourFormats)
	{
		//	require format to be right?
		size_t DummyFrameRate = 0;
		auto PixelFormat = GetPixelMeta(ColourFormat, DummyFrameRate);
		if (SameDim(Format, PixelFormat))
			return ColourFormat;
	}

	//	allow a way to get the no-depth mode
	if (Format == SoyPixelsMeta())
		return K4A_COLOR_RESOLUTION_OFF;

	//	todo: if pixel format is right, return best res
	//	todo: get depth for mixed pixel format
	std::stringstream Error;
	Error << "Couldn't convert pixel meta to a colour format; " << Format;
	throw Soy::AssertException(Error);
}

SoyPixelsFormat::Type KinectAzure::GetFormat(k4a_image_format_t Format)
{
	switch (Format)
	{
	case K4A_IMAGE_FORMAT_CUSTOM16:
	case K4A_IMAGE_FORMAT_DEPTH16:
	case K4A_IMAGE_FORMAT_IR16:
		return SoyPixelsFormat::FreenectDepthmm;

	case K4A_IMAGE_FORMAT_COLOR_BGRA32:	return SoyPixelsFormat::BGRA;
	case K4A_IMAGE_FORMAT_COLOR_NV12:	return SoyPixelsFormat::Nv12;
	case K4A_IMAGE_FORMAT_COLOR_YUY2:	return SoyPixelsFormat::Yuv_844_Ntsc;
	case K4A_IMAGE_FORMAT_CUSTOM8:		return SoyPixelsFormat::Greyscale;

	default:
	case K4A_IMAGE_FORMAT_COLOR_MJPG:
	case K4A_IMAGE_FORMAT_CUSTOM:
		break;
	}

	std::stringstream Error;
	Error << "Unhandled pixel format " << magic_enum::enum_name(Format);
	throw Soy::AssertException(Error);
}


class KinectAzure::TPixelReader : public TFrameReader
{
public:
	TPixelReader(size_t DeviceIndex, bool KeepAlive, std::function<void(const SoyPixelsImpl&, SoyTime)> OnFrame, k4a_depth_mode_t DepthMode, k4a_color_resolution_t ColourMode) :
		TFrameReader	(DeviceIndex, KeepAlive),
		mOnNewFrame		(OnFrame),
		mDepthMode		( DepthMode ),
		mColourMode		( ColourMode )
	{
	}

private:
	virtual void		OnFrame(const k4a_capture_t Frame, k4a_imu_sample_t Imu, SoyTime CaptureTime) override;
	void				OnFrame(const SoyPixelsImpl& Frame, SoyTime CaptureTime);
	virtual k4a_depth_mode_t		GetDepthMode() override { return mDepthMode; }
	virtual k4a_color_resolution_t	GetColourMode() override { return mColourMode; }

	std::function<void(const SoyPixelsImpl&, SoyTime)>	mOnNewFrame;
	k4a_depth_mode_t		mDepthMode = K4A_DEPTH_MODE_OFF;
	k4a_color_resolution_t	mColourMode = K4A_COLOR_RESOLUTION_OFF;
};


void KinectAzure::LoadDll()
{
	static bool DllLoaded = false;

	//	load the lazy-load libraries
	if (DllLoaded)
		return;

	//	we should just try, because if the parent process has loaded it, this
	//	will just load
	Soy::TRuntimeLibrary DllK4a("k4a.dll");
	//Soy::TRuntimeLibrary DllK4abt("k4abt.dll");

	DllLoaded = true;
}

void KinectAzure::InitDebugHandler()
{
	LoadDll();

	auto OnDebug = [](void* Context, k4a_log_level_t Level, const char* Filename, const int Line, const char* Message)
	{
		std::Debug << "Kinect[" << magic_enum::enum_name(Level) << "] " << Filename << "(" << Line << "): " << Message << std::endl;
	};

	void* Context = nullptr;
	//auto DebugLevel = K4A_LOG_LEVEL_TRACE;
	auto DebugLevel = K4A_LOG_LEVEL_WARNING;
	auto Result = k4a_set_debug_message_handler(OnDebug, Context, DebugLevel);
	IsOkay(Result, "k4a_set_debug_message_handler");

	//	set these env vars for extra logging
	//	https://github.com/microsoft/Azure-Kinect-Sensor-SDK/issues/709
	//Platform::SetEnvVar("K4ABT_ENABLE_LOG_TO_A_FILE", "k4abt.log");
	//Platform::SetEnvVar("K4ABT_LOG_LEVEL", "i");	//	or W
}

void KinectAzure::EnumDeviceNameAndFormats(std::function<void(const std::string&,ArrayBridge<std::string>&&)> Enum)
{
	auto DeviceCount = k4a_device_get_installed_count();
	std::Debug << "KinectDevice count: " << DeviceCount << std::endl;

	//	formats are known
	//	todo: support depth & colour as seperate planes inheritely as multiple images
	Array<std::string> Formats;

	auto PushDepthFormat = [&](k4a_depth_mode_t Mode)
	{
		size_t FrameRate = 0;
		auto Meta = GetPixelMeta(Mode, FrameRate);
		auto FormatString = PopCameraDevice::GetFormatString(Meta, FrameRate);
		Formats.PushBack(FormatString);
	};

	auto PushColourFormat = [&](k4a_color_resolution_t Mode,SoyPixelsFormat::Type Format)
	{
		size_t FrameRate = 0;
		auto Meta = GetPixelMeta(Mode, FrameRate, Format);
		auto FormatString = PopCameraDevice::GetFormatString(Meta, FrameRate);
		Formats.PushBack(FormatString);
	};

	//	todo: colour & depth formats, including RGBA where a is depth

	//	this is in prefered order
	PushDepthFormat(K4A_DEPTH_MODE_NFOV_UNBINNED);
	PushDepthFormat(K4A_DEPTH_MODE_NFOV_2X2BINNED);
	PushDepthFormat(K4A_DEPTH_MODE_WFOV_UNBINNED);
	PushDepthFormat(K4A_DEPTH_MODE_WFOV_2X2BINNED);

	SoyPixelsFormat::Type ColourPixelFormats[] = { SoyPixelsFormat::Nv12, SoyPixelsFormat::RGB };
	for (auto Format : ColourPixelFormats)
	{
		PushColourFormat(K4A_COLOR_RESOLUTION_3072P, Format);
		PushColourFormat(K4A_COLOR_RESOLUTION_2160P, Format);
		PushColourFormat(K4A_COLOR_RESOLUTION_1536P, Format);
		PushColourFormat(K4A_COLOR_RESOLUTION_1440P, Format);
		PushColourFormat(K4A_COLOR_RESOLUTION_1080P, Format);
		PushColourFormat(K4A_COLOR_RESOLUTION_720P, Format);
	}

	for (auto i = 0; i < DeviceCount; i++)
	{
		try
		{
			KinectAzure::TDevice Device(i, K4A_DEPTH_MODE_OFF, K4A_COLOR_RESOLUTION_OFF);
			auto Serial = Device.GetSerial();

			Enum(Serial, GetArrayBridge(Formats));
		}
		catch (std::exception& e)
		{
			std::Debug << "Error getting kinect device serial: " << e.what() << std::endl;
		}
	}
}


size_t KinectAzure::GetDeviceIndex(const std::string& Serial)
{
	ssize_t SerialIndex = -1;
	size_t RunningIndex = 0;
	auto EnumName = [&](const std::string& DeviceSerial,ArrayBridge<std::string>& Formats)
	{
		if (SerialIndex >= 0)
			return;
		if (DeviceSerial == Serial)
			SerialIndex = RunningIndex;

		RunningIndex++;
	};
	EnumDeviceNameAndFormats(EnumName);

	if (SerialIndex < 0)
	{
		throw Soy::AssertException(std::string("No kinect with serial ") + Serial);
	}

	return SerialIndex;
}

KinectAzure::TDevice::TDevice(size_t DeviceIndex, k4a_depth_mode_t DepthMode, k4a_color_resolution_t ColourMode)
{
	//	this fails the second time if we didn't close properly (app still has exclusive access)
	//	so make sure we shutdown if we fail
	auto Error = k4a_device_open(DeviceIndex, &mDevice);
	IsOkay(Error, "k4a_device_open");

	//	don't start cameras if nothing selected (throws error)
	//	use this config for simple enum
	if (DepthMode == K4A_DEPTH_MODE_OFF && ColourMode == K4A_COLOR_RESOLUTION_OFF)
		return;

	try
	{
		k4a_device_configuration_t DeviceConfig = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		DeviceConfig.depth_mode = DepthMode;
		DeviceConfig.color_resolution = ColourMode;
		Error = k4a_device_start_cameras(mDevice, &DeviceConfig);
		IsOkay(Error, "k4a_device_start_cameras");
		
		//	start IMU so we can read gyro as a joint
		Error = k4a_device_start_imu(mDevice);
		IsOkay(Error, "k4a_device_start_imu");

		Error = k4a_device_get_calibration(mDevice, DeviceConfig.depth_mode, DeviceConfig.color_resolution, &mCalibration);
		IsOkay(Error, "k4a_device_get_calibration");

		/*
		k4abt_tracker_configuration_t TrackerConfig = K4ABT_TRACKER_CONFIG_DEFAULT;

		if (mGpuId == Kinect::GpuId::Cpu)
		{
			TrackerConfig.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_CPU;
		}
		else
		{
			TrackerConfig.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU;
			TrackerConfig.gpu_device_id = mGpuId;
		}

		Error = k4abt_tracker_create(&mCalibration, TrackerConfig, &mTracker);
		Kinect::IsOkay(Error, "k4abt_tracker_create");
		*/
	}
	catch (std::exception& e)
	{
		Shutdown();
		throw;
	}
}

KinectAzure::TDevice::~TDevice()
{
	try
	{
		Shutdown();
	}
	catch (std::exception& e)
	{
		std::Debug << "Error shutting down k4a device " << e.what() << std::endl;
	}
};

void KinectAzure::TDevice::Shutdown()
{
	if (!mDevice)
		return;
	std::Debug << "Shutdown" << std::endl;

	k4a_device_stop_imu(mDevice);
	k4a_device_stop_cameras(mDevice);
	//k4abt_tracker_shutdown(mTracker);
	//k4abt_tracker_destroy(mTracker);
	k4a_device_close(mDevice);
	mDevice = nullptr;
}


std::string KinectAzure::TDevice::GetSerial()
{
	size_t SerialNumberLength = 0;

	//	get size first
	auto Error = k4a_device_get_serialnum(mDevice, nullptr, &SerialNumberLength);
	if ( Error != K4A_BUFFER_RESULT_TOO_SMALL )
		IsOkay(Error, "k4a_device_get_serialnum(get length)");

	Array<char> SerialBuffer;
	SerialBuffer.SetSize(SerialNumberLength);
	SerialBuffer.PushBack('\0');
	Error = k4a_device_get_serialnum(mDevice, SerialBuffer.GetArray(), &SerialNumberLength);
	IsOkay(Error, "k4a_device_get_serialnum(get buffer)");

	std::string Serial(SerialBuffer.GetArray());
	return SerialPrefix + Serial;
}

KinectAzure::TCameraDevice::TCameraDevice(const std::string& Serial, const std::string& FormatString)
{
	if (!Soy::StringBeginsWith(Serial, KinectAzure::SerialPrefix, true))
		throw PopCameraDevice::TInvalidNameException();

	LoadDll();
	InitDebugHandler();

	auto DeviceIndex = GetDeviceIndex(Serial);
	//	todo: remove keep alive when PopEngine/CAPI is fixed
	auto KeepAlive = true;	//	keep reopening the device in the reader

	std::function<void(const SoyPixelsImpl&,SoyTime)> OnNewFrame = std::bind(&TCameraDevice::OnFrame, this, std::placeholders::_1, std::placeholders::_2);
	
	//	work out which reader we need to create
	//	gr: what default should we use, default to both?
	//	gr: this default should be first from EnumDeviceNameAndFormats()!
	size_t FrameRate = 0;
	SoyPixelsMeta Format = GetPixelMeta(K4A_DEPTH_MODE_NFOV_UNBINNED, FrameRate);
	PopCameraDevice::DecodeFormatString(FormatString, Format, FrameRate);

	auto DepthMode = GetDepthMode(Format);
	auto ColourMode = GetColourMode(Format);
	mReader.reset( new TPixelReader(DeviceIndex, KeepAlive, OnNewFrame, DepthMode, ColourMode) );
}

KinectAzure::TCameraDevice::~TCameraDevice()
{
}

void KinectAzure::TCameraDevice::OnFrame(const SoyPixelsImpl& Pixels,SoyTime Time)
{
	auto PixelMeta = Pixels.GetMeta();
	std::string Meta;
	float3x3 Transform;
	std::shared_ptr<TPixelBuffer> PixelBuffer(new TDumbPixelBuffer(Pixels, Transform));

	PushFrame(PixelBuffer, PixelMeta, Time, Meta);
}


void KinectAzure::TCameraDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature, bool Enable)
{
	throw Soy::AssertException("Feature not supported");
}



void KinectAzure::IsOkay(k4a_result_t Error, const char* Context)
{
	if (Error == K4A_RESULT_SUCCEEDED)
		return;

	std::stringstream ErrorString;
	ErrorString << "K4A error " << magic_enum::enum_name(Error) << " in " << Context;
	throw Soy::AssertException(ErrorString);
}


void KinectAzure::IsOkay(k4a_wait_result_t Error, const char* Context)
{
	if (Error == K4A_WAIT_RESULT_SUCCEEDED)
		return;

	std::stringstream ErrorString;
	ErrorString << "K4A wait error " << magic_enum::enum_name(Error) << " in " << Context;
	throw Soy::AssertException(ErrorString);
}


void KinectAzure::IsOkay(k4a_buffer_result_t Error, const char* Context)
{
	if (Error == K4A_BUFFER_RESULT_SUCCEEDED)
		return;

	std::stringstream ErrorString;
	ErrorString << "K4A wait error " << magic_enum::enum_name(Error) << " in " << Context;
	throw Soy::AssertException(ErrorString);
}

KinectAzure::TFrameReader::TFrameReader(size_t DeviceIndex,bool KeepAlive) :
	SoyThread		("TFrameReader"),
	mDeviceIndex	( DeviceIndex ),
	mKeepAlive		( KeepAlive )
{
	//	gr: problem here for non-keep alive as we may have params that have not yet been set, but stop it from booting
		//	try to open once to try and throw at construction (needed for non-keepalive anyway)
	if ( !mKeepAlive )
		Open();

	Start();
}

KinectAzure::TFrameReader::~TFrameReader()
{
	std::Debug << "~TFrameReader" << std::endl;
}

void KinectAzure::TFrameReader::Open()
{
	//	already ready
	if (mDevice)
		return;
	std::Debug << __PRETTY_FUNCTION__ << std::endl;


	_CrtCheckMemory();
	auto DepthMode = GetDepthMode();
	auto ColourMode = GetColourMode();
	mDevice.reset(new TDevice(mDeviceIndex, DepthMode, ColourMode));
}

bool KinectAzure::TFrameReader::ThreadIteration()
{
	//	gr: have a timeout, so we can abort if the thread is stopped
	//		5 secs is a good indication something has gone wrong I think...
	try
	{
		Open();

		int32_t Timeout = 5000;
		Iteration(Timeout);
	}
	catch (std::exception& e)
	{
		auto SleepMs = 1000;
		//	gr: if we get this, we should restart the capture/acquire device
		std::Debug << "Exception in TKinectAzureSkeletonReader loop: " << e.what() << " (Pausing for "<< SleepMs << "ms)" << std::endl;

		//	gr: for now at least send this in KeepAlive mode, 
		//		as the caller might send different params in the API (eg. GPU index)
		//		which may be the reason we're failing to iterate, this lets params change
		OnError(e.what());

		//	pause
		std::this_thread::sleep_for(std::chrono::milliseconds(SleepMs));

		if (mKeepAlive)
		{
			//	close device and let next iteration reopen
			std::Debug << "resetting mdevice" << std::endl; 
			mDevice.reset();
			std::Debug << "resetting mdevice done" << std::endl;
		}
		else
		{
			throw;
		}
	}
	return true;
}

bool KinectAzure::TFrameReader::HasImuMoved(k4a_imu_sample_t Imu)
{
	//	is this in G's?
	auto mAccellerationTolerance = 0.5f;

	auto x = fabsf(Imu.acc_sample.xyz.x - mLastAccell.x);
	auto y = fabsf(Imu.acc_sample.xyz.y - mLastAccell.y);
	auto z = fabsf(Imu.acc_sample.xyz.z - mLastAccell.z);
	auto BiggestMove = std::max(x, std::max(z, y));
	if (BiggestMove < mAccellerationTolerance)
		return false;

	//	save this as last move
	std::Debug << "Accellerometer moved (>" << mAccellerationTolerance << "); delta=" << x << "," << y << "," << z << std::endl;
	mLastAccell.x = Imu.acc_sample.xyz.x;
	mLastAccell.y = Imu.acc_sample.xyz.y;
	mLastAccell.z = Imu.acc_sample.xyz.z;
	return true;
}


void KinectAzure::TFrameReader::Iteration(int32_t TimeoutMs)
{
	if (!mDevice)
		throw Soy::AssertException("TFrameReader::Iteration null device");

	//auto& mTracker = mDevice->mTracker;
	auto& Device = mDevice->mDevice;

	k4a_capture_t Capture = nullptr;

	//	gr: if we wait(eg breaked) for a long time, this just keeps timing out
	//		like the device has gone to sleep? need to startvideo again?
	//		or abort if it keeps happening and let high level restart?
	//	get capture of one frame (if we don't refresh this, the skeleton doesn't move)

	//	docs:
	/*	* This function returns an error when an internal problem is encountered; such as loss of the USB connection, inability
		* to allocate enough memory, and other unexpected issues.Any error returned by this function signals the end of
		* streaming data, and caller should stop the stream using k4a_device_stop_cameras().
		*/
	auto WaitError = k4a_device_get_capture(Device, &Capture, TimeoutMs);
	/*	gr: if we disconnect the device, we just get timeout, so we can't tell the difference
	//		if timeout is high then we can assume its dead. so throw and let the system try and reconnect
	if (WaitError == K4A_WAIT_RESULT_TIMEOUT)
	{
		std::Debug << "Kinect get-capture timeout (" << TimeoutMs << "ms)" << std::endl;
		return;
	}
	*/
	IsOkay(WaitError, "k4a_device_get_capture");

	//	kinect provides a device timestamp (relative only to itself)
	//	and a system timestamp, but as our purposes are measurements inside our own system, lets stick to our own timestamps
	//	k4a_image_get_device_timestamp_usec
	//	k4a_image_get_system_timestamp_nsec
	SoyTime FrameCaptureTime(true);

	auto FreeCapture = [&]()
	{
		k4a_capture_release(Capture);
	};

	try
	{
		/*
		//	update smoothing setting
		k4abt_tracker_set_temporal_smoothing(mTracker, mSmoothing);

		//	gr: apparently this enqueue should never timeout?
		//	https://github.com/microsoft/Azure-Kinect-Samples/blob/master/body-tracking-samples/simple_cpp_sample/main.cpp#L69
		//	request skeleton
		auto WaitError = k4abt_tracker_enqueue_capture(mTracker, Capture, TimeoutMs);
		IsOkay(WaitError, "k4abt_tracker_enqueue_capture");
		*/
		//	now sample IMU as close as to the capture time, but after we've queued the tracker 
		//	call as we want to do that ASAP
		k4a_imu_sample_t ImuSample;
		ImuSample.gyro_timestamp_usec = 0;
		{
			int LoopSafety = 1000;
			while (LoopSafety--)
			{
				//	get every queued sample until we get a timeout error as they sample at a higher frequency than frames
				//	and we just want latest
				auto ImuError = k4a_device_get_imu_sample(Device, &ImuSample, 0);
				//	no more samples
				if (ImuError == K4A_WAIT_RESULT_TIMEOUT)
					break;
				IsOkay(ImuError, "k4a_device_get_imu_sample");
			}
		}
		//	contents haven't changed, didn't get a result
		if (ImuSample.gyro_timestamp_usec == 0)
		{
			//	gr: getting this oddly often
			std::Debug << "AzureKinect got 0 IMU samples in capture" << std::endl;
			//throw Soy::AssertException("IMU had no samples");
		}

		if (HasImuMoved(ImuSample))
		{
			OnImuMoved();
			/*
			std::Debug << "IMU moved, calculating new floor..." << std::endl;
			FindFloor(Capture,ImuSample);
			*/
		}
	
		//	extract skeletons
		OnFrame(Capture, ImuSample, FrameCaptureTime);

		//	cleanup
		//k4abt_frame_release(Frame);
		FreeCapture();
	}
	catch (std::exception& e)
	{
		FreeCapture();
		throw;
	}
	catch (...)
	{
		FreeCapture();
		throw;
	}
}


void KinectAzure::TPixelReader::OnFrame(const SoyPixelsImpl& Frame,SoyTime CaptureTime)
{
	mOnNewFrame(Frame, CaptureTime);
}


//	to minimise copies, we return remote pixels
SoyPixelsRemote KinectAzure::GetPixels(k4a_image_t Image)
{
	auto* PixelBuffer = k4a_image_get_buffer(Image);
	auto PixelBufferSize = k4a_image_get_size(Image);
	auto Width = k4a_image_get_width_pixels(Image);
	auto Height = k4a_image_get_height_pixels(Image);
	auto Format = k4a_image_get_format(Image);
	auto PixelFormat = GetFormat(Format);

	SoyPixelsRemote ImagePixels(PixelBuffer, Width, Height, PixelBufferSize, PixelFormat);
	return ImagePixels;
}

void KinectAzure::TPixelReader::OnFrame(const k4a_capture_t Frame,k4a_imu_sample_t Imu, SoyTime CaptureTime)
{
	auto DepthImage = k4a_capture_get_depth_image(Frame);
	auto DepthPixels = GetPixels(DepthImage);
	
	OnFrame( DepthPixels, CaptureTime );

	k4a_image_release(DepthImage);
}



