#pragma once

#include "TCameraDevice.h"
#include "Avf.h"
#include "SoyMedia.h"
#include "SoyEnum.h"
#include "AvfPixelBuffer.h"


namespace json11
{
	class Json;
}

namespace Arkit
{
	class TSession;
	class TFrameProxyDevice;	//	device which doesn't own/open any sessions, but you can pass in an ARFrame externally (ie. unity) and pull out frames on demand
	class TFrameDevice;			//	base class for ARFrame
	class TSessionCamera;		//	arkit session which reads a certain source

	namespace ArFrameSource
	{
		enum Type
		{
			Invalid,
			capturedImage,
			capturedDepthData,
			sceneDepth,
			sceneDepthConfidence,
			segmentationBuffer,
			
			FrontColour,		//	capturedImage
			FrontDepth,			//	capturedDepthData
			RearDepth,			//	sceneDepth
			RearDepthConfidence,	//	sceneDepth
			Segmentation,		//	segmentationBuffer
			
			Depth,			//	default
		};
		const static auto Default	= RearDepth;
		
		DECLARE_SOYENUM(ArFrameSource);
	}
	
	//	return device names for arkit-specific accessors to ARFrame images
	void	EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName);
}





class Arkit::TFrameDevice : public PopCameraDevice::TDevice
{
public:
	void			PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp,json11::Json& Meta);
	void			PushFrame(AVDepthData* DepthData,SoyTime Timestamp,json11::Json& Meta);
	void			PushFrame(ARDepthData* DepthData,SoyTime Timestamp,json11::Json& Meta);
	void			PushFrame(ARFrame* Frame,ArFrameSource::Type Source);
};




class Arkit::TSessionCamera : public Arkit::TFrameDevice
{
public:
	static inline const char* DeviceName_SceneDepth = "Arkit Rear Depth";
	static inline const char* DeviceName_FrontDepth = "Arkit Front Depth";
public:
	TSessionCamera(const std::string& DeviceName,const std::string& Format);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	void			OnFrame(ARFrame* Frame);
	
	std::shared_ptr<TSession>	mSession;
	ArFrameSource::Type			mSource = ArFrameSource::Invalid;
};



//	this special camera lets the user pass in an ARFrame and read back the depth which gets added to the queue
class Arkit::TFrameProxyDevice : public Arkit::TFrameDevice
{
public:
	static inline const char* DeviceName = "AVFrameProxy";
public:
	TFrameProxyDevice(const std::string& Format);
	
	virtual void	ReadNativeHandle(void* ArFrameHandle) override;
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	ArFrameSource::Type	mSource = ArFrameSource::Default;
};

