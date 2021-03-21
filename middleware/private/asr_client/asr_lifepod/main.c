#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <resolv.h>
#include <sys/stat.h>

#define PROCPS_BUFSIZE 1024
volatile long long tts_end_tick = 0;

#include "wm_util.h"
#include "caiman.h"
#include "ccaptureGeneral.h"
#include "save_record_file.h"
#include "alexa_request_json.h"
#include "cxdish_ctrol.h"
#include "alexa_json_common.h"
#include "alexa_cmd.h"
#include "alexa.h"
#include "alexa_authorization.h"
#include "lpcrypto.h"
#include "lpcfg.h"

#ifdef OPENWRT_AVS
#include <sys/types.h>
#include <sys/shm.h>
#include "multiroom_config.h"
#endif
#include "asr_state.h"
#include "asr_config.h"
#include "avs_player.h"
#include "alexa_system.h"
#ifdef BT_HFP_ENABLE
#include "playback_bt_hfp.h"
#endif
#include "file_record.h"

#define CMD_ALERT_FORE "ALERT+FORE"
#define CMD_ALERT_BACK "ALERT+BACK"

#define MAX_RECORD_TIMEOUT_MS 15000

#define DEFAULT_CH_ASR 1     // left channel for ASR
#define DEFAULT_CH_TRIGGER 1 // left channel for Trigger
#if defined(A98_TEUFEL)
#define DEFAULT_NOISE_GATE 0
#else
#define DEFAULT_NOISE_GATE 1
#endif
#define MIC_DISTANCE_ANKER_TR1 (42)

#ifdef HARMAN_HOME_ALLURE
#define MIN_START_PROMPT_VOLUME 10
#else
#define MIN_START_PROMPT_VOLUME 20
#endif

#ifdef OPENWRT_AVS
#define ASR_SEARCH_ID_DEFAULT 8
#else
#if defined(YD_AMAZON_MODULE)
#define ASR_SEARCH_ID_DEFAULT 13 // 14
#else
#define ASR_SEARCH_ID_DEFAULT 4
#endif
#endif

#ifndef ASR_HAVE_PROMPT_DEFAULT
#define ASR_HAVE_PROMPT_DEFAULT 0
#endif

#include "alexa.h"
#include "alexa_alert.h"
#include "wm_api.h"
#include "alexa_cfg.h"
#include "intercom_record.h"
#include "alexa_cmd.h"
#include "alexa_json_common.h"
#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

WIIMU_CONTEXT *g_wiimu_shm = 0;
alexa_dump_t alexa_dump_info;
static int do_talkstop(int errcode, int req_from);
static ASR_ERROR_CODE do_talkstart_check(int req_from);
static int do_talkstart(int req_from);
static int do_talkstart_prepare(int req_from);
static int do_talkcontinue(int errCode, int req_from);
extern void StartRecordTest(void);
extern void StopRecordTest(void);
static void unauthorize_action(void);
extern int mic_channel_test_mode;
int g_TTS_aborted = 0;

int g_ntp_synced = 0;
int g_alexa_req_from = 0;
static int g_asr_disable = 0;
static int g_record_testing = 0;
static int g_comms_start = 0;
int g_internet_access = 0; // debug 1
extern uint32_t chunk_sample_total_min;
extern uint32_t chunk_sample_total_max;
extern uint32_t chunk_sample_total;
extern uint32_t samples_per_chunk;
extern uint32_t voice_detected;
extern int ignore_reset_trigger;
extern unsigned int g_engr_voice;
extern unsigned int g_engr_noise;
extern void ASR_SetRecording_TimeOut(int time_out);

#define ASR_MIC_BOOST_MAX ((g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC) ? 40 : 30)

static void pcm_playback(char *path)
{
    char command[128] = {0};
    char timestr[20] = {0};
    char record_file[128] = {0};

    if (!path) {
        return;
    }

    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDPLAYTTS",
                             strlen("GNOTIFY=MCULEDPLAYTTS"), NULL, NULL, 0);
#if defined(PLATFORM_AMLOGIC)
    system("echo AXX+MUT+000 > /dev/ttyS3");
#else
    system("echo AXX+MUT+000 > /dev/ttyS0");
#endif

    if (alexa_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
        struct tm *pTm = gmtime(&alexa_dump_info.last_dialog_ts);

        snprintf(timestr, sizeof(timestr), ALEXA_DATE_FMT, pTm->tm_year + 1900, pTm->tm_mon + 1,
                 pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

        if (!strncmp(path, ALEXA_RECORD_FILE, strlen(path))) {
            snprintf(record_file, sizeof(record_file), ALEXA_RECORD_FMT, g_wiimu_shm->mmc_path,
                     timestr);
        } else if (!strncmp(path, ALEXA_UPLOAD_FILE, strlen(path))) {
            snprintf(record_file, sizeof(record_file), ALEXA_UPLOAD_FMT, g_wiimu_shm->mmc_path,
                     timestr);
        }
    } else {
        memcpy(record_file, path, strlen(path));
    }

    if (mic_channel_test_mode > 0)
        snprintf(command, sizeof(command), "aplay -r 16000 -c 2 -f S16_LE %s", record_file);
    else
        snprintf(command, sizeof(command), "echo 1 >  /tmp/pcm_output_channel && "
                                           "aplay -r 16000 -c 1 -f S16_LE %s && "
                                           "rm /tmp/pcm_output_channel -f",
                 record_file);
    printf("run[%s]\n", command);
    system(command);
    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDIDLE",
                             strlen("GNOTIFY=MCULEDIDLE"), NULL, NULL, 0);
}

#define ALSA_FAR_MIN_VOL (0x200)
#define ALSA_FAR_MAX_VOL (0x10000)

void switch_content_to_background(int val)
{
    g_wiimu_shm->asr_status = val ? 1 : 0;
    if ((CLOSE_TALK != AlexaProfileType()) && ASR_IS_NEED_PAUSE(alexa_dump_info.asr_test_flag)) {
        return;
    }

    if (val) {
#ifdef AVS_MRM_ENABLE
        if (!AlexaDisableMrm()) {
            SocketClientReadWriteMsg("/tmp/alexa_mrm", "GNOTIFY=ReqAvsMrmFocusBackground",
                                     strlen("GNOTIFY=ReqAvsMrmFocusBackground"), NULL, NULL, 0);
        }
#endif

#ifdef HARMAN
        if (g_wiimu_shm->device_is_hk_portable == 1)
            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 4, SNDSRC_ASR);
        else
            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 9, SNDSRC_ASR);
#else
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 1, SNDSRC_ASR);
#endif
    } else {
#ifdef AVS_MRM_ENABLE
        if (!AlexaDisableMrm()) {
            SocketClientReadWriteMsg("/tmp/alexa_mrm", "GNOTIFY=ReqAvsMrmFocusForeground",
                                     strlen("GNOTIFY=ReqAvsMrmFocusForeground"), NULL, NULL, 0);
        }
#endif
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);
    }
}
#define CMD_ALERT_FORE "ALERT+FORE"
#define CMD_ALERT_BACK "ALERT+BACK"

void switch_alert_to_background(int val)
{
    if (val)
        SocketClientReadWriteMsg(ALERT_SOCKET_NODE, CMD_ALERT_BACK, strlen(CMD_ALERT_BACK), NULL,
                                 NULL, 0);
    else {
        if (!g_comms_start)
            SocketClientReadWriteMsg(ALERT_SOCKET_NODE, CMD_ALERT_FORE, strlen(CMD_ALERT_FORE),
                                     NULL, NULL, 0);
    }
}

size_t get_file_len(const char *path)
{
    struct stat buf;
    if (stat(path, &buf))
        return 0;
    return buf.st_size;
}

// void delay_for_talkmode_switch(void)
//{
//  if (g_wiimu_shm->priv_cap2 & (1<<CAP2_FUNC_DISABLE_PLAY_CAP_SIM))
//      usleep(100000);// waiting for mcu talkmode switch
//}

void block_voice_prompt(char *path, int block)
{
    char file_vol[256];
    int volume = 100;
    char *is_start_voice = NULL;

    // printf(">>>>init value=%d\n",g_wiimu_shm->volume);
    if (g_wiimu_shm->max_prompt_volume && g_wiimu_shm->volume >= g_wiimu_shm->max_prompt_volume)
        volume = 100 * g_wiimu_shm->max_prompt_volume / g_wiimu_shm->volume;
    strncpy(file_vol, path, sizeof(file_vol) - 1);
    if (!volume_prompt_exist(path)) {
        strncpy(file_vol, "common/ring2.mp3", sizeof(file_vol) - 1);
    }

    is_start_voice = strstr(file_vol, "alexa_start");
    if (!is_start_voice) {
        is_start_voice = strstr(file_vol, "talk_start_t");
    }
    if (is_start_voice && (far_field_judge(g_wiimu_shm))) {
        volume = g_wiimu_shm->volume;
        if (volume < 50)
            volume = 100;
        else
            volume = 100 - (volume * 50) / 100;
        volume_pormpt_direct(file_vol, block, volume);
    } else if (volume == 100) {
        volume_pormpt_direct(file_vol, block, 0);
    } else {
        volume_pormpt_direct(file_vol, block, volume);
    }
}

/* block play alexa erro prompt */
void play_error_voice(int req_from, int errcode)
{
    int prompt_type = -1;

    switch (errcode) {
    case ERR_ASR_NO_INTERNET:
        prompt_type = VOLUME_INTERNET_LOST; // internet_lost
        break;

    case ERR_ASR_MODULE_DISABLED:
        // block_voice_prompt("common/ring2.mp3", 1);
        break;

    case ERR_ASR_DEVICE_IS_SLAVE:
        if (0 == req_from) {
            prompt_type = VOLUME_ALEXA_SLAVE_NO_SUPPORT; // alexa_not_support_slave.mp3
        }
        break;

    case ERR_ASR_NOTHING_PLAY: {
        char *path = "common/ring2.mp3";
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0 &&
            volume_prompt_exist("cn/CN_nothing_play.mp3"))
            path = "cn/CN_nothing_play.mp3";
        block_voice_prompt(path, 1);
        prompt_type = -1;
    } break;

    case ERR_ASR_USER_LOGIN_FAIL:
        prompt_type = VOLUME_ALEXA_AUTH; // alexa_auth.mp3,
        break;

    case ERR_ASR_NO_USER_ACCOUNT:
        prompt_type = VOLUME_ALEXA_AUTH; // alexa_auth.mp3,
        break;

    case ERR_ASR_HAVE_PROMBLEM:
        prompt_type = VOLUME_ALEXA_TROUBLE; // alexa_trouble.mp3
        break;

    default:
        break;
    } // switch

    if (prompt_type != -1) {
        volume_pormpt_block(prompt_type, 1);
    }
}

void asr_set_audio_pause(int flags)
{
    switch_content_to_background(1);
    switch_alert_to_background(1);
    alexa_book_ctrol(1);
}

void asr_set_audio_resume(void)
{
    wiimu_log(1, 0, 0, 0, "%s resume mode=%d, TTS status=%d, alert_status=%d", __FUNCTION__,
              g_wiimu_shm->play_mode, alexa_get_tts_status(), g_wiimu_shm->alert_status);
    if (g_wiimu_shm->play_mode == PLAYER_MODE_BT)
        usleep(200000);
    focus_process_lock();

    //{{workaround for "will play a little before pause when we try to voice to pause the playback"
    if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode) && g_wiimu_shm->play_status == player_pausing)
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_MPLAYER, 1);
    //}}

    switch_content_to_background(0);

    if (alexa_get_tts_status() == 0 && g_wiimu_shm->alert_status == 0) {
        g_wiimu_shm->asr_pause_request = 0;
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        alexa_book_ctrol(0);
    }
    if (alexa_get_tts_status() == 0)
        switch_alert_to_background(0);
    focus_process_unlock();
}

