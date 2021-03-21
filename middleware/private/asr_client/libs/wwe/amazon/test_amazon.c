#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pryon_lite.h"

//#include "asr_tts.h"
//#include "wm_util.h"
//
// level: set to 1 to open the log
// flag_index:  should be 0 ~ 16, 0 means no flag
// flag:  the flag value for flag_index
// domain/sub_domain: default to 0 means no domain
#define LOG_ENTRY 16

#define LOG_TYPE_STAT 1
#define LOG_TYPE_TS 2
#define LOG_TYPE_COUNT 3

static long _log_stat[LOG_ENTRY];
static long _log_timeout[LOG_ENTRY];
static long _log_count[LOG_ENTRY];
static int log_init = 0;
PryonLiteDecoderHandle sDecoder = NULL;

#define WWE_PATH_PREFIX "./models/"
// big model files
#define WWE_BIN_US WWE_PATH_PREFIX "Uen_US_alexa.bin"
#define WWE_BIN_IN WWE_PATH_PREFIX "Uen_IN_alexa.bin"
#define WWE_BIN_GB WWE_PATH_PREFIX "Uen_GB_alexa.bin"
#define WWE_BIN_DE WWE_PATH_PREFIX "Ude_DE_alexa.bin"
//#define WWE_BIN_JP  WWE_PATH_PREFIX "Uja_JP_alexa.bin"
#define WWE_BIN_JP WWE_PATH_PREFIX "D.ja-JP.alexa.bin"
char *model_file = NULL;

static long long tickSincePowerOn(void)
{
    long long sec;
    long long msec;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    sec = ts.tv_sec;
    msec = (ts.tv_nsec + 500000) / 1000000;
    return sec * 1000 + msec;
}

static void wiimu_log(int level, int type, int index, long value, char *format, ...)
{
    int log = 1;
    long long ts = tickSincePowerOn();
    if (log_init == 0) {
        memset(_log_timeout, 0, sizeof(long) * LOG_ENTRY);
        memset(_log_stat, 0, sizeof(long) * LOG_ENTRY);
        memset(_log_count, 0, sizeof(long) * LOG_ENTRY);
        log_init = 1;
    }
    if (level == 0)
        log = 0;
    else if (index < LOG_ENTRY && index >= 0) {
        if (type == LOG_TYPE_STAT && value == _log_stat[index])
            log = 0;
        else if (type == LOG_TYPE_TS) {
            long cur_ts = ts / 1000;
            if (cur_ts < _log_timeout[index] + value)
                log = 0;
        }
    }

    if (log) {
        char sz_log[8192];
        va_list args;
        int hour = ts / 3600000;
        int min = ts % 3600000 / 60000;
        int sec = ts % 60000 / 1000;
        int ms = ts % 1000;

        va_start(args, format);
        vsprintf(sz_log, format, args);
        va_end(args);

        printf("[%02d:%02d:%02d.%03d] %s \n", hour, min, sec, ms, sz_log);

        if (index < LOG_ENTRY && index >= 0) {
            if (type == LOG_TYPE_STAT)
                _log_stat[index] = value;
            else if (type == LOG_TYPE_TS)
                _log_timeout[index] = ts / 1000;
        }
    }
}

static PryonLiteSessionInfo sessionInfo;
// static PryonLiteDecoderConfig PryonLiteDecoderConfig_Default;
static PryonLiteDecoderConfig config = PryonLiteDecoderConfig_Default;

static int _CCaptCosume_Asr_Amazon(char *buf, int size, void *priv);
static int _CCaptCosume_initAsr_Amazon(void *priv);

static int _CCaptCosume_Asr_Amazon(char *buf, int size, void *priv)
{
    int pcmCount = 0;
    int pcmSize = 0;
    int ret = 0;
    pcmSize = size;
    while (pcmSize > 0) {
        int len = sessionInfo.samplesPerFrame * 2; // 960;
        if (pcmSize < len)
            len = pcmSize;
        // pass samples to decoder
        // ret = PryonLiteDecoder_PushAudioSamples( (short*)(buf+pcmCount),  len/2);
        ret = PryonLiteDecoder_PushAudioSamples(sDecoder, (short *)(buf + pcmCount), len / 2);
        if (ret != PRYON_LITE_ERROR_OK) {
            printf("ERROR ret=%d\n", ret);
            break;
        }
        pcmCount += (long)len;
        pcmSize -= (long)len;
        // usleep(15000);
    }
    return size;
}

