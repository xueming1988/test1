#include <stdio.h>
#include <string.h>
#include <openssl/des.h>
#include <openssl/bio.h>
#include <openssl/rc4.h>
#include "enc_dec.h"

unsigned short passwd[6][8] = {{0x42a3, 0x9db4, 0x60ac, 0x57f4, 0xe4ab, 0x9312, 0x9f82, 0x1023},
                               {0x39eb, 0x8984, 0xd76d, 0xa30b, 0x7678, 0x9383, 0x0101, 0x7382},
                               {0x014d, 0xf472, 0xf5d3, 0x1ea0, 0x0e68, 0x8aba, 0x1020, 0x3212},
                               {0x30f1, 0x9545, 0xa832, 0x87f2, 0xc370, 0x9edf, 0x11aa, 0xa110},
                               {0x1cd1, 0xa352, 0xbd31, 0x736c, 0xb681, 0xaddf, 0x5555, 0xeeff},
                               {0x40b2, 0xdb41, 0x9802, 0x9c92, 0x014b, 0xfeff, 0xa5a5, 0xf23d}};

unsigned char dt[7] = {0x03, 0x01, 0x0a, 0x07, 0x05, 0x01, 0x00};
unsigned char tmpkey[32] = {0};

/**
*   purpose : get key password from ramdom array passwd[][], the length of key password is 16 bytes
*/
static void get_key(const unsigned char **key_pwd)
{
    int i = 0;
    int n = 6;
    unsigned char *p_tmpkey = tmpkey;
    unsigned char *p_passwd = NULL;
    memset(&tmpkey, 0x00, sizeof(tmpkey));
    for (i = 0; i < n; i++) {
        p_passwd = ((unsigned char *)passwd[i] + dt[n - i]);
        strncpy(p_tmpkey, p_passwd, i + 1);
        p_tmpkey += (i + 1);
    }
    tmpkey[31] = '\0';

    *key_pwd = (const unsigned char *)tmpkey;
}

/**
*   purpose  : encrypt/decrypt string
*   params   : buff_in : string need to encrypt/decrypt
*        len_in  : the length of string need to encrypt/decrypt
*        buff_out: the result(encrypted/decrypted) string
*/
int enc_dec_str(const unsigned char *buff_in, int len_in, unsigned char *buff_out)
{
    static const unsigned char *key_pwd = NULL;

    RC4_KEY key;
    if (!key_pwd) {
        get_key(&key_pwd);
    }
    /*
        printf("cheryl debug key :");
        int i = 0;
        for (i = 0;i <16;i++) printf("%x,",*(key_pwd+i));
        printf("\n");
    */
    RC4_set_key(&key, 16, key_pwd);

    RC4(&key, len_in, buff_in, buff_out);

    return 0;
}

/**
*   purpose  : encrypt/decrypt file
*   params   : file_in : input file path (need to encrypt/decrypt)
*        file_out: the result(encrypted/decrypted) file path
*/
int enc_file(char *file_in, char *file_out)
{
    FILE *fin = NULL;
    FILE *fout = NULL;
    unsigned char buff_src[MAX_ENC_BUF_LEN] = {0};
    unsigned char buff_enc[MAX_ENC_BUF_LEN] = {0};
    int len_src = 0;

    fin = fopen(file_in, "rb");
    fout = fopen(file_out, "wb");
    if (!(fin && fout)) {
        printf("[enc_file] open file failed\n");
        if (fin)
            fclose(fin);
        if (fout)
            fclose(fout);
        return -1;
    }

    while (1) {
        len_src = fread(buff_src, 1, MAX_ENC_BUF_LEN, fin);
        if (len_src <= 0)
            break;
        enc_dec_str(buff_src, len_src, buff_enc);
        fwrite(buff_enc, 1, len_src, fout);
    }

    fclose(fin);
    fclose(fout);
    return 0;
}

/**
*   purpose  : encrypt/decrypt file
*   params   : file_in : input file path (need to encrypt/decrypt)
*        str_out: the result(encrypted/decrypted) string
*/

int enc_file_strout(char *file_in, char *str_out)
{
    FILE *fin = NULL;
    unsigned char buff_src[MAX_ENC_BUF_LEN] = {0};
    unsigned char buff_enc[MAX_ENC_BUF_LEN] = {0};
    int len_src = 0;

    fin = fopen(file_in, "rb");
    if (!fin) {
        printf("[enc_file] open file failed\n");
        return -1;
    }

    while (1) {
        len_src = fread(buff_src, 1, MAX_ENC_BUF_LEN, fin);
        if (len_src <= 0)
            break;
        enc_dec_str(buff_src, len_src, buff_enc);
        strncpy(str_out, buff_enc, len_src);
        str_out += len_src;
    }

    fclose(fin);
    return 0;
}