void AudioCapReset(int enable)
{
    if (enable) {
        if (g_wiimu_shm->asr_mic_needless == 0) {
            CAIMan_deinit();
            CAIMan_YD_stop();
            CAIMan_ASR_stop();

            CAIMan_init();
            CAIMan_YD_start();
            CAIMan_ASR_start();
            CAIMan_YD_enable();
        }
    } else {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();
    }
    g_wiimu_shm->asr_ongoing = 0;
    g_asr_disable = 0;
}

int get_asr_search_id(void)
{
    int id = ASR_SEARCH_ID_DEFAULT;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_SEARCH_ID, buf, sizeof(buf));
    if (strlen(buf) > 0)
        id = atoi(buf);

    if (id < 1 || id > 20)
        id = ASR_SEARCH_ID_DEFAULT;
    g_wiimu_shm->asr_search_id = id;
    return id;
}

static int web_dump_refresh(SOCKET_CONTEXT *pmsg_socket, char *recv_buf)
{
    struct tm *pTm = gmtime(&alexa_dump_info.last_dialog_ts);
    char timestr[20] = {0};
    char tmp_buf[128] = {0};
    char record_file[128] = {0};
    char upload_file[128] = {0};
    char error_file[128] = {0};
    char cmdline[1024] = {0};
    char request_para[512] = {0};
    char *cxdish_str[5] = {NULL};
    int record_len = 0;
    int upload_len = 0;
    if (!strlen(g_wiimu_shm->asr_dsp_fw_ver)) {
#ifndef S11_EVB_IMUZOBOX
        if (far_field_judge(g_wiimu_shm)) {
            read_dsp_version(g_wiimu_shm);
        }
#endif
    }

    snprintf(timestr, sizeof(timestr), "%d%.2d%.2d_%.2d_%.2d_%.2d", pTm->tm_year + 1900,
             pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    snprintf(record_file, sizeof(record_file), "%s_v%d_%s_%s", g_wiimu_shm->project_name,
             g_wiimu_shm->mcu_ver, timestr, "record.pcm");
    snprintf(upload_file, sizeof(upload_file), "%s_v%d_%s_%s", g_wiimu_shm->project_name,
             g_wiimu_shm->mcu_ver, timestr, "upload.pcm");
    snprintf(error_file, sizeof(error_file), "%s.%s", ALEXA_ERROR_FILE, timestr);

    if (alexa_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
        memset(tmp_buf, 0x0, sizeof(tmp_buf));
        snprintf(tmp_buf, sizeof(tmp_buf), "%s/%s_record.pcm", g_wiimu_shm->mmc_path, timestr);
        memset(cmdline, 0x0, sizeof(cmdline));
        snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", tmp_buf,
                 record_file);
        system(cmdline);
        record_len = get_file_len(tmp_buf);

        memset(tmp_buf, 0x0, sizeof(tmp_buf));
        snprintf(tmp_buf, sizeof(tmp_buf), "%s/%s_upload.pcm", g_wiimu_shm->mmc_path, timestr);
        memset(cmdline, 0x0, sizeof(cmdline));
        snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", tmp_buf,
                 upload_file);
        system(cmdline);
        upload_len = get_file_len(tmp_buf);
    } else {
        memset(cmdline, 0x0, sizeof(cmdline));
        snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", ALEXA_RECORD_FILE,
                 record_file);
        system(cmdline);
        record_len = get_file_len(ALEXA_RECORD_FILE);

        memset(cmdline, 0x0, sizeof(cmdline));
        snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", ALEXA_UPLOAD_FILE,
                 upload_file);
        system(cmdline);
        upload_len = get_file_len(ALEXA_UPLOAD_FILE);
    }

    memset(cmdline, 0x0, sizeof(cmdline));
    snprintf(cmdline, sizeof(cmdline), "ln -s %s %s 2>/dev/null", ALEXA_ERROR_FILE, error_file);
    system(cmdline);

    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN,
             "TIME: %s<hr>"
             "record_ms=%lld vad_ms=%lld<hr>"
             "record_len=%d upload_len=%d error_len=%d<hr>"
             "login=%d http_ret_code=%d on_line=%d record_to_sd=%d<hr>",
             timestr, alexa_dump_info.record_end_ts - alexa_dump_info.record_start_ts,
             (alexa_dump_info.vad_ts) ? alexa_dump_info.vad_ts - alexa_dump_info.record_start_ts
                                      : 0LL,
             record_len, upload_len, get_file_len(ALEXA_ERROR_FILE), alexa_dump_info.login,
             alexa_dump_info.http_ret_code, alexa_dump_info.on_line, alexa_dump_info.record_to_sd);

    strcat(recv_buf, "alexa_record.pcm &nbsp;&nbsp;<a href=httpapi.asp?command=talkdump:0 "
                     "target=_alexaplay>Play</a>");
    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN, "%s&nbsp;&nbsp;<a href=data/%s>download</a><hr>",
             recv_buf, record_file);

    strcat(recv_buf, "alexa_upload.pcm &nbsp;&nbsp;<a href=httpapi.asp?command=talkdump:1 "
                     "target=_alexaplay>Play</a>");
    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN, "%s&nbsp;&nbsp;<a href=data/%s>download</a><hr>",
             recv_buf, upload_file);

    // if (get_file_len(ALEXA_ERROR_FILE) > 0) {
    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN,
             "%salexa_response.txt&nbsp;<a href=data/alexa_response.txt.%s>download</a><hr>",
             recv_buf, timestr);
//}

#ifdef VAD_SUPPORT
    char chunk_para[128] = {0};
    snprintf(chunk_para, sizeof(chunk_para), "chunk_min=%d "
                                             "chunk_max=%d "
                                             "chunk_avg=%d "
                                             "voice_detected=%d <hr>",
             chunk_sample_total_min / samples_per_chunk, chunk_sample_total_max / samples_per_chunk,
             chunk_sample_total / samples_per_chunk, voice_detected);
    strcat(recv_buf, chunk_para);
#endif

    snprintf(request_para, sizeof(request_para), "times_talk_on=%d "
                                                 "times_request_succeed=%d "
                                                 "times_request_failed=%d "
                                                 "times_vad_time_out=%d "
                                                 "times_play_failed=%d "
                                                 "times_open_failed=%d "
                                                 "times_capture_failed=%d "
                                                 "score=%d "
                                                 "status:%d<hr>",
             g_wiimu_shm->times_talk_on, g_wiimu_shm->times_request_succeed,
             g_wiimu_shm->times_request_failed, g_wiimu_shm->times_vad_time_out,
             g_wiimu_shm->times_play_failed, g_wiimu_shm->times_open_failed,
             g_wiimu_shm->times_capture_failed, g_wiimu_shm->wakeup_score,
             g_wiimu_shm->asr_last_status);
    strcat(recv_buf, request_para);

#ifndef S11_EVB_IMUZOBOX
    if (far_field_judge(g_wiimu_shm)) {
        int i = 0;
        memset(request_para, 0x0, sizeof(request_para));
        if (!cxdish_para_get(cxdish_str)) {
            char cxdish_ver_str[128];
            memset((void *)cxdish_ver_str, 0, sizeof(cxdish_ver_str));
            snprintf(cxdish_ver_str, sizeof(cxdish_ver_str),
                     "search id:%d cxdish version:%s testflag:%d-0x%x(%s mode) (vad mode:%s) "
                     "(wakeup word:%d) (have prompt:%d)<hr>",
                     get_asr_search_id(), g_wiimu_shm->asr_dsp_fw_ver,
                     alexa_dump_info.asr_test_flag, alexa_dump_info.asr_test_flag,
                     ASR_IS_I2S_REFRENCE(alexa_dump_info.asr_test_flag)
                         ? "i2s reference"
                         : (ASR_IS_MIC_BYPASS(alexa_dump_info.asr_test_flag) ? "2 mic bypass"
                                                                             : "normal"),
                     ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag) ? "server" : "local",
                     ASR_IS_WAKEUP_WORD(alexa_dump_info.asr_test_flag) ? 1 : 0,
                     g_wiimu_shm->asr_have_prompt);
            strcat(recv_buf, cxdish_ver_str);
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetTestFlag:0 "
                             "target=_alexaplay>Normal MODE</a>&nbsp;&nbsp;");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetTestFlag:4 target=_alexaplay>I2S "
                             "Refrence MODE</a>&nbsp;&nbsp;");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetTestFlag:8 target=_alexaplay>MIC "
                             "bypass MODE</a>&nbsp;&nbsp;");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetTestFlag:-4 "
                             "target=_alexaplay>Local Vad</a>&nbsp;&nbsp;");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetTestFlag:-5 target=_alexaplay>no "
                             "wake up word</a>&nbsp;&nbsp;<hr>");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetPrompt:1 target=_alexaplay>Have "
                             "Prompt</a>&nbsp;&nbsp;");
            strcat(recv_buf, "<a href=httpapi.asp?command=talksetPrompt:0 target=_alexaplay>No "
                             "Prompt</a>&nbsp;&nbsp;<hr>");

            if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC) {
                snprintf(request_para, sizeof(request_para),
                         "CXDISH_MODE=%s gain1=%s gain2=%s gain3=%s gain4=%s<hr>", cxdish_str[0],
                         cxdish_str[1], cxdish_str[2], cxdish_str[3], cxdish_str[4]);
            } else {
                snprintf(request_para, sizeof(request_para),
                         "CXDISH_MODE=%s ADC0_GAIN=%s ADC1_GAIN=%s ADC0_BOOST=%s ADC1_BOOST=%s<hr>",
                         cxdish_str[0], cxdish_str[1], cxdish_str[2], cxdish_str[3], cxdish_str[4]);
            }
            strcat(recv_buf, request_para);
            for (i = 0; i < 5; i++)
                free(cxdish_str[i]);
        }
    }
#endif

    if (g_wiimu_shm->asr_test_mode) { // vad_test mode on
        strcat(recv_buf, "<a href=httpapi.asp?command=vad_test:0 target=_alexaplay>SWITCH TO "
                         "NORMAL MODE</a><hr>");
    } else {
        strcat(recv_buf, "<a href=httpapi.asp?command=vad_test:1 target=_alexaplay>SWITCH TO MIC "
                         "TEST MODE</a><hr>");
    }
#ifdef AVS_ARBITRATION
    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN,
             "%sESP data : En_voice = %u, En_noise = %u.&nbsp;<a href=data/vad.txt>vad.txt "
             "download</a> <a href=data/energyVal.csv>energyVal.csv download</a><hr>",
             recv_buf, g_engr_voice, g_engr_noise);
#endif
    strcat(recv_buf, "<a href=httpapi.asp?command=alexadump target=_self>Refresh</a><br>");
    UnixSocketServer_write(pmsg_socket, recv_buf, strlen(recv_buf));
    return 0;
}

