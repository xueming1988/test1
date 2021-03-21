#include <stdio.h>
#include <stdlib.h>
#include "deviceSDK.h"
#include <sys/stat.h>
#include <unistd.h>
#include "xiaoweiSDK.h"
#include <sys/time.h>
#include "voipSDK.h"
#include "string.h"
#include <stdbool.h>
#include "wm_context.h"
#include "wm_util.h"
#include "wm_db.h"
#include <signal.h>
#include <pthread.h>
#include <resolv.h>
#include <string.h>
#include <curl/curl.h>

#include "asr_tts.h"
#include "caiman.h"
#include "xiaowei_player.h"
#include "url_player_xiaowei.h"
#include "deviceSDK.h"
#include "xiaowei_interface.h"
#ifdef BT_HFP_ENABLE
#include "playback_bt_hfp.h"
#endif
#include <fcntl.h>

WIIMU_CONTEXT *g_wiimu_shm;
int g_xiaowei_debug_level = 5;
int g_xiaowei_wakeup_check_debug = 0;
int g_xiaowei_talkstart_test = 0;
#define XIAOWEI_TALKSTART_VOICE 1
static int do_talkstop(int errcode);
static ASR_ERROR_CODE do_talkstart_check(int req_from);
static void do_talkstart(int req_from);
static int do_talkstart_prepare(int req_from);
static void do_talkcontinue(int errCode);
extern void StartRecordTest(void);
extern void StopRecordTest(void);

pthread_mutex_t mutex_asr_tts_op = PTHREAD_MUTEX_INITIALIZER;
int g_tts_whether_start = 0;
char g_tts_weather_cmd[256] = {0};

int g_asr_step = 0;          // 0 asr mode;  //1  wait for search name
int g_asr_search_engine = 0; // 1: TTPOD,  2: Ximalaya
int g_asr_errors = 0;
int g_silence_ota_query = 0;

asr_dump_t asr_dump_info;

static int g_asr_disable = 0;
static int g_record_testing = 0;

extern int ignore_reset_trigger;
#if defined(ASR_FAR_FIELD)
extern int mic_channel_test_mode;
#endif

int g_record_asr_input = 0;
int g_asr_index = 0;
int g_tx_log_enable = 0;
int g_tx_net_ble = 0;

#define os_calloc(n, size) calloc((n), (size))

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

#define os_strdup(ptr) ((ptr != NULL) ? strdup(ptr) : NULL)

#define os_power_on_tick() tickSincePowerOn()

#define PLAYER_MANAGER_DEBUG(fmt, ...)                                                             \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Player Manager] " fmt,                              \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

// xiaowei url player handle functions
static url_player_t *url_player = NULL;
XIAOWEI_PARAM_STATE g_xiaowei_report_state;

int url_notify_cb(e_url_notify_t notify, url_stream_t *url_stream, void *ctx)
{
    int ret = 0;
    url_player_t *player_manager = (url_player_t *)ctx;

    if (!url_stream || !player_manager) {
        return -1;
    }

    PLAYER_MANAGER_DEBUG("--------url_notify=%d cur_pos=%lld token=%s--------\n", notify,
                         url_stream->cur_pos, url_stream->stream_id);

    switch (notify) {
    case e_notify_get_url:
        if (g_xiaowei_report_state.play_id)
            free(g_xiaowei_report_state.play_id);
        g_xiaowei_report_state.play_id = strdup(url_stream->stream_id);
        g_xiaowei_report_state.play_state = play_state_preload;
        g_xiaowei_report_state.play_offset = 0; // play started offset 0
        g_xiaowei_report_state.volume = g_wiimu_shm->volume;
        printf("---1---xiaowei_local_state_report:into,offset=%llu,state=%d,play_id=%s\n",
               g_xiaowei_report_state.play_offset, g_xiaowei_report_state.play_state,
               g_xiaowei_report_state.play_id);
        ret = xiaowei_report_state(&g_xiaowei_report_state);
        if (ret) {
            printf("%s:errcode=%d\n", __func__, ret);
        }
        break;
    case e_notify_play_started: {
        g_xiaowei_report_state.play_state = play_state_start;
        g_xiaowei_report_state.play_offset = 0; // play started offset 0
        printf("---1---xiaowei_local_state_report:into,offset=%llu,state=%d,play_id=%s\n",
               g_xiaowei_report_state.play_offset, g_xiaowei_report_state.play_state,
               g_xiaowei_report_state.play_id);
        g_xiaowei_report_state.volume = g_wiimu_shm->volume;
        ret = xiaowei_report_state(&g_xiaowei_report_state);
        if (ret) {
            printf("%s:errcode=%d\n", __func__, ret);
        }
    } break;
    case e_notify_play_stopped: {
        g_xiaowei_report_state.play_state = play_state_abort;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_paused: {
        g_xiaowei_report_state.play_state = play_state_paused;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_resumed: {
        g_xiaowei_report_state.play_state = play_state_resume;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_near_finished: {

    } break;
    case e_notify_play_finished: {
        g_xiaowei_report_state.play_state = play_state_finished;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_error: {
        g_xiaowei_report_state.play_state = play_state_err;
        xiaowei_refresh_music_id(url_stream->stream_id);
    } break;
    case e_notify_play_delay_passed: {
    } break;
    case e_notify_play_interval_passed: {
    } break;
    case e_notify_play_position_passed: {
    } break;
    case e_notify_play_end: {
    } break;
    case e_notify_play_need_more: {
        if (url_player_get_more_playlist(url_player))
            xiaowei_get_more_playlist(url_stream->stream_id, 0);
    } break;
    case e_notify_play_need_history: {
        if (url_player_get_history_playlist(url_player))
            xiaowei_get_history_playlist();
    } break;
    default: {
    } break;
    }

    return 0;
}

int xiaowei_url_player_init(void)
{
    g_xiaowei_report_state.play_mode = e_mode_playlist_loop; // default mode
    if (url_player == NULL) {
        url_player = (url_player_t *)os_calloc(1, sizeof(url_player));
        if (url_player) {
            url_player = url_player_create(url_notify_cb, url_player);
        }
    }
    set_player_mode(url_player, e_mode_playlist_loop);

    return 0;
}

int xiaowei_url_player_uninit(void)
{
    if (url_player) {
        url_player_destroy(&url_player);
    }
    return 0;
}

int xiaowei_url_player_set_play_url(char *id, char *content, int type, unsigned int offset)
{
    url_stream_t new_stream = {0};
    new_stream.stream_id = strdup(id);
    new_stream.play_pos = offset;
    new_stream.url = strdup(content);
    new_stream.behavior = type;
    url_player_insert_stream(url_player, &new_stream);
}

#define MIN_START_PROMPT_VOLUME 10

int asr_config_get(char *param, char *value) { return wm_db_get(param, value); }

int asr_config_set(char *param, char *value) { return wm_db_set(param, value); }

int asr_config_set_sync(char *param, char *value) { return wm_db_set_commit(param, value); }

void asr_config_sync(void) { wm_db_commit(); }

#define ALSA_FAR_MIN_VOL (0x200)
#define ALSA_FAR_MAX_VOL (0x10000)

void switch_content_to_background(int val)
{
    g_wiimu_shm->asr_status = val ? 1 : 0;

    if (val) {
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 4, SNDSRC_ASR);
    } else {
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);
    }
}

size_t get_file_len(const char *path)
{
    struct stat buf;
    if (stat(path, &buf))
        return 0;
    return buf.st_size;
}

void delay_for_talkmode_switch(void)
{
    if (g_wiimu_shm->priv_cap2 & (1 << CAP2_FUNC_DISABLE_PLAY_CAP_SIM))
        usleep(100000); // waiting for mcu talkmode switch
}

void block_voice_prompt(char *path, int block)
{
    char file_vol[256];
    int volume = 100;
    int tmp_volume = g_wiimu_shm->volume;
    char *is_start_voice = NULL;

    // printf(">>>>init value=%d\n",g_wiimu_shm->volume);
    if (g_wiimu_shm->max_prompt_volume && g_wiimu_shm->volume >= g_wiimu_shm->max_prompt_volume)
        volume = 100 * g_wiimu_shm->max_prompt_volume / g_wiimu_shm->volume;
    strcpy(file_vol, path);
    if (!volume_prompt_exist(path)) {
        strcpy(file_vol, "common/ring2.mp3");
    }

/*
is_start_voice = strstr(file_vol,"alexa_start");
if (!is_start_voice) {
    is_start_voice = strstr(file_vol,"talk_start_t");
}
*/
#if 0
    if (g_wiimu_shm->mute == 1 && is_start_voice) {          // mute status
        if (g_wiimu_shm->volume < MIN_START_PROMPT_VOLUME) { // temporarilly voice up prompt voice
            char buff[128] = {0};
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", MIN_START_PROMPT_VOLUME);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
            delay_for_talkmode_switch();
            volume_pormpt_direct(file_vol, 1, 0);

            memset(buff, 0, sizeof(buff));
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", 0); // recover volume 0
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        } else { // do not voice up prompt voice
            char buff[128] = {0};
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", g_wiimu_shm->volume);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
            delay_for_talkmode_switch();
            volume_pormpt_direct(file_vol, 1, 0);

            memset(buff, 0, sizeof(buff));
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", 0); // recover volume 0
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        }

        return;
    } else if (g_wiimu_shm->volume < MIN_START_PROMPT_VOLUME && is_start_voice) { // volume 0 status
        char buff[128] = {0};
        int vol = g_wiimu_shm->volume;
        sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", MIN_START_PROMPT_VOLUME);
        SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        delay_for_talkmode_switch();
        volume_pormpt_direct(file_vol, 1, 0);

        memset(buff, 0, sizeof(buff));
        sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", vol); // recover volume 0
        SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        return;
    }
#endif

    if (far_field_judge(g_wiimu_shm)) {
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

#define ALSA_FAR_MIN_VOL (0x200)

static int playback_paused = 0;
void asr_set_audio_silence(void)
{
// printf("asr_set_audio_silence at %d \r\n", (int)tickSincePowerOn());

#if 0
#if defined(ASR_FAR_FIELD)
    snd_pause_flag_set(1); //ALSA_FAR_MIN_VOL
#else
    snd_pause_flag_set(1);
#endif
#else
    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 0, SNDSRC_ASR);
#endif
}

void asr_set_audio_resume(void)
{
    // wiimu_log(1,0,0,0, "%s resume mode=%d, status1=%d, status2=%d", __FUNCTION__,
    // g_wiimu_shm->play_mode, playback_paused, g_wiimu_shm->asr_pause_request);
    switch_content_to_background(0);
    playback_paused = 0;
}

void AudioCapReset(int enable)
{
    if (enable) {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();

        CAIMan_init();
        CAIMan_YD_start();
        CAIMan_ASR_start();
        CAIMan_YD_enable();
    } else {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();
    }
    g_wiimu_shm->asr_ongoing = 0;
    g_asr_errors = 0;
    g_asr_disable = 0;
}

int getSessionID(char *ptr)
{
    char *pDivide;
    char idBuf[16] = {0};

    if (ptr == NULL || ptr[0] == 0) {
        return -1;
    }

    pDivide = strstr(ptr, ":");
    if (pDivide == NULL) {
        return -1;
    }

    if (pDivide == ptr) {
        return -1;
    }

    if (pDivide - ptr > 8) {
        return -1;
    }

    strncpy(idBuf, ptr, pDivide - ptr);

    return atoi(idBuf);
}

char *removeSessionID(char *ptr)
{
    char *pDivide;

    if (ptr == NULL || ptr[0] == 0) {
        return ptr;
    }

    pDivide = strstr(ptr, ":");
    if (pDivide == NULL) {
        return ptr;
    }

    if (pDivide == ptr) {
        return ptr;
    }

    if (pDivide - ptr > 8) {
        return ptr;
    }

    return pDivide + 1;
}

/*
* return 0 for ok,  others for fail reason
*/
static ASR_ERROR_CODE do_talkstart_check(int req_from)
{
    if (g_asr_disable) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (g_wiimu_shm->privacy_mode) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (g_wiimu_shm->internet == 0 ||
        (g_wiimu_shm->netstat != NET_CONN && g_wiimu_shm->Ethernet == 0)) {
        return ERR_ASR_NO_INTERNET;
    }
    if (0 == g_wiimu_shm->xiaowei_online_status) {
        // return ERR_ASR_USER_LOGIN_FAIL;//even online status is 0,still request(new xiaowei sdk)
    }

    if (g_wiimu_shm->slave_notified) {
        return ERR_ASR_DEVICE_IS_SLAVE;
    }
    return 0;
}

static void do_talkstart_play_voice(int req_from)
{
    // g_wiimu_shm->smplay_skip_silence = 1;
    // block_voice_prompt("common/ring3.mp3", 1);
    // g_wiimu_shm->asr_ongoing = 0;
}

static int do_talkstart_prepare(int req_from)
{
    g_wiimu_shm->asr_ongoing = 1;

    asr_set_audio_silence();

#if defined(ASR_FAR_FIELD)
// cxdish_change_boost();
#endif

    // CAIMan_ASR_disable(); //?? why

    return 0;
}

static void do_talkstart(int req_from)
{
    int ret;
    int is_cancel_in_record = 0;
    g_asr_step = 0;
    g_asr_errors = 0;
    g_asr_index++;

    printf("do_talkstart at %d\n", (int)tickSincePowerOn());

    if (g_wiimu_shm->asr_trigger_test_on) {
        set_asr_led(5); // error led
        do_talkstop(-1);
        return;
    }
    if (g_xiaowei_talkstart_test) {
#ifdef XIAOWEI_TALKSTART_VOICE
        block_voice_prompt("cn/asr_start.mp3", 0);
#endif
        return;
    }
    ret = do_talkstart_check(req_from);
    if (ret) {
        if (ret != ERR_ASR_MODULE_DISABLED)
            set_asr_led(5); // error led
        do_talkstop(ret);
        // set_asr_led(1); //idle led
        return;
    }

    do_talkstart_prepare(req_from);
    is_cancel_in_record = OnCancel();
    if (is_cancel_in_record) {
        req_from =
            0; // if canceled when in record,should set req_from to 0,so do not upload wakeup word
    }
    set_asr_led(2);
#ifdef XIAOWEI_TALKSTART_VOICE
    block_voice_prompt("cn/asr_start.mp3", 0);
#endif
    xiaoweiInitRecordData();
    CAIMan_ASR_enable(0);
    CAIMan_YD_enable();

    onWakeUp(req_from);
    printf("talk to enable at %d\n", (int)tickSincePowerOn());
}

static int do_talkstop(int errcode)
{
    if (g_record_testing)
        return 0;

    printf("do_talkstop %d +++++++\n", errcode);

    if (errcode == ERR_ASR_NO_INTERNET) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
#ifdef MARSHALL_XIAOWEI
            block_voice_prompt("cn/network_trouble.mp3", 1);
            set_asr_led(1);
#else

#ifdef YD_XIAOWEI_HOBOT_WWE_MODULE
            // Disable mic data flow into WWE when playing voice prompt
            // This is a workaround for false acception that
            // "小微尚未联网" triggers wake word engine.
            CAIMan_YD_disable();
#endif // end of YD_XIAOWEI_HOBOT_WWE_MODULE

            block_voice_prompt("cn/network_trouble.mp3", 1);

#ifdef YD_XIAOWEI_HOBOT_WWE_MODULE
            CAIMan_YD_enable();
#endif // end of YD_XIAOWEI_HOBOT_WWE_MODULE

#endif
        } else
            block_voice_prompt("us/internet_lost.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_LISTEN_NONE) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_beg_pardon.mp3", 0);
        else
            block_voice_prompt("us/beg_pardon.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MODULE_DISABLED) {
        CAIMan_ASR_disable();
        // block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_DEVICE_IS_SLAVE) {
        CAIMan_ASR_disable();
        block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_NOTHING_PLAY) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_BOOK_SEARCH_FAIL) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_book_search_fail.mp3"))
                block_voice_prompt("cn/CN_book_search_fail.mp3", 0);
            else
                block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        } else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MUSIC_SEARCH_FAIL) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_music_search_fail.mp3"))
                block_voice_prompt("cn/CN_music_search_fail.mp3", 0);
            else
                block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        } else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_SEARCHING_BEGIN) {
        CAIMan_ASR_disable();
        g_wiimu_shm->smplay_skip_silence = 1;
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_music_searching.mp3", 0);
        else
            block_voice_prompt("common/ring2.mp3", 0);
    } else if (errcode == ERR_ASR_NETWORK_LOW_STOP) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_slow_connection.mp3", 0);
        else
            block_voice_prompt("us/slow_connection.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_USER_LOGIN_FAIL) {
        CAIMan_ASR_disable();
        block_voice_prompt("cn/not_login.mp3", 0);

    } else if (errcode == ERR_ASR_EXCEPTION) {
        CAIMan_ASR_disable();
        block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MUSIC_STOP) {
        CAIMan_ASR_disable();
        CAIMan_YD_enable();
    } else if (errcode == ERR_ASR_MUSIC_RESUME) {

    } else {
        CAIMan_ASR_disable();
        // asr_set_audio_resume();
    }

    CAIMan_YD_enable();
    g_wiimu_shm->asr_ongoing = 0;
    // g_wiimu_shm->asr_pause_request = 0;
    // snd_pause_flag_set(0);
    playback_paused = 0;
    printf("do_talkstop ------\n");

    return 0;
}

