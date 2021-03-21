#include "caiman.h"
#include <stdio.h>
#include "ccaptureGeneral.h"
#include "wm_util.h"

#if defined(SOFT_PREP_MODULE)
#include "pthread.h"
#include "soft_pre_process.h"

SOFT_PRE_PROCESS *g_soft_pre_process = NULL;
static pthread_mutex_t mutex_soft_pre_process = PTHREAD_MUTEX_INITIALIZER;

#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
extern SOFT_PRE_PROCESS Soft_prep_dspc;
#endif
#if defined(SOFT_PREP_HOBOT_MODULE)
extern SOFT_PRE_PROCESS Soft_prep_hobot;
#endif

extern WIIMU_CONTEXT *g_wiimu_shm;

int CAIMan_soft_prep_init(void)
{
    int ret = -1;

    pthread_mutex_lock(&mutex_soft_pre_process);
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    g_soft_pre_process = &Soft_prep_dspc;
    ret = 0;
#elif defined(SOFT_PREP_HOBOT_MODULE)
    printf("CAIMan_soft_hobot_init value\n");
    g_soft_pre_process = &Soft_prep_hobot;
    ret = 0;
#else
    printf("=============Miss Soft PreProcess module definition!============");
#endif
    pthread_mutex_unlock(&mutex_soft_pre_process);

    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_init) {
        SOFT_PREP_INIT_PARAM input_param;
        memset(&input_param, 0, sizeof(SOFT_PREP_INIT_PARAM));
        input_param.mic_numb = g_wiimu_shm->asr_mic_number;
        input_param.cap_dev = SOFT_PREP_CAP_PDM;
#if defined(YANDEX_HW_PLATFORM) || defined(ADC_ES7243_ENABLE)
        input_param.cap_dev = SOFT_PREP_CAP_TDMC;
#endif
        ret = g_soft_pre_process->cSoft_PreP_init(input_param);
    }

    return ret;
}

void CAIMan_soft_prep_deinit(void)
{
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_unInit) {
        g_soft_pre_process->cSoft_PreP_unInit();
    }

    pthread_mutex_lock(&mutex_soft_pre_process);
    g_soft_pre_process = NULL;
    pthread_mutex_unlock(&mutex_soft_pre_process);
}

void CAIMan_soft_prep_enable(int enable)
{
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_enable) {
        g_soft_pre_process->cSoft_PreP_enable(enable);
    }
}

void CAIMan_soft_prep_reset(void)
{
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_reset) {
        printf("CAIMan_soft_prep_reset\n");
        g_soft_pre_process->cSoft_PreP_reset();
    }
}

int CAIMan_soft_prep_privRecord(
    int status, int source,
    char *path) // status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
{
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_privRecord) {
        g_soft_pre_process->cSoft_PreP_privRecord(status, source, path);
    }

    return 0;
}
#endif

void CAIMan_pre_init(void)
{
#if defined(SOFT_PREP_MODULE)
    if (!CAIMan_soft_prep_init()) {
        printf("Soft PreProcess init fail\n");
        return -1;
    }
#endif
}

void CAIMan_init(void)
{

#if defined(SOFT_PREP_MODULE)
    CAIMan_soft_prep_enable(1);
#endif

    CCaptStop();

    if (!IsCCaptStart())
        CCaptStart();
}

void CAIMan_deinit(void)
{

#if defined(SOFT_PREP_MODULE)
    CAIMan_soft_prep_enable(0);
#if defined(NAVER_LINE) && defined(SOFT_PREP_HOBOT_MODULE)
    CAIMan_soft_prep_reset(); // different hobot_cfg for different keyword, reinit
#endif
#endif

    CCaptStop();
}

int CAIMan_IsInit(void) { return IsCCaptStart(); }

int CAIMan_YD_start(void)
{

#if defined(YD_IFLYTEK_MODULE)
    AIYD_Start_iflytek();
#elif defined(YD_SENSORY_MODULE)
    AIYD_Start_sensory();
#elif defined(YD_AMAZON_MODULE)
    ASR_Start_Amazon();
#elif defined(YD_BAIDU_MODULE)
    AIYD_Start_baidu();
#elif defined(YD_CYBERON_MODULE)
    AIYD_Start_cyberon();
#elif defined(YD_XIAOWEI_HOBOT_WWE_MODULE)
    ASR_Start_Weixin();   // hobot xiaowei wwe
#endif

// for double trigger
#if defined(SENSORY_DOUBLE_TRIGGER)
    AIYD_Start_sensory_double();
#elif defined(AMAZON_DOUBLE_TRIGGER)
    ASR_Start_Amazon_double();
#endif

    return 0;
}

