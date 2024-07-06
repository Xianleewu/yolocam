cmake_minimum_required(VERSION 2.9)

add_compile_options(-fPIC -Wall -Wno-error -O2)
add_compile_options(-Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-misleading-indentation
    -Wno-unused-label
    -Wno-format-truncation
    -Wall
    )
set(CMAKE_CXX_STANDARD 11)

PROJECT(yolocam)

set(OpenCV_DIR ${CMAKE_CURRENT_LIST_DIR}/packages/opencv/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)

set(RknnApi_DIR ${CMAKE_CURRENT_LIST_DIR}/packages/rknn_api/cmake)
find_package(RknnApi REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SYSROOT}/usr/include/libdrm/
    ${CMAKE_CURRENT_LIST_DIR}/src/allocator
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${RknnApi_INCLUDE_DIRS}
    )

SET(ALLOCATOR_SRCS
    src/allocator/dma_alloc.cpp)

SET(UTILS_SRCS
    src/serial_comm.c
    src/image_pkt.c
    src/ptr_queue.c
    src/v4l2_device.c)

link_directories(${CMAKE_SOURCE_DIR}/lib)


ADD_EXECUTABLE(yolocam main.cpp src/rkdrm_display.c src/rknn_runner.cpp src/postprocess.cc ${ALLOCATOR_SRCS} ${UTILS_SRCS})
target_link_libraries(yolocam
    ${OpenCV_LIBS}
    ${RknnApi_LIBS}
    rga
    drm
    rknnmrt
    pthread)
