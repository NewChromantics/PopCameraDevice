LIB_NAME = PopCameraDevice
APP_NAME = PopCameraDeviceTestApp

# X ?= if X env var isn't set, use this default
compiler ?= g++
CC = ${compiler}

# osTarget is the target os eg. Nvidia / Pi4
# tsdk: the naming of the build folders are handled by the github actions
# osTarget is still used for if statements in the make file
osTarget ?= Linux
BUILD_DIR = ../Build

PROJECT_PATH = ..
SRC_PATH = $(PROJECT_PATH)/Source
SRCTESTAPP_PATH = $(PROJECT_PATH)/Source_TestApp
SOY_PATH = $(PROJECT_PATH)/Source/SoyLib

LIB_INCLUDES = \
-I$(SRC_PATH)	\
-I$(SRC_PATH)/MagicEnum/include \
-I$(SOY_PATH)/src	\


LIB_SRC_FILES  = \
$(SRC_PATH)/PopCameraDevice.cpp \
$(SRC_PATH)/TestDevice.cpp \
$(SRC_PATH)/TCameraDevice.cpp \
$(SRC_PATH)/Json11/json11.cpp	\
$(SRC_PATH)/JsonFunctions.cpp	\

# soy lib files
LIB_SRC_FILES  += \
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
$(SOY_PATH)/src/SoyRuntimeLibrary.cpp \
$(SOY_PATH)/src/SoyThread.cpp \
$(SOY_PATH)/src/SoyFilesystem.cpp \
$(SOY_PATH)/src/SoyPlatform.cpp \


APP_SRC_FILES =	\
$(SRCTESTAPP_PATH)/PopCameraDevice_TestApp.cpp 


LIB_DEFINES = \
-DTARGET_LINUX	\

ifeq ($(osTarget),Nvidia)
	LIB_INCLUDES += \
	-I$(SRC_PATH)/Libs/KinectAzure/build/native/include

	LIB_SRC_FILES  += \
	$(SRC_PATH)/KinectAzure.cpp \

	LIB_LINK_LIBS = \
	-lk4a	\
	-ldepthengine	\
	-L/usr/lib/aarch64-linux-gnu/libk4a1.4	\
	-L/usr/lib/aarch64-linux-gnu/

	LIB_DEFINES += \
	-DENABLE_KINECTAZURE	\

	# -L$(SRC)/Source/Libs/KinectAzure/linux/lib/native/arm64/release

	# gr: these are now installed in the OS
	K4A_LIB = \
	$(SRC_PATH)/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so

	DEPTHENGINE_LIB = \
	$(SRC_PATH)/Libs/KinectAzure/linux/lib/native/arm64/release/libdepthengine.so
else
	NON_KINECT_LINK_LIBS = -ldl
endif

# -g stops symbol stripping
# -f[lag] PIC position independent code (for dyanmically loaded code)
# -rpath $ORIGIN makes the shared object runtimepath be same place as executable
CFLAGS = -c $(LIB_DEFINES) $(LIB_INCLUDES) -std=c++17 -fPIC -g
LDFLAGS = -pthread
LDFLAGS += '-Wl,-rpath,$$ORIGIN'
LDFLAGS += '-Wl,-rpath-link=$(BUILD_DIR)'
# Don't let symbols get overriden on load	https://stackoverflow.com/questions/6538501/linking-two-shared-libraries-with-some-of-the-same-symbols
LDFLAGS += '-Wl,-Bsymbolic'

LIB_LINK_FLAGS = $(LDFLAGS) -shared
APP_LINK_FLAGS = $(LDFLAGS) 

LIB_OBJECTS=$(LIB_SRC_FILES:.cpp=.o)
APP_OBJECTS=$(APP_SRC_FILES:.cpp=.o)

OUT_LIB=$(BUILD_DIR)/lib${LIB_NAME}.so
OUT_APP=$(BUILD_DIR)/${APP_NAME}

# These files (without directory structure) get copied to build dir
OUT_FILE_SOURCES = \
$(SRC_PATH)/PopCameraDevice.h	\

# convert to Build/xxx target files
OUT_FILES = $(addprefix ${BUILD_DIR}/,$(notdir ${OUT_FILE_SOURCES}))
#$(info out files...)
#$(info ${OUT_FILES})


GithubWorkflow: $(OUT_APP) $(OUT_LIB)
	$(info ::set-output name=executable:$(OUT_APP))
	$(info ::set-output name=library:$(OUT_LIB))
.PHONY: GithubWorkflow

exec: $(OUT_APP) 
.PHONY: exec

lib: $(OUT_LIB) 
.PHONY: lib

# Copy other output files
$(OUT_FILES): $(OUT_FILE_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(info Copying output files $(OUT_FILES))
	cp $(OUT_FILE_SOURCES) $(BUILD_DIR)

$(OUT_APP): $(APP_OBJECTS) $(OUT_LIB)
	$(info Building app $(OUT_APP))
	mkdir -p $(BUILD_DIR)
	$(CC) $(APP_LINK_FLAGS) $(APP_OBJECTS) -o $@ -L$(BUILD_DIR) -l$(LIB_NAME) $(NON_KINECT_LINK_LIBS)

# $(K4A_LIB)
$(OUT_LIB): $(LIB_OBJECTS) $(OUT_FILES)
	$(info Building library $(OUT_LIB))
	$(CC) $(LIB_LINK_FLAGS) -o $@ $(LIB_OBJECTS) $(LIB_LINK_LIBS)
	$(info Listing dependencies of $(OUT_LIB))
	ldd $(OUT_LIB)

.cpp.o:
	$(CC) $(CFLAGS) $(APP_INCLUDES) $(LIB_INCLUDES) $< -o $@

clean:
	# clean build files, anything .o
	$(info Cleaning files)
	rm -f $(APP_OBJECTS)
	rm -f $(LIB_OBJECTS)
	rm -f $(OUT_LIB)
	rm -f $(OUT_APP)


# gr: this is redundant now, as we have to install to OS
KinectAzureDependencies: $(K4A_LIB)


$(K4A_LIB):
# sudo apt-get install nuget
#	nuget restore ../Libs/
	mkdir -p ../Source/Libs/KinectAzure
	echo "downloading Kinect package..."
	wget https://www.nuget.org/api/v2/package/Microsoft.Azure.Kinect.Sensor/1.4.1 -O ../Source/Libs/MicrosoftAzureKinectSensor.zip
	unzip -o ../Source/Libs/MicrosoftAzureKinectSensor.zip -d ../Source/Libs/KinectAzure
	ln ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so.1.4.1 $(K4A_LIB)
	# at runtime the code(maybe depth engine?) tries to load libk4a.1.4 instead of 1.4.1
	ln ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so.1.4.1 ../Source/Libs/KinectAzure/linux/lib/native/arm64/release/libk4a.so.1.4

