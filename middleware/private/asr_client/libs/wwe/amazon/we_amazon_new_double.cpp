#if defined(YD_AMAZON_MODULE)
#if defined(AMAZON_DOUBLE_TRIGGER)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "pthread.h"
#include "wm_util.h"

#define MIN_WKEUP_DELTA_TIME 500 // 500ms

extern pthread_mutex_t mutex_YDserver_op;

extern WIIMU_CONTEXT *g_wiimu_shm;
#include "pryon_lite.h"
static PryonLiteSessionInfo sessionInfo;
static PryonLiteDecoderConfig config = PryonLiteDecoderConfig_Default;
// decoder handle
PryonLiteDecoderHandle sDecoder_double = NULL;

extern int asr_waked;
#ifdef AVS_CLOUD_BASED_WWV
PryonLiteResult g_alexa_last_plresult_double = {0};
int64_t g_cosume_asr_totlen_double = 0;
int64_t g_cosume_asr_last_totlen_double = 0;
#endif
static int ww_len_min_ms = 0; // 500ms

extern int amazon_detectionCallback_waked;

extern "C" {
int get_asr_search_id_double(void);
int get_asr_amazon_enable_double(void);
int CCaptCosume_Asr_Amazon_double(char *buf, int size, void *priv);
int CCaptCosume_initAsr_Amazon_double(void *priv);
int CCaptCosume_deinitAsr_Amazon_double(void *priv);
int alexa_set_recognize_info(int64_t start_sample, int64_t end_sample);
char *asr_get_language(void);
}

#define LOCALE_US "en-US"
#define LOCALE_US_ES "es-US"
#define LOCALE_UK "en-GB"
#define LOCALE_DE "de-DE"
#define LOCALE_IN "en-IN"
#define LOCALE_IN_HI "hi-IN"
#define LOCALE_JP "ja-JP"
#define LOCALE_FR "fr-FR"
#define LOCALE_AU "en-AU"
#define LOCALE_CA_EN "en-CA"
#define LOCALE_CA_FR "fr-CA"
#define LOCALE_IT "it-IT"
#define LOCALE_MX "es-MX"
#define LOCALE_ES "es-ES"
#define LOCALE_BR "pt-BR"

// Austria is exactly the same as Germany in terms of TTS, WWE, language etc
// Ireland is exactly the same as UK
// New Zealand is exactly the same as Australia.

typedef struct {
    const char *locale_key;
    const char *model_path;
    int threshold;
} locale_info;

#define WWE_PATH_PREFIX "/system/workdir/lib/"
#if defined(PLATFORM_AMLOGIC)
#define WWE_BIN_US WWE_PATH_PREFIX "D.en-US.alexa.bin"
#define WWE_BIN_IN WWE_PATH_PREFIX "D.en-IN.alexa.bin"
#define WWE_BIN_DE WWE_PATH_PREFIX "D.de-DE.alexa.bin"
#define WWE_BIN_JP WWE_PATH_PREFIX "D.ja-JP.alexa.bin"
#define WWE_BIN_FR WWE_PATH_PREFIX "D.fr-FR.alexa.bin"
#define WWE_BIN_CA WWE_PATH_PREFIX "D.fr-FR.alexa.bin"
#define WWE_BIN_ES WWE_PATH_PREFIX "D.es-ES.alexa.bin"
#define WWE_BIN_MX WWE_PATH_PREFIX "D.es-ES.alexa.bin"
#define WWE_BIN_IT WWE_PATH_PREFIX "D.it-IT.alexa.bin"
#define WWE_BIN_AU WWE_PATH_PREFIX "D.en-US.alexa.bin"
#define WWE_BIN_BR WWE_PATH_PREFIX "D.pt-BR.alexa.bin"
#define WWE_BIN_GB WWE_BIN_US

