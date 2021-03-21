#ifndef __ENC_DEC_H__
#define __ENC_DEC_H__

#define MAX_ENC_BUF_LEN 10 * 1024

int enc_dec_str(const unsigned char *buff_in, int len_in, unsigned char *buff_out);
int enc_file(char *file_in, char *file_out);
int enc_file_strout(char *file_in, char *str_out);

#endif
