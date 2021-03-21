
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"

#include "ccaptureGeneral.h"
#include "wm_util.h"

#define MIN_WAKEUP_SCORE 200
#define MIN_WKEUP_DELTA_TIME 500 // 500ms

extern WIIMU_CONTEXT *g_wiimu_shm;
#if defined(YD_SENSORY_MODULE) && defined(SENSORY_DOUBLE_TRIGGER)

#include "trulyhandsfree.h"

#ifdef MOVE_VOICE_PORMPT_TO_USER
#define SENSORY_RES_PATH "/vendor/misc/Voice-prompt/sensory/trigger2"
#else
#define SENSORY_RES_PATH "/system/workdir/misc/Voice-prompt/sensory/trigger2"
#endif

//#define DEBUG_DUMP_YD
static thf_t *g_double_session = NULL;
static recog_t *g_double_recog = NULL;
static searchs_t *g_double_search = NULL;
// pronuns_t *g_pronun=NULL;
static unsigned short g_double_yd_status = 0;

// hello blue genie 27K
#define HBG_27K_PREFIX SENSORY_RES_PATH "/sensory_demo_hbg_en_us_sfs_delivery04_pruned"

// hello blue genie 168K
#define HBG_168K_PREFIX SENSORY_RES_PATH "/sensory_demo_hbg_en_us_fbank_delivery01"

// ni hao xiao zhi 136K
#define NHXZ_136K_PREFIX SENSORY_RES_PATH "/ly_x_ni_hao_xiao_zhi_zh_sfs14_b24551b2_delivery01"

// xiao du xiao du 290K
#define XDXD_290K_PREFIX SENSORY_RES_PATH "/bu_x_xiaoduxiaodu_zh_sfs14_b2f83a38_delivery01"

// alexa 2016-09-26 135K
#define ALEXA_135K_PREFIX SENSORY_RES_PATH "/amazon_alexa_135_en_US_sfs14_delivery01"

// alexa 2016-12-16 1062k
#define ALEXA_1062K_PREFIX SENSORY_RES_PATH "/amazon_alexa_1062_en_US_sfs14_delivery01"

// alexa 2016-12-16 289k
#define ALEXA_289K_PREFIX SENSORY_RES_PATH "/amazon_alexa_289_en_US_sfs14_delivery01"

// alexa 2016-09-26 2M
//#define ALEXA_2M_PREFIX       SENSORY_RES_PATH "/sensory_alexa_sfs14"

// alexa 2016-09-26 1M
//#define ALEXA_1M_PREFIX       SENSORY_RES_PATH "/sensory_alexa_1062_sfs14"

// hey clova 2M
#define HEY_CLOVA_PREFIX                                                                           \
    SENSORY_RES_PATH "/hey_clova/naver_x_hey_clova_koKR_sfs14_b3e3cbba_delivery01"

// clova 2M
#define CLOVA_2M_PREFIX SENSORY_RES_PATH "/lgu_x_clova_koKR_sfs14_b3e3cbba_delivery03"

// yupeultibi 2M
#define YUPEULTIBI_2M_PREFIX SENSORY_RES_PATH "/lgu_x_yupeultibi_ko_sfs14_b3e3cbba_delivery04"

// aladdin 2M
#define ALADDIN_2M_PREFIX SENSORY_RES_PATH "/lgu_x_aladdin_koKR_sfs14_b3e3cbba_delivery03"

#if defined(BAIDU_DUMI)
#define SENSORY_NETFILE XDXD_290K_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE XDXD_290K_PREFIX "_search_%d.raw"
#elif defined(NAVER_LINE) && defined(LGUPLUS_IPTV_ENABLE)
#define SENSORY_NETFILE YUPEULTIBI_2M_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE YUPEULTIBI_2M_PREFIX "_search_%d.raw"
#elif defined(UPLUS_LINE)
//#define SENSORY_NETFILE     YUPEULTIBI_2M_PREFIX "_am.raw"
//#define SENSORY_SEARCHFILE      YUPEULTIBI_2M_PREFIX "_search_%d.raw"
#define SENSORY_NETFILE ALADDIN_2M_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE ALADDIN_2M_PREFIX "_search_%d.raw"
#elif defined(ASR_AMAZON_MODULE)
#define SENSORY_NETFILE ALEXA_289K_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE ALEXA_289K_PREFIX "_search_%d.raw"
#else
#define SENSORY_NETFILE ALEXA_289K_PREFIX "_am.raw"
#define SENSORY_SEARCHFILE ALEXA_289K_PREFIX "_search_%d.raw"
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

