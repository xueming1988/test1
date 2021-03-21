
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "asr_cfg.h"
#include "debug_test.h"
#include "wm_util.h"

#define ASR_UPLOAD_FILE "/tmp/web/asr_upload.pcm"
#define ASR_RECORD_FILE "/tmp/web/asr_record.pcm"

static int g_record_testing = 0;
extern WIIMU_CONTEXT *g_wiimu_shm;

extern void CAIMan_init(void);
extern int IsCCaptStart(void);
extern void CCapt_Start_GeneralFile(char *fileName);
extern void CCapt_Stop_GeneralFile(char *fileName);
extern int CAIMan_YD_enable(void);
extern void do_record_reset(int enable);
extern void StartRecordTest(void);
extern void StopRecordTest(void);
extern void StartRecordServer(int nChannel);
extern void StopRecordServer(void);
extern void killPidbyName(char *sPidString);
#if defined(SOFT_PREP_HOBOT_MODULE)
extern int CAIMan_soft_prep_privRecord(int status, int source, char *path);
#endif

static size_t get_file_len(const char *path)
{
    struct stat buf;
    if (stat(path, &buf))
        return 0;
    return buf.st_size;
}

static int web_dump_refresh(SOCKET_CONTEXT *pmsg_socket, char *recv_buf, size_t len)
{
    char record_file[128] = {0};
    char upload_file[128] = {0};
    char cmdline[1024] = {0};
    char request_para[512] = {0};
    int record_len = 0;
    int upload_len = 0;

    snprintf(record_file, sizeof(record_file), "%s_v%d_%s", g_wiimu_shm->project_name,
             g_wiimu_shm->mcu_ver, "record.pcm");
    snprintf(upload_file, sizeof(upload_file), "%s_v%d_%s", g_wiimu_shm->project_name,
             g_wiimu_shm->mcu_ver, "upload.pcm");

    memset(cmdline, 0x0, sizeof(cmdline));
    snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", ASR_RECORD_FILE,
             record_file);
    system(cmdline);
    record_len = get_file_len(ASR_RECORD_FILE);

    memset(cmdline, 0x0, sizeof(cmdline));
    snprintf(cmdline, sizeof(cmdline), "ln -s %s /tmp/web/%s 2>/dev/null", ASR_UPLOAD_FILE,
             upload_file);
    system(cmdline);
    upload_len = get_file_len(ASR_UPLOAD_FILE);

    snprintf(recv_buf, len, "DebugDump: <hr>"
                            "record_len=%d upload_len=%d on_line=%d<hr>",
             record_len, upload_len, g_wiimu_shm->alexa_online);

    snprintf(recv_buf, len, "%sasr_record.pcm &nbsp;<a href=data/%s>download</a><hr>", recv_buf,
             record_file);
    snprintf(recv_buf, len, "%sasr_upload.pcm &nbsp;<a href=data/%s>download</a><hr>", recv_buf,
             upload_file);

    snprintf(request_para, sizeof(request_para), "asr_onginig=%d "
                                                 "privacy_mode=%d "
                                                 "times_talk_on=%d "
                                                 "times_trigger1=%d "
                                                 "times_trigger2=%d "
                                                 "times_request_succeed=%d<hr> "
                                                 "times_request_failed=%d "
                                                 "times_vad_time_out=%d "
                                                 "times_play_failed=%d "
                                                 "times_open_failed=%d "
                                                 "times_capture_failed=%d "
                                                 "score=%d "
                                                 "status:%d<hr>",
             g_wiimu_shm->asr_ongoing, g_wiimu_shm->privacy_mode, g_wiimu_shm->times_talk_on,
             g_wiimu_shm->times_trigger1, g_wiimu_shm->times_trigger2,
             g_wiimu_shm->times_request_succeed, g_wiimu_shm->times_request_failed,
             g_wiimu_shm->times_vad_time_out, g_wiimu_shm->times_play_failed,
             g_wiimu_shm->times_open_failed, g_wiimu_shm->times_capture_failed,
             g_wiimu_shm->wakeup_score, g_wiimu_shm->asr_last_status);
    strcat(recv_buf, request_para);
    strcat(recv_buf, "<a href=httpapi.asp?command=alexadump target=_self>Refresh</a><br>");
    UnixSocketServer_write(pmsg_socket, recv_buf, strlen(recv_buf));
    return 0;
}

int debug_mode_on(void)
{
    int ret = 0;

    if (g_wiimu_shm->asr_test_mode || g_wiimu_shm->asr_trigger_test_on ||
        g_wiimu_shm->asr_trigger_mis_test_on || g_record_testing != 0) {
        ret = 1;
    }

    return ret;
}

