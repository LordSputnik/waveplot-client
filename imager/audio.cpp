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
#include <limits>
#include <list>

#include "error.hpp"

//#define DEBUG

#pragma GCC diagnostic ignored "-Wswitch"

using namespace std;

namespace Audio
{
  namespace
  {
    uint32_t duration_secs_;
    uint32_t duration_secs_trimmed_;

    uint8_t num_channels_;

    uint16_t bit_rate_;
    uint32_t sample_rate_;
    AVSampleFormat sample_format_;

    list<double> waveplot_chunks_;
    list<double> preview_chunks_;

    vector< list<double> > dr_channel_peak_;
    vector< list<double> > dr_channel_rms_;
    vector< size_t > dr_num_processed_samples;

    static array<png_color,4> palette = {{{0x00, 0x00, 0x00}, {0x73, 0x6D, 0xAB}, {0xFF,0xBA, 0x58}, {0xFF,0xFF,0xFF}}}; //Nice C++11 Initialization :) 5 lines of code -> 1.

    string format_id_;

    static const array<double,4> sample_weightings_ = {{10.0,8.0,5.0,3.0}};

    bool WritePNG(uint16_t image_width, uint16_t image_height, png_byte** data)
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

    double ProcessNormalizedSampleForDR(double sample, uint8_t channel)
    {
      static const size_t num_samples_per_chunk = sample_rate_ * 3;

      if(dr_num_processed_samples[channel] == num_samples_per_chunk)
      {
        dr_channel_rms_[channel].back() = sqrt((2.0 * dr_channel_rms_[channel].back()) / num_samples_per_chunk);

        dr_channel_peak_[channel].push_back(0.0);
        dr_channel_rms_[channel].push_back(0.0);

        dr_num_processed_samples[channel] = 0;
      }

      dr_channel_peak_[channel].back() = max(dr_channel_peak_[channel].back(),fabs(sample));
      dr_channel_rms_[channel].back() += sample*sample;
      ++dr_num_processed_samples[channel];
    }

    double ProcessPlanarBufferData(std::vector<double> & dest, uint8_t* src)
    {
      return 0.0;
    }

    double ProcessInterlacedBufferData(std::vector<double> & dest, uint8_t* src)
    {
      if((sample_format_ > AV_SAMPLE_FMT_DBL) | (sample_format_ < AV_SAMPLE_FMT_U8))
        error(ERROR_BAD_SAMPLE_FORMAT);

      //Various possible formats.
      uint8_t* src_u8 = src;
      int16_t* src_s16 = reinterpret_cast<int16_t*>(src);
      int32_t* src_s32 = reinterpret_cast<int32_t*>(src);
      float* src_f = reinterpret_cast<float*>(src);
      double* src_d = reinterpret_cast<double*>(src);

      double channel_delta = 0.0;

      for(size_t i = 0; i != dest.size(); ++i)
      {
        size_t offset = i*num_channels_;

        if(num_channels_ == 2)
        {
          switch(sample_format_)
          {
            case AV_SAMPLE_FMT_U8:
              channel_delta += static_cast<double>(abs(src_u8[offset] - src_u8[offset+1]));
              break;
            case AV_SAMPLE_FMT_S16:
              channel_delta += static_cast<double>(abs(src_s16[offset] - src_s16[offset+1]));
              break;
            case AV_SAMPLE_FMT_S32:
              channel_delta += static_cast<double>(abs(src_s32[offset] - src_s32[offset+1]));
              break;
            case AV_SAMPLE_FMT_FLT:
              channel_delta += static_cast<double>(fabs(src_f[offset] - src_f[offset+1]));
              break;
            case AV_SAMPLE_FMT_DBL:
              channel_delta += static_cast<double>(fabs(src_d[offset] - src_d[offset+1]));
              break;
          }
        }

        for(size_t channel = 0; channel != num_channels_; ++channel)
        {
//          dr_channel_peaks_[channel];
          switch(sample_format_)
          {
            case AV_SAMPLE_FMT_U8:

              break;
            case AV_SAMPLE_FMT_S16:
              dest[i] += static_cast<double>(abs(src_s16[(i*num_channels_) + channel]));
              ProcessNormalizedSampleForDR(static_cast<double>(src_s16[(i*num_channels_) + channel]) / 32768.0, channel);
              break;
            case AV_SAMPLE_FMT_S32:
              dest[i] += static_cast<double>(abs(src_s32[(i*num_channels_) + channel]));
              ProcessNormalizedSampleForDR(static_cast<double>(src_s32[(i*num_channels_) + channel]) / 2147483648.0, channel);
              break;
            case AV_SAMPLE_FMT_FLT:
              dest[i] += static_cast<double>(fabs(src_f[(i*num_channels_) + channel]));
              ProcessNormalizedSampleForDR(static_cast<double>(src_f[(i*num_channels_) + channel]), channel);
              break;
            case AV_SAMPLE_FMT_DBL:
              dest[i] += fabs(src_d[(i*num_channels_) + channel]);
              ProcessNormalizedSampleForDR(src_d[(i*num_channels_) + channel], channel);
              break;
          }
        }
      }

      switch(sample_format_)
      {
        case AV_SAMPLE_FMT_U8:
          channel_delta /= std::numeric_limits<uint8_t>::max(); //Max diff = UCHAR_MAX
          break;
        case AV_SAMPLE_FMT_S16:
          channel_delta /= std::numeric_limits<uint16_t>::max(); //Max diff = USHRT_MAX
          break;
        case AV_SAMPLE_FMT_S32:
          channel_delta /= std::numeric_limits<uint32_t>::max(); //Max diff = UINT_MAX
          break;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_DBL:
          channel_delta /= 2.0f; //Max diff = 1 - -1 = 2
          break;
      }

      return channel_delta;
    }

