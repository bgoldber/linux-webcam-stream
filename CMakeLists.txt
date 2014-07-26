cmake_minimum_required(VERSION 2.6)
project(linux-webcam-stream)

set(FFMPEG ${PROJECT_SOURCE_DIR}/build/ffmpeg)
set(X264 ${PROJECT_SOURCE_DIR}/build/x264)

message(status, ${FFMPEG})
message(status, ${X264})

include_directories(
  ${FFMPEG}/include
  ${X264}/include
)

link_directories(
  ${FFMPEG}/lib
  ${X264}/lib
)

add_executable(linux-webcam-stream streamMuxer.c
  cameraCapture.c
)

target_link_libraries(linux-webcam-stream
  m
  z
  pthread
  v4l2
  x264
  avformat
  avcodec
  avutil
  swscale
  swresample
)
