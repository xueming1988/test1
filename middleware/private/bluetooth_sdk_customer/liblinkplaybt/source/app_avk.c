/*****************************************************************************
 **
 **  Name:           app_avk.c
 **
 **  Description:    Bluetooth Manager application
 **
 **  Copyright (c) 2009-2015, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"

#include "buildcfg.h"

#include "bsa_api.h"
#include "bsa_avk_api.h"

#include "gki.h"
#include "uipc.h"

#include "app_xml_utils.h"
#include "app_disc.h"
#include "app_utils.h"
#include "app_wav.h"

#ifdef PCM_ALSA
#include "alsa/asoundlib.h"
#define APP_AVK_ASLA_DEV "default"
#endif

#include "app_avk.h"

int g_BT_fc_enable = 1;
//#define BSA_AVK_AAC_SUPPORTED
#undef BSA_AVK_AAC_SUPPORTED
/*
 * Defines
 */
pthread_mutex_t mutex_callback = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_aac = PTHREAD_MUTEX_INITIALIZER;

#define APP_AVK_SOUND_FILE "test_avk"

#ifndef BSA_AVK_SECURITY
#define BSA_AVK_SECURITY BSA_SEC_AUTHORIZATION
#endif
#ifndef BSA_AVK_FEATURES
#define BSA_AVK_FEATURES                                                                           \
    (BSA_AVK_FEAT_RCCT | BSA_AVK_FEAT_RCTG | BSA_AVK_FEAT_VENDOR | BSA_AVK_FEAT_METADATA |         \
     BSA_AVK_FEAT_BROWSE | (1 << BSA_AVK_FEAT_DELAY_RPT))
#endif

#ifndef BSA_AVK_DUMP_RX_DATA
#define BSA_AVK_DUMP_RX_DATA FALSE
#endif

//#ifndef BSA_AVK_AAC_SUPPORTED
//#define BSA_AVK_AAC_SUPPORTED FALSE
//#endif

#ifdef BSA_AVK_AAC_SUPPORTED
#include <aacdecoder_lib.h>
#define DECODE_BUF_LEN (8 * 2 * 1024)

typedef struct {
    HANDLE_AACDECODER aac_handle;
    BOOLEAN has_aac_handle; // True if aac_handle is valid
    INT_PCM *decode_buf;
    //   decoded_data_callback_t decode_callback;
} tA2DP_AAC_DECODER_CB;
static tA2DP_AAC_DECODER_CB a2dp_aac_decoder_cb;
#endif
/* bitmask of events that BSA client is interested in registering for notifications */
tBSA_AVK_REG_NOTIFICATIONS reg_notifications =
    (1 << (BSA_AVK_RC_EVT_PLAY_STATUS_CHANGE - 1)) | (1 << (BSA_AVK_RC_EVT_TRACK_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_TRACK_REACHED_END - 1)) |
    (1 << (BSA_AVK_RC_EVT_TRACK_REACHED_START - 1)) |
    //  (1 << (BSA_AVK_RC_EVT_PLAY_POS_CHANGED - 1)) |
    (1 << (BSA_AVK_RC_EVT_BATTERY_STATUS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_SYSTEM_STATUS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_APP_SETTING_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_NOW_PLAYING_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_AVAL_PLAYERS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_ADDR_PLAYER_CHANGE - 1)) | (1 << (BSA_AVK_RC_EVT_UIDS_CHANGE - 1));

/*
 * global variable
 */

tAPP_AVK_CB app_avk_cb;

#ifdef PCM_ALSA
static char *alsa_device = "default"; /* ALSA playback device */
#endif

/*
 * Local functions
 */
static void app_avk_close_wave_file(tAPP_AVK_CONNECTION *connection);
static void app_avk_create_wave_file(void);
static void app_avk_uipc_cback(BT_HDR *p_msg);

static tAvkCallback *s_pCallback = NULL;
static tAPP_AVK_CONNECTION *pStreamingConn = NULL;

static fn_pcm_open g_pcm_open_call = NULL;
static fn_pcm_close g_pcm_close_call = NULL;
static void *g_pcm_context = NULL;
static fn_pcm_write_data g_pcm_write_data_call = NULL;

extern int app_hs_is_calling();

/*******************************************************************************
 **
 ** Function         app_avk_get_label
 **
 ** Description      get transaction label (used to distinguish transactions)
 **
 ** Returns          UINT8
 **
 *******************************************************************************/
UINT8 app_avk_get_label()
{
    if (app_avk_cb.label >= 15)
        app_avk_cb.label = 0;
    return app_avk_cb.label++;
}

/*******************************************************************************
 **
 ** Function         app_avk_create_wave_file
 **
 ** Description     create a new wave file
 **
 ** Returns          void
 **
 *******************************************************************************/
static void app_avk_create_wave_file(void)
{
    int file_index = 0;
    int fd;
    char filename[200];

#ifdef PCM_ALSA
    return;
#endif

    /* If a wav file is currently open => close it */
    if (app_avk_cb.fd != -1) {
        tAPP_AVK_CONNECTION dummy;
        memset(&dummy, 0, sizeof(tAPP_AVK_CONNECTION));
        app_avk_close_wave_file(&dummy);
    }

    do {
        snprintf(filename, sizeof(filename), "%s-%d.wav", APP_AVK_SOUND_FILE, file_index);
        filename[sizeof(filename) - 1] = '\0';
        fd = app_wav_create_file(filename, O_EXCL);
        file_index++;
    } while (fd < 0);

    app_avk_cb.fd = fd;
}
/*******************************************************************************
 **
 ** Function         app_avk_create_aac_file
 **
 ** Description     create a new wave file
 **
 ** Returns          void
 **
 *******************************************************************************/
static void app_avk_create_aac_file(void)
{
    int file_index = 0;
    int fd;
    char filename[200];

    /* If a wav file is currently open => close it */
    if (app_avk_cb.fd != -1) {
        //        close(app_avk_cb.fd);
        //        app_avk_cb.fd = -1;
        return;
    }

    do {
        int flags = O_EXCL | O_RDWR | O_CREAT | O_TRUNC;

        snprintf(filename, sizeof(filename), "%s-%d.aac", APP_AVK_SOUND_FILE, file_index);
        filename[sizeof(filename) - 1] = '\0';

        /* Create file in read/write mode, reset the length field */
        fd = open(filename, flags, 0666);
        if (fd < 0) {
            APP_ERROR1("open(%s) failed: %d", filename, errno);
        }

        file_index++;
    } while ((fd < 0) && (file_index < 100));

    app_avk_cb.fd_codec_type = BSA_AVK_CODEC_M24;
    app_avk_cb.fd = fd;
}

/*******************************************************************************
 **
 ** Function         app_avk_close_wave_file
 **
 ** Description     proper header and close the wave file
 **
 ** Returns          void
 **
 *******************************************************************************/
static void app_avk_close_wave_file(tAPP_AVK_CONNECTION *connection)
{
    tAPP_WAV_FILE_FORMAT format;

#ifdef PCM_ALSA
    return;
#endif

    if (app_avk_cb.fd == -1) {
        printf("app_avk_close_wave_file: no file to close\n");
        return;
    }

    format.bits_per_sample = connection->bit_per_sample;
    format.nb_channels = connection->num_channel;
    format.sample_rate = connection->sample_rate;

    app_wav_close_file(app_avk_cb.fd, &format);

    app_avk_cb.fd = -1;
}

/*******************************************************************************
**
** Function         app_avk_end
**
** Description      This function is used to close AV
**
** Returns          void
**
*******************************************************************************/
void app_avk_end(void)
{
    tBSA_AVK_DISABLE disable_param;
    tBSA_AVK_DEREGISTER deregister_param;

    /* deregister avk */
    BSA_AvkDeregisterInit(&deregister_param);
    BSA_AvkDeregister(&deregister_param);

    /* disable avk */
    BSA_AvkDisableInit(&disable_param);
    BSA_AvkDisable(&disable_param);
}
#ifdef BSA_AVK_AAC_SUPPORTED
static UINT16 app_avk_aac_get_sample_rate(UINT16 index)
{
    UINT32 ret = 0;
    index = index & A2D_M24_IE_SAMP_FREQ_MSK;
    switch (index) {
    case A2D_M24_IE_SAMP_FREQ_8:
        ret = 8000;
        break;
    case A2D_M24_IE_SAMP_FREQ_11:
        ret = 11000;
        break;
    case A2D_M24_IE_SAMP_FREQ_12:
        ret = 12000;
        break;
    case A2D_M24_IE_SAMP_FREQ_16:
        ret = 16000;
        break;
    case A2D_M24_IE_SAMP_FREQ_22:
        ret = 22050;
        break;
    case A2D_M24_IE_SAMP_FREQ_24:
        ret = 24000;
        break;
    case A2D_M24_IE_SAMP_FREQ_32:
        ret = 32000;
        break;
    case A2D_M24_IE_SAMP_FREQ_44:
        ret = 44100;
        break;
    case A2D_M24_IE_SAMP_FREQ_48:
        ret = 48000;
        break;
    case A2D_M24_IE_SAMP_FREQ_64:
        ret = 64000;
        break;
    case A2D_M24_IE_SAMP_FREQ_88:
        ret = 88000;
        break;
    case A2D_M24_IE_SAMP_FREQ_96:
        ret = 96000;
        break;
    default:
        printf("%s: can't find the sample rate, use default 44.1khz\n", __func__);
        ret = 44100;
    }
    return ret;
}
static UINT8 app_avk_aac_get_channel_num(UINT8 index)
{
    UINT32 ret = 0;
    index = index & A2D_M24_IE_CHNL_MSK;
    switch (index) {
    case A2D_M24_IE_CHNL_1:
        ret = 1;
        break;
    case A2D_M24_IE_CHNL_2:
        ret = 2;
        break;
    default:
        APP_ERROR1("%s: can't find the channel num, use default 2\n", __func__);
        ret = 2;
    }
    return ret;
}
static void app_avk_aac_create_decoder(tBSA_AVK_MSG *p_data, tAPP_AVK_CONNECTION *connection)
{
    if (!a2dp_aac_decoder_cb.has_aac_handle) {
        a2dp_aac_decoder_cb.aac_handle = aacDecoder_Open(TT_MP4_LATM_MCP1, 1 /* nrOfLayers */);
        a2dp_aac_decoder_cb.has_aac_handle = TRUE;
        a2dp_aac_decoder_cb.decode_buf =
            malloc(sizeof(a2dp_aac_decoder_cb.decode_buf[0]) * DECODE_BUF_LEN);
    }
    connection->sample_rate =
        app_avk_aac_get_sample_rate(p_data->start.media_receiving.cfg.aac.samp_freq);
    connection->num_channel =
        app_avk_aac_get_channel_num(p_data->start.media_receiving.cfg.aac.chnl);
    connection->bit_per_sample = 16;
    APP_INFO1("Sampling rate:%d, number of channel:%d \n", connection->sample_rate,
              connection->num_channel);
}
static void app_avk_aac_destroy_decoder()
{
    if (a2dp_aac_decoder_cb.has_aac_handle) {
        aacDecoder_Close(a2dp_aac_decoder_cb.aac_handle);
        free(a2dp_aac_decoder_cb.decode_buf);
    }
    memset(&a2dp_aac_decoder_cb, 0, sizeof(a2dp_aac_decoder_cb));
}

