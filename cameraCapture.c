/*****************************************************************************
 * cameraCapture.c: Wrapper around Video4Linux2 Webcam Driver
 *****************************************************************************
 * Copyright (C) 2014 linux-webcam-stream project
 *
 * Authors: Benjamin Goldberg <benjamin.goldberg@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>

#define FALSE 0
#define TRUE 1

#define FORMAT_MJPEG "mjpeg"
#define FORMAT_H264 "h264"

struct frameBuffer {
  void   *data;
  size_t  length;
};

static int terminate = FALSE;
static unsigned int bufferCount = 0;
static struct frameBuffer *frameBuffers;

#define CLEAR(x) memset(&(x), 0, sizeof(x))

////////////////////////////////////////////////////////////////////////////////
///
/// @fn int captureFrames(int fileDescriptor)
///
///  Creates memory buffers for receiving frames from the video capture device
///  and then enters a frame-capture loop that runs until the terminate flag is
///  toggled.
///
/// @param fileDescriptor The open file descriptor to the camera device
///
/// @return 0 on success, -1 otherwise.
///
////////////////////////////////////////////////////////////////////////////////
int captureFrames(int fileDescriptor) {
  struct v4l2_buffer deviceBuffer;
  struct v4l2_buffer readBuffer;
  struct v4l2_buffer queryBuffer;
  fd_set fileDescriptorSet;
  struct timeval time;
  int ready = -1;
  int i = 0;

  CLEAR(time);

  for(i = 0; i < bufferCount; i++) {
    CLEAR(deviceBuffer);
    CLEAR(queryBuffer);

    deviceBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    deviceBuffer.memory = V4L2_MEMORY_MMAP;
    deviceBuffer.index = i;

    // Query the device memory buffer for settings
    if (xioctl(fileDescriptor, VIDIOC_QUERYBUF, &deviceBuffer) == -1) {
      perror("Error querying the device memory buffer");
      return -1;
    }

    // Generate memory-mapped buffer to device memory
    frameBuffers[i].length = deviceBuffer.length;
    frameBuffers[i].data = mmap(
        NULL, deviceBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
        fileDescriptor, deviceBuffer.m.offset);

    if (frameBuffers[i].data == MAP_FAILED) {
      perror("Error establishing memory map");
    }

    queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queryBuffer.memory = V4L2_MEMORY_MMAP;
    queryBuffer.index = i;
    if(xioctl(fileDescriptor, VIDIOC_QBUF, &queryBuffer) == -1) {
      perror("Error establishing device query buffer");
      return -1;
    }
  }

  // Turn video streaming on
  if (xioctl(fileDescriptor, VIDIOC_STREAMON, &queryBuffer.type) == -1) {
    perror("Error starting the device video stream");
    return -1;
  }

  // Begin reading from the device
  while(!terminate) {
    // Initiailize the file descriptor set
    FD_ZERO(&fileDescriptorSet);
    FD_SET(fileDescriptor, &fileDescriptorSet);

    // Set select timeout to be one second
    time.tv_sec = 2;
    time.tv_usec = 0;

    // Wait for signal indicating the device has delivered a frame
    ready = select(fileDescriptor + 1, &fileDescriptorSet, NULL, NULL, &time);
    if (ready == -1) {
      perror("Error waiting on video frame");
      continue;
    }

    // Capture the frame
    CLEAR(readBuffer);
    readBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    readBuffer.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fileDescriptor, VIDIOC_DQBUF, &readBuffer) == -1) {
      perror("Failed to retrieve frame from device");
    } else {
      fprintf(stderr, ".");
      fflush(stdout);
    }

    if (xioctl(fileDescriptor, VIDIOC_QBUF, &readBuffer) == -1) {
      perror("Error queueing the video buffer");
    }
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn int configureDevice(int fileDescriptor, char format[], int width,
///            int height)
///
///  Configures the video device to capture with the specified parameters. If
///  those parameters can't be satisfied or an error occurs during setup, the
///  issue is returned to the user.
///
/// @param fileDescriptor The open file descriptor to the camera device
/// @param format The name of the desired capture video format, eg 'h264'
/// @param width Desired image width in pixels
/// @param height Desired image height in pixels
///
/// @return 0 on success, -1 otherwise.
///
////////////////////////////////////////////////////////////////////////////////
int configureDevice(int fileDescriptor, char format[], int width, int height) {
  struct v4l2_format cameraFormat;
  struct v4l2_requestbuffers requestBuffers;
  int i = 0;

  CLEAR(cameraFormat);
  CLEAR(requestBuffers);

  // Configure basic properties for the video capture stream
  cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cameraFormat.fmt.pix.width = width;
  cameraFormat.fmt.pix.height = height;

  // Convert the format string to lower case
  for (i = 2; i < strlen(format); i++) {
    format[i] = tolower(format[i]);
  }

  // Map the user provided format against a supported V4L2 format
  if (strcmp(format, FORMAT_H264) == 0) {
    cameraFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    cameraFormat.fmt.pix.field = V4L2_FIELD_INTERLACED;
  } else if (strcmp(format, FORMAT_MJPEG) == 0) {
    cameraFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    cameraFormat.fmt.pix.field = V4L2_FIELD_NONE;
  } else {
    fprintf(stderr, "Pixel format %s is unsupported.", format);
    return -1;
  }

  // Set the properties on the device
  if (xioctl(fileDescriptor, VIDIOC_S_FMT, &cameraFormat) == -1) {
    fprintf(stderr, "\nError setting video properties on the device:\n"
        "Video Format: %s\n"
        "Video Size: %i x %i\n",
        format, width, height);
    return -1;
  }

  // Configure device memory buffers using memory mapping
  requestBuffers.count = 4;
  requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  requestBuffers.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fileDescriptor, VIDIOC_REQBUFS, &requestBuffers) == -1) {
    perror("Error requesting device memory buffer");
    return -1;
  }

  bufferCount = requestBuffers.count;
  frameBuffers = calloc(bufferCount, sizeof(*frameBuffers));

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn int openDevice(char videoDevice[])
///
///  Opens the specified video device for reading and returns the file
///  descriptor to the caller.
///
/// @param videoDevice The name of the video device, eg '/dev/video0'
///
/// @return fileDescriptor The positive integer file descriptor on success. On
///   failure -1 is returned.
///
////////////////////////////////////////////////////////////////////////////////
int openDevice(char videoDevice[]) {
  int fileDescriptor = -1;
  struct v4l2_capability deviceCapabilities;

  CLEAR(deviceCapabilities);

  // Open the video device
  fileDescriptor = open(videoDevice, O_RDWR | O_NONBLOCK);
  if (fileDescriptor == -1) {
    perror("Failed to open the video device");
    return -1;
  }

  // Populate the device capabilities by querying the V4L2 Driver
  if (xioctl(fileDescriptor, VIDIOC_QUERYCAP, &deviceCapabilities) == -1) {
    perror("Failed to retrieve device parameters");
    return -1;
  }

  return fileDescriptor;
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn void printHelp(void)
///
///  Prints the usage help for the program.
///
////////////////////////////////////////////////////////////////////////////////
void printHelp(void) {
  fprintf(stdout,
      "\nOptions:\n"
      "-d / --device Where the video camera is mounted. Typically /dev/videoN.\n"
      "-h / --help   Display this help message.\n"
      "-f / --format Video format to display. Use -a to see formats available.\n"
      "-s / --frame-size Size of video frame. Use -a to see sizes available.\n"
      "-a / --available Prints available formats and sizes and then exits.\n");
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn void signalHandler(int signalNumber)
///
///  Callback function for when a system signal is received, eg 'SIGINT'
///
/// @param signalNumber Number representing the signal received
///
////////////////////////////////////////////////////////////////////////////////
void signalHandler(int signalNumber) {
  switch (signalNumber) {
    case SIGINT:
      terminate = TRUE;
      break;

    default:
      fprintf(stdout, "\nUnhandled signal %d was received.\n", signalNumber);
      break;
  }
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn shutdownDevice(int fileDescriptor)
///
///  Shuts down the capture device and cleans up buffers.
///
/// @param fileDescriptor The open file descriptor to the camera device
///
/// @return 0 on success, -1 otherwise.
///
////////////////////////////////////////////////////////////////////////////////
int shutdownDevice(int fileDescriptor) {
  enum v4l2_buf_type bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int i = 0;

  if(xioctl(fileDescriptor, VIDIOC_STREAMOFF, &bufferType) == -1) {
    perror("Error closing video stream");
    return -1;
  }

  for (i = 0; i < bufferCount; i++) {
    if (-1 == munmap(frameBuffers[i].data, frameBuffers[i].length)) {
      perror("Error unmapping memory buffers");
    }
  }

  free(frameBuffers);

  if(close(fileDescriptor) == -1) {
    perror("Error closing file descriptor to device.");
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn xioctl(int fileDescriptor, int request, void *arg)
///
///  Wrapper function for ioctl invocation to the camera device.
///
/// @param fileDescriptor File descriptor
/// @param request The request to be made against the camera device. See the
///                documentation for v4l2 for parameters.
/// @param arg Pointer to additional ioctl arguments
///
/// @return 0 on success
///
////////////////////////////////////////////////////////////////////////////////
int xioctl(int fileDescriptor, int request, void *arg) {
  int r;
  do r = ioctl (fileDescriptor, request, arg);
  while (-1 == r && EINTR == errno);
  return r;
}


////////////////////////////////////////////////////////////////////////////////
///
/// @fn main(int argc, char **argv)
///
///  Main entry point for program execution
///
/// @param argc Count of input arguments
/// @param argv Pointer to array of argument arrays
///
/// @return 0 on success
///
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
  // Video parameters
  char videoDevice[] = "/dev/video0";
  int frameWidth = 320;
  int frameHeight = 240;
  float frameRate = 30.0f;
  char format[] = FORMAT_H264;

  // Configure getopt parameters
  int arg = 0;
  int argumentIndex = 0;
  const char abbreviations[] = "dhfs";
  const struct option full[] = {
    {"device", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"format", no_argument, NULL, 'f'},
    {"frame-size", no_argument, NULL, 's'},
    {"available", no_argument, NULL, 'a'}
  };

  // Register signal handler for interrupts
  signal(SIGINT, signalHandler);

  for(;;) {
    arg = getopt_long(argc, argv, abbreviations, full, &argumentIndex);

    // Getopt returns -1 when there are no more arguments left to parse
    if (arg == -1) {
      break;
    }

    switch(arg) {
      case 'd':
        printf("Using device %s\n", optarg);
        break;

      case 'h':
        printHelp();
        break;

      case 'f':
        printf("Using format %s\n", optarg);
        break;

      case 's':
        printf("Using frame size %s\n", optarg);
        break;

      case 'a':
        printf("Available video formats\n");
        printf("Available video sizes\n");
        return 0;

      default:
        break;
    }
  }

  // Begin camera capture
  fprintf(stdout, "Initializing camera capture.\n"
      "Capture Device: %s\n"
      "Video Format: %s\n"
      "Video Size: %i x %i\n"
      "Frame Rate: %0.2f\n",
      videoDevice, format, frameWidth, frameHeight, frameRate);

  // Step 1: Open the device for capture and test for its existence
  int fileDescriptor = openDevice(videoDevice);
  if (fileDescriptor == -1) {
    return EXIT_FAILURE;
  }

  // Step 2: Configure device image format
  if (configureDevice(fileDescriptor, format, frameWidth, frameHeight) == -1 ) {
    return EXIT_FAILURE;
  }

  // Step 3: Capture frames from the device until SIGINT is received
  fprintf(stdout, "\nBeginning frame capture. Press `ctrl+c` to exit.\n\n");
  if (captureFrames(fileDescriptor) == -1) {
    return EXIT_FAILURE;
  }

  // Step 4: Cleanup and exit
  if (shutdownDevice(fileDescriptor) == -1) {
    return EXIT_FAILURE;
  }

  fprintf(stdout, "Successfully Exiting.\n");
  return EXIT_SUCCESS;
}
