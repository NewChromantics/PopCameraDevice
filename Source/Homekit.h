#pragma once

#include "TCameraDevice.h"

//	can't really forward declare json11::Json::object
#include "Json11/json11.hpp"

//	https://developer.apple.com/documentation/homekit/interacting_with_a_home_automation_network
//	get a home
//	get an accessory
//	get an accessory with a camera
//	get camera stream
//	renders only to a view? do we have to capture that view?
//HMCameraStreamControl

namespace Homekit
{
	class TCamera;

	void	EnumDevices(std::function<void(const std::string&)> EnumName);
}

/*
class Arkit::TFrameDevice : public PopCameraDevice::TDevice
{
public:
	TFrameDevice(json11::Json& Options);
	
	void			PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName);
	void			PushFrame(AVDepthData* DepthData,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName);
	void			PushFrame(ARDepthData* DepthData,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName);
	void			PushFrame(ARFrame* Frame,ArFrameSource::Type Source);
	
	void			PushGeometryFrame(const TAnchorGeometry& Geometry);
	
	SoyTime			mPreviousDepthTime;
	SoyTime			mPreviousFrameTime;
	TCaptureParams	mParams;
};

*/