#endif
/*******************************************************************************
**
** Function         app_avk_handle_start
**
** Description      This function is the AVK start handler function
**
** Returns          void
**
*******************************************************************************/
static void app_avk_handle_start(tBSA_AVK_MSG *p_data, tAPP_AVK_CONNECTION *connection)
{
    char *avk_format_display[12] = {"SBC", "M12", "M24", "??", "ATRAC", "PCM", "APT-X", "SEC"};
#ifdef PCM_ALSA
    int status;
    snd_pcm_format_t format;
#endif

    if (p_data->start.media_receiving.format <
        (sizeof(avk_format_display) / sizeof(avk_format_display[0]))) {
        APP_INFO1("AVK start: format %s", avk_format_display[p_data->start.media_receiving.format]);
    } else {
        APP_INFO1("AVK start: format code (%u) unknown", p_data->start.media_receiving.format);
    }

    connection->format = p_data->start.media_receiving.format;

    if (connection->format == BSA_AVK_CODEC_PCM) {
        connection->sample_rate = p_data->start.media_receiving.cfg.pcm.sampling_freq;
        connection->num_channel = p_data->start.media_receiving.cfg.pcm.num_channel;
        connection->bit_per_sample = p_data->start.media_receiving.cfg.pcm.bit_per_sample;

    } else if (connection->format == BSA_AVK_CODEC_M24) {
/* create and open an aac file to dump data to */
#ifdef BSA_AVK_AAC_SUPPORTED
        pthread_mutex_lock(&mutex_aac);
        app_avk_aac_create_decoder(p_data, connection);
        pthread_mutex_unlock(&mutex_aac);
#endif
        /* Initialize the decoder with the format information here */
    }
#ifdef PCM_ALSA
    if (connection->bit_per_sample == 8)
        format = SND_PCM_FORMAT_U8;
    else
        format = SND_PCM_FORMAT_S16_LE;
    pthread_mutex_lock(&mutex_callback);
    if (app_avk_cb.pcm_open_call)
        app_avk_cb.alsa_handle =
            app_avk_cb.pcm_open_call(g_pcm_context, format, connection->sample_rate,
                                     connection->num_channel, connection->bit_per_sample);
    pthread_mutex_unlock(&mutex_callback);
#endif
}

/*******************************************************************************
 **
 ** Function         app_avk_cback
 **
 ** Description      This function is the AVK callback function
 **
 ** Returns          void
 **
 *******************************************************************************/
