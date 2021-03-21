
#ifndef __NAVER_LOG_H__
#define __NAVER_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_PROJECT_NAME "clova-donut"
// Change to the firmware version
#define TEST_PROJECT_VERSION "1.0.0"
// Just a tag
#define TEST_LOG_SOURCE "nelo2-log"
// We can using the device id or sn number
#define TEST_LOG_TYPE "nelo2-app"
//#define TEST_SERVER_ADDRESS     "10.96.250.211"
//#define TEST_HTTPS_SERVER_ADDRESS     "10.96.250.214"
#define TEST_SERVER_ADDRESS "10.96.250.211"
#define TEST_HTTPS_SERVER_ADDRESS "nelo2-col.navercorp.com"
#define TEST_SERVER_PORT 80
#define TEST_HTTPS_SERVER_PORT 443

typedef struct _naver_log_s naver_log_t;

// log level
typedef enum _LOG_LEVEL {
    LL_FATAL = 0,
    LL_ERROR = 3,
    LL_WARN = 4,
    LL_INFO = 5,
    LL_DEBUG = 7,
#define LL_TRACE LL_DEBUG
    LL_INVALID = -1
} LOG_LEVEL;

naver_log_t *naver_log_create(void);

void naver_log_destroy(naver_log_t **self_p);

int naver_log_init(naver_log_t *self, const char *project_name, const char *project_version,
                   const char *log_source, const char *log_type, const char *server_addr,
                   const int server_port, const int https);

int naver_log_set_level(naver_log_t *self, LOG_LEVEL level);

int naver_log_open_crash_catcher(naver_log_t *self, const char *path);

int naver_log_send_log(naver_log_t *self, LOG_LEVEL level, const char *str_msg);

#ifdef __cplusplus
}
#endif

#endif
