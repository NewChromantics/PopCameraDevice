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
	class TPlatformCamera;	//	objc object in .mm source

	void	EnumDevices(std::function<void(const std::string&)> EnumName);
}


class Homekit::TCamera : public PopCameraDevice::TDevice
{
public:
	TCamera(const std::string& Name,json11::Json& Options);

	std::shared_ptr<TPlatformCamera>	mPlatfomCamera;
};
