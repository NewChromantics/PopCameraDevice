#include "KinectAzure.h"
#include <k4abt.h>
#include <k4a/k4a.h>
#include "SoyDebug.h"
#include "HeapArray.hpp"
#include "MagicEnum/include/magic_enum.hpp"
#include "SoyRuntimeLibrary.h"
#include "SoyThread.h"


#if !defined(ENABLE_KINECTAZURE)
#error Expected ENABLE_KINECTAZURE to be defined
#endif


namespace KinectAzure
{
	class TFrameReader;
	class TDepthFrameReader;

	void		IsOkay(k4a_result_t Error, const char* Context);
	void		IsOkay(k4a_wait_result_t Error, const char* Context);
	void		IsOkay(k4a_buffer_result_t Error, const char* Context);
	void		InitDebugHandler();
	void		LoadDll();
	size_t		GetDeviceIndex(const std::string& Serial);

	SoyPixelsRemote			GetPixels(k4a_image_t Image);
	SoyPixelsFormat::Type	GetFormat(k4a_image_format_t Format);
}



//	a device just holds low-level handle
class KinectAzure::TDevice
{
public:
	TDevice(size_t DeviceIndex, k4a_depth_mode_t DepthMode, k4a_color_resolution_t ColourMode);
	~TDevice();

	std::string				GetSerial();

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

protected:
	virtual void		PushFrame(const k4a_capture_t Frame, k4a_imu_sample_t Imu, SoyTime CaptureTime) = 0;
	virtual void		OnImuMoved() {}

	//	as we keep-alive, we need to know these modes
	virtual k4a_depth_mode_t		GetDepthMode() = 0;
	virtual k4a_color_resolution_t	GetColourMode() = 0;

private:
	void				Iteration(int32_t TimeoutMs);
	virtual void		Thread() override;
	bool				HasImuMoved(k4a_imu_sample_t Imu);	//	check if the device has been moved

	void				Open();
	
public:
	Soy::TSemaphore		mOnNewFrameSemaphore;

protected:
	std::mutex			mLastFrameLock;

private:
	std::shared_ptr<KinectAzure::TDevice>	mDevice;
	bool				mKeepAlive = false;
	size_t				mDeviceIndex = 0;
	vec3f				mLastAccell;			//	store accelleromter to detect movement & re-find floor plane
};


class KinectAzure::TDepthReader : public TFrameReader
{
public:
	TDepthReader(size_t DeviceIndex, bool KeepAlive) :
		TFrameReader(DeviceIndex, KeepAlive)
	{
	}

	std::shared_ptr<SoyPixelsImpl>	PopFrame(bool Blocking);

private:
	virtual void		PushFrame(const k4a_capture_t Frame, k4a_imu_sample_t Imu, SoyTime CaptureTime) override;
	void				PushFrame(const SoyPixelsImpl& Frame, SoyTime CaptureTime);
	virtual k4a_depth_mode_t		GetDepthMode() override { return K4A_DEPTH_MODE_NFOV_UNBINNED; }
	virtual k4a_color_resolution_t	GetColourMode() override { return K4A_COLOR_RESOLUTION_OFF; }

public:
	std::shared_ptr<SoyPixelsImpl>	mLastFrame;
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

void KinectAzure::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	auto DeviceCount = k4a_device_get_installed_count();
	std::Debug << "KinectDevice count: " << DeviceCount << std::endl;

	for (auto i = 0; i < DeviceCount; i++)
	{
		try
		{
			KinectAzure::TDevice Device(i, K4A_DEPTH_MODE_OFF, K4A_COLOR_RESOLUTION_OFF);
			auto Serial = Device.GetSerial();
			Enum(Serial);
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
	auto EnumName = [&](const std::string& DeviceSerial)
	{
		if (DeviceSerial == Serial)
			SerialIndex = RunningIndex;

		RunningIndex++;
	};
	EnumDeviceNames(EnumName);

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
	k4a_device_stop_imu(mDevice);
	k4a_device_stop_cameras(mDevice);
	//k4abt_tracker_shutdown(mTracker);
	//k4abt_tracker_destroy(mTracker);
	k4a_device_close(mDevice);
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
	return Serial;
}

KinectAzure::TCameraDevice::TCameraDevice(const std::string& Serial)
{
	LoadDll();
	InitDebugHandler();
	
	auto DeviceIndex = GetDeviceIndex(Serial);
	//	todo: remove keep alive when PopEngine/CAPI is fixed
	auto KeepAlive = true;	//	keep reopening the device in the reader
	mReader.reset( new TDepthReader(DeviceIndex, KeepAlive) );
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


void KinectAzure::TFrameReader::Open()
{
	//	already ready
	if (mDevice)
		return;

	auto DepthMode = GetDepthMode();
	auto ColourMode = GetColourMode();
	mDevice.reset(new TDevice(mDeviceIndex, DepthMode, ColourMode));
}

void KinectAzure::TFrameReader::Thread()
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
		this->mOnNewFrameSemaphore.OnFailed(e.what());

		//	pause
		std::this_thread::sleep_for(std::chrono::milliseconds(SleepMs));

		if (mKeepAlive)
		{
			//	close device and let next iteration reopen
			mDevice.reset();
		}
		else
		{
			throw;
		}
	}
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
			throw Soy::AssertException("IMU had no samples");

		if (HasImuMoved(ImuSample))
		{
			OnImuMoved();
			/*
			std::Debug << "IMU moved, calculating new floor..." << std::endl;
			FindFloor(Capture,ImuSample);
			*/
		}
	
		//	extract skeletons
		PushFrame(Capture, ImuSample, FrameCaptureTime);

		//	cleanup
		//k4abt_frame_release(Frame);
		FreeCapture();
	}
	catch (...)
	{
		FreeCapture();
		throw;
	}
}


void KinectAzure::TDepthReader::PushFrame(const SoyPixelsImpl& Frame,SoyTime CaptureTime)
{
	{
		std::lock_guard<std::mutex> Lock(mLastFrameLock);
		if (!mLastFrame)
			mLastFrame.reset(new SoyPixels);
		mLastFrame->Copy(Frame);
	}

	//	notify new frame
	mOnNewFrameSemaphore.OnCompleted();
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

void KinectAzure::TDepthReader::PushFrame(const k4a_capture_t Frame,k4a_imu_sample_t Imu, SoyTime CaptureTime)
{
	auto DepthImage = k4a_capture_get_depth_image(Frame);
	auto DepthPixels = GetPixels(DepthImage);
	PushFrame( DepthPixels, CaptureTime );
}


std::shared_ptr<SoyPixelsImpl> KinectAzure::TDepthReader::PopFrame(bool Blocking)
{
	//	if we're blocking, wait for the reader to say there's a frame waiting
	if (Blocking)
	{
		//mOnNewFrameSemaphore.WaitAndReset("TKinectAzureSkeletonReader::PopFrame");
		mOnNewFrameSemaphore.WaitAndReset(nullptr);
	}

	std::lock_guard<std::mutex> Lock(mLastFrameLock);
	auto Copy = mLastFrame;
	mLastFrame.reset();
	return Copy;
}

