#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <curl/curl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "soft_prep_rel_interface.h"

#define SOFT_PREP_REL_TEST_DUMP_FILE "/tmp/soft_prep_dump.pcm"
#define SOFT_PREP_REL_TEST_DUMPRAW_FILE "/tmp/soft_raw_dump.pcm"
#define SOFT_PREP_REL_TEST_MIC_NUMB 4

int GetAudioInputProcessorObserverState(void *opaque) { return 1; }

int dump_buf_append(char *filename, char *ptr, int size)
{
    int ofd;
    int ret;
    ofd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (ofd < 0) {
        fprintf(stderr, "Can't open %s\n", filename);
        return 0;
    }

    ret = write(ofd, ptr, size);
    fsync(ofd);
    close(ofd);
    return ret;
}

int CCapt_soft_prep_capture(void *opaque, char *buffer, int dataLen)
{
    int ret = dataLen;
    // printf("%s ++ %x %d\n", __func__, buffer, dataLen);

    ret = dump_buf_append(SOFT_PREP_REL_TEST_DUMP_FILE, buffer, dataLen);

    return ret;
}

int CCapt_soft_raw_capture(void *opaque, char *buffer, int dataLen)
{
    int ret = 0;
    // printf("%s ++ %x %d\n", __func__, buffer, dataLen);

    ret = dump_buf_append(SOFT_PREP_REL_TEST_DUMPRAW_FILE, buffer, dataLen);

    return ret;
}

int CCapt_Soft_prep_wakeup(void *opaque, WWE_RESULT *wwe_result)
{
    printf("wakeup callback %d ++\n", wwe_result->detected_numb);
    return 0;
}

void main(void)
{
    SOFT_PREP_INIT init_param;
    WAVE_PARAM output_format;
    WAVE_PARAM micraw_format;
    int cap;

    // setup init parameter
    memset(&init_param, 0, sizeof(SOFT_PREP_INIT));
    init_param.captureCallback = CCapt_soft_prep_capture;
    init_param.micrawCallback = CCapt_soft_raw_capture;
    init_param.wakeupCallback = CCapt_Soft_prep_wakeup;
    init_param.ObserverStateCallback = GetAudioInputProcessorObserverState;
    init_param.mic_numb = SOFT_PREP_REL_TEST_MIC_NUMB;
    init_param.configFile = "/system/workdir/AMLogic_VUI_Solution_Model.awb";

    Soft_Prep_Init(init_param);

    // get the output format information
    output_format = Soft_Prep_GetAudioParam();
    printf("Output format:\n");
    printf("\tBit per Sample:%d\n", output_format.BitsPerSample);
    printf("\tSampleRate:%d\n", output_format.SampleRate);
    printf("\tChannel:%d\n", output_format.NumChannels);

    // get the mic format information
    micraw_format = Soft_Prep_GetMicrawAudioParam();
    printf("Mic raw format:\n");
    printf("\tBit per Sample:%d\n", micraw_format.BitsPerSample);
    printf("\tSampleRate:%d\n", micraw_format.SampleRate);
    printf("\tChannel:%d\n", micraw_format.NumChannels);

    // get the capability of software preprocess library
    cap = Soft_Prep_GetCapability();
    printf("Support Capability:\n");
    if (cap & SOFP_PREP_CAP_AEC)
        printf("\tCapability of AEC\n");
    if (cap & SOFP_PREP_CAP_WAKEUP)
        printf("\tCapability of Wakeup\n");

    // start capture
    Soft_Prep_StartCapture();

    // wait for a while
    sleep(10);

    // test disable wwe
    if (cap & SOFP_PREP_CAP_WAKEUP)
        Soft_Prep_StopWWE();

    // wait for a while
    sleep(5);

    // stop capture
    Soft_Prep_StopCapture();

    // deinit
    Soft_Prep_Deinit();
}