static int web_getLifepodProfile(SOCKET_CONTEXT *pmsg_socket, int encry)
{
    char *result = LifepodGetDeviceInfoJson();
    char *encrypt_str = NULL;
    lp_aes_cbc_ctx_t aes_cbc_param;
    if (result) {
        if (encry) {
            encrypt_str = (char *)calloc(1, strlen(result) * 2);
            if (encrypt_str) {
                memset(&aes_cbc_param, 0, sizeof(lp_aes_cbc_ctx_t));
                memcpy(aes_cbc_param.key, g_wiimu_shm->uuid, sizeof(aes_cbc_param.key));
                aes_cbc_param.key_bits = 16 * 8;
                aes_cbc_param.padding_mode = LP_CIPHER_PADDING_MODE_ZERO;
                aes_cbc_param.is_base64_format = 1;
                lp_aes_cbc_encrypt((unsigned char *)result, strlen(result),
                                   (unsigned char *)encrypt_str, strlen(result) * 2,
                                   &aes_cbc_param);
                UnixSocketServer_write(pmsg_socket, encrypt_str, strlen(encrypt_str));

                free(encrypt_str);
            }
        } else {
            UnixSocketServer_write(pmsg_socket, result, strlen(result));
        }
        free(result);
    }
    return 0;
}

static void get_angle_base_from_nvram(void)
{
    int val = 0;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_MIC_ANGLE, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);

    if (val > 0 && val <= 360)
        g_wiimu_shm->asr_mic_angle_base = val;
}

static void get_mic_distance_from_nvram(void)
{
    int val = 0;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_MIC_DISTANCE, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);

    g_wiimu_shm->asr_mic_distance = val;
}

static void get_trigger_ch_from_nvram(void)
{
    int val = 1;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_CH_TRIGGER, buf, sizeof(buf));
    g_wiimu_shm->asr_ch_trigger = DEFAULT_CH_TRIGGER;
    if (strlen(buf) > 0) {
        val = atoi(buf);
        g_wiimu_shm->asr_ch_trigger = val;
    }
}

static void get_asr_ch_from_nvram(void)
{
    int val = DEFAULT_CH_ASR;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_CH_ASR, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);
    g_wiimu_shm->asr_ch_asr = val;
#if defined(SOFT_PREP_HOBOT_MODULE)
    printf("#### get_asr_ch_from_nvram = %d", g_wiimu_shm->asr_ch_asr);
    g_wiimu_shm->asr_ch_asr = 1; // force set asr_channel
#endif
}

static void get_noise_gate_from_nvram(void)
{
    int val = 3;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_NOISE_GATE, buf, sizeof(buf));
    g_wiimu_shm->asr_dsp_noise_gating = DEFAULT_NOISE_GATE;
    if (strlen(buf) > 0) {
        val = atoi(buf);
        g_wiimu_shm->asr_dsp_noise_gating = val;
    }
}

static void get_prompt_setting_from_nvram(void)
{
    int val = ASR_HAVE_PROMPT_DEFAULT;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_HAVE_PROMPT, buf, sizeof(buf));
    if (strlen(buf) > 0) {
        val = atoi(buf);
    }
    g_wiimu_shm->asr_have_prompt = (val == 1) ? 1 : 0;
}

#ifdef IHOME_ALEXA
static int default_mic_boost[ASR_MIC_BOOST_STEPS] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 8};
#elif defined(FENDA_W55)
static int default_mic_boost[ASR_MIC_BOOST_STEPS] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 8};
// MEMS MIC
#elif defined(S11_EVB_EUFY_DOT)
static int default_mic_boost[ASR_MIC_BOOST_STEPS] = {18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
#elif defined(MIC_TYPE_IS_MEMS)
static int default_mic_boost[ASR_MIC_BOOST_STEPS] = {18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
#else
static int default_mic_boost[ASR_MIC_BOOST_STEPS] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 8};
#endif
static int default_dmic_gain[ASR_MIC_BOOST_STEPS] = {40, 40, 40, 40, 40, 40, 40, 40, 36, 36, 36};
static int default_mems_mic_boost[ASR_MIC_BOOST_STEPS] = {18, 18, 18, 18, 18, 18,
                                                          18, 18, 18, 18, 18};

static void get_micboost_from_nvram(void)
{
    int val = 0;
    int i = 0;
    char buf[128];
    char str[256] = {0};
    for (i = 0; i < ASR_MIC_BOOST_STEPS; ++i) {
        if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC)
            default_mic_boost[i] = default_dmic_gain[i];
        else if (strcmp(g_wiimu_shm->project_name, "Zolo_Halo") == 0)
            default_mic_boost[i] = default_mems_mic_boost[i];
        memset((void *)buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%s%d", NVRAM_ASR_MIC_BOOST, i);
        asr_config_get(buf, str, sizeof(str));
        if (strlen(str) > 0) {
            val = atoi(str);
            if (val > 0 && val <= ASR_MIC_BOOST_MAX) {
                default_mic_boost[i] = val;
                printf("default_mic_boost[%d] from nvram = %d\n", i, default_mic_boost[i]);
            }
        }
    }
}

#define ADC_GAIN_DEFAULT_VAL 0x258
#define ADC_GAIN_PLAYBACK_DEFAULT_VAL 0x258
static void get_adc_gain_from_nvram(int id)
{
    int val = 0;
    id = id ? 1 : 0;
    char buf[256] = {0};
    asr_config_get((id == 0) ? NVRAM_ASR_ADC_GAIN0 : NVRAM_ASR_ADC_GAIN1, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);

    if (val <= 0)
        g_wiimu_shm->asr_adc_gain_step[id] =
            (id == 0) ? ADC_GAIN_DEFAULT_VAL : ADC_GAIN_PLAYBACK_DEFAULT_VAL;
    else
        g_wiimu_shm->asr_adc_gain_step[id] = val;
}

static void cxdish_set_adc_gain_step(int id, int val)
{
    char str[32];
    memset((void *)str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d", val);
    if (id == 0 || id == 2) {
        asr_config_set(NVRAM_ASR_ADC_GAIN0, str);
        g_wiimu_shm->asr_adc_gain_step[0] = val;
    }
    if (id == 1 || id == 2) {
        asr_config_set(NVRAM_ASR_ADC_GAIN1, str);
        g_wiimu_shm->asr_adc_gain_step[1] = val;
    }
    asr_config_sync();
}

static void update_mic_boost_config(int adjust)
{
    int i;
    char nvram_micboost[32];
    char nvram_micboost_val[32];
    for (i = 0; i < ASR_MIC_BOOST_STEPS; ++i) {
        g_wiimu_shm->asr_mic_boost[i] = default_mic_boost[i] + adjust;
        if (g_wiimu_shm->asr_mic_boost[i] > ASR_MIC_BOOST_MAX)
            g_wiimu_shm->asr_mic_boost[i] = ASR_MIC_BOOST_MAX;
        else if (g_wiimu_shm->asr_mic_boost[i] < 0)
            g_wiimu_shm->asr_mic_boost[i] = 0;
        if (adjust) {
            memset((void *)nvram_micboost, 0, sizeof(nvram_micboost));
            snprintf(nvram_micboost, sizeof(nvram_micboost), "%s%d", NVRAM_ASR_MIC_BOOST, i);
            memset((void *)nvram_micboost_val, 0, sizeof(nvram_micboost_val));
            snprintf(nvram_micboost_val, sizeof(nvram_micboost_val), "%d",
                     g_wiimu_shm->asr_mic_boost[i]);
            asr_config_set(nvram_micboost, nvram_micboost_val);
        }
    }
    if (adjust)
        asr_config_sync();
}

static void asr_dsp_init(void)
{
    get_adc_gain_from_nvram(0);
    get_adc_gain_from_nvram(1);
    get_micboost_from_nvram();
    get_angle_base_from_nvram();
    get_mic_distance_from_nvram();
    get_trigger_ch_from_nvram();
    get_asr_ch_from_nvram();
    get_noise_gate_from_nvram();

    update_mic_boost_config(0);

    if (!strlen(g_wiimu_shm->asr_dsp_fw_ver) || strlen(g_wiimu_shm->asr_dsp_fw_ver) > 16)
        read_dsp_version(g_wiimu_shm);

    dsp_init();
}

#if defined(A98_EATON)
int g_prompt_on = 0;
#endif
static void asr_play_stop_prompt(int flag)
{
    if (flag && (CLOSE_TALK != AlexaProfileType())) {
        CAIMan_ASR_finished();
    }
    if (g_wiimu_shm->asr_have_prompt && CLOSE_TALK != AlexaProfileType()) {
        g_wiimu_shm->smplay_skip_silence = 1;
#if defined(A98_EATON)
        g_prompt_on = 1;
#endif
        block_voice_prompt("common/stop_prompt.mp3", 1);
#if defined(A98_EATON)
        usleep(100000);
        g_prompt_on = 0;
#endif
    } else if (CLOSE_TALK == AlexaProfileType()) {
        g_wiimu_shm->smplay_skip_silence = 1;
#if defined(SOFT_PREP_HOBOT_MODULE)
        if (g_wiimu_shm->asr_have_prompt) {
#endif
            if (g_alexa_req_from != 3)
                block_voice_prompt("common/talk_stop_t.mp3", 1);
#if defined(SOFT_PREP_HOBOT_MODULE)
        }
#endif
    }
}

int evtSocket_start(void)
{
    int recv_num;
    char *recv_buf = (char *)malloc(MAX_SOCKET_RECV_BUF_LEN);
    int ret;
    int tlk_start_cnt = 0;
    int is_background = 0;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/RequestASRTTS";
    msg_socket.blocking = 1;

    if (!recv_buf) {
        fprintf(stderr, "%s malloc failed\n", __FUNCTION__);
        exit(-1);
    }

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        g_wiimu_shm->asr_ready = 0;
        printf("ASRTTS UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1) {
        g_wiimu_shm->asr_ready = 1;
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            printf("ASRTTS UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, MAX_SOCKET_RECV_BUF_LEN);
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, MAX_SOCKET_RECV_BUF_LEN);
            if (recv_num <= 0) {
                printf("ASRTTS recv failed\r\n");
                if (recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';

                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf %s +++++++++", recv_buf);
                if (strncmp(recv_buf, "CAP_RESET", strlen("CAP_RESET")) == 0) {
                    if (!g_asr_disable)
                        AudioCapReset(1);
                    else
                        AudioCapReset(0);
                } else if (strncmp(recv_buf, "PLMSwitched", strlen("PLMSwitched")) == 0) {
                    g_TTS_aborted = 1;
                } else if (strncmp(recv_buf, "play_onepause", strlen("play_onepause")) == 0) {
                    g_TTS_aborted = 1;
                } else if (strncmp(recv_buf, "FocusManage:", strlen("FocusManage:")) == 0) {
                    char *action = recv_buf + strlen("FocusManage:");
                    if (strncmp(action, "AlertBegin", strlen("AlertBegin")) == 0) {
                        focus_process_lock();

#ifdef HARMAN
                        if (g_wiimu_shm->device_is_hk_portable == 1)
                            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 2, SNDSRC_SMPLAYER);
                        else
                            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 4, SNDSRC_SMPLAYER);
#else
                        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 1, SNDSRC_SMPLAYER);
#endif
                        focus_process_unlock();
                    } else if (strncmp(action, "AlertEnd", strlen("AlertEnd")) == 0) {
                        focus_process_lock();
                        printf("TTS status=%d \n", alexa_get_tts_status());
                        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_SMPLAYER);
                        if (alexa_get_tts_status() == 0)
                            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
                        if (alexa_get_tts_status() == 0 && g_wiimu_shm->asr_status == 0) {
                            alexa_book_ctrol(0);
                            g_wiimu_shm->asr_pause_request = 0;
                        }
                        focus_process_unlock();
                    } else if (strncmp(action, "TTSBegin", strlen("TTSBegin")) == 0) {
                        printf("alert_status=%d is_background=%d\n", g_wiimu_shm->alert_status,
                               is_background);
                        focus_process_lock();

#ifdef HARMAN
                        if (g_wiimu_shm->device_is_hk_portable == 1)
                            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 2, SNDSRC_ALEXA_TTS);
                        else
                            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 4, SNDSRC_ALEXA_TTS);
#else
                        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS);
#endif
                        switch_alert_to_background(1);
                        if (g_wiimu_shm->alert_status && is_background == 0) {
                            is_background = 1;
                            alexa_string_cmd(EVENT_ALERT_BACKGROUND, NAME_ALERT_BACKGROUND);
                        }
                        focus_process_unlock();

#if defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(MARSHALL_ACTON_AVS_TBD_BETA)
                        usleep(100000);
                        NOTIFY_CMD(IO_GUARD, PROMPT_START);
#endif
                    } else if (strncmp(action, "TTSEnd", strlen("TTSEnd")) == 0) {
                        printf("alert_status=%d is_background=%d cut off value=%lld\n",
                               g_wiimu_shm->alert_status, is_background,
                               tickSincePowerOn() - tts_end_tick);
#if defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(MARSHALL_ACTON_AVS_TBD_BETA)
                        NOTIFY_CMD(IO_GUARD, PROMPT_END);
#endif
                        focus_process_lock();
                        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
                        if (alexa_get_tts_status() == 0 && g_wiimu_shm->asr_status == 0)
                            switch_alert_to_background(0);
                        if (alexa_get_tts_status() == 0 && g_wiimu_shm->alert_status == 0 &&
                            g_wiimu_shm->asr_status == 0) {
                            alexa_book_ctrol(0);
                            g_wiimu_shm->asr_pause_request = 0;
                        }
                        if (g_wiimu_shm->alert_status) {
                            alexa_string_cmd(EVENT_ALERT_FORCEGROUND, NAME_ALERT_FOREGROUND);
                        }
                        is_background = 0;
                        focus_process_unlock();
                    } else if (strncmp(action, "COMMSSTART", strlen("COMMSSTART")) == 0) {
                        g_comms_start = 1;
                        NOTIFY_CMD(IO_GUARD, IO_FOCUS_CALL_1);
                        NOTIFY_CMD(A01Controller, ALEXA_PLAY_STOP);
                        avs_player_force_stop(0);
                        switch_alert_to_background(1);
                        // focus_process_lock();
                        // switch_content_to_background(1);
                        // focus_process_unlock();
                    } else if (strncmp(action, "COMMSEND", strlen("COMMSEND")) == 0) {
                        g_comms_start = 0;
                        NOTIFY_CMD(IO_GUARD, IO_FOCUS_CALL_0);
                        focus_process_lock();
                        switch_content_to_background(0);
                        switch_alert_to_background(0);
                        focus_process_unlock();
                    }
                } else if (strncmp(recv_buf, "talkstart:", strlen("talkstart:")) == 0) {
                    wiimu_log(1, 0, 0, 0, "asr_ongoing(%d) privacy_mode(%d)\n",
                              g_wiimu_shm->asr_ongoing, g_wiimu_shm->privacy_mode);
                    if (g_wiimu_shm->bt_hfp_status) {
                        wiimu_log(1, 0, 0, 0, "hfp is in calling, ignore avs\n");
                        UnixSocketServer_close(&msg_socket);
                        continue;
                    }
                    /* If the volume is controlled by the MCU, adjust the volume by itself after
                     * receiving the AXX+WAK++UP command
                     * and do not display the light effect of adjusting the volume. If not, do not
                     * do any processing.
                     */
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=ASR_WAKEUP",
                                             strlen("GNOTIFY=ASR_WAKEUP"), NULL, NULL, 0);
