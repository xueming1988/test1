#ifndef _CAIMAN_H
#define _CAIMAN_H

typedef enum {
    ERR_ASR_NO_UNKNOWN = -1000,
    ERR_ASR_NO_INTERNET = -1001,
    ERR_ASR_NO_USER_ACCOUNT = -1002,
    ERR_ASR_USER_LOGIN_FAIL = -1003,
    ERR_ASR_MODULE_DISABLED = -1004,
    ERR_ASR_DEVICE_IS_SLAVE = -1005,
    ERR_ASR_LISTEN_NONE = -1006,
    ERR_ASR_NOTHING_PLAY = -1007,
    ERR_ASR_BOOK_SEARCH_FAIL = -1008,
    ERR_ASR_MUSIC_SEARCH_FAIL = -1009,
    ERR_ASR_MIS_UNDERSTAND = -1010,
    ERR_ASR_CONTINUE_BOOK = -1011,
    ERR_ASR_CONTINUE_MUSIC = -1012,
    ERR_ASR_CONTINUE = -1013,
    ERR_ASR_SEARCHING_BEGIN = -1014,
    ERR_ASR_NETWORK_LOW_CONTINUE = -1015,
    ERR_ASR_NETWORK_LOW_STOP = -1016,
    ERR_ASR_EXCEPTION = -1099,
    ERR_ASR_MUSIC_STOP = -1100,
    ERR_ASR_MUSIC_RESUME = -1101,
    ERR_ASR_PROPMT_STOP = -1102,
    ERR_ASR_HAVE_PROMBLEM = -1103,
} ASR_ERROR_CODE;

void CAIMan_pre_init(void);
void CAIMan_init(void);
void CAIMan_deinit(void);
int CAIMan_IsInit(void);

int CAIMan_ASR_start(void);
int CAIMan_ASR_stop(void);

int CAIMan_ASR_enable(int talk_from);
int CAIMan_ASR_disable(void);
int CAIMan_ASR_finished(void);

int CAIMan_YD_start(void);
int CAIMan_YD_stop(void);

int CAIMan_YD_enable(void);
int CAIMan_YD_disable(void);

#ifdef SENSORY_DOUBLE_TRIGGER
int CAIMan_YD_start_double(void);
int CAIMan_YD_stop_double(void);

int CAIMan_YD_enable_double(void);
int CAIMan_YD_disable_double(void);
#endif

int CAIMan_soft_prep_init(void);
void CAIMan_soft_prep_deinit(void);

int CAIMan_soft_prep_privRecord(int status, int source, char *path);

#endif
