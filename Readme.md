Build Status
==========================
![Build Windows](https://github.com/SoylentGraham/PopCameraDevice/workflows/Build%20Windows/badge.svg)
![Build Windows_NoKinect](https://github.com/SoylentGraham/PopCameraDevice/workflows/Build%20Windows%20NoKinect/badge.svg)
![Build Osx](https://github.com/SoylentGraham/PopCameraDevice/workflows/Build%20Osx/badge.svg)
![Build Ios](https://github.com/SoylentGraham/PopCameraDevice/workflows/Build%20Ios/badge.svg)
![Build Linux](https://github.com/SoylentGraham/PopH264/workflows/Build%20Linux/badge.svg)

Kinect Azure
============
- Windows compiles with SDK path
- Linux comes via nuget package (Comes with windows, so we should switch to that)
- Added a `packages.config` in `/Source/Libs/`
- Osx
	- `brew install nuget`
	- `cd Source/Libs`
	- `nuget install`
- Linux
	- `sudo apt install nuget`
	- Install rules so KinectAzure can run without root access; https://github.com/microsoft/Azure-Kinect-Sensor-SDK/blob/develop/docs/usage.md#linux-device-setup
- Windows
	- We should be able to add packages.config to the project...