static locale_info locale_info_tbl_double[] = {
/* USA - English */
#if defined(WWE_DOUBLE_TRIGGER)
    {LOCALE_US, WWE_BIN_US, 500}, // FAR number is 1 for simulation
#else
    {LOCALE_US, WWE_BIN_US, 350}, // FAR number is 1 for simulation
#endif
    /* USA - Spanish */
    {LOCALE_US_ES, WWE_BIN_ES, 500}, // FAR number is 1 for simulation
    /* UK - English */
    {LOCALE_UK, WWE_BIN_GB, 500}, // FAR number is 1 for simulation
    /* Germany - German */
    {LOCALE_DE, WWE_BIN_DE, 600}, // FAR number is 1 for simulation
    /* Indian - English */
    {LOCALE_IN, WWE_BIN_IN, 500}, // FAR number is 3 for simulation !!!
    /* Indian - Hindi */
    {LOCALE_IN_HI, WWE_BIN_IN, 600}, // FAR number is 2 for simulation
    /* Japan - Japanese */
    {LOCALE_JP, WWE_BIN_JP, 350}, // FAR number is 0 for simulation
    /* France - French */
    {LOCALE_FR, WWE_BIN_FR, 350}, // FAR number is 2 for simulation
    /* Australia - English */
    {LOCALE_AU, WWE_BIN_US, 500}, // FAR number is 7 for simulation
    /* Canada - English */
    {LOCALE_CA_EN, WWE_BIN_US, 350}, // Same as US
    /* Canada - French */
    {LOCALE_CA_FR, WWE_BIN_CA, 350},
    /* Italy - Italian */
    {LOCALE_IT, WWE_BIN_IT, 350}, // FAR number is 0 for simulation
    /* Mexico - Spanish */
    {LOCALE_MX, WWE_BIN_MX, 550}, // FAR number is 3 for simulation
    /* Span - Spanish */
    {LOCALE_ES, WWE_BIN_ES, 350},
    /* Brazil - Portuguese */
    {LOCALE_BR, WWE_BIN_BR, 550}, // FAR number is 0 for simulation
};

#else
#define WWE_BIN_US WWE_PATH_PREFIX "U_250k.en-US.alexa.bin"
#define WWE_BIN_IN WWE_PATH_PREFIX "U_250k.en-IN.alexa.bin"
#define WWE_BIN_DE WWE_PATH_PREFIX "U_250k.de-DE.alexa.bin"
#define WWE_BIN_JP WWE_PATH_PREFIX "U_250k.ja-JP.alexa.bin"
#define WWE_BIN_FR WWE_PATH_PREFIX "U_250k.fr-FR.alexa.bin"
#define WWE_BIN_CA WWE_PATH_PREFIX "U_250k.fr-FR.alexa.bin"
#define WWE_BIN_GB WWE_PATH_PREFIX "U_250k.en-GB.alexa.bin"
#define WWE_BIN_ES WWE_PATH_PREFIX "U_250k.es-ES.alexa.bin"
#define WWE_BIN_MX WWE_PATH_PREFIX "U_250k.es-MX.alexa.bin"
#define WWE_BIN_IT WWE_PATH_PREFIX "U_250k.it-IT.alexa.bin"
#define WWE_BIN_AU WWE_PATH_PREFIX "U_250k.en-AU.alexa.bin"
#define WWE_BIN_BR WWE_PATH_PREFIX "W_250k.pt-BR.alexa.bin"

