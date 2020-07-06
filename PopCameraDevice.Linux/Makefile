# Raspberry Pi
# CC = arm-linux-gnueabihf-g++

# Standard
CC = g++
SRC = ..
SOY_PATH = $(SRC)/Source/SoyLib

LOCAL_C_INCLUDES = \
-I$(SRC)/Source	\
-I$(SOY_PATH)/src	\
-I$(SRC)/Source/MagicEnum/include

LOCAL_SRC_FILES  = \
$(SRC)/Source/PopCameraDevice.cpp \
$(SRC)/Source/TestDevice.cpp \
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

CFLAGS = -c -DTARGET_LINUX $(LOCAL_C_INCLUDES) -std=gnu++17
LDFLAGS = -static -pthread

OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o) $(LOCAL_BUILD_FILES:.cpp=.o)
LIB_OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o)
BUILD_OBJECTS=$(LOCAL_BUILD_FILES:.cpp=.o)

ARCHITECTURE=${archTarget}
LIBRARY=PopCameraDevice.a
EXECUTABLE=pop

exec: $(EXECUTABLE) clean cleanExec
.PHONY: exec

lib: $(LIBRARY) clean
.PHONY: lib

$(EXECUTABLE): $(BUILD_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $^ -o $@

 $(LIBRARY): $(OBJECTS)
	ar rcs $@ $(LIB_OBJECTS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	mkdir -p ./libs/$(ARCHITECTURE)/objects/
	mv $(OBJECTS) ./libs/$(ARCHITECTURE)/objects
	mv $(LIBRARY) ./libs/$(ARCHITECTURE)/

cleanExec:
	mv $(EXECUTABLE) ./libs/$(ARCHITECTURE)/