#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "linkplay_bluetooth_interface.h"
#include "common.h"
#include "linkplay_resample_interface.h"

const char *BTAVFIFONAME = "/tmp/loop_capture_pcm";
enum {
    AV_PLAY_START,
    AV_PLAY_SUSPEND,
};

#define MAX_SOURCE_BUFFER_SIZE 2048

typedef struct _BT_A2DP_SOURCE_CONTEXT {
    af_instance_t *resample_instance;
    af_data_t af_data;
    int av_link_state;
    int av_play_state;
    int cur_samplerate;
    int default_samplerate;
    int bt_av_fd;
} BT_A2DP_SOURCE_CONTEXT;

#define ARCH_ADD(p, a) (*(p) += (a))
#define ARCH_CMPXCHG(p, a, b) (*(p)) /* fake */
static pthread_mutex_t mutex_source_pcm = PTHREAD_MUTEX_INITIALIZER;

BT_A2DP_SOURCE_CONTEXT g_source_context;

#define MUTE_AUDIO_OUT_AMP "echo 0 > /sys/class/gpio/gpio510/value"
#define UNMUTE_AUDIO_OUT_AMP "echo 1 > /sys/class/gpio/gpio510/value"

static void set_bt_transfer(int enable)
{
    if (enable) {
        system(MUTE_AUDIO_OUT_AMP); // turn off amp
    } else {
        system(UNMUTE_AUDIO_OUT_AMP); // turn on amp
    }
}
/*
static void mix_areas_16(unsigned int size,
                         volatile signed short *dst, signed short *src,
                         volatile signed int *sum)
{
    register signed int sample, old_sample;

    for(;;) {
        sample = *src;
        old_sample = *sum;
        if(ARCH_CMPXCHG(dst, 0, 1) == 0)
            sample -= old_sample;
        ARCH_ADD(sum, sample);
        do {
            old_sample = *sum;
            if(old_sample > 0x7fff)
                sample = 0x7fff;
            else if(old_sample < -0x8000)
                sample = -0x8000;
            else
                sample = old_sample;
            *dst = sample;
        } while(*sum != old_sample);
        if(!--size)
            return;
        src++;
        dst++;
        sum++;
    }
}
*/
static int btreadstream(BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source_context, char *dst_buf, int len)
{

    int read_len = read(bt_a2dp_source_context->bt_av_fd, dst_buf, len);
    if (read_len > 0) {
        // if(read_len != MAX_SOURCE_BUFFER_SIZE)
        //    printf("A2DPtransmitstream %d  %d \r\n", read_len, index);
        // mix_areas_16(read_len / 2, dst_buf, buf, (signed int *)dst_sum);
    } else {
        close(bt_a2dp_source_context->bt_av_fd);
        bt_a2dp_source_context->bt_av_fd = open(BTAVFIFONAME, O_RDONLY | O_NONBLOCK);
    }

    return read_len;
}

static int source_stream_send_buffer(BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source, void *audio_buf,
                                     int length)
{
    if (bt_a2dp_source->cur_samplerate != bt_a2dp_source->default_samplerate) {
        if (bt_a2dp_source->resample_instance) {
            af_data_t *outdata;
            bt_a2dp_source->af_data.audio = audio_buf;
            bt_a2dp_source->af_data.len = length;
            outdata =
                linkPlay_resample(&bt_a2dp_source->af_data, bt_a2dp_source->resample_instance);
            if (outdata) {
                audio_buf = outdata->audio;
                length = outdata->len;
            }
        }
    }
    return linkplay_bluetooth_source_send_stream(audio_buf, length);
}

