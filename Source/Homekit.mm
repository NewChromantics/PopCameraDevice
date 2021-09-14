#include "Homekit.h"

#import <HomeKit/HomeKit.h>
#include <SoyThread.h>
#include <SoyMedia.h>

//	https://developer.apple.com/documentation/homekit/interacting_with_a_home_automation_network
//	get a home
//	get an accessory
//	get an accessory with a camera
//	get camera stream
//	renders only to a view? do we have to capture that view?
//HMCameraStreamControl

@class CameraStreamDelegate;

namespace Homekit
{
	class TSession;
	
	std::shared_ptr<TSession>	gSession;
	TSession&					GetSession();
	
	bool						IsSupported();
	void						EnumCameraProfiles(std::function<void(const std::string&,ObjcPtr<HMCameraProfile>&)> EnumCameraProfile);
}



class Homekit::TSession
{
public:
	TSession();
	
	HMHomeManager*	mManager = nullptr;
};


class Homekit::TPlatformCamera
{
public:
	TPlatformCamera(ObjcPtr<HMCameraProfile>& Profile,std::function<void(const std::shared_ptr<TPixelBuffer>&)> OnFrame);
	~TPlatformCamera();

	void		OnStreamStarted();
	void		OnStreamStopped(NSError* Error);
	void		CaptureFrame(HMCameraStream* Stream);
	
public:
	ObjcPtr<CameraStreamDelegate>	mDelegate;
	ObjcPtr<HMCameraProfile>		mProfile;
	//ObjcPtr<HMCameraView>			mView;
	ObjcPtr<UIView>			mView;
	ObjcPtr<UIGraphicsImageRenderer>	mRenderer;	//	render view into cg context
	
	std::function<void(const std::shared_ptr<TPixelBuffer>&)>	mOnFrame;
	
	std::shared_ptr<SoyThread>		mThread;
};


@interface CameraStreamDelegate : NSObject<HMCameraStreamControlDelegate>
{
	Homekit::TPlatformCamera*	mParent;
}

- (id)initWithParent:(Homekit::TPlatformCamera*)parent;

- (void)cameraStreamControlDidStartStream:(HMCameraStreamControl *)cameraStreamControl;
- (void)cameraStreamControl:(HMCameraStreamControl *)cameraStreamControl didStopStreamWithError:(NSError *__nullable)error;

@end




@implementation CameraStreamDelegate

- (id)initWithParent:(Homekit::TPlatformCamera*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)cameraStreamControlDidStartStream:(HMCameraStreamControl *)cameraStreamControl
{
	mParent->OnStreamStarted();
}

- (void)cameraStreamControl:(HMCameraStreamControl *)cameraStreamControl didStopStreamWithError:(NSError *__nullable)error
{
	mParent->OnStreamStopped(error);
}

@end





Homekit::TSession& Homekit::GetSession()
{
	if ( !gSession )
		gSession.reset( new TSession );
	return *gSession; 
}


Homekit::TSession::TSession()
{
	if ( !IsSupported() )
		throw Soy::AssertException("HomeKit not supported");
		
	mManager = [[HMHomeManager alloc] init];
	//mManager.delegate = 
}

