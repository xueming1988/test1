#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <hrsc_sdk.h>
#include <sys/wait.h>

#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include "stdtostring.h"
#endif
//#include <utils/Log.h>
#include "hrsc_sdk.h"
#include "audiofile.h"
char *save_data_path;

#define LOG_TAG "MihrscDemo"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define AUDIO_FILE_PATH "/data/hrsc/data"

int count_data = 0;
int count_data_i = 0;
uint64_t file_length(FILE *f)
{
    uint64_t len;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    return len;
}

char *file_read_buf(char *fn, int *n)
{
    FILE *file = fopen(fn, "rb");
    char *p = 0;
    int len;

    if (file) {
        len = file_length(file);
        p = (char *)malloc(len + 1);
        len = fread(p, 1, len, file);
        if (n) {
            *n = len;
        }
        fclose(file);
        p[len] = 0;
    }
    return p;
}

static void demo_log_cb(hrsc_log_lvl_t lvl, const char *tag, const char *format, ...)
{
    char buffer[1024] = {0};
    va_list args;

    if (lvl > HRSC_LOG_LVL_DEBUG) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, 1023, format, args);
    va_end(args);
    printf("%s", buffer);
}

static void demo_event_handle(void *cookie, hrsc_event_t event)
{
    printf("cookie=%p, event=%d\n", cookie, event);
}

static void demo_wakeup_data_handle(void *cookie, const hrsc_callback_data_t *data,
                                    int keyword_index)
{
    //    static struct wave_file* p_wf = NULL;
    //    char* file_name[64] = {0};
    //    static file_cnt = 0;
    //
    //    if (NULL == data) {
    //        printf("%s: error: data=NULL\n", __FUNCTION__);
    //        return;
    //    }
    //    if(data->type == HRSC_CALLBACK_DATA_ALL || data->type == HRSC_CALLBACK_DATA_HEADER) {
    //        if(p_wf == NULL) {
    //            snprintf(file_name, 63, "%s/wakeup_%d.wav", AUDIO_FILE_PATH, file_cnt++);
    //            p_wf = create_wav_file(16000, 1, file_name);
    //        }
    //    }
    //    if(p_wf != NULL) {
    //        write_wav_file(p_wf, data->buffer.raw, data->buffer.size >> 1);
    //    }
    //    if(data->type == HRSC_CALLBACK_DATA_ALL || data->type == HRSC_CALLBACK_DATA_TAIL) {
    //        if(p_wf != NULL) {
    //            release_wav_file(p_wf);
    //            p_wf = NULL;
    //        }
    //    }
    //    printf("%s: cookie=%p, data.size=%d, data.type=%d, data.score=%f, data.timestamp=%llu
    //    keyword_index=%d\n",
    //           __FUNCTION__, cookie, data->buffer.size, data->type, data->score,
    //           data->buffer.start_time,keyword_index);
    if (NULL == data) {
        printf("%s: error: data=NULL\n", __FUNCTION__);
        return;
    }

    printf("%s: cookie=%p, data.size=%d, data.type=%d, data.score=%f, data.timestamp=%llu\n",
           __FUNCTION__, cookie, data->buffer.size, data->type, data->score,
           data->buffer.start_time);

    /* FILE *wake_file;
     std::string wake_file_path = std::to_string(save_data_path) + "/wake_callback_" +
     std::to_string(count_data) + ".pcm";
     count_data ++;
     wake_file = fopen(wake_file_path.c_str(), "wb+");
     if(data->buffer.size == 0) {
         printf("data size is 0 \n");
         exit(0);
     }
     fwrite(data->buffer.raw, data->buffer.size, sizeof(char), wake_file);
     fflush(wake_file);*/
    //    if(data->buffer.raw) {
    //        printf("free buffer.raw=%p\n", data->buffer.raw);
    //        free(data->buffer.raw);
    //    }
}
FILE *asr_file;
static void demo_asr_data_handle(void *cookie, const hrsc_callback_data_t *data)
{
    //    static struct wave_file* p_wf = NULL;
    //    char* file_name[64] = {0};
    //    static file_cnt = 0;
    //
    //    if (NULL == data) {
    //        printf("%s: error: data=NULL\n", __FUNCTION__);
    //        return;
    //    }
    //    if(data->type == HRSC_CALLBACK_DATA_ALL || data->type == HRSC_CALLBACK_DATA_HEADER) {
    //        if(p_wf == NULL) {
    //            snprintf(file_name, 63, "%s/asr_%d.wav", AUDIO_FILE_PATH, file_cnt++);
    //            p_wf = create_wav_file(16000, 1, file_name);
    //        }
    //    }
    //    if(p_wf != NULL) {
    //        write_wav_file(p_wf, data->buffer.raw, data->buffer.size >> 1);
    //    }
    //    if(data->type == HRSC_CALLBACK_DATA_ALL || data->type == HRSC_CALLBACK_DATA_TAIL) {
    //        if(p_wf != NULL) {
    //            release_wav_file(p_wf);
    //            p_wf = NULL;
    //        }
    //    }
    //

    /*    printf("%s: cookie=%p, data.size=%d, data.type=%d, data.timestamp=%llu\n",
               __FUNCTION__, cookie, data->buffer.size, data->type, data->buffer.start_time);

    */

    /* if (data->type == HRSC_CALLBACK_DATA_HEADER || data->type == HRSC_CALLBACK_DATA_ALL) {
         std::string asr_file_path = std::to_string(save_data_path) + "/asr_callback_" +
     std::to_string(count_data_i) + ".pcm";
         printf("create asr file success\n");
         asr_file = fopen(asr_file_path.c_str(), "wb+");
         count_data_i ++;
     }
     if(data->buffer.size != 0) {
         fwrite(data->buffer.raw, sizeof(char), data->buffer.size, asr_file);
         fflush(asr_file);
     } else {
         printf("data size is 0\n\n\n\n\n");
     }*/
}

