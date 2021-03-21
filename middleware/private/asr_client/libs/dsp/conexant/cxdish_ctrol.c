
#include <sys/ioctl.h>
#include <pthread.h>
#include "cxdish_ctrol.h"

enum {
    CMD_IDX_GET_ADC_GAIN0 = 0,
    CMD_IDX_GET_ADC_GAIN1,
    CMD_IDX_SET_ADC_GAIN0,
    CMD_IDX_SET_ADC_GAIN1,

    CMD_IDX_GET_MIC_BOOST0,
    CMD_IDX_GET_MIC_BOOST1,
    CMD_IDX_SET_MIC_BOOST0,
    CMD_IDX_SET_MIC_BOOST1,

    CMD_IDX_OUTPUT_TO_REF,
    CMD_IDX_OUTPUT_TO_MIC,
    CMD_IDX_OUTPUT_TO_PROCESSED,

    CMD_IDX_SSP_DISABLE,
    CMD_IDX_SSP_ENABLE,

    CMD_IDX_GET_DMIC_GAIN0,
    CMD_IDX_GET_DMIC_GAIN1,
    CMD_IDX_GET_DMIC_GAIN2,
    CMD_IDX_GET_DMIC_GAIN3,

    CMD_IDX_SET_DMIC_GAIN0,
    CMD_IDX_SET_DMIC_GAIN1,
    CMD_IDX_SET_DMIC_GAIN2,
    CMD_IDX_SET_DMIC_GAIN3,

    CMD_IDX_GET_ANGLE,

    CMD_IDX_SET_DRC_GAIN_FUN,
    CMD_IDX_GET_DRC_GAIN_FUN,

    CMD_IDX_AGC_ENABLE,
    CMD_IDX_AGC_DISABLE,

    CMD_IDX_SET_MIC_DISTANCE,
    CMD_IDX_GET_MIC_DISTANCE,

    CMD_IDX_SET_NOISE_GATING,

    CMD_IDX_GET_DSP_SERIAL,

    CMD_IDX_SET_SWITCH_MODE_VOIP,
    CMD_IDX_SET_SWITCH_MODE_ASR
};

#define CXDISH_COMMAND_LEN (512)
#define CXDISH_TMP_FILE ("/tmp/cxdish_tmp")
#define CXDISH_ADC_GAIN_FILE ("/tmp/cxdish_ADC_Gain")
#define CXDISH_GET_MODE ("cxdish -r 148 -D /dev/i2cM0 get-mode > /tmp/cxdish_tmp")

#define CXDISH_CMD_GET_ANGLE ("cxdish get-angle")
#define CXDISH_EXEC ("cxdish sendcmd")

#define CMD_SET_MIC_BOOST0 ("0xB72D3300 0x00000030 0x0000000e")
#define CMD_SET_MIC_BOOST1 ("0xB72D3300 0x00000030 0x0000000f")
#define CMD_GET_MIC_BOOST0 ("0xB72D3300 0x00000130 0x0000000e")
#define CMD_GET_MIC_BOOST1 ("0xB72D3300 0x00000130 0x0000000f")

#define CMD_SET_ADC_GAIN0 ("0xB72D3300 0x00000030 0x00000006")
#define CMD_SET_ADC_GAIN1 ("0xB72D3300 0x00000030 0x00000007")
#define CMD_GET_ADC_GAIN0 ("0xB72D3300 0x00000130 0x00000006")
#define CMD_GET_ADC_GAIN1 ("0xB72D3300 0x00000130 0x00000007")

// changed from 6.47.0.0
#define CXDISH_CMD_V2_BASE (6470000LL)
#define CMD_SET_MIC_BOOST0_V2 ("0xB72D3300 0x00000083 0x0000000e")
#define CMD_SET_MIC_BOOST1_V2 ("0xB72D3300 0x00000083 0x0000000f")
#define CMD_GET_MIC_BOOST0_V2 ("0xB72D3300 0x00000183 0x0000000e")
#define CMD_GET_MIC_BOOST1_V2 ("0xB72D3300 0x00000183 0x0000000f")

#define CMD_SET_ADC_GAIN0_V2 ("0xB72D3300 0x00000083 0x00000006")
#define CMD_SET_ADC_GAIN1_V2 ("0xB72D3300 0x00000083 0x00000007")
#define CMD_GET_ADC_GAIN0_V2 ("0xB72D3300 0x00000183 0x00000006")
#define CMD_GET_ADC_GAIN1_V2 ("0xB72D3300 0x00000183 0x00000007")

