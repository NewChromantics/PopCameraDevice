#include "ArkitCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"
#import <ARKit/ARKit.h>
#include "Json11/json11.hpp"

//  gr: make this a proper check, quickly disabling for build here
#define ENABLE_IOS14    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 140000)
#define ENABLE_IOS13    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
#define ENABLE_IOS12   (__IPHONE_OS_VERSION_MAX_ALLOWED >= 120000)

#if ENABLE_IOS12
#pragma message("IOS 12")
#endif

#if ENABLE_IOS13
#pragma message("IOS 13")
#endif

#if ENABLE_IOS14
#pragma message("IOS 14")
#endif


@interface ARSessionProxy : NSObject <ARSessionDelegate>
{
	Arkit::TSession*	mParent;
}

- (id)initWithSession:(Arkit::TSession*)parent;
- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame;

@end


class Arkit::TSession
{
public:
	TSession(bool RearCamera);
	~TSession();
	
	void				OnFrame(ARFrame* Frame);
	
	std::function<void(ARFrame*)>	mOnFrame;
	ObjcPtr<ARSessionProxy>			mSessionProxy;
	ObjcPtr<ARSession>				mSession;
};



@implementation ARSessionProxy


- (id)initWithSession:(Arkit::TSession*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame
{
	mParent->OnFrame(frame);
}

@end



void Arkit::EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName)
{
	//	proxy device
	{
		Array<std::string> FormatStrings;
		FormatStrings.PushBack( ArFrameSource::ToString(ArFrameSource::capturedImage) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::capturedDepthData) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::FrontDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::sceneDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::RearDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::Depth) );
		EnumName( TFrameProxyDevice::DeviceName, GetArrayBridge(FormatStrings) );
	}
	
	//	if ( ArkitSupported )
	{
		Array<std::string> FormatStrings;
		EnumName( TSessionCamera::DeviceName_SceneDepth, GetArrayBridge(FormatStrings) );
		EnumName( TSessionCamera::DeviceName_FrontDepth, GetArrayBridge(FormatStrings) );
	}
}


Arkit::TSession::TSession(bool RearCamera)
{
	mSessionProxy.Retain([[ARSessionProxy alloc] initWithSession:(this)]);
	
	mSession.Retain( [[ARSession alloc] init] );
	mSession.mObject.delegate = mSessionProxy.mObject;

	//	rear
	//	ARWorldTrackingConfiguration
	//	ARBodyTrackingConfiguration
	//	AROrientationTrackingConfiguration
	//	ARImageTrackingConfiguration
	//	ARObjectScanningConfiguration
	
	//	"jsut position"
	//ARPositionalTrackingConfiguration
	
	//	gps
	//ARGeoTrackingConfiguration
	
	//	front camera
	//	ARFaceTrackingConfiguration
	
	ARConfiguration* Configuration;
	if ( RearCamera )
		Configuration = [ARWorldTrackingConfiguration alloc];
	else
		Configuration = [ARFaceTrackingConfiguration alloc];
	ARSessionRunOptions Options = 0;
	//ARSessionRunOptionResetTracking
	//ARSessionRunOptionRemoveExistingAnchors
	[mSession runWithConfiguration:Configuration options:(Options)];
}


Arkit::TSession::~TSession()
{
}

void Arkit::TSession::OnFrame(ARFrame* Frame)
{
	std::Debug << "New ARkit frame" << std::endl;
	if ( mOnFrame )
		mOnFrame( Frame );
}




Arkit::TFrameProxyDevice::TFrameProxyDevice(json11::Json& Options) :
	TFrameDevice	( Options )
{
	if ( Options["Source"].is_string() )
	{
		auto Source = Options["Source"].string_value();
		mSource = ArFrameSource::ToType(Source);
	}
	
	//	convert aliases
	if ( mSource == ArFrameSource::Depth )			mSource = ArFrameSource::RearDepth;
	if ( mSource == ArFrameSource::FrontColour )	mSource = ArFrameSource::capturedImage;
	if ( mSource == ArFrameSource::FrontDepth )		mSource = ArFrameSource::capturedDepthData;
	if ( mSource == ArFrameSource::RearDepth )		mSource = ArFrameSource::sceneDepth;

	std::Debug << __PRETTY_FUNCTION__ << " using " << mSource << std::endl;
}

