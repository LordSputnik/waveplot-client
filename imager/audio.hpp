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

#ifndef _WAVEPLOT_AUDIO_HPP_
#define _WAVEPLOT_AUDIO_HPP_

#include <string>
#include <cstdint>
#include <vector>
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

#define HI_RES_IMAGE_HEIGHT 401

#define MID_RES_IMAGE_WIDTH 400
#define MID_RES_IMAGE_HEIGHT 151

#define LOW_RES_IMAGE_WIDTH 50
#define LOW_RES_IMAGE_HEIGHT 21

#define PALETTE_SIZE 4
#define PALETTE_NUM_BITS 2

class Audio
{
  Audio(const Audio & other){};
  Audio & operator = (const Audio & other){ return *this; };

  bool valid_ = false;

  int sample_rate_;
  int bit_rate_;

  std::string format_id;

  const std::array<double,4> sample_weightings = {{10.0,8.0,5.0,3.0}};
  const std::array<double,2> thumb_sample_weightings = {{5.0,3.0}};

  bool CalculateMixHash();

  bool WritePNG(uint16_t image_width, uint16_t image_height, png_byte** data);

  public:
    uint32_t duration_secs_;
    uint32_t duration_trimmed_secs_;
    int num_channels_;
    int samples_per_chunk_;
    std::vector<int16_t> samples_;
    Audio(const char* filename);

    bool SaveWavePlotImage();
    bool SaveWavePlotLargeThumb();
    bool SaveWavePlotSmallThumb();
    bool SaveWavePlotInfo();
};

#endif // _WAVEPLOT_AUDIO_HPP_

