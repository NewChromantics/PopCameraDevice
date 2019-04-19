#pragma once

#include "TCameraDevice.h"


class TestDevice : public PopCameraDevice::TDevice
{
public:
	TestDevice()
	{
		GenerateFrame();
	}

	static void											EnumDeviceNames(std::function<void(const std::string&)> Enum);
	static std::shared_ptr<PopCameraDevice::TDevice>	CreateDevice(const std::string& Name);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
	void			GenerateFrame();
};
