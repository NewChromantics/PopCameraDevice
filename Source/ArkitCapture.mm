#include "ArkitCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"
#import <ARKit/ARKit.h>
#include "Json11/json11.hpp"
#include "PopCameraDevice.h"

//  gr: make this a proper check, quickly disabling for build here
#define ENABLE_IOS14    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 140000)
#define ENABLE_IOS13    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
#define ENABLE_IOS12   (__IPHONE_OS_VERSION_MAX_ALLOWED >= 120000)

#pragma warning ARKit sdk __IPHONE_OS_VERSION_MIN_REQUIRED

#if ENABLE_IOS12
#pragma message("IOS 12")
#endif

#if ENABLE_IOS13
#pragma message("IOS 13")
#endif

#if ENABLE_IOS14
#pragma message("IOS 14")
#endif



@interface ARSessionProxy : NSObject<ARSessionObserver,ARSCNViewDelegate,ARSessionDelegate>
{
	Arkit::TSession*	mParent;
}

- (id)initWithSession:(Arkit::TSession*)parent;
- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame;

- (void)session:(ARSession *)session didFailWithError:(NSError *)error;
- (void)session:(ARSession *)session cameraDidChangeTrackingState:(ARCamera *)camera;
- (void)sessionWasInterrupted:(ARSession *)session;
- (void)sessionInterruptionEnded:(ARSession *)session;
- (BOOL)sessionShouldAttemptRelocalization:(ARSession *)session API_AVAILABLE(ios(11.3));
- (void)session:(ARSession *)session didOutputAudioSampleBuffer:(CMSampleBufferRef)audioSampleBuffer;
- (void)session:(ARSession *)session didOutputCollaborationData:(ARCollaborationData *)data API_AVAILABLE(ios(13.0));
- (void)session:(ARSession *)session didChangeGeoTrackingStatus:(ARGeoTrackingStatus*)geoTrackingStatus API_AVAILABLE(ios(14.0));


@end


class Arkit::TSession
{
public:
	TSession(bool RearCamera,TCaptureParams& Params);
	~TSession();
	
	void				OnFrame(ARFrame* Frame);
	
	std::function<void(ARFrame*)>	mOnFrame;
	ObjcPtr<ARSessionProxy>			mSessionProxy;
	ObjcPtr<ARSession>				mSession;

	//	the test app sits on the main thread, so no frames were coming through
	dispatch_queue_t				mQueue = nullptr;
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
	try
	{
		mParent->OnFrame(frame);
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

- (void)session:(ARSession *)session didFailWithError:(NSError *)error
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session cameraDidChangeTrackingState:(ARCamera *)camera
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)sessionWasInterrupted:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)sessionInterruptionEnded:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (BOOL)sessionShouldAttemptRelocalization:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	return true;
}

- (void)session:(ARSession *)session didOutputAudioSampleBuffer:(CMSampleBufferRef)audioSampleBuffer
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session didOutputCollaborationData:(ARCollaborationData *)data
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session didChangeGeoTrackingStatus:(ARGeoTrackingStatus*)geoTrackingStatus
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}



@end