Homekit::TPlatformCamera::TPlatformCamera(ObjcPtr<HMCameraProfile>& Profile,std::function<void(const std::shared_ptr<TPixelBuffer>&)> OnFrame) :
	mOnFrame	( OnFrame )
{
	mProfile.Retain(Profile);
	auto* Control = Profile.mObject.streamControl;
	mDelegate.Retain( [[CameraStreamDelegate alloc] initWithParent:this] );
	
	//	delegate isnt working :/
	Control.delegate = mDelegate.mObject;
	//[Control setDelegate:mDelegate.mObject];
	
	[Control startStream];
	std::Debug << "Control.streamState:" << Control.streamState << std::endl;
	
	//	null initially
	//auto* Stream = Control.cameraStream;
	//[Control stopStream];


	auto Thread = [this]()
	{
		auto* Control = mProfile.mObject.streamControl;
		auto* Stream = Control.cameraStream;
		std::Debug << "Control.streamState:" << Control.streamState << std::endl;
		
		
		if ( Stream )
		{
			if ( true )
			{
				Soy::TSemaphore Wait;
				auto* pWait = &Wait;
				
				//	gr: this will block, if we're not in an app with a main dispatch thread (ie, our test app running from main)
				auto MainQueue = dispatch_get_main_queue();
				dispatch_async( MainQueue , ^(void){
					this->CaptureFrame(Stream);
					pWait->OnCompleted();
				});
				Wait.Wait();
			}
			else
			{
				this->CaptureFrame(Stream);
			}
		}
			
		
		std::this_thread::sleep_for( std::chrono::milliseconds(100) );
		return true;
	};
	
	mThread.reset( new SoyThreadLambda("Homekit update",Thread) );
}
Homekit::TPlatformCamera::~TPlatformCamera()
{
}

