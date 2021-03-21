
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"

#include "ccaptureGeneral.h"
#include "wm_util.h"

#include <snsr.h>

#define MIN_WAKEUP_SCORE 200

extern WIIMU_CONTEXT *g_wiimu_shm;
#if defined(YD_SENSORY_MODULE)

#include "trulyhandsfree.h"

#ifdef MOVE_VOICE_PORMPT_TO_USER
#define SENSORY_RES_PATH "/vendor/misc/Voice-prompt/sensory/sample"
#else
#define SENSORY_RES_PATH "/system/workdir/misc/Voice-prompt/sensory/sample"
#endif
//#define DEBUG_DUMP_YD
SnsrSession g_session;

#define XIAOWEI_285K_PREFIX SENSORY_RES_PATH "/tt_x_hixiaoweis_zh_sfs14_a3ba1263_delivery01s"
// XIAOWEI SENSORY V6
#define SNSR_HIXIAOWEI_MODE SENSORY_RES_PATH "/trigger_lp_x_zhCN_02s_hixiaowei_sfs14_a51d9f15.snsr"
#define XIAOWEI_SEARCH_ID (3)

// hello blue genie 27K
#define HBG_27K_PREFIX SENSORY_RES_PATH "/sensory_demo_hbg_en_us_sfs_delivery04_pruned"

// hello blue genie 168K
#define HBG_168K_PREFIX SENSORY_RES_PATH "/sensory_demo_hbg_en_us_fbank_delivery01"

// alexa 2016-09-26 135K
#define ALEXA_135K_PREFIX SENSORY_RES_PATH "/amazon_alexa_135_en_US_sfs14_delivery01"

// alexa 2016-12-16 1062k
#define ALEXA_1062K_PREFIX SENSORY_RES_PATH "/amazon_alexa_1062_en_US_sfs14_delivery01"

// alexa 2016-12-16 289k
#define ALEXA_289K_PREFIX SENSORY_RES_PATH "/amazon_alexa_289_en_US_sfs14_delivery01"

#define HXW_PREFIX SENSORY_RES_PATH "/tt_x_hixiaowei_zh_sfs14_a3fab417_delivery01s"

#define SNSR_HEY_SPOTIFY                                                                           \
    SENSORY_RES_PATH                                                                               \
    "/spot-spy_x_heyokspotify_1mb_125kb_en_US_sfs16_f4b459ff_delivery01s_am_so50.snsr"
#define HEY_SPOTIFY_SEARCH_ID (3)

#define SNSR_HEY_EINSTEIN SENSORY_RES_PATH "/sensory-thf-enUS-heyeinstein-e3441105e.snsr"
#define HEY_EINSTEIN_SEARCH_ID (3)

#if defined(NAVER_LINE)
// hey clova
#define NAVER_HEY_CLOVA_SNSR                                                                       \
    SENSORY_RES_PATH "/hey_clova/naver_x_hey_clova_koKR_sfs16_f4acfe99_delivery08p_am.snsr"
#define NAVER_HEY_CLOVA_SEARCH_ID (3)

// jjangguya
#define NAVER_JJANGGUYA_SNSR SENSORY_RES_PATH "/jjangguya/trigger_nr_x_koKR_01_jjangguya_sfs14.snsr"
#define NAVER_JJANGGUYA_SEARCH_ID (2)

// clova
#define NAVER_CLOVA_SNSR SENSORY_RES_PATH "/clova/nr_x_clova_ko_sfs14_b3e3cbba_delivery07_am.raw"
#define NAVER_CLOVA_SEARCH_ID (4)

// ddokddoka
#define NAVER_DDOKDDOKA_SNSR                                                                       \
    SENSORY_RES_PATH "/ddokddoka/lgu_x_ddokddoka_ko_sfs14_b3e3cbba_delivery01_am.raw"
#define NAVER_DDOKDDOKA_SEARCH_ID (3)

// jesika
#define NAVER_JESIKA_SNSR                                                                          \
    SENSORY_RES_PATH "/jesika/naver_x_jesika_ko_sfs14_b3e3cbba_delivery02_am.raw"
#define NAVER_JESIKA_SEARCH_ID (4)

// pinokio
#define NAVER_PINOKIO_SNSR                                                                         \
    SENSORY_RES_PATH "/pinokio/naver_x_pinokio_ko_sfs14_b3e3cbba_delivery02_am.raw"