static pthread_mutex_t mutex_YDserver_double_op = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG_DUMP_YD
static FILE *fpYdDump = NULL;
static long dump_file_size = 0;
static long dump_max_file_size = 0;
#define DUMP_YD_SENSORY_FILE "/tmp/yd_sensory2.wav"
#define DUMP_SENSORY_OK_WAV_FILE "/tmp/web/sensory2_ok.wav"
#define DUMP_SENSORY_FAIL_WAV_FILE "/tmp/web/sensory2_fail.wav"
static long sensory_mistake_ts = 0;
#endif
extern int asr_waked;
extern int get_asr_search_id2(void);
extern void begin_wave(int fd, size_t cnt, int channels);

int CCaptCosume_YD_sensory_double(char *buf, int size, void *priv);
int CCaptCosume_initYD_sensory_double(void *priv);
int CCaptCosume_deinitYD_sensory_double(void *priv);

CAPTURE_COMUSER Cap_Cosume_double_AIYD_sensory = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_YD_sensory_double,
    .cCapture_init = CCaptCosume_initYD_sensory_double,
    .cCapture_deinit = CCaptCosume_deinitYD_sensory_double,
#if defined(HOBOT_DUAL_CHANNEL_TRIGGER) && defined(SOFT_PREP_HOBOT_MODULE)
    .id = AUDIO_ID_WWE_DUAL_CH,
#else
    .id = AUDIO_ID_WWE,
#endif
};

static int parse_keyword_length_ms(char *word_str)
{
    char *ptr_keyword_start = NULL;
    char *ptr_keyword_end = NULL;
    char *pfind = NULL;
    long keyword_start_ms = 0;
    long keyword_end_ms = 0;
    int keyword_ms = 0;

    ptr_keyword_start = word_str;
    if (ptr_keyword_start) {
        pfind = strstr(ptr_keyword_start, " ");
        if (pfind) {
            ptr_keyword_end = pfind + 1;
            *pfind = '\0';
            keyword_start_ms = atol(ptr_keyword_start);
            if (ptr_keyword_end) {
                pfind = strstr(ptr_keyword_end, " ");
                if (pfind) {
                    *pfind = '\0';
                    keyword_end_ms = atol(ptr_keyword_end);
                    if (keyword_end_ms > keyword_start_ms) {
                        keyword_ms = keyword_end_ms - keyword_start_ms;
                        wiimu_log(1, 0, 0, 0, "keyword length %d [%ld, %ld] ms\r\n", keyword_ms,
                                  keyword_start_ms, keyword_end_ms);
                    }
                }
            }
        }
    }

    return keyword_ms;
}