Arkit::TCaptureParams::TCaptureParams(json11::Json& Options)
{
	auto SetInt = [&](const char* Name,size_t& ValueUnsigned)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_number() )
			return false;
		auto Value = Handle.int_value();
		if ( Value < 0 )
		{
			std::stringstream Error;
			Error << "Value for " << Name << " is " << Value << ", not expecting negative";
			throw Soy::AssertException(Error);
		}
		ValueUnsigned = Value;
		return true;
	};
	auto SetBool = [&](const char* Name,bool& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_bool() )
			return false;
		Value = Handle.bool_value();
		return true;
	};
	auto SetString = [&](const char* Name,std::string& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_string() )
			return false;
		Value = Handle.string_value();
		return true;
	};

	auto SetPixelFormat = [&](const char* Name,SoyPixelsFormat::Type& Value)
	{
		std::string EnumString;
		if ( !SetString(Name,EnumString) )
			return false;
		
		Value = SoyPixelsFormat::Validate(EnumString);
		return true;
	};

	SetBool( POPCAMERADEVICE_KEY_HDRCOLOUR, mHdrColour );
	SetBool( POPCAMERADEVICE_KEY_ENABLEAUTOFOCUS, mEnableAutoFocus );
	SetBool( POPCAMERADEVICE_KEY_ENBALEPLANETRACKING, mEnablePlanesHorz );
	SetBool( POPCAMERADEVICE_KEY_ENABLEFACETRACKING, mEnableFaceTracking );
	SetBool( POPCAMERADEVICE_KEY_ENABLELIGHTESTIMATION, mEnableLightEstimation );
	SetBool( POPCAMERADEVICE_KEY_ENABLEBODYTRACKING, mEnableBodyDetection );
	SetBool( POPCAMERADEVICE_KEY_ENABLESEGMENTATION, mEnablePersonSegmentation );
	SetBool( POPCAMERADEVICE_KEY_RESETTRACKING, mResetTracking );
	SetBool( POPCAMERADEVICE_KEY_RESETANCHORS, mResetAnchors );
	SetBool( POPCAMERADEVICE_KEY_FEATURES, mOutputFeatures );
	SetBool( POPCAMERADEVICE_KEY_DEPTHCONFIDENCE, mOutputSceneDepthConfidence );
	

	//	probably don't need this to be seperate
	mEnablePlanesVert = mEnablePlanesHorz;

	//	any format = enable	
	auto DepthFormat = SoyPixelsFormat::Invalid;
	SetPixelFormat( POPCAMERADEVICE_KEY_DEPTHFORMAT, DepthFormat );
	mEnableSceneDepth = ( DepthFormat != SoyPixelsFormat::Invalid );
	
	SetPixelFormat( POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
}

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


