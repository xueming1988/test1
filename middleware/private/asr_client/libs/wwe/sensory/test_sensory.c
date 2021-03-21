
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

#include "trulyhandsfree.h"

#ifdef MOVE_VOICE_PORMPT_TO_USER
#define SENSORY_RES_PATH "/vendor/misc/Voice-prompt/sensory/sample"
#else
#define SENSORY_RES_PATH "/system/workdir/misc/Voice-prompt/sensory/sample"
#endif

//#define DEBUG_DUMP_YD
thf_t *g_session = NULL;
recog_t *g_recog = NULL;
searchs_t *g_search = NULL;
// pronuns_t *g_pronun=NULL;
unsigned short g_yd_status = 0;

// hello blue genus
//#define HBG_NETFILE               SENSORY_RES_PATH
//"/sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_am.raw"
//#define HBG_SEARCHFILE        SENSORY_RES_PATH
//"/sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_9.raw"

// alexa 27K
#define HBG_NETFILE SENSORY_RES_PATH "/amazon_alexa_135_en_US_sfs14_delivery01_am.raw"
//#define HBG_SEARCHFILE        SENSORY_RES_PATH "/sensory_alexa_sfs14_dsp_search.raw"
static char hbg_netfile[256];
static char hbg_searchfile[256];

// alexa 2M
//#define HBG_NETFILE               SENSORY_RES_PATH "/sensory_alexa_sfs14_am.raw"
//#define HBG_SEARCHFILE        SENSORY_RES_PATH "/sensory_alexa_sfs14_9.raw"

// alexa 1M
//#define HBG_NETFILE               SENSORY_RES_PATH "/sensory_alexa_1062_sfs14_am.raw"
//#define HBG_SEARCHFILE        SENSORY_RES_PATH "/sensory_alexa_1062_sfs14_9.raw"

#define NBEST 3
#define AUDIO_BUFFERSZ 250
#define SAMPLERATE 16000
//#define NUMCHANNELS 1
//#define PHRASESPOT_PARAMA_OFFSET   (0)        /* Phrasespotting ParamA Offset */
//#define PHRASESPOT_BEAM    (100.f)            /* Pruning beam */
//#define PHRASESPOT_ABSBEAM (100.f)            /* Pruning absolute beam */

#define TRIGGER_SCORE 200.0
#define GENDER_DETECT 0

#if GENDER_DETECT
#define HBG_GENDER_MODEL "/system/workdir/misc/sensory/sample/hbg_genderModel.raw"
#define PHRASESPOT_DELAY 15 /* Phrasespotting Delay */
#else
#define PHRASESPOT_DELAY                                                                           \
    PHRASESPOT_DELAY_ASAP // 64 //PHRASESPOT_DELAY_ASAP  /* Phrasespotting Delay */
#endif

static int _CCaptCosume_YD_sensory(char *buf, int size, void *priv);
static int _CCaptCosume_initYD_sensory(void *priv);
static int _CCaptCosume_deinitYD_sensory(void *priv);