static void app_avk_cback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    switch (event) {
    case BSA_AVK_OPEN_EVT:
        APP_DEBUG1("BSA_AVK_OPEN_EVT status 0x%x", p_data->open.status);

        if (p_data->open.status == BSA_SUCCESS) {
            connection = app_avk_add_connection(p_data->open.bd_addr);

            if (connection == NULL) {
                APP_DEBUG0("BSA_AVK_OPEN_EVT cannot allocate connection cb");
                break;
            }

            connection->ccb_handle = p_data->open.ccb_handle;
            connection->is_open = TRUE;
            connection->is_streaming_open = FALSE;
            APP_INFO1("AVK connected to device index %d \n", connection->index);
        }

        app_avk_cb.open_pending = FALSE;

        APP_INFO1("AVK connected to device %02X:%02X:%02X:%02X:%02X:%02X\n",
                  p_data->open.bd_addr[0], p_data->open.bd_addr[1], p_data->open.bd_addr[2],
                  p_data->open.bd_addr[3], p_data->open.bd_addr[4], p_data->open.bd_addr[5]);
        break;

    case BSA_AVK_CLOSE_EVT:
        /* Close event, reason BTA_AVK_CLOSE_STR_CLOSED or BTA_AVK_CLOSE_CONN_LOSS*/
        APP_DEBUG1("BSA_AVK_CLOSE_EVT status 0x%x, handle %d", p_data->close.status,
                   p_data->close.ccb_handle);

        pthread_mutex_lock(&mutex_callback);
        if (app_avk_cb.pcm_close_call && app_avk_cb.alsa_handle) {
            app_avk_cb.pcm_close_call(g_pcm_context, app_avk_cb.alsa_handle);
            app_avk_cb.alsa_handle = NULL;
        }
        pthread_mutex_unlock(&mutex_callback);
        connection = app_avk_find_connection_by_bd_addr(p_data->close.bd_addr);
        if (connection == NULL) {
            APP_DEBUG1("BSA_AVK_CLOSE_EVT unknown handle %d", p_data->open.ccb_handle);
            break;
        }

        connection->is_open = FALSE;
        app_avk_cb.open_pending = FALSE;
        if (connection->is_started == TRUE) {
            connection->is_started = FALSE;

            if (connection->format == BSA_AVK_CODEC_M24) {
                close(app_avk_cb.fd);
                app_avk_cb.fd = -1;
            } else
                app_avk_close_wave_file(connection);
        }
        app_avk_reset_connection(p_data->close.bd_addr);
#if (BSA_AVK_AAC_SUPPORTED == TRUE)
        pthread_mutex_lock(&mutex_aac);
        app_avk_aac_destroy_decoder();
        pthread_mutex_unlock(&mutex_aac);
#endif

        break;

    case BSA_AVK_START_EVT:

        APP_DEBUG1("BSA_AVK_START_EVT status 0x%x, streaming %d", p_data->start.status,
                   p_data->start.streaming);

        {
            if (p_data->start.streaming == FALSE) {
                connection = app_avk_find_connection_by_av_handle(p_data->start.ccb_handle);
                if (connection == NULL) {
                    return;
                }
                connection->is_streaming_open = TRUE;
                return;
            } else {
                /* We got START_EVT for a new device that is streaming but server discards the data
                    because another stream is already active */
                if (p_data->start.discarded) {
                    connection = app_avk_find_connection_by_bd_addr(p_data->start.bd_addr);
                    if (connection) {
                        // connection->is_started = TRUE;
                        connection->is_streaming_open = TRUE;
                    }
                    return;
                }

                pStreamingConn = NULL;

                connection = app_avk_find_connection_by_bd_addr(p_data->start.bd_addr);

                if (connection == NULL || connection->is_started) {
                    return;
                } else {
                    app_avk_pause_other_streams(p_data->start.bd_addr);
                    connection->is_started = TRUE;
                    app_avk_handle_start(p_data, connection);
                    connection->is_streaming_open = TRUE;
                    pStreamingConn = connection;
                }
            }
        }
        break;

    case BSA_AVK_STOP_EVT:
        //    APP_DEBUG1("BSA_AVK_STOP_EVT streaming: %d  Suspended: %d", p_data->stop.streaming,
        //               p_data->stop.suspended);
        APP_DEBUG1("BSA_AVK_STOP_EVT streaming: %d	Suspended: %d, status: %d, ccb_handle: %d, "
                   "bd_addr: %02X:%02X:%02X:%02X:%02X:%02X",
                   p_data->stop.streaming, p_data->stop.suspended, p_data->stop.status,
                   p_data->stop.ccb_handle, p_data->stop.bd_addr[0], p_data->stop.bd_addr[1],
                   p_data->stop.bd_addr[2], p_data->stop.bd_addr[3], p_data->stop.bd_addr[4],
                   p_data->stop.bd_addr[5]);

        if (p_data->stop.suspended) {
            connection = app_avk_find_connection_by_bd_addr(p_data->stop.bd_addr);
            if (connection == NULL) {
                break;
            }

            connection->is_streaming_open = FALSE;
            connection->is_started = FALSE;
            APP_DEBUG1("%s %d \r\n", __func__, __LINE__);
        } else if (p_data->stop.streaming == FALSE) {
            connection = app_avk_find_connection_by_av_handle(p_data->stop.ccb_handle);
            if (connection == NULL) {
                break;
            }

            connection->is_streaming_open = FALSE;
            connection->is_started = FALSE;
        } else {
            connection = app_avk_find_streaming_connection();

            if (connection == NULL) {
                break;
            }

            connection->is_started = FALSE;
            APP_DEBUG1("%s %d \r\n", __func__, __LINE__);

            if (connection->format == BSA_AVK_CODEC_M24) {
                close(app_avk_cb.fd);
                app_avk_cb.fd = -1;
            } else
                app_avk_close_wave_file(connection);
        }
        pthread_mutex_lock(&mutex_callback);
        if (app_avk_cb.pcm_close_call && app_avk_cb.alsa_handle) {
            app_avk_cb.pcm_close_call(g_pcm_context, app_avk_cb.alsa_handle);
            app_avk_cb.alsa_handle = NULL;
        }
        pthread_mutex_unlock(&mutex_callback);

        break;

    case BSA_AVK_RC_OPEN_EVT:

        if (p_data->rc_open.status == BSA_SUCCESS) {
            APP_DEBUG0("BSA_AVK_RC_OPEN_EVT ");

            connection = app_avk_add_connection(p_data->rc_open.bd_addr);
            if (connection == NULL) {
                APP_DEBUG0("BSA_AVK_RC_OPEN_EVT could not allocate connection");
                break;
            }
            APP_DEBUG1("BSA_AVK_RC_OPEN_EVT peer_feature=0x%x rc_handle=%d",
                       p_data->rc_open.peer_features, p_data->rc_open.rc_handle);
            app_avk_cb.rc_handle = p_data->rc_open.rc_handle;
            connection->rc_handle = p_data->rc_open.rc_handle;
            connection->peer_features = p_data->rc_open.peer_features;
            connection->peer_version = p_data->rc_open.peer_version;
            connection->is_rc_open = TRUE;
            app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
        }
        break;

    case BSA_AVK_RC_PEER_OPEN_EVT:
        APP_DEBUG0("BSA_AVK_RC_PEER_OPEN_EVT ");

        connection = app_avk_find_connection_by_rc_handle(p_data->rc_open.rc_handle);
        if (connection == NULL) {
            APP_DEBUG1("BSA_AVK_RC_PEER_OPEN_EVT could not find connection handle %d",
                       p_data->rc_open.rc_handle);
            break;
        }

        APP_DEBUG1("BSA_AVK_RC_PEER_OPEN_EVT peer_feature=0x%x rc_handle=%d",
                   p_data->rc_open.peer_features, p_data->rc_open.rc_handle);
        app_avk_cb.rc_handle = p_data->rc_open.rc_handle;
        connection->peer_features = p_data->rc_open.peer_features;
        connection->peer_version = p_data->rc_open.peer_version;
        connection->is_rc_open = TRUE;
        break;

    case BSA_AVK_RC_CLOSE_EVT:
        APP_DEBUG0("BSA_AVK_RC_CLOSE_EVT");
        connection = app_avk_find_connection_by_rc_handle(p_data->rc_close.rc_handle);
        if (connection == NULL) {
            break;
        }

        //      SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=BTSYNC_DISCONNECT",
        //      strlen("GNOTIFY=BTSYNC_DISCONNECT"), NULL,
        //                               NULL, 0);
        connection->is_rc_open = FALSE;
        break;

    case BSA_AVK_REMOTE_RSP_EVT:
        APP_DEBUG0("BSA_AVK_REMOTE_RSP_EVT");
        break;

    case BSA_AVK_VENDOR_CMD_EVT:
        APP_DEBUG0("BSA_AVK_VENDOR_CMD_EVT");
        APP_DEBUG1(" code:0x%x", p_data->vendor_cmd.code);
        APP_DEBUG1(" company_id:0x%x", p_data->vendor_cmd.company_id);
        APP_DEBUG1(" label:0x%x", p_data->vendor_cmd.label);
        APP_DEBUG1(" len:0x%x", p_data->vendor_cmd.len);
#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_data->vendor_cmd.data, p_data->vendor_cmd.len);
#endif
        break;

    case BSA_AVK_VENDOR_RSP_EVT:
        APP_DEBUG0("BSA_AVK_VENDOR_RSP_EVT");
        APP_DEBUG1(" code:0x%x", p_data->vendor_rsp.code);
        APP_DEBUG1(" company_id:0x%x", p_data->vendor_rsp.company_id);
        APP_DEBUG1(" label:0x%x", p_data->vendor_rsp.label);
        APP_DEBUG1(" len:0x%x", p_data->vendor_rsp.len);
#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_data->vendor_rsp.data, p_data->vendor_rsp.len);
#endif
        break;

    case BSA_AVK_CP_INFO_EVT:
        APP_DEBUG0("BSA_AVK_CP_INFO_EVT");
        if (p_data->cp_info.id == BSA_AVK_CP_SCMS_T_ID) {
            switch (p_data->cp_info.info.scmst_flag) {
            case BSA_AVK_CP_SCMS_COPY_NEVER:
                APP_INFO1(" content protection:0x%x - COPY NEVER", p_data->cp_info.info.scmst_flag);
                break;
            case BSA_AVK_CP_SCMS_COPY_ONCE:
                APP_INFO1(" content protection:0x%x - COPY ONCE", p_data->cp_info.info.scmst_flag);
                break;
            case BSA_AVK_CP_SCMS_COPY_FREE:
            case (BSA_AVK_CP_SCMS_COPY_FREE + 1):
                APP_INFO1(" content protection:0x%x - COPY FREE", p_data->cp_info.info.scmst_flag);
                break;
            default:
                APP_ERROR1(" content protection:0x%x - UNKNOWN VALUE",
                           p_data->cp_info.info.scmst_flag);
                break;
            }
        } else {
            APP_INFO0(" no content protection");
        }
        break;

    case BSA_AVK_REGISTER_NOTIFICATION_EVT:
        //  APP_INFO1("BSA_AVK_REGISTER_NOTIFICATION_EVT pdu %d. status=%d, opcode=%d, eid=%d ctype
        //  = %d",
        //            p_data->reg_notif.rsp.pdu,
        //            p_data->reg_notif.rsp.status,
        //            p_data->reg_notif.rsp.opcode,
        //            p_data->reg_notif.rsp.event_id,
        //            p_data->reg_notif.rsp.ctype
        //           );
        if (p_data->reg_notif.rsp.ctype == AVRC_RSP_INTERIM) {
            // skip AVRC RSP INTERIM
            return;
        }
        if (p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_STATUS_CHANGE) {
            //  APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_PLAY_STATUS_CHANGE
            //  play_status=%d, handle %d",
            //           (int)p_data->reg_notif.rsp.param.play_status, p_data->reg_notif.handle);
            // if (p_data->reg_notif.rsp.opcode == AVRC_RSP_CHANGED) {
            //	APP_INFO1("+++ Response CHANGED, playstatus: %d",
            // p_data->reg_notif.rsp.param.play_status);
            //} else if (p_data->reg_notif.rsp.opcode == AVRC_RSP_INTERIM) {
            //	APP_DEBUG1("++++ Response INTERIM, playstatus: %d",
            // p_data->reg_notif.rsp.param.play_status);
            //	break;
            //}

            switch (p_data->reg_notif.rsp.param.play_status) {
            case AVRC_PLAYSTATE_STOPPED:
                APP_INFO0("AVRC_Stopped!");
                // if(app_avk_cb.play_state != AVRC_PLAYSTATE_STOPPED) {
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                //     app_avk_cb.play_state = AVRC_PLAYSTATE_STOPPED;
                // }
                break;
            case AVRC_PLAYSTATE_PLAYING:
                APP_INFO0("AVRC_Playing!");
                // if(app_avk_cb.play_state != AVRC_PLAYSTATE_PLAYING) {
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                //     app_avk_cb.play_state = AVRC_PLAYSTATE_PLAYING;
                // }
                break;
            case AVRC_PLAYSTATE_PAUSED:
                APP_INFO0("AVRC_Paused!");
                //  if(app_avk_cb.play_state != AVRC_PLAYSTATE_PAUSED) {
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                //     app_avk_cb.play_state = AVRC_PLAYSTATE_PAUSED;
                //  }
                break;
            case AVRC_PLAYSTATE_FWD_SEEK:
                APP_INFO0("AVRC_Fwd_seek!");
                break;
            case AVRC_PLAYSTATE_REV_SEEK:
                APP_INFO0("AVRC_Rev_seek!");
                break;
            case AVRC_PLAYSTATE_ERROR:
                APP_INFO0("avrcp play_error!");
                break;
            }
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_CHANGE) {
            //    APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_REACHED_START) {
            //     APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_REACHED_START");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_REACHED_END) {
            //      APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_REACHED_END");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_POS_CHANGED) {
            //       APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_PLAY_POS_CHANGED
            //       pos=%d",
            //                  (int)p_data->reg_notif.rsp.param.play_pos);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_BATTERY_STATUS_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_BATTERY_STATUS_CHANGE
            //        bat=%d",
            //                  (int)p_data->reg_notif.rsp.param.battery_status);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_SYSTEM_STATUS_CHANGE) {
            //          APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT
            //          AVRC_EVT_SYSTEM_STATUS_CHANGE  sys=%d",
            //                  (int)p_data->reg_notif.rsp.param.system_status);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_APP_SETTING_CHANGE) {
            //        APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_APP_SETTING_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_NOW_PLAYING_CHANGE) {
            //        APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_NOW_PLAYING_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_AVAL_PLAYERS_CHANGE) {
            //       APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_AVAL_PLAYERS_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_ADDR_PLAYER_CHANGE) {
            //         APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT
            //         AVRC_EVT_ADDR_PLAYER_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_UIDS_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_UIDS_CHANGE
            //        uid_counter=%d",
            //                  (int)p_data->reg_notif.rsp.param.uid_counter);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_VOLUME_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_VOLUME_CHANGE
            //        vol=%d",
            //                   (int)p_data->reg_notif.rsp.param.volume);
        }

        if (AVRC_STS_NO_ERROR == p_data->reg_notif.rsp.status &&
            p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_STATUS_CHANGE) {
            connection = app_avk_find_connection_by_rc_handle(p_data->reg_notif.handle);
            if (connection != NULL) {
                connection->playstate = p_data->reg_notif.rsp.param.play_status;
            }
        }
        break;

    case BSA_AVK_LIST_PLAYER_APP_ATTR_EVT:
        APP_INFO0("BSA_AVK_LIST_PLAYER_APP_ATTR_EVT");
        break;
    case BSA_AVK_LIST_PLAYER_APP_VALUES_EVT:
        APP_INFO0("BSA_AVK_LIST_PLAYER_APP_VALUES_EVT");
        break;
    case BSA_AVK_SET_PLAYER_APP_VALUE_EVT:
        APP_INFO0("BSA_AVK_SET_PLAYER_APP_VALUE_EVT");
        break;
    case BSA_AVK_GET_PLAYER_APP_VALUE_EVT:
        APP_INFO0("BSA_AVK_GET_PLAYER_APP_VALUE_EVT");
        break;
    case BSA_AVK_GET_ELEM_ATTR_EVT:
        APP_INFO0("BSA_AVK_GET_ELEM_ATTR_EVT");
        break;
    case BSA_AVK_GET_PLAY_STATUS_EVT:
        APP_INFO0("BSA_AVK_GET_PLAY_STATUS_EVT");
        break;
    case BSA_AVK_SET_ADDRESSED_PLAYER_EVT:
        APP_INFO0("BSA_AVK_SET_ADDRESSED_PLAYER_EVT");
        break;
    case BSA_AVK_SET_BROWSED_PLAYER_EVT:
        APP_INFO0("BSA_AVK_SET_BROWSED_PLAYER_EVT");
        break;
    case BSA_AVK_GET_FOLDER_ITEMS_EVT:
        APP_INFO0("BSA_AVK_GET_FOLDER_ITEMS_EVT");
        break;
    case BSA_AVK_CHANGE_PATH_EVT:
        APP_INFO0("BSA_AVK_CHANGE_PATH_EVT");
        break;
    case BSA_AVK_GET_ITEM_ATTR_EVT:
        APP_INFO0("BSA_AVK_GET_ITEM_ATTR_EVT");
        break;
    case BSA_AVK_PLAY_ITEM_EVT:
        APP_INFO0("BSA_AVK_PLAY_ITEM_EVT");
        break;
    case BSA_AVK_ADD_TO_NOW_PLAYING_EVT:
        APP_INFO0("BSA_AVK_ADD_TO_NOW_PLAYING_EVT");
        break;

    case BSA_AVK_SET_ABS_VOL_CMD_EVT: {
        connection = app_avk_find_connection_by_rc_handle(p_data->abs_volume.handle);
        if ((connection != NULL) && connection->m_bAbsVolumeSupported) {
            /* Peer requested change in volume. Make the change and send response with new system
             * volume. BSA is TG role in AVK */

            if (p_data->abs_volume.abs_volume_cmd.volume < BSA_MAX_ABS_VOLUME) {
                app_avk_cb.volume =
                    p_data->abs_volume.abs_volume_cmd.volume * 100 / BSA_MAX_ABS_VOLUME;
                //  app_avk_set_abs_vol_rsp(p_data->abs_volume.abs_volume_cmd.volume,
                //  connection->rc_handle, p_data->abs_volume.label);
            } else
                app_avk_cb.volume = 100;
            app_avk_set_abs_vol_rsp(p_data->abs_volume.abs_volume_cmd.volume, connection->rc_handle,
                                    p_data->abs_volume.label);
            APP_DEBUG1("BSA_AVK_SET_ABS_VOL_CMD_EVT %d Transaction Label %d connection "
                       "volchangelabel %d \r\n",
                       p_data->abs_volume.abs_volume_cmd.volume, p_data->abs_volume.label,
                       connection->volChangeLabel);

        } else {
            APP_ERROR1("not changing volume connection 0x%x m_bAbsVolumeSupported %d", connection,
                       connection->m_bAbsVolumeSupported);
        }
    } break;

    case BSA_AVK_REG_NOTIFICATION_CMD_EVT: {
        APP_DEBUG0("BSA_AVK_REG_NOTIFICATION_CMD_EVT ");
        if (p_data->reg_notif_cmd.reg_notif_cmd.event_id == AVRC_EVT_VOLUME_CHANGE) {
            connection = app_avk_find_connection_by_rc_handle(p_data->reg_notif_cmd.handle);
            if (connection != NULL) {
                connection->m_bAbsVolumeSupported = TRUE;
                connection->volChangeLabel = p_data->reg_notif_cmd.label;
                /* Peer requested registration for vol change event. Send response with current
                 * system volume. BSA is TG role in AVK */
                app_avk_reg_notfn_rsp(
                    app_avk_cb.volume, p_data->reg_notif_cmd.handle, p_data->reg_notif_cmd.label,
                    p_data->reg_notif_cmd.reg_notif_cmd.event_id, BTA_AV_RSP_INTERIM);
            }
        }
    } break;

    default:
        APP_ERROR1("Unsupported event %d", event);
        break;
    }

    /* forward the callback to registered applications */
    if (s_pCallback)
        s_pCallback(event, p_data);
}

