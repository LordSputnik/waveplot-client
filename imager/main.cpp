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

  #ifdef WIN32
    #include <fcntl.h>
    #include <io.h>
  #endif
}

#include "audio.hpp"

//#define STANDALONE

void die(const char* message)
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}

array<double,4> sample_weightings {10.0,8.0,5.0,3.0}; //Symmetrical... eg. {10,7} => 7,10,7

const char* version = "BANNANA";

int main(int argc, char* argv[])
{
#ifndef STANDALONE
  if(argc != 3)
  {
    fprintf(stderr,"Exiting Imager: Wrong number of program parameters: %i (Required 3)\n",argc);
    return 1;
  }

  if(strcmp(version,argv[2]) != 0)
  {
    fprintf(stderr,"Exiting Imager: Scanner-Imager version mismatch! (Scanner says: %s, Imager says: %s)\n",argv[3],version);
    return 1;
  }

  char* input_filename = argv[1];
#else
  char* input_filename = "test.flac";
#endif

#ifdef WIN32
  fprintf(stderr,"Setting stdout to binary mode!");
  if(_setmode(1, O_BINARY) == -1)
  {
    puts("Failed to do so.");
    return 10;
  }
#endif

  // This call is necessarily done once in your app to initialize
  // libavformat to register all the muxers, demuxers and protocols.
  av_register_all();

  // A media container
  Audio my_song(input_filename);

  fputs("WAVEPLOT_START",stdout);
  my_song.SaveWavePlotImage();
  fputs("WAVEPLOT_LARGE_THUMB",stdout);
  my_song.SaveWavePlotLargeThumb();
  fputs("WAVEPLOT_SMALL_THUMB",stdout);
  my_song.SaveWavePlotSmallThumb();
  fputs("WAVEPLOT_INFO",stdout);
  my_song.SaveWavePlotInfo();
  fputs("WAVEPLOT_END",stdout);
  return 0;
}
