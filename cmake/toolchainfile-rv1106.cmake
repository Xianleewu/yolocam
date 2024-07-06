SET(CMAKE_C_COMPILER /home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc)
SET(CMAKE_CXX_COMPILER /home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-g++)

SET(CMAKE_FIND_ROOT_PATH /home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/arm-rockchip830-linux-uclibcgnueabihf/sysroot)

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Additional flags or settings
SET(CMAKE_C_FLAGS "--sysroot=/home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/arm-rockchip830-linux-uclibcgnueabihf/sysroot -Wall")
SET(CMAKE_CXX_FLAGS "--sysroot=/home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/arm-rockchip830-linux-uclibcgnueabihf/sysroot -Wall")

# Include directories
INCLUDE_DIRECTORIES(/home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/include)

# Library directories
LINK_DIRECTORIES(/home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/lib)
LINK_DIRECTORIES(/home/xianlee/workspace/hinlink/solo-linker/sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/runtime_lib)