#define NAVER_PINOKIO_SEARCH_ID (1)

// seliya
#define NAVER_SELIYA_SNSR SENSORY_RES_PATH "/seliya/nr_x_seliya_ko_sfs14_b3e3cbba_delivery01_am.raw"
#define NAVER_SELIYA_SEARCH_ID (3)

// Hi LG
#define NAVER_HI_LG_SNSR SENSORY_RES_PATH "/hi_lg/trigger_lge_x_koKR_02s_hi_lg_sfs16_f51d9f15.snsr"
#define NAVER_HI_LG_SEARCH_ID (8)
#endif

#define SNSR_HELLO_LIFEPOD_MODE SENSORY_RES_PATH "/sensory-thf-enUS-hellolifepod-edae77c87.snsr"
#define HELLO_LIFEPOD_SEARCH_ID (8)

#ifdef ASR_AMAZON_MODULE
#if 1
#define ASR_SEARCH_ID_DEFAULT 6
//#define SENSORY_NETFILE           HXW_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE SNSR_HIXIAOWEI_MODE
#else
#define ASR_SEARCH_ID_DEFAULT 13
#define SENSORY_NETFILE ALEXA_289K_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE ALEXA_289K_PREFIX "_search_%d.raw"
#endif
#else
#define ASR_SEARCH_ID_DEFAULT 4
#define SENSORY_NETFILE HBG_168K_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE HBG_168K_PREFIX "_search_%d.raw"
#endif

#define NBEST 3
#define AUDIO_BUFFERSZ 250
#define SAMPLERATE 16000
//#define NUMCHANNELS 1
//#define PHRASESPOT_PARAMA_OFFSET   (0)        /* Phrasespotting ParamA Offset */
//#define PHRASESPOT_BEAM    (100.f)            /* Pruning beam */
//#define PHRASESPOT_ABSBEAM (100.f)            /* Pruning absolute beam */

#define GENDER_DETECT 0

#if GENDER_DETECT
#define HBG_GENDER_MODEL "/system/workdir/misc/sensory/sample/hbg_genderModel.raw"
#define PHRASESPOT_DELAY 15 /* Phrasespotting Delay */
#else
#define PHRASESPOT_DELAY PHRASESPOT_DELAY_ASAP /* Phrasespotting Delay */
#endif

static pthread_mutex_t mutex_YDserver_op = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG_DUMP_YD
static FILE *fpYdDump = NULL;
long dump_file_size = 0;
long dump_max_file_size = 0;
#define DUMP_YD_SENSORY_FILE "/tmp/yd_sensory.wav"
#define DUMP_SENSORY_OK_WAV_FILE "/tmp/web/sensory_ok.wav"
#define DUMP_SENSORY_FAIL_WAV_FILE "/tmp/web/sensory_fail.wav"
long sensory_mistake_ts = 0;
#endif

typedef struct {
    char *data;
    size_t dataSize;
    size_t index;
} ProviderData;

static SnsrRC streamOpen(SnsrStream stream)
{
    ProviderData *instanceData = snsrStream_getData(stream);
    if (!instanceData->data)
        return SNSR_RC_ERROR;
    instanceData->index = 0;
    return SNSR_RC_OK;
}

static SnsrRC streamClose(SnsrStream stream)
{
    ProviderData *instanceData = snsrStream_getData(stream);
    instanceData->index = 0;
    return SNSR_RC_OK;
}

static void streamRelease(SnsrStream stream)
{
    ProviderData *instanceData = snsrStream_getData(stream);
    free(instanceData);
}

static size_t streamRead(SnsrStream stream, void *buffer, size_t readSize)
{
    /* NOTE: For a live audio stream, if there is no data available,
     * this call should block until there is more data available.
     */
    ProviderData *d = snsrStream_getData(stream);
    size_t available, read;

    read = readSize;
    available = d->dataSize - d->index;
    if (read > available) {
        read = available;
        /* Session will end with SNSR_RC_STREAM_END */
        snsrStream_setRC(stream, SNSR_RC_EOF);
    }
    if (read)
        memcpy(buffer, d->data + d->index, read);
    d->index += read;
    return read;
}

