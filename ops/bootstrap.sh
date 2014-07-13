#! /bin/bash

# Ensure we're running on Linux
OS_NAME=`uname`
if [ "$OS_NAME" != "Linux" ]; then
  echo "This software is only compatible with Linux due usage of Video4Linux."
  exit 1
fi

# Check for Debian-based Linux with aptitude.
# TODO: Support RHEL-based Linux distributions
if !(hash apt-get 2> /dev/null); then
  echo "This installation requires Debian based Linux with aptitude"
  exit 1
fi

# Install build dependencies
sudo apt-get -y install autoconf automake build-essential pkg-config libv4l-0 \
    libv4l-dev yasm

######################################
# Create a build directory
######################################
mkdir -p build
BASE_DIR=`pwd`
BUILD_DIR=$BASE_DIR/build
echo "Build directory is set to:" $BUILD_DIR

#######################################
# Install x264
#######################################
mkdir -p $BUILD_DIR/x264 && cd $BASE_DIR/x264
./configure --enable-shared --prefix=$BUILD_DIR/x264
make && make install

#######################################
# Install ffmpeg
#######################################
mkdir -p $BUILD_DIR/ffmpeg && cd $BASE_DIR/ffmpeg
./configure --prefix=$BUILD_DIR/ffmpeg --enable-libv4l2 --enable-libx264 \
  --enable-gpl --extra-ldflags="-L$BUILD_DIR/x264/lib" \
  --extra-cflags="-I$BUILD_DIR/x264/include"
make && make install

echo "x264 and ffmpeg have now been built and installed to" $BUILD_DIR
echo "Please note that this installation did not install to global directories"
echo "  such as /usr/local/* or /usr/*. It is meant to run locally as a set of"
echo "  specific dependencies to this software."
