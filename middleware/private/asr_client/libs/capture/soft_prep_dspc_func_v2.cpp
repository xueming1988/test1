#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include <alsa/asoundlib.h>
#include "wm_api.h"
#include "capture_alsa.h"

#if defined(SOFT_PREP_DSPC_V2_MODULE)
#include <awelib_v2.h>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "soft_prep_dspc_func_v2.h"
// #include "AMLogic_VUI_Solution_Model.h"
#if defined(A98_BELKIN_MODULE)
#include "BelkinLinkplay_VUI_HFP_v1p0_ControlInterface_AWE6.h"
#define AWE_VRState_ID AWE_AFE____VRState_ID
#define AWE_VRState_value_OFFSET AWE_AFE____VRState_value_OFFSET
#else
#include "lp_external_WW_twoTrigger_twoMic_v0p2f_ControlInterface_AWE6.h"
#endif
#include "TcpIO2.h"

int dump_data = 0;
FILE *fp_input;
FILE *fp_output;

#if defined(SOFT_PREP_SDK_REL)
#define DSP_DEBUG 0
#else
#define DSP_DEBUG 1
#endif

#if !defined(SECURITY_IMPROVE) || defined(SECURITY_DEBUGPRINT)
#define DSP_SOCKET 1
#else
#define DSP_SOCKET 0
#endif

#if defined(ADC_ES7243_ENABLE)
#if defined(ALSA_DSNOOP_ENABLE)
#define REC_DEVICE_NAME "input_tdm"
#else // defined(ALSA_DSNOOP_ENABLE)
#define REC_DEVICE_NAME "hw:0,1"
#endif // defined(ALSA_DSNOOP_ENABLE)
#else  // defined(ADC_ES7243_ENABLE)
#if defined(MIC_BOOST_USING_ALSA)
#define REC_DEVICE_NAME "dmicarray"
#else // defined(MIC_BOOST_USING_ALSA)
#if defined(ALSA_DSNOOP_ENABLE)
#define REC_DEVICE_NAME "input_pdm"
#else // defined(ALSA_DSNOOP_ENABLE)
#define REC_DEVICE_NAME "hw:0,2"
#endif // defined(ALSA_DSNOOP_ENABLE)
#endif // defined(MIC_BOOST_USING_ALSA)
#endif // defined(ADC_ES7243_ENABLE)

#ifndef MIC_SENSITIVITY_DBFS
#if defined(A98_BELKIN_MODULE)
#define MIC_SENSITIVITY_DBFS (-46.0f)
#elif defined(A98_POLKA_MODULE)
#define MIC_SENSITIVITY_DBFS (-37.0f)
#else
#define MIC_SENSITIVITY_DBFS (-26.0f)
#endif
#endif

#ifndef TARGET_LEVEL_DBFS
#define TARGET_LEVEL_DBFS (-17.0f)
#endif

static const int NUM_INPUT_CHANNELS = 8;
static const int NUM_OUTPUT_CHANNELS = 0;
static const unsigned long PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP = 768;
static const unsigned long PREFERRED_SAMPLES_PER_CALLBACK_FOR_ALSA = 256;
static const unsigned long RING_BUFFER_SIZE =
    PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * NUM_INPUT_CHANNELS * 64;
int prev_state = 0;

bool wwe_enable;
bool mic_run_flag;
bool read_running;
bool dsp_running;
std::mutex m_mutex_enable;
pthread_t dsp_process = NULL;
std::thread pcm_read_thread;
#if DSP_DEBUG == 1
std::thread pcm_debug_write_thread;
#endif

void *do_dsp_processing(void *priv);
void do_debug_pcm_write();
void *do_pcm_read(void *priv);

int loopCount = 0;
UINT32 inCount = 0;
UINT32 outCount = 0;
UINT32 in_samps = 0;
UINT32 in_chans = 0;
UINT32 out_samps = 0;
UINT32 out_chans = 0;

CAWELib *pAwelib;

std::vector<int16_t> audioData_SDS;

std::vector<int> out_samples;
std::vector<int> in_samples;
std::vector<int> micraw_samples;
#if defined(MICRAW_FORMAT_CHG)
std::vector<int> micraw_samples_resample;
#endif
#if defined(ENABLE_RECORD_SERVER)
std::vector<int> recServer_samples;
#endif
std::vector<int> ring_buffer;
volatile int rp = 0;
volatile int wp = 0;

#define SAMPLE_RATE 48000
#define READ_FRAME 768
#define BUFFER_SIZE (SAMPLE_RATE / 2)
#define PERIOD_SIZE (BUFFER_SIZE / 4)
#define WRITE_DEVICE_NAME "default"

// lock to sync bwtween pcm reading and dsp processing
std::mutex rb_mutex;
std::condition_variable rb_cond;

#define MIC_NULL 99

char g_dspc_mic_array_2[10] = {0, // active
                               MIC_NULL, MIC_NULL,
                               1, // active
                               MIC_NULL, MIC_NULL, MIC_NULL, MIC_NULL, 6, 7};
char g_dspc_mic_array_4[10] = {0, // active
                               MIC_NULL,
                               1, // active
                               MIC_NULL,
                               2, // active
                               MIC_NULL,
                               3, // active
                               MIC_NULL, 6, 7};
char g_dspc_mic_array_6[10] = {0, 1, 2, 3, 4, 5, MIC_NULL, MIC_NULL, 6, 7};

char *g_dspc_mic_array_mapping = NULL;
char *g_configFile = NULL;

