
#include <stdio.h>
#include <stdlib.h>

#include "wm_util.h"

int lguplus_parse_fw_url(char *message)
{
    char *buf = NULL;

    if (message) {
        buf = (char *)calloc(1, strlen(message) + 100);
        if (buf) {
            set_force_ota(1);
            volume_pormpt_action(VOLUME_FW_01);
            SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=OTA_DOWNLOADING",
                                     strlen("GNOTIFY=OTA_DOWNLOADING"), NULL, NULL, 0);
            snprintf(buf, strlen(message) + 100, "LguplusFwInfo:%s", message);
            SocketClientReadWriteMsg("/tmp/a01remoteupdate", buf, strlen(buf), NULL, NULL, 0);
            free(buf);
        }
    }

    return 0;
}
