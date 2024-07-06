# RknnApiConfig.cmake

# Configuration to find and use RknnApi

# Set the include directories for RknnApi
set(RknnApi_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include")

# Set the library directories for RknnApi
set(RknnApi_LIBRARY_DIRS "${CMAKE_CURRENT_LIST_DIR}/../armhf")

# Set the RknnApi libraries
set(RknnApi_LIBRARIES "rknn_api")

# Provide the RknnApi targets
add_library(RknnApi INTERFACE)
target_include_directories(RknnApi INTERFACE ${RknnApi_INCLUDE_DIRS})
target_link_libraries(RknnApi INTERFACE ${RknnApi_LIBRARIES})

# Provide information to the user
message(STATUS "Found RknnApi: ${RknnApi_LIBRARIES}")

