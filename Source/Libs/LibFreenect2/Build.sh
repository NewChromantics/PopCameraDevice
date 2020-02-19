brew install libusb
brew install cmake
cd $1
mkdir Build
cd Build
cmake ../LibFreenect2x -DENABLE_OPENCL:BOOL=OFF -DBUILD_OPENNI2_DRIVER:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_OPENGL:BOOL=OFF
make
