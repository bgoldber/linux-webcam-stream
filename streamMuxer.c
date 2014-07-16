/*****************************************************************************
 * streamMuxer.c: Wrapper around ffmpeg streaming driver
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

#include "streamMuxer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>


////////////////////////////////////////////////////////////////////////////////
///
/// @fn initializeStreamer(void)
///
///  Configures streaming module before streaming begins
///
/// @return 0 on success, -1 otherwise
///
////////////////////////////////////////////////////////////////////////////////
int initializeStreamer(void) {
  AVFormatContext *outputContext = NULL;
  AVOutputFormat *outputFormat = NULL;
  AVStream *audioStream, *videoStream = NULL;
  AVCodec *audioCodec, *videoCodec = NULL;

  // Register all ffmpeg audio/video formats
  av_register_all();

  // TODO: Configure/accept ingestion buffers for video and audio frames
  avformat_alloc_output_context2(&outputContext, NULL, "hls", NULL);
  if (outputContext == NULL) {
    perror("Failed to allocate avformat output context");
  }

  // Set the output format
  outputFormat = outputContext->oformat;

  if (outputFormat->video_codec != AV_CODEC_ID_NONE) {
    videoStream = add_stream(
        outputContext, &videoCodec, outputFormat->video_codec);
  }

  if (outputFormat->audio_codec != AV_CODEC_ID_NONE) {
    audioStream = add_stream(
        outputContext, &audioCodec, outputFormat->audio_codec);
  }

  return 0;
}