#define CMD_GET_DMIC_GAIN0 ("0xB72D3300 0x0000012F 0x00000000 00000005")
#define CMD_GET_DMIC_GAIN1 ("0xB72D3300 0x0000012F 0x00000000 00000006")
#define CMD_GET_DMIC_GAIN2 ("0xB72D3300 0x0000012F 0x00000001 00000005")
#define CMD_GET_DMIC_GAIN3 ("0xB72D3300 0x0000012F 0x00000001 00000006")
#define CMD_SET_DMIC_GAIN0 ("0xB72D3300 0x0000002F 0x00000000 00000005")
#define CMD_SET_DMIC_GAIN1 ("0xB72D3300 0x0000002F 0x00000000 00000006")
#define CMD_SET_DMIC_GAIN2 ("0xB72D3300 0x0000002F 0x00000001 00000005")
#define CMD_SET_DMIC_GAIN3 ("0xB72D3300 0x0000002F 0x00000001 00000006")

#define CMD_GET_DMIC_SERIAL ("0xB32D2300 0x00000187")
#define CMD_GET_DMIC_GET_ANGEL ("0xD3086339 0x00000140 0x")

#define CMD_SSP_DISABLE ("0xD308631E 0x00000007 0x00000000")
#define CMD_SSP_ENABLE ("0xD308631E 0x00000007 0x00000001")
#define CMD_SET_MODE_ZSH2 ("0xB32D2300 0x00000004 0x4a8cfa00")

#define CMD_OUTPUT_TO_REF                                                                          \
    ("0xD308632C 0x00000040 0x000002c0 000002c1 00000000 00000000 00000000 00000000 00000000 "     \
     "00000000")
#define CMD_OUTPUT_TO_MIC                                                                          \
    ("0xD308632C 0x00000040 0x000001c0 000001c1 00000000 00000000 00000000 00000000 00000000 "     \
     "00000000")
#define CMD_OUTPUT_TO_PROCESSED                                                                    \
    ("0xD308632C 0x00000040 0x000000c0 000000c0 00000000 00000000 00000000 00000000 00000000 "     \
     "00000000")

#define CMD_SET_DRC_GAIN_FUNC ("0xD3086329 0x00000041")

#define CMD_AGC_DISABLE ("0xD3086324 0x00000007 0x00000000")
#define CMD_AGC_ENABLE ("0xD3086324 0x00000007 0x00000000")

#define CMD_SET_MIC_DISTANCE ("0xD308631E 0x00000057")
#define CMD_GET_MIC_DISTANCE ("0xD308631E 0x00000157")

#define CMD_SET_NOISE_GATING_0                                                                     \
    "0xD308631E 0x00000048 0x19999900 7ffcb900 7ffcb900 0ccccc00 00800000 0147ae00 01000000 "      \
    "02800000"
#define CMD_SET_NOISE_GATING_1                                                                     \
    "0xD308631E 0x00000048 0x19999900 26666600 26666600 0ccccc00 00800000 0147ae00 01000000 "      \
    "02800000"
#define CMD_SET_NOISE_GATING_2                                                                     \
    "0xD308631E 0x00000048 0x19999900 19999900 19999900 0ccccc00 00800000 0147ae00 01000000 "      \
    "02800000"
#define CMD_SET_NOISE_GATING_3                                                                     \
    "0xD308631E 0x00000048 0x19999900 0ccccc00 0ccccc00 0ccccc00 00800000 0147ae00 01000000 "      \
    "02800000"
#define CMD_SET_NOISE_GATING_4                                                                     \
    "0xD308631E 0x00000048 0x19999900 06666600 00000000 0ccccc00 00800000 0147ae00 01000000 "      \
    "02800000"
#define CMD_SET_SWITCH_MODE_VOIP "0xB32D2300 0x00000004 0x4b3dba00"
#define CMD_SET_SWITCH_MODE_ASR "0xB32D2300 0x00000004 0xe2ccfa00"

#define ADC_GAIN_DEFAULT_VAL 0x288
#define ADC_GAIN_PLAYBACK_DEFAULT_VAL 0x258
#define MIC_DISTANCE_MIN 30
#define MIC_DISTANCE_MAX 120

extern WIIMU_CONTEXT *g_wiimu_shm;

int mic_channel_test_mode = 0;

static pthread_mutex_t g_boost_mutex = PTHREAD_MUTEX_INITIALIZER;

#define boost_mutex_lock() pthread_mutex_lock(&g_boost_mutex)
#define boost_mutex_unlock() pthread_mutex_unlock(&g_boost_mutex)

/*
* version format a.b.c.d
*   a.b = a.b.0.0
*   a.b.c = a.b.c.0
*/
static int get_cxdish_command_version(void)
{
// old versions before 6.47.x does not supported
#if 1
    return 2;
#else
    long long ver = 0;
    char *p = (char *)g_wiimu_shm->asr_dsp_fw_ver;
    long long min_ver = 0;
    long long multi = 100 * 100 * 100;
    while (*p != '\0') {
        if (*p == '.') {
            ver = ver + min_ver * multi;
            min_ver = 0;
            multi = multi / 100;
        } else if (*p >= '0' && *p <= '9') {
            min_ver = min_ver * 10 + (*p - '0');
        }

        ++p;
        if (*p == '\0') {
            ver += min_ver;
            break;
        }
    }
    return (ver > CXDISH_CMD_V2_BASE || ver == 0) ? 2 : 0;
#endif
}