static void do_talkcontinue(int errCode)
{
    int ret = -1;
    if (g_record_testing)
        return;

    if (g_asr_errors > 0) {
        do_talkstop(-1);
        return;
    }

    ret = do_talkstart_check(errCode);
    if (ret) {
        do_talkstop(ret);
        return;
    }

    if (errCode == ERR_ASR_LISTEN_NONE) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_beg_pardon.mp3", 1);
        else
            block_voice_prompt("us/beg_pardon.mp3", 1);
        g_asr_errors++;
    } else if (errCode == ERR_ASR_CONTINUE_MUSIC) {
        g_wiimu_shm->smplay_skip_silence = 1;
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_what_music.mp3"))
                block_voice_prompt("cn/CN_what_music.mp3", 1);
            else
                block_voice_prompt("cn/CN_what_listen.mp3", 1);
        } else {
            if (volume_prompt_exist("us/what_music.mp3"))
                block_voice_prompt("us/what_music.mp3", 1);
            else if (volume_prompt_exist("us/what_listen.mp3"))
                block_voice_prompt("us/what_listen.mp3", 1);
            else
                block_voice_prompt("common/ring1.mp3", 1);
        }
        g_asr_step = 1;
        g_asr_search_engine = 1;
    } else if (errCode == ERR_ASR_CONTINUE_BOOK) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_what_book.mp3"))
                block_voice_prompt("cn/CN_what_book.mp3", 1);
            else
                block_voice_prompt("cn/CN_what_listen.mp3", 1);
        } else {
            if (volume_prompt_exist("us/what_book.mp3"))
                block_voice_prompt("us/what_book.mp3", 1);
            else if (volume_prompt_exist("us/what_listen.mp3"))
                block_voice_prompt("us/what_listen.mp3", 1);
            else
                block_voice_prompt("common/ring1.mp3", 1);
        }
        g_asr_step = 1;
        g_asr_search_engine = 2;
    } else if (errCode == ERR_ASR_MIS_UNDERSTAND) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_what_question.mp3", 1);
        else
            block_voice_prompt("common/ring1.mp3", 1);
        g_asr_errors++;
    } else if (errCode == ERR_ASR_NETWORK_LOW_CONTINUE) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_slow_connection.mp3", 1);
        else
            block_voice_prompt("us/slow_connection.mp3", 1);
        g_asr_errors++;
    } else if (errCode == ERR_ASR_CONTINUE) {
    } else {
        g_wiimu_shm->asr_ongoing = 0;
    }

    CAIMan_YD_disable();

    asr_dump_info.record_start_ts = tickSincePowerOn();
    time(&asr_dump_info.last_dialog_ts);

    // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=MCUTLKON",strlen("GNOTIFY=MCUTLKON"),NULL,NULL,0);
    // CAIMan_ASR_enable();

    // qq_xiaowei_talk_ctrol(1, 0);

    printf("talk to continue enable at %d\n", (int)tickSincePowerOn());
}

void sigroutine(int dunno)
{
    printf("QQ get a signal -- %d \n", dunno);
    switch (dunno) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        exit(1);
    default:
        printf("QQ get a defailt signal %d\n", dunno);
        break;
    }

    return;
}

static int ble_easy_setup_res(char *code)
{
    NotifyMessage(BT_MANAGER_SOCKET, code);
    return 0;
}

void weixin_bind_ticket_callback(int errCode, const char *ticket, unsigned int expireTime,
                                 unsigned int expire_in)
{
    char cmd[64];
    static int ticket_timeout_flag = 0;
    wiimu_log(1, 0, 0, 0, "%s:errorcode=%d,ticket=%s,expiretime=%u,expire_in=%u\n", __func__,
              errCode, ticket, expireTime, expire_in);
    if ((!errCode) && (ticket)) {
        ticket_timeout_flag = 0;
        strncpy(g_wiimu_shm->txqq_token, ticket, sizeof(g_wiimu_shm->txqq_token) - 1);
        // g_wiimu_shm->xiaowei_get_ticket_flag =1;

        snprintf(cmd, sizeof(cmd), "blecode:22:00");
        ble_easy_setup_res(cmd); // NETRES:DHCPOK
    } else if (0 != errCode)     // 35,maybe network is not good,call getticket func again.
    {
        wiimu_log(1, 0, 0, 0, "%s:ticket_timeout_flag=%d\n", __func__, ticket_timeout_flag);
        if (ticket_timeout_flag == 0) {
            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "INTERNET_ACCESS:1",
                                     strlen("INTERNET_ACCESS:1"), NULL, NULL, 0);
        } else {
            ticket_timeout_flag = 0;
        }
        ticket_timeout_flag++;
    }
}

void autoAcceptContactRequest(DEVICE_BINDER_INFO info)
{
    printf("autoAcceptContactRequest info.username = %s", info.username);
    // device_handle_binder_verify_request(info.username, true);
}
void online_net_status_changed(int status)
{
    int ret = 0;
    wiimu_log(1, 0, 0, 0, "%s:status=%d\n", __func__, status);
    if (0 == status)
        g_wiimu_shm->xiaowei_online_status = 0;
    else {
        g_wiimu_shm->xiaowei_online_status = 1;
        // volume_pormpt_action(VOLUME_ALEXA_READY);
    }
}

void loginComplete(int error_code)
{
    int ret = 0;
    wiimu_log(1, 0, 0, 0, "login ok , err code = %d,ezlink_ble_setup=%d\n", error_code,
              g_wiimu_shm->ezlink_ble_setup);
    if (!error_code) {
        if (g_wiimu_shm->ezlink_ble_setup && g_wiimu_shm->xiaowei_login_flag == 0) {
            ret = device_get_bind_ticket(weixin_bind_ticket_callback);
            if (ret) {
                wiimu_log(1, 0, 0, 0, "%s:device_get_bind_ticket errorcode=%d\n", __func__, ret);
            }
        }
        g_wiimu_shm->xiaowei_login_flag = 1;
    }
}

void onBinderListChange(int error_code, DEVICE_BINDER_INFO *pBinderList, int nCount)
{
    printf("onBinderListChange count = %d\n", nCount);
    for (int i = 0; i < nCount; i++) {
        printf("binder[%d], username = %s, nickname = %s, remark = %s, exportid = %s\n", i,
               pBinderList[i].username, pBinderList[i].nickname, pBinderList[i].remark,
               pBinderList[i].export_id);
    }
}
#ifdef XIAOWEI_BETA
#define TXQQ_DEFAULT_LICENSE                                                                       \
    "MEQCIAWqyl7iTNpl7IvybF2+cAgKWvHpFpOqE42P40SE3QYRAiBXzxMEhyy0nihcYBOiHVrvfMpimfRBx8d+i1IQE/"   \
    "7Pgw=="