static pthread_mutex_t mutex_wake_lock = PTHREAD_MUTEX_INITIALIZER;
static int wake_detected = 0;
static int wake_count = 0;
static int get_wake_count(void) { return wake_count; }

static int get_wake_result(void)
{
    int ret;
    pthread_mutex_lock(&mutex_wake_lock);
    ret = wake_detected;
    pthread_mutex_unlock(&mutex_wake_lock);
    return ret;
}

static void set_wake_result(int val)
{
    pthread_mutex_lock(&mutex_wake_lock);
    wake_detected = val;
    pthread_mutex_unlock(&mutex_wake_lock);
}
// keyword detection callback

void detectionCallback(PryonLiteDecoderHandle handle, const PryonLiteResult *result)
{
    set_wake_result(1);
    ++wake_count;
    printf("Detected keyword '%s' count=%d\n", result->keyword, wake_count);
}

int file_size_read(char *filename)
{
    int file_size = 0;
    if (filename == NULL)
        return -1;
    FILE *fd_file = fopen(filename, "rb");
    if (NULL == fd_file) {
        printf("%s file open error\n", __FUNCTION__);
        return -1;
    }
    fseek(fd_file, 0L, SEEK_END);
    file_size = ftell(fd_file);
    if (fd_file)
        fclose(fd_file);
    return file_size;
}
static unsigned char *model_buf = NULL;

void loadModel(PryonLiteDecoderConfig *config)
{
    // In order to detect keywords, the wakeword engine uses a model which defines the parameters,
    // neural network weights, classifiers, etc that are used at runtime to process the audio
    // and give detection results.    // Each model is packaged in two formats:
    // 1. A .bin file that can be loaded from disk (via fopen, fread, etc)
    // 2. A .cpp file that can be hard-coded at compile time

    char *model_path = NULL;
    int model_size = 0;
    int read_len = 0;
    FILE *fb_model = NULL;

    // default US
    if (model_path == NULL || model_size <= 0) {
        model_size = file_size_read((char *)model_file);
        model_path = (char *)model_file;
    }
    printf("###loadModel wwe_1.0 path=%s size=%d+++++\n", model_path, model_size);

    if (model_size > 0) {
        fb_model = fopen(model_path, "rb");
        if (model_buf != NULL) {
            free(model_buf);
            model_buf = NULL;
        }
        model_buf = (unsigned char *)calloc(1, model_size);
        if (model_buf) {
            read_len = fread(model_buf, 1, model_size, fb_model); // Read file to buffer
            if (read_len <= 0) {
                free(model_buf);
                model_buf = NULL;
                printf("%s read failed\n", model_path);
                goto end;
            }
        } else {
            model_buf = NULL;
            printf("model_buf calloc failed\n");
            goto end;
        }
    }

    config->sizeofModel =
        model_size;            // example value, will be the size of the binary model byte array
    config->model = model_buf; // pointer to model in memory
    printf("### model_size = %d\n", model_size);
    printf("### loadModel-----\n");

end:
    if (fb_model)
        fclose(fb_model);
}