// 0:succeed of else failed
int cxdish_log_out(char **des)
{
    FILE *fp;
    char tmp_buf[128] = {0};
    if ((access(CXDISH_TMP_FILE, 0)) != 0) // not exist
        return -1;

    if ((fp = fopen(CXDISH_TMP_FILE, "r")) == NULL) {
        return -1;
    }
    if (fgets(tmp_buf, sizeof(tmp_buf), fp) == NULL) {
        if (feof(fp) != 0) { // get the end of file
        } else {
            fclose(fp);
            return -1;
        }
    }
    if (strlen(tmp_buf)) {
        *des = malloc(strlen(tmp_buf) + 1);
        memset(*des, 0x0, strlen(tmp_buf) + 1);
        if (tmp_buf[strlen(tmp_buf) - 1] == '\n')
            tmp_buf[strlen(tmp_buf) - 1] = 0;
        strncpy(*des, tmp_buf, strlen(tmp_buf));
        fclose(fp);
        return 0;
    } else {
        fclose(fp);
        return -1;
    }
    return -1;
}

static int cxdish_run(int cmd, unsigned int val, unsigned int *result)
{
    char buf[1024];
    char *p_outbuf = NULL;
    char *p_cmd = NULL;

    // if (!far_field_judge(g_wiimu_shm))
    //  return -1;

    memset((void *)buf, 0, sizeof(buf));
    switch (cmd) {
    case CMD_IDX_GET_ADC_GAIN0:
    case CMD_IDX_GET_ADC_GAIN1:
    case CMD_IDX_GET_MIC_BOOST0:
    case CMD_IDX_GET_MIC_BOOST1:
    case CMD_IDX_GET_DMIC_GAIN0:
    case CMD_IDX_GET_DMIC_GAIN1:
    case CMD_IDX_GET_DMIC_GAIN2:
    case CMD_IDX_GET_DMIC_GAIN3:
    case CMD_IDX_GET_DSP_SERIAL:
    case CMD_IDX_GET_ANGLE:
        if (cmd == CMD_IDX_GET_ADC_GAIN0)
            p_cmd = get_cxdish_command_version() >= 2 ? CMD_GET_ADC_GAIN0_V2 : CMD_GET_ADC_GAIN0;
        else if (cmd == CMD_IDX_GET_ADC_GAIN1)
            p_cmd = get_cxdish_command_version() >= 2 ? CMD_GET_ADC_GAIN1_V2 : CMD_GET_ADC_GAIN1;
        else if (cmd == CMD_IDX_GET_MIC_BOOST0)
            p_cmd = get_cxdish_command_version() >= 2 ? CMD_GET_MIC_BOOST0_V2 : CMD_GET_MIC_BOOST0;
        else if (cmd == CMD_IDX_GET_MIC_BOOST1)
            p_cmd = get_cxdish_command_version() >= 2 ? CMD_GET_MIC_BOOST1_V2 : CMD_GET_MIC_BOOST1;
        else if (cmd == CMD_IDX_GET_DMIC_GAIN0)
            p_cmd = CMD_GET_DMIC_GAIN0;
        else if (cmd == CMD_IDX_GET_DMIC_GAIN1)
            p_cmd = CMD_GET_DMIC_GAIN1;
        else if (cmd == CMD_IDX_GET_DMIC_GAIN2)
            p_cmd = CMD_GET_DMIC_GAIN2;
        else if (cmd == CMD_IDX_GET_DMIC_GAIN3)
            p_cmd = CMD_GET_DMIC_GAIN3;
        else if (cmd == CMD_IDX_GET_DSP_SERIAL)
            p_cmd = CMD_GET_DMIC_SERIAL;
        else if (cmd == CMD_IDX_GET_ANGLE)
            p_cmd = CMD_GET_DMIC_GET_ANGEL;

        // if( cmd == CMD_IDX_GET_ANGLE )
        //  sprintf(buf,  "%s > %s", CXDISH_CMD_GET_ANGLE, CXDISH_TMP_FILE);
        // else
        sprintf(buf, "%s %s > %s", CXDISH_EXEC, p_cmd, CXDISH_TMP_FILE);
        if (result) {
            system(buf);
            if (cxdish_log_out(&p_outbuf) != -1 && p_outbuf && strlen(p_outbuf)) {
                if (CMD_IDX_GET_DSP_SERIAL == cmd) { // Serial form is special
                    // printf("p_outbuf=%s\n",p_outbuf);
                    // asprintf(&result, "%s", p_outbuf);
                    memcpy(result, p_outbuf, strlen(p_outbuf));
                    printf("CMD_IDX_GET_DSP_SERIAL result=%s\n", result);
                } else if (cmd == CMD_IDX_GET_ANGLE) {
                    // printf("[ASR MIC Angle] angle=%s\n",  p_outbuf);
                    unsigned int ret1, ret2, ret3;
                    sscanf(p_outbuf, "%x %x %x", &ret1, &ret2, &ret3);
                    // printf("[ASR MIC Angle] ret1=%x %x %x\n",  ret1, ret2, ret3);

                    float mag = ((float)((long)ret2)) / (1 << 23);
                    float angle_of_arrival = 2.0 * ((float)((long)ret1)) / (1 << 23);
                    // printf("[ASR MIC Angle] mag=%.2f angle=%.2f\n",  mag, angle_of_arrival);
                    *result = (unsigned int)angle_of_arrival;
                } else {
                    sscanf(p_outbuf, "%x", result);
                    //  wiimu_log(1, 0, 0, 0, "%s cmd=%d val=%d p_outbuf=[%s]result=%x",
                    //  __FUNCTION__, cmd, val, p_outbuf, *result);
                }
            }
            free(p_outbuf);
        }
        memset((void *)buf, 0, sizeof(buf));
        remove(CXDISH_TMP_FILE);
        break;

    case CMD_IDX_SET_ADC_GAIN0:
        p_cmd = get_cxdish_command_version() >= 2 ? CMD_SET_ADC_GAIN0_V2 : CMD_SET_ADC_GAIN0;
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, p_cmd, val);
        g_wiimu_shm->asr_adc_gain[0] = val;
        break;

    case CMD_IDX_SET_ADC_GAIN1:
        p_cmd = get_cxdish_command_version() >= 2 ? CMD_SET_ADC_GAIN1_V2 : CMD_SET_ADC_GAIN1;
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, p_cmd, val);
        g_wiimu_shm->asr_adc_gain[1] = val;
        break;

    case CMD_IDX_SET_MIC_BOOST0:
        p_cmd = get_cxdish_command_version() >= 2 ? CMD_SET_MIC_BOOST0_V2 : CMD_SET_MIC_BOOST0;
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, p_cmd, val * 2);
        break;
    case CMD_IDX_SET_MIC_BOOST1:
        p_cmd = get_cxdish_command_version() >= 2 ? CMD_SET_MIC_BOOST1_V2 : CMD_SET_MIC_BOOST1;
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, p_cmd, val * 2);
        break;
    case CMD_IDX_OUTPUT_TO_REF:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_OUTPUT_TO_REF);
        break;
    case CMD_IDX_OUTPUT_TO_MIC:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_OUTPUT_TO_MIC);
        break;
    case CMD_IDX_OUTPUT_TO_PROCESSED:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_OUTPUT_TO_PROCESSED);
        break;

    case CMD_IDX_SSP_DISABLE:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_SSP_DISABLE);
        break;
    case CMD_IDX_SSP_ENABLE:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_SSP_ENABLE);
        break;

    case CMD_IDX_SET_DMIC_GAIN0:
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, CMD_SET_DMIC_GAIN0, val * 2);
        break;

    case CMD_IDX_SET_DMIC_GAIN1:
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, CMD_SET_DMIC_GAIN1, val * 2);
        break;

    case CMD_IDX_SET_DMIC_GAIN2:
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, CMD_SET_DMIC_GAIN2, val * 2);
        break;

    case CMD_IDX_SET_DMIC_GAIN3:
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, CMD_SET_DMIC_GAIN3, val * 2);
        break;

    case CMD_IDX_AGC_DISABLE:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_AGC_DISABLE);
        break;
    case CMD_IDX_AGC_ENABLE:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_AGC_ENABLE);
        break;
    case CMD_IDX_SET_MIC_DISTANCE:
        sprintf(buf, "%s %s %08x", CXDISH_EXEC, CMD_SET_MIC_DISTANCE, val);
        g_wiimu_shm->asr_mic_distance = val;
        break;

    case CMD_IDX_SET_NOISE_GATING:
        switch (val) {
        case 0:
            p_cmd = CMD_SET_NOISE_GATING_0;
            break;

        case 1:
            p_cmd = CMD_SET_NOISE_GATING_1;
            break;

        case 2:
            p_cmd = CMD_SET_NOISE_GATING_2;
            break;

        case 3:
            p_cmd = CMD_SET_NOISE_GATING_3;
            break;

        case 4:
            p_cmd = CMD_SET_NOISE_GATING_4;
            break;

        default:
            printf("invalid noise gating %d\n", val);
            break;
        }

        if (p_cmd) {
            sprintf(buf, "%s %s", CXDISH_EXEC, p_cmd);
            g_wiimu_shm->asr_dsp_noise_gating = val;
        }
        break;
    case CMD_IDX_SET_SWITCH_MODE_VOIP:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_SET_SWITCH_MODE_VOIP);
        break;
    case CMD_IDX_SET_SWITCH_MODE_ASR:
        sprintf(buf, "%s %s", CXDISH_EXEC, CMD_SET_SWITCH_MODE_ASR);
        break;
    default:
        wiimu_log(1, 0, 0, 0, "%s unknown cmd=%d val=%d", __FUNCTION__, cmd, val);
        break;
    }

    if (strlen(buf) && g_wiimu_shm->device_status == DEV_NORMAL) {

        if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
            g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC) {
        } else if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
                   !(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC)) {
        } else {
            wiimu_log(1, 0, 0, 0, "%s cmd=%d val=%d[%s]", __FUNCTION__, cmd, val, buf);
            system(buf);
        }
    }
    return 0;
}