#define TXQQ_DEFAULT_GUID "53H94987A0000001"
#define TXQQ_DEFAULT_PID "691"
#define TXQQ_DEFAULT_APPID "ilinkapp_060000df9fcdc9"
#define TXQQ_DEFAULT_KEY_VER "1"

char txqq_licence[TXQQ_DEFAULT_LICENSE_LEN] = TXQQ_DEFAULT_LICENSE;
char txqq_guid[TXQQ_DEFAULT_GUID_LEN] = TXQQ_DEFAULT_GUID;
char txqq_product_id[TXQQ_DEFAULT_PID_LEN] = TXQQ_DEFAULT_PID;
char txqq_appid[TXQQ_DEFAULT_APPID_LEN] = TXQQ_DEFAULT_APPID;
char txqq_key_ver[TXQQ_DEFAULT_KEY_VER_LEN] = TXQQ_DEFAULT_KEY_VER;

#else
char txqq_licence[TXQQ_DEFAULT_LICENSE_LEN] = {0};
char txqq_guid[TXQQ_DEFAULT_GUID_LEN] = {0};
char txqq_product_id[TXQQ_DEFAULT_PID_LEN] = {0};
char txqq_appid[TXQQ_DEFAULT_APPID_LEN] = {0};
char txqq_key_ver[TXQQ_DEFAULT_KEY_VER_LEN] = {0};
#endif

#ifdef A98_ALEXA_BELKIN
void get_qqid_info(void)
{

    int fp = -1;
    char buffer[256] = {0};
    unsigned int readsize = 0;

    system("echo deviceid>/sys/class/unifykeys/name");
    fp = open("/sys/class/unifykeys/read", O_RDONLY);
    if (fp > 0) {
        readsize = read(fp, buffer, 256);
        sscanf(buffer, "SN:%s LICENSE:%s", txqq_guid, txqq_licence);
        printf("%s:read sn/license info:readsize=%d,buffer=%s,guid=%s,license=%s\n", __func__,
               readsize, buffer, txqq_guid, txqq_licence);

        close(fp);
    } else {
        printf("%s:open unifykeys file fail,fp=%d\n", __func__, fp);
    }
    strcpy(txqq_product_id, "1732");
    strcpy(txqq_appid, "ilinkapp_06000000d6ffcb");
    strcpy(txqq_key_ver, "2");
}
#else
void get_qqid_info(void)
{
    char buf[256] = {0};
    if (wm_db_get_ex(FACOTRY_NVRAM, "txqq_pid", buf) >= 0) {
        strcpy(txqq_product_id, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, "txqq_license", buf) >= 0) {
        strcpy(txqq_licence, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, "txqq_guid", buf) >= 0) {
        strcpy(txqq_guid, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, "txqq_appid", buf) >= 0) {
        strcpy(txqq_appid, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, "txqq_key_ver", buf) >= 0) {
        strcpy(txqq_key_ver, buf);
    }
    strcpy(g_wiimu_shm->txqq_product_id, txqq_product_id);
}
#endif
void weixin_device_init(void)
{
    DEVICE_INFO device_info = {0};
    DEVICE_INIT_PATH path = {0};
    DEVICE_NOTIFY notify = {0};

    wiimu_log(1, 0, 0, 0, "%s:pid=%s,license=%s,guid=%s,appid=%s,key_ver=%s\n", __func__,
              txqq_product_id, txqq_licence, txqq_guid, txqq_appid, txqq_key_ver);
    // device_info
    device_info.device_license = txqq_licence;
    device_info.device_serial_number = txqq_guid;

    device_info.product_id = atoi(txqq_product_id);
    device_info.xiaowei_appid = txqq_appid;
    device_info.key_version = atoi(txqq_key_ver);
    device_info.run_mode = 1;
    device_info.user_hardware_version = 3;

    strcpy(g_wiimu_shm->txqq_guid, txqq_guid);
    // notify
    notify.on_login_complete = loginComplete;
    notify.on_net_status_changed = online_net_status_changed;
    notify.on_binder_list_change = onBinderListChange;
    // contact test
    notify.on_binder_verify = autoAcceptContactRequest;

    // path
    path.system_path = (char *)"/vendor/weixin";
    path.system_path_capacity = 1024 * 1024;
    /* test param end */
    wiimu_log(1, 0, 0, 0, "init device result = %d \n",
              device_init(&device_info, &notify, &path, 0, NULL));
    wiimu_log(1, 0, 0, 0, "------uin=%llu------\n", device_get_uin());
}

void getDeviceSDKVer(void)
{
    /* get sdk version */
    unsigned int main_ver;
    unsigned int sub_ver;
    unsigned int fix_ver;
    unsigned int build_no;
    device_get_sdk_version(&main_ver, &sub_ver, &fix_ver, &build_no);
    printf("\ndevice sdk ver = %d.%d.%d.%d\n", main_ver, sub_ver, fix_ver, build_no);
}

int userLog(int level, const char *tag, const char *filename, int line, const char *funcname,
            const char *data)
{
#if 0
    if (level >= g_xiaowei_debug_level) {
        wiimu_log(1, 0, 0, 0,
                  "level = %d, tag = %s, filename = %s, line = %d, funcname = %s, data = %s\n",
                  level, tag, filename, line, funcname, data);
    }
#endif
    return 1;
}

void setLogFunc(void) { device_set_log_func(userLog, 4, true, false); }

void onPublicCode(int errCode, const char *path, unsigned int expireTime)
{
    printf("on get onPublicCode, errcode = %d, path = %s, expireTime = %d\n", errCode, path,
           expireTime);
}

void getPublicQrCode(void)
{
    // device_get_public_qr_code(onPublicCode);
}

int xiaowei_local_state_report(XIAOWEI_PARAM_STATE *_report_state)
{
    int ret = 0;
    if (_report_state->play_state != play_state_resume)
        _report_state->play_offset = get_play_pos();
    printf("------%s:into,offset=%llu,state=%d,play_id=%s\n", __func__, _report_state->play_offset,
           _report_state->play_state, _report_state->play_id);
    _report_state->volume = g_wiimu_shm->volume;
    ret = xiaowei_report_state(_report_state);
    if (ret) {
        printf("%s:errcode=%d\n", __func__, ret);
    }
}

int xiaowei_other_mode_state_report(XIAOWEI_PARAM_STATE *_report_state)
{
    int ret = 0;
    _report_state->play_state = play_state_idle;
    if (_report_state->skill_info.id) {
        free(_report_state->skill_info.id);
        _report_state->skill_info.id = NULL;
    }
    if (_report_state->skill_info.name) {
        free(_report_state->skill_info.name);
        _report_state->skill_info.name = NULL;
    }
    _report_state->play_offset = 0;
    _report_state->volume = g_wiimu_shm->volume;
    ret = xiaowei_report_state(_report_state);
    if (ret) {
        printf("%s:errcode=%d\n", __func__, ret);
    }
    return ret;
}

int global_control_handle(XIAOWEI_PARAM_RESPONSE *_response)
{
    char volume_buf[32] = {0};
    g_wiimu_shm->xiaowei_control_id = 0;
    printf("------%s:control_id=%d,control_value=%s\n", __func__, _response->control_id,
           _response->control_value);
    if (xiaowei_cmd_pause == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_pause, 0);
    } else if (xiaowei_cmd_play == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_resume, 0);
    } else if (xiaowei_cmd_stop == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_stop, 0);
    } else if (xiaowei_cmd_play_prev == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_prev, 0);
    } else if (xiaowei_cmd_play_next == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_next, 0);
    } else if (xiaowei_cmd_replay == _response->control_id) {
        url_player_set_cmd(url_player, e_player_cmd_replay, 0);
    } else if (xiaowei_cmd_play_res_id == _response->control_id) {
        if (_response->control_value) {
            int set_index = -1;
            if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
                printf("%s:before get playlist index by res_id\n", __func__);
                set_index = url_player_get_playid_index(url_player, _response->control_value);
                if (set_index != -1) {
                    url_player_set_cmd(url_player, e_player_cmd_set_index, set_index);
                }
            }
        }
    } else if (xiaowei_cmd_offset_set == _response->control_id) {
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (_response->control_value) {
                long long tmp_position = (long long)atol(_response->control_value);
                if (tmp_position < 0) {
                    tmp_position = 0;
                }
                url_player_set_cmd(url_player, e_player_cmd_seekto, tmp_position / 1000);

                int ret = 0;
                g_xiaowei_report_state.play_offset = tmp_position;
                g_xiaowei_report_state.play_state = play_state_resume;
                g_xiaowei_report_state.volume = g_wiimu_shm->volume;
                printf("------%s:into,state=%d,offset=%llu\n", __func__,
                       g_xiaowei_report_state.play_state, g_xiaowei_report_state.play_offset);
                ret = xiaowei_report_state(&g_xiaowei_report_state);
                if (ret) {
                    printf("%s:errcode=%d\n", __func__, ret);
                }
            }
        }
    } else if (xiaowei_cmd_offset_add == _response->control_id) {
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (_response->control_value) {
                long long tmp_position = (long long)atol(_response->control_value);
                long long target_offset = get_play_pos() / 1000 + tmp_position / 1000;
                if (target_offset < 0) {
                    target_offset = 0;
                }
                printf("---%s:get_play_pos=%lld,target_offset=%lld\n", __func__, get_play_pos(),
                       target_offset);
                url_player_set_cmd(url_player, e_player_cmd_seekto, target_offset);

                int ret = 0;
                g_xiaowei_report_state.play_offset = target_offset * 1000;
                g_xiaowei_report_state.play_state = play_state_resume;
                g_xiaowei_report_state.volume = g_wiimu_shm->volume;
                printf("------%s:into,state=%d,offset=%llu\n", __func__,
                       g_xiaowei_report_state.play_state, g_xiaowei_report_state.play_offset);
                ret = xiaowei_report_state(&g_xiaowei_report_state);
                if (ret) {
                    printf("%s:errcode=%d\n", __func__, ret);
                }
            }
        }
    } else if (xiaowei_cmd_mute == _response->control_id) {
        wiimu_player_ctrl_set_mute(1);
    } else if (xiaowei_cmd_unmute == _response->control_id) {
        wiimu_player_ctrl_set_mute(0);
    } else if (xiaowei_cmd_vol_set_to == _response->control_id) {
        int vol_value = 0;
        if (_response->control_value) {
            vol_value = atoi(_response->control_value);
            if (vol_value < 0) {
                vol_value = 0;
            }
            if (vol_value > 100) {
                vol_value = 100;
            }
            sprintf(volume_buf, "setPlayerCmd:vol:%d", vol_value);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", volume_buf, strlen(volume_buf),
                                     NULL, NULL, 0);
        }
    } else if (xiaowei_cmd_vol_add == _response->control_id) {
        int vol_step = 0;
        vol_step = atoi(_response->control_value);
        if (vol_step > 100) {
            vol_step = 100;
        }
        if (vol_step < -100) {
            vol_step = -100;
        }
        if (vol_step >= 0) {
            sprintf(volume_buf, "setPlayerCmd:Vol++%d", vol_step);
        } else {
            vol_step = -vol_step;
            sprintf(volume_buf, "setPlayerCmd:Vol--%d", vol_step);
        }
        SocketClientReadWriteMsg("/tmp/Requesta01controller", volume_buf, strlen(volume_buf), NULL,
                                 NULL, 0);
    } else if (xiaowei_cmd_close_mic == _response->control_id) {
        if (!g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 1;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                 strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_PRIVACY_MODE, "1");
        asr_config_sync();
    } else if (xiaowei_cmd_open_mic == _response->control_id) {
        if (g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 0;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                 strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_PRIVACY_MODE, "0");
        asr_config_sync();
    } else if (xiaowei_cmd_set_play_mode == _response->control_id) {
        int tmp_play_mode = 0;
        tmp_play_mode = atoi(_response->control_value);
        g_xiaowei_report_state.play_mode = tmp_play_mode;
        set_player_mode(url_player, tmp_play_mode);
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } else if (xiaowei_cmd_report_state == _response->control_id) {
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } else if (xiaowei_cmd_bluetooth_mode_on == _response->control_id) {
        if (_response->resource_groups_size == 0) {
#ifdef A98_MARSHALL_STEPIN
            SocketClientReadWriteMsg("/tmp/bt_manager", "btconnectrequest:3",
                                     strlen("btconnectrequest:3"), NULL, NULL, 0);
#else
            SocketClientReadWriteMsg("/tmp/bt_manager", "btavkenterpair", strlen("btavkenterpair"),
                                     NULL, NULL, 0);
#endif
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_bluetooth_mode_on;
        }
    } else if (xiaowei_cmd_bluetooth_mode_off == _response->control_id) {
        char rep[32];
        SocketClientReadWriteMsg("/tmp/bt_manager", "btdisconnectall", strlen("btdisconnectall"),
                                 NULL, NULL, 0);
        NotifyMessageACKBlockIng(BT_MANAGER_SOCKET, "btmodeswitch:0", rep, 32);
    } else if (xiaowei_cmd_shutdown == _response->control_id) {
        if (_response->resource_groups_size == 0) {
            SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=VOICE_REQUEST_SHUTDOWN",
                                     strlen("GNOTIFY=VOICE_REQUEST_SHUTDOWN"), NULL, NULL, 0);
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_shutdown;
        }
    } else if (xiaowei_cmd_reboot == _response->control_id) {
        if (_response->resource_groups_size == 0) {
            SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=REBOOT",
                                     strlen("GNOTIFY=REBOOT"), NULL, 0, 0);
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_reboot;
        }
    }
}

