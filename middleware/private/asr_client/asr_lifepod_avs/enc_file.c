#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "enc.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        puts("usage: enc_file <inputfilename> <outputfilename>");
        return -1;
    }

    enc_file(argv[1], argv[2]);
}