void Homekit::TPlatformCamera::CaptureFrame(HMCameraStream* Stream)
{
	CGRect Frame;
	Frame.origin.x = 0;
	Frame.origin.y = 0;
	Frame.size.width = 640;
	Frame.size.height = 480;
	if ( !mView )
	{
		HMCameraView* CameraView = [[HMCameraView alloc]initWithFrame:Frame];
		CameraView.cameraSource = Stream;
		mView.Retain( CameraView );
		//UIButton* Button = [[UIButton alloc]initWithFrame:Frame];
		//UIButton* Button = [UIButton buttonWithType:UIButtonTypeRoundedRect];    
		//[Button setFrame:Frame];
		//mView.Retain( Button );
		
		auto* Window = [[UIApplication sharedApplication] keyWindow];
		
		[Window addSubview:mView.mObject];
		[Window bringSubviewToFront:mView.mObject];
	}

	UIView* View = mView.mObject;
	CGSize Size = View.bounds.size;
	//Size.width = 640;
	//Size.height = 480;
	
	if ( !mRenderer )
	{
		mRenderer.Retain( [[UIGraphicsImageRenderer alloc] initWithSize:Size] );
	}
	
	UIView* ViewToRender = mView.mObject;
	auto* Window = [[UIApplication sharedApplication] keyWindow];
	//ViewToRender = Window;
	
	static int test = 0;
	test++;

	auto* Renderer = mRenderer.mObject;
	auto Render = ^(UIGraphicsRendererContext*_Nonnull UiContext)
	{
		auto Context = UiContext.CGContext;

		//[[UIColor red] setStroke];
		//[Context strokeRect:Renderer.format.bounds];
		//	clear to black
		//[[UIColor colorWithRed:200/255.0 green:128/255.0 blue:64/255.0 alpha:1] setFill];
		[[UIColor colorWithRed:0 green:0 blue:0 alpha:0] setFill];
		[UiContext fillRect:CGRectMake(0, 0, Size.width, Size.height)];

		
		//	this posts says we can just draw as we're already in the cg context
		//	https://stackoverflow.com/a/13980032/355753
		auto Frame = ViewToRender.frame;
		auto Bounds = ViewToRender.bounds;
		
		
		if ( (test % 4) == 0 )
			[ViewToRender drawRect:Frame]; 
		
		if ( (test % 4) == 1 )
		{
			auto Success = [ViewToRender drawViewHierarchyInRect:Frame afterScreenUpdates:NO];
			if ( !Success )
				std::Debug << "drawViewHierarchyInRect error: " << Success << std::endl;
		}
/*
		if ( (test % 4) == 2 )
		{
			auto Success = [ViewToRender drawViewHierarchyInRect:Frame afterScreenUpdates:YES];
			if ( !Success )
				std::Debug << "drawViewHierarchyInRect error: " << Success << std::endl;
		}
*/
		if ( (test % 4) == 3 )
		{
			CALayer* Layer = ViewToRender.layer;
			//	doesnt seem to alter buffer
			[Layer renderInContext:Context];
		}
	};
	
		[ViewToRender setNeedsDisplay];
		[ViewToRender updateConstraintsIfNeeded];
		[ViewToRender layoutIfNeeded];
	UIImage* snapshotImage = [Renderer imageWithActions:Render];
	UIView* SnapshotView = [ViewToRender snapshotViewAfterScreenUpdates:NO];
	
	
	/*
	//	gr: this doesnt seem to be working
	//2021-09-13 12:51:54.556162+0100 PopCameraDevice_TestApp_Ios[6798:312280] [Unknown process name] CGContextSaveGState: invalid context 0x0. If you want to see the backtrace, please set CG_CONTEXT_SHOW_BACKTRACE environmental variable.
	UIGraphicsBeginImageContextWithOptions(View.bounds.size, YES, 0.0f);
	CGContextRef CgContext = UIGraphicsGetCurrentContext();

	

	UIImage *snapshotImage = UIGraphicsGetImageFromCurrentImageContext();
	UIGraphicsEndImageContext();
	*/
	if ( snapshotImage )
	{
		CGImage* Snapshotcg = snapshotImage.CGImage;
		auto Width = CGImageGetWidth(Snapshotcg);
		auto Height = CGImageGetHeight(Snapshotcg);

		SoyPixelsMeta PixelMeta(Width, Height, SoyPixelsFormat::RGBA);
		std::shared_ptr<TDumbPixelBuffer> PixelBuffer( new TDumbPixelBuffer(PixelMeta) );
		//	render into an rgba image
		auto BytesPerPixel = 4;
		auto BytesPerRow = BytesPerPixel * Width;
		auto& PixelsArray = PixelBuffer->mPixels.GetPixelsArray();
		auto* pPixels = PixelsArray.GetArray();
		auto BitsPerComponent = 8;
		auto ColourspaceRgb = CGColorSpaceCreateDeviceRGB();
		
		auto ContextRgb = CGBitmapContextCreate( pPixels, Width, Height, BitsPerComponent, BytesPerRow, ColourspaceRgb, kCGImageAlphaPremultipliedFirst );
		if ( (test%4)==2)
		{
			UIGraphicsPushContext(ContextRgb);
			[ViewToRender drawViewHierarchyInRect:Frame afterScreenUpdates:NO];
			UIGraphicsPopContext();
		}
		else
		{	
			CGContextDrawImage( ContextRgb, CGRectMake(0, 0, CGFloat(Width), CGFloat(Height)), Snapshotcg );
		}
		
		//	look for non blank pixels
		bool HasPixel = false;
		for ( int i=0;	i<Width*Height*4;	i++ )
		{
			auto p = pPixels[i];
			if ( p!=0 )
			{
				HasPixel = true;
				break;
			}
		}
		std::Debug << "Test #" << (test%4) << " has pixel " << (HasPixel ? "true":"false") << std::endl; 
		
		mOnFrame( PixelBuffer );
		/*		
		
		
		//	no simple way to get CVImageBuffer (to use soyavf code)
		auto BitsPerComponent = CGImageGetBitsPerComponent(Snapshotcg);
		auto BitsPerPixel = CGImageGetBitsPerPixel(Snapshotcg);
		auto Components = BitsPerPixel / BitsPerComponent;
		CGImagePixelFormatInfo Format = CGImageGetPixelFormatInfo(Snapshotcg);

		CFDataRef PixelData = CGDataProviderCopyData(CGImageGetDataProvider(Snapshotcg));
		CFIndex PixelDataSize = CFDataGetLength(PixelData);
		const UInt8* PixelBuffer = CFDataGetBytePtr(PixelData);
		//SoyPixelsFormat::From
		//SoyPixelsMeta Meta( Width, Height, 
		//SoyPixelsRemote Pixels( PixelBuffer, PixelDataSize, 
		CFRelease(PixelData);
		*/
	}
	/*
et renderer = UIGraphicsImageRenderer(size: view.bounds.size)
let image = renderer.image { ctx in
    view.drawHierarchy(in: view.bounds, afterScreenUpdates: true)
}
	*/
}

