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

#ifndef _WAVEPLOT_ERROR_HPP_
#define _WAVEPLOT_ERROR_HPP_

#include <cstdint>

enum
{
  ERROR_UNKNOWN = 1,

  ERROR_NO_AUDIO_STREAM,
  ERROR_BAD_SAMPLE_FORMAT,
  ERROR_NO_SAMPLES,

  ERROR_AVFORMAT_OPEN_INPUT,
  ERROR_AVFORMAT_FIND_STREAM_INFO,
  ERROR_AVCODEC_OPEN2
};

void error(uint16_t code);

#endif // _WAVEPLOT_ERROR_HPP_
