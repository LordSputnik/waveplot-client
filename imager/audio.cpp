/*
 * Copyright 2013 Ben Ockmore
 *
 * This file is part of WavePlot Imager.

 * WavePlot Imager is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * WavePlot Imager is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with WavePlot Imager. If not, see <http://www.gnu.org/licenses/>.
 */

#include "audio.hpp"
#include "error.hpp"

#include <cstdint>

#include <algorithm>
#include <array>
#include <list>
#include <string>
#include <vector>
#include <limits>
#include <sstream>

//#define DEBUG

#pragma GCC diagnostic ignored "-Wswitch"

using namespace std;

namespace Audio
{
  namespace
  {
    static uint32_t duration_secs_;
    static uint32_t duration_secs_trimmed_;

    static uint8_t num_channels_;

    static uint32_t bit_rate_;
    static uint32_t sample_rate_;
    static AVSampleFormat sample_format_;

    static string format_id_;

    static const array<double,4> sample_weightings_ = {{10.0,8.0,5.0,3.0}};

    static list<double> waveplot_chunks_;
    static list<double> preview_chunks_;

    static vector< list<double> > dr_channel_peak_;
    static vector< list<double> > dr_channel_rms_;
    static vector< size_t > dr_num_processed_samples;

    static void ProcessNormalizedSampleForDR(double sample, uint8_t channel)
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

    //Functions for normalizing specific formats.
    static double NormalizeSample(int16_t sample)
    {
      return (static_cast<double>(sample) + 0.5) / 32767.5;
    }

    static double NormalizeSample(int32_t sample)
    {
      return (static_cast<double>(sample) + 0.5) / 2147483647.5;
    }

    static double NormalizeSample(float sample)
    {
      return static_cast<double>(sample);
    }

    static double NormalizeSample(double sample)
    {
      return sample;
    }

    template <typename T>
    static double ProcessPlanarBufferData(std::vector<double> & dest, T** src)
    {
      double channel_delta = 0.0;

      for(size_t i = 0; i != dest.size(); ++i)
      {
        double sample_channel_delta = 0.0;

        for(size_t channel = 0; channel != num_channels_; ++channel)
        {
          double normalized_sample = NormalizeSample(src[channel][i]);

          ProcessNormalizedSampleForDR(normalized_sample, channel);

          dest[i] += fabs(normalized_sample);

          if(num_channels_ == 2)
            sample_channel_delta += (channel == 1 ? -0.5 : 0.5) * normalized_sample;
        }

        channel_delta += fabs(sample_channel_delta);
      }

      return channel_delta;
    }

    template <typename T>
    static double ProcessInterlacedBufferData(std::vector<double> & dest, T* src)
    {
      double channel_delta = 0.0;

      for(size_t i = 0; i != dest.size(); ++i)
      {
        size_t offset = i * num_channels_;
        double sample_channel_delta = 0.0;

        for(size_t channel = 0; channel != num_channels_; ++channel)
        {
          double normalized_sample = NormalizeSample(src[(offset) + channel]);

          ProcessNormalizedSampleForDR(normalized_sample, channel);

          dest[i] += fabs(normalized_sample);

          if(num_channels_ == 2)
            sample_channel_delta += (channel == 1 ? -0.5 : 0.5) * normalized_sample;
        }

        channel_delta += fabs(sample_channel_delta);
      }

      return channel_delta;
    }

    static void AddSamplesWavePlot(const std::vector<double> & new_samples)
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

    static void OutputDRData()
    {
      double dr_rating = 0.0;
      for(size_t i = 0; i != num_channels_; ++i)
      {
        dr_channel_rms_[i].back() = sqrt((2.0 * dr_channel_rms_[i].back()) / dr_num_processed_samples[i]);

        dr_channel_rms_[i].sort([](double first, double second) { return (first > second); });

        size_t values_to_sample = std::max(size_t(1),size_t(dr_channel_rms_[i].size() / 5));

        dr_channel_rms_[i].resize(values_to_sample);

        double rms_sum = 0.0;
        for (double rms_value : dr_channel_rms_[i])
        {
          rms_sum += rms_value * rms_value;
        }

        rms_sum = sqrt(rms_sum / values_to_sample);

        double second_max_value = -100.0;
        double max_value = -100.0;

        for(double peak : dr_channel_peak_[i])
        {
           if(peak >= max_value)
           {
             second_max_value = max_value;
             max_value = peak;
           }
           else if(peak > second_max_value)
           {
             second_max_value = peak;
           }
        }

        if(dr_channel_peak_[i].size() < 3)
        {
          dr_rating += 20.0*log10(max_value/rms_sum);
        }
        else
        {
          dr_rating += 20.0*log10(second_max_value/rms_sum);
        }
      }

      dr_rating /= num_channels_;
      fprintf(stderr,"DR Level: %2.5f\n",dr_rating);
      fprintf(stdout,"%2.1f",dr_rating);
    }