#if DSP_DEBUG == 1
bool write_running;
#define WRITE_UNIT (READ_FRAME * 2)
#define RING_PCM_SIZE (BUFFER_SIZE * 2)

int ring_pcm_buffer[RING_PCM_SIZE];
int write_pcm_buffer[WRITE_UNIT];
volatile int rp_pcm = 0;
volatile int wp_pcm = 0;
#endif

#if DSP_SOCKET == 1
/** The socket listener. */
static CTcpIO2 *ptcpIO = 0;
static UINT32 s_pktLen = 0;
static UINT32 s_partial = 0;
static UINT32 commandBuf[MAX_COMMAND_BUFFER_LEN];
/**
 * @brief Handle AWE server messages.
 * @param [in] pAwe             the library instance
 */
static void NotifyFunction(void *pAwe, int count)
{
    CAWELib *pAwelib = (CAWELib *)pAwe;
    SReceiveMessage *pCurrentReplyMsg = ptcpIO->BinaryGetMessage();
    const int msglen = pCurrentReplyMsg->size();

    if (msglen != 0) {
        // This is a binary message.
        UINT32 *txPacket_Host = (UINT32 *)(pCurrentReplyMsg->GetData());
        if (txPacket_Host) {
            UINT32 first = txPacket_Host[0];
            UINT32 len = (first >> 16);

            if (s_pktLen) {
                len = s_pktLen;
            }

            if (len > MAX_COMMAND_BUFFER_LEN) {
                printf("count=%d msglen=%d\n", count, msglen);
                printf("NotifyFunction: packet 0x%08x len %d too big (max %d)\n", first, len,
                       MAX_COMMAND_BUFFER_LEN);
                return;
            }
            if (len * sizeof(int) > s_partial + msglen) {
                // Paxket is not complete, copy partial words.
                // printf("Partial message bytes=%d len=%d s_partial=%d\n",
                //            msglen, len, s_partial);
                memcpy(((char *)commandBuf) + s_partial, txPacket_Host, msglen);
                s_partial += msglen;
                s_pktLen = len;
                return;
            }

            memcpy(((char *)commandBuf) + s_partial, txPacket_Host, msglen);
            s_partial = 0;
            s_pktLen = 0;

            // AWE processes the message.
            int ret = pAwelib->SendCommand(commandBuf, MAX_COMMAND_BUFFER_LEN);
            if (ret < 0) {
                printf("NotifyFunction: SendCommand failed with %d\n", ret);
                return;
            }

            // Verify sane AWE reply.
            len = commandBuf[0] >> 16;
            if (len > MAX_COMMAND_BUFFER_LEN) {
                printf("NotifyFunction: reply packet len %d too big (max %d)\n", len,
                       MAX_COMMAND_BUFFER_LEN);
                return;
            }

            ret = ptcpIO->SendBinaryMessage("", -1, (BYTE *)commandBuf, len * sizeof(UINT32));
            if (ret < 0) {
                printf("NotifyFunction: SendBinaryMessage failed with %d\n", ret);
                return;
            }
        } else {
            printf("NotifyFunction: impossible no message pointer\n");
            return;
        }
    } else {
        printf("NotifyFunction: illegal zero klength message\n");
        return;
    }
}
#endif

#if DSP_DEBUG == 1
void fill_write_buffer(int *input)
{
    int space = rp_pcm - wp_pcm;
    if (space <= 0)
        space = RING_PCM_SIZE + space;
    if (space < READ_FRAME * 2) {
        printf(" OVERFLOW IN fill_write_buffer wp_pcm %d , rp_pcm %d \n", wp_pcm, rp_pcm);
    }

    for (int i = 0; i < READ_FRAME * 2; i = i + 2) {
        ring_pcm_buffer[wp_pcm] = input[0];
        wp_pcm++;
        if (wp_pcm == RING_PCM_SIZE)
            wp_pcm = 0;
        ring_pcm_buffer[wp_pcm] = 0;
        wp_pcm++;
        if (wp_pcm == RING_PCM_SIZE)
            wp_pcm = 0;

        input += 2;
    }
}

void fill_write_buffer1(int *input)
{
    int space = rp_pcm - wp_pcm;
    if (space <= 0)
        space = RING_PCM_SIZE + space;

    if (space < READ_FRAME * 2) {
        printf(" OVERFLOW IN fill_write_buffer1 wp_pcm %d , rp_pcm %d \n", wp_pcm, rp_pcm);
    }

    for (int i = 0; i < READ_FRAME * 8; i = i + 8) {
        ring_pcm_buffer[wp_pcm] = input[0];
        wp_pcm++;

        if (wp_pcm == RING_PCM_SIZE)
            wp_pcm = 0;
        ring_pcm_buffer[wp_pcm] = input[1];
        wp_pcm++;
        if (wp_pcm == RING_PCM_SIZE)
            wp_pcm = 0;

        input = input + 8;
    }
}