int source_stream_stop(void)
{
    printf("source_stream_stop \r\n");
    linkplay_bluetooth_stop_stream();

    return 0;
}
static void *bt_source_mix_play_thread(void *arg)
{
    BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source = (BT_A2DP_SOURCE_CONTEXT *)arg;
    char stream_buf[MAX_SOURCE_BUFFER_SIZE];
    int ret;
    int timeoutms = 0;
    int i, fd_mix = 0;
    int link_state = 0;

    int retval;
    struct timeval tv;
    socklen_t t;
    // fd_set rfds;
    struct pollfd pollfds[1];

    unlink(BTAVFIFONAME);
    mkfifo(BTAVFIFONAME, O_CREAT | 0666);
    bt_a2dp_source->bt_av_fd = -1;
    bt_a2dp_source->bt_av_fd = open(BTAVFIFONAME, O_RDONLY | O_NONBLOCK);
    if (bt_a2dp_source->bt_av_fd < 0) {
        return NULL;
    }
    memset(stream_buf, 0x0, MAX_SOURCE_BUFFER_SIZE);

    //   }

    while (1) {
        // FD_ZERO(&rfds);
        // FD_SET(bt_a2dp_source->bt_av_fd, &rfds);
        // fd_mix = bt_a2dp_source->bt_av_fd;
        memset(pollfds, 0x0, sizeof(pollfds));
        pollfds[0].fd = bt_a2dp_source->bt_av_fd;
        pollfds[0].events |= POLLIN;
        // tv.tv_sec = 10;
        // tv.tv_usec = 0;
        link_state = 0;
        int stream_len = 0;

        int retval = poll(pollfds, sizeof(pollfds) / sizeof(struct pollfd), 10 * 1000);

        // retval = select(fd_mix + 1, &rfds, NULL, NULL, &tv);
        if (retval > 0) {
            if (pollfds[0].revents & (POLLIN)) {
                stream_len = btreadstream(bt_a2dp_source, stream_buf, MAX_SOURCE_BUFFER_SIZE);
            }
            // printf("bt_a2dp_source->av_link_state %d \r\n",bt_a2dp_source->av_link_state);
            pthread_mutex_lock(&mutex_source_pcm);
            // start av stream transfer
            if (bt_a2dp_source->av_link_state == 1) {
                if (bt_a2dp_source->av_play_state == AV_PLAY_SUSPEND) {
                    if (!linkplay_bluetooth_av_resume_play()) {
                        bt_a2dp_source->av_play_state = AV_PLAY_START;
                    }
                }
                link_state = 1;
            }

            pthread_mutex_unlock(&mutex_source_pcm);
            // printf("stream_len %d  \r\n", stream_len);

            if (link_state && stream_len > 0) {
                source_stream_send_buffer(bt_a2dp_source, stream_buf, stream_len);
            }
        } else if (retval == 0) {
            pthread_mutex_lock(&mutex_source_pcm);
            // suspend av stream transfer
            if (bt_a2dp_source->av_link_state == 1) {
                if (bt_a2dp_source->av_play_state == AV_PLAY_START) {
                    if (!linkplay_bluetooth_av_pause_play()) {
                        bt_a2dp_source->av_play_state = AV_PLAY_SUSPEND;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_source_pcm);
        }
    }
}

int source_stream_init(void *arg)
{
    tLINKPLAY_AV_MEDIA_FEED_CFG_PCM pcm_config;
    BtError bterr;
    printf("source_stream_init \r\n");
    BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source = (BT_A2DP_SOURCE_CONTEXT *)arg;

    if ((bt_a2dp_source->cur_samplerate != 44100) && (bt_a2dp_source->cur_samplerate != 48000)) {
        bt_a2dp_source->cur_samplerate = 44100;
    }

    pcm_config.sampling_freq = bt_a2dp_source->cur_samplerate;
    pcm_config.num_channel = 2;
    pcm_config.bit_per_sample = 16;

    bterr = linkplay_bluetooth_source_start_stream(&pcm_config);
    if (bterr != kBtErrorOk) {
        printf("linkplay_bluetooth_source_start_stream failed %d \r\n", bterr);
        return -1;
    }
    if ((bt_a2dp_source->cur_samplerate != bt_a2dp_source->default_samplerate) &&
        (bt_a2dp_source->resample_instance == NULL)) {
        char resample_params[64];
        snprintf(resample_params, sizeof(resample_params), "%d:16:0:5:0.0",
                 bt_a2dp_source->cur_samplerate);
        printf("linkplay_resample_instance_create %s \r\n", resample_params);
        bt_a2dp_source->resample_instance = linkplay_resample_instance_create(resample_params);
        if (bt_a2dp_source->resample_instance) {
            bt_a2dp_source->af_data.rate = bt_a2dp_source->default_samplerate;
            bt_a2dp_source->af_data.nch = 2;
            bt_a2dp_source->af_data.bps = 2;
            linkplay_set_resample_format(&bt_a2dp_source->af_data,
                                         bt_a2dp_source->resample_instance);
        }
    }
    return 0;
}
extern void lunch_capture_example(void);

void *bt_a2dp_source_init_demo_play(void)
{
    pthread_t p_bt_tid;
    pthread_attr_t attr;
    struct sched_param param;

    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 85;
    pthread_attr_setschedparam(&attr, &param);

    pthread_create(&p_bt_tid, &attr, bt_source_mix_play_thread, &g_source_context);

    pthread_attr_destroy(&attr);
    pthread_detach(p_bt_tid);

    g_source_context.resample_instance = NULL;
    g_source_context.default_samplerate = 48000; // pcm sample rate get from FIFO
    g_source_context.cur_samplerate = 48000;
    set_bt_transfer(0);

    //
    // Actually, the AEC reference caputre is running at  Wakeup engine process
    lunch_capture_example();
    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    return &g_source_context;
}

void set_av_abs_vol(int volume)
{
    int index;
    unsigned char volume_set_phone[33] = {0,  3,  7,  11, 15,  19,  23,  27,  31,  35,  39,
                                          43, 47, 51, 55, 59,  63,  67,  71,  75,  79,  83,
                                          87, 91, 95, 99, 103, 107, 111, 115, 119, 123, 127};
    unsigned char volume_set_dsp[33] = {0,  3,  6,  9,  12, 15, 18, 21, 24, 27, 30,
                                        33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63,
                                        66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 100};

    for (index = 32; index >= 0; index--) {
        if (volume_set_dsp[index] <= volume)
            break;
    }

    if (index == -1)
        return;

    if (index >= 32)
        index = 32;

    printf("volume_bt[%d] %d \r\n", index, volume_set_phone[index]);
    linkplay_bluetooth_set_abs_volume(BT_ROLE_SOURCE, volume_set_phone[index], 0);
}

int set_av_connect_state(void *arg, int connected)
{
    BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source = (BT_A2DP_SOURCE_CONTEXT *)arg;

    pthread_mutex_lock(&mutex_source_pcm);
    if (connected) {
        bt_a2dp_source->av_link_state = 1;
        source_stream_init(arg);
        bt_a2dp_source->av_play_state = AV_PLAY_START;
        set_bt_transfer(1); // trun on alsa lib transmit pcm to the example demo
    } else {
        source_stream_stop();
        set_bt_transfer(0); // trun off alsa lib transmit pcm to the example demo
        bt_a2dp_source->av_link_state = 0;
        bt_a2dp_source->av_play_state = AV_PLAY_SUSPEND;
        bt_a2dp_source->cur_samplerate = 44100;
    }
    pthread_mutex_unlock(&mutex_source_pcm);

    return 0;
}
void set_av_set_sample_rate(void *arg, int samplerate)
{
    BT_A2DP_SOURCE_CONTEXT *bt_a2dp_source = (BT_A2DP_SOURCE_CONTEXT *)arg;
    bt_a2dp_source->cur_samplerate = samplerate;
}
