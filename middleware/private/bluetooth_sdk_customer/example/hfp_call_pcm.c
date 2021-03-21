
#include "alsa/asoundlib.h"
#include "linkplay_bluetooth_interface.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "soft_prep_rel_interface.h"
#include "stream_buffer.h"

#define BT_PCM_DEVICE "hw:0,3"
#define SPEAKER_PCM_DEVICE "default"

#define SPEAKER_CHANNEL_NUM 2
#define SPEAKER_BIT_WIDTHS 16
#define SPEAKER_FRAME_BYTES ((SPEAKER_CHANNEL_NUM * SPEAKER_BIT_WIDTHS) / 8)
#define SPEAKER_BUFFER_TIME 512000
#define SPEAKER_DElAY_PERIOD 2

#define BT_CHANNEL_NUM 1
#define BT_BIT_WIDTHS 16
#define BT_FRAME_BYTES ((BT_CHANNEL_NUM * BT_BIT_WIDTHS) / 8)
#define BT_BUFFER_TIME 640000
#define BT_DElAY_PERIOD 2

#define BUFFER_PERIOD_NUM 8

#define SOFT_PREP_REL_TEST_MIC_NUMB 2

#define SOUND_CAPTURE_LEN (1024)

static int is_hfp_call_running = 0;

// static pthread_mutex_t mutex_capture_data = PTHREAD_MUTEX_INITIALIZER;

stream_buffer g_ring_Buffer = {0};

pthread_t pthread_speaker_play = 0, pthread_bt_play = 0;

static int set_hfp_pcm_para(snd_pcm_t *handle, unsigned int rate, unsigned int channel_num,
                            unsigned int max_buffer_time, unsigned int delay_period,
                            snd_pcm_sframes_t *p_period_frame)
{
    int err;
    snd_pcm_hw_params_t *params = 0;
    snd_pcm_sw_params_t *swparams = 0;
    unsigned int buffer_time = 0, period_time = 0;
    snd_pcm_uframes_t buffer_frames = 0;
    snd_pcm_uframes_t period_frames = 0;

    snd_pcm_hw_params_alloca(&params);
    if (params == 0)
        return -1;
    snd_pcm_sw_params_alloca(&swparams);
    if (swparams == 0)
        return -1;
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para Can not configure this PCM device\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para Failed to snd_pcm_hw_params_set_access\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para Failed to set SND_PCM_FORMAT_S16_LE\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_channels(handle, params, channel_num);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para Failed to APP_HS_CHANNEL=%d\n", channel_num);
        return -1;
    }
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para Failed to set PCM device to sample rate =%d\n", rate);
        return -1;
    }
    snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
    if (buffer_time > max_buffer_time)
        buffer_time = max_buffer_time;
    period_time = buffer_time / BUFFER_PERIOD_NUM;
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
    if (err < 0) {
        SNDERR("Failed to set PCM device to buffer time=%d\n", buffer_time);
        return -1;
    }

    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
    if (err < 0) {
        SNDERR("Failed to set PCM device to period time =%d\n", period_time);
        return -1;
    }
    SNDERR("set_hfp_pcm_para buffer_time=%d period_time=%d \n", buffer_time, period_time);

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        SNDERR("unable to set hw parameters\n");
        return -1;
    }
    err = snd_pcm_get_params(handle, &buffer_frames, &period_frames);
    if (buffer_frames <= period_frames) {
        SNDERR("buffer_frames<=period_frames \n");
        return -1;
    }
    SNDERR("set_hfp_pcm_para bt_handle_capture buffer_frames=%d period_frame=%d\n", buffer_frames,
           period_frames);
    if (p_period_frame) {
        *p_period_frame = period_frames;
    }
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    if (err < 0) {
        SNDERR("set_hfp_pcm_para bt_handle_capture snd_pcm_get_params fault!\n");
        return -1;
    }
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    if (delay_period > 0) {
        snd_pcm_sw_params_current(handle, swparams);
        err = snd_pcm_sw_params_set_start_threshold(handle, swparams, period_frames * delay_period);
        if (err < 0) {
            SNDERR("set_hfp_pcm_para snd_pcm_sw_params_set_start_threshold error!\n");
            return -1;
        }
        err = snd_pcm_sw_params(handle, swparams);
        if (err < 0) {
            SNDERR("set_hfp_pcm_para snd_pcm_sw_params error!\n");
            return -1;
        }
    }
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    return 0;
}