    void AddSamplesWavePlot(const std::vector<double> & new_samples)
    {
      static uint32_t current_chunk_samples = 0;
      static double current_chunk = 0.0;
      static const uint32_t samples_per_chunk = (sample_rate_ / 4);

      for(double sample : new_samples)
      {
        if(current_chunk_samples == samples_per_chunk)
        {
          waveplot_chunks_.push_back(current_chunk);

          current_chunk = 0.0;
          current_chunk_samples = 0;
        }

        current_chunk += sample;
        ++current_chunk_samples;
      }

      //No need to average here - sample is normalized later.
    }

    void AddSamplesLargeThumbnail(const std::vector<double> & new_samples)
    {

    }

    void AddSamplesSmallThumbnail(const std::vector<double> & new_samples)
    {

    }

    void OutputDRData()
    {
      double dr_rating = 0.0;
      for(size_t i = 0; i != num_channels_; ++i)
      {
        dr_channel_rms_[i].back() = sqrt((2.0 * dr_channel_rms_[i].back()) / dr_num_processed_samples[i]);

        dr_channel_rms_[i].sort([](double first, double second) { return (first > second); });

        size_t values_to_sample = dr_channel_rms_[i].size() / 5;

        dr_channel_rms_[i].resize(values_to_sample);

        double rms_sum = 0.0;
        for (double rms_value : dr_channel_rms_[i])
        {
          rms_sum += rms_value * rms_value;
        }

        rms_sum = sqrt(rms_sum / values_to_sample);

        double second_max_value = -100.0;
        double max_value = -100.0;

        for_each(dr_channel_peak_[i].begin(),dr_channel_peak_[i].end(), [&] (double peak) {
           if(peak >= max_value)
           {
             second_max_value = max_value;
             max_value = peak;
           }
           else if(peak > second_max_value)
           {
             second_max_value = peak;
           }
        });

        dr_rating += 20.0*log10(second_max_value/rms_sum);
      }

      dr_rating /= num_channels_;
      fprintf(stderr,"DR Level: %2.1lf\n",dr_rating);

    }

    void OutputWavePlotImageData()
    {
      vector<double> processed_chunks(waveplot_chunks_.size(),0.0);
      vector<uint8_t> image_output(waveplot_chunks_.size(),0);

      size_t center = 0;
      for(double chunk : waveplot_chunks_)
      {
        processed_chunks[center] += chunk * sample_weightings_[0];
        for(size_t j = 1; j != sample_weightings_.size(); ++j)
        {
          double value = chunk * sample_weightings_[j];
          size_t index = center + j;

          if(index < processed_chunks.size())
            processed_chunks[index] += value;

          index = center - j;

          if(index < center) //Will be greater than center if index has wrapped around to ULONG_MAX.
            processed_chunks[index] += value;
        }

        ++center;
      }

      double max_chunk = *max_element(processed_chunks.begin(),processed_chunks.end());

      uint8_t * image_data = image_output.data();
      for(double & chunk : processed_chunks)
      {
        chunk /= max_chunk;

        (*image_data++) = uint8_t(chunk * 200.0);
      }

      size_t min_trim_sample = 0;
      size_t max_trim_sample = 0;

      for(size_t i = 0; (min_trim_sample == 0) && (i != processed_chunks.size()); ++i)
      {
        if(processed_chunks[i] > 0.05)
          min_trim_sample = i;
      }

      for(size_t i = processed_chunks.size()-1; (max_trim_sample == 0) && (i != 0); --i)
      {
        if(processed_chunks[i] > 0.05)
          max_trim_sample = i;
      }

      duration_secs_trimmed_ = (max_trim_sample - min_trim_sample + 2) / 4;

      fwrite(image_output.data(),sizeof(uint8_t),image_output.size(),stdout);
    }