    static void OutputWavePlotImageData()
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

      size_t min_trim = numeric_limits<size_t>::max();
      for(size_t i = 0; ((i != image_output.size()) && (min_trim == numeric_limits<size_t>::max())); ++i)
      {
        if(image_output[i] > 10)
          min_trim = i;
      }

      size_t max_trim = numeric_limits<size_t>::max();
      for(size_t i = image_output.size() - 1; ((i >= min_trim) && (max_trim == numeric_limits<size_t>::max())); --i)
      {
        if(image_output[i] > 10)
          max_trim = i;
      }

      duration_secs_trimmed_ = (max_trim - min_trim)/4;

      fwrite(image_output.data(),sizeof(uint8_t),image_output.size(),stdout);
    }

    static void OutputInfo()
    {
      fprintf(stdout,"%u|%u|%s|%u",duration_secs_,duration_secs_trimmed_,format_id_.c_str(),num_channels_);
    }

    static void OutputImageData()
    {
      fputs("WAVEPLOT_START",stdout);
      OutputWavePlotImageData();
      fputs("WAVEPLOT_DR",stdout);
      OutputDRData();
      fputs("WAVEPLOT_INFO",stdout);
      OutputInfo();
      fputs("WAVEPLOT_END",stdout);
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

    AVCodecContext* codec_context = stream->codec;

    num_channels_ = static_cast<uint8_t>(codec_context->channels);
    sample_rate_  = static_cast<uint32_t>(codec_context->sample_rate);
    bit_rate_ = static_cast<uint32_t>(codec_context->bit_rate);
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
    fprintf(stderr, "Num Channels: %i Sample Rate: %i Bit Rate: %u\n", num_channels_, sample_rate_, bit_rate_);
#endif

    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    stringstream sstr;

    sstr << codec->name << '-' << bit_rate_;

    format_id_ = sstr.str();

    //format_id_ = std::string() + "-" + to_string(bit_rate_);

    fprintf(stderr,"Format String: %s Bit Rate: %u\n",format_id_.c_str(),bit_rate_);

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

        vector<double> samples(required_storage,0.0);

        #ifdef DEBUG
            fprintf(stderr,"Samples: %lu\n",required_storage);
        #endif

        switch(sample_format_)
        {
          case AV_SAMPLE_FMT_S16:
            channel_delta += ProcessInterlacedBufferData(samples,reinterpret_cast<int16_t*>(frame->data[0]));
            break;
          case AV_SAMPLE_FMT_S32:
            channel_delta += ProcessInterlacedBufferData(samples,reinterpret_cast<int32_t*>(frame->data[0]));
            break;
          case AV_SAMPLE_FMT_FLT:
            channel_delta += ProcessInterlacedBufferData(samples,reinterpret_cast<float*>(frame->data[0]));
            break;
          case AV_SAMPLE_FMT_DBL:
            channel_delta += ProcessInterlacedBufferData(samples,reinterpret_cast<double*>(frame->data[0]));
            break;
          case AV_SAMPLE_FMT_S16P:
            channel_delta += ProcessPlanarBufferData(samples,reinterpret_cast<int16_t**>(frame->data));
            break;
          case AV_SAMPLE_FMT_S32P:
            channel_delta += ProcessPlanarBufferData(samples,reinterpret_cast<int32_t**>(frame->data));
            break;
          case AV_SAMPLE_FMT_FLTP:
            channel_delta += ProcessPlanarBufferData(samples,reinterpret_cast<float**>(frame->data));
            break;
          case AV_SAMPLE_FMT_DBLP:
            channel_delta += ProcessPlanarBufferData(samples,reinterpret_cast<double**>(frame->data));
            break;
        }

        AddSamplesWavePlot(samples);
      }
      av_free_packet(&packet);
      avcodec_get_frame_defaults(frame);
    }

#if (LIBAVCODEC_VERSION_MAJOR > 53)
    avcodec_free_frame(&frame);
#else
    av_free(frame);
#endif

    float delta_ps = float(channel_delta)/total_samples;

    if((num_channels_ == 2) && (delta_ps < 0.0001))
    {
      fprintf(stderr,"Track is false stereo! (%f)\n", delta_ps);
      num_channels_ = 1;
    }

#ifdef DEBUG
    fprintf(stderr,"Channel Delta: %f\n",delta_ps);
    fprintf(stderr,"Samples: %lu\n",total_samples);
#endif

    avformat_close_input(&format_context);

    if(total_samples == 0)
      error(ERROR_NO_SAMPLES);

    OutputImageData();

    return;
  }
}
