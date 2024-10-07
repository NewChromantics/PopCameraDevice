# exit when any command fails
set -e

PROJECT_NAME=$1

# build temporary dir
BUILD_DIR="./build"
#	use BUILT_PRODUCTS_DIR for PopAction_Apple which gets first stdout output
BUILD_UNIVERSAL_DIR=${BUILT_PRODUCTS_DIR}


BUILDPATH_IOS="${BUILD_DIR}/${PROJECT_NAME}_Ios"
BUILDPATH_SIM="${BUILD_DIR}/${PROJECT_NAME}_IosSimulator"
BUILDPATH_OSX="${BUILD_DIR}/${PROJECT_NAME}_Osx"
echo "BUILDPATH_IOS=${BUILDPATH_IOS}"
echo "BUILDPATH_SIM=${BUILDPATH_SIM}"
echo "BUILDPATH_OSX=${BUILDPATH_OSX}"

PRODUCT_NAME_UNIVERSAL="${PROJECT_NAME}.xcframework"

CONFIGURATION="Release"
DESTINATION_IOS="generic/platform=iOS"
DESTINATION_MACOS="platform=macOS"


xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -sdk iphoneos
xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_SIM SKIP_INSTALL=NO -sdk iphonesimulator
xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Osx -archivePath $BUILDPATH_OSX SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination ${DESTINATION_MACOS}

echo "Building xcframework to ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL} ..."

xcodebuild -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_SIM}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_IosSimulator.framework -framework ${BUILDPATH_OSX}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Osx.framework -output ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}

echo "xcframework ${PRODUCT_NAME_UNIVERSAL} built successfully"


# output meta for https://github.com/NewChromantics/PopAction_BuildApple
# which matches a regular scheme output
echo "FULL_PRODUCT_NAME=${PRODUCT_NAME_UNIVERSAL}"
echo "BUILT_PRODUCTS_DIR=${BUILD_UNIVERSAL_DIR}"