//	https://docs.unity3d.com/Packages/com.unity.xr.arfoundation@3.0/manual/extensions.html
typedef struct UnityXRNativeSessionPtr
{
	int version;
	void* session;
} UnityXRNativeSessionPtr;

void Arkit::TFrameProxyDevice::ReadNativeHandle(void* ArFrameHandle)
{
	if ( !ArFrameHandle )
		throw Soy::AssertException("ReadNativeHandle null");
	
	auto& UnitySessionPtr = *reinterpret_cast<UnityXRNativeSessionPtr*>(ArFrameHandle);
	@try
	{
		//	gr: do some type check!
		auto* ArSession = (__bridge ARSession*)(UnitySessionPtr.session);
		if ( !ArSession )
			throw Soy::AssertException("Failed to cast bridged ARSession pointer");
		
		auto* Frame = ArSession.currentFrame;
		if ( !Frame )
			throw Soy::AssertException("Failed to cast bridged ARFrame pointer");
		
		PushFrame(Frame,mSource);
	}
	@catch (NSException* e)
	{
		std::stringstream Debug;
		Debug << "NSException " << Soy::NSErrorToString( e );
		throw Soy::AssertException(Debug);
	}
}


void Arkit::TFrameProxyDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}


#if !ENABLE_IOS13
class ARSkeleton2D
{
	
};
#endif

namespace Avf
{
	void	GetMeta(ARFrame* Frame,json11::Json::object& Meta);
	void	GetMeta(AVDepthData* Depth,json11::Json::object& Meta);
	void	GetMeta(ARCamera* Camera,json11::Json::object& Meta);
	void	GetMeta(ARDepthData* Camera,json11::Json::object& Meta);

	json11::Json::object	GetSkeleton(ARSkeleton2D* Skeleton);
}


json11::Json::array GetJsonArray(simd_float2 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	return Array;
}

json11::Json::array GetJsonArray(simd_float3 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	Array.push_back( Values[2] );
	return Array;
}

json11::Json::array GetJsonArray(simd_float4 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	Array.push_back( Values[2] );
	Array.push_back( Values[3] );
	return Array;
}

json11::Json::array GetJsonArray(simd_float4x4 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<4;	r++ )
		for ( auto c=0;	c<4;	c++ )
			Array.push_back( Values.columns[c][r] );
	return Array;
}

json11::Json::array GetJsonArrayAs4x4(simd_float4x3 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<3;	r++ )
		for ( auto c=0;	c<4;	c++ )
			Array.push_back( Values.columns[c][r] );
	Array.push_back(0);
	Array.push_back(0);
	Array.push_back(0);
	Array.push_back(1);
	return Array;
}

json11::Json::array GetJsonArray(simd_float3x3 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<3;	r++ )
		for ( auto c=0;	c<3;	c++ )
			Array.push_back( Values.columns[c][r] );
	return Array;
}

json11::Json::array GetJsonArray(CGSize Values)
{
	//	gr: should this return a object with Width/Height?
	json11::Json::array Array;
	Array.push_back( Values.width );
	Array.push_back( Values.height );
	return Array;
}

namespace Avf
{
	namespace GeoTrackingState
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			NotAvailable = ARGeoTrackingStateNotAvailable,
			Initializing = ARGeoTrackingStateInitializing,
			Localizing = ARGeoTrackingStateLocalizing,
			Localized = ARGeoTrackingStateLocalized
#endif
        };
		DECLARE_SOYENUM(GeoTrackingState);
	}

	namespace GeoTrackingAccuracy
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			Undetermined = ARGeoTrackingAccuracyUndetermined,
			Low = ARGeoTrackingAccuracyLow,
			Medium = ARGeoTrackingAccuracyMedium,
			High = ARGeoTrackingAccuracyHigh
