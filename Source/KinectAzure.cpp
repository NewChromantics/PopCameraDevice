#include "KinectAzure.h"
#include "SoyDebug.h"
#include "HeapArray.hpp"
#include <magic_enum/include/magic_enum/magic_enum.hpp>
#include "SoyThread.h"
#include "SoyMedia.h"
#include <cmath>	//	fabsf
#include <SoyFilesystem.h>
#include "PopCameraDevice.h"

//	these macros are missing on linux
#if defined(TARGET_LINUX)
#define K4A_DEPRECATED	__attribute__((deprecated))
#define K4A_STATIC_DEFINE
#endif
#include <k4a/k4a.h>

#if defined(TARGET_WINDOWS)
#define K4A_DLL	"k4a.dll"
#endif

#if defined(TARGET_LINUX)
//#define K4A_DLL	"libk4a.so"
//#define DEPTHENGINE_DLL	"libdepthengine.so"
#endif

#if defined(K4A_DLL) || defined(DEPTHENGINE_DLL)
#include "SoyRuntimeLibrary.h"
#endif

#if !defined(ENABLE_KINECTAZURE)
#error Expected ENABLE_KINECTAZURE to be defined
#endif


class k4a_colour_mode_t
{
public:
	k4a_image_format_t		format = K4A_IMAGE_FORMAT_CUSTOM;
	k4a_color_resolution_t	resolution = K4A_COLOR_RESOLUTION_OFF;
};

namespace KinectAzure
{
	class TFrameReader;
	class TPixelReader;
	class TCaptureFrame;

	void		IsOkay(k4a_result_t Error, const char* Context);
	void		IsOkay(k4a_wait_result_t Error, const char* Context);
	void		IsOkay(k4a_buffer_result_t Error, const char* Context);
	void		InitDebugHandler(bool Verbose);
	void		LoadDll();
	size_t		GetDeviceIndex(const std::string& Serial);

	SoyPixelsRemote			GetPixels(k4a_image_t Image);
	SoyPixelsFormat::Type	GetFormat(k4a_image_format_t Format);
	k4a_image_format_t		GetFormat(SoyPixelsFormat::Type Format);
	k4a_image_format_t		GetColourFormat(SoyPixelsFormat::Type Format);
	k4a_colour_mode_t		GetColourMode(SoyPixelsMeta Format,size_t& FrameRate);
	k4a_depth_mode_t		GetDepthMode(SoyPixelsMeta Format, size_t& FrameRate);
	k4a_fps_t				GetFrameRate(size_t FrameRate);
	k4a_color_resolution_t	GetLargestColourResolution(SoyPixelsFormat::Type Format);

	constexpr auto	SerialPrefix = "KinectAzure_";

	//	default to best format
	auto const static K4A_DEPTH_MODE_DEFAULT = K4A_DEPTH_MODE_NFOV_UNBINNED;
}



//	a device just holds low-level handle
class KinectAzure::TDevice
{
public:
	TDevice(size_t DeviceIndex, k4a_depth_mode_t DepthMode, k4a_colour_mode_t ColourMode, k4a_fps_t FrameRate, k4a_wired_sync_mode_t SyncMode);
	~TDevice();

	std::string				GetSerial();
	k4a_transformation_t	GetDepthToImageTransform();

private:
	void					Shutdown();

public:
	k4a_device_t			mDevice = nullptr;
	k4a_calibration_t		mCalibration;
	k4a_transformation_t	mTransformation = nullptr;

};


class KinectAzure::TFrameReader : public SoyThread
{
public:
	//	gr: because of the current use of the API, we have an option to keep alive here
	//		todo: pure RAII, but need to fix PopEngine first
	TFrameReader(size_t DeviceIndex,bool KeepAlive,bool VerboseDebug);
	~TFrameReader();

protected:
	virtual void		OnFrame(TCaptureFrame& Frame) = 0;
	virtual void		OnImuMoved() {}
	virtual void		OnError(const char* Error) {}

	//	as we keep-alive, we need to know these modes
	virtual k4a_depth_mode_t		GetDepthMode() = 0;
	virtual k4a_colour_mode_t		GetColourMode() = 0;
	bool							IsExpectingDepthAndColour();
	virtual k4a_fps_t				GetFrameRate() = 0;
	k4a_calibration_t				GetCalibration() { return mDevice->mCalibration; }
	k4a_transformation_t			GetDepthToImageTransform() { return mDevice->GetDepthToImageTransform(); }
	virtual k4a_wired_sync_mode_t	GetSyncMode() = 0;

private:
	void				Iteration(int32_t TimeoutMs);
	virtual bool		ThreadIteration() override;
	bool				HasImuMoved(k4a_imu_sample_t Imu);	//	check if the device has been moved

