#pragma once

#include "TCameraDevice.h"
#include "Avf.h"
#include "SoyMedia.h"
#include "SoyEnum.h"



namespace Avf
{
	class TCaptureExtractor;
	class TCamera;
	class TArFrameProxy;


	void	EnumArFrameDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName);
}
class AvfVideoCapture;

namespace ArFrameSource
{
	enum Type
	{
		Invalid,
		capturedImage,
		capturedDepthData,
		sceneDepth,
		
		FrontColour,	//	capturedImage
		FrontDepth,		//	capturedDepthData
		RearDepth,		//	sceneDepth
		
		Depth,			//	default
	};
	const static auto Default	= RearDepth;
	
	DECLARE_SOYENUM(ArFrameSource);
}


class Avf::TCamera : public PopCameraDevice::TDevice
{
public:
	TCamera(const std::string& DeviceName,const std::string& Format);

	void			PushLatestFrame(size_t StreamIndex);
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

	std::shared_ptr<AvfVideoCapture>	mExtractor;
};



//	this special camera lets the user pass in an ARFrame and read back the depth which gets added to the queue
class Avf::TArFrameProxy : public PopCameraDevice::TDevice
{
public:
	static inline const char* DeviceName = "AVFrameProxy";
public:
	TArFrameProxy(const std::string& Format);
	
	virtual void	ReadNativeHandle(void* ArFrameHandle) override;
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

	ArFrameSource::Type	mSource = ArFrameSource::Default;
};