bool onRequestCallback(const char *voice_id, XIAOWEI_EVENT event, const char *state_info,
                       const char *extend_info, unsigned int extend_info_len)
{
// printf("onRequestCallback:: state_info=%s\n", (char *)state_info);
#ifdef A98_PHILIPST
    if ((g_wiimu_shm->play_mode != PLAYER_MODE_LINEIN))
#endif
    {
        if (event == xiaowei_event_on_idle) {
            wiimu_log(1, 0, 0, 0, "onRequestCallback:: dialog end \n");
            // set_asr_led(1);
        }
        if (event == xiaowei_event_on_request_start) {
            wiimu_log(1, 0, 0, 0, "onRequestCallback:: request start \n");
            set_asr_led(2);
        }
        if (event == xiaowei_event_on_speak) {
            printf("onRequestCallback:: on speak \n");
        }
        if (event == xiaowei_event_on_silent) {
            wiimu_log(1, 0, 0, 0, "onRequestCallback:: on silent \n");
            OnSilence();
            set_asr_led(3); // thinking
        }
        if (event == xiaowei_event_on_recognize) {
            // process asr result
            // string asrResult = extend_info;
            if (extend_info)
                printf("onRequestCallback:: get asr result: %s \n", extend_info);
        }
        if (event == xiaowei_event_on_response) {
            // get final result
            XIAOWEI_PARAM_RESPONSE *response = (XIAOWEI_PARAM_RESPONSE *)state_info;
            wiimu_log(1, 0, 0, 0, "***************************************** Debug resposne "
                                  "***********************************\n");
            wiimu_log(1, 0, 0, 0, "********basic result: errcode = %d, request_text = %s,  "
                                  "groupSize = %d ******\n",
                      response->error_code, response->request_text, response->resource_groups_size);

            if (response->skill_info.id && response->skill_info.name) {
                wiimu_log(1, 0, 0, 0, "%s:skillinfo name=%s,id=%s\n", __func__,
                          response->skill_info.name, response->skill_info.id);
                if (g_xiaowei_report_state.skill_info.name &&
                    g_xiaowei_report_state.skill_info.id) {
                    // xiaowei_local_state_report(&g_xiaowei_report_state);
                }
            }
            if (response->skill_info.id) {
                if (strncmp(response->skill_info.id, DEF_XIAOWEI_SKILL_ID_ALARM,
                            strlen(DEF_XIAOWEI_SKILL_ID_ALARM)) == 0) {
                    alarm_handle(response);
                } else {
                    if (response->is_recovery) {
                        if (g_xiaowei_report_state.skill_info.id)
                            free(g_xiaowei_report_state.skill_info.id);
                        if (g_xiaowei_report_state.skill_info.name)
                            free(g_xiaowei_report_state.skill_info.name);
                        g_xiaowei_report_state.skill_info.id = strdup(response->skill_info.id);
                        g_xiaowei_report_state.skill_info.name = strdup(response->skill_info.name);
                        g_xiaowei_report_state.skill_info.type = response->skill_info.type;
                    }
                }
            }
            wiimu_log(
                1, 0, 0, 0,
                "------grp_size=%d,more_playlist=%d,has_history=%d,is_recovery=%d,is_notify=%d,"
                "wakeup=%d,play_behave=%d\n",
                response->resource_groups_size, response->has_more_playlist,
                response->has_history_playlist, response->is_recovery, response->is_notify,
                response->wakeup_flag, response->play_behavior);

            if (g_xiaowei_wakeup_check_debug) {
                if (xiaowei_wakeup_flag_fail == response->wakeup_flag) {
                    long long ts = tickSincePowerOn();
                    int hour = ts / 3600000;
                    int min = ts % 3600000 / 60000;
                    int sec = ts % 60000 / 1000;
                    int ms = ts % 1000;
                    char *xiaowei_wakeup_check_fail = NULL;
                    char mv_cmd[128] = {0};

                    wiimu_log(1, 0, 0, 0, "%s,find xiaowei cloud check wake up word fail\n",
                              __func__);
                    asprintf(&xiaowei_wakeup_check_fail,
                             "wakeup_check_fail_%02d_%02d_%02d_%03d.pcm\n", hour, min, sec, ms);
                    if (xiaowei_wakeup_check_fail) {
                        wiimu_log(1, 0, 0, 0, "%s:xiaowei_wakeup_check_fail=%s\n", __func__,
                                  xiaowei_wakeup_check_fail);
                        sprintf(mv_cmd, "mv -f /tmp/web/asr_upload.pcm /tmp/web/%s &",
                                xiaowei_wakeup_check_fail);
                        wiimu_log(1, 0, 0, 0, "%s:mv_cmd=%s\n", __func__, mv_cmd);
                        system(mv_cmd);
                        free(xiaowei_wakeup_check_fail);
                    }
                }
            }
            int playlist_replaceall_done_flag = 0;
            for (int i = 0; i < response->resource_groups_size; i++) {
                for (int j = 0; j < response->resource_groups[i].resources_size; j++) {
                    if (response->resource_groups[i]
                            .resources[j]
                            .id /*&& response->resource_groups[i].resources[j].format == 0*/)
                        wiimu_log(1, 0, 0, 0,
                                  "###In group %d, resource %d, id = %s, type = %d , content = %s, "
                                  "playcount = %d, extendBuf = %s \n",
                                  i, j, response->resource_groups[i].resources[j].id,
                                  response->resource_groups[i].resources[j].format,
                                  response->resource_groups[i].resources[j].content,
                                  response->resource_groups[i].resources[j].play_count,
                                  response->resource_groups[i].resources[j].extend_buffer);
                    if ((response->resource_groups[i].resources[j].content) &&
                        (response->resource_groups[i].resources[j].id)) {
                        if ((response->resource_groups[i].resources[j].format ==
                             xiaowei_resource_tts_url) &&
                            (i == 0)) {
                            set_asr_led(4);
                            xiaowei_play_tts_url(response->resource_groups[i].resources[j].content,
                                                 response->resource_groups[i].resources[j].id);
                        } else {
                            if (response->resource_list_type == xiaowei_playlist_type_default) {
                                if ((response->resource_groups[i].resources[j].format ==
                                     xiaowei_resource_url) ||
                                    (response->resource_groups[i].resources[j].format ==
                                     xiaowei_resource_tts_url)) {
                                    if ((xiaowei_playlist_replace_all == response->play_behavior) &&
                                        (playlist_replaceall_done_flag == 0)) {
                                        playlist_replaceall_done_flag = 1;
                                        if (response->skill_info.id) {
                                            if (strncmp(response->skill_info.id,
                                                        DEF_XIAOWEI_SKILL_ID_NEWS,
                                                        strlen(DEF_XIAOWEI_SKILL_ID_NEWS)) == 0 ||
                                                strncmp(
                                                    response->skill_info.id,
                                                    DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE,
                                                    strlen(
                                                        DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE)) ==
                                                    0) {
                                                g_xiaowei_report_state.play_mode =
                                                    e_mode_playlist_sequential; // news mode
                                                set_player_mode(url_player,
                                                                e_mode_playlist_sequential);
                                            } else {
                                                g_xiaowei_report_state.play_mode =
                                                    e_mode_playlist_loop; // default mode
                                                set_player_mode(url_player, e_mode_playlist_loop);
                                                url_player_set_more_playlist(
                                                    url_player, (int)(response->has_more_playlist));
                                                url_player_set_history_playlist(
                                                    url_player,
                                                    (int)(response->has_history_playlist));
                                            }
                                        }
                                        xiaowei_url_player_set_play_url(
                                            response->resource_groups[i].resources[j].id,
                                            response->resource_groups[i].resources[j].content,
                                            e_replace_all, 0);
                                    } else {
                                        xiaowei_url_player_set_play_url(
                                            response->resource_groups[i].resources[j].id,
                                            response->resource_groups[i].resources[j].content,
                                            e_endqueue, 0);
                                    }
                                } else {
                                    wiimu_log(1, 0, 0, 0, "%s:unsupported resource type=%d\n",
                                              __func__,
                                              response->resource_groups[i].resources[j].format);
                                }
                            } else if (response->resource_list_type ==
                                       xiaowei_playlist_type_history) {
                                xiaowei_url_player_set_play_url(
                                    (char *)response->resource_groups[i].resources[j].id,
                                    (char *)response->resource_groups[i].resources[j].content,
                                    e_add_history, 0);
                            }
                        }
                    }
                }
            }
            playlist_replaceall_done_flag = 0;
            global_control_handle(response);
            printf("action type: %d \n", (response->play_behavior));
            if (response) {
                // you need to process the result here async
                if (response->context.id != NULL && strlen(response->context.id) >= 16) {
                    printf(
                        "onRequestCallback:: session not end, should start another voice request "
                        "with this session id = %s \n",
                        response->context.id);
                    g_wiimu_shm->continue_query_mode = 1;
                    wakeup_context_handle(&(response->context));
                } else {
                    g_wiimu_shm->continue_query_mode = 0;
                }
            }
            if (response->resource_groups_size == 0) {
                if (xiaowei_wakeup_flag_suc_continue == response->wakeup_flag) {
                    // do nothing,this is for wake up check response
                } else if (response->control_id == xiaowei_cmd_report_state) {
                    // do nothing,this is for global control get play state
                } else {
                    wiimu_log(1, 0, 0, 0, (char *)"%s:no resource group\n", __func__);
                    if ((response->control_id == xiaowei_cmd_pause) ||
                        (response->control_id == xiaowei_cmd_play_prev) ||
                        (response->control_id == xiaowei_cmd_play_next)) {
                        usleep(200000);
                    }
                    set_query_status(e_query_idle);
                    asr_set_audio_resume();
                    set_asr_led(1);
                }
            }
        }
        if (event == xiaowei_event_on_tts) {
            // get final result
            XIAOWEI_PARAM_AUDIO_DATA *ttsData = (XIAOWEI_PARAM_AUDIO_DATA *)state_info;
            printf("onRequestCallback:: get tts package \n");
            if (ttsData && ttsData->id) {
                printf("ttsData id = %s, seq = %d, is end = %d", ttsData->id, ttsData->seq,
                       ttsData->is_end);
            }
        }
        if (event == xiaowei_event_on_response_intermediate) {
            printf("onRequestCallback:: this is not for use \n");
        }
    }
    return true;
}