/*******************************************************************************
 **
 ** Function         app_avk_open
 **
 ** Description      Example of function to open AVK connection
 **
 ** Returns          void
 **
 *******************************************************************************/
int app_avk_open_by_addr(BD_ADDR *bd_addr_in, int timeoutms)
{
    tBSA_STATUS status;
    int choice;
    BOOLEAN connect = FALSE;
    BD_ADDR bd_addr;
    UINT8 *p_name;
    tBSA_AVK_OPEN open_param;
    tAPP_AVK_CONNECTION *connection = NULL;
    int try_count = timeoutms / 20;
    int last_active = 0;

    if (app_avk_cb.open_pending) {
        APP_ERROR0("already trying to connect");
        return ALREADY_CONNECTING;
    }

    bdcpy(bd_addr, *bd_addr_in);
    APP_INFO1("Connecting bdaddr %02x:%02x:%02x:%02x:%02x:%02x\n", bd_addr[0], bd_addr[1],
              bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);

    xml_db_lock();
    if (app_read_xml_remote_devices() == -1) {
        xml_db_unlock();
        return BT_DEVICES_NO_HISTORY;
    }

    for (; choice < APP_NUM_ELEMENTS(app_xml_remote_devices_db); choice++) {
        if ((app_xml_remote_devices_db[choice].in_use == TRUE) &&
            (!bdcmp(app_xml_remote_devices_db[choice].bd_addr, bd_addr))) {
            p_name = app_xml_remote_devices_db[choice].name;
            connect = TRUE;
            break;
        }
    }
    xml_db_unlock();

    if (connect != FALSE) {
        /* Open AVK stream */
        APP_INFO1("Connecting to AV device:%s \n", p_name);

        app_avk_cb.open_pending = TRUE;

        BSA_AvkOpenInit(&open_param);
        memcpy((char *)(open_param.bd_addr), bd_addr, sizeof(BD_ADDR));

        open_param.sec_mask = BSA_SEC_NONE;
        status = BSA_AvkOpen(&open_param);
        if (status != BSA_SUCCESS) {
            APP_ERROR1("Unable to connect to device %02X:%02X:%02X:%02X:%02X:%02X with status %d",
                       open_param.bd_addr[0], open_param.bd_addr[1], open_param.bd_addr[2],
                       open_param.bd_addr[3], open_param.bd_addr[4], open_param.bd_addr[5], status);

            app_avk_cb.open_pending = FALSE;
        } else {
            /* this is an active wait for demo purpose */
            APP_INFO0("waiting for AV connection to open\n");

            while (avk_is_open_pending() == TRUE && try_count--) {
                usleep(20000);
            }

            connection = app_avk_find_connection_by_bd_addr(open_param.bd_addr);
            if (connection == NULL || connection->is_open == FALSE) {
                APP_ERROR0("failure opening AV connection  \n");
                return BT_CONNECTED_FAILED;
            } else {
                /* XML Database update should be done upon reception of AV OPEN event */
                APP_DEBUG1("Connected to AV device:%s", p_name);
                /* Read the Remote device xml file to have a fresh view */
                xml_db_lock();
                app_read_xml_remote_devices();

                /* Add AV service for this devices in XML database */
                app_xml_add_trusted_services_db(
                    app_xml_remote_devices_db, APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                    BSA_A2DP_SERVICE_MASK | BSA_AVRCP_SERVICE_MASK, PROFILE_TYPE_A2DP_SOURCE);

                app_xml_update_name_db(app_xml_remote_devices_db,
                                       APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                                       p_name);

                /* Update database => write to disk */
                if (app_write_xml_remote_devices() < 0) {
                    APP_ERROR0("Failed to store remote devices database");
                }
                xml_db_unlock();

                return BT_CONNECTED_SUCCESSED;
            }
        }
    }

    return BT_HISTORY_NOT_FOUND;
}

/*******************************************************************************
 **
 ** Function         app_avk_open
 **
 ** Description      Example of function to open AVK connection
 **
 ** Returns          void
 **
 *******************************************************************************/
