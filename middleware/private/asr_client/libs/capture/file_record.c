#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include "wm_util.h"

typedef struct {
    pthread_t tid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int is_runnning;
    int fd;
} file_record_t;

static file_record_t g_record_ctx;
extern int AlexaWriteRecordData(char *data, size_t size);
extern int AlexaStopWriteData(void);
extern void switch_content_to_background(int val);

#define TMP_RECORD
#ifdef TMP_RECORD
static FILE *g_record_intercom = NULL;
#endif

static void record_begin(void)
{
    pthread_mutex_lock(&g_record_ctx.mutex);
    g_record_ctx.is_runnning = 1;
    pthread_mutex_unlock(&g_record_ctx.mutex);
    switch_content_to_background(1);
    AlexaInitRecordData();
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:3", strlen("talkstart:3"), 0, 0, 0);
#ifdef TMP_RECORD
    g_record_intercom = fopen("/tmp/web/file.pcm", "wb+");
#endif
}

static void record_end(void)
{
    AlexaFinishWriteData();
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "VAD_FINISH", strlen("VAD_FINISH"), NULL, NULL,
                             0);

#ifdef TMP_RECORD
    if (g_record_intercom) {
        fclose(g_record_intercom);
    }
#endif
    pthread_mutex_lock(&g_record_ctx.mutex);
    if (g_record_ctx.fd > 0) {
        close(g_record_ctx.fd);
        g_record_ctx.fd = -1;
    }
    g_record_ctx.is_runnning = 0;
    pthread_mutex_unlock(&g_record_ctx.mutex);
}

static void write_record_data(char *buf, int len)
{
    AlexaWriteRecordData(buf, len);
#ifdef TMP_RECORD
    if (g_record_intercom) {
        fwrite(buf, len, 1, g_record_intercom);
    }
#endif
}

static int read_audio_data(char *buf, int len)
{
    if (!buf || len <= 0 || g_record_ctx.fd < 0)
        return -1;

    return read(g_record_ctx.fd, buf, len);
}

static void *file_record_thread(void *arg)
{
    char buf[16384] = {0};
    int len = 0;
    while (1) {
        wiimu_log(1, 0, 0, 0, "%s %d", __FUNCTION__, __LINE__);
        pthread_mutex_lock(&g_record_ctx.mutex);
        pthread_cond_wait(&g_record_ctx.cond, &g_record_ctx.mutex);
        pthread_mutex_unlock(&g_record_ctx.mutex);
        wiimu_log(1, 0, 0, 0, "%s %d", __FUNCTION__, __LINE__);
        record_begin();
        while (1) {
            if (!g_record_ctx.is_runnning)
                break;
            len = read_audio_data(buf, sizeof(buf));
            if (len <= 0)
                break;
            write_record_data(buf, len);
        }
        record_end();
    }
    printf("%s exit\n", __FUNCTION__);
    return (void *)0;
}

int file_record_start(char *path)
{
    int ret = 0;
    pthread_mutex_lock(&g_record_ctx.mutex);
    if (!g_record_ctx.is_runnning) {
        g_record_ctx.fd = open(path, O_RDONLY);
        pthread_cond_signal(&g_record_ctx.cond);
    } else {
        ret = -1;
    }
    pthread_mutex_unlock(&g_record_ctx.mutex);
    return ret;
}

int file_record_init(void)
{
    int ret = 0;
    memset((void *)&g_record_ctx, 0x0, sizeof(g_record_ctx));
    pthread_mutex_init(&g_record_ctx.mutex, NULL);
    pthread_cond_init(&g_record_ctx.cond, NULL);

    ret = pthread_create(&g_record_ctx.tid, NULL, file_record_thread, (void *)&g_record_ctx);
    if (0 != ret) {
        fprintf(stderr, "%s pthread_create failed %d\n", __FUNCTION__, ret);
        pthread_mutex_destroy(&g_record_ctx.mutex);
        pthread_cond_destroy(&g_record_ctx.cond);
        return ret;
    }
    printf("%s OK\n", __FUNCTION__);
    return 0;
}

int file_record_uninit(void)
{
    pthread_mutex_lock(&g_record_ctx.mutex);
    g_record_ctx.is_runnning = 0;
    pthread_mutex_unlock(&g_record_ctx.mutex);

    if (g_record_ctx.tid > 0) {
        pthread_join(g_record_ctx.tid, NULL);
    }
    pthread_mutex_destroy(&g_record_ctx.mutex);
}