#ifdef A98_FENGHUO
                    if ((PLAYER_MODE_UDISK == g_wiimu_shm->play_mode) ||
                        (PLAYER_MODE_TFCARD == g_wiimu_shm->play_mode)) {
                        char send_buf[32 + 1] = {0};
                        snprintf(send_buf, strlen("usb_or_tf_awaken:") + 1, "%s",
                                 "usb_or_tf_awaken:");
                        snprintf(send_buf + strlen("usb_or_tf_awaken:"), strlen("AXX+PLM-010") + 1,
                                 "%s", "AXX+PLM-010");
                        wiimu_log(1, 0, 0, 0, "%s send_buf:%s", __func__, send_buf);
                        SocketClientReadWriteMsg("/tmp/RequestIOGuard", send_buf, strlen(send_buf),
                                                 NULL, NULL, 0);
                    }
#endif
                    int req_from = atoi(recv_buf + strlen("talkstart:"));
#if defined(UPLOAD_PRERECORD)
                    if (req_from == 0) {
                        g_wiimu_shm->last_trigger_index = ASR_WWE_TRIG2;
                    } else if (req_from == 1) {
                        g_wiimu_shm->last_trigger_index = ASR_WWE_TRIG1;
                    }
#endif
                    CleanSilentOTAFlag(g_wiimu_shm);

                    /* stop alarm when talk key pressed */
                    if (g_wiimu_shm->alert_status) {
                        if (req_from == AWAKE_BY_BUTTON) {
#ifdef EN_AVS_COMMS
                            avs_comms_stop_ux();
#endif
                            killPidbyName("smplayer");
                            killPidbyName("ringtone_smplayer");
                            SocketClientReadWriteMsg(ALERT_SOCKET_NODE, "ALERT+LOCAL+DEL",
                                                     strlen("ALERT+LOCAL+DEL"), NULL, NULL, 0);
                            wiimu_log(1, 0, 0, 0,
                                      "push talk button to stop alerts, ASRTTS recv_buf------");
                            UnixSocketServer_close(&msg_socket);
                            continue;
                        }
                    }

                    if ((req_from == AWAKE_BY_WAKEWORD || req_from == 4) &&
                        g_wiimu_shm->asr_session_start == 1) {
                        wiimu_log(1, 0, 0, 0, "is already wakeuped/talk started \n");
                        UnixSocketServer_close(&msg_socket);
                        continue;
                    }

                    if (g_wiimu_shm->asr_ongoing == 0) {
                        if (!g_wiimu_shm->privacy_mode || req_from == 3) {
                            if (ignore_reset_trigger && req_from != 3) {
                                ignore_reset_trigger = 0;
                                wiimu_log(1, 0, 0, 0, "ignore_reset_trigger ==\n");
                            } else {
                                g_wiimu_shm->asr_ongoing = 1;
                                SetAudioInputProcessorObserverState(BUSY);
                                if ((req_from == 0 && tlk_start_cnt == 0) || (req_from != 0)) {
                                    alexa_dump_info.record_start_ts = 0;
                                    alexa_dump_info.record_end_ts = 0;
                                    alexa_dump_info.vad_ts = 0;
                                    alexa_dump_info.http_ret_code = 0;
                                    int ret = do_talkstart(req_from);
                                    if (0 == ret) {
                                        if (CLOSE_TALK != AlexaProfileType()) {
                                            tlk_start_cnt++;
                                        } else {
                                            tlk_start_cnt = 1;
                                        }
                                    }
                                } else if (req_from == 0 && tlk_start_cnt == 1) {
                                    if (!(g_wiimu_shm->asr_test_mode ||
                                          g_wiimu_shm->asr_trigger_test_on ||
                                          g_wiimu_shm->asr_trigger_mis_test_on))
                                        SocketClientReadWriteMsg(
                                            "/tmp/AMAZON_ASRTTS", "amazonRecognizeStop",
                                            strlen("amazonRecognizeStop"), NULL, NULL, 0);
                                    tlk_start_cnt = 0;
                                }
                            }
                        }
                    }
                } else if (strncmp(recv_buf, "talkstop", strlen("talkstop")) == 0) {
#ifdef A98_FENGHUO
                    wiimu_log(1, 0, 0, 0, "%s mode:%d, need_keep_wifi_mode:%d", __func__,
                              g_wiimu_shm->play_mode, g_wiimu_shm->need_keep_wifi_mode);
                    char send_buf[32 + 1] = {0};
                    snprintf(send_buf, strlen("usb_or_tf_awaken:") + 1, "%s", "usb_or_tf_awaken:");
                    if ((PLAYER_MODE_UDISK == g_wiimu_shm->play_mode) ||
                        (PLAYER_MODE_TFCARD == g_wiimu_shm->play_mode)) {
                        if (1 == g_wiimu_shm->need_keep_wifi_mode) { // keep wifi mode
                            wiimu_log(1, 0, 0, 0, "%s keep wifi mode", __func__);
                            snprintf(send_buf + strlen("usb_or_tf_awaken:"),
                                     strlen("AXX+PLM+010") + 1, "%s", "AXX+PLM+010");
                            SocketClientReadWriteMsg("/tmp/RequestIOGuard", send_buf,
                                                     strlen(send_buf), NULL, NULL, 0);
                        } else { // 2、back mode
                            wiimu_log(1, 0, 0, 0, "%s back mode play_mode:%d", __func__,
                                      g_wiimu_shm->play_mode);
                            if (PLAYER_MODE_UDISK == g_wiimu_shm->play_mode) {
                                snprintf(send_buf + strlen("usb_or_tf_awaken:"),
                                         strlen("AXX+PLM+042") + 1, "%s", "AXX+PLM+042");
                                SocketClientReadWriteMsg("/tmp/RequestIOGuard", send_buf,
                                                         strlen(send_buf), NULL, NULL, 0);
                            }
                            if (PLAYER_MODE_TFCARD == g_wiimu_shm->play_mode) {
                                snprintf(send_buf + strlen("usb_or_tf_awaken:"),
                                         strlen("AXX+PLM+052") + 1, "%s", "AXX+PLM+052");
                                SocketClientReadWriteMsg("/tmp/RequestIOGuard", send_buf,
                                                         strlen(send_buf), NULL, NULL, 0);
                            }
                        }
                    }
#endif
                    int req_from = 0;
                    if (strstr(recv_buf, "talkstop:"))
                        req_from = atoi(recv_buf + strlen("talkstop:"));
                    do_talkstop(req_from, req_from);
                    if (tlk_start_cnt > 0) {
                        tlk_start_cnt = 0;
                    }
                } else if (strncmp(recv_buf, "talkcontinue:", strlen("talkcontinue:")) == 0) {
                    int errCode = atoi(recv_buf + strlen("talkcontinue:"));
                    int ret = do_talkcontinue(errCode, 1);
                    if (0 == ret) {
                        if (CLOSE_TALK != AlexaProfileType()) {
                            tlk_start_cnt++;
                        } else {
                            tlk_start_cnt = 1;
                        }
                    }
                } else if (strncmp(recv_buf, "handsfree_disable", strlen("handsfree_disable")) ==
                           0) {
                    g_wiimu_shm->asr_handsfree_disabled = 1;
                    block_voice_prompt("common/mic_off.mp3", 0);
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=handsfree_disable",
                                             strlen("GNOTIFY=handsfree_disable"), NULL, NULL, 0);
                    asr_config_set(NVRAM_ASR_HANDSFREE_DISABLED, "1");
                    asr_config_sync();

                } else if (strncmp(recv_buf, "handsfree_enable", strlen("handsfree_enable")) == 0) {
                    g_wiimu_shm->asr_handsfree_disabled = 0;
                    block_voice_prompt("common/mic_on.mp3", 0);
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=handsfree_enable",
                                             strlen("GNOTIFY=handsfree_enable"), NULL, NULL, 0);
                    asr_config_set(NVRAM_ASR_HANDSFREE_DISABLED, "0");
                    asr_config_sync();
                } else if (strncmp(recv_buf, "privacy_enable", strlen("privacy_enable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOn", strlen("talksetPrivacyOn")) == 0) {

                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                             strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
#ifdef EN_AVS_COMMS
                    avs_comms_notify("call_mute_mic:1");
#endif
                    if (!g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 1;
                    alexa_update_action_time();

#ifdef PURE_RPC /* able to control whether or not the audio cue is played */
                    if (0 ==
                        (strncmp(recv_buf + strlen("privacy_enable") + 1, "true",
                                 strlen("true")))) { // privacy_enable:true   privacy_enable:false
                        block_voice_prompt(
                            "common/mic_off.mp3",
                            0); // pure control control audio cue is played, else not play
                    }
#endif /* endif PURE_RPC */
#if defined(A98_EATON)
                    block_voice_prompt("common/mic_off.mp3", 0);
#endif
                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "1");
                    asr_config_sync();

                } else if (strncmp(recv_buf, "privacy_disable", strlen("privacy_disable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOff", strlen("talksetPrivacyOff")) ==
                               0) {
#ifdef EN_AVS_COMMS
                    avs_comms_notify("call_mute_mic:0");
#endif
                    if (g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 0;
                    alexa_update_action_time();
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                             strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);

#ifdef PURE_RPC /* able to control whether or not the audio cue is played */
                    if (0 == (strncmp(recv_buf + strlen("privacy_disable") + 1, "true",
                                      strlen("true")))) { // privacy_disable:true
                        block_voice_prompt(
                            "common/mic_on.mp3",
                            0); // pure control control audio cue is played, else not play
                    }
#endif
#if defined(A98_EATON)
                    block_voice_prompt("common/mic_on.mp3", 0);
#endif

                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "0");
                    asr_config_sync();
#if defined(HARMAN) || defined(PURE_RPC)
                    AudioCapReset(1);
#endif
                } else if (strncmp(recv_buf, "INTERNET_ACCESS:", strlen("INTERNET_ACCESS:")) == 0) {
                    g_internet_access = atoi(recv_buf + strlen("INTERNET_ACCESS:"));
#ifdef EN_AVS_COMMS
                    avs_comms_state_change(e_state_internet, g_internet_access == 0 ? e_off : e_on);
#endif
                    g_wiimu_shm->asr_ongoing = 0;
#if 0
                    if(!g_record_testing) {
                        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "CAP_RESET", strlen("CAP_RESET"), NULL, NULL, 0);
                    }
#endif
                } else if (strncmp(recv_buf, "NTPSYNC", strlen("NTPSYNC")) == 0) {
                    g_ntp_synced = 1;
                } else if (strncmp(recv_buf, "USER_REQUEST:", strlen("USER_REQUEST:")) == 0) {
                    int asr_disable = atoi(recv_buf + strlen("USER_REQUEST:"));
                    g_asr_disable = asr_disable;
                    if (!g_record_testing) {
                        AudioCapReset(g_asr_disable);
                    }
                } else if (strncmp(recv_buf, "VAD_FINISH", strlen("VAD_FINISH")) == 0) {
                    g_wiimu_shm->asr_session_start = 0;
                    if (tlk_start_cnt == 1) {
                        asr_play_stop_prompt(1);
                        tlk_start_cnt = 0;
                    } else {
                        tlk_start_cnt--;
                        tlk_start_cnt = tlk_start_cnt > 0 ? tlk_start_cnt : 0;
                    }
                    CAIMan_YD_enable();
                    if (g_wiimu_shm->asr_test_mode) {
                        pcm_playback(ALEXA_RECORD_FILE);
                        g_wiimu_shm->asr_last_status = 0;
                        g_wiimu_shm->asr_last_talkon = g_wiimu_shm->times_talk_on;
                        g_wiimu_shm->asr_ongoing = 0;
                    }
                } else if (strncmp(recv_buf, "VAD_TIMEOUT", strlen("VAD_TIMEOUT")) == 0) {
                    g_wiimu_shm->asr_session_start = 0;
                    if (tlk_start_cnt == 1) {
                        asr_play_stop_prompt(0);
                        tlk_start_cnt = 0;
                    } else {
                        tlk_start_cnt--;
                        tlk_start_cnt = tlk_start_cnt > 0 ? tlk_start_cnt : 0;
                    }
                    CAIMan_YD_enable();
                    if (g_wiimu_shm->asr_test_mode) {
                        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDRECOERROR",
                                                 strlen("GNOTIFY=MCULEDRECOERROR"), NULL, NULL, 0);
                        g_wiimu_shm->smplay_skip_silence = 1;
                        volume_pormpt_direct("common/ring2.mp3", 1, MIN_START_PROMPT_VOLUME);
                        pcm_playback(ALEXA_RECORD_FILE);
                        g_wiimu_shm->asr_last_status = -1;
                        g_wiimu_shm->asr_last_talkon = g_wiimu_shm->times_talk_on;
                        g_wiimu_shm->asr_ongoing = 0;
                    }
                } else if (strncmp(recv_buf, "talkTriggerTestStart",
                                   strlen("talkTriggerTestStart")) == 0) {
                    g_wiimu_shm->asr_trigger_test_on = 1;
                    g_wiimu_shm->asr_trigger_mis_test_on = 0;
                    g_wiimu_shm->asr_yd_recording = 1;
                } else if (strncmp(recv_buf, "talkTriggerTestStop",
                                   strlen("talkTriggerTestStop")) == 0) {
                    g_wiimu_shm->asr_trigger_test_on = 0;
                } else if (strncmp(recv_buf, "talkTriggerMisTestStart",
                                   strlen("talkTriggerMisTestStart")) == 0) {
                    g_wiimu_shm->asr_trigger_test_on = 0;
                    g_wiimu_shm->asr_trigger_mis_test_on = 1;
                    g_wiimu_shm->asr_yd_recording = 1;
                } else if (strncmp(recv_buf, "talkTriggerMisTestStop",
                                   strlen("talkTriggerMisTestStop")) == 0) {
                    g_wiimu_shm->asr_trigger_mis_test_on = 0;
                } else if (strncmp(recv_buf, "talksetTriggerScore",
                                   strlen("talkSetTriggerScore")) == 0) {
                    int flag = atoi(recv_buf + strlen("talkSetTriggerScore:"));
                    if (flag > 100 && flag < 1000)
                        g_wiimu_shm->asr_yd_config_score = flag;
                } else if (strncmp(recv_buf, "talksetTriggerInterval",
                                   strlen("talkSetTriggerInterval")) == 0) {
                    int flag = atoi(recv_buf + strlen("talkSetTriggerInterval:"));
                    if (flag > 20 && flag < 180)
                        g_wiimu_shm->asr_yd_test_interval = flag;
                } else if (strncmp(recv_buf, "talksetDisableMic", strlen("talksetDisableMic")) ==
                           0) {
                    g_wiimu_shm->asr_mic_needless = 1;
                    AudioCapReset(0);
                } else if (strncmp(recv_buf, "talksetEnableMic", strlen("talksetEnableMic")) == 0) {
                    g_wiimu_shm->asr_mic_needless = 0;
                    AudioCapReset(1);
                } else if (strncmp(recv_buf, "talksetSearchId:", strlen("talksetSearchId:")) == 0) {
                    char *p = recv_buf + strlen("talksetSearchId:");
                    int val = atoi(p);
                    if (strlen(p) > 0 && val >= 1 && val <= 17) {
                        asr_config_set(NVRAM_ASR_SEARCH_ID, p);
                        asr_config_sync();
                        g_wiimu_shm->asr_search_id = val;
                        AudioCapReset(1);
                    }
                } else if (strncmp(recv_buf, "talksetMicDistance:",
                                   strlen("talksetMicDistance:")) == 0) {
                    char *p = recv_buf + strlen("talksetMicDistance:");
                    int val = atoi(p);
                    asr_config_set(NVRAM_ASR_MIC_DISTANCE, p);
                    asr_config_sync();
                    cxdish_set_mic_distance(val);
                } else if (strncmp(recv_buf, "talksetChannelAsr:", strlen("talksetChannelAsr:")) ==
                           0) {
                    char *p = recv_buf + strlen("talksetChannelAsr:");
                    int val = atoi(p);
                    asr_config_set(NVRAM_ASR_CH_ASR, p);
                    asr_config_sync();
                    g_wiimu_shm->asr_ch_asr = val;
                } else if (strncmp(recv_buf, "talksetChannelTrigger:",
                                   strlen("talksetChannelTrigger:")) == 0) {
                    char *p = recv_buf + strlen("talksetChannelTrigger:");
                    int val = atoi(p);
                    asr_config_set(NVRAM_ASR_CH_TRIGGER, p);
                    asr_config_sync();
                    g_wiimu_shm->asr_ch_trigger = val;
                } else if (strncmp(recv_buf, "talksetNoiseGate:", strlen("talksetNoiseGate:")) ==
                           0) {
                    char *p = recv_buf + strlen("talksetNoiseGate:");
                    int val = atoi(p);
                    asr_config_set(NVRAM_ASR_NOISE_GATE, p);
                    asr_config_sync();
                    cxdish_set_noise_gating(val);
                }
                // talksetPrompt: 1 (play prompt), others (do not play prompt)
                else if (strncmp(recv_buf, "talksetPrompt:", strlen("talksetPrompt:")) == 0) {
                    char *p = recv_buf + strlen("talksetPrompt:");
                    int val = atoi(p);
                    if (strlen(p) > 0) {
                        asr_config_set(NVRAM_ASR_HAVE_PROMPT, p);
                        asr_config_sync();
                        g_wiimu_shm->asr_have_prompt = (val == 1) ? 1 : 0;
                    }
                } else if (strncmp(recv_buf, "talksetAngle:", strlen("talksetAngle:")) == 0) {
                    char *p = recv_buf + strlen("talksetAngle:");
                    int val = atoi(p);
                    if (strlen(p) > 0 && val >= 1 && val < 360) {
                        asr_config_set(NVRAM_ASR_MIC_ANGLE, p);
                        asr_config_sync();
                        g_wiimu_shm->asr_mic_angle_base = val;
                    }
                }
                /* talksetMicBoost:N:val*/
                else if (strncmp(recv_buf, "talksetMicBoost:", strlen("talksetMicBoost:")) == 0 ||
                         strncmp(recv_buf, "talksetDmicGain:", strlen("talksetDmicGain:")) == 0) {
                    char *p = strstr(recv_buf, ":");
                    int step = -1;
                    int val = -1;
                    char *pVal = NULL;
                    char nvram_micboost[32];
                    if (p && strlen(p) > 1) {
                        pVal = strstr(p + 1, ":");
                        *p = 0;
                        step = atoi(p + 1);
                        if (pVal)
                            val = atoi(pVal + 1);
                    }
                    if (step >= 0 && step < ASR_MIC_BOOST_STEPS && val >= 0 &&
                        val <= ASR_MIC_BOOST_MAX) {
                        memset((void *)nvram_micboost, 0, sizeof(nvram_micboost));
                        snprintf(nvram_micboost, sizeof(nvram_micboost), "%s%d",
                                 NVRAM_ASR_MIC_BOOST, step);
                        asr_config_set(nvram_micboost, pVal + 1);
                        asr_config_sync();
                        g_wiimu_shm->asr_mic_boost[step] = val;
                        default_mic_boost[step] = val;
                    }
                }
                /* talksetMicBoost:val*/
                else if (strncmp(recv_buf, "talksetMicBoostAdjust:",
                                 strlen("talksetMicBoostAdjust:")) == 0 ||
                         strncmp(recv_buf, "talksetDmicGainAdjust:",
                                 strlen("talksetDmicGainAdjust:")) == 0) {
                    char *p = strstr(recv_buf, ":");
                    int val = 0;
                    if (p && strlen(p) > 1)
                        val = atoi(p + 1);

                    if (val)
                        update_mic_boost_config(val);
                }
                /* talksetAdcGain:N:val*/
                else if (strncmp(recv_buf, "talksetAdcGain:", strlen("talksetAdcGain:")) == 0) {
                    char *p = strstr(recv_buf, ":");
                    int id = -1;
                    int val = -1;
                    if (p && strlen(p) > 1) {
                        char *pVal = strstr(p + 1, ":");
                        *p = 0;
                        id = atoi(p + 1);
                        if (pVal)
                            val = atoi(pVal + 1);
                    }

                    if (id >= 0 && id <= 2 && val >= 0 && val <= 0x2b8) {
                        cxdish_set_adc_gain(id, val);
                    }
                }
                /* talksetAdcGainStep:N:val*/
                else if (strncmp(recv_buf, "talksetAdcGainStep:", strlen("talksetAdcGainStep:")) ==
                         0) {
                    char *p = strstr(recv_buf, ":");
                    int id = -1;
                    int val = -1;
                    if (p && strlen(p) > 1) {
                        char *pVal = strstr(p + 1, ":");
                        *p = 0;
                        id = atoi(p + 1);
                        if (pVal)
                            val = atoi(pVal + 1);
                    }

                    if (id >= 0 && id <= 2 && val >= 0 && val <= 0x2b8) {
                        cxdish_set_adc_gain_step(id, val);
                    }
                }
                /* talksetDrcGainFunc:high_input:high_output:low_input:low_output:k_high:k_middle:k_low
                   */
                else if (strncmp(recv_buf, "talksetDrcGainFunc:", strlen("talksetDrcGainFunc:")) ==
                         0) {
                    int high_input, high_output, low_input, low_output, k_high, k_middle, k_low;
                    int ret = 0;
                    ret = sscanf(recv_buf + strlen("talksetDrcGainFunc:"), "%d:%d:%d:%d:%d:%d:%d",
                                 &high_input, &high_output, &low_input, &low_output, &k_high,
                                 &k_middle, &k_low);
                    printf("ret=%d, hi=%d ho=%d li=%d lo=%d kh=%d km=%d kl=%d\n", ret, high_input,
                           high_output, low_input, low_output, k_high, k_middle, k_low);
                    cxdish_set_drc_gain_func(high_input, high_output, low_input, low_output, k_high,
                                             k_middle, k_low);
                }
                /* talksetDSPStatusGet*/
                /*SSP,FDNLP,DRC, AEC,AGC*/
                else if (strncmp(recv_buf, "talksetDSPSerialGet", strlen("talksetDSPSerialGet")) ==
                         0) {
                    dsp_serial_get(recv_buf);
                    UnixSocketServer_write(&msg_socket, recv_buf, strlen(recv_buf));
                } else if (strncmp(recv_buf, "talksetTestFlag", strlen("talksetTestFlag")) == 0) {
                    int flag = atoi(recv_buf + strlen("talksetTestFlag:"));
                    // set to test mode
                    if (flag < 0) {
                        if ((flag == -1) || (flag == -4) || (flag == -5)) {
                            alexa_dump_info.asr_test_flag |= (1 << (-flag));
                        }
                    } else {
                        if (flag & (ASR_TEST_FlAG_I2S_REFRENCE | ASR_TEST_FlAG_MIC_BYPASS)) {
                            if (ASR_IS_I2S_REFRENCE(flag)) {
                                cxdish_mic_channel_set(3, NULL, NULL);
                                flag |= ASR_TEST_FLAG_NOT_PAUSE;
                                // Set output to Left-Ref and Right-Ref output
                                cxdish_output_to_ref();
                                // system("cxdish sendcmd 0xD308632C 0x00000040 0x000002c0 000002c1
                                // 00000000 00000000 00000000 00000000 00000000 00000000");
                            } else {
                                flag &= ~ASR_TEST_FLAG_NOT_PAUSE;
                                cxdish_mic_channel_set(3, NULL, NULL);
                                // Set output to Left-Mic and Right-Mic output
                                cxdish_output_to_mic();
                                // system("cxdish sendcmd 0xD308632C 0x00000040 0x000001c0 000001c1
                                // 00000000 00000000 00000000 00000000 00000000 00000000");
                            }
                        }
                        // reset to normal mode
                        else if (alexa_dump_info.asr_test_flag &
                                 (ASR_TEST_FlAG_I2S_REFRENCE | ASR_TEST_FlAG_MIC_BYPASS)) {
                            cxdish_mic_channel_set(-1, NULL, NULL);
                        }

                        alexa_dump_info.asr_test_flag = flag;
                    }
                } else if (strncmp(recv_buf, "recordTest", strlen("recordTest")) == 0) {
                    int flag = atoi(recv_buf + strlen("recordTest:"));
                    g_record_testing = flag;
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:alexa:stop",
                                             strlen("setPlayerCmd:alexa:stop"), NULL, NULL, 0);
                    killPidbyName("smplayer");
                    CAIMan_init();
                    if (flag) {
                        g_wiimu_shm->asr_ongoing = 1;
                        StartRecordTest();
                    } else {
                        StopRecordTest();
                        g_wiimu_shm->asr_ongoing = 0;
                        CAIMan_YD_enable();
                    }
                }
