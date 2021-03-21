
#include <stdio.h>
#include "alexa_event_cmd.h"

int main(int argc, char **argv)
{
    if (argc <= 2) {
        printf("please use: %s <cmd_type> <params ...>\n", argv[0]);
        printf("alert: %s alert <alert_token>\n", argv[0]);
        return -1;
    }

    printf("run %s %s\n", argv[0], argv[1]);

    if (!strncmp("alert", argv[1], strlen("alert"))) {
        printf("alert token: %s\n", argv[2]);
        AlexaSendAlertLocalDelete(argv[2]);
    } else {
    }

    return 0;
}
