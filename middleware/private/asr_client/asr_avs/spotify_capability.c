
#include <stdio.h>
#include <string.h>

#include "wm_util.h"
#include "spotify_capability.h"
#include "alexa_products.h"

static int g_enable_flag = 1;
extern WIIMU_CONTEXT *g_wiimu_shm;

void init_spotify_capability(void)
{
    if (g_wiimu_shm) {
        fprintf(stderr, "start %s ----------\n", g_wiimu_shm->project_name);
        AlexaDeviceInfo device_info = {0};
        int ret = get_alexa_device_info(ALEXA_PRODUCTS_FILE_PATH, g_wiimu_shm->project_name,
                                        &device_info);
        if (ret == 0) {
            if (device_info.spotify == 0) {
                g_enable_flag = 0;
                fprintf(stderr, "@@@@@@ %s project not support avs spotify @@@@@@\n",
                        g_wiimu_shm->project_name);
            }
        }
        fprintf(stderr, "ret = %d spotify= %d\n", ret, device_info.spotify);
        free_alexa_device_info(&device_info);
    }
}

int get_spotify_capability(void) { return g_enable_flag; }