	void				Open();

protected:
	std::mutex								mDeviceLock;	//	to prevent nulling mDevice whilst in use
	std::shared_ptr<KinectAzure::TDevice>	mDevice;
	bool				mVerboseDebug = false;

private:
	bool				mKeepAlive = false;
	size_t				mDeviceIndex = 0;
	vec3f				mLastAccell;			//	store accelleromter to detect movement & re-find floor plane
};


class KinectAzure::TCaptureFrame
{
public:
	k4a_capture_t			mCapture = nullptr;
	k4a_imu_sample_t		mImu = { 0 };
	SoyTime					mTime;
	k4a_calibration_t		mCalibration = { 0 };
	k4a_transformation_t	mDepthToImageTransform = nullptr;
	bool					mSyncInCable = false;
	bool					mSyncOutCable = false;
};

SoyPixelsMeta GetPixelMeta(k4a_depth_mode_t Mode, size_t& FrameRate)
{
	auto PixelFormat = SoyPixelsFormat::Depth16mm;
	if ( FrameRate==0 )
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
	Error << "Unhandled K4A_DEPTH_MODE_XXX " << Mode;
	throw Soy::AssertException(Error);
}


size_t GetMaxFrameRate(k4a_color_resolution_t Mode)
{
	switch (Mode)
	{
	case K4A_COLOR_RESOLUTION_OFF:	
		return 0;

	case K4A_COLOR_RESOLUTION_720P:
	case K4A_COLOR_RESOLUTION_1080P:
	case K4A_COLOR_RESOLUTION_1440P:
	case K4A_COLOR_RESOLUTION_1536P:
	case K4A_COLOR_RESOLUTION_2160P:
		return 30;

	case K4A_COLOR_RESOLUTION_3072P:
		return 15;
	}

	std::stringstream Error;
	Error << __PRETTY_FUNCTION__ << " Unhandled K4A_COLOR_RESOLUTION_XXX " << Mode;
	throw Soy::AssertException(Error);
}