int app_avk_open(int input)
{
    tBSA_STATUS status;
    int choice;
    BOOLEAN connect = FALSE;
    BD_ADDR bd_addr;
    UINT8 *p_name;
    tBSA_AVK_OPEN open_param;
    tAPP_AVK_CONNECTION *connection = NULL;
    int try_count = 250;
    int last_active = 0;

    if (app_avk_cb.open_pending) {
        APP_ERROR0("already trying to connect");
        return -1;
    }
    choice = input;

    if (choice == -1) {
        last_active = 1;
        choice = 0;
    }

    /* Read the XML file which contains all the bonded devices */
    xml_db_lock();
    if (app_read_xml_remote_devices() == -1) {
        xml_db_unlock();
        return -2;
    }

    for (; choice < APP_NUM_ELEMENTS(app_xml_remote_devices_db); choice++) {
        if (app_xml_remote_devices_db[choice].in_use != FALSE) {
            if (last_active && app_xml_remote_devices_db[choice].last_active) {
                bdcpy(bd_addr, app_xml_remote_devices_db[choice].bd_addr);
                p_name = app_xml_remote_devices_db[choice].name;
                connect = TRUE;
                break;
            } else if (!last_active) {
                bdcpy(bd_addr, app_xml_remote_devices_db[choice].bd_addr);
                p_name = app_xml_remote_devices_db[choice].name;
                connect = TRUE;
                break;
            }
        } else {
            // APP_ERROR0("Device entry not in use");
        }
    }

    // else
    //{
    //    APP_ERROR0("Unsupported device number");
    //}

    xml_db_unlock();

    if (connect != FALSE) {
        /* Open AVK stream */
        printf("Connecting to AV device:%s \n", p_name);

        app_avk_cb.open_pending = TRUE;

        BSA_AvkOpenInit(&open_param);
        memcpy((char *)(open_param.bd_addr), bd_addr, sizeof(BD_ADDR));

        open_param.sec_mask = BSA_SEC_NONE;
        status = BSA_AvkOpen(&open_param);
        if (status != BSA_SUCCESS) {
            APP_ERROR1("Unable to connect to device %02X:%02X:%02X:%02X:%02X:%02X with status %d",
                       open_param.bd_addr[0], open_param.bd_addr[1], open_param.bd_addr[2],
                       open_param.bd_addr[3], open_param.bd_addr[4], open_param.bd_addr[5], status);

            app_avk_cb.open_pending = FALSE;
        } else {
            /* this is an active wait for demo purpose */
            APP_INFO0("waiting for AV connection to open\n");

            while (avk_is_open_pending() == TRUE && try_count--) {
                usleep(20000);
            }

            connection = app_avk_find_connection_by_bd_addr(open_param.bd_addr);
            if (connection == NULL || connection->is_open == FALSE) {
                APP_ERROR0("failure opening AV connection  \n");
                return -1;
            } else {
                /* XML Database update should be done upon reception of AV OPEN event */
                APP_DEBUG1("Connected to AV device:%s", p_name);
                /* Read the Remote device xml file to have a fresh view */
                xml_db_lock();
                app_read_xml_remote_devices();

                /* Add AV service for this devices in XML database */
                app_xml_add_trusted_services_db(
                    app_xml_remote_devices_db, APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                    BSA_A2DP_SERVICE_MASK | BSA_AVRCP_SERVICE_MASK, PROFILE_TYPE_A2DP_SOURCE);

                app_xml_update_name_db(app_xml_remote_devices_db,
                                       APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                                       p_name);

                /* Update database => write to disk */
                if (app_write_xml_remote_devices() < 0) {
                    APP_ERROR0("Failed to store remote devices database");
                }
                xml_db_unlock();

                return (input == -1) ? 0 : input;
            }
        }
    }

    return -3;
}

/*******************************************************************************
 **
 ** Function         app_avk_close
 **
 ** Description      Function to close AVK connection
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_close(BD_ADDR bda)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE bsa_avk_close_param;
    tAPP_AVK_CONNECTION *connection = NULL;

    /* Close AVK connection */
    BSA_AvkCloseInit(&bsa_avk_close_param);

    connection = app_avk_find_connection_by_bd_addr(bda);

    if (connection == NULL) {
        APP_ERROR0("Unable to Close AVK connection , invalid BDA");
        return;
    }

    bsa_avk_close_param.ccb_handle = connection->ccb_handle;
    bsa_avk_close_param.rc_handle = connection->rc_handle;

    status = BSA_AvkClose(&bsa_avk_close_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Close AVK connection with status %d", status);
    } else {
        app_avk_reset_connection(bda);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_stop_all
 **
 ** Description      Function to stop all AVK connections
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_stop_all()
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE) {
            app_avk_play_stop(app_avk_cb.connections[index].rc_handle);
        }
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_close_all
 **
 ** Description      Function to close all AVK connections
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_close_all()
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE) {
            app_avk_close(app_avk_cb.connections[index].bda_connected);
        }
    }
}

/*******************************************************************************
**
** Function         app_avk_open_rc
**
** Description      Function to opens avrc controller connection. AVK should be open before opening
*rc.
**
** Returns          void
**
*******************************************************************************/
void app_avk_open_rc(BD_ADDR bd_addr)
{
    tBSA_STATUS status;
    tBSA_AVK_OPEN bsa_avk_open_param;

    /* open avrc connection */
    BSA_AvkOpenRcInit(&bsa_avk_open_param);
    bdcpy(bsa_avk_open_param.bd_addr, bd_addr);
    status = BSA_AvkOpenRc(&bsa_avk_open_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to open arvc connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function         app_avk_close_rc
**
** Description      Function to closes avrc controller connection.
**
** Returns          void
**
*******************************************************************************/
void app_avk_close_rc(UINT8 rc_handle)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE bsa_avk_close_param;

    /* close avrc connection */
    BSA_AvkCloseRcInit(&bsa_avk_close_param);

    bsa_avk_close_param.rc_handle = rc_handle;

    status = BSA_AvkCloseRc(&bsa_avk_close_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to close arvc connection with status %d", status);
    }
}

/*******************************************************************************
 **
 ** Function        app_avk_start
 **
 ** Description     Example of function to start an av stream
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_avk_start(void)
{
    tBSA_STATUS status;
    tBSA_AVK_START req;

    /* Close AVK connection */
    BSA_AvkStartInit(&req);
    status = BSA_AvkStart(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to start AVK connection with status %d", status);
    }
}