/*
      bt  one channle format is  A 0xFFFF B 0xFFFF C 0xFFFF  ( A  B C is short word and left channel
   data)

     change one channle   to  two channel  AABBCC
 */
static void bt_one_channel_to_double(char *pData, int len)
{

    short *src_start = (short *)pData;
    short *src_end = (short *)(pData + len - 4);

    for (; src_start <= src_end; src_start = src_start + 2) {
        *(src_start + 1) = *(src_start);
    }
}

static void pcm_play(snd_pcm_t *p_play_handle, snd_pcm_sframes_t period_frame, char *pData, int len,
                     int frame_bytes)
{
    int ret = 0;
    snd_pcm_sframes_t total_frames = 0;
    snd_pcm_sframes_t frames_to_write = 0;

    while (len > 0) {
        total_frames = len / frame_bytes;
        if (total_frames <= 0) {
            break;
        }
        frames_to_write = period_frame <= total_frames ? period_frame : total_frames;
        ret = snd_pcm_writei(p_play_handle, pData, frames_to_write);
        if (ret == -EPIPE) /* over or under run */
        {
            SNDERR("EPIPE: %d", EPIPE);
            if ((ret = snd_pcm_prepare(p_play_handle)) < 0) {
                SNDERR("snd_pcm_prepare error: %s\n", snd_strerror(ret));
                break;
            }
        } else if (ret == -ESTRPIPE) /* suspend */
        {
            SNDERR("ESTRPIPE: %d", ESTRPIPE);
            while ((ret = snd_pcm_resume(p_play_handle)) == -EAGAIN)
                sleep(1);
        } else if (ret < 0) {
            SNDERR(" snd_pcm_writei hope: %d, actual: %d\n", frames_to_write, ret);
            break;
        } else if ((ret > 0) && (ret < frames_to_write)) {
            SNDERR(" Short write (expected %li, wrote %li)", (long)frames_to_write, ret);
        }
        if (ret > 0) {
            len = len - ret * frame_bytes;
            pData = pData + ret * frame_bytes;
        }
    }
}