int CCaptCosume_YD_sensory_double(char *buf, int size, void *priv)
{
    long pcmCount = 0;
    long pcmSize = 0;
    int detect = 0;
    int ret = 0;

#ifdef DEBUG_DUMP_YD
    if (g_wiimu_shm->asr_trigger_mis_test_on) {
        long ts = tickSincePowerOn() / 1000;
        if (access(DUMP_SENSORY_FAIL_WAV_FILE, 0) == 0)
            (void)remove(DUMP_SENSORY_FAIL_WAV_FILE);
        if (sensory_mistake_ts &&
            ts < sensory_mistake_ts + (g_wiimu_shm->asr_yd_test_interval * 2)) {
            usleep(15000);
            return size;
        }
        sensory_mistake_ts = 0;
        dump_max_file_size = g_wiimu_shm->asr_yd_test_interval * SAMPLERATE * 2;
    } else if (g_wiimu_shm->asr_trigger_test_on) {
        if (access(DUMP_SENSORY_OK_WAV_FILE, 0) == 0)
            remove(DUMP_SENSORY_OK_WAV_FILE);
        dump_max_file_size = 10 * SAMPLERATE * 2;
    } else {
        if (access(DUMP_SENSORY_FAIL_WAV_FILE, 0) == 0)
            remove(DUMP_SENSORY_FAIL_WAV_FILE);
        if (access(DUMP_SENSORY_OK_WAV_FILE, 0) == 0)
            remove(DUMP_SENSORY_OK_WAV_FILE);
        pthread_mutex_lock(&mutex_YDserver_double_op);
        if (fpYdDump) {
            end_wave(fpYdDump, 100);
            (void)remove(DUMP_YD_SENSORY_FILE);
            fpYdDump = 0;
        }
        pthread_mutex_unlock(&mutex_YDserver_double_op);
    }
    sensory_mistake_ts = 0;
#endif

    if (!Cap_Cosume_double_AIYD_sensory.enable || !g_double_session ||
        !g_double_recog) { // || CheckPlayerStatus(g_wiimu_shm) )
        // usleep(15000);
        return size;
    }

    pcmSize = size;

    // long long start_ts = tickSincePowerOn();
    while (g_double_session && pcmSize) {
        unsigned int len = 960;
        if (pcmSize < len)
            len = pcmSize;

        pthread_mutex_lock(&mutex_YDserver_double_op);
        g_double_yd_status = 0;
        // thfResetLastError(g_double_session);
        ret = thfRecogPipe(g_double_session, g_double_recog, (unsigned short)len / 2,
                           (short *)(buf + pcmCount), RECOG_ONLY,
                           &g_double_yd_status); // SDET_RECOG

#ifdef DEBUG_DUMP_YD
        if (g_wiimu_shm->asr_trigger_test_on || g_wiimu_shm->asr_trigger_mis_test_on) {
            if (g_wiimu_shm->asr_yd_recording) {
                if (!fpYdDump) {
                    fpYdDump = open64(DUMP_YD_SENSORY_FILE, O_RDWR | O_CREAT, 0644);
                    if (fpYdDump == -1) {
                        printf("create new sensory YD record file after detect speech failed\n");
                        fpYdDump = 0;
                    } else {
                        begin_wave(fpYdDump, 2147483648LL, 1);
                        dump_file_size = 0;
                    }
                }

                if (fpYdDump) {
                    if (write(fpYdDump, buf + pcmCount, len) != len)
                        printf("write yd sensory dump data to file failed\n");
                    dump_file_size += len;
                }
            }
        }
#endif
        pthread_mutex_unlock(&mutex_YDserver_double_op);

        if (RECOG_DONE == g_double_yd_status) {
            float score = 0;
            const char *rres = NULL;
            const char *walign = NULL, *palign = NULL;
            ret = thfRecogResult(g_double_session, g_double_recog, &score, &rres, &walign, &palign,
                                 NULL, NULL, NULL, NULL);
            if (ret == 0)
                printf("thfRecogResult ERROR %s\n", thfGetLastError(g_double_session));
            else {
                wiimu_log(1, 0, 0, 0, "sensory Result: %s (%.3f)\n", rres, score);

                /* word alignments */
                if (walign) {
                    wiimu_log(1, 0, 0, 0, "walign %s\r\n", walign);
                    g_wiimu_shm->trigger2_len_ms = parse_keyword_length_ms(walign);
                    g_wiimu_shm->trigger1_len_ms = 0;
                } else {
                    g_wiimu_shm->trigger2_len_ms = 0;
                }

                /* phoneme alignments */
                if (palign) {
                    wiimu_log(1, 0, 0, 0, "palign %s\r\n", palign);
                }
            }

#if GENDER_DETECT
            {
                float genderProb = 0;
                if (!thfSpeakerGender(g_double_session, g_double_recog, &genderProb)) {
                    printf("ERROR thfSpeakerGender");
                } else {
                    if (genderProb < 0.5) {
                        printf("Gender: MALE (score=%f)\n", (1.0 - genderProb));
                    } else {
                        printf("Gender: FEMALE (score=%f)\n", genderProb);
                    }
                }
            }
#endif

            thfRecogReset(g_double_session, g_double_recog);
            if (ret) {
                g_wiimu_shm->times_trigger2++;
                detect = (g_wiimu_shm->asr_ongoing == 0);
#if defined(HOBOT_DUAL_CHANNEL_TRIGGER) && defined(SOFT_PREP_HOBOT_MODULE) ||                      \
    (defined(WWE_DOUBLE_TRIGGER))
                detect &=
                    ((tickSincePowerOn() - g_wiimu_shm->asr_wakeup_tick) > MIN_WKEUP_DELTA_TIME);
                g_wiimu_shm->asr_wakeup_tick = tickSincePowerOn();
#endif
                if (detect) {
                    SocketClientReadWriteMsg(
                        "/tmp/RequestIOGuard", (char *)"GNOTIFY=WWE_TRIGGED:DOUBLE_SENSORY",
                        strlen("GNOTIFY=WWE_TRIGGED:DOUBLE_SENSORY"), NULL, NULL, 0);
                    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:4",
                                             strlen("talkstart:4"), NULL, NULL, 0);
#if defined(AVS_CLOUD_BASED_WWV)
                    asr_waked = 1;

#endif
                }
            }

#ifdef DEBUG_DUMP_YD
            pthread_mutex_lock(&mutex_YDserver_double_op);
            if (fpYdDump) {
                end_wave(fpYdDump, dump_file_size);

                if (score <= g_wiimu_shm->asr_yd_config_score && g_wiimu_shm->asr_trigger_test_on) {
                    printf("Sensory trigger with score %f, save dump file size %d\n", score,
                           dump_file_size);
                    rename(DUMP_YD_SENSORY_FILE, DUMP_SENSORY_FAIL_WAV_FILE);
                } else if (score > g_wiimu_shm->asr_yd_config_score &&
                           g_wiimu_shm->asr_trigger_mis_test_on) {
                    printf("Sensory mis trigger with score %f, save dump file size %d\n", score,
                           dump_file_size);
                    rename(DUMP_YD_SENSORY_FILE, DUMP_SENSORY_OK_WAV_FILE);
                    sensory_mistake_ts = tickSincePowerOn() / 1000;
                } else {
                    remove(DUMP_YD_SENSORY_FILE);
                }

                if (g_wiimu_shm->asr_trigger_test_on)
                    g_wiimu_shm->asr_yd_recording = 0;

                fpYdDump = open64(DUMP_YD_SENSORY_FILE, O_RDWR | O_CREAT, 0644);
                if (fpYdDump == -1) {
                    printf("create new sensory YD record file after detect speech failed\n");
                    fpYdDump = 0;
                } else {
                    begin_wave(fpYdDump, 2147483648LL, 1);
                    dump_file_size = 0;
                }
            }
            pthread_mutex_unlock(&mutex_YDserver_double_op);
#endif
            g_wiimu_shm->wakeup_score = (int)score;
            break;
        }

        // printf("CCaptCosume_YD_sensory--\n");
        if (ret == 0) {
            printf("ERROR thfRecogPipe %s\n", thfGetLastError(g_double_session));
            // CCaptCosume_deinitYD_sensory(&Cap_Cosume_double_AIYD_sensory);
            // CCaptCosume_initYD_sensory(&Cap_Cosume_double_AIYD_sensory);
            break;
        }

        pcmCount += (long)len;
        pcmSize -= (long)len;
        // usleep(15000);
    }