static size_t streamWrite(SnsrStream stream, const void *buffer, size_t writeSize)
{
    /* NOTE: For a live audio stream, implementing a streamWrite
     * would make no sense and this function should be removed.
     */
    ProviderData *d = snsrStream_getData(stream);
    size_t available, written;

    written = writeSize;
    available = d->dataSize - d->index;
    if (written > available) {
        written = available;
        /* Session will end with SNSR_RC_STREAM_END */
        snsrStream_setRC(stream, SNSR_RC_EOF);
    }
    if (written)
        memcpy(d->data + d->index, buffer, written);
    d->index += written;
    return written;
}

static SnsrRC showAvailablePoint(SnsrSession s, const char *key, void *privateData)
{
    SnsrRC r;
    int point, *first = (int *)privateData;
    r = snsrGetInt(s, SNSR_RES_AVAILABLE_POINT, &point);
    if (r == SNSR_RC_OK) {
        if (*first)
            printf("Available operating points: %i", point);
        else
            printf(", %i", point);
        *first = 0;
    }
    return r;
}

/* This is the interface any SnsrStream has to provide
 * (Virtual Method Table)
 */
static SnsrStream_Vmt methods = {"data",         &streamOpen, &streamClose,
                                 &streamRelease, &streamRead, &streamWrite};

/* This is the 'constructor' for this kind of stream */
static SnsrStream streamFromData(void *data, size_t dataSize, SnsrStreamMode mode)
{
    SnsrStream dataStream;
    int readable = (mode == SNSR_ST_MODE_READ);
    int writeable = (mode == SNSR_ST_MODE_WRITE);
    /* The stream object has instance data (particular to this instance)
     * and virtual method pointers (particular to this type)
     * just like it would in C++
     */
    ProviderData *instanceData = malloc(sizeof(*instanceData));
    memset(instanceData, 0, sizeof(*instanceData));
    instanceData->data = data;
    instanceData->dataSize = dataSize;

    dataStream = snsrStream_alloc(&methods, instanceData, readable, writeable);
    if (data == NULL)
        snsrStream_setRC(dataStream, SNSR_RC_INVALID_ARG);
    return dataStream;
}

extern int asr_waked;
static SnsrRC resultEvent(SnsrSession s, const char *key, void *privateData)
{
    SnsrRC r;
    const char *phrase;
    double begin, end;
    int detect = 0;

    /* Retrieve the phrase text and alignments from the session handle */
    snsrGetDouble(s, SNSR_RES_BEGIN_SAMPLE, &begin);
    snsrGetDouble(s, SNSR_RES_END_SAMPLE, &end);
    r = snsrGetString(s, SNSR_RES_TEXT, &phrase);
    /* Quit early if an error occurred. */
    if (r != SNSR_RC_OK)
        return r;
    printf("Spotted \"%s\" from sample %.0f to sample %.0f.\n", phrase, begin, end);

    g_wiimu_shm->trigger1_len_ms = (end - begin) / 16;
    g_wiimu_shm->trigger2_len_ms = 0;
    g_wiimu_shm->times_trigger1++;

#if defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    detect = 1;
#else
    detect = (g_wiimu_shm->asr_ongoing == 0);
#endif
#if (defined(HOBOT_DUAL_CHANNEL_TRIGGER) && defined(SOFT_PREP_HOBOT_MODULE)) ||                    \
    (defined(WWE_DOUBLE_TRIGGER))
    detect &= ((tickSincePowerOn() - g_wiimu_shm->asr_wakeup_tick) > MIN_WKEUP_DELTA_TIME);
    g_wiimu_shm->asr_wakeup_tick = tickSincePowerOn();
#endif

    if (detect) {
        printf("--------wake up %s-------\n", phrase);
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"), NULL,
                                 NULL, 0);
#if defined(AVS_CLOUD_BASED_WWV) || defined(ASR_WAKEUP_TEXT_UPLOAD)
        asr_waked = 1;
#endif
    }

    return SNSR_RC_OK;
}

/* Push one audio buffer through the recognition pipeline.
 */

static SnsrRC processAudioChunk(SnsrSession session, short *buffer, size_t samples)
{
    /* Create a SnsrStream wrapper for the audio segment and set
     * it as the SnsrSession input audio source. */
    snsrSetStream(session, SNSR_SOURCE_AUDIO_PCM,
                  snsrStreamFromMemory(buffer, sizeof(*buffer) * samples, SNSR_ST_MODE_READ));
    /* Run the pipeline. */
    if (snsrRun(session) == SNSR_RC_STREAM_END) {
        /* All the samples in m were processed. Clear return code. */
        snsrClearRC(session);
    }
    /* Report all other return codes. */
    return snsrRC(session);
}