static void *bt_capture_speaker_play_thread()
{
    int ret = 0;
    snd_pcm_t *p_speaker_play_handle = 0;
    snd_pcm_t *p_bt_capture_handle = 0;
    snd_pcm_sframes_t read_period_frame = 0;
    snd_pcm_sframes_t write_period_frame = 0;
    char *pbuffer = NULL;

    ret = snd_pcm_open(&p_speaker_play_handle, SPEAKER_PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        SNDERR("unable to open speaker_play_handle : %s\n", strerror(ret));
        goto error;
    }

    ret = set_hfp_pcm_para(p_speaker_play_handle, linkplay_bluetooth_get_hfp_capture_rate(),
                           SPEAKER_CHANNEL_NUM, SPEAKER_BUFFER_TIME, SPEAKER_DElAY_PERIOD,
                           &write_period_frame);
    if (ret < 0) {
        SNDERR("p_speaker_play_handle set_hfp_pcm_para  failed : %s\n", strerror(ret));
        goto error;
    }

    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    ret = snd_pcm_open(&p_bt_capture_handle, BT_PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        SNDERR("unable to open bt_capture_handle : %s\n", strerror(ret));
        goto error;
    }

    ret = set_hfp_pcm_para(p_bt_capture_handle, linkplay_bluetooth_get_hfp_capture_rate(),
                           BT_CHANNEL_NUM, SPEAKER_BUFFER_TIME, 0, &read_period_frame);

    if (ret < 0) {
        SNDERR("pp_bt_capture_handle set_hfp_pcm_para  failed : %s\n", strerror(ret));
        goto error;
    }

    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    pbuffer = malloc(read_period_frame * BT_FRAME_BYTES);
    if (pbuffer == NULL) {
        SNDERR("bt_capture_speaker_play_thread malloc read_period_bytes failed\n");
        goto error;
    }
    while (is_hfp_call_running) {
        ret = snd_pcm_readi(p_bt_capture_handle, pbuffer, read_period_frame);
        if (ret == -EPIPE) /* over or under run  */
        {
            SNDERR("EPIPE: %d", EPIPE);
            if ((ret = snd_pcm_prepare(p_bt_capture_handle)) < 0) {
                SNDERR("snd_pcm_prepare error: %s\n", snd_strerror(ret));
            }
        } else if (ret < 0) {
            if (p_bt_capture_handle)
                SNDERR("%s snd_pcm_readi failed! ret=%d\n", snd_strerror(ret), ret);
        } else if ((ret > 0) && (ret < read_period_frame)) {
            if (p_bt_capture_handle)
                SNDERR("Short read (expected %i, read %i)\n", (int)read_period_frame, (int)ret);
        }
        if (ret > 0) {
            // printf("%s snd_pcm_readi ret=%d\n",__FUNCTION__,ret);
            bt_one_channel_to_double(pbuffer, ret * BT_FRAME_BYTES);
            pcm_play(p_speaker_play_handle, write_period_frame, pbuffer, ret * BT_FRAME_BYTES,
                     SPEAKER_FRAME_BYTES);
        }
    }

error:
    if (p_speaker_play_handle) {
        snd_pcm_close(p_speaker_play_handle);
    }
    if (p_bt_capture_handle) {
        snd_pcm_close(p_bt_capture_handle);
    }
    if (pbuffer) {
        free(pbuffer);
    }
    is_hfp_call_running = 0;
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    return 0;
}

int CCapt_soft_prep_capture(void *opaque, char *buffer, int dataLen)
{
    int ret = 0;
    stream_node *pNode = 0;

    if (dataLen > SOUND_CAPTURE_LEN) {
        printf("data is  too big  dataLen=%d\r\n", dataLen);
        return 0;
    }
    if (!is_hfp_call_running) {
        return 0;
    }
    pNode = stream_buffer_get_write_node(&g_ring_Buffer);
    if (pNode == 0) {
        usleep(10);
        printf("CCapt_soft_prep_capture  buffer   full.\n");
        return 0;
    }
    memcpy(pNode->buffer, buffer, dataLen);
    pNode->len = dataLen;
    stream_buffer_write_commit(&g_ring_Buffer);

    return ret;
}

static void start_sound_capture()
{
    SOFT_PREP_INIT init_param;
    WAVE_PARAM output_format;
    int cap;

    // setup init parameter
    memset(&init_param, 0, sizeof(SOFT_PREP_INIT));
    init_param.captureCallback = CCapt_soft_prep_capture;
    init_param.wakeupCallback = NULL;
    init_param.ObserverStateCallback = NULL;
    init_param.mic_numb = SOFT_PREP_REL_TEST_MIC_NUMB;
    init_param.cap_dev = SOFT_PREP_CAP_PDM;
    // init_param.cap_dev = SOFT_PREP_CAP_TDMC;       /*  attention for  yandex   */
    printf("set cap dev:%d\n", init_param.cap_dev);

    Soft_Prep_Init(init_param);

    // get the output format information
    output_format = Soft_Prep_GetAudioParam();
    printf("Output format:\n");
    printf("\tBit per Sample:%d\n", output_format.BitsPerSample);
    printf("\tSampleRate:%d\n", output_format.SampleRate);
    printf("\tChannel:%d\n", output_format.NumChannels);

    // get the capability of software preprocess library
    cap = Soft_Prep_GetCapability();
    printf("Support Capability:\n");
    if (cap & SOFP_PREP_CAP_AEC)
        printf("\tCapability of AEC\n");
    if (cap & SOFP_PREP_CAP_WAKEUP)
        printf("\tCapability of Wakeup\n");

    // start capture
    Soft_Prep_StartCapture();
}