void Homekit::TPlatformCamera::OnStreamStarted()
{
	std::Debug << "OnStreamStarted" << std::endl;
}

void Homekit::TPlatformCamera::OnStreamStopped(NSError* Error)
{
	std::Debug << "OnStreamStopped(" << Soy::NSErrorToString(Error) << std::endl;
}
	

bool Homekit::IsSupported()
{
	//	note: no error if the capabilties arent included?
	//	but we get this error on another thread if missing from info.plist, which we can check 
	//	[access] This app has crashed because it attempted to access privacy-sensitive data without a usage description.  The app's Info.plist must contain an NSHomeKitUsageDescription key with a string value explaining to the user how the app uses this data.
	auto UsageDescription = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSHomeKitUsageDescription"];
	if ( !UsageDescription )
		return false;

	return true;
}

void Homekit::EnumCameraProfiles(std::function<void(const std::string&,ObjcPtr<HMCameraProfile>&)> EnumCameraProfile)
{
	auto& Session = GetSession();
	
	//	dont always appear immeditely... remove this after testing
	std::this_thread::sleep_for( std::chrono::milliseconds(300));

	auto Homes = Session.mManager.homes;
	for ( auto h=0;	h<[Homes count];	h++ )
	{
		HMHome* Home = [Homes objectAtIndex:h];
		std::Debug << "home " << Soy::NSStringToString(Home.name) << std::endl;
		
		for ( auto a=0;	a<[Home.accessories count];	a++ )
		{
			HMAccessory* Accessory = [Home.accessories objectAtIndex:a];
			std::Debug << "Accessory " << Soy::NSStringToString(Accessory.name) << std::endl;

			std::string AccessoryName = Soy::NSStringToString(Accessory.name);
		
			//	camera interfaces	
			for ( auto c=0;	c<[Accessory.cameraProfiles count];	c++ )
			{
				//	if camera has multiple profiles, we need a way to access them
				HMCameraProfile* Profile = [Accessory.cameraProfiles objectAtIndex:c];
				bool UseSuffix = [Accessory.cameraProfiles count] > 1;
				auto CameraSuffix = UseSuffix ? Soy::NSStringToString(Profile.uniqueIdentifier.UUIDString) : std::string();
				
				std::string CameraName = AccessoryName + CameraSuffix;
				
				ObjcPtr<HMCameraProfile> pProfile( Profile );
				EnumCameraProfile( CameraName, pProfile );
			}
		}
	}
}


void Homekit::EnumDevices(std::function<void(const std::string&)> EnumName)
{
	if ( !IsSupported() )
	{
		std::Debug << "NSHomeKitUsageDescription missing, homekit not supported" << std::endl;
		return;
	}
	
	auto EnumCamera = [&](const std::string& Name,ObjcPtr<HMCameraProfile>& Camera)
	{
		EnumName( Name );
	};
	EnumCameraProfiles( EnumCamera );
}


Homekit::TCamera::TCamera(const std::string& Name,json11::Json& Options)
{
	if ( !IsSupported() )
		throw PopCameraDevice::TInvalidNameException();

	//	find the camera
	ObjcPtr<HMCameraProfile> Profile;
	auto EnumCamera = [&](const std::string& CameraName,ObjcPtr<HMCameraProfile>& Camera)
	{
		if ( Name != CameraName )
			return;
		Profile.Retain(Camera.mObject);
	};
	EnumCameraProfiles( EnumCamera );
	
	if ( !Profile )
		throw PopCameraDevice::TInvalidNameException();
	
	auto OnFrame = [this](const std::shared_ptr<TPixelBuffer>& Pixels)
	{
		SoyTime FrameTime(true);
		json11::Json::object FrameMeta;
		PushFrame(Pixels, FrameTime, FrameMeta );
	};
	mPlatfomCamera.reset( new TPlatformCamera(Profile,OnFrame) );
}