#endif
		};
		DECLARE_SOYENUM(GeoTrackingAccuracy);
	}

	namespace GeoTrackingStateReason
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			None = ARGeoTrackingStateReasonNone,
			NotAvailableAtLocation = ARGeoTrackingStateReasonNotAvailableAtLocation,
			NeedLocationPermissions = ARGeoTrackingStateReasonNeedLocationPermissions,
			WorldTrackingUnstable = ARGeoTrackingStateReasonWorldTrackingUnstable,
			WaitingForLocation = ARGeoTrackingStateReasonWaitingForLocation,
			GeoDataNotLoaded = ARGeoTrackingStateReasonGeoDataNotLoaded,
			DevicePointedTooLow = ARGeoTrackingStateReasonDevicePointedTooLow,
			VisualLocalizationFailed = ARGeoTrackingStateReasonVisualLocalizationFailed,
#endif
        };
		DECLARE_SOYENUM(GeoTrackingStateReason);
	}
}


void Avf::GetMeta(ARCamera* Camera,json11::Json::object& Meta)
{
	//	"rotation and translation in world space"
	//	so camera to world?
	auto LocalToWorld = GetJsonArray(Camera.transform);
	auto LocalEulerRadians = GetJsonArray( Camera.eulerAngles );
	auto Tracking = magic_enum::enum_name(Camera.trackingState);
	auto TrackingStateReason = magic_enum::enum_name(Camera.trackingStateReason);
	auto Intrinsics = GetJsonArray(Camera.intrinsics);
	auto CameraResolution = GetJsonArray(Camera.imageResolution);
	
	Meta["LocalToWorld"] = LocalToWorld;
	Meta["LocalEulerRadians"] = LocalEulerRadians;
	Meta["Tracking"] = Tracking;
	Meta["TrackingStateReason"] = TrackingStateReason;
	Meta["Intrinsics"] = Intrinsics;
}


//ARSkeletonJointNameHead
json11::Json::object Avf::GetSkeleton(ARSkeleton2D* Skeleton)
{
	//	gr: cache this as the definition doesn't change
	//		then copy it and replace values
	json11::Json::object SkeletonJson;
	
	
#if ENABLE_IOS14
	auto EnumJoint = [&](NSString* Name)
	{
		auto Position2 = [Skeleton landmarkForJointNamed:Name];
		auto PositionArray = GetJsonArray(Position2);
		auto Key = Soy::NSStringToString(Name);
		SkeletonJson[Key] = PositionArray;
	};
	
	auto Definition = Skeleton.definition;
	auto JointNames = Definition.jointNames;
	Platform::NSArray_ForEach<NSString*>(JointNames,EnumJoint);
#endif

	return SkeletonJson;
}
	