void *do_pcm_write(void *priv)
{
    printf("inside thread do_debug_pcm_write \n");
    int err;
    int dir;
    unsigned int sampleRate = SAMPLE_RATE;
    snd_pcm_uframes_t periodSize = PERIOD_SIZE;
    snd_pcm_uframes_t bufferSize = BUFFER_SIZE;
    int level;
    int first = 0;

    wm_alsa_context_t *playback_alsa_context = NULL;

    playback_alsa_context = alsa_context_create(NULL);
    if (playback_alsa_context) {
        if (!playback_alsa_context->device_name) {
            playback_alsa_context->device_name = strdup(WRITE_DEVICE_NAME);
        }
        playback_alsa_context->stream_type = SND_PCM_STREAM_PLAYBACK;
        playback_alsa_context->format = SND_PCM_FORMAT_S32_LE;
        playback_alsa_context->access = SND_PCM_ACCESS_RW_INTERLEAVED;
        playback_alsa_context->channels = 2;
        playback_alsa_context->rate = SAMPLE_RATE;
        playback_alsa_context->latency_ms = 100;
        playback_alsa_context->bits_per_sample = 32;
        playback_alsa_context->block_mode = 1;
    } else {
        printf("[AFE/ALSA] failed to creaet alsa context\n");
        goto error;
    }

    err = capture_alsa_open(playback_alsa_context);
    if (err) {
        printf("[AFE/ALSA] Unable to prepare alsa device: %s\n",
               playback_alsa_context->device_name);
        goto error;
    }

    // open alsa device succeeds.
    capture_alsa_dump_config(playback_alsa_context);

    while (write_running) {
        level = wp_pcm - rp_pcm;
        if (level < 0)
            level = RING_PCM_SIZE + level;

        if (first == 0 && level < RING_PCM_SIZE * 3 / 4) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        } else
            first = 1;

        if (level >= WRITE_UNIT) {
            for (int i = 0; i < WRITE_UNIT; i++) {
                write_pcm_buffer[i] = ring_pcm_buffer[rp_pcm];
                rp_pcm++;
                if (rp_pcm == RING_PCM_SIZE)
                    rp_pcm = 0;
            }

            err =
                snd_pcm_writei(playback_alsa_context->handle, &write_pcm_buffer[0], WRITE_UNIT / 2);
            if (err < 0) {
                printf("[AFE/ALSA] alsa pcm write recovery\n");
                if (capture_alsa_recovery(playback_alsa_context, err, 1000)) {
                    printf("[AFE/ALSA] ret=%d, recover failed\n", err);
                    break;
                } else {
                    printf("[AFE/ALSA] ret=%d, recover succeed\n", err);
                }
            }
        }
    }

error:
    if (playback_alsa_context)
        alsa_context_destroy(playback_alsa_context);
    printf("[AFE/ALSA] do_pcm_write thread quited!!!\n");
}
#endif

void setup_DSP()
{
    UINT32 wireId1 = 1;
    UINT32 wireId2 = 2;
    UINT32 layoutId = 0;
    int error = 0;
    char *file;

    // Create AWE.
    pAwelib = AWELibraryFactory();
    error = pAwelib->CreateEx("libtest", 1e9f, 1e7f, true);
    if (error < 0) {
        printf("[DSP] Create AWE failed with %d\n", error);
        delete pAwelib;
        exit(1);
    }
    printf("[DSP Using library '%s'\n", pAwelib->GetLibraryVersion());

    char *defaultfile = "AMLogic_VUI_Solution_Model.awb";
    if (g_configFile)
        file = g_configFile;
    else
        file = defaultfile;

    uint32_t pos;
    // Only load a layout if asked.
    printf("load file from:%s\n", file);
    error = pAwelib->LoadAwbFile(file, &pos);
    if (error < 0) {
        printf("[DSP] LoadAwbFile failed with %d(%s)\n", error, file);
    } else {
        printf("[DSP] LoadAwbFile Success\n");
        goto load_awb_success;
    }

#if !defined(SECURITY_IMPROVE) || defined(SECURITY_DEBUGPRINT)
    error = pAwelib->LoadAwbFileUnencrypted(file, &pos);
    if (error < 0) {
        printf("[DSP] LoadAwbFileUnencrypted failed with %d(%s)\n", error, file);
    } else {
        printf("[DSP] LoadAwbFileUnencrypted Success\n");
        goto load_awb_success;
    }
#endif

    delete pAwelib;
    exit(1);

load_awb_success:
    UINT32 in_complex = 0;
    UINT32 out_complex = 0;
    error = pAwelib->PinProps(in_samps, in_chans, in_complex, wireId1, out_samps, out_chans,
                              out_complex, wireId2);

    if (error < 0) {
        printf("PinProps failed with %d\n", error);
        delete pAwelib;
        exit(1);
    }

    if (error > 0) {
        // We have a layout.
        if (in_complex) {
            in_chans *= 2;
        }

        if (out_complex) {
            out_chans *= 2;
        }
        printf("[DSP] AWB layout In: %d samples, %d chans ID=%d; out: %d samples, %d chans ID=%d\n",
               in_samps, in_chans, wireId1, out_samps, out_chans, wireId2);
        inCount = in_samps * in_chans;
        outCount = out_samps * out_chans;
    } else {
        // We have no layout.
        printf("[DSP] AWB No layout\n");
    }

    // Now we know the IDs, set them.
    pAwelib->SetLayoutAddresses(wireId1, wireId2, layoutId);

    Sample master_gain;
    master_gain.fVal = 0.0f;
    Sample channel_gains[10];

    master_gain = pAwelib->FetchValue(
        MAKE_ADDRESS(AWE_MicTrimGains_ID, AWE_MicTrimGains_masterGain_OFFSET), &error);
    if (0 == error) {
        printf("[DSP] MicTrimGains_masterGain preset is %f dB\n", master_gain.fVal);
    } else {
        printf("[DSP] FetchValue mic master_gain error %d\n", error);
    }

    error = pAwelib->FetchValues(
        MAKE_ADDRESS(AWE_MicTrimGains_ID, AWE_MicTrimGains_trimGain_OFFSET), 10, channel_gains, 0);
    if (0 == error) {
        printf("[DSP] MicTrimGains_trimGain preset is (dB):\n");
        for (int i = 0; i < 10; ++i) {
            printf(" %f", channel_gains[i].fVal);
        }
        printf("\n");
    } else {
        printf("[DSP] FetchValues mic trim_gains error %d\n", error);
    }

    master_gain.fVal = 0.0f;
    for (int i = 0; i < 10; ++i) {
        channel_gains[i].fVal = ((MIC_NULL == g_dspc_mic_array_mapping[i] || i >= 8)
                                     ? 0
                                     : (TARGET_LEVEL_DBFS - MIC_SENSITIVITY_DBFS));
    }

    error = pAwelib->SetValue(MAKE_ADDRESS(AWE_MicTrimGains_ID, AWE_MicTrimGains_masterGain_OFFSET),
                              master_gain);
    if (0 == error) {
        printf("[DSP] MicTrimGains_masterGain set to %f dB\n", master_gain.fVal);
    } else {
        printf("[DSP] SetValue mic master_gain error %d\n", error);
    }

    error = pAwelib->SetValues(MAKE_ADDRESS(AWE_MicTrimGains_ID, AWE_MicTrimGains_trimGain_OFFSET),
                               10, channel_gains, 0);
    if (0 == error) {
        printf("[DSP] MicTrimGains_trimGain set to %f dB\n",
               (TARGET_LEVEL_DBFS - MIC_SENSITIVITY_DBFS));
    } else {
        printf("[DSP] SetValues mic trim_gains error %d\n", error);
    }

    in_samples.resize(inCount);
    micraw_samples.resize(inCount);
#if defined(MICRAW_FORMAT_CHG)
    micraw_samples_resample.resize(inCount);
#endif
#if defined(ENABLE_RECORD_SERVER)
    recServer_samples.resize(inCount);
#endif
    out_samples.resize(outCount);
    ring_buffer.resize(RING_BUFFER_SIZE);

#if DSP_SOCKET == 1
    // Listen for AWE server on 15002.
    UINT32 listenPort = 15002;
    ptcpIO = new CTcpIO2();
    ptcpIO->SetNotifyFunction(pAwelib, NotifyFunction);
    HRESULT hr = ptcpIO->CreateBinaryListenerSocket(listenPort);
    printf("[DSPC AFE] CreateBinaryListenerSocket\n");
    if (FAILED(hr)) {
        printf("Could not open socket on %d failed with %d\n", listenPort, hr);
        delete ptcpIO;
        // delete pAwelib;
        // exit(1);
    }
#endif
}

