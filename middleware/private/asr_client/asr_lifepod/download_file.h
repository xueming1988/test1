
#ifndef __DOWNLOAD_FILE_H__
#define __DOWNLOAD_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

int curl_download(char *url, char *header, char *data, char *file);

#ifdef __cplusplus
}
#endif

#endif