int CCaptCosume_YD_sensory(char *buf, int size, void *priv);
int CCaptCosume_initYD_sensory(void *priv);
int CCaptCosume_deinitYD_sensory(void *priv);

CAPTURE_COMUSER Cap_Cosume_AIYD_sensory = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_YD_sensory,
    .cCapture_init = CCaptCosume_initYD_sensory,
    .cCapture_deinit = CCaptCosume_deinitYD_sensory,
    .id = AUDIO_ID_WWE,
};

int CCaptCosume_YD_sensory(char *buf, int size, void *priv)
{
    long pcmCount = 0;
    long pcmSize = 0;
    SnsrRC r;

    if (!Cap_Cosume_AIYD_sensory.enable) {
        // usleep(15000);
        return size;
    }

    pcmSize = size;

    while (pcmSize) {
        unsigned int len = 320;
        if (pcmSize < len)
            len = pcmSize;

        pthread_mutex_lock(&mutex_YDserver_op);
        /* Process one block of audio. */
        r = processAudioChunk(g_session, (short *)(buf + pcmCount), len / sizeof(short));
        /* The result callback returns SNSR_RC_STOP when a phrase is spotted. */
        if (r == SNSR_RC_STOP) {
            fprintf(stderr, "ERROR1: %s\n", snsrErrorDetail(g_session));
            // break;
        }
        if (r != SNSR_RC_OK) {
            fprintf(stderr, "ERROR2: %s\n", snsrErrorDetail(g_session));
        }
        pthread_mutex_unlock(&mutex_YDserver_op);
#if 0
        if(read && g_wiimu_shm->asr_ongoing == 0) {
            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"), NULL, NULL, 0);
        }
#endif
        pcmCount += (long)len;
        pcmSize -= (long)len;
    }
    return size;
}

static char *formatExpirationDate(time_t expiration)
{
    static char expdate[33];
    if (!expiration)
        return "never";
    strftime(expdate, 32, "%m/%d/%Y 00:00:00 GMT", gmtime(&expiration));
    return expdate;
}

