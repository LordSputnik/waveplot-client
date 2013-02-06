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

#include "audio.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

array<png_color,4> palette = {{{0x00, 0x00, 0x00}, {0x73, 0x6D, 0xAB}, {0xFF,0xBA, 0x58}, {0xFF,0xFF,0xFF}}}; //Nice C++11 Initialization :) 5 lines of code -> 1.

Audio::Audio(const char* filename)
{
  if(avformat_open_input(&container_, filename, NULL, NULL) < 0)
    return;

  if(av_find_stream_info(container_) < 0)
    return;

  int stream_id = -1;
  for (int i = 0; ((i < container_->nb_streams) && (stream_id < 0)); i++)
  {
    if (container_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO )
      stream_id = i;
  }

  if (stream_id == -1)
    return;

  int64_t duration = container_->streams[stream_id]->duration;
  AVRational time_base = container_->streams[stream_id]->time_base;

  duration_secs_ = static_cast<uint32_t>((duration*time_base.num)/time_base.den);

  AVCodecContext* codec_context = container_->streams[stream_id]->codec;

  num_channels_ = codec_context->channels;
  sample_rate_  = codec_context->sample_rate;
  bit_rate_     = codec_context->bit_rate;

  printf("Channels: %i, Rate: %i, Bit Rate: %i\n",num_channels_,sample_rate_,bit_rate_);
  
  size_t samples_reserve_size = num_channels_*sample_rate_*duration_secs_;

  samples_.reserve(samples_reserve_size); //Attempt to reserve enough estimated storage space for all of the audio samples.

  //Create chunk size independent of sample rate or number of channels - constant for given song time.

  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  std::stringstream sstr;

  sstr << codec->name << "-" << bit_rate_;

  format_id = sstr.str();

  cout << format_id << endl;

  if (!avcodec_open(codec_context, codec) < 0)
    return;

  AVPacket packet;
  int16_t buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE/2];

  while (1)
  {
    int buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;

    // Read one packet into `packet`
    if (av_read_frame(container_, &packet) < 0)
      break;  // End of stream. Done decoding.

    // Decodes from `packet` into the buffer
    if (avcodec_decode_audio3(codec_context, buffer, &buffer_size, &packet) < 1)
      break;  // Error in decoding

    size_t insertion_point = samples_.size();
    samples_.resize(insertion_point+(buffer_size/2),0);

    memcpy(&(samples_[insertion_point]),buffer,buffer_size);
  }

  printf("Samples: %lu\n",samples_.size());
  
  av_close_input_file(container_);

  valid_ = true;
  return;
}