void *initialize(SOFT_PREP_INIT_PARAM input)
{
    int processed_channel;
    Soft_prep_dspc_wrapper *handle =
        (Soft_prep_dspc_wrapper *)malloc(sizeof(Soft_prep_dspc_wrapper));
    if (!handle) {
        printf("malloc handle failed\n");
        return NULL;
    }
    memset(handle, 0, sizeof(Soft_prep_dspc_wrapper));

    if (input.configFile && strlen(input.configFile)) {
        asprintf(&g_configFile, "%s", input.configFile);
    }

    handle->mic_numb = input.mic_numb;
    if (input.mic_numb == 2)
        g_dspc_mic_array_mapping = g_dspc_mic_array_2;
    else if (input.mic_numb == 4)
        g_dspc_mic_array_mapping = g_dspc_mic_array_4;
    else if (input.mic_numb == 6)
        g_dspc_mic_array_mapping = g_dspc_mic_array_6;
    else
        printf("unsopport mic number!\n");

    setup_DSP();

#if defined(WWE_DOUBLE_TRIGGER)
    processed_channel = 4;
#else
    processed_channel = 2;
#endif

    audioData_SDS.resize(PREFERRED_SAMPLES_PER_CALLBACK_FOR_ALSA * processed_channel);

    read_running = 1;
    dsp_running = 1;
    wwe_enable = 1;
    pcm_read_thread = std::thread(&do_pcm_read, handle);
#if DSP_DEBUG == 1
    pcm_debug_write_thread = std::thread(&do_pcm_write, handle);
#endif
    if (dump_data) {
        fp_input = fopen("/tmp/input.bin", "wb");
        fp_output = fopen("/tmp/output.bin", "wb");
    }

    return (void *)handle;
}

int deInitialize(void *handle)
{
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;

    dsp_running = 0;
    read_running = 0;
    wwe_enable = 0;

    if (dsp_process > 0) {
        pthread_join(dsp_process, NULL);
        dsp_process = 0;
    }

#if DSP_SOCKET == 1
    delete ptcpIO;
#endif

    pcm_read_thread.join();
#if DSP_DEBUG == 1
    if (write_running) {
        write_running = 0;
        pcm_debug_write_thread.join();
    }
#endif
    pAwelib->Destroy();
    pAwelib = NULL;
    free(wrapper);
    if (g_configFile) {
        free(g_configFile);
        g_configFile = NULL;
    }
    return 0;
}

int setFillCallback(int handle, soft_prep_capture_CBACK *callback_func, void *opaque)
{
    m_mutex_enable.lock();
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;
    wrapper->fill_callback = callback_func;
    wrapper->fillPriv = opaque;
    m_mutex_enable.unlock();
    return 0;
}

int setWakeupCallBack(int handle, soft_prep_wakeup_CBACK *callback_func, void *opaque)
{
    m_mutex_enable.lock();
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;
    wrapper->wakeup_callback = callback_func;
    wrapper->wakeupPriv = opaque;
    m_mutex_enable.unlock();
    return 0;
}

