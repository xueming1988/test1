#ifndef __FILE_RECORD_H__
#define __FILE_RECORD_H__

#ifdef __cplusplus
extern "C" {
#endif

int file_record_init(void);

int file_record_uninit(void);

int file_record_start(char *path);

#ifdef __cplusplus
}
#endif

#endif
