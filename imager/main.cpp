/*
 * Copyright 2013 Ben Ockmore
 *
 * This file is part of WavePlot Imager.

 * WavePlot Imager is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * WavePlot Imager is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with WavePlot Imager. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>

extern "C"
{
  #ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
  #endif

  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/avutil.h>

  #ifdef WIN32
    #include <fcntl.h>
    #include <io.h>
  #endif
}

#include "audio.hpp"

using namespace std;

//#define STANDALONE

void die(const char* message)
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}

const char version[] = "CITRUS";

#ifndef STANDALONE
int main(int argc, char* argv[])
{
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
int main()
{
  char input_filename[] = "test.flac";
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

  Audio::Process(input_filename);

  return 0;
}