bool Audio::SaveWavePlotImage()
{
  uint32_t samples_per_chunk = num_channels_ * (sample_rate_ / 4);
  size_t image_border_size = sample_weightings.size();

  uint16_t image_width = samples_.size()/samples_per_chunk;

  std::vector<double> sample_sums(image_width,0.0);
  std::vector<double> smoothed_sample_sums(image_width,0.0);

  std::vector<uint8_t> image_values(image_width,0);

  for(size_t i = 0; i != sample_sums.size(); ++i)
  {
    size_t offset = i * samples_per_chunk;
    for(size_t j = 0; j != samples_per_chunk;)
    {
      for(size_t k = 0; k != num_channels_; ++k, ++j)
      {
        sample_sums[i] += sqrt(double(samples_[offset+j]) * double(samples_[offset+j])); //Add the power to the sum.
      }
    }
    //No need to average here - sample is normalized later.
  }

  double max_sample = 0.0;

  for(size_t i = image_border_size-1; i != image_width-image_border_size; ++i)
  {
    smoothed_sample_sums[i] = sample_sums[i] * sample_weightings[0];
    for(size_t j = 1; j != sample_weightings.size(); ++j)
    {
      smoothed_sample_sums[i] += (sample_sums[i+j] + sample_sums[i-j]) * sample_weightings[j];
    }
    max_sample = std::max(smoothed_sample_sums[i],max_sample);
  }

  size_t min_trim_sample = 0;
  size_t max_trim_sample = 0;
  for(size_t i = 0; i != smoothed_sample_sums.size(); ++i)
  {
    smoothed_sample_sums[i] /= max_sample;

    image_values[i] = uint8_t(smoothed_sample_sums[i] * 200.0);
  }

  for(size_t i = 0; i != smoothed_sample_sums.size(); ++i)
  {
    if(smoothed_sample_sums[i] > 0.05)
    {
      min_trim_sample = i;
      break;
    }
  }

  for(size_t i = smoothed_sample_sums.size()-1; i != 0; --i)
  {
    if(smoothed_sample_sums[i] > 0.05)
    {
      max_trim_sample = i;
      break;
    }
  }

  duration_trimmed_secs_ = (max_trim_sample - min_trim_sample + 2) / 4;

  array<png_bytep,HI_RES_IMAGE_HEIGHT> rows;
  size_t centre_row = rows.size()/2;
  png_byte** centre = &rows[centre_row];

  std::generate(rows.begin(), rows.end(), [&] { return new png_byte[image_width]; });

  //Initialize default values for pixels and add max and min trim vertical lines.
  for(png_byte* p : rows)
  {
    std::fill_n(p, image_width, 3);
    p[min_trim_sample] = p[max_trim_sample] = 2;
  }

  for(uint16_t y = 20; y != centre_row; y += 20)
  {
    std::fill_n(rows[centre_row+y], image_width, 2); //Create a horizontal line at centre_row+y
    std::fill_n(rows[centre_row-y], image_width, 2); //Create a horizontal line at centre_row-y
  }

  for(uint16_t x = 0; x != image_width; ++x)
  {
    std::for_each(centre - image_values[x], centre + image_values[x] + 1, [&] (png_byte* p) {p[x] = 1;}); //Create a bar based on the value at this x.
  }

  if(WritePNG(image_width, HI_RES_IMAGE_HEIGHT, rows.data()) == false) //Write the PNG image.
    cout << "Exiting Imager: Couldn't open output image." << endl;

  //Clean up memory.
  for(png_byte* p : rows)
    delete p;
}

bool Audio::WritePNG(uint16_t image_width, uint16_t image_height, png_byte** data)
{
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  png_infop info_ptr = png_create_info_struct(png_ptr);

  png_init_io(png_ptr, stdout);

  png_set_PLTE(png_ptr, info_ptr, palette.data(), PALETTE_SIZE);

  png_set_IHDR(png_ptr, info_ptr, image_width, image_height,
                     PALETTE_NUM_BITS, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_set_rows(png_ptr, info_ptr, data);

  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING, NULL);

  return true;
}

bool Audio::SaveWavePlotLargeThumb()
{
  double samples_per_chunk = double(samples_.size())/MID_RES_IMAGE_WIDTH;
  double error = 0.0;

  cout << samples_per_chunk << endl;

  size_t image_border_size = thumb_sample_weightings.size();

  std::vector<double> sample_sums(MID_RES_IMAGE_WIDTH,0.0);
  std::vector<double> smoothed_sample_sums(MID_RES_IMAGE_WIDTH,0.0);

  std::vector<uint8_t> image_values(MID_RES_IMAGE_WIDTH,0);

  for(size_t i = 0, offset = 0; i != sample_sums.size(); ++i)
  {
    uint32_t samples_per_chunk_i = uint32_t((samples_per_chunk + error)/2)*2;
    error += samples_per_chunk - double(samples_per_chunk_i);
    for(size_t j = 0; j != samples_per_chunk_i;)
    {
      for(size_t k = 0; k != num_channels_; ++k, ++j)
      {
        sample_sums[i] += sqrt(double(samples_[offset+j]) * double(samples_[offset+j])); //Add the power to the sum.
      }
    }
    offset += samples_per_chunk_i;
    //No need to average here - sample is normalized later.
  }

  double max_sample = 0.0;

  for(size_t i = image_border_size-1; i != MID_RES_IMAGE_WIDTH-image_border_size; ++i)
  {
    smoothed_sample_sums[i] = sample_sums[i] * thumb_sample_weightings[0];
    for(size_t j = 1; j != thumb_sample_weightings.size(); ++j)
    {
      smoothed_sample_sums[i] += (sample_sums[i+j] + sample_sums[i-j]) * thumb_sample_weightings[j];
    }
    max_sample = std::max(smoothed_sample_sums[i],max_sample);
  }

  for(size_t i = 0; i != smoothed_sample_sums.size(); ++i)
  {
    smoothed_sample_sums[i] /= max_sample;

    image_values[i] = uint8_t(smoothed_sample_sums[i] * 75.0);
  }

  array<png_bytep,MID_RES_IMAGE_HEIGHT> rows;
  size_t centre_row = rows.size()/2;
  png_byte** centre = &rows[centre_row];

  std::generate(rows.begin(), rows.end(), [&] { return new png_byte[MID_RES_IMAGE_WIDTH]; });

  //Initialize default values for pixels and add max and min trim vertical lines.
  for(png_byte* p : rows)
    std::fill_n(p, MID_RES_IMAGE_WIDTH, 3);

  for(uint16_t x = 0; x != MID_RES_IMAGE_WIDTH; ++x)
    std::for_each(centre - image_values[x], centre + image_values[x] + 1, [&] (png_byte* p) {p[x] = 1;}); //Create a bar based on the value at this x.

  if(WritePNG(MID_RES_IMAGE_WIDTH, MID_RES_IMAGE_HEIGHT, rows.data()) == false) //Write the PNG image.
    cout << "Exiting Imager: Couldn't open output image." << endl;

  //Clean up memory.
  for(png_byte* p : rows)
    delete p;
}

