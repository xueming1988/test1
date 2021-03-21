/*************************************************************************
    > File Name: save_record_file.c
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 11:07:38 PM EDT
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "wm_util.h"
#include "ccaptureGeneral.h"
#include "wave_file.h"
#include "save_record_file.h"

#define _LARGEFILE64_SOURCE

#if CXDISH_6CH_SAMPLE_RATE
#undef USB_CAPTURE_SAMPLERATE
#define USB_CAPTURE_SAMPLERATE CXDISH_6CH_SAMPLE_RATE
#endif

#define SAMPLE_RATE_FAR (USB_CAPTURE_SAMPLERATE)

static FILE *g_record_fp = 0;
static long long record_file_len = 0;

typedef struct _CAPFILE {
    char *fileName;
    long long count;
} CAPFILE, *PCAPFILE;

CAPTURE_COMUSER Cosume_File = {
    .enable = 0,
    .pre_process = 1,
    .cCapture_dataCosume = CCaptCosume_FileSave,
    .cCapture_init = CCaptCosume_initFileSave,
    .cCapture_deinit = CCaptCosume_deinitFileSave,
};

int CCaptCosume_FileSave(char *buf, int size, void *priv)
{
    int res = 0;
    int postsize;
    // printf("capture file save:%d\n", size);
    PCAPFILE capFile = (PCAPFILE)priv;
    capFile->count += size;
    if (!buf || !size) {
        return 0;
    }

    if (g_record_fp) {
        if (write(g_record_fp, buf, size) != size) {
            printf("write capture data to file failed\n");
        }
    }
    if (record_file_len > 0 && capFile->count >= record_file_len) {
        SocketClientReadWriteMsg("/tmp/RequestGoheadCmd", "recordFileStop",
                                 strlen("recordFileStop"), NULL, NULL, 0);
        record_file_len = 0;
    }
    return res;
}

int CCaptCosume_initFileSave(void *priv)
{
    PCAPFILE capFile = (PCAPFILE)priv;
    char fileName[100] = "/tmp/web/capture.pcm";
    int channels = Cosume_File.pre_process ? 2 : 1;
    if (strlen(capFile->fileName)) {
        snprintf(fileName, sizeof(fileName), "%s%s", capFile->fileName, ".swp");
    }
    if ((g_record_fp = open64(fileName, O_WRONLY | O_CREAT, 0644)) == -1) {
        printf("create new record file failed\n");
    }
#if !CXDISH_6CH_SAMPLE_RATE
    begin_wave(g_record_fp, 2147483648LL, channels);
#endif

    return 0;
}

int CCaptCosume_deinitFileSave(void *priv)
{
    PCAPFILE capFile = (PCAPFILE)priv;
    end_wave(g_record_fp, capFile->count);

    // save media info to .inf file
    char inf[200];
    char swp[200];

    snprintf(inf, sizeof(inf), "%s.inf", capFile->fileName);
    snprintf(swp, sizeof(swp), "%s.swp", capFile->fileName);
    FILE *fpinf = fopen(inf, "w");
    if (fpinf) {
        char temp[100];
        snprintf(temp, sizeof(temp), "channels:%d\n", Cosume_File.pre_process ? 2 : 1);
        fputs(temp, fpinf);
        snprintf(temp, sizeof(temp), "rate:%d\n",
                 Cosume_File.pre_process ? 44100 : SAMPLE_RATE_FAR);
        fputs(temp, fpinf);
        fclose(fpinf);
    }

    rename(swp, capFile->fileName);

    return 0;
}

void CCapt_Start_GeneralFile(char *fileName)
{
    long long len = 0;
    int channel = 2;
    char *p = strstr(fileName, ":");
    if (p && strlen(p) > 1) {
        char *pChannel = strstr(p + 1, ":");
        *p = 0;
        len = atoll(p + 1);
        if (pChannel)
            channel = atoi(pChannel + 1);
    }
    Cosume_File.pre_process = (channel == 2) ? 1 : 0;
    printf("CCapt_Start_GeneralFile start[%s] len=%lld pre_process=%d\n", fileName, len,
           Cosume_File.pre_process);
    record_file_len = (len > 0) ? len : 0;

    PCAPFILE fileCap = malloc(sizeof(CAPFILE));
    memset(fileCap, 0, sizeof(CAPFILE));
    fileCap->fileName = strdup(fileName);
    Cosume_File.priv = (void *)fileCap;
    CCaptCosume_initFileSave(Cosume_File.priv);
    CCaptRegister(&Cosume_File);
    Cosume_File.enable = 1;

    printf("CCapt_Start_GeneralFile start end\n");
}

void CCapt_Stop_GeneralFile(char *fileName)
{
    printf("CCapt_Stop_GeneralFile stop\n");

    Cosume_File.enable = 0;
    CCaptUnregister(&Cosume_File);
    if (fileName) {
        PCAPFILE capFile = (PCAPFILE)Cosume_File.priv;
        strncpy(fileName, capFile->fileName, strlen(capFile->fileName));
    }
    CCaptCosume_deinitFileSave(Cosume_File.priv);
    PCAPFILE fileCap = (PCAPFILE)Cosume_File.priv;

    if (fileCap->fileName)
        free(fileCap->fileName);
    free(fileCap);
    Cosume_File.priv = NULL;

    printf("CCapt_Stop_GeneralFile end\n");
}
