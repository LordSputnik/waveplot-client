/*
 * Copyright 2013 Ben Ockmore
 *
 * This file is part of WavePlot.

 * WavePlot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * WavePlot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with WavePlot. If not, see <http://www.gnu.org/licenses/>.
 */

using namespace std;

#include <cstdio>
#include <cmath>

#include <iostream>
#include <vector>
#include <string>
#include <array>

extern "C"
{
  #ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
  #endif

  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/avutil.h>

  #define PNG_DEBUG 3
  #include <png.h>
}

#include "Audio.hpp"

void die(const char* message)
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}

array<double,4> sample_weightings {10.0,8.0,5.0,3.0}; //Symmetrical... eg. {10,7} => 7,10,7

const char* version = "BANNANA";

int main(int argc, char* argv[])
{
//#define STANDALONE
#ifndef STANDALONE
  if(argc != 7)
  {
    printf("Exiting Imager: Wrong number of program parameters: %i\n",argc);
    return 1;
  }

  if(strcmp(version,argv[6]) != 0)
  {
    printf("Exiting Imager: Scanner-Imager version mismatch! (Scanner says: %s, Imager says: %s)\n",argv[3],version);
    return 1;
  }

  char* input_filename = argv[1];
  char* image_filename = argv[2];
  char* large_thumb_filename = argv[3];
  char* small_thumb_filename = argv[4];
  char* info_filename = argv[5];
#else
  char* input_filename = "test-break.flac";
  char* image_filename = "output-hi.png";
  char* large_thumb_filename = "output-mid.png";
  char* small_thumb_filename = "output-low.png";
  char* info_filename = "info";
#endif


  // This call is necessarily done once in your app to initialize
  // libavformat to register all the muxers, demuxers and protocols.
  av_register_all();

  // A media container
  Audio my_song(input_filename);

  my_song.SaveWavePlotImage(image_filename);
  my_song.SaveWavePlotLargeThumb(large_thumb_filename);
  my_song.SaveWavePlotSmallThumb(small_thumb_filename);
  my_song.SaveWavePlotInfo(info_filename);

  return 0;
}
