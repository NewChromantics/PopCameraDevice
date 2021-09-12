#include "Homekit.h"

#import <HomeKit/HomeKit.h>


//	https://developer.apple.com/documentation/homekit/interacting_with_a_home_automation_network
//	get a home
//	get an accessory
//	get an accessory with a camera
//	get camera stream
//	renders only to a view? do we have to capture that view?
//HMCameraStreamControl

namespace Homekit
{
	class TSession;
	
	std::shared_ptr<TSession>	gSession;
	TSession&					GetSession();
}

class Homekit::TSession
{
public:
	TSession();
	
	HMHomeManager*	mManager = nullptr;
};

Homekit::TSession& Homekit::GetSession()
{
	if ( !gSession )
		gSession.reset( new TSession );
	return *gSession; 
}


Homekit::TSession::TSession()
{
	mManager = [[HMHomeManager alloc] init];
	//mManager.delegate = 
}


void Homekit::EnumDevices(std::function<void(const std::string&)> EnumName)
{
	//	check if homekit is supported
	//	note: no error if the capabilties arent included?
	//	but we get this error on another thread if missing from info.plist, which we can check 
	//	[access] This app has crashed because it attempted to access privacy-sensitive data without a usage description.  The app's Info.plist must contain an NSHomeKitUsageDescription key with a string value explaining to the user how the app uses this data.
	auto UsageDescription = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSHomeKitUsageDescription"];
	if ( !UsageDescription )
	{
		std::Debug << "NSHomeKitUsageDescription missing, homekit not supported" << std::endl;
		return;
	}

	auto& Session = GetSession();
	
	//	dont always appear immeditely
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
				EnumName( CameraName );
			}
		}
	}
	
	
}
