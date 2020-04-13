#pragma once

#include "TCameraDevice.h"


namespace PopOutputDevice
{
	class TDevice;
}


class PopOutputDevice::TDevice
{
public:
	virtual void		PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,SoyPixelsMeta PixelMeta,SoyTime FrameTime,const std::string& FrameMeta)=0;
};