static int default_threshold = 400;
static int _CCaptCosume_initAsr_Amazon(void *priv)
{
    wiimu_log(1, 0, 0, 0, (char *)"%s++\n", __FUNCTION__);

    // config = PryonLiteDecoderConfig_Default;
    loadModel(&config);

    // Query for the size of instance memory required by the decoder
    PryonLiteModelAttributes modelAttributes;
    PryonLiteError err =
        PryonLite_GetModelAttributes(config.model, config.sizeofModel, &modelAttributes);
    config.decoderMem = (char *)malloc(modelAttributes.requiredDecoderMem);
    if (config.decoderMem == NULL) {
        // handle error
        return -1;
    }
    config.sizeofDecoderMem = modelAttributes.requiredDecoderMem;

    config.detectThreshold = default_threshold; // default threshold
    config.resultCallback = detectionCallback;  // register detection handler
    // config.vadCallback = vadCallback; // register VAD handler
    // config.useVad = true;  // disable voice activity detector
    config.vadCallback = NULL; // register VAD handler
    config.useVad = false;     // disable voice activity detector

    err = PryonLiteDecoder_Initialize(&config, &sessionInfo, &sDecoder);
    if (err != PRYON_LITE_ERROR_OK) {
        free(config.decoderMem);
        wiimu_log(1, 0, 0, 0, (char *)"PryonLiteDecoder_Initialize failed err=%d\n", err);
    } else
        wiimu_log(1, 0, 0, 0, (char *)"PryonLiteDecoder_Initialize succeed, samplesPerFrame=%d "
                                      "detectThreshold=%d=====\n",
                  sessionInfo.samplesPerFrame, config.detectThreshold);

    wiimu_log(1, 0, 0, 0, (char *)"%s--\n", __FUNCTION__);

    return 0;
}

static void AIYD_Start_sensory(void) { _CCaptCosume_initAsr_Amazon(NULL); }

static void AIYD_Stop_sensory(void) {}

static off_t get_file_len(const char *path)
{
    struct stat buf;
    if (stat(path, &buf))
        return 0;
    return buf.st_size;
}

static int read_file(char *path, char *buf, int len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    int left = len;
    char *p = buf;
    while (left > 0) {
        int ret = read(fd, p, left);
        if (ret <= 0)
            break;
        left -= ret;
        p += ret;
    }
    close(fd);
    return (len - left);
}

#define SAMPLE_RATE 16000
#define BYTES_TO_MS(x) (1000 * (x) / 2 / SAMPLE_RATE)

static int do_test(int chunk_size, char *buf, int file_size)
{
    char *p = buf;
    int size = file_size;
    int ret = 0;
    volatile long long ts_start = tickSincePowerOn();
    while (size > 0) {
        volatile long long ts_start1 = tickSincePowerOn();
        if (chunk_size > size)
            chunk_size = size;
        ret = _CCaptCosume_Asr_Amazon(p, chunk_size, NULL);
        volatile long long ts_start2 = tickSincePowerOn();
        size -= chunk_size;
        p += chunk_size;
        // printf("should sleep %d us\n", BYTES_TO_MS(chunk_size)*1000 - (int)(ts_start2 -
        // ts_start1)*1000);
        // usleep(BYTES_TO_MS(chunk_size)*1000 - (ts_start2 - ts_start1)*1000); // the same as
        // capture
        if (ret >= 0)
            break;
    }
    volatile long long ts_end = tickSincePowerOn();

#if 0
    if(ret > 0)
        wiimu_log(1, 0, 0, 0, " score %d chunk %d size = %d(%d/%d ms) %lld ms",
                  ret, chunk_size, file_size - size, BYTES_TO_MS(file_size - size), BYTES_TO_MS(file_size), (ts_end - ts_start));
    else
        wiimu_log(1, 0, 0, 0, "   -----score %d chunk %d size = %d(%d/%d ms) %lld ms",
                  ret, chunk_size, file_size - size, BYTES_TO_MS(file_size - size), BYTES_TO_MS(file_size), (ts_end - ts_start));
#endif
    return ret;
}

#include <sys/mman.h>

#define SAMPLE_RATE 16000
#define BYTES_TO_MS(x) (1000 * (x) / 2 / SAMPLE_RATE)
#define MS_TO_BYTES(x) (SAMPLE_RATE * 2 * (x) / 1000)
static int search_id = 8;