// 0:succeed of else failed
int cxdish_para_get(char *des[])
{
    unsigned int result = 0;

    // if (!far_field_judge(g_wiimu_shm))
    //  return -1;
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
        g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC) {
        // do not care adc gain and micboost for digital mic
        // cxdish_run(CMD_IDX_GET_DMIC_GAIN0, 0, &result);
        // asprintf(&des[1], "%08x", result);
        // cxdish_run(CMD_IDX_GET_DMIC_GAIN1, 0, &result);
        // asprintf(&des[2], "%08x", result);
        // cxdish_run(CMD_IDX_GET_DMIC_GAIN2, 0, &result);
        // asprintf(&des[3], "%08x", result);
        // cxdish_run(CMD_IDX_GET_DMIC_GAIN3, 0, &result);
        // asprintf(&des[4], "%08x", result);
        return 0;
    } else if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
               !(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC)) {
    } else {
        // cxdish-mode
        system(CXDISH_GET_MODE);
        if (cxdish_log_out(&des[0]))
            return -1;
        cxdish_run(CMD_IDX_GET_ADC_GAIN0, 0, &result);
        asprintf(&des[1], "%08x", result);
        cxdish_run(CMD_IDX_GET_ADC_GAIN1, 0, &result);
        asprintf(&des[2], "%08x", result);
        cxdish_run(CMD_IDX_GET_MIC_BOOST0, 0, &result);
        asprintf(&des[3], "%08x", result);
        cxdish_run(CMD_IDX_GET_MIC_BOOST1, 0, &result);
        asprintf(&des[4], "%08x", result);
    }
    return 0;
}

