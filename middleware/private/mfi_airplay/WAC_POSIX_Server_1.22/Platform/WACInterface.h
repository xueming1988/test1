#ifndef __WAC_COMMON_INTERFACE__
#define __WAC_COMMON_INTERFACE__

#include "Common.h"
#include "WACServerAPI.h"

#define WAC_STATE_BUF_LEN			(128)
#define WAC_MAX_BUF_LEN				(512)
#define WAC_SERVER_CONNECT_WIFI_TIMEOUT                (90) // 1.5  mins now

#define WAC_SERVER_STATE_INIT				(0)
#define WAC_SERVER_STATE_READY				(1)
#define WAC_SERVER_STATE_CONFIG_START		(2)
#define WAC_SERVER_STATE_CONFIG_RECEVIED	(3)
#define WAC_SERVER_STATE_CONFIG_COMPLETE 	(4)
#define WAC_SERVER_STATE_STOPPED			(5)
#define WAC_SERVER_STATE_ERROR 				(6)
#define WAC_SERVER_STATE_NONE				(-1)

#define NVRAM_WAC_NAME "WAC_NAME"
#define NVRAM_AIRPLAY_PASSWORD "AIRPLAY_PASSWORD"

void platform_save_airplay_password(const char * const inPlayPassword);
void platform_save_wac_name( const char * const inName );
int platform_join_wifi( const char * const inSSID, const uint8_t * const inWiFiPSK, size_t inWiFiPSKLen );
void platform_softap_start(const uint8_t *inIE, size_t inIELen );
void platform_softap_stop(void);
void platform_get_wac_name(char *name);
void platform_get_mac_address(char *mac);
/*
flag = 0, unconfigured, exit wac mode
flag = 1, unconfigured, in wac mode
flag = 2, configured, exit wac mode
*/
#define WAC_FLAG_UNCONFIGURED_TIMEOUT 0
#define WAC_FLAG_UNCONFIGURED         1
#define WAC_FLAG_CONFIGURED           2
void platform_update_wac_status(int flag);
int platform_is_wac_unconfigured(void);
void platform_get_wac_device_info(WACPlatformParameters_t *param);
void platform_prepare_airplay_name_and_password(void);

#endif  //  __WAC_COMMON_INTERFACE__