static void end_sound_capture()
{
    Soft_Prep_StopCapture(); // stop capture
    Soft_Prep_Deinit();      // deinit

    printf("%s  %d  \n", __func__, __LINE__);
}

static void *bt_play_thread()
{
    int ret = 0;
    snd_pcm_t *p_bt_play_handle = 0;
    snd_pcm_sframes_t bt_play_frame = 0;
    char *pbuffer = NULL;
    int read_len = 0;
    int max_node_len = 0;
    stream_node *pNode = 0;

    ret = snd_pcm_open(&p_bt_play_handle, BT_PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        SNDERR("unable to open p_bt_play_handle : %s\n", strerror(ret));
        goto error;
    }
    ret = set_hfp_pcm_para(p_bt_play_handle, linkplay_bluetooth_get_hfp_capture_rate(),
                           BT_CHANNEL_NUM, BT_BUFFER_TIME, BT_DElAY_PERIOD, &bt_play_frame);
    if (ret < 0) {
        SNDERR("bt_play_thread set_hfp_pcm_para error\n", strerror(ret));
        goto error;
    }

    max_node_len = bt_play_frame * BT_FRAME_BYTES > SOUND_CAPTURE_LEN
                       ? bt_play_frame * BT_FRAME_BYTES
                       : SOUND_CAPTURE_LEN;
    pbuffer = malloc(max_node_len + 4);
    if (pbuffer == 0) {
        SNDERR("alloc buffer fail\n");
        goto error;
    }
    printf("max_node_len=%d\n", max_node_len);
    if (stream_buffer_init(&g_ring_Buffer, max_node_len + 4, 60) < 0) {
        printf("test stream_buffer_init  failed!\n");
        goto error;
    }

    while (is_hfp_call_running) {
        pNode = stream_buffer_get_read_node(&g_ring_Buffer); /* buffer empty*/
        if (pNode == 0) {
            usleep(10);
            continue;
        }
        memcpy(pbuffer, pNode->buffer, pNode->len);
        read_len = pNode->len;
        pNode->len = 0;
        stream_buffer_read_commit(&g_ring_Buffer);
        pcm_play(p_bt_play_handle, bt_play_frame, pbuffer, read_len, BT_FRAME_BYTES);
    }
error:
    if (p_bt_play_handle) {
        printf("%s read_len = %d \r\n", __FUNCTION__, read_len);
        snd_pcm_close(p_bt_play_handle);
        p_bt_play_handle = 0;
    }
    if (pbuffer) {
        free(pbuffer);
    }
    stream_buffer_destroy(&g_ring_Buffer);
    is_hfp_call_running = 0;
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    return 0;
}

void start_sound_capture_thread()
{
    static int is_sound_capture_thread_started = 0;
    if (is_sound_capture_thread_started == 0) {
        start_sound_capture();
        is_sound_capture_thread_started = 1;
    }
}

void start_call()
{
    int ret = 0;

    is_hfp_call_running = 1;

    ret = pthread_create(&(pthread_speaker_play), (const pthread_attr_t *)NULL,
                         bt_capture_speaker_play_thread, (void *)NULL);
    if (ret != 0) {
        printf("create bt_capture_speaker_play_thread error: %s\n", strerror(ret));
        return;
    }
    ret = pthread_create(&(pthread_bt_play), (const pthread_attr_t *)NULL, bt_play_thread,
                         (void *)NULL);
    if (ret != 0) {
        printf("create bt_play_thread error: %s\n", strerror(ret));
        return;
    }
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
}

void end_call()
{
    is_hfp_call_running = 0;

    pthread_join(pthread_speaker_play, 0);
    pthread_join(pthread_bt_play, 0);
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
}