int cxdish_clock_change(void)
{
    if (far_field_judge(g_wiimu_shm)) {
        int hfile = open("/dev/i2cM0", O_RDWR);
        if (hfile != -1) {
            ioctl(hfile, _IO('I', 0)); // reset the iic
            close(hfile);
        } else
            return -1;
    }
    return 0;
}

int cxdish_angle_get(void)
{
    int angle = 0;
    cxdish_run(CMD_IDX_GET_ANGLE, 0, &angle);
    return angle;
}

// return 0:succeed or else failed
int cxdish_mic_value_judge(char *val)
{
    FILE *fp;

    char tmp_buf[128] = {0};
    if ((access(CXDISH_ADC_GAIN_FILE, 0)) != 0) // not exist
        return -1;

    if ((fp = fopen(CXDISH_ADC_GAIN_FILE, "r")) == NULL) {
        return -1;
    }
    if (fgets(tmp_buf, sizeof(tmp_buf), fp) == NULL) {
        if (feof(fp) == 0) { // get the end of file
            fclose(fp);
            return -1;
        }
    }
    if (strlen(tmp_buf)) {
        if (!strncmp(tmp_buf, val, strlen(val))) {
            fclose(fp);
            return 0;
        } else {
            fclose(fp);
            return -1;
        }
    } else {
        fclose(fp);
        return -1;
    }
}