Arkit::TSession::TSession(bool RearCamera,TCaptureParams& Params)
{
	mSessionProxy.Retain([[ARSessionProxy alloc] initWithSession:(this)]);
	
	mSession.Retain( [[ARSession alloc] init] );
	auto* Session = mSession.mObject;

	id<ARSessionDelegate> Proxy = mSessionProxy.mObject;

	mQueue = dispatch_queue_create( __PRETTY_FUNCTION__, NULL);
		
	Session.delegate = Proxy;
	Session.delegateQueue = mQueue;
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
	auto WorldSupported = [ARWorldTrackingConfiguration isSupported];		
	auto WorldSupportsMeshing = [ARWorldTrackingConfiguration supportsSceneReconstruction:ARSceneReconstructionMesh];
	auto WorldSupportsMeshingWithClassification = [ARWorldTrackingConfiguration supportsSceneReconstruction:ARSceneReconstructionMeshWithClassification];
	auto WorldSupportsFaceTracking = [ARWorldTrackingConfiguration supportsUserFaceTracking];
	auto FrontSupported = [ARFaceTrackingConfiguration isSupported];		
	auto BodyTrackingSupported = [ARBodyTrackingConfiguration isSupported];
	
	NSArray<ARVideoFormat *>* WorldVideoFormats = ARWorldTrackingConfiguration.supportedVideoFormats;

	
	std::Debug << "Rear supported=" << WorldSupported << std::endl;
	std::Debug << "Rear SupportsMeshing=" << WorldSupportsMeshing << std::endl;
	std::Debug << "Rear SupportsMeshingWithClassification=" << WorldSupportsMeshingWithClassification << std::endl;
	std::Debug << "Rear SupportsFaceTracking=" << WorldSupportsFaceTracking << std::endl;
	std::Debug << "FrontSupported=" << FrontSupported << std::endl;
	std::Debug << "BodyTrackingSupported=" << BodyTrackingSupported << std::endl;

	
	ARConfiguration* Configuration = nullptr;
	if ( RearCamera )
	{
		auto* WorldConfig = [[ARWorldTrackingConfiguration alloc] init];
		Configuration = WorldConfig;
		
		//	oo other format!
		WorldConfig.wantsHDREnvironmentTextures = Params.mHdrColour;	

		//	on by default
		WorldConfig.autoFocusEnabled = Params.mEnableAutoFocus;

		//	gr: does this need to be if colourformat != null ?
		//	AREnvironmentTexturingNone, AREnvironmentTexturingManual, AREnvironmentTexturingAutomatic
		WorldConfig.environmentTexturing = AREnvironmentTexturingAutomatic;
		
		//	enable various options
		WorldConfig.planeDetection = ARPlaneDetectionNone;
		if ( Params.mEnablePlanesHorz )
			WorldConfig.planeDetection |= ARPlaneDetectionHorizontal;
		if ( Params.mEnablePlanesVert )
			WorldConfig.planeDetection |= ARPlaneDetectionVertical;
			
		// NSSet<ARReferenceImage *> *detectionImages API_AVAILABLE(ios(11.3));

		//	enable face tracking (not supported)
		WorldConfig.userFaceTrackingEnabled = Params.mEnableFaceTracking && WorldSupportsFaceTracking;
	
		WorldConfig.lightEstimationEnabled = Params.mEnableLightEstimation;
		
		WorldConfig.frameSemantics = ARFrameSemanticNone;
		if ( Params.mEnableBodyDetection )
			WorldConfig.frameSemantics |= ARFrameSemanticBodyDetection;
		if ( Params.mEnableSceneDepth )
			WorldConfig.frameSemantics |= ARFrameSemanticSceneDepth;
		if ( Params.mEnablePersonSegmentation )
			WorldConfig.frameSemantics |= ARFrameSemanticPersonSegmentation;
		
		WorldConfig.worldAlignment = ARWorldAlignmentGravityAndHeading;
		WorldConfig.worldAlignment = ARWorldAlignmentCamera;
		WorldConfig.worldAlignment = ARWorldAlignmentGravity;
		
		//	todo: use colour format
		WorldConfig.videoFormat = WorldVideoFormats[2];
	}
	else
	{
		Configuration = [[ARFaceTrackingConfiguration alloc] init];
	}
	
	ARSessionRunOptions Options = 0;
	if ( Params.mResetTracking )
		Options |= ARSessionRunOptionResetTracking;
	if ( Params.mResetAnchors )
		Options |= ARSessionRunOptionRemoveExistingAnchors;

	if ( Options == 0 )
		[Session runWithConfiguration:Configuration];
	else
		[Session runWithConfiguration:Configuration options:(Options)];
	
	auto* Frame = Session.currentFrame;
	std::Debug << "Currentframe " << Frame << std::endl;
}


