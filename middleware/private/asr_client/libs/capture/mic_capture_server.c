#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include "pthread.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "msp_cmn.h"
#include "msp_errors.h"
#include "ccaptureGeneral.h"
#include "wm_util.h"

#include "mic_capture_server.h"

extern WIIMU_CONTEXT *g_wiimu_shm;

static int mic_fifo_fd = -1;

static int mic_capture_Server_Init(void *priv);
static int mic_capture_Server_DeInit(void *priv);
static int mic_capture_Server_SaveData(char *buf, int size, void *priv);

static CAPTURE_COMUSER mic_capture_server = {
    .enable = 0,
    .pre_process = 0,
    .id = 2, /* audio data is 16000hz-16bit-1ch data */
    .cCapture_dataCosume = mic_capture_Server_SaveData,
    .cCapture_init = mic_capture_Server_Init,
    .cCapture_deinit = mic_capture_Server_DeInit,
    .priv = NULL,
};

static int mic_capture_Server_Init(void *priv)
{
    int fails = 0;

    if (access(MIC_CAPTURE_SERVER_FIFO, F_OK) < 0) {
        int rc = mkfifo(MIC_CAPTURE_SERVER_FIFO, O_CREAT | 0666);

        if (rc < 0) {
            wiimu_log(1, 0, 0, 0, "mkfifo %s error: %d,%s\n", MIC_CAPTURE_SERVER_FIFO, errno,
                      strerror(errno));
            return -1;
        }

        sync();
    }

    return 0;
}

static int mic_capture_Server_DeInit(void *priv)
{
    if (mic_fifo_fd > 0) {
        close(mic_fifo_fd);
    }

    unlink(MIC_CAPTURE_SERVER_FIFO);

    CCaptUnregister(&mic_capture_server);

    return 0;
}

static int mic_capture_Server_SaveData(char *buf, int size, void *priv)
{
    int w_size = 0;

    if (mic_capture_server.enable) {
        if (mic_fifo_fd < 0)
            mic_fifo_fd = open(MIC_CAPTURE_SERVER_FIFO, O_WRONLY | O_NONBLOCK);

        if (mic_fifo_fd > 0 && buf && size > 0) {
            w_size = write(mic_fifo_fd, buf, size);

            close(mic_fifo_fd);
            mic_fifo_fd = -1;
        }
    }

    return w_size;
}

int mic_capture_server_register(void)
{
    CCaptRegister(&mic_capture_server);

    mic_capture_server.cCapture_init(NULL);

    return 0;
}

int mic_capture_enable(void)
{
    wiimu_log(1, 0, 0, 0, "Starting mic capture.\n");

    /* donot trigger alexa */
    g_wiimu_shm->asr_ongoing = 1;

    mic_capture_server.enable = 1;

    return 0;
}

int mic_capture_disable(void)
{
    wiimu_log(1, 0, 0, 0, "Stop mic capture, enable=[%d].\n", mic_capture_server.enable);

    if (mic_capture_server.enable) {
        /* recover alexa state */
        g_wiimu_shm->asr_ongoing = 0;
    }

    mic_capture_server.enable = 0;

    return 0;
}