bool onNetDelayCallback(const char *voice_id, unsigned long long time)
{
    // printf("NetDelayCallback,not use\n");
    return true;
}

static int asr_dump_refresh(SOCKET_CONTEXT *pmsg_socket, char *recv_buf)
{
    struct tm *pTm = gmtime(&asr_dump_info.last_dialog_ts);
    char timestr[20] = {0};
    char tmp_buf[128] = {0};
    char record_file[128] = {0};
    char result_file[128] = {0};
    char cmdline[1024] = {0};
    int record_len = 0;

    sprintf(timestr, "%d%.2d%.2d_%.2d_%.2d_%.2d", pTm->tm_year + 1900, pTm->tm_mon + 1,
            pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    sprintf(record_file, "%s_v%d_%s_%s", g_wiimu_shm->project_name, g_wiimu_shm->mcu_ver, timestr,
            "record.pcm");
    sprintf(result_file, "%s_%s", timestr, "asr_result.txt");

    // printf("record_file:%s \r\n", record_file);
    // printf("result_file:%s \r\n", result_file);
    if (asr_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
        memset(tmp_buf, 0x0, sizeof(tmp_buf));
        sprintf(tmp_buf, "%s/%s_record.pcm", g_wiimu_shm->mmc_path, timestr);
        memset(cmdline, 0x0, sizeof(cmdline));
        sprintf(cmdline, "ln -s %s /tmp/web/%s 2>/dev/null", tmp_buf, record_file);
        system(cmdline);
        record_len = get_file_len(tmp_buf);
    } else {
        memset(cmdline, 0x0, sizeof(cmdline));
        sprintf(cmdline, "ln -s %s /tmp/web/%s 2>/dev/null", ASR_RECORD_FILE, record_file);
        system(cmdline);
        record_len = get_file_len(ASR_RECORD_FILE);
    }

    memset(cmdline, 0x0, sizeof(cmdline));
    sprintf(cmdline, "ln -s %s /tmp/web/%s 2>/dev/null", ASR_RESULT_FILE, result_file);
    system(cmdline);

    sprintf(recv_buf, "TIME: %s<hr>"
                      "record_ms=%lld vad_ms=%lld<hr>"
                      "record_len=%d result_len=%d<hr>"
                      "times_talk_on=%d http_ret_code=%d record_to_sd=%d<hr>",
            timestr, asr_dump_info.record_end_ts - asr_dump_info.record_start_ts,
            (asr_dump_info.vad_ts) ? asr_dump_info.vad_ts - asr_dump_info.record_start_ts : 0LL,
            record_len, get_file_len(ASR_RESULT_FILE), g_wiimu_shm->times_talk_on,
            asr_dump_info.http_ret_code, asr_dump_info.record_to_sd);

    strcat(recv_buf, "asr_record.pcm &nbsp;&nbsp;<a href=httpapi.asp?command=talkdump:0 "
                     "target=_asrplay>Play</a>");
    sprintf(recv_buf, "%s&nbsp;&nbsp;<a href=data/%s>download</a><hr>", recv_buf, record_file);

    sprintf(recv_buf, "%sasr_result.txt &nbsp;&nbsp;<a href=data/%s>download</a><hr>", recv_buf,
            result_file);

    strcat(recv_buf, "<a href=httpapi.asp?command=asrdump: target=_self>Refresh</a><br>");
    UnixSocketServer_write(pmsg_socket, recv_buf, strlen(recv_buf));
}

void weixinStart(void)
{
    /* xiaowei service */
    XIAOWEI_CALLBACK xiaoweiCB = {0};
    xiaoweiCB.on_request_callback = onRequestCallback;
    xiaoweiCB.on_net_delay_callback = onNetDelayCallback;
    XIAOWEI_NOTIFY xiaoweoNotify = {0};
    printf("init xiaowei service result = %d \n",
           xiaowei_service_start(&xiaoweiCB, &xiaoweoNotify));
}

void xiaoweiTextReq(void)
{
    const char *text = "上海今天天气";
    char voice_id[33] = {0};
    XIAOWEI_PARAM_CONTEXT context = {0};
    context.silent_timeout = 500;
    context.speak_timeout = 5000;
    context.voice_request_begin = true;
    context.voice_request_end = false;
    printf("send text request, result = %d\n",
           xiaowei_request(voice_id, xiaowei_chat_via_text, text, (int)strlen(text), &context));
}

void xiaowei_text_to_tts(void)
{
    const char *text = "麦克风已打开";
    char voice_id[33] = {0};
    XIAOWEI_PARAM_CONTEXT context = {0};
    context.silent_timeout = 500;
    context.speak_timeout = 5000;
    context.voice_request_begin = true;
    context.voice_request_end = false;
    printf("send text request, result = %d\n",
           xiaowei_request(voice_id, xiaowei_chat_only_tts, text, (int)strlen(text), &context));
}

void _XIAOWEI_ON_REQUEST_CMD_CALLBACK(const char *voice_id, int xiaowei_err_code, const char *json)
{
    wiimu_log(1, 0, 0, 0, "%s:voice_id=%s,error_code=%d\n", __func__, voice_id, xiaowei_err_code);
    json_object *response_obj = NULL;
    json_object *res_grp_obj = NULL, *res_res_obj = NULL;
    json_object *tmp_grp = NULL, *tmp_res = NULL;
    int obj_grp_num = 0, obj_res_num = 0;
    int i = 0, j = 0;
    char *content_url = NULL, *_voice_id = NULL;
    json_object *content_obj = NULL;
    int format = 0;
    int play_behavior = 0;
    if (!json) {
        printf("%s,json null\n", __func__);
        goto exit;
    }
    response_obj = json_tokener_parse(json);
    if (!response_obj) {
        printf("%s,response_obj null\n", __func__);
        goto exit;
    }
    play_behavior = JSON_GET_INT_FROM_OBJECT(response_obj, "play_behavior");
    printf("%s:play_behavior=%d\n", __func__, play_behavior);
    res_grp_obj = json_object_object_get(response_obj, "resource_groups");
    if (!res_grp_obj) {
        printf("%s,res_grp_obj null\n", __func__);
        goto exit;
    }

    obj_grp_num = json_object_array_length(res_grp_obj);
    printf("%s,obj_grp_num =%d\n", __func__, obj_grp_num);
    for (i = 0; i < obj_grp_num; i++) {
        tmp_grp = json_object_array_get_idx(res_grp_obj, i);
        if (!tmp_grp) {
            printf("%s,tmp_grp null\n", __func__);
            goto exit;
        }
        res_res_obj = json_object_object_get(tmp_grp, "resources");
        if (!res_res_obj) {
            printf("%s,res_res_obj null\n", __func__);
            goto exit;
        }
        obj_res_num = json_object_array_length(res_res_obj);
        printf("%s,obj_res_num =%d\n", __func__, obj_res_num);
        for (j = 0; j < obj_res_num; j++) {
            tmp_res = json_object_array_get_idx(res_res_obj, j);
            if (tmp_res) {
                content_url = JSON_GET_STRING_FROM_OBJECT(tmp_res, "content");
                format = JSON_GET_INT_FROM_OBJECT(tmp_res, "format");
                _voice_id = JSON_GET_STRING_FROM_OBJECT(tmp_res, "id");
                if (content_url && _voice_id) {
                    printf("%s:content_url=%s,voice_id=%s\n", __func__, content_url, _voice_id);
                    if (play_behavior == 4) {
                        url_player_refresh_playid_url(url_player, _voice_id, content_url);
                    } else {
                        if (format == 3) {
                            set_asr_led(4);
                            xiaowei_play_tts_url(content_url, voice_id);
                        }
                    }
                }
            }
        }
    }
exit:
    if (response_obj) {
        json_object_put(response_obj);
    }
}

int xiaowei_alarm_get_remind(char *remind_voice_id, char *remind_clock_id)
{
    char tmp_param[64] = {0};
    int ret = 0;

    if (NULL == remind_voice_id || NULL == remind_clock_id) {
        return 1;
    }
    wiimu_log(1, 0, 0, 0, "%s,voice_id=%s,clock_id=%s ++++++", __func__, remind_voice_id,
              remind_clock_id);
    snprintf(tmp_param, 64, "{\"clock_id\":%ld}", atol(remind_clock_id));

    ret = xiaowei_request_cmd(remind_voice_id, "ALARM", "get_timing_task", tmp_param,
                              _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
    // CleanSilentOTAFlag(g_wiimu_shm);
    wiimu_log(1, 0, 0, 0, "%s ---tmp_param=%s---", __func__, tmp_param);
}

int xiaowei_set_music_quality(int quality)
{
    char tmp_voiceid[128];
    char tmp_param[256] = {0};

    snprintf(tmp_param, 256, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"quality_config\": {\"type\": 0,\"quality\": %d}}",
             quality);
    xiaowei_request_cmd(tmp_voiceid, "PLAY_RESOURCE", "quality_config", tmp_param,
                        _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
}

int xiaowei_refresh_music_id(char *res_id)
{
    char tmp_voiceid[128];
    char tmp_param[256] = {0};

    snprintf(tmp_param, 256, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"play_ids\": [\"%s\"]}",
             res_id);
    xiaowei_request_cmd(tmp_voiceid, "PLAY_RESOURCE", "refresh", tmp_param,
                        _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
}

int xiaowei_get_more_playlist(char *res_id, int is_up)
{
    char tmp_param[512] = {0};
    char voice_id[64] = {0};
    int ret = 0;

    if (NULL == res_id) {
        return 1;
    }
    wiimu_log(1, 0, 0, 0, "%s,res_id=%s ++++++\n", __func__, res_id);
    if (g_xiaowei_report_state.skill_info.id && g_xiaowei_report_state.skill_info.name) {
        wiimu_log(1, 0, 0, 0, "%s,g_xiaowei_report_state.skill_info.id=%s,g_xiaowei_report_state."
                              "skill_info.name=%s ++++++\n",
                  __func__, g_xiaowei_report_state.skill_info.id,
                  g_xiaowei_report_state.skill_info.name);
        if (is_up) {
            snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                                     "\"%s\",\"name\": "
                                     "\"%s\"},\"play_id\": \"%s\",\"is_up\": true}",
                     g_xiaowei_report_state.skill_info.id, g_xiaowei_report_state.skill_info.name,
                     res_id);
        } else {
            snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                                     "\"%s\",\"name\": "
                                     "\"%s\"},\"play_id\": \"%s\",\"is_up\": false}",
                     g_xiaowei_report_state.skill_info.id, g_xiaowei_report_state.skill_info.name,
                     res_id);
        }

        ret = xiaowei_request_cmd(voice_id, "PLAY_RESOURCE", "get_more", tmp_param, NULL);
    }
    // CleanSilentOTAFlag(g_wiimu_shm);
    wiimu_log(1, 0, 0, 0, "%s,voidid=%s ---tmp_param=%s---", __func__, voice_id, tmp_param);
    return ret;
}

int xiaowei_get_history_playlist(void)
{
    char tmp_param[512] = {0};
    char voice_id[64] = {0};
    int ret = 0;

    snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"play_id\": \"rd001zmy3Y3Evni9\",\"is_up\": false}");
    xiaowei_request_cmd(voice_id, "PLAY_RESOURCE", "get_history", tmp_param, NULL);
    wiimu_log(1, 0, 0, 0, "%s,voidid=%s ---tmp_param=%s---", __func__, voice_id, tmp_param);

    return ret;
}

#define XIAOWEI_ALARM_COUNT_MAX 20
static pthread_cond_t g_alarm_active_condition;
static pthread_mutex_t mutex_alarm;

void xiaowei_start_alarm(void)
{
    g_wiimu_shm->xiaowei_alarm_start = 1;
    pthread_mutex_lock(&mutex_alarm);
    pthread_cond_signal(&g_alarm_active_condition);
    pthread_mutex_unlock(&mutex_alarm);
}

void xiaowei_stop_alarm(void)
{
    g_wiimu_shm->xiaowei_alarm_start = 0;
    if (GetPidbyName("smplayer")) {
        killPidbyName("smplayer");
    }
    usleep(200000);
}

void *xiaowei_alarm_thread(void *arg)
{
    int ret = 0;
    int alarm_count = 0;
    static int need_resume_playbcak = 0;
    int play_status = 0;
    while (1) {
        pthread_mutex_lock(&mutex_alarm);
        ret = pthread_cond_wait(&g_alarm_active_condition, &mutex_alarm);
        pthread_mutex_unlock(&mutex_alarm);
        set_asr_led(4);
        play_status = url_player_get_play_status(url_player);
        if (1 == play_status || 2 == play_status) {
            need_resume_playbcak = 1;
            url_player_set_cmd(url_player, e_player_cmd_pause, 0);
        }
        while ((alarm_count < XIAOWEI_ALARM_COUNT_MAX) && (g_wiimu_shm->xiaowei_alarm_start)) {
            {
                // printf("%s:internet=%d\n",__func__,g_wiimu_shm->internet);
                block_voice_prompt("common/alarm_sound.mp3", 1);
            }
            alarm_count++;
        }
        if (need_resume_playbcak) {
            url_player_set_cmd(url_player, e_player_cmd_resume, 0);
            need_resume_playbcak = 0;
        }
        set_asr_led(1);
        printf("%s:exit alarm loop\n", __func__);
        g_wiimu_shm->xiaowei_alarm_start = 0;
        alarm_count = 0;
        set_asr_led(9);
    }
}

static int force_ota_disabled(void)
{
    char tmp_buf[128] = {0};
    wm_db_get(NVRAM_ASR_DIS_FORCE_OTA, tmp_buf);
    if (tmp_buf && strlen(tmp_buf) > 0 && tmp_buf[0] == '1')
        return 1;
    return 0;
}

static int all_ota_disabled(void)
{
    char tmp_buf[128] = {0};
    wm_db_get(NVRAM_ASR_DIS_ALL_OTA, tmp_buf);
    if (tmp_buf && strlen(tmp_buf) > 0 && tmp_buf[0] == '1')
        return 1;
    return 0;
}

static unsigned int get_xiaowei_ota_flag(void)
{
    // set xiaowei query ota server flag
    char tmp_ota_flag_buf[128] = {0};
    unsigned int tmp_xiaowei_ota_flag = 1;
    wm_db_get(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, tmp_ota_flag_buf);
    if (tmp_ota_flag_buf && strlen(tmp_ota_flag_buf) > 0 && tmp_ota_flag_buf[0] == '2') {
        tmp_xiaowei_ota_flag = 2;
    } else {
#if defined(A98_MARSHALL_STEPIN) || defined(A98_PHILIPST)
        tmp_xiaowei_ota_flag = 2;
#else
        tmp_xiaowei_ota_flag = 1;
#endif
    }
    wiimu_log(1, 0, 0, 0, "%s:tmp_xiaowei_ota_flag=%d\n", __func__, tmp_xiaowei_ota_flag);
    return tmp_xiaowei_ota_flag;
}

void *evtSocket_thread(void *arg)
{
    int recv_num;
    char recv_buf[65536];
    int ret;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/RequestASRTTS";
    msg_socket.blocking = 1;

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
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
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
                    if (g_wiimu_shm->internet && !g_asr_disable)
                        AudioCapReset(1);
                    else
                        AudioCapReset(0);
                } else if (strncmp(recv_buf, "TtsStart", strlen("TtsStart")) == 0) {
                    char *ptr = recv_buf + strlen("TtsStart:");
                    int nID;
                    if (strstr(recv_buf, "TtsStart_session:")) {
                        ptr = recv_buf + strlen("TtsStart_session:");
                        nID = getSessionID(ptr);
                        if (nID != -1 && nID != g_asr_index) {
                            continue;
                        }
                        ptr = removeSessionID(ptr);
                    }

                    g_wiimu_shm->asr_ongoing = 0;
                    if (g_wiimu_shm->internet == 0 || (g_wiimu_shm->netstat != NET_CONN) ||
                        (g_wiimu_shm->internet == 0)) {
                        block_voice_prompt("cn/CN_internet_lost.mp3", 1);
                    } else {
                    }
                }

                else if (strncmp(recv_buf, "talkwakeup", strlen("talkwakeup")) == 0) {
                    int start = atoi(recv_buf + strlen("talkwakeup:"));
                    g_wiimu_shm->asr_ongoing = 0;
                    if (start == 1)
                        CAIMan_YD_enable();
                    else if (start == 0)
                        CAIMan_YD_disable();
                    else {
                        CAIMan_YD_disable();
                        CAIMan_YD_enable();
                    }
                } else if (strncmp(recv_buf, "GNOTIFY=NET_CONNECTED",
                                   strlen("GNOTIFY=NET_CONNECTED")) == 0) {
                    g_tx_net_ble = atoi(recv_buf + strlen("GNOTIFY=NET_CONNECTED:"));
                } else if (strncmp(recv_buf, "ttsCallback:", strlen("ttsCallback:")) == 0) {
                    char *ptr = (recv_buf + strlen("ttsCallback:"));
                    if (!strcmp(ptr, "CAIMan_YD_enable")) {
                        CAIMan_YD_enable();
                    } else if (!strcmp(ptr, "CAIMan_YD_disable")) {
                        CAIMan_YD_disable();
                    } else if (!strcmp(ptr, "CAIMan_ASR_enable")) {
                        CAIMan_ASR_enable(0);
                    } else if (!strcmp(ptr, "CAIMan_ASR_disable")) {
                        CAIMan_ASR_disable();
                    }
                } else if (strncmp(recv_buf, "talksetSearchId:", strlen("talksetSearchId:")) == 0) {
                    char *p = recv_buf + strlen("talksetSearchId:");
                    int val = atoi(p);
                    if (strlen(p) > 0 && val >= 1 && val <= 21) {
                        asr_config_set(NVRAM_ASR_SEARCH_ID, p);
                        asr_config_sync();
                        g_wiimu_shm->asr_search_id = val;
                        AudioCapReset(1);
                    }
                } else if (strncmp(recv_buf, "talkstart:", strlen("talkstart:")) == 0) {
                    int req_from = atoi(recv_buf + strlen("talkstart:"));
                    g_wiimu_shm->times_talk_on++;
#ifdef ENABLE_POWER_MONITOR
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setSystemActive",
                                             strlen("setSystemActive"), NULL, NULL, 0);
#endif
#ifdef MARSHALL_XIAOWEI
                    if (g_wiimu_shm->privacy_mode) {
                        CAIMan_YD_enable();
                        continue;
                    }
                    if (g_wiimu_shm->play_mode == PLAYER_MODE_LINEIN ||
                        g_wiimu_shm->play_mode == PLAYER_MODE_RCA) {
                        CAIMan_YD_enable();
                        continue;
                    }
#endif
#ifdef A98_PHILIPST
                    if (g_wiimu_shm->play_mode == PLAYER_MODE_LINEIN) {
                        CAIMan_YD_enable();
                        continue;
                    }
#endif

                    CleanSilentOTAFlag(g_wiimu_shm);
                    if (ignore_reset_trigger) {
                        if (ignore_reset_trigger) {
                            ignore_reset_trigger = 0;
                        }
                        wiimu_log(1, 0, 0, 0, "ignore_talk_start ==\n");
                        continue;
                    }
                    if (g_wiimu_shm->xiaowei_alarm_start) {
                        xiaowei_stop_alarm();
                    } else {
                        asr_dump_info.record_start_ts = 0;
                        asr_dump_info.record_end_ts = 0;
                        asr_dump_info.vad_ts = 0;
                        asr_dump_info.http_ret_code = 0;
                        do_talkstart(req_from);
                    }
                } else if (strncmp(recv_buf, "talkstop", strlen("talkstop")) == 0) {
                    set_asr_led(1);
                    OnCancel();
                    do_talkstop(ERR_ASR_NO_UNKNOWN);
                } else if (strncmp(recv_buf, "talkcontinue", strlen("talkcontinue")) == 0) {
                    int errCode = 0;
                    int index;
                    char *ptr;
                    if (strstr(recv_buf, "talkcontinue_session:")) {
                        index = getSessionID(recv_buf + strlen("talkcontinue_session:"));
                        if (index != -1 && index != g_asr_index) {
                            continue;
                        }

                        ptr = removeSessionID(recv_buf + strlen("talkcontinue_session:"));
                        errCode = atoi(ptr);
                    } else if (strstr(recv_buf, "talkcontinue:"))
                        errCode = atoi(recv_buf + strlen("talkcontinue:"));

                    do_talkcontinue(errCode);
                } else if (strncmp(recv_buf, "privacy_enable", strlen("privacy_enable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOn", strlen("talksetPrivacyOn") == 0)) {
                    if (!g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 1;
                    // block_voice_prompt("common/mic_off.mp3", 0);
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                             strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "1");
                    asr_config_sync();
                } else if (strncmp(recv_buf, "privacy_disable", strlen("privacy_disable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOff",
                                   strlen("talksetPrivacyOff") == 0)) {
                    if (g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 0;
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                             strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);
                    // block_voice_prompt("common/mic_on.mp3", 0);
                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "0");
                    asr_config_sync();
                } else if (strncmp(recv_buf, "asrResult", strlen("asrResult")) == 0) {
                    char *ptr = recv_buf + strlen("asrResult:0:");

                    if (strstr(recv_buf, "asrResult_session:")) {
                        ptr = recv_buf + strlen("asrResult_session:0:");
                        int index = getSessionID(ptr);
                        if (index != -1 && index != g_asr_index) {
                            continue;
                        }
                        // don't remove the sesion id, parse result will use the session id
                    }
                    CAIMan_ASR_disable();
                    CAIMan_YD_enable();
                    // CAIMan_ASR_parseResult(ptr);
                } else if (strncmp(recv_buf, "change_record_path", strlen("change_record_path")) ==
                           0) {
                    int change_flag = atoi(recv_buf + strlen("change_record_path:"));
                    memset(recv_buf, 0, sizeof(recv_buf));
                    if (1 == change_flag) {
                        if (strlen(g_wiimu_shm->mmc_path) > 0) {
                            if (!asr_dump_info.record_to_sd) {
                                asr_dump_info.record_to_sd = 1;
                            }
                            strcat(recv_buf, "record to SD card!");
                        } else {
                            strcat(recv_buf, "Error: none sd card found");
                        }
                    } else {
                        if (asr_dump_info.record_to_sd) {
                            asr_dump_info.record_to_sd = 0;
                        }
                        strcat(recv_buf, "record to tmp!");
                    }
                    UnixSocketServer_write(&msg_socket, recv_buf, strlen(recv_buf));
                } else if (strncmp(recv_buf, "INTERNET_ACCESS:", strlen("INTERNET_ACCESS:")) == 0) {
                    int flag = atoi(recv_buf + strlen("INTERNET_ACCESS:"));
                    int ret = 0;
                    if (flag == 1) {
                        weixin_device_init();
                        if (g_wiimu_shm->ezlink_ble_setup && g_wiimu_shm->xiaowei_login_flag) {
                            ret = device_get_bind_ticket(weixin_bind_ticket_callback);
                            if (ret) {
                                wiimu_log(1, 0, 0, 0, "%s:device_get_bind_ticket errorcode=%d\n",
                                          __func__, ret);
                            }
                        }
                    }
                } else if (strncmp(recv_buf, "setXiaoweiLogLevel:",
                                   strlen("setXiaoweiLogLevel:")) == 0) {
                    int tmp_debug_level = atoi(recv_buf + strlen("setXiaoweiLogLevel:"));
                    if (tmp_debug_level < 0 || tmp_debug_level > 5) {
                        printf("%s:debug level error value set\n", __func__);
                    } else {
                        g_xiaowei_debug_level = tmp_debug_level;
                    }
                } else if (strncmp(recv_buf, "setXiaoweiWakeupCheckDebug:",
                                   strlen("setXiaoweiWakeupCheckDebug:")) == 0) {
                    int tmp_wakeup_check_debug =
                        atoi(recv_buf + strlen("setXiaoweiWakeupCheckDebug:"));
                    if (tmp_wakeup_check_debug < 0) {
                        printf("%s:setXiaoweiWakeupCheckDebug error value set\n", __func__);
                    } else {
                        g_xiaowei_wakeup_check_debug = tmp_wakeup_check_debug;
                    }
                } else if (strncmp(recv_buf, "xiaoweiTalkstartTest:",
                                   strlen("xiaoweiTalkstartTest:")) == 0) {
                    int tmp_talkstart_test = atoi(recv_buf + strlen("xiaoweiTalkstartTest:"));
                    if (tmp_talkstart_test < 0) {
                        printf("%s:xiaoweiTalkstartTest error value set\n", __func__);
                    } else {
                        g_xiaowei_talkstart_test = tmp_talkstart_test;
                    }
                } else if (strncmp(recv_buf, "USER_REQUEST:", strlen("USER_REQUEST:")) == 0) {
                    int asr_disable = atoi(recv_buf + strlen("USER_REQUEST:"));
                    g_asr_disable = asr_disable;
                    if (!g_record_testing) {
                        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "CAP_RESET",
                                                 strlen("CAP_RESET"), NULL, NULL, 0);
                    }
                } else if (strncmp(recv_buf, "recordTest", strlen("recordTest")) == 0) {
                    int flag = atoi(recv_buf + strlen("recordTest:"));
                    g_record_testing = flag;
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:stop",
                                             strlen("setPlayerCmd:stop"), NULL, NULL, 0);
                    system("killall -9 smplayer");
                    CAIMan_init();
                    if (flag) {
                        g_wiimu_shm->asr_ongoing = 1;
                        StartRecordTest();
                    } else {
                        StopRecordTest();
                        g_wiimu_shm->asr_ongoing = 0;
                    }
                } else if (strncmp(recv_buf, "recordAsrTest", strlen("recordAsrTest")) == 0) {
                    int flag = atoi(recv_buf + strlen("recordAsrTest:"));
                    if (flag == 1) {
                        remove("/tmp/web/dump_asr.pcm");
                        remove("/tmp/web/dump_cap.pcm");
                        remove("/tmp/dump_asr.pcm");
                        remove("/tmp/dump_cap.pcm");
                        sync();
                    }
                    g_record_asr_input = flag;
                    if (flag == 0) {
                        usleep(300000);
                        sync();
                        system("cp /tmp/dump_asr.pcm /tmp/web/dump_asr.pcm");
                        system("cp /tmp/dump_cap.pcm /tmp/web/dump_cap.pcm");
                        sync();
                    }
                } else if (strncmp(recv_buf, "txLogCtrl", strlen("txLogCtrl")) == 0) {
                    int flag = atoi(recv_buf + strlen("txLogCtrl:"));
                    g_tx_log_enable = flag;
                }
                //#if defined(RECORD_MODULE_ENABLE)
                else if (strncmp(recv_buf, "recordFileStart:", strlen("recordFileStart:")) == 0) {
                    char *fileName = (recv_buf + strlen("recordFileStart:"));
                    g_record_testing = 1;

                    if (!IsCCaptStart()) {
                        CAIMan_init();
                    }
                    CCapt_Start_GeneralFile(fileName);
                } else if (strncmp(recv_buf, "recordFileStop", strlen("recordFileStop")) == 0) {
                    char buf[256];
                    memset(buf, 0x0, sizeof(buf));
                    g_record_testing = 0;
                    CCapt_Stop_GeneralFile(buf);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "rmLastRecordFile", strlen("rmLastRecordFile")) == 0) {
                    if (strlen(asr_dump_info.last_rec_file) > 0) {
                        char cmdbuff[256] = {0};
                        sprintf(cmdbuff, "rm -rf %s", asr_dump_info.last_rec_file);
                        system(cmdbuff);
                        printf("rm file %s\r\n", cmdbuff);
                    }
                }
