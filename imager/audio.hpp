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

#ifndef _WAVEPLOT_AUDIO_HPP_
#define _WAVEPLOT_AUDIO_HPP_

extern "C"
{
  #ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
  #endif

  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/avutil.h>
  #include <libavutil/channel_layout.h>
}

namespace Audio
{
  void Process(const char* filename);
}

#endif // _WAVEPLOT_AUDIO_HPP_

