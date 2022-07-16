# exit when any command fails
set -e

PROJECT_NAME=$1
BUILDPATH_IOS="${BUILT_PRODUCTS_DIR}/${PROJECT_NAME}_Ios"
BUILDPATH_SIM="${BUILT_PRODUCTS_DIR}/${PROJECT_NAME}_IosSimulator"
BUILDPATH_OSX="${BUILT_PRODUCTS_DIR}/${PROJECT_NAME}_Osx"
echo "BUILDPATH_IOS=${BUILDPATH_IOS}"
echo "BUILDPATH_SIM=${BUILDPATH_SIM}"
echo "BUILDPATH_OSX=${BUILDPATH_OSX}"
xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -sdk iphoneos
xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_SIM SKIP_INSTALL=NO -sdk iphonesimulator
xcodebuild -jobs 1 archive -scheme ${PROJECT_NAME}_Osx -archivePath $BUILDPATH_OSX SKIP_INSTALL=NO
xcodebuild -jobs 1 -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_SIM}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_OSX}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Osx.framework -output ${BUILT_PRODUCTS_DIR}/${FULL_PRODUCT_NAME}