//#if defined(RECORD_MODULE_ENABLE)
#if defined(SOFT_PREP_HOBOT_MODULE)
                else if (strncmp(recv_buf, "hobotStartCAP:",
                                 strlen("hobotStartCAP:")) ==
                         0) { // hobotStartCAP:0:path    0:44.1K source  1:processed
                    char *tmp = NULL;
                    char *path = NULL;
                    int source = atoi((recv_buf + strlen("hobotStartCAP:")));
                    printf("source = %d\n", source);
                    tmp = recv_buf + strlen("hobotStartCAP:");
                    path = strstr(tmp, ":");
                    CAIMan_soft_prep_privRecord(
                        1, source,
                        path + 1); // status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
                    UnixSocketServer_write(&msg_socket, "OK", 2);
                } else if (strncmp(recv_buf, "hobotStopCAP", strlen("hobotStopCAP")) == 0) {
                    CAIMan_soft_prep_privRecord(
                        0, 0,
                        NULL); // status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
                    UnixSocketServer_write(&msg_socket, "OK", 2);
                }
#endif
                else if (strncmp(recv_buf, "recordFileStart:", strlen("recordFileStart:")) == 0) {
                    char *fileName = (recv_buf + strlen("recordFileStart:"));
                    g_record_testing = 1;

                    if (!IsCCaptStart()) {
                        CAIMan_init();
                    }
                    CCapt_Start_GeneralFile(fileName);
                } else if (strncmp(recv_buf, "recordFileStop", strlen("recordFileStop")) == 0) {
                    char buf[100];
                    g_record_testing = 0;
                    CCapt_Stop_GeneralFile(buf);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                    CAIMan_YD_enable();
                } else if (strncmp(recv_buf, "getAvsDevInfo", strlen("getAvsDevInfo")) == 0) {
                    web_getLifepodProfile(&msg_socket, 1);
                } else if (strncmp(recv_buf, "unauthorize_res_exception",
                                   strlen("unauthorize_res_exception")) == 0) {
                    printf("alexa receive++++\n");
                    unauthorize_action();
                }
                //#endif
                else if (strncmp(recv_buf, "talkdump:0", strlen("talkdump:0")) == 0) {
                    pcm_playback(ALEXA_RECORD_FILE);
                } else if (strncmp(recv_buf, "talkdump:1", strlen("talkdump:1")) == 0) {
                    pcm_playback(ALEXA_UPLOAD_FILE);
                } else if (strncmp(recv_buf, "mic_channel",
                                   strlen("mic_channel")) == 0) { // 0:ALL channel mute 1:left
                                                                  // channel on 2:right channel on
                                                                  // 3:ALL channel on
                    int vad_flag = atoi(recv_buf + strlen("mic_channel:"));
                    cxdish_mic_channel_set(vad_flag, &msg_socket, recv_buf);
                } else if (strncmp(recv_buf, "alexadump", strlen("alexadump")) == 0) {
                    web_dump_refresh(&msg_socket, recv_buf);
                }
