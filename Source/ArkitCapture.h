#pragma once

#include "TCameraDevice.h"
#include "Avf.h"
#include "SoyMedia.h"
#include "SoyEnum.h"
#include "AvfPixelBuffer.h"

//	can't really forward declare json11::Json::object
#include "Json11/json11.hpp"

namespace Arkit
{
	class TSession;
	class TFrameProxyDevice;	//	device which doesn't own/open any sessions, but you can pass in an ARFrame externally (ie. unity) and pull out frames on demand
	class TFrameDevice;			//	base class for ARFrame
	class TSessionCamera;		//	arkit session which reads a certain source
	class TCaptureParams;
	
	class TAnchorGeometry;
	
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
	
	namespace AnchorChange
	{
		enum Type
		{
			Added,
			Updated,
			Removed
		};
	};
	
	//	return device names for arkit-specific accessors to ARFrame images
	void	EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName);
}


class Arkit::TCaptureParams
{
public:
	TCaptureParams(json11::Json& Options);

	bool	mHdrColour = false;	//	maybe should be a pixel format
	bool	mEnableAutoFocus = true;
	bool	mEnablePlanesHorz = true;
	bool	mEnablePlanesVert = true;
	bool	mEnableFaceTracking = true;
	bool	mEnableLightEstimation = true;
	bool	mEnableBodyDetection = false;	//	body+scene not allowed, so this is off by default
	bool	mEnableSceneDepth = false;		//	should this be depthformat != null?
	bool	mEnablePersonSegmentation = false;
	bool	mResetTracking = false;	//	on start
	bool	mResetAnchors = false;	//	on start
	SoyPixelsFormat::Type	mColourFormat = SoyPixelsFormat::Invalid;
	bool	mOutputFeatures = false;	//	these get huge in quantity, so explicitly enable
	bool	mOutputAnchors = true;		//	these can also get quite large, but on by default (use to get planes from localtoworld transforms)
	bool	mOutputAnchorGeometry = false;		//	these get large, use image streams! left in for legacy
	bool	mOutputSceneDepthConfidence = false;
	bool	mOutputSceneDepthSmooth = false;
	bool	mVerboseDebug = false;
	//	todo: colour format
	bool	mEnableAnchorGeometry = false;	//	output anchor geometry triangles as a float-image stream (with meta describing contents)
	bool	mEnableWorldGeometry = false;	//	output world (scene reconstruction) geometry as a float image stream
};



class Arkit::TAnchorGeometry
{
public:
	FixedRemoteArray<float>	AllocatePositions(size_t PositionCount,size_t PositionComponentCount);	//	allocates pixels, resizes image etc and returns the underlying float array

public:
	std::string				mUuid;
	std::string				mName;	//	some anchors are named
	std::string				mType;	//	ARAnchor type
	vec3f					mBoundsCenter;
	vec3f					mBoundsSize;
	size_t					mPositionCount = 0;
	SoyTime					mTimestamp;
	std::shared_ptr<SoyPixelsImpl>	mTrianglePositionsPixels;	//	floats stored in a pixel buffer. done early to avoid a copy/alloc later
	//Array<float>			mTrianglePositions;
	float4x4				mLocalToWorld;
};

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




class Arkit::TSessionCamera : public Arkit::TFrameDevice
{
public:
	static inline const char* DeviceName_SceneDepth = "Arkit Rear Depth";
	static inline const char* DeviceName_FrontDepth = "Arkit Front Depth";
public:
	TSessionCamera(const std::string& DeviceName,json11::Json& Options);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	void			OnFrame(ARFrame* Frame);
	void			OnAnchorChange(ARAnchor* Frame,AnchorChange::Type Change);
	
	std::shared_ptr<TSession>	mSession;
	ArFrameSource::Type			mSource = ArFrameSource::Invalid;
};



//	this special camera lets the user pass in an ARFrame and read back the depth which gets added to the queue
class Arkit::TFrameProxyDevice : public Arkit::TFrameDevice
{
public:
	static inline const char* DeviceName = "AVFrameProxy";
public:
	TFrameProxyDevice(json11::Json& Options);
	
	virtual void	ReadNativeHandle(void* ArFrameHandle) override;
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	ArFrameSource::Type	mSource = ArFrameSource::Default;
};