//#endif
#if defined(ENCAMR_MODULE_ENABLE)
                else if (strncmp(recv_buf, "AmrEncStart", strlen("AmrEncStart")) == 0) {

                    char *temp = (char *)malloc(13 + sizeof(ALI_REC_FILE));
                    int type = atoi(recv_buf + strlen("AmrEncStart:"));
                    if (!IsCCaptStart()) {
                        CAIMan_init();
                    }
                    g_ali_rec_file.method = 0; // start
                    strcpy(g_ali_rec_file.format, "amr");
                    if (type == 0) {
                        strcpy(g_ali_rec_file.type, "audio");
                    } else {
                        strcpy(g_ali_rec_file.type, "chat");
                    }

                    strcpy(temp, "ali_rec_start");

                    sprintf(g_ali_rec_file.path, "%s/%lld_capture.amr", g_wiimu_shm->mmc_path,
                            timeMsSince2000());
                    printf("file_path %s \r\n", g_ali_rec_file.path);
                    CCapt_Start_EncAmr(g_ali_rec_file.path);
                    memcpy(temp + 13, &g_ali_rec_file, sizeof(ALI_REC_FILE));
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", temp,
                                             13 + sizeof(ALI_REC_FILE), NULL, NULL, 0);
                    free(temp);
                } else if (strncmp(recv_buf, "AmrEncStop", strlen("AmrEncStop")) == 0) {
                    char *temp = (char *)malloc(11 + sizeof(ALI_REC_FILE));
                    CCapt_Stop_EncAmr();
                    CCapt_Get_Result(g_ali_rec_file.start_time, &g_ali_rec_file.duration,
                                     &g_ali_rec_file.error_code);
                    g_ali_rec_file.method = 1;
                    g_ali_rec_file.file_size = GetFileSizeInStorage(g_ali_rec_file.path);
                    strcpy(temp, "ali_rec_end");
                    memcpy(temp + 11, &g_ali_rec_file, sizeof(ALI_REC_FILE));
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", temp,
                                             11 + sizeof(ALI_REC_FILE), NULL, NULL, 0);
                    free(temp);
                }