/*******************************************************************************
 **
 ** Function        app_avk_stop
 **
 ** Description     Example of function to stop an av stream
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_avk_stop(BOOLEAN suspend)
{
    tBSA_STATUS status;
    tBSA_AVK_STOP req;

    /* Close AVK connection */
    BSA_AvkStopInit(&req);
    req.pause = suspend; /* suspend or local stop*/

    status = BSA_AvkStop(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to stop AVK connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function         app_avk_close_str
**
** Description      Function to close an A2DP Steam connection
**
** Returns          void
**
*******************************************************************************/
void app_avk_close_str(UINT8 ccb_handle)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE_STR bsa_avk_close_str_param;

    /* close avrc connection */
    BSA_AvkCloseStrInit(&bsa_avk_close_str_param);

    bsa_avk_close_str_param.ccb_handle = ccb_handle;

    status = BSA_AvkCloseStr(&bsa_avk_close_str_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to close arvc connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function        app_avk_register
**
** Description     Example of function to register an avk endpoint
**
** Returns         void
**
*******************************************************************************/
int app_avk_register(int b_support_aac_decode)
{
    tBSA_STATUS status;
    tBSA_AVK_REGISTER bsa_avk_register_param;

    /* register an audio AVK end point */
    BSA_AvkRegisterInit(&bsa_avk_register_param);

    bsa_avk_register_param.media_sup_format.audio.pcm_supported = TRUE;
    bsa_avk_register_param.media_sup_format.audio.sec_supported = TRUE;
    bsa_avk_register_param.media_sup_format.audio.aac_supported = FALSE;
#ifdef BSA_AVK_AAC_SUPPORTED
    /* Enable AAC support in the app */
    bsa_avk_register_param.media_sup_format.audio.aac_supported = b_support_aac_decode;
#endif
    //#endif
    strncpy(bsa_avk_register_param.service_name, "BSA Audio Service", BSA_AVK_SERVICE_NAME_LEN_MAX);

    bsa_avk_register_param.reg_notifications = reg_notifications;

    status = BSA_AvkRegister(&bsa_avk_register_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to register an AV sink with status %d", status);
        return -1;
    }
    /* Save UIPC channel */
    app_avk_cb.uipc_audio_channel = bsa_avk_register_param.uipc_channel;

    /* open the UIPC channel to receive the pcm */
    if (UIPC_Open(bsa_avk_register_param.uipc_channel, app_avk_uipc_cback) == FALSE) {
        APP_ERROR0("Unable to open UIPC channel");
        return -1;
    }

    app_avk_cb.fd = -1;

    return 0;
}

/*******************************************************************************
 **
 ** Function        app_avk_deregister
 **
 ** Description     Example of function to deregister an avk endpoint
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_avk_deregister(void)
{
    tBSA_STATUS status;
    tBSA_AVK_DEREGISTER req;

    /* register an audio AVK end point */
    BSA_AvkDeregisterInit(&req);
    status = BSA_AvkDeregister(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to deregister an AV sink with status %d", status);
    }

#ifdef TODO
    if (app_avk_cb.format == BSA_AVK_CODEC_M24) {
        close(app_avk_cb.fd);
        app_avk_cb.fd = -1;
    } else
        app_avk_close_wave_file();
#endif

    /* close the UIPC channel receiving the pcm */
    UIPC_Close(app_avk_cb.uipc_audio_channel);
    app_avk_cb.uipc_audio_channel = UIPC_CH_ID_BAD;
}

/*******************************************************************************
**
** Function        app_avk_uipc_cback
**
** Description     uipc audio call back function.
**
** Parameters      pointer on a buffer containing PCM sample with a BT_HDR header
**
** Returns          void
**
*******************************************************************************/
static void app_avk_uipc_cback(BT_HDR *p_msg)
{
#ifdef PCM_ALSA
    snd_pcm_sframes_t alsa_frames;
    snd_pcm_sframes_t alsa_frames_to_send;
#endif
    UINT8 *p_buffer;
    int dummy;
    tAPP_AVK_CONNECTION *connection = NULL;
    int avail;
    if (p_msg == NULL) {
        APP_ERROR0("app_avk_uipc_cback p_msg is NULL ?!");
        return;
    }
    // printf("%s len %d \r\n",__FUNCTION__,p_msg->len);

    p_buffer = ((UINT8 *)(p_msg + 1)) + p_msg->offset;

    connection = app_avk_find_streaming_connection();

    if (!connection) {
        APP_ERROR0("app_avk_uipc_cback unable to find connection!!!!!!");
        GKI_freebuf(p_msg);
        return;
    }

    if (connection->format == BSA_AVK_CODEC_M24) {
/* Invoke AAC decoder here for the current buffer.
    decode the AAC file that is created */
#ifdef BSA_AVK_AAC_SUPPORTED
        pthread_mutex_lock(&mutex_aac);

        if (!a2dp_aac_decoder_cb.has_aac_handle) {
            pthread_mutex_unlock(&mutex_aac);
            GKI_freebuf(p_msg);
            return;
        }

        UINT bufferSize = p_msg->len;
        UINT bytesValid = p_msg->len;

        while (bytesValid > 0) {
            AAC_DECODER_ERROR err = aacDecoder_Fill(a2dp_aac_decoder_cb.aac_handle, &p_buffer,
                                                    &bufferSize, &bytesValid);
            if (err != AAC_DEC_OK) {
                APP_ERROR1("%s: aacDecoder_Fill failed: %x bytesValid %d ", __func__, err,
                           bytesValid);
                break;
            }

            while (1) {
                err = aacDecoder_DecodeFrame(a2dp_aac_decoder_cb.aac_handle,
                                             a2dp_aac_decoder_cb.decode_buf, DECODE_BUF_LEN,
                                             0 /* flags */);
                if (err == AAC_DEC_NOT_ENOUGH_BITS) {
                    break;
                }
                if (err != AAC_DEC_OK) {
                    APP_ERROR1("%s: aacDecoder_DecodeFrame failed: %x", __func__, err);
                    break;
                }

                CStreamInfo *info = aacDecoder_GetStreamInfo(a2dp_aac_decoder_cb.aac_handle);
                if (!info || info->sampleRate <= 0) {
                    APP_ERROR1("%s: Invalid stream info", __func__);
                    break;
                }

                size_t frame_len =
                    info->frameSize * info->numChannels * sizeof(a2dp_aac_decoder_cb.decode_buf[0]);
#ifdef PCM_ALSA
                pthread_mutex_lock(&mutex_callback);
                if (app_avk_cb.alsa_handle != NULL && frame_len) {
                    /* Compute number of PCM samples (contained in p_msg->len bytes) */
                    if (app_avk_cb.pcm_write_data_call)
                        app_avk_cb.pcm_write_data_call(g_pcm_context, app_avk_cb.alsa_handle,
                                                       connection->index,
                                                       a2dp_aac_decoder_cb.decode_buf, frame_len);
                }
                pthread_mutex_unlock(&mutex_callback);
#endif
            }
        }

        pthread_mutex_unlock(&mutex_aac);
#endif
        GKI_freebuf(p_msg);
        return;
    }

    if (connection->format == BSA_AVK_CODEC_PCM && app_avk_cb.fd_codec_type != BSA_AVK_CODEC_M24) {
        /* Invoke AAC decoder here for the current buffer.
            decode the AAC file that is created */
        if (app_avk_cb.fd != -1) {
            APP_DEBUG1("app_avk_uipc_cback Writing Data 0x%x, len = %d", p_buffer, p_msg->len);
            dummy = write(app_avk_cb.fd, p_buffer, p_msg->len);
            (void)dummy;
        }

#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_buffer, p_msg->len);
#endif
    }
    if (connection->num_channel == 0) {
        APP_ERROR0("connection->num_channel == 0");
        GKI_freebuf(p_msg);
        return;
    }
#ifdef PCM_ALSA
    pthread_mutex_lock(&mutex_callback);
    if (app_avk_cb.alsa_handle != NULL && p_buffer && p_msg->len) {
        /* Compute number of PCM samples (contained in p_msg->len bytes) */
        alsa_frames_to_send = p_msg->len;
        if (app_avk_cb.pcm_write_data_call)
            app_avk_cb.pcm_write_data_call(g_pcm_context, app_avk_cb.alsa_handle, connection->index,
                                           p_buffer, alsa_frames_to_send);
    }
    pthread_mutex_unlock(&mutex_callback);
#endif
    GKI_freebuf(p_msg);
}

/*******************************************************************************
**
** Function         app_avk_init
**
** Description      Init Manager application
**
** Parameters       Application callback (if null, default will be used)
**
** Returns          0 if successful, error code otherwise
**
*******************************************************************************/
int app_avk_init(tAvkCallback pcb /* = NULL */)
{
    tBSA_AVK_ENABLE bsa_avk_enable_param;
    tBSA_STATUS status;

    /* register callback */
    s_pCallback = pcb;

    /* Initialize the control structure */
    memset(&app_avk_cb, 0, sizeof(app_avk_cb));
    app_avk_cb.uipc_audio_channel = UIPC_CH_ID_BAD;

    app_avk_cb.fd = -1;
    app_avk_cb.open_pending = FALSE;

    app_avk_cb.pcm_open_call = g_pcm_open_call;
    app_avk_cb.pcm_close_call = g_pcm_close_call;
    app_avk_cb.pcm_write_data_call = g_pcm_write_data_call;

    /* set sytem vol at 50% */
    app_avk_cb.volume = (UINT8)((BSA_MAX_ABS_VOLUME - BSA_MIN_ABS_VOLUME) >> 1);

    /* get hold on the AVK resource, synchronous mode */
    BSA_AvkEnableInit(&bsa_avk_enable_param);

    bsa_avk_enable_param.sec_mask = BSA_AVK_SECURITY;
    bsa_avk_enable_param.features = BSA_AVK_FEATURES;
    bsa_avk_enable_param.p_cback = app_avk_cback;

    status = BSA_AvkEnable(&bsa_avk_enable_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to enable an AV sink with status %d", status);

        if (BSA_ERROR_SRV_AVK_AV_REGISTERED == status) {
            APP_ERROR0("AV was registered before AVK, AVK should be registered before AV");
        }

        return -1;
    }

    return BSA_SUCCESS;
}

/*******************************************************************************
**
** Function         app_avk_rc_send_command
**
** Description      Example of Send a RemoteControl command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_send_command(UINT8 key_state, UINT8 command, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_REM_CMD bsa_avk_rc_cmd;

    /* Send remote control command */
    status = BSA_AvkRemoteCmdInit(&bsa_avk_rc_cmd);
    bsa_avk_rc_cmd.rc_handle = rc_handle;
    bsa_avk_rc_cmd.key_state = (tBSA_AVK_STATE)key_state;
    bsa_avk_rc_cmd.rc_id = (tBSA_AVK_RC)command;
    bsa_avk_rc_cmd.label = app_avk_get_label();

    status = BSA_AvkRemoteCmd(&bsa_avk_rc_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV RC Command %d", status);
    }
}

/*******************************************************************************
**
** Function         app_avk_rc_send_click
**
** Description      Send press and release states of a command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_send_click(UINT8 command, UINT8 rc_handle)
{
    app_avk_rc_send_command(BSA_AV_STATE_PRESS, command, rc_handle);
    GKI_delay(300);
    app_avk_rc_send_command(BSA_AV_STATE_RELEASE, command, rc_handle);
}

/*******************************************************************************
**
** Function         app_avk_play_start
**
** Description      Example of start steam play
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_start(UINT8 rc_handle)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_rc_handle(rc_handle);
    if (connection == NULL) {
        APP_DEBUG1("app_avk_play_start could not find connection handle %d", rc_handle);
        return;
    }

    if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
        app_avk_rc_send_click(BSA_AVK_RC_PLAY, rc_handle);
    } else {
        //  APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
        //             (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
        app_avk_start();
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_play_stop
 **
 ** Description      Example of stop steam play
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_play_stop(UINT8 rc_handle)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_rc_handle(rc_handle);
    if (connection == NULL) {
        APP_DEBUG1("app_avk_play_stop could not find connection handle %d", rc_handle);
        return;
    }

    if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
        app_avk_rc_send_click(BSA_AVK_RC_STOP, rc_handle);
    } else {
        //  APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
        //             (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
        app_avk_stop(TRUE);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_play_pause
 **
 ** Description      Example of pause steam pause
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_play_pause(UINT8 rc_handle)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_rc_handle(rc_handle);
    if (connection == NULL) {
        APP_DEBUG1("app_avk_play_pause could not find connection handle %d", rc_handle);
        return;
    }

    if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
        app_avk_rc_send_click(BSA_AVK_RC_PAUSE, rc_handle);
    } else {
        // APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
        //            (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
        app_avk_stop(TRUE);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_play_next_track
 **
 ** Description      Example of play next track
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_play_next_track(UINT8 rc_handle)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_rc_handle(rc_handle);
    if (connection == NULL) {
        APP_DEBUG1("app_avk_play_pause could not find connection handle %d", rc_handle);
        return;
    }

    if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
        app_avk_rc_send_click(BSA_AVK_RC_FORWARD, rc_handle);
    } else {
        APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
                   (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_play_previous_track
 **
 ** Description      Example of play previous track
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_play_previous_track(UINT8 rc_handle)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_rc_handle(rc_handle);
    if (connection == NULL) {
        APP_DEBUG1("app_avk_play_pause could not find connection handle %d", rc_handle);
        return;
    }

    if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
        app_avk_rc_send_click(BSA_AVK_RC_BACKWARD, rc_handle);
    } else {
        APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
                   (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_cmd
 **
 ** Description      Example of function to open AVK connection
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_cmd(UINT8 rc_handle)
{
    int choice;

    do {
        printf("Bluetooth AVK AVRC CMD menu:\n");
        printf("    0 play\n");
        printf("    1 stop\n");
        printf("    2 pause\n");
        printf("    3 forward\n");
        printf("    4 backward\n");
        printf("    5 rewind key_press\n");
        printf("    6 rewind key_release\n");
        printf("    7 fast forward key_press\n");
        printf("    8 fast forward key_release\n");

        printf("    99 exit\n");

        choice = app_get_choice("Select source");

        switch (choice) {
        case 0:
            app_avk_rc_send_click(BSA_AVK_RC_PLAY, rc_handle);
            break;

        case 1:
            app_avk_rc_send_click(BSA_AVK_RC_STOP, rc_handle);
            break;

        case 2:
            app_avk_rc_send_click(BSA_AVK_RC_PAUSE, rc_handle);
            break;

        case 3:
            app_avk_rc_send_click(BSA_AVK_RC_FORWARD, rc_handle);
            break;

        case 4:
            app_avk_rc_send_click(BSA_AVK_RC_BACKWARD, rc_handle);
            break;

        case 5:
            app_avk_rc_send_command(BSA_AV_STATE_PRESS, BSA_AVK_RC_REWIND, rc_handle);
            break;

        case 6:
            app_avk_rc_send_command(BSA_AV_STATE_RELEASE, BSA_AVK_RC_REWIND, rc_handle);
            break;

        case 7:
            app_avk_rc_send_command(BSA_AV_STATE_PRESS, BSA_AVK_RC_FAST_FOR, rc_handle);
            break;

        case 8:
            app_avk_rc_send_command(BSA_AV_STATE_RELEASE, BSA_AVK_RC_FAST_FOR, rc_handle);
            break;

        default:
            APP_ERROR0("Unsupported choice");
            break;
        }
    } while (choice != 99);
}

/*******************************************************************************
 **
 ** Function         app_avk_send_get_element_attributes_cmd
 **
 ** Description      Example of function to send Vendor Dependent Command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_send_get_element_attributes_cmd(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_STATUS;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_GET_ELEMENT_ATTR;
    bsa_avk_vendor_cmd.data[1] = 0;    /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;    /* length high*/
    bsa_avk_vendor_cmd.data[3] = 0x11; /* length low*/

    /* data 4 ~ 11 are 0, which means "identifier 0x0 PLAYING" */

    bsa_avk_vendor_cmd.data[12] = 2; /* num of attributes */

    /* data 13 ~ 16 are 0x1, which means "attribute ID 1 : Title of media" */
    bsa_avk_vendor_cmd.data[16] = 0x1;

    /* data 17 ~ 20 are 0x7, which means "attribute ID 2 : Playing Time" */
    bsa_avk_vendor_cmd.data[20] = 0x7;

    bsa_avk_vendor_cmd.length = 21;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