SoyPixelsMeta GetPixelMeta(k4a_color_resolution_t Mode, size_t& FrameRate, SoyPixelsFormat::Type PixelFormat)
{
	if (FrameRate == 0)
		FrameRate = GetMaxFrameRate(Mode);

	switch (Mode)
	{
	case K4A_COLOR_RESOLUTION_OFF:		return SoyPixelsMeta(0, 0, SoyPixelsFormat::Invalid);
	case K4A_COLOR_RESOLUTION_720P:		return SoyPixelsMeta(1280, 720, PixelFormat);
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


k4a_depth_mode_t KinectAzure::GetDepthMode(SoyPixelsMeta Format, size_t& FrameRate)
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

	//	allow a way to get the no-depth mode
	if (Format == SoyPixelsMeta())
		return K4A_DEPTH_MODE_OFF;

	//	look for size matches
	//	gr: should also do framerate if !=0
	for (auto DepthFormat : DepthFormats)
	{
		size_t DummyFrameRate = FrameRate;
		auto PixelFormat = GetPixelMeta(DepthFormat, DummyFrameRate);
		if (SameDim(Format, PixelFormat))
		{
			//	output framerate for this configuration
			FrameRate = DummyFrameRate;
			return DepthFormat;
		}
	}

	//	no size matches, so return something if they requested a depth
	switch (Format.GetFormat())
	{
	case SoyPixelsFormat::Depth16mm:
	{
		//	get framerate
		auto DepthFormat = K4A_DEPTH_MODE_DEFAULT;
		GetPixelMeta(DepthFormat, FrameRate);
		return DepthFormat;
	}
	default:break;
	}

	std::stringstream Error;
	Error << "Couldn't convert pixel meta to a depth format; " << Format;
	throw Soy::AssertException(Error);
}

k4a_fps_t KinectAzure::GetFrameRate(size_t FrameRate)
{
	switch (FrameRate)
	{
	case 5:		return K4A_FRAMES_PER_SECOND_5;
	case 15:	return K4A_FRAMES_PER_SECOND_15;
	case 30:	return K4A_FRAMES_PER_SECOND_30;
	}

	std::stringstream Error;
	Error << "Frame rate " << FrameRate << " not supported";
	throw Soy::AssertException(Error);
}

k4a_color_resolution_t KinectAzure::GetLargestColourResolution(SoyPixelsFormat::Type Format)
{
	switch (Format)
	{
		//	Yuv_844 + Depth doesn't work
	case SoyPixelsFormat::Yuv_844:
		return K4A_COLOR_RESOLUTION_OFF;

		//	nv12 + depth	maxes out at 720P
	case SoyPixelsFormat::Depth16mm:
		return K4A_COLOR_RESOLUTION_720P;

		//	bgra+depth max 1536P
	case SoyPixelsFormat::BGRA:
		return K4A_COLOR_RESOLUTION_1536P;

	case SoyPixelsFormat::Yuv_8_88:
		return K4A_COLOR_RESOLUTION_3072P;
	}

	return K4A_COLOR_RESOLUTION_OFF;
}


k4a_colour_mode_t KinectAzure::GetColourMode(SoyPixelsMeta Format, size_t& FrameRate)
{
	auto SameDim = [](SoyPixelsMeta a, SoyPixelsMeta b)
	{
		if (a.GetWidth() != b.GetWidth())return false;
		if (a.GetHeight() != b.GetHeight())return false;
		return true;
	};

	k4a_colour_mode_t ColorMode;

	//	work out the format
	//	default to BGRA
	if (Format.GetFormat() == SoyPixelsFormat::Invalid)
		ColorMode.format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
	else
		ColorMode.format = GetColourFormat(Format.GetFormat());


	auto ColourResolutions =
	{
		//K4A_COLOR_RESOLUTION_OFF,
		K4A_COLOR_RESOLUTION_720P,
		K4A_COLOR_RESOLUTION_1080P,
		K4A_COLOR_RESOLUTION_1440P,
		K4A_COLOR_RESOLUTION_1536P,
		K4A_COLOR_RESOLUTION_2160P,
		K4A_COLOR_RESOLUTION_3072P
	};
	for (auto ColourResolution : ColourResolutions)
	{
		//	require format to be right?
		size_t DummyFrameRate = FrameRate;
		auto PixelMeta = GetPixelMeta(ColourResolution, DummyFrameRate, Format.GetFormat());
		if (SameDim(Format, PixelMeta))
		{
			ColorMode.resolution = ColourResolution;
			FrameRate = DummyFrameRate;
			return ColorMode;
		}
	}

	//	allow a way to get the no-depth mode
	if (Format == SoyPixelsMeta())
		return k4a_colour_mode_t();

	//	if no dimensions, but we have a pixelformat, then default the resolution
	if (SameDim(Format, SoyPixelsMeta(0, 0, SoyPixelsFormat::Invalid)))
	{
		//	get biggest res for a format, need to make a better system for this
		auto Resolution = GetLargestColourResolution(Format.GetFormat());
		if (Resolution != K4A_COLOR_RESOLUTION_OFF)
		{
			ColorMode.resolution = Resolution;
			if (FrameRate == 0)
				FrameRate = GetMaxFrameRate(ColorMode.resolution);
			return ColorMode;
		}
	}

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
		return SoyPixelsFormat::Depth16mm;

	case K4A_IMAGE_FORMAT_COLOR_BGRA32:	return SoyPixelsFormat::BGRA;
	case K4A_IMAGE_FORMAT_COLOR_NV12:	return SoyPixelsFormat::Nv12;
	case K4A_IMAGE_FORMAT_COLOR_YUY2:	return SoyPixelsFormat::YUY2;
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


k4a_image_format_t KinectAzure::GetFormat(SoyPixelsFormat::Type Format)
{
	switch (Format)
	{
	case SoyPixelsFormat::Depth16mm:
		return K4A_IMAGE_FORMAT_DEPTH16;

	case SoyPixelsFormat::RGBA:
	case SoyPixelsFormat::BGRA:
		return K4A_IMAGE_FORMAT_COLOR_BGRA32;

	case SoyPixelsFormat::Nv12:
		return K4A_IMAGE_FORMAT_COLOR_NV12;

	case SoyPixelsFormat::YUY2:
		return K4A_IMAGE_FORMAT_COLOR_YUY2;

		//	return YUV for luma?
	case SoyPixelsFormat::Greyscale:
		return K4A_IMAGE_FORMAT_CUSTOM8;
	}

	std::stringstream Error;
	Error << "Unhandled pixel format " << magic_enum::enum_name(Format);
	throw Soy::AssertException(Error);
}


k4a_image_format_t KinectAzure::GetColourFormat(SoyPixelsFormat::Type Format)
{
	switch (Format)
	{
	case SoyPixelsFormat::RGBA:
	case SoyPixelsFormat::BGRA:
		return K4A_IMAGE_FORMAT_COLOR_BGRA32;

	case SoyPixelsFormat::Nv12:
		return K4A_IMAGE_FORMAT_COLOR_NV12;

	case SoyPixelsFormat::YUY2:
		return K4A_IMAGE_FORMAT_COLOR_YUY2;

		//	return YUV for luma?
	case SoyPixelsFormat::Greyscale:
		return K4A_IMAGE_FORMAT_CUSTOM8;
	}

	std::stringstream Error;
	Error << "Unhandled pixel format " << magic_enum::enum_name(Format);
	throw Soy::AssertException(Error);
}

class KinectAzure::TPixelReader : public TFrameReader
{
public:
	TPixelReader(size_t DeviceIndex, bool KeepAlive, bool VerboseDebug, std::function<void(std::shared_ptr<TPixelBuffer>&,SoyTime, json11::Json::object&)> OnFrame, k4a_depth_mode_t DepthMode, k4a_colour_mode_t ColourMode,k4a_fps_t FrameRate, k4a_wired_sync_mode_t SyncMode) :
		TFrameReader	(DeviceIndex, KeepAlive, VerboseDebug),
		mOnNewFrame		(OnFrame),
		mDepthMode		( DepthMode ),
		mColourMode		( ColourMode ),
		mFrameRate		( FrameRate ),
		mSyncMode		( SyncMode )
	{
		Start();
	}
	~TPixelReader();

private:
	virtual void				OnFrame(TCaptureFrame& Frame) override;
	virtual k4a_depth_mode_t	GetDepthMode() override { return mDepthMode; }
	virtual k4a_colour_mode_t	GetColourMode() override { return mColourMode; }
	virtual k4a_fps_t			GetFrameRate() override { return mFrameRate; }
	virtual k4a_wired_sync_mode_t	GetSyncMode() override { return mSyncMode; }

	std::function<void(std::shared_ptr<TPixelBuffer>&,SoyTime,json11::Json::object&)>	mOnNewFrame;
	k4a_depth_mode_t		mDepthMode = K4A_DEPTH_MODE_OFF;
	k4a_colour_mode_t		mColourMode;
	k4a_fps_t				mFrameRate = K4A_FRAMES_PER_SECOND_30;
	k4a_wired_sync_mode_t	mSyncMode = K4A_WIRED_SYNC_MODE_STANDALONE;
};


void KinectAzure::LoadDll()
{
	static bool DllLoaded = false;

	//	load the lazy-load libraries
	if (DllLoaded)
		return;

	//	linux requres DISPLAY env var to be 0 for headless mode
#if defined(TARGET_LINUX)
	Platform::SetEnvVar("DISPLAY",":0");
#endif

#if defined(K4A_DLL)
	//	we should just try, because if the parent process has loaded it, this
	//	will just load
	Soy::TRuntimeLibrary DllK4a(K4A_DLL);
	//Soy::TRuntimeLibrary DllK4abt("k4abt.dll");
#endif
	
	DllLoaded = true;
}

void KinectAzure::InitDebugHandler(bool Verbose)
{
	LoadDll();

	auto OnDebug = [](void* Context, k4a_log_level_t Level, const char* Filename, const int Line, const char* Message)
	{
		std::Debug << "Kinect[" << magic_enum::enum_name(Level) << "] " << Filename << "(" << Line << "): " << Message << std::endl;
	};

	void* Context = nullptr;
	auto DebugLevel = Verbose ? K4A_LOG_LEVEL_TRACE : K4A_LOG_LEVEL_WARNING;
	auto Result = k4a_set_debug_message_handler(OnDebug, Context, DebugLevel);
	IsOkay(Result, "k4a_set_debug_message_handler");

	//	set these env vars for extra logging
	//	https://github.com/microsoft/Azure-Kinect-Sensor-SDK/issues/709
	//Platform::SetEnvVar("K4ABT_ENABLE_LOG_TO_A_FILE", "k4abt.log");
	//Platform::SetEnvVar("K4ABT_LOG_LEVEL", "i");	//	or W
}

void KinectAzure::EnumDeviceNameAndFormats(std::function<void(const std::string&,ArrayBridge<std::string>&&)> Enum)
{
	//	gr: if this throws, the dll is probbaly missing
	try
	{
		LoadDll();
	}
	catch (std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << "; " << e.what() << std::endl;
		return;
	}

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

	//	DO NOT WORK - all 844's dont work
	//Yuv_8_88_Ntsc_Depth16 ^ 1920x1080
	//Yuv_8_88_Ntsc_Depth16 ^ 2560x1440
	//Yuv_8_88_Ntsc_Depth16 ^ 2048x1536

	//	this is in prefered order
	PushDepthFormat(K4A_DEPTH_MODE_NFOV_UNBINNED);
	PushDepthFormat(K4A_DEPTH_MODE_NFOV_2X2BINNED);
	PushDepthFormat(K4A_DEPTH_MODE_WFOV_UNBINNED);
	PushDepthFormat(K4A_DEPTH_MODE_WFOV_2X2BINNED);

	//k4a_image_format_t ColourPixelFormats[] = { K4A_IMAGE_FORMAT_COLOR_NV12, K4A_IMAGE_FORMAT_COLOR_YUY2, K4A_IMAGE_FORMAT_COLOR_BGRA32 };
	k4a_image_format_t ColourPixelFormats[] = { K4A_IMAGE_FORMAT_COLOR_NV12, K4A_IMAGE_FORMAT_COLOR_BGRA32 };
	for (auto ImageFormat : ColourPixelFormats)
	{
		auto PixelFormat = GetFormat(ImageFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_2160P, PixelFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_3072P, PixelFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_1536P, PixelFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_1440P, PixelFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_1080P, PixelFormat);
		PushColourFormat(K4A_COLOR_RESOLUTION_720P, PixelFormat);
	}

	for (auto i = 0; i < DeviceCount; i++)
	{
		try
		{
			auto SyncMode = K4A_WIRED_SYNC_MODE_STANDALONE;
			KinectAzure::TDevice Device(i, K4A_DEPTH_MODE_OFF, k4a_colour_mode_t(), K4A_FRAMES_PER_SECOND_5,SyncMode);
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
	auto EnumName = [&](const std::string& DeviceSerial,ArrayBridge<std::string>&& Formats)
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

KinectAzure::TDevice::TDevice(size_t DeviceIndex, k4a_depth_mode_t DepthMode, k4a_colour_mode_t ColourMode, k4a_fps_t FrameRate, k4a_wired_sync_mode_t SyncMode)
{
	//	this fails the second time if we didn't close properly (app still has exclusive access)
	//	so make sure we shutdown if we fail
	auto Error = k4a_device_open(DeviceIndex, &mDevice);
	IsOkay(Error, "k4a_device_open");

	//	don't start cameras if nothing selected (throws error)
	//	use this config for simple enum
	if (DepthMode == K4A_DEPTH_MODE_OFF && ColourMode.resolution == K4A_COLOR_RESOLUTION_OFF)
		return;

	try
	{
		k4a_device_configuration_t DeviceConfig = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		DeviceConfig.depth_mode = DepthMode;
		DeviceConfig.color_resolution = ColourMode.resolution;
		DeviceConfig.wired_sync_mode = SyncMode;
		//	start_cameras fails if this is set to say, custom, so don't change from default(mjpg)
		if (ColourMode.resolution != K4A_COLOR_RESOLUTION_OFF )
			DeviceConfig.color_format = ColourMode.format;
		DeviceConfig.camera_fps = FrameRate;

		//	gr: if this fails when using depth on linux with error 204, may need to set headless display mode before running
		//export DISPLAY:=0

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


k4a_transformation_t KinectAzure::TDevice::GetDepthToImageTransform()
{
	if (mTransformation)
		return mTransformation;

	mTransformation = k4a_transformation_create(&mCalibration);
	return mTransformation;
}

KinectAzure::TCaptureParams::TCaptureParams(json11::Json& Options)
{

	Read( Options, POPCAMERADEVICE_KEY_FRAMERATE, mFrameRate );
	Read( Options, POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
	Read( Options, POPCAMERADEVICE_KEY_DEPTHFORMAT, mDepthFormat );
	Read( Options, POPCAMERADEVICE_KEY_DEBUG, mVerboseDebug );
	Read( Options, POPCAMERADEVICE_KEY_SYNCPRIMARY, mSyncPrimary);
	Read( Options, POPCAMERADEVICE_KEY_SYNCSECONDARY, mSyncSecondary);
	
	//	Allow our default of Depth, but if provided "depth" as format, let this happen
	if (mDepthFormat == mColourFormat)
		mDepthFormat = SoyPixelsFormat::Invalid;

	//	allow Format to be depth
	if (mDepthFormat == SoyPixelsFormat::Invalid && SoyPixelsFormat::IsDepthFormat(mColourFormat))
	{
		mDepthFormat = mColourFormat;
		mColourFormat = SoyPixelsFormat::Invalid;
	}
}

KinectAzure::TCameraDevice::TCameraDevice(const std::string& Serial,json11::Json& Options)
{
	TCaptureParams Params(Options);
	if (!Soy::StringBeginsWith(Serial, KinectAzure::SerialPrefix, true))
		throw PopCameraDevice::TInvalidNameException();

	k4a_wired_sync_mode_t SyncMode = K4A_WIRED_SYNC_MODE_STANDALONE;
	if (Params.mSyncPrimary && Params.mSyncSecondary)
		throw Soy::AssertException("Cannot be configured as sync master and sync sub");
	if (Params.mSyncPrimary)
		SyncMode = K4A_WIRED_SYNC_MODE_MASTER;
	if (Params.mSyncSecondary)
		SyncMode = K4A_WIRED_SYNC_MODE_SUBORDINATE;

	LoadDll();
	InitDebugHandler( Params.mVerboseDebug );

	auto DeviceIndex = GetDeviceIndex(Serial);
	//	todo: remove keep alive when PopEngine/CAPI is fixed
	auto KeepAlive = true;	//	keep reopening the device in the reader


	//	this all needs some giant case list mixing user-desired w/h/fps/colour/depth vs default
	//	and fill in the gaps
	size_t FrameRate = Params.mFrameRate;
	auto DepthMode = GetDepthMode( SoyPixelsMeta(0,0,Params.mDepthFormat), FrameRate );//K4A_DEPTH_MODE_NFOV_UNBINNED;
	//	colour defaults off
	k4a_colour_mode_t ColourMode;
	if (Params.mColourFormat != SoyPixelsFormat::Invalid)
	{
		//	if there's depth too, let that dictate max colour res
		if (Params.mDepthFormat != SoyPixelsFormat::Invalid )
			ColourMode.resolution = GetLargestColourResolution(Params.mDepthFormat);
		else
			ColourMode.resolution = GetLargestColourResolution(Params.mColourFormat);
		ColourMode.format = GetFormat(Params.mColourFormat);
	}
	auto Fps = GetFrameRate( FrameRate );

	auto OnNewFrame = [this](std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyTime FrameTime,json11::Json::object& FrameMeta)
	{
		PushFrame(FramePixelBuffer, FrameTime, FrameMeta);
	};

	mReader.reset( new TPixelReader(DeviceIndex, KeepAlive, Params.mVerboseDebug, OnNewFrame, DepthMode, ColourMode, Fps, SyncMode) );
}

KinectAzure::TCameraDevice::~TCameraDevice()
{
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

static int FrameReaderThreadCount = 0;

KinectAzure::TFrameReader::TFrameReader(size_t DeviceIndex,bool KeepAlive,bool VerboseDebug) :
	SoyThread		( std::string("TFrameReader ") + std::to_string(FrameReaderThreadCount++) ),
	mDeviceIndex	( DeviceIndex ),
	mKeepAlive		( KeepAlive ),
	mVerboseDebug	( VerboseDebug )
{
	//	gr: problem here for non-keep alive as we may have params that have not yet been set, but stop it from booting
		//	try to open once to try and throw at construction (needed for non-keepalive anyway)
	if ( !mKeepAlive )
		Open();

	//	gr: calling this here, the thread runs faster than the constructor can finish, so virtuals aren't setup
	//Start();
}



KinectAzure::TFrameReader::~TFrameReader()
{
	std::Debug << "~TFrameReader" << std::endl;
	try
	{
		this->Stop(true);
	}
	catch (std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << e.what() << std::endl;
	}
}

void KinectAzure::TFrameReader::Open()
{
	if (mDevice)
		return;
	
	auto DepthMode = GetDepthMode();
	auto ColourMode = GetColourMode();
	auto FrameRate = GetFrameRate();
	auto SyncMode = GetSyncMode();
	mDevice.reset(new TDevice(mDeviceIndex, DepthMode, ColourMode, FrameRate,SyncMode ));
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
		std::Debug << "Exception in " << __PRETTY_FUNCTION__ << " loop: " << e.what() << " (Pausing for "<< SleepMs << "ms)" << std::endl;

		//	gr: for now at least send this in KeepAlive mode, 
		//		as the caller might send different params in the API (eg. GPU index)
		//		which may be the reason we're failing to iterate, this lets params change
		OnError(e.what());

		//	pause
		std::this_thread::sleep_for(std::chrono::milliseconds(SleepMs));

		if (mKeepAlive)
		{
			//	quick fix for shutdown syncing (funcs in here refer to mDevice)
			std::lock_guard<std::mutex> DeviceLock(mDeviceLock);
			if ( mVerboseDebug )
				std::Debug << "Reader clear mdevice" << std::endl;
			//	close device and let next iteration reopen
			mDevice.reset();
		}
		else
		{
			throw;
		}
	}
	return true;
}

bool KinectAzure::TFrameReader::IsExpectingDepthAndColour()
{
	auto Colour = GetColourMode();
	auto Depth = GetDepthMode();
	auto HasDepth = (Depth != K4A_DEPTH_MODE_OFF);
	auto HasColour = (Colour.resolution != K4A_COLOR_RESOLUTION_OFF);
	return HasDepth && HasColour;
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
	//	shutdown could null mDevice, and lock so we wanna grab any device stuff now, and let SDK
	//	fail functions.
	//	we could just copy mDevice, then this frame releases it, but better to ensure shutdown() releases it
	//	we can't keep the lock all iteration, as OnFrame() could callback to the PopCameraDevice instances lock,
	//	which whilst shutting down (when we need this lock) could be locked, so we get a deadlock
	k4a_device_t Device = nullptr;
	k4a_calibration_t Calibration = { 0 };
	k4a_transformation_t DepthToImageTransform = { 0 };
	{
		//	quick fix for shutdown syncing (funcs in here refer to mDevice)
		std::lock_guard<std::mutex> DeviceLock(mDeviceLock);
		if (!mDevice)
			throw Soy::AssertException("TFrameReader::Iteration null device");

		//auto& mTracker = mDevice->mTracker;
		Device = mDevice->mDevice;
		Calibration = GetCalibration();
		DepthToImageTransform = GetDepthToImageTransform();
	}

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
	if ( mVerboseDebug )
		std::Debug << "Got capture" << std::endl;
	/*	gr: if we disconnect the device, we just get timeout, so we can't tell the difference
	//		if timeout is high then we can assume its dead. so throw and let the system try and reconnect
	if (WaitError == K4A_WAIT_RESULT_TIMEOUT)
	{
		std::Debug << "Kinect get-capture timeout (" << TimeoutMs << "ms)" << std::endl;
		return;
	}
	*/
	auto FreeCapture = [&]()
	{
		if (mVerboseDebug)
			std::Debug << "Free capture" << std::endl;
		if (Capture)
			k4a_capture_release(Capture);
	};
	/*
	static int Counter = 0;
	if (Counter++ > 10)
	{
		FreeCapture();
		throw Soy::AssertException("Fail");
	}*/
	if (WaitError != K4A_WAIT_RESULT_SUCCEEDED)
	{
		std::Debug << __PRETTY_FUNCTION__ << "Non-success message (" << WaitError << "), explicitly freeing capture(" << (Capture?"non-null":"null") <<" (source of previous crash?)" << std::endl;
		FreeCapture();
	}
	IsOkay(WaitError, "k4a_device_get_capture");

	//	kinect provides a device timestamp (relative only to itself)
	//	and a system timestamp, but as our purposes are measurements inside our own system, lets stick to our own timestamps
	//	k4a_image_get_device_timestamp_usec
	//	k4a_image_get_system_timestamp_nsec
	SoyTime FrameCaptureTime(true);

	

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
		if ( mVerboseDebug )
			std::Debug << "OnFrame start" << std::endl;
		TCaptureFrame CaptureFrame;
		CaptureFrame.mCalibration = Calibration;
		CaptureFrame.mCapture = Capture;
		CaptureFrame.mImu = ImuSample;
		CaptureFrame.mDepthToImageTransform = DepthToImageTransform;
		CaptureFrame.mTime = FrameCaptureTime;

		//	get sync-connection meta
		{
			auto Result = k4a_device_get_sync_jack(Device, &CaptureFrame.mSyncInCable, &CaptureFrame.mSyncOutCable);
			try
			{
				IsOkay(Result, "k4a_device_get_sync_jack");
			}
			catch (std::exception&e)
			{
				std::Debug << e.what() << std::endl;
			}
		}

		//	we can get a deadlock if the thread is waiting to shutdown, and the OnFrame callback results in
		//	a call to the instances lock that called the shutdown, so skip if thread is stop[ping]
		if (this->IsThreadRunning())
		{
			OnFrame(CaptureFrame);
		}
		else
		{
			std::Debug << "Skipping Capture frame callback as thread is stopping" << std::endl;
		}

		if ( mVerboseDebug )
			std::Debug << "OnFrame finished" << std::endl;

		//	cleanup
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


KinectAzure::TPixelReader::~TPixelReader()
{
	if ( mVerboseDebug )
		std::Debug << "~TPixelReader" << std::endl;
	//	we need to stop the thread here (before FrameReader destructor)
	//	because as this class is destroyed, so is the virtual OnFrame func
	//	which when called by the thread abort()s because we've destructed this class
	try
	{
		this->Stop(false);
		{
			//	quick fix for shutdown syncing (funcs in here refer to mDevice)
			std::lock_guard<std::mutex> DeviceLock(mDeviceLock);
			mDevice.reset();
		}
		this->Stop(true);
	}
	catch (std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << e.what() << std::endl;
	}
}

std::vector<float> GetTransform4x4(k4a_calibration_extrinsics_t Extrinsics)
{
	auto& r = Extrinsics.rotation;
	auto& t = Extrinsics.translation;
	std::vector<float> Matrix =
	{
		r[0], r[1], r[2],	0,
		r[3], r[4], r[5],	0,
		r[6], r[7], r[8],	0,
		t[0], t[1], t[2],	1,
	};
	return Matrix;
}

void GetMeta(k4a_calibration_camera_t Calibration, json11::Json::object& Json)
{
	//	might be a bit useless
	Json["MaxFov"] = Calibration.metric_radius;

	//	turn lens rotation & translation (from middle of camera I think)
	auto Transform = GetTransform4x4(Calibration.extrinsics);
	std::vector<float> mtx;
	Json["LocalToLensTransform"] = mtx;

	if (Calibration.intrinsics.type != K4A_CALIBRATION_LENS_DISTORTION_MODEL_BROWN_CONRADY)
	{
		//	just print all the values
		Json["IntrinsicsMode"] = magic_enum::enum_name(Calibration.intrinsics.type);
		std::vector<float> Intrinsics;
		for (auto i = 0; i < Calibration.intrinsics.parameter_count; i++)
			Intrinsics.push_back(Calibration.intrinsics.parameters.v[i]);
		Json["Intrinsics"] = Intrinsics;
	}
	else
	{
		//	gr: todo: turn this into a projection matrix, but get it right :)
		Json["cx"] = Calibration.intrinsics.parameters.param.cx;
		Json["cy"] = Calibration.intrinsics.parameters.param.cy;
		Json["fx"] = Calibration.intrinsics.parameters.param.fx;
		Json["fy"] = Calibration.intrinsics.parameters.param.fy;
		Json["k1"] = Calibration.intrinsics.parameters.param.k1;
		Json["k2"] = Calibration.intrinsics.parameters.param.k2;
		Json["k3"] = Calibration.intrinsics.parameters.param.k3;
		Json["k4"] = Calibration.intrinsics.parameters.param.k4;
		Json["k5"] = Calibration.intrinsics.parameters.param.k5;
		Json["k6"] = Calibration.intrinsics.parameters.param.k6;
		Json["codx"] = Calibration.intrinsics.parameters.param.codx;
		Json["cody"] = Calibration.intrinsics.parameters.param.cody;
		Json["p1"] = Calibration.intrinsics.parameters.param.p1;
		Json["p2"] = Calibration.intrinsics.parameters.param.p2;
		Json["metric_radius"] = Calibration.intrinsics.parameters.param.metric_radius;
	}
}


void KinectAzure::TPixelReader::OnFrame(TCaptureFrame& CaptureFrame)
{
	auto& Frame = CaptureFrame.mCapture;
	auto& Calibration = CaptureFrame.mCalibration;
	auto& Imu = CaptureFrame.mImu;
	auto& CaptureTime = CaptureFrame.mTime;

	auto DepthImage = k4a_capture_get_depth_image(Frame);
	auto ColourImage = k4a_capture_get_color_image(Frame);
	bool DepthIsAlignedToColour = false;

	//	gr: we had a problem where we got depth with no colour, so was full res
	//		then subsequent frames were resized, or went back and forth
	auto ExpectingDepthAndColour = IsExpectingDepthAndColour();
	if (ExpectingDepthAndColour)
	{
		if (DepthImage && ColourImage)
		{

		}
		else
		{
			//throw Soy::AssertException("Skipping frame as we require depth and colour");
			std::Debug << "Skipping frame (" << CaptureTime << ") as we require depth" << (DepthImage?"":"(null)") << " and colour" << (ColourImage?"":"(null)") << std::endl;
			return;
		}
	}


	//	extract general meta
	json11::Json::object FrameMeta;
	FrameMeta["Temperature"] = Imu.temperature;
	FrameMeta["Accelerometer"] = json11::Json::array{ Imu.acc_sample.xyz.x, Imu.acc_sample.xyz.y, Imu.acc_sample.xyz.z };
	FrameMeta["Gyro"] = json11::Json::array{ Imu.gyro_sample.xyz.x, Imu.gyro_sample.xyz.y, Imu.gyro_sample.xyz.z };
	FrameMeta["SyncInCable"] = CaptureFrame.mSyncInCable;
	FrameMeta["SyncOutCable"] = CaptureFrame.mSyncOutCable;

	auto PushImage = [&](k4a_image_t Image, k4a_calibration_camera_t Calibration)
	{
		auto Pixels = GetPixels(Image);
		float3x3 Transform;
		
		//	add calibration meta here
		auto Meta = FrameMeta;
		GetMeta(Calibration, Meta);
		
		std::shared_ptr<TPixelBuffer> PixelBuffer(new TDumbPixelBuffer(Pixels,Transform));
		this->mOnNewFrame(PixelBuffer, CaptureTime, Meta);
	};

	//	if colour & depth, realign so depth matches colour
	if (ColourImage && DepthImage)
	{
		auto ColourPixels = GetPixels(ColourImage);
		auto Transform = CaptureFrame.mDepthToImageTransform;
		k4a_image_t TransformedDepthImage = nullptr;
		auto Width = ColourPixels.GetWidth();
		auto Height = ColourPixels.GetHeight();
		//auto Stride = ColourPixels.GetMeta().GetRowDataSize();
		auto Stride = SoyPixelsMeta(Width, Height, SoyPixelsFormat::Depth16mm).GetRowDataSize();
		auto Result = k4a_image_create(K4A_IMAGE_FORMAT_DEPTH16, Width, Height, Stride, &TransformedDepthImage);
		IsOkay(Result, "k4a_image_create(transformed depth");

		Result = k4a_transformation_depth_image_to_color_camera(Transform, DepthImage, TransformedDepthImage);
		IsOkay(Result, "k4a_transformation_depth_image_to_color_camera");

		//	swap depth for new depth
		k4a_image_release(DepthImage);
		DepthImage = TransformedDepthImage;
		DepthIsAlignedToColour = true;
	}

	auto Cleanup = [&]()
	{
		if (DepthImage)
		{
			k4a_image_release(DepthImage);
			DepthImage = nullptr;
		}

		if (ColourImage)
		{
			k4a_image_release(ColourImage);
			ColourImage = nullptr;
		}
	};
	
	try
	{
		if (!DepthImage && !ColourImage)
		{
			//	gr: throw?
			std::Debug << __PRETTY_FUNCTION__ << " Frame had no depth or colour" << std::endl;
		}

		if (DepthImage)
			PushImage(DepthImage, Calibration.depth_camera_calibration);
		if ( ColourImage )
			PushImage(ColourImage, Calibration.color_camera_calibration);
		
		Cleanup();
	}
	catch (...)
	{
		Cleanup();
		throw;
	}	
}



