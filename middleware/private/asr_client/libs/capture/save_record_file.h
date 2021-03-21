/*************************************************************************
    > File Name: save_record_file.h
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 11:07:42 PM EDT
 ************************************************************************/

#ifndef _SAVE_RECORD_FILE_H
#define _SAVE_RECORD_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

int CCaptCosume_FileSave(char *buf, int size, void *priv);
int CCaptCosume_initFileSave(void *priv);
int CCaptCosume_deinitFileSave(void *priv);
void CCapt_Start_GeneralFile(char *fileName);
void CCapt_Stop_GeneralFile(char *fileName);

#ifdef __cplusplus
}
#endif

#endif