bool Audio::SaveWavePlotSmallThumb()
{
  double samples_per_chunk = double(samples_.size())/LOW_RES_IMAGE_WIDTH;
  double error = 0.0;

  cout << samples_per_chunk << endl;

  size_t image_border_size = sample_weightings.size();

  std::vector<double> sample_sums(LOW_RES_IMAGE_WIDTH,0.0);
  std::vector<double> smoothed_sample_sums(LOW_RES_IMAGE_WIDTH,0.0);

  std::vector<uint8_t> image_values(LOW_RES_IMAGE_WIDTH,0);

  for(size_t i = 0, offset = 0; i != sample_sums.size(); ++i)
  {
    uint32_t samples_per_chunk_i = uint32_t((samples_per_chunk + error)/2)*2;
    error += samples_per_chunk - double(samples_per_chunk_i);
    for(size_t j = 0; j != samples_per_chunk_i;)
    {
      for(size_t k = 0; k != num_channels_; ++k, ++j)
      {
        sample_sums[i] += sqrt(double(samples_[offset+j]) * double(samples_[offset+j])); //Add the power to the sum.
      }
    }
    offset += samples_per_chunk_i;
    //No need to average here - sample is normalized later.
  }

  double max_sample = 0.0;

  for(size_t i = 0; i != LOW_RES_IMAGE_WIDTH; ++i)
  {
    smoothed_sample_sums[i] = sample_sums[i];
    max_sample = std::max(smoothed_sample_sums[i],max_sample);
  }

  for(size_t i = 0; i != smoothed_sample_sums.size(); ++i)
  {
    smoothed_sample_sums[i] /= max_sample;

    image_values[i] = uint8_t(smoothed_sample_sums[i] * 10.0);
  }

  array<png_bytep,LOW_RES_IMAGE_HEIGHT> rows;
  size_t centre_row = rows.size()/2;
  png_byte** centre = &rows[centre_row];

  std::generate(rows.begin(), rows.end(), [&] { return new png_byte[LOW_RES_IMAGE_WIDTH]; });

  //Initialize default values for pixels and add max and min trim vertical lines.
  for(png_byte* p : rows)
    std::fill_n(p, LOW_RES_IMAGE_WIDTH, 3);

  for(uint16_t x = 0; x != LOW_RES_IMAGE_WIDTH; ++x)
    std::for_each(centre - image_values[x], centre + image_values[x] + 1, [&] (png_byte* p) {p[x] = 1;}); //Create a bar based on the value at this x.

  if(WritePNG(LOW_RES_IMAGE_WIDTH, LOW_RES_IMAGE_HEIGHT, rows.data()) == false) //Write the PNG image.
    cout << "Exiting Imager: Couldn't open output image." << endl;

  //Clean up memory.
  for(png_byte* p : rows)
    delete p;
}

bool Audio::SaveWavePlotInfo()
{
  //CalculateMixHash();
  
  fprintf(stdout,"%u|%u|%s|%u",duration_secs_,duration_trimmed_secs_,format_id.c_str(),num_channels_);

}

/*bool CalculateMixHash()
{
  double samples_per_chunk_thumb = double(samples_.size())/400.0;
}*/