static void demo_voip_data_handle(void *cookie, const hrsc_callback_data_t *data)
{
    if (NULL == data) {
        printf("%s: error: data=NULL\n", __FUNCTION__);
        return;
    }

    printf("%s: cookie=%p, data.size=%d, data.type=%d, data.timestamp=%lld", __FUNCTION__, cookie,
           data->buffer.size, data->type, data->buffer.start_time);
}
FILE *process_data_file = NULL;
static void demo_processed_data_handle(void *cookie, const hrsc_callback_data_t *data)
{
    //    printf("process data callback\n");
    if (NULL == data) {
        printf("%s: error: data=NULL\n", __FUNCTION__);
        return;
    }
    if (process_data_file == NULL) {
        std::string process_file_path = std::to_string(save_data_path) + "/process_callback_" +
                                        std::to_string(count_data) + ".pcm";
        process_data_file = fopen(process_file_path.c_str(), "wb+");
    }

    if (data->buffer.size == 0) {
        printf("data size is 0 \n");
        exit(0);
    }
    // fwrite(data->buffer.raw, data->buffer.size, sizeof(char), process_data_file);
    // fflush(process_data_file);
    //    printf("%s: cookie=%p, data.size=%d, data.type=%d, data.timestamp=%lld \n",
    //           __FUNCTION__, cookie, data->buffer.size, data->type, data->buffer.start_time);
}

static void secure_auth_callback(int code)
{
    printf("auth code is: %d\n", code);
    if (1 == code)
        printf("auth success\n");
    else
        printf("failed auth\n");
}