#ifdef DEBUG_DUMP_YD
    pthread_mutex_lock(&mutex_YDserver_double_op);
    if (fpYdDump && dump_file_size >= dump_max_file_size) {
        end_wave(fpYdDump, dump_file_size);
        if (g_wiimu_shm->asr_trigger_test_on) {
            printf("Sensory trigger timeout save dump file size %d (%d)\n", dump_file_size,
                   dump_max_file_size);
            if (rename(DUMP_YD_SENSORY_FILE, DUMP_SENSORY_FAIL_WAV_FILE) < 0) {
                pthread_mutex_unlock(&mutex_YDserver_double_op);
                return size;
            }
            g_wiimu_shm->asr_yd_recording = 0;
        } else {
            remove(DUMP_YD_SENSORY_FILE);
        }
        fpYdDump = open64(DUMP_YD_SENSORY_FILE, O_RDWR | O_CREAT, 0644);
        if (fpYdDump == -1) {
            printf("create new sensory YD record file after reach max file size failed\n");
            fpYdDump = 0;
        } else {
            begin_wave(fpYdDump, 2147483648LL, 1);
            dump_file_size = 0;
        }
    }
    pthread_mutex_unlock(&mutex_YDserver_double_op);
#endif

    // long long end_ts = tickSincePowerOn();
    // if(end_ts > start_ts + size/128)
    //  wiimu_log(1,0,0,0, "CCaptCosume_YD_sensory size=%d (%d), cost = %lld", size, size/32,
    //  end_ts-start_ts);
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

