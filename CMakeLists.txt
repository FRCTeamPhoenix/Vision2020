cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(OpenCV_STATIC ON)
set(WPILIB_PATH /usr/local/wpilib)
cmake_policy(SET CMP0028 NEW)

#[[
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER /bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /bin/arm-linux-gnueabihf-g++)
]]

project(Vision2020)

include_directories(${WPILIB_PATH}/lib ${WPILIB_PATH}/include ${WPILIB_PATH}/include/cameraserver ${WPILIB_PATH}/include/cscore ${WPILIB_PATH}/include/ntcore ${WPILIB_PATH}/include/wpiutil)

find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(client vision_client.cpp)
add_executable(server vision_server.cpp)

target_link_libraries(client ${OpenCV_LIBS})
target_link_libraries(client ${WPILIB_PATH}/lib/libcameraserver.a ${WPILIB_PATH}/lib/libcscore.a ${WPILIB_PATH}/lib/libntcore.a ${WPILIB_PATH}/lib/libwpiutil.a)
target_link_libraries (client ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(server ${OpenCV_LIBS})
target_link_libraries(server ${WPILIB_PATH}/lib/libcameraserver.a ${WPILIB_PATH}/lib/libcscore.a ${WPILIB_PATH}/lib/libntcore.a ${WPILIB_PATH}/lib/libwpiutil.a)
target_link_libraries (server ${CMAKE_THREAD_LIBS_INIT})