static locale_info locale_info_tbl_double[] = {
    /* USA - English */
    {LOCALE_US, WWE_BIN_US, 450}, // FAR number is 0 for simulation
    /* USA - Spanish */
    {LOCALE_US_ES, WWE_BIN_ES, 500}, // FAR number is 1 for simulation
    /* UK - English */
    {LOCALE_UK, WWE_BIN_GB, 500}, // FAR number is 2 for simulation
    /* Germany - German */
    {LOCALE_DE, WWE_BIN_DE, 350}, // FAR number is 1 for simulation
    /* Indian - English */
    {LOCALE_IN, WWE_BIN_IN, 350}, // FAR number is 0 for simulation
    /* Indian - Hindi */
    {LOCALE_IN_HI, WWE_BIN_IN, 600}, // FAR number is 2 for simulation
    /* Japan - Japanese */
    {LOCALE_JP, WWE_BIN_JP, 500}, // FAR number is 5 for simulation !!!
    /* France - French */
    {LOCALE_FR, WWE_BIN_FR, 450}, // FAR number is 1 for simulation
    /* Australia - English */
    {LOCALE_AU, WWE_BIN_AU, 500}, // FAR number is 8 for simulation ?
    /* Canada - English */
    {LOCALE_CA_EN, WWE_BIN_US, 450}, // Same as US
    /* Canada - French */
    {LOCALE_CA_FR, WWE_BIN_CA, 350},
    /* Italy - Italian */
    {LOCALE_IT, WWE_BIN_IT, 350}, // FAR number is 0 for simulation
    /* Mexico - Spanish */
    {LOCALE_MX, WWE_BIN_MX, 550},
    /* Span - Spanish */
    {LOCALE_ES, WWE_BIN_ES, 350},
    /* Brazil - Portuguese */
    {LOCALE_BR, WWE_BIN_BR, 650}, // FAR number is 1 for simulation
};

#endif
#define WWE_SDK_VERSION "pryon_lite_1.13.5-zelda-2019.07.23.1159"

// static PryonLiteDecoderConfig PryonLiteDecoderConfig_Default;
static unsigned char *model_buf = NULL;
// const PryonLiteDecoderConfig PryonLiteDecoderConfig_Default =
// {NULL,NULL,500,false,false,NULL,0,NULL,0,NULL,NULL};

int CCaptCosume_Asr_Amazon_double(char *buf, int size, void *priv)
{
    int pcmCount = 0;
    int pcmSize = size;
    int ret = 0;
    int len = sessionInfo.samplesPerFrame * 2;
    static char databuf[320] = {0};
    static int dataLen = 0;
    if (!get_asr_amazon_enable_double()) {
        return size;
    }
    if (dataLen) {
        memcpy(databuf + dataLen, buf, len - dataLen);
        pthread_mutex_lock(&mutex_YDserver_op);
        // pass samples to decoder
        ret = PryonLiteDecoder_PushAudioSamples(sDecoder_double, (short *)(databuf), len / 2);
        // printf("PryonLiteDecoder_PushAudioSamples return %d, len=%d ts=%lld\n", ret, len, ts);
        pthread_mutex_unlock(&mutex_YDserver_op);
        if (ret != PRYON_LITE_ERROR_OK) {
            wiimu_log(1, 0, 0, 0, (char *)"%s ERROR ret=%d size=%d, len=%d\n", __FUNCTION__, ret,
                      size, len);
        }

        pcmCount += (long)(len - dataLen);
        pcmSize -= (long)(len - dataLen);
        dataLen = 0;
    }
    while (pcmSize > 0) {
        if (pcmSize < len) {
            dataLen = pcmSize;
            memcpy(databuf, buf + pcmCount, dataLen);
            break;
        }

        pthread_mutex_lock(&mutex_YDserver_op);
        // pass samples to decoder
        ret =
            PryonLiteDecoder_PushAudioSamples(sDecoder_double, (short *)(buf + pcmCount), len / 2);
        // printf("PryonLiteDecoder_PushAudioSamples return %d, len=%d ts=%lld\n", ret, len, ts);
        pthread_mutex_unlock(&mutex_YDserver_op);
        if (ret != PRYON_LITE_ERROR_OK) {
            wiimu_log(1, 0, 0, 0, (char *)"%s ERROR ret=%d\n", __FUNCTION__, ret);
            break;
        }

        pcmCount += (long)len;
        pcmSize -= (long)len;
    }
    return size;
}