**
** Function         avk_is_open_pending
**
** Description      Check if AVK Open is pending
**
** Parameters
**
** Returns          TRUE if open is pending, FALSE otherwise
**
*******************************************************************************/
BOOLEAN avk_is_open_pending() { return app_avk_cb.open_pending; }

/*******************************************************************************
**
** Function         avk_set_open_pending
**
** Description      Set AVK open pending
**
** Parameters
**
** Returns          void
**
*******************************************************************************/
void avk_set_open_pending(BOOLEAN bopenpend) { app_avk_cb.open_pending = bopenpend; }

/*******************************************************************************
**
** Function         avk_is_open
**
** Description      Check if AVK is open
**
** Parameters
**
** Returns          TRUE if AVK is open, FALSE otherwise. Return BDA of the connected device
**
*******************************************************************************/
BOOLEAN avk_is_open(BD_ADDR bda)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((app_avk_cb.connections[index].in_use == TRUE) &&
            (app_avk_cb.connections[index].is_open == TRUE)) {
            bdcpy(bda, app_avk_cb.connections[index].bda_connected);
            return TRUE;
        }
    }

    return FALSE;
}

/*******************************************************************************
**
** Function         avk_is_rc_open
**
** Description      Check if AVRC is open
**
** Parameters
**
** Returns          TRUE if AVRC is open, FALSE otherwise.
**
*******************************************************************************/
BOOLEAN avk_is_rc_open(BD_ADDR bda)
{
    tAPP_AVK_CONNECTION *connection = app_avk_find_connection_by_bd_addr(bda);

    if (connection == NULL)
        return FALSE;

    return connection->is_rc_open;
}

