#!/bin/bash

usage()
{
    echo "USAGE: [-U] [-CK] [-A] [-p] [-o] [-u] [-v VERSION_NAME]  "
    echo "No ARGS means use default build option                  "
    echo "WHERE: -U = build uboot                                 "
    echo "       -C = build kernel with Clang                     "
    echo "       -K = build kernel                                "
    echo "       -A = build android                               "
    echo "       -p = will build packaging in IMAGE      "
    echo "       -o = build OTA package                           "
    echo "       -u = build update.img                            "
    echo "       -v = build android with 'user' or 'userdebug'    "
    echo "       -d = huild kernel dts name    "
    echo "       -V = build version    "
    echo "       -J = build jobs    "
    exit 1
}

function clean_cmake_config()
{
    rm -rf CMakeCache.txt
    rm -rf CMakeFiles
    rm -rf cmake_install.cmake
    rm -rf Makefile
    rm -rf CTestTestfile.cmake
}

format_code()
{
    find . -name "*.c" -o -name "*.h" | xargs clang-format -style=WebKit -i
    find . -name "*.h" | xargs clang-format -style=LLVM -i
    find . -name "*.cpp" | xargs clang-format -style=LLVM -i
}

if [ ! -d "output/release" ]; then
    mkdir -p "output/release"
fi

BUILD_CLEAN=false
BUILD_ARCH=arm

# check pass argument
while getopts "RC" arg
do
    case $arg in
        R)
            echo "will reconfigure project"
            BUILD_CLEAN=true
            ;;
        C)
            echo "will build cross platform"
            BUILD_CROSS=arm
            ;;
        ?)
            usage ;;
    esac
done

# build clean
if [ "$BUILD_CLEAN" = true ] ; then
    rm output/* -rf
fi

cd output

# 根据你的SDK的路径替换这里的路径在SDK里find -name toolchainfile.cmake 找到这个文件的路径，然后放在=后面

# build cross
if [ "$BUILD_ARCH" = "arm" ] ; then
    echo "start build cross paltform"
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchainfile-rv1106.cmake -DCMAKE_INSTALL_PREFIX=./release ../
elif [ "$BUILD_ARCH" = "arm64" ] ; then
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchainfile-rk3588.cmake -DCMAKE_INSTALL_PREFIX=./release ../
else
    cmake -DCMAKE_INSTALL_PREFIX=./release ../
fi

make -j8

if [ $? -eq 0  ]; then
    echo Build finished!
    # make install
    # clean_cmake_config
    cd ..
fi