FILE *file;
int main(int argc, char *argv[])
{
    signal(SIGINT, []() { exit(0); });
    hrsc_effect_config_t hrsc_cfg;
    hrsc_audio_buffer_t input;

    printf("libhrsc version is %s\n", hrsc_get_version_info());
    if (argc < 2) {
        printf("Usage: [-i][-o][-cfg]\n\n-i: input file absoulute path\n -o: outfile folder\n "
               "-cfg:cfg path hrsc folder path\n");
        exit(0);
    }
    char *fn, *data, *cfg_path;

    while (*argv) {
        if (strcmp(*argv, "-i") == 0) {
            argv++;
            fn = *argv;
            printf("source file path is %s\n", fn);
        } else if (strcmp(*argv, "-o") == 0) {
            argv++;
            save_data_path = *argv;
            printf("save file folder is %s\n", save_data_path);
        } else if (strcmp(*argv, "-cfg") == 0) {
            argv++;
            cfg_path = *argv;
            printf("cfg file path is %s\n", cfg_path);
        }
        if (*argv) {
            argv++;
        }
    }

    int len;
    int step, pos, n;

    hrsc_cfg.inputCfg.samplingRate = 16000;
    hrsc_cfg.inputCfg.channels = 4;
    hrsc_cfg.inputCfg.format = HRSC_AUDIO_FORMAT_PCM_16_BIT;
    hrsc_cfg.outputCfg.samplingRate = 16000;
    hrsc_cfg.outputCfg.channels = 1;
    hrsc_cfg.outputCfg.format = HRSC_AUDIO_FORMAT_PCM_16_BIT;
    /*
     * ref_ch_index 从0开始 2代表的是第三声道
     */
    hrsc_cfg.ref_ch_index = 2;
    hrsc_cfg.wakeup_prefix = 200;
    hrsc_cfg.wakeup_suffix = 200;
    hrsc_cfg.wait_asr_timeout = 3000;

    /*
     * vad_timeout 推荐值 400ms
     */
    hrsc_cfg.vad_timeout = 400;
    hrsc_cfg.vad_switch = 1;
    hrsc_cfg.wakeup_data_switch = 1;
    hrsc_cfg.processed_data_switch = 1;
    /*
     * taget_score 目前未生效，待下一版本提交
     */
    hrsc_cfg.target_score = 0.3;
    hrsc_cfg.effect_mode = HRSC_EFFECT_MODE_ASR;
    hrsc_cfg.cfg_file_path = cfg_path;
    hrsc_cfg.priv = (void *)&hrsc_cfg;
    printf("cookie=%p\n", hrsc_cfg.priv);
    hrsc_cfg.log_callback = demo_log_cb;
    hrsc_cfg.event_callback = demo_event_handle;
    hrsc_cfg.wakeup_data_callback = demo_wakeup_data_handle;
    hrsc_cfg.asr_data_callback = demo_asr_data_handle;
    hrsc_cfg.voip_data_callback = demo_voip_data_handle;
    hrsc_cfg.processed_data_callback = demo_processed_data_handle;
    hrsc_cfg.licenseStr = "";
    hrsc_cfg.activeBuff = new char[1024];
    hrsc_cfg.secure_auth_callback = secure_auth_callback;

    printf("start init .... \n");
    if (0 != hrsc_init((const hrsc_effect_config_t *)&hrsc_cfg)) {
        exit(1);
    }

    printf("end init .... \n");
    if (0 != hrsc_start()) {
        hrsc_release();
        exit(1);
    }
    printf("start .... \n");

    // fn = "/data/mic.wav";
    file = fopen(fn, "rb");
    //    data = file_read_buf(fn,&len);
    if (!file) {
        printf("data read fail\n");
        exit(1);
    }
    pos = 0;
    step = 32 * 20 * hrsc_cfg.inputCfg.channels;
    // char * wav_head = new char[44];
    // fread(wav_head, sizeof(char), 44, file);
    char *audio_buffer = new char[step];
    int read_size = 0;
    while ((read_size = fread(audio_buffer, sizeof(char), step, file)) > 0) {
        //        n = min(step,len-pos);
        input.raw = (void *)(audio_buffer);
        input.size = read_size;
        hrsc_process(&input);
        usleep(20 * 1000);
        //        pos += n;
    }
    while (1) {
        sleep(1);
    }
    delete[] audio_buffer;
    free(data);

    /*
     * 阻塞接口，当所有数据处理完成后hrsc_stop才会返回
     */
    hrsc_stop();
    hrsc_release();
    return 0;
}