#if 0
static void save_wake_word(char *out_path, char *p, off_t offset, off_t file_size,
                           off_t len, off_t pre_bytes)
{
    off_t post_bytes = len - pre_bytes;
    if(pre_bytes > offset)
        pre_bytes = offset;
    if(post_bytes > (file_size - offset))
        post_bytes = (file_size - offset);

    int fd = open(out_path, O_RDWR | O_TRUNC | O_CREAT, 0644);
    if(fd < 0) {
        printf("%s %s failed\n", __FUNCTION__, out_path);
        return;
    }

    char *buf = (p - pre_bytes);
    long long left = (post_bytes + pre_bytes);
    while(left > 0) {
        long long ret = write(fd, buf, left);
        if(ret <= 0)
            break;
        left -= ret;
        buf += ret;
    }
    close(fd);
}
#else
#define DUMP_FILE_LEN MS_TO_BYTES(10000)
static char dump_file_buf[DUMP_FILE_LEN];
static int buf_write_offset = 0;

static void update_recent_buffer(char *buf, int len)
{
    int len1;
    int len2 = 0;
    char *p = buf;

    if (len > DUMP_FILE_LEN) {
        p = buf + (len - DUMP_FILE_LEN);
        len = DUMP_FILE_LEN;
    }
    len1 = len;
    if (buf_write_offset + len > DUMP_FILE_LEN) {
        len1 = DUMP_FILE_LEN - buf_write_offset;
        len2 = len - len1;
    }

    if (len1 > 0) {
        memcpy(&dump_file_buf[buf_write_offset], p, len1);
        buf_write_offset = (buf_write_offset + len1) % DUMP_FILE_LEN;
        p += len1;
    }

    if (len2 > 0) {
        memcpy(&dump_file_buf[buf_write_offset], p, len2);
        buf_write_offset = (buf_write_offset + len2) % DUMP_FILE_LEN;
    }
}

void save_wake_word(char *path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        int len = DUMP_FILE_LEN - buf_write_offset;
        int len2 = 0;
        if (len == 0)
            len = DUMP_FILE_LEN;
        else
            len2 = DUMP_FILE_LEN - len;
        if (len > 0)
            write(fd, &dump_file_buf[buf_write_offset], len);

        if (len2 > 0)
            write(fd, &dump_file_buf[0], len2);
        close(fd);
    }
}
#endif

static int do_test_2(char *path)
{
    int chunk_size = 160 * 2;
    off_t file_size = get_file_len(path);
    off_t left = file_size;
    int ret = 0;
    char *p = NULL;
    char *start_addr = NULL;
    //  int wake_count = 0;
    int fd = -1;
    long long file_len_ms = (file_size / (16000 * 2)) * 1000;
    long offset_ms = 0;
    int i = 1;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open %s failed\n", path);
        return -1;
    }

#if 0
    start_addr = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if(start_addr == (char *) -1) {
        printf("mmap failed, file_size=%ld\n", file_size);
        close(fd);
        return -1;
    }
    //save_wake_word("test.wav", start_addr, 0, file_size, file_size, 0);
    //return 0;

    p = start_addr;
#else
    p = (char *)malloc(chunk_size);
#endif
    AIYD_Start_sensory();
    wiimu_log(1, 0, 0, 0, "start test %s threshold=%d len=%lld duration=%.2ld:%.2ld:%.2ld chunk=%d",
              path, config.detectThreshold, file_size, (long)(file_len_ms / 1000 / 3600),
              (long)((file_len_ms / 1000 / 60) % 60), (long)((file_len_ms / 1000) % 60),
              chunk_size);

    long long start_ts = tickSincePowerOn() / 1000;

    while (left > 0 && p) {
        if (chunk_size > left)
            chunk_size = left;

        ret = read(fd, p, chunk_size);
        if (ret <= 0)
            break;

        update_recent_buffer(p, ret);
        ret = _CCaptCosume_Asr_Amazon(p, chunk_size, NULL);
        if (get_wake_result() > 0) {
            char out_path[256];
            offset_ms = file_len_ms - (left / (16000 * 2)) * 1000;
            wiimu_log(1, 0, 0, 0, "wake count %d offset=%.2ld:%.2ld:%.2ld", wake_count,
                      offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60,
                      (offset_ms / 1000) % 60);
            memset((void *)out_path, 0, sizeof(out_path));
            snprintf(out_path, sizeof(out_path), "%s_%.2ld_%.2ld_%.2ld.pcm", path,
                     offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60,
                     (offset_ms / 1000) % 60);
            // save_wake_word(out_path, p, (unsigned int)(p - start_addr), file_size,
            // MS_TO_BYTES(10000), MS_TO_BYTES(10000));
            save_wake_word(out_path);
            set_wake_result(0);
        } else {
#if 1 // show log every 10 seconds
            long long current_ts = tickSincePowerOn() / 1000;
            if ((current_ts - start_ts) > 0 && (current_ts - start_ts) == i * 10) {
                offset_ms = file_len_ms - (left / (16000 * 2)) * 1000;
                wiimu_log(1, 0, 0, 0, "%d wake count %d offset=%.2ld:%.2ld:%.2ld", i++, wake_count,
                          offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60,
                          (offset_ms / 1000) % 60);
            }
#endif
        }

        left -= chunk_size;
        // p += chunk_size;
    }
    AIYD_Stop_sensory();
    // munmap(start_addr, file_size);
    free(p);
    close(fd);

    return (get_wake_count() > 0 ? 0 : -1);
}

