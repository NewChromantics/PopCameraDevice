#pragma once

#include "TCameraDevice.h"
#include "Avf.h"
#include "SoyMedia.h"



namespace Avf
{
	class TCaptureExtractor;
	class TCamera;
}
class AvfVideoCapture;


class Avf::TCamera : public  PopCameraDevice::TDevice
{
public:
	TCamera(const std::string& DeviceName,const std::string& Format);

	void			PushLatestFrame(size_t StreamIndex);
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

	std::shared_ptr<AvfVideoCapture>	mExtractor;
};

/*
class Avf::TCaptureExtractor : public MfExtractor
{
public:
	TCaptureExtractor(const TMediaExtractorParams& Params);
	~TCaptureExtractor();

protected:
	virtual void		AllocSourceReader(const std::string& Filename) override;
	virtual bool		CanSeek() override				{	return false;	}
	virtual void		FilterStreams(ArrayBridge<TStreamMeta>& Streams) override;
	virtual void		CorrectIncomingTimecode(TMediaPacket& Timecode) override;

public:
};

*/