int debug_cmd_parse(SOCKET_CONTEXT *msg_socket, char *recv_buf, size_t len)
{
    if (strncmp(recv_buf, "talkTriggerTestStart", strlen("talkTriggerTestStart")) == 0) {
        g_wiimu_shm->asr_trigger_test_on = 1;
        g_wiimu_shm->asr_trigger_mis_test_on = 0;
        g_wiimu_shm->asr_yd_recording = 1;
    } else if (strncmp(recv_buf, "talkTriggerTestStop", strlen("talkTriggerTestStop")) == 0) {
        g_wiimu_shm->asr_trigger_test_on = 0;
    } else if (strncmp(recv_buf, "talkTriggerMisTestStart", strlen("talkTriggerMisTestStart")) ==
               0) {
        g_wiimu_shm->asr_trigger_test_on = 0;
        g_wiimu_shm->asr_trigger_mis_test_on = 1;
        g_wiimu_shm->asr_yd_recording = 1;
    } else if (strncmp(recv_buf, "talkTriggerMisTestStop", strlen("talkTriggerMisTestStop")) == 0) {
        g_wiimu_shm->asr_trigger_mis_test_on = 0;
    } else if (strncmp(recv_buf, "talksetTriggerScore", strlen("talkSetTriggerScore")) == 0) {
        int flag = atoi(recv_buf + strlen("talkSetTriggerScore:"));
        if (flag > 100 && flag < 1000)
            g_wiimu_shm->asr_yd_config_score = flag;
    } else if (strncmp(recv_buf, "talksetTriggerInterval", strlen("talkSetTriggerInterval")) == 0) {
        int flag = atoi(recv_buf + strlen("talkSetTriggerInterval:"));
        if (flag > 20 && flag < 180)
            g_wiimu_shm->asr_yd_test_interval = flag;
    } else if (strncmp(recv_buf, "talksetSearchId2:", strlen("talksetSearchId2:")) == 0) {
        char *p = recv_buf + strlen("talksetSearchId2:");
        int val = atoi(p);
        if (strlen(p) > 0 && val >= 1 && val <= 20) {
            asr_config_set(NVRAM_ASR_SEARCH_ID2, p);
            asr_config_sync();
            g_wiimu_shm->asr_search_id2 = val;
            do_record_reset(1);
        }
    } else if (strncmp(recv_buf, "talksetSearchId:", strlen("talksetSearchId:")) == 0) {
        char *p = recv_buf + strlen("talksetSearchId:");
        int val = atoi(p);
        if (strlen(p) > 0 && val >= 1 && val <= 20) {
            asr_config_set(NVRAM_ASR_SEARCH_ID, p);
            asr_config_sync();
            g_wiimu_shm->asr_search_id = val;
            do_record_reset(1);
        }
    } else if (strncmp(recv_buf, "talksetKeywordIndex:", strlen("talksetKeywordIndex:")) == 0) {
        char *p = recv_buf + strlen("talksetKeywordIndex:");
        int val = atoi(p);
        if (strlen(p) > 0 && val >= 0 && val <= 20 && g_wiimu_shm->trigger1_keyword_index != val) {
            set_asr_trigger1_keyword_index(val);
            do_record_reset(1);
        }
    } else if (strncmp(recv_buf, "talksetChannelAsr:", strlen("talksetChannelAsr:")) == 0) {
        char *p = recv_buf + strlen("talksetChannelAsr:");
        int val = atoi(p);
        asr_config_set(NVRAM_ASR_CH_ASR, p);
        asr_config_sync();
        g_wiimu_shm->asr_ch_asr = val;
    } else if (strncmp(recv_buf, "talksetChannelTrigger:", strlen("talksetChannelTrigger:")) == 0) {
        char *p = recv_buf + strlen("talksetChannelTrigger:");
        int val = atoi(p);
        asr_config_set(NVRAM_ASR_CH_TRIGGER, p);
        asr_config_sync();
        g_wiimu_shm->asr_ch_trigger = val;
    } else if (strncmp(recv_buf, "talksetPrompt:", strlen("talksetPrompt:")) == 0) {
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
    } else if (strncmp(recv_buf, "recordFileStart:", strlen("recordFileStart:")) == 0) {
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
        UnixSocketServer_write(msg_socket, buf, strlen(buf));
        CAIMan_YD_enable();
    } else if (strncmp(recv_buf, "alexadump", strlen("alexadump")) == 0) {
        web_dump_refresh(msg_socket, recv_buf, len);
    } else if (strncmp(recv_buf, "startRecordServer", strlen("startRecordServer")) == 0) {
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
#if defined(SOFT_PREP_HOBOT_MODULE)
    // hobotStartCAP:0:path    0:44.1K source  1:processed
    else if (strncmp(recv_buf, "hobotStartCAP:", strlen("hobotStartCAP:")) == 0) {
        char *tmp = NULL;
        char *path = NULL;
        int source = atoi((recv_buf + strlen("hobotStartCAP:")));
        printf("source = %d\n", source);
        tmp = recv_buf + strlen("hobotStartCAP:");
        path = strstr(tmp, ":");
        // status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
        CAIMan_soft_prep_privRecord(1, source, path + 1);
        UnixSocketServer_write(msg_socket, "OK", 2);
        // status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
    } else if (strncmp(recv_buf, "hobotStopCAP", strlen("hobotStopCAP")) == 0) {
        CAIMan_soft_prep_privRecord(0, 0, NULL);
        UnixSocketServer_write(msg_socket, "OK", 2);
    }
#endif

    return 0;
}