// keyword detection callback
void detectionCallback_double(PryonLiteDecoderHandle handle, const PryonLiteResult *result)
{
    static int count = 0;
    int len_ms = 0;
    int score = 0;
    int detect = 1;

    if (!result) {
        fprintf(stderr, "%s result is null\n", __FUNCTION__);
        return;
    }
    wiimu_log(1, 0, 0, 0, (char *)"[DOUBLE] Amazon detection Result: %s", result->keyword);

    len_ms = (int)(result->endSampleIndex - result->beginSampleIndex) / 16;
    score = result->confidence;

#ifdef EN_AVS_COMMS
    if (g_wiimu_shm->asr_ongoing == 0 && g_wiimu_shm->asr_session_start == 0)
#else
    if (g_wiimu_shm->asr_ongoing == 0 &&
        g_wiimu_shm->asr_session_start ==
            0) // add asr_session_start flag by weiqiang.huang for STAB-241
#endif
    {
        if (strcmp(result->keyword, "ALEXA") == 0) {
            wiimu_log(1, 0, 0, 0, (char *)"[DOUBLE] Amazon Result: %s (%d/%d) score:%d len_ms:%d",
                      result->keyword, ++count, g_wiimu_shm->times_talk_on + 1, score, len_ms);

            if (len_ms < ww_len_min_ms) {
                wiimu_log(1, 0, 0, 0, (char *)"[DOUBLE] wake word too short %d < %d ms", len_ms,
                          ww_len_min_ms);
                return;
            }
#ifdef AVS_CLOUD_BASED_WWV
            printf("[DOUBLE] [CBV info] beginSampleIndex = %lld,endSampleIndex = %lld, "
                   "g_cosume_asr_totlen_double = %lld,dt1=%lld,dt2=%lld\n",
                   result->beginSampleIndex, result->endSampleIndex, g_cosume_asr_totlen_double,
                   (result->endSampleIndex - result->beginSampleIndex),
                   (g_cosume_asr_totlen_double - result->endSampleIndex));
            g_alexa_last_plresult_double.beginSampleIndex = result->beginSampleIndex;
            g_alexa_last_plresult_double.endSampleIndex = result->endSampleIndex;
            g_cosume_asr_last_totlen_double = g_cosume_asr_totlen_double;
            // alexa_set_recognize_info(result->beginSampleIndex, result->endSampleIndex);
            alexa_set_recognize_info(8000, 20800);
#endif
#if defined(WWE_DOUBLE_TRIGGER)
            detect &= ((tickSincePowerOn() - g_wiimu_shm->asr_wakeup_tick) > MIN_WKEUP_DELTA_TIME);
#endif
            if (detect) {
                SocketClientReadWriteMsg(
                    "/tmp/RequestIOGuard", (char *)"GNOTIFY=WWE_TRIGGED:DOUBLE_AMAZON",
                    strlen("GNOTIFY=WWE_TRIGGED:DOUBLE_AMAZON"), NULL, NULL, 0);
#if defined(WWE_DOUBLE_TRIGGER)
                g_wiimu_shm->times_trigger2++;
                g_wiimu_shm->asr_wakeup_tick = tickSincePowerOn();
#endif
#ifndef AVS_ARBITRATION
                SocketClientReadWriteMsg("/tmp/RequestASRTTS", (char *)"talkstart:4",
                                         strlen("talkstart:4"), NULL, NULL, 0);
#else
                amazon_detectionCallback_waked = 1;
#endif
#if !defined(BAIDU_DUMI) && !defined(TENCENT)
                asr_waked = 1;
#endif
            }
        } else if (strcmp(result->keyword, "STOP") == 0) {
            wiimu_log(1, 0, 0, 0, (char *)"[DOUBLE] !!!!Amazon Result: %s score:%d len_ms:%d",
                      result->keyword, score, len_ms);
            if (g_wiimu_shm->internet == 0) {
                if (g_wiimu_shm->alert_status)
                    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, "ALERT+LOCAL+DEL",
                                             strlen("ALERT+LOCAL+DEL"), NULL, NULL, 0);
                SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:stop",
                                         strlen("setPlayerCmd:stop"), NULL, NULL, 0);
            }
        } else {
            printf("[DOUBLE] !!!!!!!!!!!!Unknown wakeup word %s score:%d len_ms:%d\n",
                   result->keyword, score, len_ms);
        }
    } else {
        wiimu_log(1, 0, 0, 0, (char *)"[DOUBLE] Amazon Result: %s already waked count(%d) "
                                      "asr_ongoing(%d) score:%d len_ms:%d\n",
                  result->keyword, count, g_wiimu_shm->asr_ongoing, score, len_ms);
    }
}