static int _CCaptCosume_YD_sensory(char *buf, int size, void *priv)
{
    long pcmCount = 0;
    long pcmSize = 0;
    int ret = 0;

    pcmSize = size;

    // long long start_ts = tickSincePowerOn();
    while (g_session && pcmSize) {
        unsigned int len = 6400;
        // if (pcmSize < 6400)
        len = pcmSize;

        g_yd_status = 0;
        // thfResetLastError(g_session);
        ret = thfRecogPipe(g_session, g_recog, (unsigned short)len / 2, (short *)(buf + pcmCount),
                           RECOG_ONLY,
                           &g_yd_status); // SDET_RECOG

        if (RECOG_DONE == g_yd_status) {
            float score = 0;
            const char *rres = NULL;
            const char *wordAlign = NULL;
            const char *phoneAlign = NULL;
            ret = thfRecogResult(g_session, g_recog, &score, &rres, &wordAlign, &phoneAlign, NULL,
                                 NULL, NULL, NULL);
            if (ret == 0)
                printf("thfRecogResult ERROR %s\n", thfGetLastError(g_session));
            else {
                // wiimu_log(1, 0, 0, 0, "sensory Result: %s score(%.3f)wordAlign(%s)\n",rres,score,
                // wordAlign);
            }

#if GENDER_DETECT
            {
                float genderProb = 0;
                if (!thfSpeakerGender(g_session, g_recog, &genderProb)) {
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
            if (ret) {
                thfRecogReset(g_session, g_recog);
                return (int)score;
            }
        }

        // printf("CCaptCosume_YD_sensory--\n");
        if (ret == 0) {
            printf("ERROR thfRecogPipe %s\n", thfGetLastError(g_session));
            // CCaptCosume_deinitYD_sensory(&Cap_Cosume_AIYD_sensory);
            // CCaptCosume_initYD_sensory(&Cap_Cosume_AIYD_sensory);
            break;
        }

        pcmCount += (long)len;
        pcmSize -= (long)len;
        // usleep(15000);
    }

    // long long end_ts = tickSincePowerOn();
    // if(end_ts > start_ts + size/128)
    //  wiimu_log(1,0,0,0, "CCaptCosume_YD_sensory size=%d (%d), cost = %lld", size, size/32,
    //  end_ts-start_ts);
    return -1;
}

static char *formatExpirationDate(time_t expiration)
{
    static char expdate[33];
    if (!expiration)
        return "never";
    strftime(expdate, 32, "%m/%d/%Y 00:00:00 GMT", gmtime(&expiration));
    return expdate;
}

static int _CCaptCosume_initYD_sensory(void *priv)
{
// printf("AIYD sensory init ++\n");

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
    //  begin_wave(fpYdDump, 2147483648LL);
    //  dump_file_size = 0;
    //}
    sensory_mistake_ts = 0;

    if (g_wiimu_shm->asr_yd_test_interval == 0)
        g_wiimu_shm->asr_yd_test_interval = 30;
    dump_max_file_size = (g_wiimu_shm->asr_yd_test_interval + 5) * SAMPLERATE * 2;
#endif

    // if(g_wiimu_shm->asr_yd_config_score == 0)
    //  g_wiimu_shm->asr_yd_config_score = MIN_WAKEUP_SCORE;

    g_session = thfSessionCreate();

    if (!g_session)
        printf("ERROR thfSessionCreate %s\n", thfGetLastError(g_session));
    else {
        /* Display TrulyHandsfree SDK library version number */
        printf("TrulyHandsfree SDK Library version: %s\n", thfVersion());
        printf("Expiration date: %s\n", formatExpirationDate(thfGetLicenseExpiration()));
        printf("License feature: %s\n", thfGetLicenseFeatures());

        g_recog = thfRecogCreateFromFile(g_session, hbg_netfile,
                                         (unsigned short)(AUDIO_BUFFERSZ / 1000.f * SAMPLERATE), -1,
                                         SDET); // NO_SDET
        if (!g_recog) {
            printf("ERROR thfRecogCreateFromFile %s\n", thfGetLastError(g_session));
        } else {
            g_search = thfSearchCreateFromFile(g_session, g_recog, hbg_searchfile, 1);
            if (!g_search) {
                printf("ERROR thfSearchCreateFromFile %s\n", thfGetLastError(g_session));
            } else {
                if (!thfPhrasespotConfigSet(g_session, g_recog, g_search, PS_DELAY,
                                            PHRASESPOT_DELAY))
                    printf("ERROR--------------------- thfPhrasespotConfigSet: PS_DELAY\n");

#if 0
                thfRecogConfigSet(g_session, g_recog, REC_KEEP_SDET_HISTORY, 0);
                thfRecogConfigSet(g_session, g_recog, REC_KEEP_FEATURE_HISTORY, 0);
                thfRecogConfigSet(g_session, g_recog, REC_KEEP_FEATURES, 0);
#endif

#if 1
                if (!thfRecogConfigSet(g_session, g_recog, REC_MINDUR,
                                       450.0)) // discard the voice < 450ms
                    printf("ERROR--------------------- thfRecogConfigSet: REC_MINDUR\n");

                if (!thfSearchConfigSet(g_session, g_recog, g_search, SCH_GARBAGE,
                                        0.1)) // smaller garbage results in less fase-accept
                    printf("ERROR--------------------- thfSearchConfigSet: SCH_GARBAGE\n");

                if (!thfSearchConfigSet(g_session, g_recog, g_search, SCH_NOTA,
                                        0.9)) // larger nota  results in less fase-accept
                    printf("ERROR--------------------- thfSearchConfigSet: SCH_NOTA\n");
#endif
                if (!thfRecogInit(g_session, g_recog, g_search,
                                  RECOG_KEEP_WORD_PHONEME)) // RECOG_KEEP_WAVE_WORD_PHONEME
                    printf("ERROR-------------- thfRecogInit\n");

#if GENDER_DETECT
                /* initialize a speaker object with a single speaker,
                 * and arbitrarily set it up
                 * to hold one recording from this speaker
                 */
                if (!thfSpeakerInit(g_session, g_recog, 0, 1))
                    printf("ERROR--------- thfSpeakerInit\n");

                /* now read the gender model, which has been tuned to the target phrase */
                if (!thfSpeakerReadGenderModel(g_session, g_recog, HBG_GENDER_MODEL))
                    printf("ERROR--------- thfSpeakerReadGenderModel\n");
#endif
            }
        }
    }

    // printf("AIYD sensory init --\n");
    return -1;
}

static int _CCaptCosume_deinitYD_sensory(void *priv)
{
    if (g_recog)
        thfRecogDestroy(g_recog);
    g_recog = NULL;
    if (g_search)
        thfSearchDestroy(g_search);
    g_search = NULL;
    // if(g_pronun)
    //  thfPronunDestroy(g_pronun); g_pronun=NULL;
    if (g_session)
        thfSessionDestroy(g_session);
    g_session = NULL;

    return 0;
}

static void AIYD_Start_sensory(void) { _CCaptCosume_initYD_sensory(NULL); }

static void AIYD_Stop_sensory(void)
{
    if (g_session)
        _CCaptCosume_deinitYD_sensory(NULL);
}

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

static int do_sensory_test(int chunk_size, char *buf, int file_size)
{
    char *p = buf;
    int size = file_size;
    int ret = 0;
    volatile long long ts_start = tickSincePowerOn();
    while (size > 0) {
        volatile long long ts_start1 = tickSincePowerOn();
        if (chunk_size > size)
            chunk_size = size;
        ret = _CCaptCosume_YD_sensory(p, chunk_size, NULL);
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

static void save_wake_word(char *out_path, char *p, off_t offset, off_t file_size, off_t len,
                           off_t pre_bytes)
{
    off_t post_bytes = len - pre_bytes;
    if (pre_bytes > offset)
        pre_bytes = offset;
    if (post_bytes > (file_size - offset))
        post_bytes = (file_size - offset);

    char *new_path = strrchr(out_path, '/');
    if (new_path && strlen(new_path) > 0) {
        ++new_path;
    } else {
        new_path = out_path;
    }
    int fd = open(new_path, O_RDWR | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        printf("%s %s failed\n", __FUNCTION__, new_path);
        return;
    }

    char *buf = (p - pre_bytes);
    long long left = (post_bytes + pre_bytes);
    while (left > 0) {
        long long ret = write(fd, buf, left);
        if (ret <= 0)
            break;
        left -= ret;
        buf += ret;
    }
    close(fd);
}

// calc cpu usage, 100seconds data
static void show_cpu_usage(void)
{
    int i;
    int chunk_size = 320;
    char *pbuf = (char *)malloc(chunk_size);
    AIYD_Start_sensory();
    volatile long long start_ts = tickSincePowerOn() / 1000;

    i = (16000 * 2 * 100) / chunk_size;
    printf("CPU usage test i=%d\n", i);

    while (i-- > 0)
        _CCaptCosume_YD_sensory(pbuf, chunk_size, NULL);
    volatile long long delta = tickSincePowerOn() / 1000 - start_ts;
    printf("CPU usage %lld%%\n", delta);
    free(pbuf);
}

static int do_sensory_test_2(char *path)
{
    int chunk_size = 960;
    off_t file_size = get_file_len(path);
    off_t left = file_size;
    int ret = 0;
    char *p = NULL;
    char *start_addr = NULL;
    int wake_count = 0;
    int fd = -1;
    long file_len_ms = (file_size / (16000 * 2)) * 1000;
    long offset_ms = 0;
    int i = 1;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open %s failed\n", path);
        return -1;
    }

    start_addr = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (start_addr == (char *)-1) {
        printf("mmap failed, file_size=%ld\n", file_size);
        close(fd);
        return -1;
    }
    // save_wake_word("test.wav", start_addr, 0, file_size, file_size, 0);
    // return 0;

    wiimu_log(1, 0, 0, 0, "start test %s len=%u duration=%.2ld:%.2ld:%.2ld chunk=%d netfile %s",
              path, file_size, file_len_ms / 1000 / 3600, (file_len_ms / 1000 / 60) % 60,
              (file_len_ms / 1000) % 60, chunk_size, hbg_netfile);
    p = start_addr;
    AIYD_Start_sensory();
    long long start_ts = tickSincePowerOn() / 1000;

    while (left > 0 && p) {
        if (chunk_size > left)
            chunk_size = left;

#if 0 // show log every 10 seconds
        long long current_ts = tickSincePowerOn() / 1000;
        if((current_ts - start_ts) > 0 && (current_ts - start_ts) == i * 10) {
            offset_ms = file_len_ms - (left / (16000 * 2)) * 1000;
            wiimu_log(1, 0, 0, 0, "%d wake count %d score %d  offset=%.2ld:%.2ld:%.2ld",
                      i++, wake_count, ret, offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60, (offset_ms / 1000) % 60);
        }
#endif
        ret = _CCaptCosume_YD_sensory(p, chunk_size, NULL);
        if (ret >= 0) {
            char out_path[256];
            offset_ms = file_len_ms - (left / (16000 * 2)) * 1000;
            wiimu_log(1, 0, 0, 0, "wake count %d score %d offset=%.2ld:%.2ld:%.2ld searchfile=%d",
                      wake_count++, ret, offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60,
                      (offset_ms / 1000) % 60, search_id);
            memset((void *)out_path, 0, sizeof(out_path));
            snprintf(out_path, sizeof(out_path), "%s_search%d_%.2ld_%.2ld_%.2ld.pcm", path,
                     search_id, offset_ms / 1000 / 3600, (offset_ms / 1000 / 60) % 60,
                     (offset_ms / 1000) % 60);
            save_wake_word(out_path, p, (unsigned int)(p - start_addr), file_size,
                           MS_TO_BYTES(3000), MS_TO_BYTES(3000));
        }
        left -= chunk_size;
        p += chunk_size;
    }
    AIYD_Stop_sensory();
    munmap(start_addr, file_size);
    close(fd);

    return 0;
}

void write_to_excel(int index, char *file_name, int result)
{
    FILE *fp = fopen("./result.xls", "a+");
    if (fp) {
        if (17 == index) {
            fprintf(fp, "%d\t\n", result);
        } else if (1 == index) {
            fprintf(fp, "%s\t%d\t", file_name, result);
        } else {
            fprintf(fp, "%d\t", result);
        }
        fclose(fp);
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

    if (argc < 5) {
        fprintf(stderr, "Usage: %s 16khz_1ch_pcm_file search_file_id(1-17) raw_data_path "
                        "file_prefix [ parse_to_end ]\n",
                argv[0]);
        return -1;
    }

    if (argc >= 6)
        parse_to_end = atoi(argv[5]);
    path = argv[1];
    search_id = atoi(argv[2]);
    // if( search_id < 1 || search_id > 17 )
    //  search_id = 8;

    memset(hbg_netfile, 0x0, sizeof(hbg_netfile));
    memset(hbg_searchfile, 0x0, sizeof(hbg_searchfile));
    snprintf(hbg_netfile, sizeof(hbg_netfile), "%s/%s_am.raw", argv[3], argv[4]);
    snprintf(hbg_searchfile, sizeof(hbg_searchfile), "%s/%s_search_%d.raw", argv[3], argv[4],
             search_id);
    //  show_cpu_usage();

    if (parse_to_end)
        return do_sensory_test_2(path);

    file_size = get_file_len(path);
    size = file_size;
    if (size > 0) {
        buf = malloc(size + 1);
        if (!buf)
            return -1;
        if (read_file(path, buf, size) != size) {
            free(buf);
            return -1;
        }
    }
    AIYD_Start_sensory();
    ret = do_sensory_test(2048, buf, file_size);
    printf("%d %s %d\n", search_id, path, ret);
    char *test_name = strchr(path, '/');
    if (test_name) {
        write_to_excel(search_id, test_name + 1, ret);
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
    return (ret > 0 ? 0 : -1);
}
