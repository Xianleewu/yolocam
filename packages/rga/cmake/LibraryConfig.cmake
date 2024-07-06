# LibraryConfig.cmake

# Configuration to find and use libraries

set(LIBRARY_INCLUDE_DIRS "rga/include")

set(LIBRARY_LIBRARY_DIRS "rga/armhf")

set(LIBRARY_LIBRARIES librga;librga)

add_library(MyLibrary INTERFACE)
target_include_directories(MyLibrary INTERFACE ${LIBRARY_INCLUDE_DIRS})
target_link_libraries(MyLibrary INTERFACE ${LIBRARY_LIBRARY_DIRS}/librga)

message(STATUS "Found libraries: ${LIBRARY_LIBRARIES}")