void Avf::GetMeta(ARFrame* Frame,json11::Json::object& Meta)
{
	if ( !Frame )
		return;
	
	if ( Frame.camera )
	{
		json11::Json::object CameraMeta;
		GetMeta(Frame.camera,CameraMeta);
		Meta["Camera"] = CameraMeta;
	}
	
	//	get anchors
	{
		json11::Json::array Anchors;
		auto EnumAnchor = [&](ARAnchor* Anchor)
		{
			auto Uuid = Soy::NSStringToString(Anchor.identifier.UUIDString);
			auto Name = Soy::NSStringToString(Anchor.name);
#if ENABLE_IOS14
			auto SessionUuid = Soy::NSStringToString(Anchor.sessionIdentifier.UUIDString);
#endif
			auto LocalToWorld = GetJsonArray(Anchor.transform);
			json11::Json::object AnchorObject =
			{
				{"Uuid",Uuid},
				{"Name",Name},
#if ENABLE_IOS14
				{"SessionUuid",SessionUuid},
#endif
				{"LocalToWorld",LocalToWorld},
			};
			Anchors.push_back(AnchorObject);
		};
		Platform::NSArray_ForEach<ARAnchor*>(Frame.anchors,EnumAnchor);
		if ( Anchors.size() )
			Meta["Anchors"] = Anchors;
	}
	
	if ( Frame.lightEstimate )
	{
		Meta["LightIntensity"] = Frame.lightEstimate.ambientIntensity;
		Meta["LightTemperature"] = Frame.lightEstimate.ambientColorTemperature;
	}
	
	if ( Frame.rawFeaturePoints )
	{
		//	write all positions and their 64 bit id's (as string)
		auto* PointCloud = Frame.rawFeaturePoints;
		json11::Json::array PositionArray;
		json11::Json::array UidArray;
		for ( auto i=0;	i<PointCloud.count;	i++ )
		{
			auto& Uid64 = PointCloud.identifiers[i];
			auto& Pos3 = PointCloud.points[i];
			auto UidString = std::to_string(Uid64);
			PositionArray.push_back(Pos3[0]);
			PositionArray.push_back(Pos3[1]);
			PositionArray.push_back(Pos3[2]);
			UidArray.push_back(UidString);
		}
		json11::Json::object Features =
		{
			{"Positions",PositionArray},
			{"Uids",UidArray}
		};
		Meta["Features"] = Features;
	}
	

#if ENABLE_IOS14
	if ( Frame.detectedBody && Frame.detectedBody.skeleton )
	{
		auto Skeleton = GetSkeleton( Frame.detectedBody.skeleton );
		Meta["Skeleton"] = Skeleton;
	}
#endif
	
#if ENABLE_IOS14
	if ( Frame.geoTrackingStatus )
	{
		auto State = Avf::GeoTrackingState::ToString(static_cast<Avf::GeoTrackingState::Type>(Frame.geoTrackingStatus.state));
		auto Accuracy = Avf::GeoTrackingAccuracy::ToString(static_cast<Avf::GeoTrackingAccuracy::Type>(Frame.geoTrackingStatus.accuracy));
		auto StateReason = Avf::GeoTrackingStateReason::ToString(static_cast<Avf::GeoTrackingStateReason::Type>(Frame.geoTrackingStatus.stateReason));
		
		Meta["GeoTrackingState"] = State;
		Meta["GeoTrackingStateReason"] = StateReason;
		Meta["GeoTrackingAccuracy"] = Accuracy;
	}
#endif
	
	auto WorldMappingStatus = magic_enum::enum_name(Frame.worldMappingStatus);
	Meta["WorldMappingStatus"] = WorldMappingStatus;
}


void Avf::GetMeta(AVDepthData* Depth,json11::Json::object& Meta)
{
	auto Quality = magic_enum::enum_name(Depth.depthDataQuality);
	auto Accuracy = magic_enum::enum_name(Depth.depthDataAccuracy);
	auto IsFiltered = Depth.depthDataFiltered;
	AVCameraCalibrationData* CameraCalibration = Depth.cameraCalibrationData;
	
	Meta["Quality"] = Quality;
	Meta["Accuracy"] = Accuracy;
	Meta["IsFiltered"] = IsFiltered;
	//	gr: rename to Projection & CameraToWorld
	auto IntrinsicMatrix = GetJsonArray(CameraCalibration.intrinsicMatrix);
	auto IntrinsicPixelToMm = CameraCalibration.pixelSize;
	auto IntrinsicSize = GetJsonArray(CameraCalibration.intrinsicMatrixReferenceDimensions);
	auto ExtrinsicMatrix = GetJsonArrayAs4x4(CameraCalibration.extrinsicMatrix);
	
	json11::Json CameraJson = json11::Json::object
	{
		{ "IntrinsicMatrix",IntrinsicMatrix },
		{ "IntrinsicPixelToMm",IntrinsicPixelToMm },
		{ "IntrinsicSize",IntrinsicSize },
		{ "ExtrinsicMatrix",ExtrinsicMatrix }
	};
	Meta["CameraCalibration"] = CameraJson;
}


void Avf::GetMeta(ARDepthData* Depth,json11::Json::object& Meta)
{
	//	no meta here!
}