int CCaptCosume_initYD_sensory_double(void *priv)
{
    printf("AIYD sensory double init ++\n");
    pthread_mutex_lock(&mutex_YDserver_double_op);

#ifdef DEBUG_DUMP_YD
    // if(fpYdDump)
    //{
    //  end_wave(fpYdDump);
    //  remove(DUMP_YD_SENSORY_FILE);
    //}
    // fpYdDump = open64(DUMP_YD_SENSORY_FILE, O_RDWR | O_CREAT, 0644);
    // if(fpYdDump == -1)
    //{
    //     printf("create new sensory YD record file failed\n");
    //  fpYdDump = 0;
    //}
    // else
    //{
    //  begin_wave(fpYdDump, 2147483648LL, 1);
    //  dump_file_size = 0;
    //}
    sensory_mistake_ts = 0;

    if (g_wiimu_shm->asr_yd_test_interval == 0)
        g_wiimu_shm->asr_yd_test_interval = 30;
    dump_max_file_size = (g_wiimu_shm->asr_yd_test_interval + 5) * SAMPLERATE * 2;
#endif

    if (g_wiimu_shm->asr_yd_config_score == 0)
        g_wiimu_shm->asr_yd_config_score = MIN_WAKEUP_SCORE;

    g_double_session = thfSessionCreate();

    if (!g_double_session)
        printf("ERROR thfSessionCreate %s\n", thfGetLastError(g_double_session));
    else {
        /* Display TrulyHandsfree SDK library version number */
        printf("TrulyHandsfree SDK Library version: %s\n", thfVersion());
        printf("Expiration date: %s\n", formatExpirationDate(thfGetLicenseExpiration()));
        printf("License feature: %s\n", thfGetLicenseFeatures());

        g_double_recog = thfRecogCreateFromFile(
            g_double_session, SENSORY_NETFILE,
            (unsigned short)(AUDIO_BUFFERSZ / 1000.f * SAMPLERATE), -1, SDET); // NO_SDET
        if (!g_double_recog) {
            printf("ERROR thfRecogCreateFromFile %s\n", thfGetLastError(g_double_session));
        } else {
            char search_file_path[256];
            memset((void *)search_file_path, 0, sizeof(search_file_path));
            snprintf(search_file_path, sizeof(search_file_path), SENSORY_SEARCHFILE,
                     get_asr_search_id2());
            g_double_search =
                thfSearchCreateFromFile(g_double_session, g_double_recog, search_file_path, 1);
            if (!g_double_search) {
                printf("ERROR thfSearchCreateFromFile %s\n", thfGetLastError(g_double_session));
            } else {
                printf("sensory search file %s\n", search_file_path);
#if 0
                if(!thfRecogConfigSet(g_double_session, g_double_recog, REC_KEEP_SDET_HISTORY, 0))
                    printf("ERROR-------------- thfRecogConfigSet:REC_KEEP_SDET_HISTORY\n");

                if(!thfRecogConfigSet(g_double_session, g_double_recog, REC_KEEP_FEATURE_HISTORY, 0))
                    printf("ERROR-------------- thfRecogConfigSet:REC_KEEP_FEATURE_HISTORY\n");

                if(!thfRecogConfigSet(g_double_session, g_double_recog, REC_KEEP_FEATURES, 0))
                    printf("ERROR-------------- thfRecogConfigSet:REC_KEEP_FEATURES\n");
#endif
                if (!thfPhrasespotConfigSet(g_double_session, g_double_recog, g_double_search,
                                            PS_DELAY, PHRASESPOT_DELAY))
                    printf("ERROR--------------------- thfPhrasespotConfigSet: PS_DELAY\n");

                if (!thfRecogConfigSet(g_double_session, g_double_recog, REC_MINDUR,
                                       450.0)) // discard the voice < 450ms
                    printf("ERROR--------------------- thfRecogConfigSet: REC_MINDUR\n");

                if (!thfSearchConfigSet(g_double_session, g_double_recog, g_double_search,
                                        SCH_GARBAGE,
                                        0.1)) // smaller garbage results in less fase-accept
                    printf("ERROR--------------------- thfSearchConfigSet: SCH_GARBAGE\n");

                if (!thfSearchConfigSet(g_double_session, g_double_recog, g_double_search, SCH_NOTA,
                                        0.9)) // larger nota  results in less fase-accept
                    printf("ERROR--------------------- thfSearchConfigSet: SCH_NOTA\n");

                if (!thfRecogInit(g_double_session, g_double_recog, g_double_search,
                                  RECOG_KEEP_WORD_PHONEME)) // RECOG_KEEP_WAVE_WORD_PHONEME
                    printf("ERROR-------------- thfRecogInit, %s \n",
                           thfGetLastError(g_double_session));

#if GENDER_DETECT
                /* initialize a speaker object with a single speaker,
                 * and arbitrarily set it up
                 * to hold one recording from this speaker
                 */
                if (!thfSpeakerInit(g_double_session, g_double_recog, 0, 1))
                    printf("ERROR--------- thfSpeakerInit\n");

                /* now read the gender model, which has been tuned to the target phrase */
                if (!thfSpeakerReadGenderModel(g_double_session, g_double_recog, HBG_GENDER_MODEL))
                    printf("ERROR--------- thfSpeakerReadGenderModel\n");
#endif
            }
        }
    }

    pthread_mutex_unlock(&mutex_YDserver_double_op);
    printf("AIYD sensory double init --\n");
    return -1;
}