// 0:ALL channel mute 1:left channel on 2:right channel on 3:ALL channel on
void cxdish_mic_channel_set(int mic_val, SOCKET_CONTEXT *pmsg_socket, char *recv_buf)
{
    int cnt = 0;
    int fail_cnt = 0;
    unsigned int result;

    if (!far_field_judge(g_wiimu_shm))
        return;

    boost_mutex_lock();
    if (mic_val < 0 || mic_val == 100) {
        mic_channel_test_mode = (mic_val < 0) ? 0 : mic_val;
        if (pmsg_socket && recv_buf) {
            cxdish_run(CMD_IDX_OUTPUT_TO_PROCESSED, 0, NULL); // set cxdish to normal mode
            sprintf(recv_buf, "reset to normal mode:ZSH2");
            UnixSocketServer_write(pmsg_socket, recv_buf, strlen(recv_buf));
        }
        boost_mutex_unlock();
        return;
    }
    mic_channel_test_mode = mic_val;

    switch (mic_val) {
    case 0: {
        cxdish_set_adc_gain(0, 0);
        cxdish_run(CMD_IDX_GET_ADC_GAIN0, 0, &result);
        if (result != 0)
            cnt = 1;

        cxdish_set_adc_gain(1, 0);
        cxdish_run(CMD_IDX_GET_ADC_GAIN1, 0, &result);
        if (result != 0)
            cnt = 1;
        if (recv_buf) {
            if (!cnt)
                sprintf(recv_buf, "ALL channel mute succeed");
            else
                sprintf(recv_buf, "ALL channel mute failed");
        }
    } break;
    case 1: {
        cxdish_set_adc_gain(0, ADC_GAIN_PLAYBACK_DEFAULT_VAL);
        cxdish_run(CMD_IDX_GET_ADC_GAIN0, 0, &result);
        if (result != ADC_GAIN_PLAYBACK_DEFAULT_VAL)
            cnt = 1;

        cxdish_set_adc_gain(1, 0);
        cxdish_run(CMD_IDX_GET_ADC_GAIN1, 0, &result);
        if (result != 0)
            cnt = 1;
        cnt = fail_cnt;
        if (recv_buf) {
            if (!cnt)
                sprintf(recv_buf, "left channel on succeed");
            else
                sprintf(recv_buf, "left channel on failed");
        }
    } break;
    case 2: {
        cxdish_set_adc_gain(0, 0);
        cxdish_run(CMD_IDX_GET_ADC_GAIN0, 0, &result);
        if (result != 0)
            cnt = 1;

        cxdish_set_adc_gain(1, ADC_GAIN_PLAYBACK_DEFAULT_VAL);
        cxdish_run(CMD_IDX_GET_ADC_GAIN1, 0, &result);
        if (result != ADC_GAIN_PLAYBACK_DEFAULT_VAL)
            cnt = 1;
        if (recv_buf) {
            if (!cnt)
                sprintf(recv_buf, "right channel on succeed");
            else
                sprintf(recv_buf, "right channel on failed");
        }
    } break;
    case 3: {
        cxdish_set_adc_gain(0, ADC_GAIN_PLAYBACK_DEFAULT_VAL);
        cxdish_run(CMD_IDX_GET_ADC_GAIN0, 0, &result);
        if (result != ADC_GAIN_PLAYBACK_DEFAULT_VAL)
            cnt = 1;

        cxdish_set_adc_gain(1, ADC_GAIN_PLAYBACK_DEFAULT_VAL);
        cxdish_run(CMD_IDX_GET_ADC_GAIN1, 0, &result);
        if (result != ADC_GAIN_PLAYBACK_DEFAULT_VAL)
            cnt = 1;
        if (recv_buf) {
            if (!cnt)
                sprintf(recv_buf, "ALL channel on succeed");
            else
                sprintf(recv_buf, "ALL channel on failed");
        }
    } break;

    default:
        if (recv_buf)
            sprintf(recv_buf, "No channel select");
        break;
    }
    boost_mutex_unlock();
    if (pmsg_socket && recv_buf)
        UnixSocketServer_write(pmsg_socket, recv_buf, strlen(recv_buf));
}

int cxdish_disable_ssp(void)
{
    // cxdish_run(CMD_IDX_SSP_DISABLE, 0, NULL);
    return 0;
}

int cxdish_enable_ssp(void)
{
    // cxdish_run(CMD_IDX_SSP_ENABLE, 0, NULL);
    return 0;
}

// Set left mic boost to 14dB(0x0E),  0x0E*2=0x1C
// mic boost value table:12dB(music and vol >= 70)  18dB(silence or vol < 30)  24dB(music and vol >
// 30)
int cxdish_boost_set(int boost_val)
{
    // if (!far_field_judge(g_wiimu_shm))
    //  return 0;
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    boost_mutex_lock();
    if (g_wiimu_shm->asr_current_mic_boost != boost_val) {
        if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
            g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC) {
            // do not set micboost for digital mic, set dmic gain instead
            // cxdish_run(CMD_IDX_SET_DMIC_GAIN0, boost_val, NULL);
            // cxdish_run(CMD_IDX_SET_DMIC_GAIN1, boost_val, NULL);
            // cxdish_run(CMD_IDX_SET_DMIC_GAIN2, boost_val, NULL);
            // cxdish_run(CMD_IDX_SET_DMIC_GAIN3, boost_val, NULL);
        }
        if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
            !(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC)) {
        } else {
            wiimu_log(1, 0, 0, 0, "%s val=%d volume=%d", __FUNCTION__, boost_val,
                      g_wiimu_shm->volume);
            cxdish_run(CMD_IDX_SET_MIC_BOOST0, boost_val, NULL);
            cxdish_run(CMD_IDX_SET_MIC_BOOST1, boost_val, NULL);
        }
        g_wiimu_shm->asr_current_mic_boost = boost_val;
    }
    boost_mutex_unlock();
    return 0;
}