    void OutputPreviewImageData()
    {
      vector<double> processed_chunks(waveplot_chunks_.size(),0.0);
      vector<uint8_t> image_output(waveplot_chunks_.size(),0);

      size_t center = 0;
      for(double chunk : waveplot_chunks_)
      {
        processed_chunks[center] += chunk * sample_weightings_[0];
        for(size_t j = 1; j != sample_weightings_.size(); ++j)
        {
          double value = chunk * sample_weightings_[j];
          size_t index = center + j;

          if(index < processed_chunks.size())
            processed_chunks[index] += value;

          index = center - j;

          if(index < center) //Will be greater than center if index has wrapped around to ULONG_MAX.
            processed_chunks[index] += value;
        }

        ++center;
      }

      double max_chunk = *max_element(processed_chunks.begin(),processed_chunks.end());

      uint8_t * image_data = image_output.data();
      for(double & chunk : processed_chunks)
      {
        chunk /= max_chunk;

        (*image_data++) = uint8_t(chunk * 200.0);
      }

      size_t min_trim_sample = 0;
      size_t max_trim_sample = 0;

      for(size_t i = 0; (min_trim_sample == 0) && (i != processed_chunks.size()); ++i)
      {
        if(processed_chunks[i] > 0.05)
          min_trim_sample = i;
      }

      for(size_t i = processed_chunks.size()-1; (max_trim_sample == 0) && (i != 0); --i)
      {
        if(processed_chunks[i] > 0.05)
          max_trim_sample = i;
      }

      duration_secs_trimmed_ = (max_trim_sample - min_trim_sample + 2) / 4;

      fwrite(image_output.data(),sizeof(uint8_t),image_output.size(),stdout);
    }

    void OutputImageData()
    {
      OutputWavePlotImageData();
      OutputDRData();
    }
  }