#if defined(IHOME_ALEXA)
                else if (strncmp(recv_buf, "alertget", strlen("alertget")) == 0) {
                    alert_get_for_app(&msg_socket, recv_buf);
                    UnixSocketServer_write(&msg_socket, recv_buf, strlen(recv_buf));

                } else if (strncmp(recv_buf, "talksetAlarm", strlen("talksetAlarm")) == 0) {
                    alert_common_for_app(recv_buf);
                }
#endif
                else if (strncmp(recv_buf, "vad_test", strlen("vad_test")) == 0) {
                    int vad_flag = atoi(recv_buf + strlen("vad_test:"));
                    memset(recv_buf, 0, MAX_SOCKET_RECV_BUF_LEN);
                    snprintf(recv_buf, MAX_SOCKET_RECV_BUF_LEN, "%s", "alexadump");
                    if (1 == vad_flag) {
                        if (!g_wiimu_shm->asr_test_mode) {
                            g_wiimu_shm->asr_test_mode = 1;
                            web_dump_refresh(&msg_socket, recv_buf);
                        } else {
                            continue;
                        }
                    } else if (0 == vad_flag) {
                        if (g_wiimu_shm->asr_test_mode) {
                            g_wiimu_shm->asr_test_mode = 0;
                            web_dump_refresh(&msg_socket, recv_buf);
                        } else {
                            continue;
                        }
                    }

                    sleep(1);
                    system("killall asr_tts"); // retart app
                } else if (strncmp(recv_buf, "change_record_path", strlen("change_record_path")) ==
                           0) {
                    int change_flag = atoi(recv_buf + strlen("change_record_path:"));
                    memset(recv_buf, 0, MAX_SOCKET_RECV_BUF_LEN);
                    if (1 == change_flag) {
                        if (strlen(g_wiimu_shm->mmc_path) > 0) {
                            if (!alexa_dump_info.record_to_sd) {
                                alexa_dump_info.record_to_sd = 1;
                            }
                            strcat(recv_buf, "record to SD card!");
                        } else {
                            strcat(recv_buf, "Error: none sd card found");
                        }
                    } else {
                        if (alexa_dump_info.record_to_sd) {
                            alexa_dump_info.record_to_sd = 0;
                        }
                        strcat(recv_buf, "record to tmp!");
                    }
                    UnixSocketServer_write(&msg_socket, recv_buf, strlen(recv_buf));
                } else if (strncmp(recv_buf, "smokeTest:", strlen("smokeTest:")) == 0) {
                    if (strlen(recv_buf + strlen("smokeTest:")) > 0) {
                        int delay_time = atoi(recv_buf + strlen("smokeTest:"));
                        if (delay_time <= 0) {
                            killPidbyName("smoke_test");
                        } else {
                            char cmd_buf[128] = {0};
                            snprintf(cmd_buf, sizeof(cmd_buf), "pkill smoke_test; smoke_test %d &",
                                     delay_time);
                            system(cmd_buf);
                        }
                        UnixSocketServer_write(&msg_socket, "Ok", strlen("Ok"));
                    } else {
                        UnixSocketServer_write(&msg_socket, "Failed", strlen("Failed"));
                    }
                }
