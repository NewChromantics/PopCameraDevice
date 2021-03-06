#pragma once

#include "TCameraDevice.h"
#include "Avf.h"
#include "SoyMedia.h"
#include "SoyEnum.h"
#include "AvfPixelBuffer.h"


namespace Avf
{
	class TCaptureExtractor;
	class TCamera;
	class TFrame;
}
class AvfVideoCapture;
#if defined(__OBJC__)
@class AVDepthData;
#else
class AVDepthData;
#endif

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
	TCamera(const std::string& DeviceName,json11::Json& Options);

	void			OnFrame(Avf::TFrame& Frame);
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

	std::shared_ptr<AvfVideoCapture>	mExtractor;
};