void write_to_excel(int index, char *file_name, int result)
{
    FILE *fp = fopen("./result.xls", "a+");
    if (17 == index) {
        fprintf(fp, "%d\t\n", result);
    } else if (1 == index) {
        fprintf(fp, "%s\t%d\t", file_name, result);
    } else {
        fprintf(fp, "%d\t", result);
    }
    fclose(fp);
}

// calc cpu usage, 100seconds data
static void show_cpu_usage(void)
{
    int i;
    int chunk_size = 320;
    char *pbuf = (char *)malloc(chunk_size);
    if (pbuf) {
        AIYD_Start_sensory();
        volatile long long start_ts = tickSincePowerOn() / 1000;

        i = (16000 * 2 * 100) / chunk_size;
        printf("CPU usage test i=%d\n", i);

        while (i-- > 0)
            _CCaptCosume_Asr_Amazon(pbuf, chunk_size, NULL);
        volatile long long delta = tickSincePowerOn() / 1000 - start_ts;
        printf("CPU usage %lld%%\n", delta);
        free(pbuf);
    }
}

int main(int argc, char **argv)
{
    int ret = 0;
    int size = 0;
    char *path = NULL;
    char *buf = NULL;
    int file_size = 0;
    int i;
    int parse_to_end = 0;

    fprintf(stderr, "Usage: %s 16khz_1ch_pcm_file threshold model_file\n", argv[0]);
    if (argc < 4) {
        show_cpu_usage();
        return -1;
    }

    if (argc >= 3)
        default_threshold = atoi(argv[2]);
    path = argv[1];

    if (argc >= 4)
        model_file = argv[3];

    if (1)
        return do_test_2(path);

    file_size = get_file_len(path);
    size = file_size;
    if (size > 0) {
        buf = (char *)malloc(size + 1);
        if (!buf) {
            printf("Not enough memory, size=%d\n", size);
            return -1;
        }
        if (read_file(path, buf, size) != size)
            return -1;
    }
    AIYD_Start_sensory();
    ret = do_test(320, buf, file_size);
    // wait for result
    usleep(100000);
    printf("%d %s %d\n", 1, path, get_wake_count());
    char *test_name = strchr(path, '/');
    if (test_name) {
        write_to_excel(1, test_name + 1, get_wake_count());
    }

#if 0
    sleep(1);
    volatile long long ts_start = tickSincePowerOn();
    int fail_cnt = 0;
    for(i = 0; i < count; ++i) {

        //AIYD_Start_sensory();
        if(do_sensory_test(chunk_size, buf, file_size) <= 0)
            ++fail_cnt;
        //AIYD_Stop_sensory();
    }
    volatile long long ts_end = tickSincePowerOn();
    wiimu_log(1, 0, 0, 0, "test count = %d  fail %d %lld ms avg %ld ms PHRASESPOT_DELAY=%d",
              count, fail_cnt, (ts_end - ts_start), (long)(ts_end - ts_start) / count, PHRASESPOT_DELAY);
#endif
    AIYD_Stop_sensory();
    free(buf);
    return (get_wake_count() > 0 ? 0 : -1);
}
