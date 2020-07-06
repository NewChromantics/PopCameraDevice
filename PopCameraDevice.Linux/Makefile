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

K4A_LIB = \
$(SRC)/Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so

DEPTHENGINE_LIB = \
$(SRC)/Source/Libs/KinectAzure/linux/lib/native/arm64/release/libdepthengine.so

LOCAL_LINK_LIBS = \
-lk4a	\
-ldepthengine	

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
$(SOY_PATH)/src/SoyMediaFormat.cpp \
$(SOY_PATH)/src/SoyWave.cpp \
$(SOY_PATH)/src/SoyThread.cpp

TESTAPP_SRC_FILES =	\
$(SRC)/Source/PopCameraDevice_TestApp.cpp 

LOCAL_DEFINES = \
-DTARGET_LINUX	\
-DENABLE_KINECTAZURE	\

# PIC means position independent code, required for dynamically loaded symbols
CFLAGS = -c $(LOCAL_DEFINES) $(LOCAL_C_INCLUDES) -std=gnu++17 -fPIC

# set the rpath (runtime library search path) to a special $ORIGIN so it looks in the same dir as the executable
# https://serverfault.com/a/402590
# -Wl is a join-arguments-with-spaces thing
LDFLAGS = -pthread 
LDFLAGS += '-Wl,-rpath,$$ORIGIN'

LIB_OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o)
TESTAPP_OBJECTS=$(TESTAPP_SRC_FILES:.cpp=.o)

ARCHITECTURE=${archTarget}
OUT_LIBRARY=PopCameraDevice.so
OUT_EXECUTABLE=PopCameraTest

exec: $(OUT_EXECUTABLE) 
.PHONY: exec

lib: $(OUT_LIBRARY) 
.PHONY: lib

$(OUT_EXECUTABLE): $(TESTAPP_OBJECTS) $(OUT_LIBRARY)
	echo "Building test app $(OUT_EXECUTABLE)"
	$(CC) $(LDFLAGS) $^ -o $@ $(LOCAL_LINK_DIRS) $(LOCAL_LINK_LIBS)

$(OUT_LIBRARY): $(K4A_LIB) $(LIB_OBJECTS) 
	echo "Building library $(OUT_LIBRARY)"
	g++ -shared -o $@ $(LIB_OBJECTS)
	#ar rcs $@ $(LIB_OBJECTS)
	# when running, the kinect code is looking for the original name, but we renamed it for linking...
	cp $(K4A_LIB) ./libk4a.so.1.4
	cp $(DEPTHENGINE_LIB) .

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	# clean build files, anything .o
	echo "Cleaning files"
	rm -f $(TESTAPP_OBJECTS)
	rm -f $(LIB_OBJECTS)	
	#find ../ -type f -name '*.o' -delete
	rm -f $(OUT_LIBRARY)
	rm -f $(OUT_EXECUTABLE)


$(K4A_LIB):
# sudo apt-get install nuget
#	nuget restore ../Libs/
	wget https://www.nuget.org/api/v2/package/Microsoft.Azure.Kinect.Sensor/1.4.1 -O ../Source/Libs/MicrosoftAzureKinectSensor.zip
	unzip -o ../Source/Libs/MicrosoftAzureKinectSensor.zip -d ../Source/Libs/KinectAzure
	ln ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so.1.4.1 $(K4A_LIB)

	
