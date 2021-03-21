
#include <stdio.h>
#include <string.h>

#include "alexa_json_common.h"
#include "alexa_emplayer_macro.h"

#include "wm_util.h"

int main(int argc, const char *argv[])
{
    if (argc <= 2) {
        printf("please use: %s <cmd_type> <params ...>\n", argv[0]);
        return -1;
    }

    printf("run %s %s\n", argv[0], argv[1]);

    if (StrEq(argv[1], "play")) {
        char *cmd = NULL;
        char *params = NULL;
        Create_Emplayer_Play_Params(params, Val_Str(0), Val_Str(argv[2]), 0, 0);
        if (params) {
            Create_Emplayer_Cmd(cmd, Cmd_Play, params);
            free(params);
        }
        if (cmd) {
            Spotify_Notify(cmd, strlen(cmd), 0);
            free(cmd);
        }
    }

    return 0;
}