Arkit::TSession::~TSession()
{
	//	delete queue
	if ( mQueue )
	{
#if !defined(ARC_ENABLED)
		dispatch_release( mQueue );
#endif
		mQueue = nullptr;
	}
	
	mSessionProxy.Release();
	mSession.Release();
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
	void	GetMeta(ARFrame* Frame,json11::Json::object& Meta,Arkit::TCaptureParams& Params);
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

bool IsIdentity(simd_float4x4 Matrix)
{
	static const simd_float4x4 Identity = 
	{
		.columns[0]={1,0,0,0},
		.columns[1]={0,1,0,0},
		.columns[2]={0,0,1,0},
		.columns[3]={0,0,0,1} 
	};
	for ( int c=0;	c<std::size(Matrix.columns);	c++ )
		for ( int r=0;	r<4;	r++ )
			if ( Matrix.columns[c][r] != Identity.columns[c][r] )
				return false;
	return true;
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
	
	//	skip transform if it's identity rather than write out bad data
	if ( !IsIdentity(Camera.transform) )
		Meta["LocalToWorld"] = LocalToWorld;
	
	Meta["LocalEulerRadians"] = LocalEulerRadians;
	Meta["Tracking"] = std::string(Tracking);	//	json11 can't do string_view atm
	Meta["TrackingStateReason"] = TrackingStateReason;	//	json11 can't do string_view atm
	Meta["Intrinsics"] = Intrinsics;
	//	write out original resolution to match Intrinsics matrix in case image gets resized
	//	gr: convert to normalised projection matrix here!
	Meta["IntrinsicsCameraResolution"] = CameraResolution;
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
	
void Avf::GetMeta(ARFrame* Frame,json11::Json::object& Meta,Arkit::TCaptureParams& Params)
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
		Meta["FeatureCount"] = static_cast<int>(PointCloud.count);
		
		if ( Params.mOutputFeatures )
		{	
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


Arkit::TFrameDevice::TFrameDevice(json11::Json& Options) : 
	TDevice	( Options ),
	mParams	( Options )
{
	//	todo; get capabilities and reject params here
}


void Arkit::TFrameDevice::PushFrame(ARFrame* Frame,ArFrameSource::Type Source)
{
	auto FrameTime = Soy::Platform::GetTime( Frame.timestamp );
	auto CapDepthTime = Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp );
	std::Debug << "timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapDepthTime << std::endl;

	//	todo: allow user to specify params here with json
	json11::Json JsonOptions;
	TCaptureParams Params(JsonOptions);
	Params.mOutputFeatures = false;

	//	get meta out of the ARFrame
	json11::Json::object Meta;
	Avf::GetMeta( Frame, Meta, Params );

	//	gr: now we can handle multiple streams, just output anything we have
	if ( Frame.capturedImage )
		PushFrame( Frame.capturedImage, FrameTime, Meta );

	if ( Frame.capturedDepthData )
		PushFrame( Frame.capturedDepthData, CapDepthTime, Meta, "CapturedDepthData" );
			
#if ENABLE_IOS13
	if ( Frame.segmentationBuffer )
		PushFrame( Frame.segmentationBuffer, CapDepthTime, Meta, "SegmentationBuffer" );
#endif
			
#if ENABLE_IOS14
	if ( Frame.sceneDepth )
	{
		Avf::GetMeta( Frame.sceneDepth, Meta );
		if ( Frame.sceneDepth.depthMap )
			PushFrame( Frame.sceneDepth.depthMap, FrameTime, Meta, "SceneDepthMap" );

		if ( Frame.sceneDepth.confidenceMap )
			if ( mParams.mOutputSceneDepthConfidence )
				PushFrame( Frame.sceneDepth.confidenceMap, FrameTime, Meta, "SceneDepthConfidence" );
	}
#endif
}

void Arkit::TFrameDevice::PushFrame(AVDepthData* DepthData,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName)
{
	Soy::TScopeTimerPrint Timer("PushFrame(AVDepthData",5);
	auto DepthPixels = Avf::GetDepthPixelBuffer(DepthData);
	
	Avf::GetMeta( DepthData, Meta );
	PushFrame( DepthPixels, Timestamp, Meta, StreamName );
}



void Arkit::TFrameDevice::PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName)
{
	Soy::TScopeTimerPrint Timer("PushFrame(CVPixelBufferRef",5);
	float3x3 Transform;
	auto DoRetain = true;
	std::shared_ptr<AvfDecoderRenderer> Renderer;
	std::shared_ptr<TPixelBuffer> Buffer( new CVPixelBuffer( PixelBuffer, DoRetain, Renderer, Transform ) );
	
	if ( StreamName )
	{
		json11::Json::object MetaCopy = Meta;
		MetaCopy["StreamName"] = StreamName;
		PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, MetaCopy );
	}
	else
	{
		PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, Meta );
	}
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
	@try
	{
		mSession.reset( new TSession(RearCameraSession,mParams) );
	}
	@catch (NSException* e)
	{
		std::stringstream Debug;
		Debug << "NSException " << Soy::NSErrorToString( e );
		throw Soy::AssertException(Debug);
	}
	mSession->mOnFrame = [this](ARFrame* Frame)	
	{	
		this->OnFrame(Frame);	
	};
}

void Arkit::TSessionCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}

void Arkit::TSessionCamera::OnFrame(ARFrame* Frame)
{
	PushFrame( Frame, mSource );
}