#ifdef ENABLE_RECORD_SERVER
                else if (strncmp(recv_buf, "startRecordServer", strlen("startRecordServer")) == 0) {
                    int nChannel = 1;
                    char *pChannel = strstr(recv_buf, "startRecordServer:");

                    if (pChannel != NULL) {
                        pChannel += strlen("startRecordServer:");
                        nChannel = atoi(pChannel);
                    }

                    StartRecordServer(nChannel);
                } else if (strncmp(recv_buf, "stopRecordServer", strlen("stopRecordServer")) == 0) {
                    StopRecordServer();
                }
#endif
#ifdef PURE_RPC
                else if (strncmp(recv_buf, "GNOTIFY=ENABLE_MIC_CAPTURE",
                                 strlen("GNOTIFY=ENABLE_MIC_CAPTURE")) == 0) {
                    mic_capture_enable();
                } else if (strncmp(recv_buf, "GNOTIFY=DISABLE_MIC_CAPTURE",
                                   strlen("GNOTIFY=DISABLE_MIC_CAPTURE")) == 0) {
                    mic_capture_disable();
                }
#endif
#ifdef ASR_TALK_VIA_FILE
                else if (strncmp(recv_buf, "talksetAudioFromFile:",
                                 strlen("talksetAudioFromFile:")) == 0) {
                    char *path = (recv_buf + strlen("talksetAudioFromFile:"));
                    if (file_record_start(path)) {
                        UnixSocketServer_write(&msg_socket, "Failed", strlen("Failed"));
                    } else {
                        UnixSocketServer_write(&msg_socket, "Ok", strlen("Ok"));
                    }
                }
#endif
                else if (strncmp(recv_buf, "NetworkDHCPNewIP", strlen("NetworkDHCPNewIP")) == 0) {
#ifdef AMLOGIC_A113
                    res_init();
#endif
                }
#ifdef BT_HFP_ENABLE
                else if (strncmp(recv_buf, "bluetooth_hfp_start_play",
                                 strlen("bluetooth_hfp_start_play")) == 0) {
                    bluetooth_hfp_start_play();
                } else if (strncmp(recv_buf, "bluetooth_hfp_stop_play",
                                   strlen("bluetooth_hfp_stop_play")) == 0) {
                    bluetooth_hfp_stop_play();
                }
#endif
                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf------");
            }

            UnixSocketServer_close(&msg_socket);
        }
    }
}

/*
* return 0 for ok,  others for fail reason
*/
static ASR_ERROR_CODE do_talkstart_check(int req_from)
{
    if (g_wiimu_shm->asr_test_mode || g_wiimu_shm->asr_trigger_test_on ||
        g_wiimu_shm->asr_trigger_mis_test_on)
        return 0;

    // if(g_wiimu_shm->slave_notified) {
    //    return ERR_ASR_DEVICE_IS_SLAVE;
    //}

    if (g_asr_disable || g_wiimu_shm->privacy_mode) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if ((g_wiimu_shm->internet == 0 && g_internet_access == 0) ||
        (g_wiimu_shm->netstat != NET_CONN && g_wiimu_shm->Ethernet == 0)) {
        return ERR_ASR_NO_INTERNET;
    }

    if ((access(LIFEPOD_TOKEN_PATH, 0)) != 0) { // if access_token not exist
#if 0
        if((access("/vendor/wiimu/alexa_alarm", 0)) == 0) { //if access_token not exist
            system("rm -rf /vendor/wiimu/alexa_alarm");
            killPidbyName("alexa_alert");
            system("alexa_alert &");
        }
#endif
        return ERR_ASR_NO_USER_ACCOUNT;
    }

    if (!LifepodCheckLogin()) {
        return ERR_ASR_USER_LOGIN_FAIL;
    }

    if (alexa_dump_info.on_line == 0) {
        return ERR_ASR_HAVE_PROMBLEM;
    }

    return 0;
}

static void do_talkstart_play_voice(int req_from)
{
    char *tlk_prompt = NULL;
    if (CLOSE_TALK != AlexaProfileType() && (g_wiimu_shm->asr_have_prompt == 0)) {
        return;
    }
    g_wiimu_shm->smplay_skip_silence = 1; // cancle the silence

#if defined(SOFT_PREP_HOBOT_MODULE)
    if (g_wiimu_shm->asr_have_prompt) {
#endif
        // For far feild wakeup prompt
        if (CLOSE_TALK != AlexaProfileType()) {
#if defined(A98_SALESFORCE)
            if (g_wiimu_shm->last_trigger_index == ASR_WWE_TRIG2)
                tlk_prompt = "common/alexa_start_2.mp3";
            else
                tlk_prompt = "common/alexa_start.mp3";
#else
        tlk_prompt = "common/alexa_start.mp3";
#endif
        } else {
            tlk_prompt = "common/talk_start_t.mp3";
        }

        block_voice_prompt(tlk_prompt, 0);
#if defined(SOFT_PREP_HOBOT_MODULE)
    }
#endif
}

int read_to_buf(const char *filename, void *buf)
{
    int fd;
    /* open_read_close() would do two reads, checking for EOF.
     * When you have 10000 /proc/$NUM/stat to read, it isn't desirable */
    ssize_t ret = -1;
    fd = open(filename, O_RDONLY);
    if (fd >= 0) {
        ret = read(fd, buf, PROCPS_BUFSIZE - 1);
        close(fd);
    }
    ((char *)buf)[ret > 0 ? ret : 0] = '\0';
    return ret;
}

static int do_talkstart_prepare(int req_from)
{
    char *buf = NULL;

    g_wiimu_shm->asr_ongoing = 1;
    SetAudioInputProcessorObserverState(BUSY);
    g_alexa_req_from = req_from;
    alexa_dump_info.record_start_ts = tickSincePowerOn();
    time(&alexa_dump_info.last_dialog_ts);

#ifdef EN_AVS_COMMS
    avs_comms_stop_ux();
#endif
    killPidbyName("smplayer");
    killPidbyName("ringtone_smplayer");

    if (req_from != 3) {
        CAIMan_ASR_disable();
        AlexaInitRecordData();
    }
    avs_system_set_expect_speech_timed_out(1);

    asprintf(&buf, "amazonRecognizeStart:%d", req_from);
    if (buf) {
        if (!(g_wiimu_shm->asr_test_mode || g_wiimu_shm->asr_trigger_test_on ||
              g_wiimu_shm->asr_trigger_mis_test_on))
            SocketClientReadWriteMsg("/tmp/AMAZON_ASRTTS", buf, strlen(buf), NULL, NULL, 0);
        // g_wiimu_shm->asr_ongoing = 0;
        if (req_from != 2) {
            ASR_SetRecording_TimeOut(MAX_RECORD_TIMEOUT_MS);
        }
        free(buf);
        buf = NULL;
    }
    return 0;
}

static int do_talkstart(int req_from)
{
    int ret;
    g_wiimu_shm->asr_session_start = 1;
    g_wiimu_shm->times_talk_on++;
    g_TTS_aborted = 0;
    g_wiimu_shm->TTSPlayStatus = 0;
    wiimu_log(1, 0, 0, 0,
              "do_talkstart talk_count %d req_from=%d , trigger_test_on=%d mis_test_on=%d",
              g_wiimu_shm->times_talk_on, req_from, g_wiimu_shm->asr_trigger_test_on,
              g_wiimu_shm->asr_trigger_mis_test_on);
// CAIMan_YD_disable();
#ifdef EN_AVS_COMMS
    avs_comms_stop_ux();
#endif

    asr_set_audio_pause(1);

    ret = do_talkstart_check(req_from);
    alexa_dump_info.login = ret;
    if (ret) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDRECOERROR",
                                 strlen("GNOTIFY=MCULEDRECOERROR"), NULL, NULL, 0);
        do_talkstop(ret, req_from);
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDIDLE",
                                 strlen("GNOTIFY=MCULEDIDLE"), NULL, NULL, 0);
        SetAudioInputProcessorObserverState(IDLE);
        return -1;
    }
    if (g_wiimu_shm->asr_trigger_test_on || g_wiimu_shm->asr_trigger_mis_test_on) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDRECOERROR",
                                 strlen("GNOTIFY=MCULEDRECOERROR"), NULL, NULL, 0);
        block_voice_prompt("common/alexa_start.mp3", 1);
        do_talkstop(-1, req_from);
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDIDLE",
                                 strlen("GNOTIFY=MCULEDIDLE"), NULL, NULL, 0);
        SetAudioInputProcessorObserverState(IDLE);
        return -1;
    }

    g_alexa_req_from = req_from;
    do_talkstart_prepare(req_from);
    // cxdish_disable_ssp();//Disable SSP

    if (req_from == 1 || req_from == 4)
        CCaptWakeUpFlushState();
    // if(req_from != 3) {
    do_talkstart_play_voice(req_from);
    //}

    if (req_from != 3 && g_wiimu_shm->asr_mic_needless == 0) {
        if (!IsCCaptStart()) {
            printf("===== %s capture stopped, restart  =====\n", __FUNCTION__);
            CAIMan_deinit();
            CAIMan_ASR_stop();

            CAIMan_init();
            CAIMan_YD_start();
            CAIMan_ASR_start();
            CAIMan_YD_enable();
        }
    }
    CAIMan_ASR_enable(req_from);
    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=TLKLEDON",strlen("GNOTIFY=TLKLEDON"),NULL,NULL,0);
    return 0;
}

static int do_talkstop(int errcode, int req_from)
{
    if (g_record_testing) {
        g_wiimu_shm->asr_ongoing = 0;
        asr_set_audio_resume();
        return 0;
    }

    wiimu_log(1, 0, 0, 0, "do_talkstop ++++");
    if (errcode)
        printf("=====%s errcode=%d slave_notified=%d g_asr_disable=%d lan=%s\n", __FUNCTION__,
               errcode, g_wiimu_shm->slave_notified, g_asr_disable, g_wiimu_shm->language);

    if (g_asr_disable) {
        char *path = "common/alexa_slave_trouble.mp3";
        if (!g_asr_disable) {
            g_wiimu_shm->asr_ongoing = 1;
            if (volume_prompt_exist("us/alexa_slave_trouble.mp3"))
                path = "us/alexa_slave_trouble.mp3";
            block_voice_prompt(path, 1);
        }
        g_wiimu_shm->asr_ongoing = 0;
        g_wiimu_shm->asr_session_start = 0;
        asr_set_audio_resume();
        wiimu_log(1, 0, 0, 0, "do_talkstop ----");
        return 0;
    }
    g_wiimu_shm->asr_ongoing = 1;
    CAIMan_ASR_disable();

    play_error_voice(req_from, errcode);

    if (!g_wiimu_shm->asr_test_mode) {
        asr_set_audio_resume();
    }
    CAIMan_YD_enable();
    g_wiimu_shm->asr_ongoing = 0;
    g_wiimu_shm->asr_session_start = 0;
    if (g_wiimu_shm->privacy_mode) {
        if (g_wiimu_shm->asr_pause_request) {
            g_wiimu_shm->asr_pause_request = 0;
        }
        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
        SetAudioInputProcessorObserverState(IDLE);
    }
    wiimu_log(1, 0, 0, 0, "do_talkstop ----");
    return 0;
}