int setMicrawCallback(int handle, soft_prep_micraw_CBACK *callback_func, void *opaque)
{
    m_mutex_enable.lock();
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;
    wrapper->micrawCallback = callback_func;
    wrapper->micrawPriv = opaque;
    m_mutex_enable.unlock();
    return 0;
}

int setObserverStateCallBack(int handle, soft_prep_ObserverState_CBACK *callback_func, void *opaque)
{
    m_mutex_enable.lock();
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;
    wrapper->ObserverState_callback = callback_func;
    wrapper->observerPriv = opaque;
    m_mutex_enable.unlock();
    return 0;
}

#if defined(ENABLE_RECORD_SERVER)
int setRecordCallBack(int handle, soft_prep_record_CBACK *callback_func, void *recPriv)
{
    m_mutex_enable.lock();
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)handle;
    wrapper->record_callback = callback_func;
    wrapper->recPriv = recPriv;
    m_mutex_enable.unlock();
    return 0;
}

int recServer_processs(Soft_prep_dspc_wrapper *handle, int *input, int frame_num, void *recPriv,
                       int nStreamIndex)
{
    m_mutex_enable.lock();
    if (handle->record_callback && mic_run_flag) {
        int i;
        int j;
        int outputCount = 0;
        int nChannel = 8;
        int *output = &recServer_samples[0];
        stMiscParam *recParam = (stMiscParam *)recPriv;
        memset(output, 0, frame_num * 10);
        switch (nStreamIndex) {
        case 0:
            nChannel = 8;
            break;
        case 1:
            nChannel = 4;
            break;
        case 2:
            nChannel = 4;
            break;
        }
        for (i = 0; i < frame_num * nChannel; i = i + nChannel) {
            for (j = 0; j < nChannel; j++) {
                if (1 << j & recParam->nPrepBitMask) {
                    output[outputCount++] = input[j];
                }
            }
            input += nChannel;
        }
        handle->record_callback((char *)output, outputCount * 4, recPriv, nStreamIndex);
    }
    m_mutex_enable.unlock();
}

#endif

int enableStreamingMicrophoneData(void *handle, int value)
{
    printf("enableStreamingMicrophoneData:%d\n", value);
    m_mutex_enable.lock();
    mic_run_flag = value;
    m_mutex_enable.unlock();
    return 0;
}

int enableWWE(void *handle, int value)
{
    printf("enableWWE:%d\n", value);
    m_mutex_enable.lock();
    wwe_enable = value;
    m_mutex_enable.unlock();
    return 0;
}

// in : 32bit , 48K , 1 ch (outsamples from DSP )
// out : 16bit , 16K , 1 ch ( every 3rd sample ) ( audioData_SDS )
void convert_DSP(int *input, short *output, int frame_num)
{
    for (int i = 0; i < frame_num; i += 3) {
        *output = *input >> 14;
        output++;
        input += 3;
    }
}

// in : 32bit , 48K , 2 ch (outsamples from DSP )
// out : 16bit , 16K , 1 ch ( every 6th sample ) ( audioData_SDS )
void convert_DSP_2ch(int *input, short *output, int frame_num)
{
    for (int i = 0; i < frame_num * 2; i += 6) {
        *output = *input >> 16;
        output++;
        input += 6;
    }
}

// in : 32bit , 48K , 2 ch (outsamples from DSP )
// out : 16bit , 16K , 2 ch ( every 3th sample ) ( audioData_SDS )
void convert_DSP_2ch_real(int *input, short *output, int frame_num)
{
    for (int i = 0; i < frame_num * 2; i += 3 * 2) {
        *output++ = input[i] >> 16;
        *output++ = input[i + 1] >> 16;
    }
}

// in : 32bit , 48K , 4 ch (outsamples from DSP )
// out : 16bit , 16K , 4 ch ( every 3th sample ) ( audioData_SDS )
void convert_DSP_4ch_real(int *input, short *output, int frame_num)
{
    for (int i = 0; i < frame_num * 4; i += 3 * 4) {
        *output++ = input[i] >> 16;
        *output++ = input[i + 1] >> 16;
        *output++ = input[i + 2] >> 16;
        *output++ = input[i + 3] >> 16;
    }
}

// in : 32bit , 48K , 8 channels
// out : 16bit , 48K , 1 channel
int convert(int *input, short *output, int frame_num)
{
    int i;
    for (i = 0; i < frame_num; i++) {
        output[i] = input[8 * i] >> 14;
    }
    return 0;
}

