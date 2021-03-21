#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "enc.h"

/**
*   used to encrypt the string, output the encrypted string with Hex string
*/
int main(int argc, char *argv[])
{
    unsigned char *str_in = NULL;
    unsigned char *str_enc = NULL;
    unsigned char *str_dec = NULL;
    int len_in = 0;

    if (argc != 2) {
        puts("usage: str_enc  <input string>");
        return -1;
    }

    str_in = argv[1];
    len_in = strlen(str_in);
    printf("get len = %d,  string is :[%s]\n", len_in, str_in);

    str_enc = (unsigned char *)malloc(MAX_ENC_BUF_LEN);
    enc_str(str_in, len_in, str_enc);
    printf("enc string is :\"%s\"\n", str_enc);

    printf("\"");
    int i = 0;
    for (i = 0; i < len_in; i++)
        printf("\\x%x", str_enc[i]);
    printf("\"\n");

    str_dec = (unsigned char *)malloc(MAX_ENC_BUF_LEN);
    enc_str(str_enc, len_in, str_dec);
    printf("dec string is :[%s]\n", str_dec);

    return 0;
}