void Arkit::TFrameDevice::PushFrame(ARFrame* Frame,ArFrameSource::Type Source)
{
	auto FrameTime = Soy::Platform::GetTime( Frame.timestamp );
	auto CapDepthTime = Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp );
	std::Debug << "timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapDepthTime << std::endl;

	//	get meta out of the ARFrame
	json11::Json::object Meta;
	Avf::GetMeta( Frame, Meta );

	//	read specific pixels
	switch( Source )
	{
		case ArFrameSource::capturedImage:
			PushFrame( Frame.capturedImage, FrameTime, Meta );
			return;
			
		case ArFrameSource::capturedDepthData:
			PushFrame( Frame.capturedDepthData, CapDepthTime, Meta );
			return;
			
#pragma warning ARKit sdk __IPHONE_OS_VERSION_MIN_REQUIRED

		case ArFrameSource::segmentationBuffer:
#if ENABLE_IOS13
			PushFrame( Frame.segmentationBuffer, CapDepthTime, Meta );
#else
			throw Soy::AssertException(std::string("SegmentationBuffer requested, but library built against SDK 13 > ") + std::to_string(__IPHONE_OS_VERSION_MIN_REQUIRED) );
#pragma warning Compiling without ARFrame segmentationBuffer __IPHONE_OS_VERSION_MIN_REQUIRED
#endif
			return;
			
		case ArFrameSource::sceneDepth:
#if ENABLE_IOS14
			Avf::GetMeta( Frame.sceneDepth, Meta );
			PushFrame( Frame.sceneDepth.depthMap, FrameTime, Meta );
#else
#pragma warning Compiling without ARFrame SceneDepth __IPHONE_OS_VERSION_MIN_REQUIRED
			throw Soy::AssertException(std::string("SegmentationBuffer requested, but library built against SDK 13 > ") + std::to_string(__IPHONE_OS_VERSION_MIN_REQUIRED) );
#endif
			return;

		case ArFrameSource::sceneDepthConfidence:
#if ENABLE_IOS14
			Avf::GetMeta( Frame.sceneDepth, Meta );
			PushFrame( Frame.sceneDepth.confidenceMap, FrameTime, Meta );
#else
#pragma warning Compiling without ARFrame SceneDepth __IPHONE_OS_VERSION_MIN_REQUIRED
			throw Soy::AssertException(std::string("SegmentationBuffer requested, but library built against SDK 13 > ") + std::to_string(__IPHONE_OS_VERSION_MIN_REQUIRED) );
#endif
			return;

		default:break;
	}
	
	std::stringstream Debug;
	Debug << __PRETTY_FUNCTION__ << " unhandled source type " << Source;
	throw Soy::AssertException(Debug);
}

void Arkit::TFrameDevice::PushFrame(AVDepthData* DepthData,SoyTime Timestamp,json11::Json::object& Meta)
{
	Soy::TScopeTimerPrint Timer("PushFrame(AVDepthData",5);
	auto DepthPixels = Avf::GetDepthPixelBuffer(DepthData);
	
	Avf::GetMeta( DepthData, Meta );
	PushFrame( DepthPixels, Timestamp, Meta );
}



void Arkit::TFrameDevice::PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp,json11::Json::object& Meta)
{
	Soy::TScopeTimerPrint Timer("PushFrame(CVPixelBufferRef",5);
	float3x3 Transform;
	auto DoRetain = true;
	std::shared_ptr<AvfDecoderRenderer> Renderer;
	
	std::shared_ptr<TPixelBuffer> Buffer( new CVPixelBuffer( PixelBuffer, DoRetain, Renderer, Transform ) );
	PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, Meta );
}



Arkit::TSessionCamera::TSessionCamera(const std::string& DeviceName,json11::Json& Options) :
	TFrameDevice	( Options )
{
	if ( DeviceName == DeviceName_SceneDepth )
		mSource = ArFrameSource::sceneDepth;
	else if ( DeviceName == DeviceName_FrontDepth )
		mSource = ArFrameSource::capturedDepthData;
	else
		throw Soy::AssertException( DeviceName + std::string(" is unhandled ARKit session camera name"));

	//	here we may need to join an existing session
	//	or create one with specific configuration
	//	or if we have to have 1 global, restart it with new configuration
	bool RearCameraSession = mSource == ArFrameSource::sceneDepth;
	mSession.reset( new TSession(RearCameraSession) );
	mSession->mOnFrame = [this](ARFrame* Frame)	{	this->OnFrame(Frame);	};
}

void Arkit::TSessionCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}

void Arkit::TSessionCamera::OnFrame(ARFrame* Frame)
{
	PushFrame( Frame, mSource );
}