int CCaptCosume_deinitYD_sensory_double(void *priv)
{
    printf("AIYD sensory double deinit ++\n");
    pthread_mutex_lock(&mutex_YDserver_double_op);
    if (g_double_recog)
        thfRecogDestroy(g_double_recog);
    g_double_recog = NULL;
    if (g_double_search)
        thfSearchDestroy(g_double_search);
    g_double_search = NULL;
    // if(g_pronun)
    //  thfPronunDestroy(g_pronun); g_pronun=NULL;
    if (g_double_session)
        thfSessionDestroy(g_double_session);
    g_double_session = NULL;

#ifdef DEBUG_DUMP_YD
    if (fpYdDump) {
        end_wave(fpYdDump);
        remove(DUMP_YD_SENSORY_FILE);
        fpYdDump = 0;
    }
#endif
    pthread_mutex_unlock(&mutex_YDserver_double_op);
    printf("AIYD sensory double deinit --\n");
    return 0;
}

void AIYD_Start_sensory_double(void)
{
    printf("AIYD sensory start_double ++\n");
    CCaptCosume_initYD_sensory_double(&Cap_Cosume_double_AIYD_sensory);
    CCaptRegister(&Cap_Cosume_double_AIYD_sensory);
    printf("AIYD sensory start_double --\n");
}

void AIYD_Stop_sensory_double(void)
{
    printf("AIYD sensory stop_double ++ \n");
    if (g_double_session)
        CCaptCosume_deinitYD_sensory(&Cap_Cosume_double_AIYD_sensory);
    CCaptUnregister(&Cap_Cosume_double_AIYD_sensory);
    printf("AIYD sensory stop_double -- \n");
}

extern void CCaptFlush(void);

void AIYD_enable_sensory_double(void)
{
    printf("AIYD sensory enable_double ++\n");
    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=MCUTLKON",strlen("GNOTIFY=MCUTLKON"),NULL,NULL,0);
    pthread_mutex_lock(&mutex_YDserver_double_op);
    Cap_Cosume_double_AIYD_sensory.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_double_op);
    printf("AIYD sensory enable_double --\n");
}

void AIYD_disable_sensory_double(void)
{
    printf("AIYD sensory disable_double ++\n");
    pthread_mutex_lock(&mutex_YDserver_double_op);
    Cap_Cosume_double_AIYD_sensory.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_double_op);
    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=MCUTLKOFF",strlen("GNOTIFY=MCUTLKOFF"),NULL,NULL,0);

    printf("AIYD sensory disable_double --\n");
}

#endif
