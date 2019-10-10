rm -rf build && mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make