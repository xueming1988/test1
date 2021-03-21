#ifndef _SOFT_PRE_HOBOT_PROCESS_H
#define _SOFT_PRE_HOBOT_PROCESS_H

typedef int(soft_hobot_fill_CBACK)(char *buffer, int dataLen);

#if defined(ENABLE_RECORD_SERVER)
typedef int(soft_hobot_record_CBACK)(char *buf, int size, void *priv);
#endif

typedef struct _SOFT_HOBOT_PROCESS {
    // init
    int (*cSoft_hobot_init)(void);

    // deinit
    int (*cSoft_hobot_unInit)(void);

    // enable / disable record data
    int (*cSoft_hobot_enable)(int);

    // comsumer regitster a callback to let soft pre processer fill record data
    int (*cSoft_hobot_setCallback)(soft_hobot_fill_CBACK *);

#if defined(ENABLE_RECORD_SERVER)
    int (*cSoft_hobot_setRecordCallback)(soft_hobot_record_CBACK *, void *recPriv);
    soft_hobot_record_CBACK *soft_hobot_recCallback;
    void *recordPriv;
#endif

    // int status,int source,char path)//status: 0 stop 1 start    source:0 44.1k  1:16kprocessed
    // path
    int (*cSoft_hobot_setStatus)(int, int, char *);

    soft_hobot_fill_CBACK *soft_hobot_prep_callback;
    void *priv;
    int enable;
} SOFT_HOBOT_PROCESS;

#endif //_SOFT_PRE_PROCESS_H