/*
*  silence use micboost[0]
*  playing use micboost[(volume-1)/10+1]
*/
void cxdish_change_boost(void)
{
    char cmd[1024];
    // if (!far_field_judge(g_wiimu_shm))
    //  return;
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    int status = CheckPlayerStatus(g_wiimu_shm) == 1;
    status |= g_wiimu_shm->alexa_response_status;
    int mic_boost = 0;
    int music_is_playing = (status && g_wiimu_shm->volume > 0) ? 1 : 0;
    int asr_alert_is_playing = g_wiimu_shm->alert_status;
    boost_mutex_lock();

    if (music_is_playing || asr_alert_is_playing) {
        mic_boost = g_wiimu_shm->asr_mic_boost[(g_wiimu_shm->volume - 1) / 10 + 1];
        // change adc gain to asr_adc_gain_step[1]
        if (!(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC) &&
            g_wiimu_shm->asr_adc_gain_step[1] > 0) {
            if (g_wiimu_shm->asr_adc_gain[0] != g_wiimu_shm->asr_adc_gain_step[1])
                cxdish_run(CMD_IDX_SET_ADC_GAIN0, g_wiimu_shm->asr_adc_gain_step[1], NULL);

            if (g_wiimu_shm->asr_adc_gain[1] != g_wiimu_shm->asr_adc_gain_step[1])
                cxdish_run(CMD_IDX_SET_ADC_GAIN1, g_wiimu_shm->asr_adc_gain_step[1], NULL);
        }
    } else {
        // change adc gain to asr_adc_gain_step[0]
        if (!(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC) &&
            g_wiimu_shm->asr_adc_gain_step[0] > 0) {
            if (g_wiimu_shm->asr_adc_gain[0] != g_wiimu_shm->asr_adc_gain_step[0])
                cxdish_run(CMD_IDX_SET_ADC_GAIN0, g_wiimu_shm->asr_adc_gain_step[0], NULL);

            if (g_wiimu_shm->asr_adc_gain[1] != g_wiimu_shm->asr_adc_gain_step[0])
                cxdish_run(CMD_IDX_SET_ADC_GAIN1, g_wiimu_shm->asr_adc_gain_step[0], NULL);
        }
        mic_boost = g_wiimu_shm->asr_mic_boost[0];
    }
    boost_mutex_unlock();
    cxdish_boost_set(mic_boost);
}

int cxdish_output_to_ref(void)
{
    int ret = 0;
    boost_mutex_lock();
    ret = cxdish_run(CMD_IDX_OUTPUT_TO_REF, 0, NULL); // set cxdish to normal mode
    boost_mutex_unlock();

    return ret;
}

int cxdish_output_to_mic(void)
{
    int ret = 0;
    boost_mutex_lock();
    ret = cxdish_run(CMD_IDX_OUTPUT_TO_MIC, 0, NULL); // set cxdish to normal mode
    boost_mutex_unlock();
    return ret;
}

int cxdish_output_to_processed(void)
{
    return cxdish_run(CMD_IDX_OUTPUT_TO_PROCESSED, 0, NULL); // set cxdish to normal mode
}

void cxdish_set_adc_gain(int id, int val)
{
    // do not set adc gain for digital mic
    // if( g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC )
    //  return;
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    if (id == 0 || id == 2) {
        g_wiimu_shm->asr_adc_gain_step[0] = 0;
        g_wiimu_shm->asr_adc_gain_step[1] = 0;
        cxdish_run(CMD_IDX_SET_ADC_GAIN0, val, NULL);
    }

    if (id == 1 || id == 2) {
        g_wiimu_shm->asr_adc_gain_step[0] = 0;
        g_wiimu_shm->asr_adc_gain_step[1] = 0;
        cxdish_run(CMD_IDX_SET_ADC_GAIN1, val, NULL);
    }
}

void cxdish_set_mic_distance(int val)
{
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;
    if (val >= MIC_DISTANCE_MIN && val <= MIC_DISTANCE_MAX)
        cxdish_run(CMD_IDX_SET_MIC_DISTANCE, val, NULL);
}

void cxdish_adc_gain_init(void)
{
    // do not set adc gain for digital mic
    // if( g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC )
    //  return;
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    cxdish_run(CMD_IDX_SET_ADC_GAIN0, g_wiimu_shm->asr_adc_gain_step[0], NULL);
    cxdish_run(CMD_IDX_SET_ADC_GAIN1, g_wiimu_shm->asr_adc_gain_step[0], NULL);
}

#if defined(SUPPORT_VOIP_SWITCH) && SUPPORT_VOIP_SWITCH
int cxdish_switch_mode_voip(void) { return cxdish_run(CMD_IDX_SET_SWITCH_MODE_VOIP, 0, NULL); }

int cxdish_switch_mode_asr(void)
{
    int ret = 0;
    ret = cxdish_run(CMD_IDX_SET_SWITCH_MODE_ASR, 0, NULL);
    if (ret == 0)
        dsp_init();
    return ret;
}
#endif

