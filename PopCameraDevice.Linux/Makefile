# Raspberry Pi
# CC = arm-linux-gnueabihf-g++

# Standard
CC = g++
SRC = ..
SOY_PATH = $(SRC)/Source/SoyLib

LOCAL_C_INCLUDES = \
-I$(SRC)/Source	\
-I$(SOY_PATH)/src	\
-I$(SRC)/Source/MagicEnum/include \
-I$(SRC)/Source/Libs/KinectAzure/build/native/include

LOCAL_LINK_DIRS = \
-L$(SRC)/Source/Libs/KinectAzure/linux/lib/native/arm64/release

K4ALIB = \
$(SRC)/Source/Libs/KinectAzure/Linux/lib/native/arm64/release/libk4a.so

LOCAL_LINK_LIBS = \
-lk4a	\
-lk4arecord.so.1.4.1	\
-ldepthengine	\
-ldepthengine.so.2.0

LOCAL_SRC_FILES  = \
$(SRC)/Source/PopCameraDevice.cpp \
$(SRC)/Source/TestDevice.cpp \
$(SRC)/Source/KinectAzure.cpp \
$(SRC)/Source/TCameraDevice.cpp \
$(SRC)/Source/TOutputDevice.cpp 

# soy lib files
LOCAL_SRC_FILES  += \
$(SOY_PATH)/src/SoyTypes.cpp \
$(SOY_PATH)/src/SoyAssert.cpp \
$(SOY_PATH)/src/SoyDebug.cpp \
$(SOY_PATH)/src/SoyPixels.cpp \
$(SOY_PATH)/src/memheap.cpp \
$(SOY_PATH)/src/SoyArray.cpp \
$(SOY_PATH)/src/SoyTime.cpp \
$(SOY_PATH)/src/SoyString.cpp \
$(SOY_PATH)/src/SoyH264.cpp \
$(SOY_PATH)/src/SoyPng.cpp \
$(SOY_PATH)/src/SoyImage.cpp \
$(SOY_PATH)/src/SoyStreamBuffer.cpp \
$(SOY_PATH)/src/SoyFourcc.cpp \
$(SOY_PATH)/src/SoyMedia.cpp \
$(SOY_PATH)/src/SoyThread.cpp

LOCAL_BUILD_FILES =	\
$(SRC)/Source/PopCameraDevice_TestApp.cpp 

LOCAL_DEFINES = \
-DTARGET_LINUX	\
-DENABLE_KINECTAZURE	\

# PIC means position independent code, required for dynamically loaded symbols
CFLAGS = -c $(LOCAL_DEFINES) $(LOCAL_C_INCLUDES) -std=gnu++17 -fPIC
LDFLAGS = -static -pthread $(LOCAL_LINK_DIRS) $(LOCAL_LINK_LIBS)

OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o) $(LOCAL_BUILD_FILES:.cpp=.o)
LIB_OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o)
BUILD_OBJECTS=$(LOCAL_BUILD_FILES:.cpp=.o)

ARCHITECTURE=${archTarget}
OUT_LIBRARY=PopCameraDevice.so
OUT_EXECUTABLE=PopCameraTest

exec: $(OUT_EXECUTABLE) 
.PHONY: exec

lib: $(OUT_LIBRARY) 
.PHONY: lib

$(OUT_EXECUTABLE): $(BUILD_OBJECTS) $(OUT_LIBRARY)
	$(CC) $(LDFLAGS) $^ -o $@

$(OUT_LIBRARY): $(K4A_LIB) $(OBJECTS) 
	g++ -shared -o $@ $(LIB_OBJECTS)
	#ar rcs $@ $(LIB_OBJECTS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	# clean build files, anything .o
	find ../ -type f -name '*.o' -delete
	rm -f $(OUT_LIBRARY)
	rm -f $(OUT_EXECUTABLE)


$(K4A_LIB):
# sudo apt-get install nuget
#	nuget restore ../Libs/
	wget https://www.nuget.org/api/v2/package/Microsoft.Azure.Kinect.Sensor/1.4.1 -O ../Source/Libs/MicrosoftAzureKinectSensor.zip
	unzip -o ../Source/Libs/MicrosoftAzureKinectSensor.zip -d ../Source/Libs/KinectAzure
	ln ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so.1.4.1 ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so

	
