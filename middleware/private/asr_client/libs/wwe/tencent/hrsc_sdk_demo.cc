/*
 * Copyright (c) 2019 horizon.ai.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * horizon.ai, except with written permission of horizon.ai.
 * @author: xuecheng.cui
 * @file: hrsc_sdk_demo.cc
 * @date: 12/30/19
 * @brief: 
 */

#include "hrsc_sdk.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>

HrscEffectConfig effect_cfg{};
char *output_path{nullptr};
int32_t switch_flag = 0;

static size_t wkp_count = 0;

FILE *asr_fp{nullptr};
FILE *wkp_fp{nullptr};

template<class T>
static std::string to_string(const T &t) {
  std::ostringstream ostream;
  ostream << t;
  return ostream.str();
}

void DemoEventCallback(const void *cookie, HrscEventType event) {
  if (event == kHrscEventWkpNormal || event == kHrscEventWkpOneshot) {
    std::cout << "wakeup success, wkp count is " << ++wkp_count << std::endl;
    if (switch_flag) {
      if (asr_fp != nullptr) fclose(asr_fp);
      std::string file_name =
          to_string(output_path) + "/asr_" + to_string(wkp_count) + ".pcm";
      asr_fp = fopen(file_name.c_str(), "wb");
    }
  }
  switch (event) {
    case kHrscEventSDKStart:
      std::cout << "sdk start" << std::endl;
      break;
    case kHrscEventSDKEnd:
      std::cout << "sdk end" << std::endl;
      break;
    default:
      break;
  }
}

void DemoWakeupDataCallback(const void *cookie,
                            const HrscCallbackData *data,
                            const int keyword_index) {
  std::cout << "wakeup data , size is "
            << data->audio_buffer.size << std::endl;
  if (switch_flag && keyword_index == 0) {
    if (wkp_fp != nullptr) fclose(wkp_fp);
    std::string wkp_name =
        to_string(output_path) + "/wkp_" + to_string(wkp_count) + ".pcm";
    wkp_fp = fopen(wkp_name.c_str(), "wb");
    if (wkp_fp == nullptr) return;
    fwrite(data->audio_buffer.audio_data, 1, data->audio_buffer.size, wkp_fp);
    fflush(wkp_fp);
  }
}

void DemoAuthCallback(const void *cookie, int code) {
  fprintf(stdout, "auth code is %d\n", code);
}
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "./hrsctest -i input_audio -o output -cfg config \n");
    return -1;
  }

  char *input_audio{nullptr};
  char *cfg_path{nullptr};

  while (*argv) {
    if (strcmp(*argv, "-i") == 0) {
      ++argv;
      input_audio = *argv;
    } else if (strcmp(*argv, "-o") == 0) {
      ++argv;
      output_path = *argv;
    } else if (strcmp(*argv, "-cfg") == 0) {
      ++argv;
      cfg_path = *argv;
    } else if (strcmp(*argv, "-switch") == 0) {
      ++argv;
      if (*argv != nullptr) {
        switch_flag = strtoll(*argv, nullptr, 10);
      }
    }
    if (*argv) argv++;
  }

  if (cfg_path == nullptr || input_audio == nullptr || output_path == nullptr) {
    fprintf(stderr, "./hrsctest -i input_audio -o output -cfg config \n");
    return -1;
  }

  HrscAudioConfig input_cfg;
  input_cfg.audio_channels = 1;
  input_cfg.sample_rate = 16000;
  input_cfg.audio_format = kHrscAudioFormatPcm16Bit;

  HrscAudioConfig output_cfg;
  output_cfg.audio_channels = 1;
  output_cfg.sample_rate = 16000;
  output_cfg.audio_format = kHrscAudioFormatPcm16Bit;

  effect_cfg.input_cfg = input_cfg;
  effect_cfg.output_cfg = output_cfg;
  effect_cfg.priv = &effect_cfg;
  effect_cfg.asr_timeout = 5000;
  effect_cfg.cfg_file_path = cfg_path;
  effect_cfg.custom_wakeup_word = nullptr;
  effect_cfg.vad_timeout = 5000;
  effect_cfg.ref_ch_index = 5;
  effect_cfg.target_score = 0;
  effect_cfg.support_command_word = 0;
  effect_cfg.wakeup_prefix = 200;
  effect_cfg.wakeup_suffix = 200;
  effect_cfg.output_name = "/tmp/";
  effect_cfg.HrscEventCallback = DemoEventCallback;
  effect_cfg.HrscWakeupDataCallback = DemoWakeupDataCallback;
  effect_cfg.HrscAuthCallback = DemoAuthCallback;

  void *handle = HrscInit(&effect_cfg);
  if (handle == nullptr) {
    fprintf(stderr, "hrsc init error ! \n");
    return -1;
  }
  fprintf(stdout, "hrsc init success ! \n");

  if (HRSC_CODE_SUCCESS != HrscStart(handle)) {
    fprintf(stderr, "hrsc start error ! \n");
    return -1;
  }
  fprintf(stdout, "hrsc start success ! \n");

  FILE *input_fp = fopen(input_audio, "rb");
  if (!input_fp) {
    fprintf(stderr, "open input audio failed ! \n");
    return -1;
  }

  size_t input_len = effect_cfg.input_cfg.audio_channels * 512;
  char *input_buff = new char[input_len];
  HrscAudioBuffer hrsc_buffer;
  fprintf(stdout, "before send audio ! \n");
  while (fread(input_buff, sizeof(char), input_len, input_fp) >= input_len) {
    hrsc_buffer.audio_data = input_buff;
    hrsc_buffer.size = input_len;
    HrscProcess(handle, &hrsc_buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  fprintf(stdout, "over send audio ! \n");
  std::this_thread::sleep_for(std::chrono::seconds(2));
  fprintf(stdout, "befor stop sdk ! \n");
  HrscStop(handle);
  fprintf(stdout, "stop sdk success ! \n");
  HrscRelease(&handle);
  fprintf(stdout, "destory sdk success ! \n");
  delete[]input_buff;
  return 0;
}

