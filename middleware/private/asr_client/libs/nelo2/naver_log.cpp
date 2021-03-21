
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <nelo2.h>
#include <curl/curl.h>

#include "naver_log.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _naver_log_s {
    NELO2Log logger;
};

naver_log_t *naver_log_create(void)
{
    naver_log_t *self = new struct _naver_log_s;
    if (self) {
        return self;
    }

    return NULL;
}

void naver_log_destroy(naver_log_t **self_p)
{
    naver_log_t *self = *self_p;
    if (self) {
        delete self;
        *self_p = NULL;
    }
}

int naver_log_init(naver_log_t *self, const char *project_name, const char *project_version,
                   const char *log_source, const char *log_type, const char *server_addr,
                   const int server_port, const int https)
{
    int ret = -1;

    if (self) {
        // init nelo2 logger object, default log level is NELO_LL_INFO
        ret = self->logger.initialize(project_name, project_version, log_source, log_type,
                                      server_addr, server_port, https);
        if (NELO2_OK != ret) {
            printf("init nelo2 logger is failed.\n");
            ret = -1;
        }
        ret = 0;
    }

    return ret;
}

int naver_log_set_level(naver_log_t *self, LOG_LEVEL level)
{
    int ret = -1;
    NELO_LOG_LEVEL cur_level = NELO_LL_INVALID;

    if (self) {
        // set log level
        self->logger.setLogLevel((NELO_LOG_LEVEL)level);
        cur_level = self->logger.getLogLevel();
        if (cur_level == (NELO_LOG_LEVEL)level) {
            ret = 0;
        }
    }

    return ret;
}

int naver_log_open_crash_catcher(naver_log_t *self, const char *path)
{
    int ret = -1;

    if (self && path) {
        // set log level
        if (self->logger.openCrashCatcher(path)) {
            ret = 0;
        }
    }

    return ret;
}

int naver_log_send_log(naver_log_t *self, LOG_LEVEL level, const char *str_msg)
{
    int ret = -1;

    if (self && str_msg) {
        // set log level
        if (self->logger.sendLog((NELO_LOG_LEVEL)level, str_msg)) {
            ret = 0;
        }
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