int CAIMan_YD_stop(void)
{

#if defined(YD_IFLYTEK_MODULE)
    AIYD_Stop_iflytek();
#elif defined(YD_SENSORY_MODULE)
    AIYD_Stop_sensory();
#elif defined(YD_AMAZON_MODULE)
    ASR_Stop_Amazon();
#elif defined(YD_BAIDU_MODULE)
    AIYD_Stop_baidu();
#elif defined(YD_CYBERON_MODULE)
    AIYD_Stop_cyberon();
#elif defined(YD_XIAOWEI_HOBOT_WWE_MODULE)
    ASR_Stop_Weixin();    // hobot xiaowei wwe
#endif

// for double trigger
#if defined(SENSORY_DOUBLE_TRIGGER)
    AIYD_Stop_sensory_double();
#elif defined(AMAZON_DOUBLE_TRIGGER)
    ASR_Stop_Amazon_double();
#endif

    return 0;
}

int CAIMan_YD_enable(void)
{

#if defined(YD_IFLYTEK_MODULE)
    AIYD_enable_iflytek();
#elif defined(YD_SENSORY_MODULE)
    AIYD_enable_sensory();
#elif defined(YD_AMAZON_MODULE)
    ASR_Enable_Amazon();
#elif defined(YD_BAIDU_MODULE)
    AIYD_enable_baidu();
#elif defined(YD_CYBERON_MODULE)
    AIYD_enable_cyberon();
#elif defined(YD_XIAOWEI_HOBOT_WWE_MODULE)
    ASR_Enable_Weixin();  // hobot xiaowei wwe
#endif

// for double trigger
#if defined(SENSORY_DOUBLE_TRIGGER)
    AIYD_enable_sensory_double();
#elif defined(AMAZON_DOUBLE_TRIGGER)
    ASR_Enable_Amazon_double();
#endif

    return 0;
}

int CAIMan_YD_disable(void)
{

#if defined(YD_IFLYTEK_MODULE)
    AIYD_disable_iflytek();
#elif defined(YD_SENSORY_MODULE)
    AIYD_disable_sensory();
#elif defined(YD_AMAZON_MODULE)
    ASR_Disable_Amazon();
#elif defined(YD_BAIDU_MODULE)
    AIYD_disable_baidu();
#elif defined(YD_CYBERON_MODULE)
    AIYD_disable_cyberon();
#elif defined(YD_XIAOWEI_HOBOT_WWE_MODULE)
    ASR_Disable_Weixin(); // hobot xiaowei wwe
#endif

// for double trigger
#if defined(SENSORY_DOUBLE_TRIGGER)
    AIYD_disable_sensory_double();
#elif defined(AMAZON_DOUBLE_TRIGGER)
    ASR_Disable_Amazon_double();
#endif

    return 0;
}

int CAIMan_ASR_start(void)
{

#ifdef NAVER_LINE
    ASR_Start_Record();
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    ASR_Start_QQ();
#else
    ASR_Start_Alexa();
#endif

    return 0;
}

int CAIMan_ASR_stop(void)
{

#ifdef NAVER_LINE
    ASR_Stop_Record();
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    ASR_Stop_QQ();
#else
    ASR_Stop_Alexa();
#endif

    return 0;
}

int CAIMan_ASR_enable(int talk_from)
{

#ifdef NAVER_LINE
    ASR_Enable_Record(talk_from);
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    ASR_Enable_QQ();
#else
    ASR_Enable_Alexa(talk_from);
#endif

    return 0;
}

int CAIMan_ASR_disable(void)
{

#ifdef NAVER_LINE
    ASR_Disable_Record();
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    ASR_Disable_QQ();
#else
    ASR_Disable_Alexa();
#endif

    return 0;
}

int CAIMan_ASR_finished(void)
{

#ifdef NAVER_LINE
    ASR_Finished_Record();
#elif defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
    ASR_Finished_QQ();
#else
    ASR_Finished_Alexa();
#endif

    return 0;
}