/*******************************************************************************
**
** Function         app_avk_cancel
**
** Description      Example of cancel connection command
**
** Returns          void
**
*******************************************************************************/
void app_avk_cancel()
{
    int status;
    tBSA_AVK_CANCEL_CMD bsa_avk_cancel_cmd;

    /* Send remote control command */
    status = BSA_AvkCancelCmdInit(&bsa_avk_cancel_cmd);

    status = BSA_AvkCancelCmd(&bsa_avk_cancel_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_cancel_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_get_element_attr_command
 **
 ** Description      Example of get_element_attr_command command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_get_element_attr_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_ELEMENT_ATTR bsa_avk_get_elem_attr_cmd;

    UINT8 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                     AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                     AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                     AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetElementAttrCmdInit(&bsa_avk_get_elem_attr_cmd);
    bsa_avk_get_elem_attr_cmd.rc_handle = rc_handle;
    bsa_avk_get_elem_attr_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_avk_get_elem_attr_cmd.num_attr = num_attr;
    memcpy(bsa_avk_get_elem_attr_cmd.attrs, attrs, sizeof(attrs));

    status = BSA_AvkGetElementAttrCmd(&bsa_avk_get_elem_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_element_attr_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_list_player_attr_command
 **
 ** Description      Example of app_avk_rc_list_player_attr_command command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_list_player_attr_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_LIST_PLAYER_ATTR bsa_avk_list_player_attr_cmd;

    /* Send command */
    status = BSA_AvkListPlayerAttrCmdInit(&bsa_avk_list_player_attr_cmd);
    bsa_avk_list_player_attr_cmd.rc_handle = rc_handle;
    bsa_avk_list_player_attr_cmd.label =
        app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkListPlayerAttrCmd(&bsa_avk_list_player_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_list_player_attr_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_list_player_attr_value_command
 **
 ** Description      Example of app_avk_rc_list_player_attr_value_command command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_list_player_attr_value_command(UINT8 attr, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_LIST_PLAYER_VALUES bsa_avk_list_player_attr_cmd;

    /* Send command */
    status = BSA_AvkListPlayerValuesCmdInit(&bsa_avk_list_player_attr_cmd);
    bsa_avk_list_player_attr_cmd.rc_handle = rc_handle;
    bsa_avk_list_player_attr_cmd.label =
        app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_list_player_attr_cmd.attr = attr;

    status = BSA_AvkListPlayerValuesCmd(&bsa_avk_list_player_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_list_player_attr_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_get_player_value_command
 **
 ** Description      Example of get_player_value_command command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_get_player_value_command(UINT8 *attrs, UINT8 num_attr, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_PLAYER_VALUE bsa_avk_get_player_val_cmd;

    /* Send command */
    status = BSA_AvkGetPlayerValueCmdInit(&bsa_avk_get_player_val_cmd);
    bsa_avk_get_player_val_cmd.rc_handle = rc_handle;
    bsa_avk_get_player_val_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_get_player_val_cmd.num_attr = num_attr;

    memcpy(bsa_avk_get_player_val_cmd.attrs, attrs, sizeof(UINT8) * num_attr);

    status = BSA_AvkGetPlayerValueCmd(&bsa_avk_get_player_val_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_player_value_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_set_player_value_command
 **
 ** Description      Example of set_player_value_command command
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_set_player_value_command(UINT8 num_attr, UINT8 *attr_ids, UINT8 *attr_val,
                                         UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_PLAYER_VALUE bsa_avk_set_player_val_cmd;

    /* Send command */
    status = BSA_AvkSetPlayerValueCmdInit(&bsa_avk_set_player_val_cmd);
    bsa_avk_set_player_val_cmd.rc_handle = rc_handle;
    bsa_avk_set_player_val_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_set_player_val_cmd.num_attr = num_attr;

    memcpy(bsa_avk_set_player_val_cmd.player_attr, attr_ids, sizeof(UINT8) * num_attr);
    memcpy(bsa_avk_set_player_val_cmd.player_value, attr_val, sizeof(UINT8) * num_attr);

    status = BSA_AvkSetPlayerValueCmd(&bsa_avk_set_player_val_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_player_value_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_get_play_status_command
 **
 ** Description      Example of get_play_status
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_get_play_status_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_PLAY_STATUS bsa_avk_play_status_cmd;

    /* Send command */
    status = BSA_AvkGetPlayStatusCmdInit(&bsa_avk_play_status_cmd);
    bsa_avk_play_status_cmd.rc_handle = rc_handle;
    bsa_avk_play_status_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkGetPlayStatusCmd(&bsa_avk_play_status_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_play_status_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_set_browsed_player_command
 **
 ** Description      Example of set_browsed_player
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_set_browsed_player_command(UINT16 player_id, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_BROWSED_PLAYER bsa_avk_set_browsed_player;

    /* Send command */
    status = BSA_AvkSetBrowsedPlayerCmdInit(&bsa_avk_set_browsed_player);
    bsa_avk_set_browsed_player.player_id = player_id;
    bsa_avk_set_browsed_player.rc_handle = rc_handle;
    bsa_avk_set_browsed_player.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkSetBrowsedPlayerCmd(&bsa_avk_set_browsed_player);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_browsed_player_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_set_addressed_player_command
 **
 ** Description      Example of set_addressed_player
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_set_addressed_player_command(UINT16 player_id, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_ADDR_PLAYER bsa_avk_set_player;

    /* Send command */
    status = BSA_AvkSetAddressedPlayerCmdInit(&bsa_avk_set_player);
    bsa_avk_set_player.player_id = player_id;
    bsa_avk_set_player.rc_handle = rc_handle;
    bsa_avk_set_player.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkSetAddressedPlayerCmd(&bsa_avk_set_player);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_addressed_player_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_change_path_command
 **
 ** Description      Example of change_path
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_change_path_command(UINT16 uid_counter, UINT8 direction, tAVRC_UID folder_uid,
                                    UINT8 rc_handle)
{
    int status;
    tBSA_AVK_CHG_PATH bsa_change_path;

    /* Send command */
    status = BSA_AvkChangePathCmdInit(&bsa_change_path);

    bsa_change_path.rc_handle = rc_handle;
    bsa_change_path.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_change_path.uid_counter = uid_counter;
    bsa_change_path.direction = direction;
    memcpy(bsa_change_path.folder_uid, folder_uid, sizeof(tAVRC_UID));

    status = BSA_AvkChangePathCmd(&bsa_change_path);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_change_path_command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_get_folder_items
 **
 ** Description      Example of get_folder_items
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_get_folder_items(UINT8 scope, UINT32 start_item, UINT32 end_item, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_FOLDER_ITEMS bsa_get_folder_items;

    UINT32 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                      AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                      AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                      AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetFolderItemsCmdInit(&bsa_get_folder_items);

    bsa_get_folder_items.rc_handle = rc_handle;
    bsa_get_folder_items.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_get_folder_items.scope = scope;
    bsa_get_folder_items.start_item = start_item;
    bsa_get_folder_items.end_item = end_item;

    if (AVRC_SCOPE_PLAYER_LIST != scope) {
        bsa_get_folder_items.attr_count = num_attr;
        memcpy(bsa_get_folder_items.attrs, attrs, sizeof(attrs));
    }

    status = BSA_AvkGetFolderItemsCmd(&bsa_get_folder_items);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_folder_items %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_get_items_attr
 **
 ** Description      Example of get_items_attr
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_get_items_attr(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_ITEMS_ATTR bsa_get_items_attr;

    UINT8 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                     AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                     AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                     AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetItemsAttrCmdInit(&bsa_get_items_attr);

    bsa_get_items_attr.rc_handle = rc_handle;
    bsa_get_items_attr.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_get_items_attr.scope = scope;
    memcpy(bsa_get_items_attr.uid, uid, sizeof(tAVRC_UID));
    bsa_get_items_attr.uid_counter = uid_counter;
    bsa_get_items_attr.attr_count = num_attr;
    memcpy(bsa_get_items_attr.attrs, attrs, sizeof(attrs));

    status = BSA_AvkGetItemsAttrCmd(&bsa_get_items_attr);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_items_attr %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_play_item
 **
 ** Description      Example of play_item
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_play_item(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_PLAY_ITEM bsa_play_item;

    /* Send command */
    status = BSA_AvkPlayItemCmdInit(&bsa_play_item);

    bsa_play_item.rc_handle = rc_handle;
    bsa_play_item.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_play_item.scope = scope;
    memcpy(bsa_play_item.uid, uid, sizeof(tAVRC_UID));
    bsa_play_item.uid_counter = uid_counter;

    status = BSA_AvkPlayItemCmd(&bsa_play_item);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_play_item %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_rc_add_to_play
 **
 ** Description      Example of add_to_play
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_rc_add_to_play(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_ADD_TO_PLAY bsa_add_play;

    /* Send command */
    status = BSA_AvkAddToPlayCmdInit(&bsa_add_play);

    bsa_add_play.rc_handle = rc_handle;
    bsa_add_play.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_add_play.scope = scope;
    memcpy(bsa_add_play.uid, uid, sizeof(tAVRC_UID));
    bsa_add_play.uid_counter = uid_counter;

    status = BSA_AvkAddToPlayCmd(&bsa_add_play);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_add_to_play %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_set_abs_vol_rsp
 **
 ** Description      This function sends abs vol response
 **
 ** Returns          None
 **
 *******************************************************************************/
void app_avk_set_abs_vol_rsp(UINT8 volume, UINT8 rc_handle, UINT8 label)
{
    int status;
    tBSA_AVK_SET_ABS_VOLUME_RSP abs_vol;
    BSA_AvkSetAbsVolumeRspInit(&abs_vol);

    abs_vol.label = label;
    abs_vol.rc_handle = rc_handle;
    abs_vol.volume = volume;
    status = BSA_AvkSetAbsVolumeRsp(&abs_vol);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to send app_avk_set_abs_vol_rsp %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_reg_notfn_rsp
 **
 ** Description      This function sends reg notfn response (BSA as TG role in AVK)
 **
 ** Returns          none
 **
 *******************************************************************************/
void app_avk_reg_notfn_rsp(UINT8 volume, UINT8 rc_handle, UINT8 label, UINT8 event,
                           tBTA_AV_CODE code)
{
    int status;
    tBSA_AVK_REG_NOTIF_RSP reg_notf;
    BSA_AvkRegNotifRspInit(&reg_notf);
    app_avk_cb.volume = volume;
    reg_notf.reg_notf.param.volume = volume;
    reg_notf.reg_notf.event_id = event;
    reg_notf.rc_handle = rc_handle;
    reg_notf.label = label;
    reg_notf.code = code;

    status = BSA_AvkRegNotifRsp(&reg_notf);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to send app_avk_reg_notfn_rsp %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_send_get_capabilities
 **
 ** Description      Sample code for attaining capability for events
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_send_get_capabilities(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_STATUS;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_GET_CAPABILITIES;
    bsa_avk_vendor_cmd.data[1] = 0;    /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;    /* length high*/
    bsa_avk_vendor_cmd.data[3] = 1;    /* length low*/
    bsa_avk_vendor_cmd.data[4] = 0x03; /* for event*/

    bsa_avk_vendor_cmd.length = 5;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_send_register_notification
 **
 ** Description      Sample code for attaining capability for events
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_send_register_notification(int evt, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_NOTIF;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_REGISTER_NOTIFICATION;
    bsa_avk_vendor_cmd.data[1] = 0;   /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;   /* length high*/
    bsa_avk_vendor_cmd.data[3] = 5;   /* length low*/
    bsa_avk_vendor_cmd.data[4] = evt; /* length low*/

    bsa_avk_vendor_cmd.length = 9;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_add_connection
 **
 ** Description      This function adds a connection
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_add_connection(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            app_avk_cb.connections[index].in_use = TRUE;
            return &app_avk_cb.connections[index];
        }
    }

    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == FALSE) {
            bdcpy(app_avk_cb.connections[index].bda_connected, bd_addr);
            app_avk_cb.connections[index].in_use = TRUE;
            app_avk_cb.connections[index].index = index;
            return &app_avk_cb.connections[index];
        }
    }

    return NULL;
}
/*******************************************************************************
 **
 ** Function         app_avk_reset_connection
 **
 ** Description      This function resets a connection
 **
 **
 *******************************************************************************/
void app_avk_init_connection()
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        memset(&app_avk_cb.connections[index], 0x0, sizeof(app_avk_cb.connections[index]));
        app_avk_cb.connections[index].index = index;
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_reset_connection
 **
 ** Description      This function resets a connection
 **
 **
 *******************************************************************************/
void app_avk_reset_connection(BD_ADDR bd_addr)
{
    int index;

    APP_DEBUG1("app_avk_reset_connection %02x:%02x:%02x:%02x:%02x:%02x \r\n", bd_addr[0],
               bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            app_avk_cb.connections[index].in_use = FALSE;

            app_avk_cb.connections[index].m_uiAddressedPlayer = 0;
            app_avk_cb.connections[index].m_uidCounterAddrPlayer = 0;
            app_avk_cb.connections[index].m_uid_counter = 0;
            app_avk_cb.connections[index].m_bDeviceSupportBrowse = FALSE;
            app_avk_cb.connections[index].m_uiBrowsedPlayer = 0;
            app_avk_cb.connections[index].m_iCurrentFolderItemCount = 0;
            app_avk_cb.connections[index].m_bAbsVolumeSupported = FALSE;
            app_avk_cb.connections[index].playstate = 0;
            break;
        }
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_find_connection_by_av_handle
 **
 ** Description      This function finds the connection structure by its handle
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_av_handle(UINT8 handle)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].ccb_handle == handle)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
 ** Function         app_avk_find_connection_by_rc_handle
 **
 ** Description      This function finds the connection structure by its handle
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_rc_handle(UINT8 handle)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].rc_handle == handle)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
 ** Function         app_avk_find_connection_by_bd_addr
 **
 ** Description      This function finds the connection structure by its handle
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_bd_addr(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) &&
            (app_avk_cb.connections[index].in_use == TRUE))
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
 ** Function         app_avk_find_streaming_connection
 **
 ** Description      This function finds the connection structure that is streaming
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_streaming_connection()
{
    int index;

    if (pStreamingConn && pStreamingConn->is_streaming_open && pStreamingConn->is_started)
        return pStreamingConn;

    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].is_streaming_open == TRUE &&
            app_avk_cb.connections[index].is_started == TRUE)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
 ** Function         app_avk_num_connections
 **
 ** Description      This function number of connections
 **
 ** Returns          number of connections
 **
 *******************************************************************************/
int app_avk_num_connections()
{
    int iCount = 0;
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE)
            iCount++;
    }
    return iCount;
}
/*******************************************************************************
 **
 ** Function         app_avk_find_connection_by_index
 **
 ** Description      This function finds the connection structure by its index
 **
 ** Returns          Pointer to the found structure or NULL
 **
 *******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_index(int index)
{
    if (index < APP_AVK_MAX_CONNECTIONS)
        return &app_avk_cb.connections[index];

    return NULL;
}

/*******************************************************************************
 **
 ** Function         app_avk_pause_other_streams
 **
 ** Description      This function pauses other streams when a new stream start
 **                  is received from device identified by bd_addr
 **
 ** Returns          None
 **
 *******************************************************************************/
void app_avk_pause_other_streams(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr)) &&
            app_avk_cb.connections[index].in_use &&
            app_avk_cb.connections[index].is_streaming_open) {
            app_avk_cb.connections[index].is_started = FALSE;
        }
    }
}

/*******************************************************************************
 **
 ** Function         app_avk_send_delay_report
 **
 ** Description      Sample code to send delay report
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_send_delay_report(UINT16 delay)
{
    tBSA_AVK_DELAY_REPORT report;
    int status;

    APP_DEBUG1("send delay report:%d", delay);

    BSA_AvkDelayReportInit(&report);
    report.delay = delay;
    status = BSA_AvkDelayReport(&report);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send delay report %d", status);
    }
}
int app_avk_get_index_by_ad_addr(BD_ADDR bd_addr)
{
    int connection_index = -1;
    int index;
    APP_DEBUG1("%02x:%02x:%02x:%02x:%02x:%02x \r\n", bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3],
               bd_addr[4], bd_addr[5]);
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            connection_index = index;
            break;
        }
    }
    return connection_index;
}
int app_avk_set_pcm_call_back_function(pcm_render_call_back pcm_render_call, void *context)
{
    g_pcm_context = context;
    g_pcm_open_call = app_avk_cb.pcm_open_call = pcm_render_call.pcm_open_call_fn;
    g_pcm_close_call = app_avk_cb.pcm_close_call = pcm_render_call.pcm_close_call_fn;
    g_pcm_write_data_call = app_avk_cb.pcm_write_data_call = pcm_render_call.pcm_write_data_fn;
}