static int do_talkcontinue(int errCode, int req_from)
{
    int ret = -1;
    if (g_record_testing) {
        g_wiimu_shm->asr_ongoing = 0;
        // resume audio asap for test mode
        asr_set_audio_resume();
        return -1;
    }

    g_TTS_aborted = 0;
    g_wiimu_shm->TTSPlayStatus = 0;
    if (g_wiimu_shm->privacy_mode) {
        if (g_wiimu_shm->asr_pause_request) {
            g_wiimu_shm->asr_pause_request = 0;
        }
        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
        SetAudioInputProcessorObserverState(IDLE);
        do_talkstop(-1, req_from);
        return -1;
    }

    ret = do_talkstart_check(errCode);
    if (ret) {
        do_talkstop(ret, req_from);
        return -1;
    }

    if (errCode == ERR_ASR_CONTINUE) {
#ifdef EN_AVS_COMMS
        avs_comms_stop_ux();
#endif
        killPidbyName("smplayer");
        asr_set_audio_pause(1);
        CAIMan_ASR_disable();
        do_talkstart_play_voice(errCode);

#if defined(LIFEPOD_CLOUD) || defined(PURE_RPC)
        if (1) {
#else
        if (g_alexa_req_from != 3) {
#endif
            AlexaInitRecordData();
        }
    } else {
        do_talkstop(-1, req_from);
        return -1;
    }

#if defined(LIFEPOD_CLOUD) || defined(PURE_RPC)
    if (1) {
#else
    if (g_alexa_req_from != 3) {
#endif
        alexa_dump_info.record_start_ts = tickSincePowerOn();
        time(&alexa_dump_info.last_dialog_ts);
        g_alexa_req_from = 2;
        g_wiimu_shm->asr_session_start = 1;
        avs_system_set_expect_speech_timed_out(1);
        SocketClientReadWriteMsg("/tmp/AMAZON_ASRTTS", "amazonRecognizeStart:2",
                                 strlen("amazonRecognizeStart:2"), NULL, NULL, 0);
        if (g_wiimu_shm->asr_mic_needless == 0) {
            if (!IsCCaptStart()) {
                printf("===== %s capture stopped, restart  =====\n", __FUNCTION__);
                CAIMan_deinit();
                CAIMan_ASR_stop();

                CAIMan_init();
                CAIMan_ASR_start();
            }
        }
        CAIMan_ASR_enable(0);

        printf("talk to continue enable at %d\n", (int)tickSincePowerOn());
    } else {
        printf("ignore talk continue for intercom alexa\n");
        avs_system_set_expect_speech_timed_out(0);
    }

    return 0;
}

void sigroutine(int dunno)
{
    int i;
    // WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
    // WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS, 0);
    // switch_content_to_background(0);
    printf("Alexa exception get a signal %d\n", dunno);
    switch (dunno) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        for (i = 0; i < SNDSRC_MAX_NUMB; i++) {
            g_wiimu_shm->snd_ctrl[i].volume[SNDSRC_ALEXA_TTS] = 100;
            g_wiimu_shm->snd_ctrl[i].volume[SNDSRC_ASR] = 100;
        }
        g_wiimu_shm->snd_ctrl[SNDSRC_ALEXA_TTS].mute = 1;
        intercom_uninit();
#if defined(SOFT_PREP_MODULE)
        CAIMan_soft_prep_deinit();
#endif
#ifdef ASR_TALK_VIA_FILE
        file_record_uninit();
#endif
        exit(1);
        break;
    default:
        printf("Alexa exception get a default signal %d\n", dunno);
        break;
    }

    return;
}

static void unauthorize_action(void)
{
    printf("unauthorize_action++++\n");
    if ((access(ALEXA_TOKEN_PATH, 0)) == 0) // if access_token exist,then delete it
        system("rm -rf " ALEXA_TOKEN_PATH);
    alexa_dump_info.login = ERR_ASR_USER_LOGIN_FAIL;

    if ((access("/vendor/wiimu/alexa_alarm", 0)) == 0) { // if access_token not exist
        if ((access("/vendor/wiimu/alexa_alarm_NamedTimerAndReminder", 0)) == 0) {
            system("rm -rf /vendor/wiimu/alexa_alarm_NamedTimerAndReminder & sync");
        }
        if ((access("/tmp/NamedTimerAndReminder_Audio", 0)) == 0) {
            system("rm -rf /tmp/NamedTimerAndReminder_Audio & sync");
        }
        system("rm -rf /vendor/wiimu/alexa_alarm");
        killPidbyName("alexa_alert");
        alert_reset();
        system("alexa_alert &");
    }

    printf("unauthorize_action----\n");
}

static void init_asr_feature(void)
{
    if (0 == (far_field_judge(g_wiimu_shm))) {
        g_wiimu_shm->asr_feature_flag = 0;
        return;
    }
#if defined(SOFT_PREP_HOBOT_MODULE)
    // adjust for hobot use
    system("cxdish sendcmd 0xD308632C 0x00000040 0x000001c0 000001c1 00000000 00000000 00000000 "
           "00000000 00000000 00000000");
#endif
#ifdef HARMAN
    g_wiimu_shm->asr_feature_flag = ASR_FEATURE_FAR_FIELD;
    g_wiimu_shm->asr_feature_flag |= ASR_FEATURE_DIGITAL_MIC;
    g_wiimu_shm->priv_cap1 |= (1 << CAP1_SPEAKER_MONO);
    if (g_wiimu_shm->device_is_hk_portable == 1) {
        // g_wiimu_shm->asr_feature_flag |= ASR_FEATURE_2MIC;
    } else {
        g_wiimu_shm->asr_feature_flag |= ASR_FEATURE_4MIC;
    }
#endif
    get_prompt_setting_from_nvram();
}

#if defined(PLATFORM_AMLOGIC)
void amlogic_telnetd(int onOff)
{
    char buff[128];
    printf("%s:%d\n", __func__, onOff);
    if (onOff) {
        snprintf(buff, sizeof(buff), " sed -i 's/#telnet/telnet/g' %s", "/etc/inetd.conf");
    } else {
        snprintf(buff, sizeof(buff), " sed -i 's/telnet/#telnet/g' %s", "/etc/inetd.conf");
    }
    system(buff);
    sync();
    system("killall -HUP inetd");
}
#else
void amlogic_telnetd(int onOff)
{
    if (onOff) {
        system("telnetd &");
    }
}
#endif

#ifdef A98_SPRINT
int check_has_alexa_token(void)
{
    int has_token = 0;
    FILE *_file = fopen(LIFEPOD_TOKEN_PATH, "rb");
    if (_file) {
        fseek(_file, 0, SEEK_END);
        size_t total_bytes = ftell(_file);
        if (total_bytes > 0) {
            has_token = 1;
        }
        fclose(_file);
    }

    return has_token;
}

void sig_alarm(int sig)
{
    if (!check_has_alexa_token()) {
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "USER_REQUEST:0", strlen("USER_REQUEST:0"),
                                 NULL, NULL, 0);
    }
}
#endif

int main(int argc, char **argv)
{

#if defined(BAIDU_DUMI)
    struct sigaction act;
#endif

    signal(SIGINT, sigroutine);
    signal(SIGTERM, sigroutine);
    signal(SIGKILL, sigroutine);
    signal(SIGALRM, sigroutine);
    signal(SIGQUIT, sigroutine);

    g_wiimu_shm = WiimuContextGet();
    if (!g_wiimu_shm) {
        printf("%s call WiimuContextGet failed, exit\n", argv[0]);
        return -1;
    }
    if (lpcfg_init() != 0) {
        printf("Fatal: asr_tts failed to init lpcfg\n");
        return -1;
    }
#ifdef EN_AVS_COMMS
#ifndef SECURITY_IMPROVE
    amlogic_telnetd(1);
#endif
    killPidbyName("asr_comms");
    start_comms_monitor();
#endif

#ifdef OPENWRT_AVS
    // for sgw usb sound card test
    g_wiimu_shm->priv_cap1 |= (1 << CAP1_USB_RECORDER);
    g_wiimu_shm->internet = 1;
    g_internet_access = 1;
    g_wiimu_shm->netstat = NET_CONN;
    g_wiimu_shm->Ethernet = 1;
#endif
    g_wiimu_shm->avs_music_new = 0;

#ifdef MIC_LESS_DEVICE
    g_wiimu_shm->asr_mic_needless = 1;
    fprintf(stderr, "This is MIC LESS Device\n");
#endif

    asr_init_cfg();
    init_asr_feature();
    killPidbyName("smoke_test");

#if defined(BAIDU_DUMI)
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, 0);

#ifndef OPENSSL_IS_BORINGSSL
    OPENSSL_config(NULL);
#endif /* OPENSSL_IS_BORINGSSL */
    SSL_load_error_strings();
    SSL_library_init();
#endif

#if defined(SOFT_PREP_MODULE)
    if (!CAIMan_soft_prep_init()) {
        printf("Soft PreProcess module init fail\n");
        return -1;
    }
#endif

    g_wiimu_shm->asr_ready = 0;
    if (g_wiimu_shm->asr_test_mode) {
        printf("********asr_tts Test Mode(%d) *********\n", g_wiimu_shm->asr_test_mode);
    }

    if (!g_wiimu_shm->asr_test_mode) // if vad_profile exist ,then enter vad_test mode
        alert_init();

    g_wiimu_shm->asr_ongoing = 0;
    g_wiimu_shm->asr_session_start = 0;
    g_wiimu_shm->asr_pause_request = 0;
    switch_content_to_background(0);
    memset(&alexa_dump_info, 0x0, sizeof(alexa_dump_t));

#ifdef EXPIRE_CHECK_ENABLE
    extern int run_expire_check_thread(char *process, int expire_days, int failed_flag,
                                       char *command, int interval);
    run_expire_check_thread("asr_tts", EXPIRE_DAYS, 3, NULL, 30);
#endif

    if (CLOSE_TALK != AlexaProfileType()) {
        if (!strlen(g_wiimu_shm->asr_dsp_fw_ver) || strlen(g_wiimu_shm->asr_dsp_fw_ver) > 16)
            read_dsp_version(g_wiimu_shm);
    }

#if defined(HARMAN)
    if (g_wiimu_shm->device_is_hk_portable == 1) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", "cxdish sendcmd 0xD3086331 0x00000040 0x00000000 00000000 "
                                         "00000000 00000000 00000000 00000000 00000000 00000000");
        system(buf);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", "cxdish sendcmd 0xD3086331 0x00000040 0x00000000 00000000 "
                                         "00000000 00000000 00000000 00000000 00000000 00000000");
        system(buf);
    }
#endif

    if (g_wiimu_shm->asr_mic_needless == 0) {
        CAIMan_init();

        CAIMan_YD_start();
        CAIMan_ASR_start();
        CAIMan_YD_enable();

#ifdef PURE_RPC
        mic_capture_server_register();
#endif

        if (g_internet_access == 0) {
            g_internet_access = g_wiimu_shm->internet;
            printf("GetInternetStat: g_internet_access=%d \n", g_internet_access);
            if (g_internet_access) {
                AudioCapReset(1);
            }
        }
    }

    if (1) { // if vad_profile exist ,then enter vad_test mode
        if (CLOSE_TALK != AlexaProfileType()) {
            asr_dsp_init();
        }
#if defined(SOFT_PREP_HOBOT_MODULE)
        printf("#### get_asr_ch_from_nvram = %d", g_wiimu_shm->asr_ch_asr);
        g_wiimu_shm->asr_ch_asr = 1; // force set asr_channel
#endif
        AlexaInit();

#ifdef ASR_TALK_VIA_FILE
        file_record_init();
#endif

#ifdef INTERCOM_ENABLE
        intercom_init();
#endif

        if (!g_wiimu_shm->asr_test_mode) {
            killPidbyName("alexa_alert");
            system("alexa_alert &");
        }
    }

#ifdef A98_SPRINT
    signal(SIGALRM, sig_alarm);
    alarm(5);
#endif

    evtSocket_start();

    if (!g_wiimu_shm->asr_test_mode)
        AlexaUnInit();

    lpcfg_deinit();
    return 0;
}