// VAD event callback
void vadCallback_double(const PryonLiteVadEvent *vadEvent)
{
    printf("[DOUBLE] VAD state %d\n", (int)vadEvent->vadState);
}

static int file_size_read(const char *filename)
{
    int file_size = 0;
    if (filename == NULL)
        return -1;
    FILE *fd_file = fopen(filename, "rb");
    if (NULL == fd_file) {
        printf("%s file open %s error\n", filename, __FUNCTION__);
        return -1;
    }
    fseek(fd_file, 0L, SEEK_END);
    file_size = ftell(fd_file);
    if (fd_file)
        fclose(fd_file);
    return file_size;
}

static int file_exist(const char *filename)
{
    if (access(filename, R_OK) == 0)
        return 1;
    else
        return 0;
}

static void dump_locale_info(void)
{
    int locale_count = sizeof(locale_info_tbl_double) / sizeof(locale_info_tbl_double[0]);
    int i = 0;
    for (i = 0; i < locale_count; ++i) {
        printf("%.2d %.30s %10s %d %s\n", i, locale_info_tbl_double[i].locale_key,
               locale_info_tbl_double[i].model_path, locale_info_tbl_double[i].threshold,
               file_exist((char *)locale_info_tbl_double[i].model_path) ? " " : "(missing)");
    }
}

static void loadModel(PryonLiteDecoderConfig *config)
{
    // In order to detect keywords, the wakeword engine uses a model which defines the parameters,
    // neural network weights, classifiers, etc that are used at runtime to process the audio
    // and give detection results.    // Each model is packaged in two formats:
    // 1. A .bin file that can be loaded from disk (via fopen, fread, etc)
    // 2. A .cpp file that can be hard-coded at compile time
    char *lan = asr_get_language();
    char *model_path = NULL;
    int model_size = 0;
    int read_len = 0;
    FILE *fb_model = NULL;
    int locale_count = sizeof(locale_info_tbl_double) / sizeof(locale_info_tbl_double[0]);
    int i = 0;

    dump_locale_info();
    if (lan) {
        for (i = 0; i < locale_count; ++i) {
            if (strcmp(lan, locale_info_tbl_double[i].locale_key) == 0 &&
                file_exist(locale_info_tbl_double[i].model_path)) {
                break;
            }
        }
    }

    // default US
    if (i >= locale_count)
        i = 0;

    model_size = file_size_read(locale_info_tbl_double[i].model_path);
    model_path = (char *)locale_info_tbl_double[i].model_path;
    config->detectThreshold = locale_info_tbl_double[i].threshold;
#if defined(A98_ALEXA_D_HARMANASTRA) || defined(A98_LGUPLUS_PORTABLE)
    g_wiimu_shm->asr_search_id = get_asr_search_id();
    config->detectThreshold = (1000 - (g_wiimu_shm->asr_search_id - 1) * 50);
#endif

    printf("[DOUBLE] ###loadModel SDK %s lan=%s path=%s size=%d+++++\n", WWE_SDK_VERSION, lan,
           model_path, model_size);

    if (model_size > 0) {
        fb_model = fopen(model_path, "rb");
        if (model_buf != NULL) {
            free(model_buf);
            model_buf = NULL;
        }
        model_buf = (unsigned char *)calloc(1, model_size);
        if (model_buf) {

            if (fb_model)
                read_len = fread(model_buf, 1, model_size, fb_model); // Read file to buffer
            if (read_len <= 0) {
                free(model_buf);
                model_buf = NULL;
                printf("%s read failed\n", model_path);
                goto end;
            }
        } else {
            model_buf = NULL;
            printf("[DOUBLE] model_buf calloc failed\n");
            goto end;
        }
    }

    config->sizeofModel =
        model_size;            // example value, will be the size of the binary model byte array
    config->model = model_buf; // pointer to model in memory

end:
    if (fb_model)
        fclose(fb_model);
}