void cxdish_set_drc_gain_func(int high_input, int high_output, int low_input, int low_output,
                              int k_high, int k_middle, int k_low)
{
    char buf[1024];
    int reg_high_input = (0xce + (high_input - (-100)) / 2) << 24;
    int reg_high_output = (0xce + (high_output - (-100)) / 2) << 24;
    int reg_low_input = (0xce + (low_input - (-100)) / 2) << 24;
    int reg_low_output = (0xce + (low_output - (-100)) / 2) << 24;
#if 0
    int reg_k_high = (int)log2f(k_high);
    int reg_k_middle = (int)log2f(k_middle);
    int reg_k_low = (int)log2f(k_low);
#else
    int reg_k_high = k_high;
    int reg_k_middle = k_middle;
    int reg_k_low = k_low;
#endif

    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;
    // if (!far_field_judge(g_wiimu_shm))
    //  return;
    if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
        g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC) {
    } else if (g_wiimu_shm->asr_feature_flag & ASR_FEATURE_DIGITAL_MIC &&
               !(g_wiimu_shm->asr_feature_flag & ASR_FEATURE_4MIC)) {
    } else {
        memset((void *)buf, 0, sizeof(buf));
        sprintf(buf, "%s %s 0x%.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x", CXDISH_EXEC,
                CMD_SET_DRC_GAIN_FUNC, reg_high_input, reg_high_output, reg_low_input,
                reg_low_output, reg_k_high, reg_k_middle, reg_k_low);
        wiimu_log(1, 0, 0, 0, "%s buf=%s", __FUNCTION__, buf);
        system(buf);
    }
}

// show dsp default parameters(drc/ssp/agc/fdnlp/micboost/adcgain/mic_distance)
static void dsp_dump(void)
{
    wiimu_log(1, 0, 0, 0, "-----------------------%s----------------------\n", __FUNCTION__);
}

void cxdish_set_noise_gating(int val)
{
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;
    cxdish_run(CMD_IDX_SET_NOISE_GATING, val, NULL);
}

void dsp_init(void)
{
    int mic_distance = g_wiimu_shm->asr_mic_distance;

    dsp_dump();
    cxdish_change_boost();
    cxdish_adc_gain_init();
    cxdish_set_noise_gating(g_wiimu_shm->asr_dsp_noise_gating);
    if (mic_distance >= MIC_DISTANCE_MIN && mic_distance <= MIC_DISTANCE_MAX) {
        wiimu_log(1, 0, 0, 0, "set mic distance to %d mm\n", mic_distance);
        cxdish_run(CMD_IDX_SET_MIC_DISTANCE, mic_distance, NULL);
    } else {
#if DSP_MIC_DISTANCE
        wiimu_log(1, 0, 0, 0, "set mic distance to %d mm by default\n", DSP_MIC_DISTANCE);
        cxdish_run(CMD_IDX_SET_MIC_DISTANCE, DSP_MIC_DISTANCE, NULL);
#endif
#if defined(A98_SALESFORCE)
        if (g_wiimu_shm->device_status == DEV_NORMAL) {
            printf("dsp init set output to 0db\n");
            system("cxdish sendcmd 0xD3086331 0x00000040 0x00000000 00000000 00000000 00000000 "
                   "00000000 00000000 00000000 00000000");
        }
#endif
    }
}

int read_dsp_version(WIIMU_CONTEXT *shm)
{

    FILE *fp;
    char tmp_buf[128] = {0};
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;

    system("cxdish fw-version > /tmp/cxdish_ver");

    if ((fp = fopen("/tmp/cxdish_ver", "r")) == NULL) {

        return -1;
    }

    if (fgets(tmp_buf, 64, fp) == NULL) {
        if (feof(fp) != 0) { // get the end of file
            printf("get the end of file\n");
        } else {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    if ((access("/tmp/cxdish_ver", 0)) == 0)
        system("rm -rf /tmp/cxdish_ver");

    strncpy(shm->asr_dsp_fw_ver, tmp_buf, (strlen(tmp_buf) > sizeof(shm->asr_dsp_fw_ver))
                                              ? sizeof(shm->asr_dsp_fw_ver)
                                              : strlen(tmp_buf));

    wiimu_log(1, 0, 0, 0, "==========Version print:cx dsp version:%s==============\n",
              shm->asr_dsp_fw_ver);

#if defined(LPSDK_WMOTA)
    SocketClientReadWriteMsg("/tmp/a01remoteupdate", "CheckUpdate", strlen("CheckUpdate"), NULL,
                             NULL, 0);
#endif

    return 0;
}

int dsp_serial_get(char *recv_buf)
{
    if (g_wiimu_shm->device_status != DEV_NORMAL)
        return -1;
    char *tmp_result = malloc(128); // must longer than dsp serial length
    if (!tmp_result)
        return -1;
    memset(tmp_result, 0, 128);
    if (cxdish_run(CMD_IDX_GET_DSP_SERIAL, 0, tmp_result)) {
        free(tmp_result);
        printf("dsp_serial_get err returned\n");
        return -1;
    }

    if (recv_buf) {
        sprintf(recv_buf, "{\"SERIAL\":\"%s\"}", tmp_result);
        free(tmp_result);
        return 0;
    } else {
        free(tmp_result);
        return -1;
    }
}

void cxdish_call_boost(int flags)
{
    if (flags) {
        system("/system/workdir/bin/cxdish sendcmd 0xD3086331 0x00000040 0x0A000000 0A000000 "
               "0A000000 0A000000 0A000000 0A000000 0A000000 0A000000");
    } else {
        system("/system/workdir/bin/cxdish sendcmd 0xD3086331 0x00000040 0x00000000 00000000 "
               "00000000 00000000 00000000 00000000 00000000 00000000");
    }
}