// input : 8 ch , 0,1 feedback , 2,3,4,5 are data , 6,7 is empty , 32 bit
// output : 10 ch , 0,1,2,3,4,5 : data , 6,7 not used , 8,9 : feedback , 32 bit
int convert1(int *input, int *output, int frame_num)
{
    int i;
    memset(output, 0, frame_num * 10);
    for (i = 0; i < frame_num * 8; i = i + 8) {
#if 0
        output[0] = input [0];
        output[1] = input [1];
        output[4] = input [2];
        output[5] = input [3];
        output[2] = input [4];
        output[3] = input [5];
        output[8] = input [6];
        output[9] = input [7];
#else
        output[0] =
            (g_dspc_mic_array_mapping[0] != MIC_NULL) ? input[g_dspc_mic_array_mapping[0]] : 0;
        output[1] =
            (g_dspc_mic_array_mapping[1] != MIC_NULL) ? input[g_dspc_mic_array_mapping[1]] : 0;
        output[2] =
            (g_dspc_mic_array_mapping[2] != MIC_NULL) ? input[g_dspc_mic_array_mapping[2]] : 0;
        output[3] =
            (g_dspc_mic_array_mapping[3] != MIC_NULL) ? input[g_dspc_mic_array_mapping[3]] : 0;
        output[4] =
            (g_dspc_mic_array_mapping[4] != MIC_NULL) ? input[g_dspc_mic_array_mapping[4]] : 0;
        output[5] =
            (g_dspc_mic_array_mapping[5] != MIC_NULL) ? input[g_dspc_mic_array_mapping[5]] : 0;
        output[6] =
            (g_dspc_mic_array_mapping[6] != MIC_NULL) ? input[g_dspc_mic_array_mapping[6]] : 0;
        output[7] =
            (g_dspc_mic_array_mapping[7] != MIC_NULL) ? input[g_dspc_mic_array_mapping[7]] : 0;
        output[8] =
            (g_dspc_mic_array_mapping[8] != MIC_NULL) ? input[g_dspc_mic_array_mapping[8]] : 0;
        output[9] =
            (g_dspc_mic_array_mapping[9] != MIC_NULL) ? input[g_dspc_mic_array_mapping[9]] : 0;
#endif
        input += 8;
        output += 10;
    }

    return 0;
}

// input : 8 ch , 0,1 feedback , 2,3,4,5 are data , 6,7 is empty , 32 bit
// output : depend on mic
int micraw_convert(int ch, int *input, int *output, int frame_num)
{
    int i;
    memset(output, 0, frame_num * 10);
    for (i = 0; i < frame_num * 8; i = i + 8) {
        output[0] = input[0];
        output[1] = input[1];
        if (ch > 2) {
            output[2] = input[2];
            output[3] = input[3];
        }
        if (ch > 4) {
            output[4] = input[4];
            output[5] = input[5];
        }

        input += 8;
        output += ch;
    }

    return 0;
}

int resample_process2(int *inBuf, int inSize, short *outBuf, int *outSize)
{
    int processed = 0;
    int temp;
    for (int i = 0; i < inSize; i += 3) {
        temp = *inBuf;
        *outBuf = (short)(temp >> 16);
        outBuf++;
        inBuf += 3;
        processed++;
    }
    *outSize = processed;
    // printf("innum:%d out:%d\n", inSize, processed);
    return processed;
}

#ifdef BLUETOOTH_SRC_LOOP_CAPTURE_ENABLE
static int bluetoothStreamWrite(int *input, int frame_num)
{
    int i, j;
    int bt_stream_enable = 0;
    int hw_pcm_enable = 0;
    static int btav_res = -1;

    unsigned short refOriBuffer[READ_FRAME * 2];

    for (i = 0, j = 0; i < frame_num * 8; i = i + 8) {

        refOriBuffer[j++] = (input[6] >> 16) & 0xffff;
        refOriBuffer[j++] = (input[7] >> 16) & 0xffff;
        input += 8;
    }

    int tfd = open("/sys/class/tdm_state/amp_table", O_RDONLY);
    if (tfd >= 0) {
        char mute_flag[4] = {0};
        if (read(tfd, (void *)mute_flag, 4) > 0) {
            bt_stream_enable = !atoi(mute_flag);
        }
        close(tfd);
    }

    tfd = open("/sys/class/tdm_state/pcm_table", O_RDONLY);
    if (tfd >= 0) {
        char pcm_state[4] = {0};
        if (read(tfd, (void *)pcm_state, 4) > 0) {
            hw_pcm_enable = atoi(pcm_state);
        }
        close(tfd);
    }

    if (bt_stream_enable && hw_pcm_enable && (access("/tmp/loop_capture_pcm", F_OK) != -1)) {
        if (btav_res == -1) {
            btav_res = open("/tmp/loop_capture_pcm", O_WRONLY | O_NONBLOCK);
        }
        if (btav_res >= 0) {
            int ret = write(btav_res, refOriBuffer, frame_num * 4);
            if (ret < 0) {
                close(btav_res);
                btav_res = -1;
            }
        }
    }

    return 0;
}
#endif
void do_dsp_processing_fn(int *in_samples, int *out_samples, int inCount, int outCount)
{

    int error = 0;
    Sample retVal;

    // printf(" do_dsp_processing_fn \n");
    loopCount += PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP;
    if (loopCount > 16000 * 8) {
        // printf("detection loop heart beat ...\n");
        loopCount = 0;
    }

    if (pAwelib->IsStarted()) {
        error = pAwelib->PumpAudio(&in_samples[0], &out_samples[0], inCount, outCount);
    } else {
        memset(&out_samples[0], 0, outCount * 4);
    }

    // printf(" post PumpAudio \n");
    if (dump_data) {
        fwrite(&in_samples[0], PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * 10, sizeof(int), fp_input);
        fflush(fp_input);
    }

#if DSP_DEBUG == 1
    if (write_running)
        fill_write_buffer(&out_samples[0]);
#endif

    if (error < 0) {
        printf("[DSP pump of audio failed with %d\n", error);
        delete pAwelib;
        exit(1);
    }

    /*error = pAwelib->FetchValues(MAKE_ADDRESS(AWE_isTriggered_ID, AWE_isTriggered_value_OFFSET),
    1,
                                 &retVal, 0);
    result = retVal.iVal;

    error = pAwelib->FetchValues(MAKE_ADDRESS(AWE_startIndex_ID, AWE_startIndex_value_OFFSET), 1,
                                 startIndex, 0);
    // printf(" error startindex %d \n" , error);
    error = pAwelib->FetchValues(MAKE_ADDRESS(AWE_endIndex_ID, AWE_endIndex_value_OFFSET), 1,
                                 endIndex, 0);*/

    // printf(" error endindex %d \n" , error);
    // printf("[DSP] detectionResult %d \n" , result);
}

