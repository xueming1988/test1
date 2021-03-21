#include <alsa/asoundlib.h>
#include <stdint.h>
#include <stdio.h>
#include "pthread.h"
#include <signal.h>
#include <sys/time.h>
// channel num
const int CHANNELS = 8;
const int PER_BYTES = 2;
// const int CHUNK_SIZE = 8190;
const int CHUNK_SIZE = 1024;
const int SAMPLE_RATE = 48000;
int stopRecord = 0;

static snd_pcm_hw_params_t *record_hw_params = 0;
static snd_pcm_sw_params_t *record_sw_params = 0;

static int set_capture_params(snd_pcm_t *handle)
{
    int err;
    int sampling_rate = SAMPLE_RATE;
    snd_pcm_uframes_t chunk_size = CHUNK_SIZE;
    unsigned int buffer_time = 0;
    unsigned int period_time = 0;
    snd_pcm_uframes_t buffer_size;

    snd_pcm_hw_params_alloca(&record_hw_params);
    snd_pcm_sw_params_alloca(&record_sw_params);
    err = snd_pcm_hw_params_any(handle, record_hw_params);
    if (err < 0) {
        printf("Broken configuration for this PCM: no configurations available\n");
    }
    err = snd_pcm_hw_params_set_access(handle, record_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_format(handle, record_hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        printf("Sample format non available\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_channels(handle, record_hw_params, CHANNELS);
    if (err < 0) {
        printf("Channels count non available\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_rate_near(handle, record_hw_params, (unsigned int *)&sampling_rate,
                                          0);
    if (err < 0) {
        printf("sampling_rate set faile %d\n", sampling_rate);
    }

    err = snd_pcm_hw_params_get_buffer_time_max(record_hw_params, &buffer_time, 0);
    printf("buffer_time %d\n", buffer_time);

    if (buffer_time > 500000)
        buffer_time = 500000;
    period_time = buffer_time / 4;

    err = snd_pcm_hw_params_set_buffer_time_near(handle, record_hw_params, &buffer_time, 0);
    if (err < 0) {
        printf("buffer_time set faile %d\n", buffer_time);
    }
    err = snd_pcm_hw_params_set_period_time_near(handle, record_hw_params, &period_time, 0);
    if (err < 0) {
        printf("period_time set faile %d\n", period_time);
    }

    err = snd_pcm_hw_params(handle, record_hw_params);

    snd_pcm_hw_params_get_period_size(record_hw_params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(record_hw_params, &buffer_size);
    if (chunk_size == buffer_size) {
        printf("Can't use period equal to buffer size (%lu == %lu)\n", chunk_size, buffer_size);
    }

    err = snd_pcm_nonblock(handle, 1);
    if (err < 0) {
        printf("capture nonblock setting error: %s\n", snd_strerror(err));
        return -1;
    }

    return 0;
}

// AEC reference caputre Process
static void *recordTest(void *arg)
{
    char capdev[32] = {0};
    printf("recordTest \r\n ");

#if defined(ALSA_DSNOOP_ENABLE)
    snprintf(capdev, sizeof(capdev), "%s", "input_pdm");
#else  // defined(ALSA_DSNOOP_ENABLE)
    snprintf(capdev, sizeof(capdev), "%s", "hw:0,2");
#endif // defined(ALSA_DSNOOP_ENABLE)

    int ALLOCATE_ARR_SIZE = CHUNK_SIZE * CHANNELS * PER_BYTES; // bytes
    int MONO_ARR_SIZE = ALLOCATE_ARR_SIZE / 8;

    snd_pcm_t *alsa_record_handle = 0;

    int err = snd_pcm_open(&alsa_record_handle, capdev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0 || alsa_record_handle == 0) {
        printf("capture open error: %s", snd_strerror(err));
        return NULL;
    }

    if (set_capture_params(alsa_record_handle) < 0) {
        printf("set_capture_params error !!!!\n");
        snd_pcm_close(alsa_record_handle);
        return NULL;
    }

    char *capture_data = (char *)malloc(ALLOCATE_ARR_SIZE);
    // for bt stream out
    short *refOriBuffer = (char *)malloc(MONO_ARR_SIZE / 2 * sizeof(short) * 2);
    memset(refOriBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short) * 2);

    int res;
    int write_len = 0;
    while (!stopRecord) {
        write_len = CHUNK_SIZE;
        err = snd_pcm_readi(alsa_record_handle, capture_data, write_len);
        if (err == 0)
            usleep(1000);
        if (err == -EAGAIN) {
            usleep(16000);
            printf("Eagain\n");
        } else if (err == -EPIPE) {
            snd_pcm_status_t *status;
            snd_pcm_status_alloca(&status);
            printf("capture EPIPE\n");
            if ((res = snd_pcm_status(alsa_record_handle, status)) < 0) {
                printf("status error: %s", snd_strerror(res));
                break;
            }
            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
                printf("Capture underrun !!!\n");
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture xrun: prepare error: %s", snd_strerror(res));
                    break;
                }
            } else if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture xrun(DRAINING): prepare error: %s", snd_strerror(res));
                    break;
                }
            }
        } else if (err == -ESTRPIPE) {
            printf("capture ESTRPIPE\n");
            while ((res = snd_pcm_resume(alsa_record_handle)) == -EAGAIN)
                sleep(1);
            if (res < 0) {
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture suspend: prepare error: %s", snd_strerror(res));
                    break;
                }
            }
        } else if (err < 0) {
            printf("capture error: %s", snd_strerror(err));
            break;
        }

        if (err > 0) {
            int tempLen = 0;

            // tempLen = write_len < (err * 16) ? write_len : (err * 16);
            tempLen = err;
            short *origin_data = (short *)capture_data;

            for (int i = 0; i < tempLen * 16 / 2 / 8; ++i) {
                refOriBuffer[i * 2 + 0] = origin_data[8 * i + 6];
                refOriBuffer[i * 2 + 1] = origin_data[8 * i + 7];
            }
            int bt_stream_enable = 0;
            int hw_pcm_enable = 0;
            // Mute detected
            int tfd = open("/sys/class/gpio/gpio510/value", O_RDONLY);
            if (tfd >= 0) {
                char mute_flag[4] = {0};
                if (read(tfd, (void *)mute_flag, 4) > 0) {
                    bt_stream_enable = !atoi(mute_flag);
                }
                close(tfd);
            }

            static int btav_res = -1;

            if (bt_stream_enable && access("/tmp/loop_capture_pcm", F_OK) != -1) {
                if (btav_res == -1) {
                    btav_res = open("/tmp/loop_capture_pcm", O_WRONLY | O_NONBLOCK);
                }

                //	printf("capture err : %d \r\n ", err );
                if (btav_res >= 0) {
                    int ret = write(btav_res, refOriBuffer, err * 4);
                    if (ret < 0) {
                        close(btav_res);
                        btav_res = -1;
                    }
                }
            }
        }
    }

    snd_pcm_close(alsa_record_handle);

    free(refOriBuffer);
    free(capture_data);
}

void lunch_capture_example(void)
{
    pthread_t p_bt_tid;
    pthread_attr_t attr;
    struct sched_param param;

    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 90;
    pthread_attr_setschedparam(&attr, &param);

    pthread_create(&p_bt_tid, &attr, recordTest, NULL);
}