#endif
                else if (strncmp(recv_buf, "AmazonPlayRec", strlen("AmazonPlayRec")) == 0) {
                    // system("aplay -r 16000 -c 1 -f S16_LE /tmp/amazon_record.pcm &");
                    system("aplay -r 16000 -c 1 -f S16_LE /tmp/amazon_upload.pcm &");
                } else if (strncmp(recv_buf, "QQLocalCtl:", strlen("QQLocalCtl:")) == 0) {
                    char *ptr = strchr(recv_buf, ':') + 1;
                    if (strncmp(ptr, "next", strlen("next")) == 0) {
                        url_player_set_cmd(url_player, e_player_cmd_next, 0);
                    } else if (strncmp(ptr, "prev", strlen("prev")) == 0) {
                        url_player_set_cmd(url_player, e_player_cmd_prev, 0);
                    }
                } else if (strncmp(recv_buf, "QQCALL:", strlen("QQCALL:")) == 0) {
                    int call_event = atoi(recv_buf + strlen("QQCALL:"));
                    // qq_call_control(call_event);
                } else if (strncmp(recv_buf, "alarm_get_remind", strlen("alarm_get_remind")) == 0) {
                    char remind_voice_id[64];
                    char *pvoice_start, *pclock_start;
                    pvoice_start = strchr(recv_buf, ':') + 1;
                    pclock_start = strchr(pvoice_start, ':') + 1;
                    strncpy(remind_voice_id, pvoice_start, pclock_start - pvoice_start - 1);
                    remind_voice_id[pclock_start - pvoice_start - 1] = '\0';
                    OnCancel();
                    printf("%s,before xiaowei_alarm_get_remind \n", __func__);
                    xiaowei_alarm_get_remind(remind_voice_id, pclock_start);
                    if (g_wiimu_shm->xiaowei_alarm_start) {
                        xiaowei_stop_alarm();
                    }
                    xiaowei_start_alarm();
                } else if (strncmp(recv_buf, "xiaowei_alarm_stop", strlen("xiaowei_alarm_stop")) ==
                           0) {
                    xiaowei_stop_alarm();
                } else if (strncmp(recv_buf, "PlayEventNotify:", strlen("PlayEventNotify:")) == 0) {
                    char *ptr = recv_buf + strlen("PlayEventNotify:");
                    /*if (ptr && (strncmp(ptr, "forcestop", strlen("forcestop")) == 0)) {
                        g_xiaowei_report_state.play_state = play_state_stoped;
                        xiaowei_local_state_report(&g_xiaowei_report_state);
                    }*/
                    // qq_xiaowei_playevent_notification(ptr);
                } else if (strncmp(recv_buf, "VoiceLinkStart", strlen("VoiceLinkStart")) == 0) {
                    // xiaowei_start_voiceLink();
                } else if (strncmp(recv_buf, "VoiceLinkStop", strlen("VoiceLinkStop")) == 0) {
                    // xiaowei_stop_voiceLink();
                } else if (strncmp(recv_buf, "setFactoryTXQQID:", strlen("setFactoryTXQQID:")) ==
                           0) {
                    char buf[256];
                    char *str = recv_buf + strlen("setFactoryTXQQID:");
                    char *pid = NULL;
                    char *guid = NULL;
                    char *license = NULL;
                    char *appid = NULL;
                    char *key_ver = NULL;
                    int failed = 0;

                    sprintf(buf, "%s", "Failed");

                    pid = str;
                    str = strstr(str, ":");
                    if (str) {
                        str[0] = '\0';
                        str++;
                        guid = str;
                        str = strstr(str, ":");
                        if (str) {
                            str[0] = '\0';
                            str++;
                            appid = str;
                            str = strstr(str, ":");
                            if (str) {
                                str[0] = '\0';
                                str++;
                                license = str;
                                str = strstr(str, ":");
                                if (str) {
                                    str[0] = '\0';
                                    str++;
                                    key_ver = str;
                                    if ((strlen(license) < TXQQ_DEFAULT_LICENSE_LEN) &&
                                        (strlen(guid) < TXQQ_DEFAULT_GUID_LEN) &&
                                        (strlen(appid) < TXQQ_DEFAULT_APPID_LEN) &&
                                        (strlen(pid) < TXQQ_DEFAULT_PID_LEN) &&
                                        (strlen(key_ver) < TXQQ_DEFAULT_KEY_VER_LEN)) {
                                        strcpy(txqq_licence, license);
                                        strcpy(txqq_guid, guid);
                                        strcpy(txqq_product_id, pid);
                                        strcpy(txqq_appid, appid);
                                        strcpy(txqq_key_ver, key_ver);
                                        strcpy(g_wiimu_shm->txqq_guid, txqq_guid);
                                        strcpy(g_wiimu_shm->txqq_product_id, txqq_product_id);
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_pid", pid) !=
                                            0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_license",
                                                                license) != 0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_guid", guid) !=
                                            0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_appid",
                                                                appid) != 0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_key_ver",
                                                                key_ver) != 0) {
                                            failed |= 1;
                                        }
                                    } else {
                                        failed = 1;
                                    }
                                    if (!failed)
                                        sprintf(buf, "%s", "Success");
                                }
                            }
                        }
                    }
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "getFactoryTXQQID", strlen("getFactoryTXQQID")) == 0) {
                    char buf[512];

                    snprintf(buf, 512, "{\"pid\":\"%s\",\"guid\":\"%s\",\"license\":\"%s\","
                                       "\"appid\":\"%s\",\"key_ver\":\"%s\"}",
                             txqq_product_id, txqq_guid, txqq_licence, txqq_appid, txqq_key_ver);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "setTXQQQuality:", strlen("setTXQQQuality:")) == 0) {
                    char buf[256];
                    char *str = recv_buf + strlen("setTXQQQuality:");
                    int failed = 0;

                    sprintf(buf, "%s", "Failed");

                    // txqq_quality = atoi(str);
                    if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_quality", str) != 0) {
                        failed |= 1;
                    }

                    if (!failed) {
                        // tx_ai_audio_set_quality(txqq_quality);
                        sprintf(buf, "%s", "Success");
                    }
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "getTXQQQuality", strlen("getTXQQQuality")) == 0) {
                    char buf[512];

                    // sprintf(buf, "{\"quality\":\"%d\"}", txqq_quality);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "ttsEndNotify", strlen("ttsEndNotify")) == 0) {
                    int finish = atoi(recv_buf + strlen("ttsEndNotify:"));
                    // xiaowei_tts_end_notification(finish);
                } else if (strncmp(recv_buf, "query_xiaowei_ota_status",
                                   sizeof("query_xiaowei_ota_status")) == 0) {
                    int res = 0;
                    res = device_query_ota_update(NULL, 1, 3, get_xiaowei_ota_flag());
                    g_silence_ota_query = 1;
                    wiimu_log(1, 0, 0, 0, "device_query_ota_update return=%d\n", res);
                } else if (strncmp(recv_buf, "xiaowei_other_mode_state_report",
                                   sizeof("xiaowei_other_mode_state_report")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                } else if (strncmp(recv_buf, "RESTOREFACTORY", sizeof("RESTOREFACTORY")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(url_player, e_player_cmd_stop, 0);
                    device_erase_all_binders();
                } else if (strncmp(recv_buf, "quit_wifi_playing_mode",
                                   sizeof("quit_wifi_playing_mode")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(url_player, e_player_cmd_clear_queue, 0);
                } else if (strncmp(recv_buf, "enter_wps_setup_mode",
                                   sizeof("enter_wps_setup_mode")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(url_player, e_player_cmd_stop, 0);
                } else if (strncmp(recv_buf, "setDisableForceOTA", strlen("setDisableForceOTA")) ==
                           0) {
                    int flag = atoi(recv_buf + strlen("setDisableForceOTA:"));
                    if (flag) {
                        asr_config_set(NVRAM_ASR_DIS_FORCE_OTA, "1");
                    } else {
                        asr_config_set(NVRAM_ASR_DIS_FORCE_OTA, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "setDisableAllOTA", strlen("setDisableAllOTA")) == 0) {
                    int flag = atoi(recv_buf + strlen("setDisableAllOTA:"));
                    if (flag) {
                        asr_config_set(NVRAM_ASR_DIS_ALL_OTA, "1");
                    } else {
                        asr_config_set(NVRAM_ASR_DIS_ALL_OTA, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "setXiaoweiDebugOTA", strlen("setXiaoweiDebugOTA")) ==
                           0) {
                    int flag = atoi(recv_buf + strlen("setXiaoweiDebugOTA:"));
                    if (2 == flag) {
                        asr_config_set(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, "2");
                    } else {
                        asr_config_set(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "alexadump", strlen("alexadump")) == 0) {
                    memset(recv_buf, 0, sizeof(recv_buf));
                    asr_dump_refresh(&msg_socket, recv_buf);
                } else if (strncmp(recv_buf, "talkTriggerTestStart",
                                   strlen("talkTriggerTestStart")) == 0) {
                    g_wiimu_shm->asr_trigger_test_on = 1;
                } else if (strncmp(recv_buf, "talkTriggerTestStop",
                                   strlen("talkTriggerTestStop")) == 0) {
                    g_wiimu_shm->asr_trigger_test_on = 0;
                } else if (strncmp(recv_buf, "ttsStop", sizeof("ttsStop")) == 0) {
                    // if(g_wiimu_shm->asr_tts_start == 1) {
                    // pcm_audio_cancel();
                    // g_wiimu_shm->asr_tts_start = 0;
                    //}
                } else if (strncmp(recv_buf, "NetworkDHCPNewIP", strlen("NetworkDHCPNewIP")) == 0) {
#ifdef AMLOGIC_A113
                    res_init();
#endif
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

    pthread_exit("exit udpEvt thread");
}
#if 0
void xiaoweiVoicReq(void)
{
    char buf[4800];
    FILE *f = fopen("/system/workdir/misc/music.wav", "rb+");
    fseek(f, 46, 0);
    size_t read = 0;

    char voice_id[33] = {0};
    XIAOWEI_PARAM_CONTEXT context = {0};
    context.silent_timeout = 500;
    context.speak_timeout = 5000;
    context.voice_request_begin = true;
    context.voice_request_end = false;

    printf("voice request begin");

    XIAOWEI_PARAM_PROPERTY property = xiaowei_param_local_vad;//本地静音检测

    context.voice_request_begin = true;//第一个语音包begin要是true，在这之前也可以选择性调stop_request
    context.request_param = property;
    read = fread(buf, 1, 3200, f);

    xiaowei_request(voice_id, xiaowei_chat_via_voice, buf, (int)read, &context);
    context.voice_request_begin = false;

    int i = 0;
    while(1) { //正常收到静音就无需再发送了，这里先不管
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long starttime = tv.tv_usec / 1000 + tv.tv_sec * 1000;
        read = fread(buf, 1, 1024, f);
        if(read == 0) break;
        printf("send voice request data[%d], result = %d\n", i, xiaowei_request(voice_id, xiaowei_chat_via_voice, buf,
                (int)read, &context));
        gettimeofday(&tv, NULL);
        long endtime = tv.tv_usec / 1000 + tv.tv_sec * 1000;
//        printf("testsend over:%d...end time = %ld",i,endtime);
//        printf("***testsend[%d]: starttime = %ld, end time = %ld, timecost = %ld \n***",i,starttime, endtime, endtime - starttime);
        usleep(1000 * 50);
        i++;
    }
    //假装发完了，按了按钮，这时候发一个静音包，这个包里面可以有语音数据，也可以没有
    context.voice_request_end = true;
    char bufEmpyt[3200] = {0};
    xiaowei_request(voice_id, xiaowei_chat_via_voice, bufEmpyt, 3200, &context);
    printf("voice request end\n");
    fclose(f);
}
#endif

DEVICE_OTA_RESULT ota_callback;
void on_ota_check_result(int err_code, const char *bussiness_id, bool new_version,
                         OTA_UPDATE_INFO *update_info, OTA_PACKAGE_INFO *package_info)
{
    wiimu_log(1, 0, 0, 0, "------%s--err=%d,buss_id=%s,new_version=%d----\n", __func__, err_code,
              bussiness_id, new_version);
    if (update_info) {
        wiimu_log(1, 0, 0, 0, "updateinfo:deviversion=%d,extra_info=%s,update_level=%d,update_time="
                              "%llu,update_tips=%s,version=%llu,version_name=%s\n",
                  update_info->dev_flag, update_info->extra_info, update_info->update_level,
                  update_info->update_time, update_info->update_tips, update_info->version,
                  update_info->version_name);
    }
    if (package_info) {
        if (package_info->package_url && strlen(package_info->package_url) > 0) {
            fprintf(stderr, "%s packageinfo:hash=%s,hashtype=%d,package size=%llu,url=%s\n",
                    __func__, package_info->package_hash, package_info->package_hash_type,
                    package_info->package_size, package_info->package_url);
            char buf[1024] = {0};
            if (update_info && update_info->extra_info) {
                if (all_ota_disabled() == 0) {
                    if ((force_ota_disabled() == 0) || (g_silence_ota_query == 1)) {
                        if (g_silence_ota_query) {
                            g_silence_ota_query = 0;
                        }
                        if (strstr(update_info->extra_info, "DSP.")) {
                            char *tmp_dsp_str = NULL;
                            char *tmp_dsp_ver = NULL;
                            tmp_dsp_str = strstr(update_info->extra_info, "DSP.");
                            tmp_dsp_ver = tmp_dsp_str + strlen("DSP.");
                            if (tmp_dsp_ver) {
                                snprintf(g_wiimu_shm->asr_dsp_fw_newver, 32, "%s", tmp_dsp_ver);
                                printf("%s:dsp_ver=%s,local_dsp_ver=%s,dsp_ver_new=%s\n", __func__,
                                       tmp_dsp_ver, g_wiimu_shm->asr_dsp_fw_ver,
                                       g_wiimu_shm->asr_dsp_fw_newver);
                            }
                        }
                        if (strncmp(update_info->extra_info, g_wiimu_shm->firmware_ver,
                                    strlen(g_wiimu_shm->firmware_ver))) {
                            printf("%s:before send ota cmd\n", __func__);
                            if (1) {
                                sprintf(buf, "StartUpdate:%s", package_info->package_url);
                            } else {
                                sprintf(buf, "SetAllInOneUpdateUrl:%s", package_info->package_url);
                            }
                            SocketClientReadWriteMsg("/tmp/a01remoteupdate", buf, strlen(buf), NULL,
                                                     NULL, 0);
                        }
                    }
                }
            }
        }
    }
}

void on_ota_download_progress(const char *url, double current, double total)
{
    wiimu_log(1, 0, 0, 0, "------%s------\n", __func__);
}

void on_ota_download_error(const char *url, int err_code)
{
    wiimu_log(1, 0, 0, 0, "------%s------\n", __func__);
}

void on_ota_download_complete(const char *url, const char *file_path)
{
    wiimu_log(1, 0, 0, 0, "------%s------\n", __func__);
}

int main(int argc, const char *argv[])
{
    int res;
    char buf[256] = {0};
    pthread_t a_thread;
    pthread_t alarm_thread;
    g_wiimu_shm = WiimuContextGet();

    signal(SIGINT, sigroutine);
    signal(SIGTERM, sigroutine);
    signal(SIGKILL, sigroutine);
    signal(SIGALRM, sigroutine);
    signal(SIGQUIT, sigroutine);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    system("mkdir -p /vendor/weixin");
    setLogFunc();

    if (lpcfg_init() != 0) {
        printf("Fatal: asr_tts failed to init lpcfg\n");
        return -1;
    }
#if defined(SOFT_PREP_MODULE)
    if (!CAIMan_soft_prep_init()) {
        printf("Soft PreProcess module init fail\n");
        return -1;
    }
#endif

    g_wiimu_shm->asr_ongoing = 0;
    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);

// init_asr_feature();

#ifdef EXPIRE_CHECK_ENABLE
    extern int run_expire_check_thread(char *process, int expire_days, int failed_flag,
                                       char *command, int interval);
    run_expire_check_thread("asr_tts", EXPIRE_DAYS, 3, NULL, 30);
#endif
    xiaoweiRingBufferInit();

    CAIMan_init();
    CAIMan_YD_start();
    CAIMan_ASR_start();
    CAIMan_YD_enable();
    set_asr_led(1);

    get_qqid_info();
    xiaowei_url_player_init();
    xiaowei_player_init();
    res = pthread_create(&a_thread, NULL, evtSocket_thread, NULL);
    res = pthread_create(&alarm_thread, NULL, xiaowei_alarm_thread, NULL);
    if (1 == g_wiimu_shm->internet) {
        weixin_device_init();
    }
    while (g_wiimu_shm->xiaowei_login_flag == 0)
        usleep(10000);

    wiimu_log(1, 0, 0, 0, "%s:before config_ota\n", __func__);
    ota_callback.on_ota_check_result = on_ota_check_result;
    ota_callback.on_ota_download_complete = on_ota_download_complete;
    ota_callback.on_ota_download_error = on_ota_download_error;
    ota_callback.on_ota_download_progress = on_ota_download_progress;
    device_config_ota(&ota_callback);
    wiimu_log(1, 0, 0, 0, "%s:after config_ota\n", __func__);
    weixinStart();
    res = device_query_ota_update(NULL, 1, 3, get_xiaowei_ota_flag());
    wiimu_log(1, 0, 0, 0, "device_query_ota_update return=%d\n", res);
    getDeviceSDKVer();
    xiaowei_set_music_quality(3);
    while (1) {
        // xiaoweiTextReq();
        sleep(100);
    }

    return 0;
}