void *do_dsp_processing(void *priv)
{
    int result;
    Sample DSP_state;
    // Sample startIndex[AWE_startIndex_value_SIZE];
    // Sample endIndex[AWE_endIndex_value_SIZE];
    uint64_t total_samps = 0;
    Soft_prep_dspc_wrapper *wrapper = (Soft_prep_dspc_wrapper *)(priv);
    int detection_count = 0;
    int out_size = 0;

    int level;
    printf("%s +++ \n", __func__);
    while (dsp_running) {
        std::unique_lock<std::mutex> mlock(rb_mutex);
        rb_cond.wait(mlock);
        level = wp - rp;
        mlock.unlock();

        if (level < 0)
            level = RING_BUFFER_SIZE + level;
        // printf(" do_dsp_processing level %d \n" , level );
        while (level >= (int)PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * NUM_INPUT_CHANNELS) {

            convert1(&ring_buffer[rp], &in_samples[0], PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP);
            if (wrapper->micrawCallback) {
                micraw_convert(wrapper->mic_numb, &ring_buffer[rp], &micraw_samples[0],
                               PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP);
#if defined(MICRAW_FORMAT_CHG)
                resample_process2(&micraw_samples[0],
                                  PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * wrapper->mic_numb,
                                  (short *)&micraw_samples_resample[0], &out_size);
                wrapper->micrawCallback(wrapper->micrawPriv, (char *)&micraw_samples_resample[0],
                                        out_size * 2);
#else
                wrapper->micrawCallback(wrapper->micrawPriv, (char *)&micraw_samples[0],
                                        PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * wrapper->mic_numb *
                                            4);
#endif
            }

// fill_write_buffer1(&ring_buffer[rp]);
#if defined(ENABLE_RECORD_SERVER)
            recServer_processs(wrapper, &ring_buffer[rp], PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP,
                               wrapper->recPriv, 0);
#endif

            int state = 0;
            m_mutex_enable.lock();
            if (wrapper->ObserverState_callback) {
                state = wrapper->ObserverState_callback(wrapper->observerPriv);
            }
            m_mutex_enable.unlock();
            switch (state) {
            case 0: /*AudioInputProcessor::ObserverInterface::State::IDLE:*/
                DSP_state.iVal = 0;
                break;
            case 1: /*AudioInputProcessor::ObserverInterface::State::EXPECTING_SPEECH:*/
                DSP_state.iVal = 1;
                break;
            case 2: /*AudioInputProcessor::ObserverInterface::State::RECOGNIZING:*/
                DSP_state.iVal = 2;
                break;
            case 3: /*AudioInputProcessor::ObserverInterface::State::BUSY:*/
                DSP_state.iVal = 3;
            }

            if (DSP_state.iVal != prev_state) {
                int ret = pAwelib->SetValues(MAKE_ADDRESS(AWE_VRState_ID, AWE_VRState_value_OFFSET),
                                             1, &DSP_state, 0);
                if (ret != 0)
                    printf(" Error from SetValues %d ", ret);

                // printf(" Switch from state %d to state %d \n" , prev_state , DSP_state.iVal);
                prev_state = DSP_state.iVal;
            }

            do_dsp_processing_fn(&in_samples[0], &out_samples[0], inCount, outCount);

// TODO: Fix bug of recording DSPC output(48kHz)
#if 0 // defined(ENABLE_RECORD_SERVER)
            recServer_processs(wrapper, &out_samples[0], PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP,
                               wrapper->recPriv, 1);
#endif

#if defined(WWE_DOUBLE_TRIGGER)
            convert_DSP_4ch_real(&out_samples[0], &audioData_SDS[0],
                                 PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP);
#else
            convert_DSP_2ch_real(&out_samples[0], &audioData_SDS[0],
                                 PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP);
#endif

            if (dump_data) {
                // fwrite(&audioData_SDS[0] , PREFERRED_SAMPLES_PER_CALLBACK_FOR_ALSA*1 ,
                // sizeof(short) , fp_output);
                fwrite(&audioData_SDS[0], PREFERRED_SAMPLES_PER_CALLBACK_FOR_ALSA * 2,
                       sizeof(short), fp_output);
                fflush(fp_output);
            }
            ssize_t returnCode = 1;
            m_mutex_enable.lock();
            if (wrapper->fill_callback && mic_run_flag) {
                returnCode = wrapper->fill_callback(wrapper->fillPriv, (char *)audioData_SDS.data(),
                                                    audioData_SDS.size() * sizeof(short));
            }
            m_mutex_enable.unlock();
            if (returnCode <= 0) {
                printf("Failed to write to stream.");
            }

            total_samps += audioData_SDS.size();

            if (result == 1) {
                detection_count++;
                // printf(" wake word detected %d \n", detection_count);
                m_mutex_enable.lock();
                if (wwe_enable && wrapper->wakeup_callback) {
                    WWE_RESULT wwe_res;
                    wwe_res.detected_numb = detection_count;
                    returnCode = wrapper->wakeup_callback(wrapper->wakeupPriv, &wwe_res);
                }
                m_mutex_enable.unlock();
            }

            rp = rp + (int)PREFERRED_SAMPLES_PER_CALLBACK_FOR_DSP * NUM_INPUT_CHANNELS;
            if (rp > (int)RING_BUFFER_SIZE) {
                printf(" rp out of bound \n");
            }

            if (rp == RING_BUFFER_SIZE) {
                rp = 0;
            }
            level = wp - rp;
            if (level < 0) {
                level = RING_BUFFER_SIZE + level;
            }
        }
    }
}

