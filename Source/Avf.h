#pragma once

#include <functional>
#include <string>



namespace Avf
{
	void	EnumCaptureDevices(std::function<void(const std::string&)> AppendName);

	class TDeviceMeta;
}


//	this can probably be generic device meta...
class Avf::TDeviceMeta
{
public:
	//	unique identifier
	//	luckily, OSX makes all names unique with #2 etc on the end
	//	so for now the struct can
	std::string&		mCookie = mName;	//	unique identifier
	std::string			mName;
	std::string			mSerial;
	std::string			mVendor;
	bool				mIsConnected = false;
	bool				mIsSuspended = false;
	bool				mHasAudio = false;
	bool				mHasVideo = false;
};