  void Process(const char* filename)
  {
    AVFormatContext* format_context = nullptr;

    if(avformat_open_input(&format_context, filename, nullptr, nullptr) < 0)
      error(ERROR_AVFORMAT_OPEN_INPUT);

    format_context->max_analyze_duration = 20000000; //Increase this here, to reduce frequency of libav warnings in the next call.

    if(avformat_find_stream_info(format_context, nullptr) < 0)
      error(ERROR_AVFORMAT_FIND_STREAM_INFO);

    int stream_no = av_find_best_stream(format_context,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);

    if(stream_no < 0)
      error(ERROR_NO_AUDIO_STREAM);

    AVStream* stream = format_context->streams[stream_no];

    //Calculate the approximate duration of the stream - assumed to be less than 18 hours long.
    duration_secs_ = static_cast<uint32_t>((stream->duration * stream->time_base.num) / stream->time_base.den);

    //Assuming 1GB of available system memory, which is still a bit high, we can't currently process more than about 50 minutes of audio in one track.
    if(duration_secs_ > 3000) //Don't analyze tracks longer than 50 minutes (sanity check).
      error(ERROR_AUDIO_TOO_LONG);

    AVCodecContext* codec_context = stream->codec;

    num_channels_ = static_cast<uint8_t>(codec_context->channels);
    sample_rate_  = static_cast<uint32_t>(codec_context->sample_rate);
    bit_rate_ = static_cast<uint16_t>(codec_context->bit_rate);
    sample_format_ = codec_context->sample_fmt;

    if(sample_format_ == AV_SAMPLE_FMT_NONE)
    {
      error(ERROR_BAD_SAMPLE_FORMAT);
    }
    else if(sample_format_ == AV_SAMPLE_FMT_U8 || sample_format_ == AV_SAMPLE_FMT_U8P)
    {
      fputs("Unsigned Byte Format not yet supported - please sent me a sample file so that I can test it!\n",stderr);
      error(ERROR_BAD_SAMPLE_FORMAT);
    }

    uint8_t sample_bytes = static_cast<uint8_t>(av_get_bytes_per_sample(sample_format_));

    dr_channel_peak_.resize(num_channels_);
    dr_channel_rms_.resize(num_channels_);
    dr_num_processed_samples.resize(num_channels_,0);

    for_each(dr_channel_peak_.begin(), dr_channel_peak_.end(), []( list<double> & peaks ) { peaks.push_back(0.0); });
    for_each(dr_channel_rms_.begin(), dr_channel_rms_.end(), []( list<double> & rms ) { rms.push_back(0.0); });

#ifdef DEBUG
    fprintf(stderr, "Num Channels: %i Sample Rate: %i Bit Rate: %i\n", num_channels_, sample_rate_, bit_rate_);
#endif

    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    format_id_ = std::string(codec->name) + "-" + to_string(bit_rate_);

    fprintf(stderr,"Format String: %s\n",format_id_.c_str());

    if (!avcodec_open2(codec_context, codec, nullptr) < 0)
      error(ERROR_AVCODEC_OPEN2);

    AVPacket packet;
    AVFrame* frame = avcodec_alloc_frame();

    double channel_delta = 0.0;
    size_t total_samples = 0;

    while(av_read_frame(format_context, &packet) == 0)
    {
      int decoded;
      avcodec_decode_audio4(codec_context,frame, &decoded, &packet);

      if(decoded != 0)
      {
        if(frame->format != sample_format_)
        {
          fprintf(stderr,"Frame Format inconsistency - exit!");
          return;
        }

        int data_size = av_samples_get_buffer_size(NULL, num_channels_, frame->nb_samples, sample_format_, 1);

        size_t required_storage = data_size / (num_channels_ * sample_bytes);
        total_samples += required_storage;

        //cout << "Data Size: " << data_size << " Required Storage: " << required_storage << " Number of Channels: " << num_channels_ << " Sample bytes: " << sample_bytes << endl;

        vector<double> samples(required_storage,0.0);

        #ifdef DEBUG
            fprintf(stderr,"Samples: %lu\n",required_storage);
        #endif

        if(sample_format_ < AV_SAMPLE_FMT_U8P) //Test for interleaved/planar
        {
          channel_delta += ProcessInterlacedBufferData(samples,frame->data[0]);
        }

        //AddSamplesDR(samples);
        AddSamplesWavePlot(samples);
      }
      av_free_packet(&packet);
      avcodec_get_frame_defaults(frame);
    }

    avcodec_free_frame(&frame);

    float delta_ps = float(channel_delta)/total_samples;

    if((num_channels_ == 2) && (delta_ps < 0.0001))
    {
      fprintf(stderr,"Track is false stereo! (%lf)\n", delta_ps);
      num_channels_ = 1;
    }

#ifdef DEBUG
    fprintf(stderr,"Samples: %lu\n",total_samples);
#endif

    avformat_close_input(&format_context);

    if(total_samples == 0)
      error(ERROR_NO_SAMPLES);

    //dr_mean_sample_ /= dr_samples_;
    //dr_mean_sample_ = sqrt(dr_mean_sample_);
    //double dr_level = 10*log10(dr_max_sample_/dr_mean_sample_);

    //fprintf(stderr,"DR Level: %u Max: %lf Mean: %lf\n",uint8_t(dr_level),dr_max_sample_,dr_mean_sample_);

    OutputImageData();

    return;
  }
}

#ifdef NEVER

bool Audio::SaveWavePlotLargeThumb()
{
  if(!valid_)
    return false;

  double samples_per_chunk = double(samples_.size())/MID_RES_IMAGE_WIDTH;
  double error = 0.0;

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
    fputs("Exiting Imager: Couldn't open output image.\n",stderr);

  //Clean up memory.
  for(png_byte* p : rows)
    delete p;
}

bool Audio::SaveWavePlotSmallThumb()
{
  if(!valid_)
    return false;

  double samples_per_chunk = double(samples_.size())/LOW_RES_IMAGE_WIDTH;
  double error = 0.0;

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
    fputs("Exiting Imager: Couldn't open output image.\n",stderr);

  //Clean up memory.
  for(png_byte* p : rows)
    delete p;
}

bool Audio::SaveWavePlotInfo()
{
  if(!valid_)
    return false;

  //CalculateMixHash();

  fprintf(stdout,"%u|%u|%s|%u",duration_secs_,duration_trimmed_secs_,format_id.c_str(),num_channels_);

}

/*bool CalculateMixHash()
{
  double samples_per_chunk_thumb = double(samples_.size())/400.0;
}*/

#endif
