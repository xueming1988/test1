#include "xiaowei_sdk_interface.h"
#include "xiaowei_tts_player.h"
#include "xiaowei_linkplay_interface.h"

using namespace xiaowei_sdk_interface;

int main(int argc, const char *argv[])
{
    signal_handle();
    wiimu_shm_init();
    system_global_init();
    setLogFunc();
    get_qqid_info();

    xiaowei_wakeup_engine_init();
    xiaowei_socket_thread_init();
    xiaowei_alarm_thread_init();

    // tts player init
    xiaowei_tts_player_init();
    // url player init
    xiaowei_url_player_init();
    // wait for internet ok
    xiaowei_wait_internet();
    XiaoweiLoginInit();
    // wait for login ok
    xiaowei_wait_login();

    xiaowei_ota_config_init();
    // start xiaowei sdk engine,after login in ok
    XiaoweiEngineStart();
    xiaowei_query_ota();
#ifdef VOIP_SUPPORT
    voip_service_init();
#endif
    xiaowei_get_sdk_ver();
    xiaowei_set_music_quality(3);
    xiaowei_get_uin_info();
    while (true) {
        usleep(200000);
    }
}