void *do_pcm_read(void *priv)
{
    printf("%s +++ \n", __func__);
    pthread_attr_t attr;
    struct sched_param param;
    unsigned int buffer_time = 0;
    unsigned int period_time = 0;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 80;
    pthread_attr_setschedparam(&attr, &param);

    pthread_create(&dsp_process, &attr, do_dsp_processing, (void *)priv);

    int err;
    int buffer_len = READ_FRAME * 8 * 4;
    int buffer[buffer_len];
    memset(buffer, 0, buffer_len);

    wm_alsa_context_t *capture_alsa_context = NULL;

    capture_alsa_context = alsa_context_create(NULL);
    if (capture_alsa_context) {
        if (!capture_alsa_context->device_name) {
            capture_alsa_context->device_name = strdup(REC_DEVICE_NAME);
        }
        capture_alsa_context->stream_type = SND_PCM_STREAM_CAPTURE;
        capture_alsa_context->format = SND_PCM_FORMAT_S32_LE;
        capture_alsa_context->access = SND_PCM_ACCESS_RW_INTERLEAVED;
        capture_alsa_context->channels = 8;
        capture_alsa_context->rate = SAMPLE_RATE;
        capture_alsa_context->latency_ms = 100;
        capture_alsa_context->bits_per_sample = 32;
        capture_alsa_context->block_mode = 1;
    } else {
        printf("[AFE/ALSA] failed to create alsa context\n");
        goto error;
    }

    err = capture_alsa_open(capture_alsa_context);
    if (err) {
        printf("[AFE/ALSA] Unable to prepare alsa device: %s\n", capture_alsa_context->device_name);
        goto error;
    }

    // open alsa device succeeds.
    capture_alsa_dump_config(capture_alsa_context);

    while (read_running) {
        if (!mic_run_flag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        err = capture_alsa_read(capture_alsa_context, buffer, buffer_len);
        if (err < 0) {
            if (capture_alsa_recovery(capture_alsa_context, err, 1000)) {
                printf("[AFE/ALSA] ret=%d, recover failed\n", err);
                break;
            } else {
                printf("[AFE/ALSA] ret=%d, recover succeed\n", err);
            }
        }

        int space = wp - rp;
        if (space <= 0)
            space = space + RING_BUFFER_SIZE;
        if (space < (int)READ_FRAME * NUM_INPUT_CHANNELS) {
            printf("Ringbuffer OVERFLOW \n");
        }

        memcpy(&ring_buffer[wp], (int *)buffer, READ_FRAME * 4 * NUM_INPUT_CHANNELS);

        wp = wp + READ_FRAME * NUM_INPUT_CHANNELS;

        if (wp > (int)RING_BUFFER_SIZE) {
            printf("RingBuffer wp out of bounds \n");
        }

        if (wp == RING_BUFFER_SIZE) {
            wp = 0;
        }
#ifdef BLUETOOTH_SRC_LOOP_CAPTURE_ENABLE
        bluetoothStreamWrite(buffer, READ_FRAME);
#endif
        // Wake up DSPC processing thread
        std::lock_guard<std::mutex> guard(rb_mutex);
        rb_cond.notify_one();
    } // end of while loop

error:
    if (capture_alsa_context)
        alsa_context_destroy(capture_alsa_context);
    printf("[AFE/ALSA] do_pcm_read thread quited!!!\n");
}

#if defined(DSPC_MAIN)
int GetAudioInputProcessorObserverState(void *handle) { return 1; }
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

void usage(char *pName)
{
    printf("usage: %s [dumpfile] [time] \r\n"
           "       capture [time] second data\r\n"
           "       [dumpfile] 1: dump; other: do not dump\r\n"
           "       by default, it will be 10s without dump if not set\r\n",
           pName);
}

int main(int argc, char **argv)
{
    SOFT_PREP_INIT_PARAM input;
    void *handle = NULL;
    int seconds = 10;
    long long starttime;
    if (argc > 3) {
        usage(argv[0]);
        return -1;
    }

    if (argc == 2) {
        dump_data = atoi(argv[1]);
    }
    if (argc == 3) {
        dump_data = atoi(argv[1]);
        seconds = atoi(argv[2]);
    }

    if (dump_data != 1) {
        dump_data = 0;
    }

// set loopback enable
#if defined(NAVER_LINE)
    system("amixer cset numid=6 1");
#else
    system("amixer cset numid=26 1");
#endif

    printf("starting %s to capture %d seconds %s dump data\r\n\r\n", argv[0], seconds,
           dump_data ? "With" : "Without");

#if DSP_DEBUG == 1
    write_running = 1;
#endif
    input.mic_numb = 4;
    handle = initialize(input);
    enableStreamingMicrophoneData(handle, 1);
    starttime = tickSincePowerOn();

    while (tickSincePowerOn() - starttime < seconds * 1000) {
        usleep(10000);
    }

    printf("time out...\nstopping dump\r\n\r\n");
    enableStreamingMicrophoneData(handle, 0);
    deInitialize(handle);
}
#endif

#endif