int CCaptCosume_initAsr_Amazon_double(void *priv)
{
    wiimu_log(1, 0, 0, 0, (char *)"%s++\n", __FUNCTION__);
    pthread_mutex_lock(&mutex_YDserver_op);
    // config = PryonLiteDecoderConfig_Default;
    // memset((void *)&config, 0, sizeof(config));
    loadModel(&config);

    // Query for the size of instance memory required by the decoder
    PryonLiteModelAttributes modelAttributes;
    PryonLiteError err =
        PryonLite_GetModelAttributes(config.model, config.sizeofModel, &modelAttributes);
    config.decoderMem = (char *)malloc(modelAttributes.requiredDecoderMem);
    if (config.decoderMem == NULL) {
        // handle error
        pthread_mutex_unlock(&mutex_YDserver_op);
        return -1;
    }
    config.sizeofDecoderMem = modelAttributes.requiredDecoderMem;

    // config.detectThreshold = get_threshold(); // default threshold
    config.resultCallback = detectionCallback_double; // register detection handler
    // config.vadCallback = vadCallback; // register VAD handler
    // config.useVad = true;  // disable voice activity detector
    config.vadCallback = NULL; // register VAD handler
    config.useVad = false;     // disable voice activity detector
    config.lowLatency = 1;

    err = PryonLiteDecoder_Initialize(&config, &sessionInfo, &sDecoder_double);
    if (err != PRYON_LITE_ERROR_OK) {
        free(config.decoderMem);
        wiimu_log(1, 0, 0, 0, (char *)"PryonLiteDecoder_Initialize failed err=%d\n", err);
    } else
        wiimu_log(1, 0, 0, 0, (char *)"PryonLiteDecoder_Initialize succeed, samplesPerFrame=%d "
                                      "detectThreshold=%d=====\n",
                  sessionInfo.samplesPerFrame, config.detectThreshold);
    pthread_mutex_unlock(&mutex_YDserver_op);
    wiimu_log(1, 0, 0, 0, (char *)"%s config.lowLatency=%d--\n", __FUNCTION__, config.lowLatency);
    return 0;
}

int CCaptCosume_deinitAsr_Amazon_double(void *priv)
{
    wiimu_log(1, 0, 0, 0, (char *)"%s++\n", __FUNCTION__);
    pthread_mutex_lock(&mutex_YDserver_op);
    PryonLiteError err = PryonLiteDecoder_Destroy(&sDecoder_double);
    if (err != PRYON_LITE_ERROR_OK) {
        if (config.decoderMem) {
            free(config.decoderMem);
            config.decoderMem = NULL;
        }
        pthread_mutex_unlock(&mutex_YDserver_op);
        return -1;
    }

    if (config.decoderMem) {
        free(config.decoderMem);
        config.decoderMem = NULL;
    }

#ifdef AVS_CLOUD_BASED_WWV
    g_cosume_asr_totlen_double = 0;
    g_cosume_asr_last_totlen_double = 0;
#endif
    pthread_mutex_unlock(&mutex_YDserver_op);

    wiimu_log(1, 0, 0, 0, (char *)"%s--\n", __FUNCTION__);
    return 0;
}

#endif
#endif