int CCaptCosume_initYD_sensory(void *priv)
{
    SnsrRC ret;
    char *expires = NULL;
    int search_id = 0;
    int first = 1;
    char snsr_model_file[512] = {0};

    printf("AIYD sensory init ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);

    if (g_wiimu_shm->asr_yd_config_score == 0)
        g_wiimu_shm->asr_yd_config_score = MIN_WAKEUP_SCORE;

#if defined(NAVER_LINE)
    get_asr_trigger1_keyword_index();
    if (g_wiimu_shm->trigger1_keyword_index == 0) {
        strncpy(snsr_model_file, NAVER_HEY_CLOVA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_HEY_CLOVA_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 1) {
        strncpy(snsr_model_file, NAVER_CLOVA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_CLOVA_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 2) {
        strncpy(snsr_model_file, NAVER_SELIYA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_SELIYA_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 3) {
        strncpy(snsr_model_file, NAVER_JESIKA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_JESIKA_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 4) {
        strncpy(snsr_model_file, NAVER_JJANGGUYA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_JJANGGUYA_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 5) {
        strncpy(snsr_model_file, NAVER_PINOKIO_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_PINOKIO_SEARCH_ID;
    } else if (g_wiimu_shm->trigger1_keyword_index == 6) {
        strncpy(snsr_model_file, NAVER_HI_LG_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_HI_LG_SEARCH_ID;
    } else {
        strncpy(snsr_model_file, NAVER_HEY_CLOVA_SNSR, sizeof(snsr_model_file) - 1);
        search_id = NAVER_HEY_CLOVA_SEARCH_ID;
    }
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    strncpy(snsr_model_file, SNSR_HIXIAOWEI_MODE, sizeof(snsr_model_file) - 1);
    search_id = XIAOWEI_SEARCH_ID;
#elif defined(A98_SALESFORCE)
    strncpy(snsr_model_file, SNSR_HEY_EINSTEIN, sizeof(snsr_model_file) - 1);
    search_id = HEY_EINSTEIN_SEARCH_ID;
#elif defined(A98_SPRINT_LIFEPOD)
    strncpy(snsr_model_file, SNSR_HELLO_LIFEPOD_MODE, sizeof(snsr_model_file) - 1);
    search_id = HELLO_LIFEPOD_SEARCH_ID;
#else
    strncpy(snsr_model_file, SNSR_HEY_SPOTIFY, sizeof(snsr_model_file) - 1);
    search_id = HEY_SPOTIFY_SEARCH_ID;
#endif

    /* Create a new session handle. */
    snsrNew(&g_session);
    /* Load and validate the spotter model task file. */
    snsrLoad(g_session, snsrStreamFromFileName(snsr_model_file, "r"));
    snsrRequire(g_session, SNSR_TASK_TYPE, SNSR_PHRASESPOT);
    snsrSetInt(g_session, SNSR_OPERATING_POINT, search_id);
    snsrGetString(g_session, SNSR_LICENSE_EXPIRES, (void *)&expires);
    printf("model:%s\n%s\n", snsr_model_file, expires ? expires : "never expire");
    snsrGetInt(g_session, SNSR_OPERATING_POINT, &search_id);
    printf("search_id = %d\n", search_id);
    snsrForEach(g_session, SNSR_OPERATING_POINT_LIST,
                snsrCallback(showAvailablePoint, NULL, &first));
    printf(".\n");

#if 0
    if(snsrRequire(g_session, SNSR_TASK_TYPE, SNSR_PHRASESPOT) != SNSR_RC_OK) {
        fprintf(stderr, "snsrRequire\n");
        /* If this is a phrasespot-vad task, wire up an output buffer */
        snsrClearRC(g_session);
        snsrRequire(g_session, SNSR_TASK_TYPE, SNSR_PHRASESPOT_VAD);

        /* Register VAD endpoint callbacks. */
        snsrSetHandler(g_session, SNSR_END_EVENT, snsrCallback(endEvent, NULL, NULL));
        snsrSetHandler(g_session, SNSR_LIMIT_EVENT, snsrCallback(endEvent, NULL, NULL));
        snsrSetHandler(g_session, SNSR_SILENCE_EVENT, snsrCallback(silenceEvent, NULL, NULL));
    }
#endif

    ret = snsrSetHandler(g_session, SNSR_RESULT_EVENT, snsrCallback(resultEvent, NULL, NULL));
    if (ret != SNSR_RC_OK) {
        fprintf(stderr, "snsrSetHandler %s\n", snsrErrorDetail(g_session));
    }

    /* Turn off automatic pipeline flushing that happens when the end of the
    * input stream is reached. */
    snsrSetInt(g_session, SNSR_AUTO_FLUSH, 0);

    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD sensory init --\n");

#ifdef CUP_TEST
    Cap_Cosume_AIYD_sensory.enable = 1;
    show_cpu_usage();
#endif
    return -1;
}

int CCaptCosume_deinitYD_sensory(void *priv)
{
    printf("AIYD sensory deinit ++++++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    snsrRelease(g_session);

#ifdef DEBUG_DUMP_YD
    if (fpYdDump) {
        end_wave(fpYdDump);
        remove(DUMP_YD_SENSORY_FILE);
        fpYdDump = 0;
    }
#endif
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD sensory deinit ------\n");
    return 0;
}

void AIYD_Start_sensory(void)
{
    printf("AIYD sensory start ++++++\n");
    CCaptCosume_initYD_sensory(&Cap_Cosume_AIYD_sensory);
    CCaptRegister(&Cap_Cosume_AIYD_sensory);
    printf("AIYD sensory start ------\n");
}

void AIYD_Stop_sensory(void)
{
    printf("AIYD sensory stop ++++++\n");
    if (g_session)
        CCaptCosume_deinitYD_sensory(&Cap_Cosume_AIYD_sensory);
    CCaptUnregister(&Cap_Cosume_AIYD_sensory);
    printf("AIYD sensory stop -- \n");
}

extern void CCaptFlush(void);

void AIYD_enable_sensory(void)
{
    printf("AIYD sensory enable ++++++\n");
    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=MCUTLKON",strlen("GNOTIFY=MCUTLKON"),NULL,NULL,0);
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_sensory.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD sensory enable ------\n");
}

void AIYD_disable_sensory(void)
{
    printf("AIYD sensory disable ++++++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_sensory.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=MCUTLKOFF",strlen("GNOTIFY=MCUTLKOFF"),NULL,NULL,0);

    printf("AIYD sensory disable ------\n");
}

#endif
