

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <errno.h>
#include <sys/un.h>
#include <pthread.h>
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_gui.h"
#include "alexa_gui_macro.h"
#include "avs_gui_common.h"

#include "avs_player.h"

#include "lp_list.h"
#include "wm_util.h"
#include "searchonlinesong.h"
#include "alexa_focus_mgmt.h"

static int set_controls_info(int *ctls_info, char *ctl_name, bool ctl_enabled, bool ctl_selected)
{
    if (StrEq(ctl_name, CONTROL_NAME_PLAY_PAUSE)) {
        *ctls_info |= (ctl_enabled << CONTROL_BIT_PLAYPAUSE_EN);
        *ctls_info |= (ctl_selected << CONTROL_BIT_PLAYPAUSE_SE);
    } else if (StrEq(ctl_name, CONTROL_NAME_PREVIOUS)) {
        *ctls_info |= (ctl_enabled << CONTROL_BIT_PRE_EN);
        *ctls_info |= (ctl_selected << CONTROL_BIT_PRE_SE);
    } else if (StrEq(ctl_name, CONTROL_NAME_NEXT)) {
        *ctls_info |= (ctl_enabled << CONTROL_BIT_NEXT_EN);
        *ctls_info |= (ctl_selected << CONTROL_BIT_NEXT_SE);
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "get gui playerinfo control name:%s error\n", ctl_name);
    }

    return 0;
}

static int strncpy_metadata(char *des, char *src, int maxlen)
{
    if (src) {
        if ((strlen(src) + 1) > maxlen) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "[%s] src is too long,src = %s, strlen = %d,maxlen = %d\n",
                        __FUNCTION__, src, strlen(src), maxlen);
            return -1;
        }
        strncpy(des, src, strlen(src) + 1);
    } else {
        strncpy(des, "", maxlen - 1);
    }

    return 0;
}

static int set_playerinfo_matadata(rt_PlayerInfo_metadata_t **playerinfo_metadata_ptr,
                                   int *pinfo_metadata_size, char *header, char *header_sub1,
                                   char *title, char *artist, char *album, char *art_url,
                                   char *provider_name, char *provider_logo, char *token,
                                   int controls_info, long duration_ms)
{
    int ret = 0;
    rt_PlayerInfo_metadata_t *pinfo_metadata = NULL;

    int art_url_size = art_url ? strlen(art_url) + 1 : 0;
    int provider_logo_size = provider_logo ? strlen(provider_logo) + 1 : 0;
    int token_size = token ? strlen(token) + 1 : 0;
    *pinfo_metadata_size =
        sizeof(rt_PlayerInfo_metadata_t) + art_url_size + provider_logo_size + token_size;

    pinfo_metadata = (rt_PlayerInfo_metadata_t *)calloc(1, *pinfo_metadata_size);
    if (!pinfo_metadata) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "[%s]malloc pinfo_metadata failed\n", __FUNCTION__);
        return -1;
    }
    // memset(pinfo_metadata,0x00,*pinfo_metadata_size);

    pinfo_metadata->controls_info = controls_info;
    ret += strncpy_metadata(pinfo_metadata->sockmsg_preamble, "playerinfo_metadata:",
                            sizeof(pinfo_metadata->sockmsg_preamble));
    ret += strncpy_metadata(pinfo_metadata->header, header, sizeof(pinfo_metadata->header));
    ret += strncpy_metadata(pinfo_metadata->header_sub1, header_sub1,
                            sizeof(pinfo_metadata->header_sub1));
    ret += strncpy_metadata(pinfo_metadata->title, title, sizeof(pinfo_metadata->title));
    ret += strncpy_metadata(pinfo_metadata->artist, artist, sizeof(pinfo_metadata->artist));
    ret += strncpy_metadata(pinfo_metadata->album, album, sizeof(pinfo_metadata->album));
    ret += strncpy_metadata(pinfo_metadata->provider_name, provider_name,
                            sizeof(pinfo_metadata->provider_name));

    int hr = (duration_ms / (long)(60 * 60 * 1000));
    int min = (duration_ms - (long)(60 * 60 * 1000) * (long)hr) / (long)(60 * 1000);
    int sec = (duration_ms - (long)(60 * 60 * 1000) * (long)hr - (long)(60 * 1000) * (long)min) /
              (long)1000;
    snprintf(pinfo_metadata->duration, sizeof(pinfo_metadata->duration), "%02d:%02d:%02d.000", hr,
             min, sec);

    char *ptr_start = (char *)(pinfo_metadata) + sizeof(rt_PlayerInfo_metadata_t);

    if (token_size > 0) {
        pinfo_metadata->token_size = token_size;
        pinfo_metadata->token = (char *)ptr_start;
        ret += strncpy_metadata(pinfo_metadata->token, token, token_size);
    }

    if (art_url_size > 0) {
        pinfo_metadata->art_url_size = art_url_size;
        pinfo_metadata->art_url = (char *)ptr_start + token_size;
        ret += strncpy_metadata(pinfo_metadata->art_url, art_url, art_url_size);
    }

    if (provider_logo_size > 0) {
        pinfo_metadata->provider_logo_size = provider_logo_size;
        pinfo_metadata->provider_logo = (char *)ptr_start + art_url_size + token_size;
        ret += strncpy_metadata(pinfo_metadata->provider_logo, provider_logo, provider_logo_size);
    }

    *playerinfo_metadata_ptr = pinfo_metadata;

    return ret;
}

int avs_gui_init(void) { return 0; }

int avs_gui_uninit(void) { return 0; }

// judge that directive is gui or not, if is gui directive return 0;
int avs_gui_directive_check(char *name_space) { return 0; }

int avs_gui_create_msg_bodytemplate1(json_object *js_payload) { return 0; }

int avs_gui_create_msg_bodytemplate2(json_object *js_payload) { return 0; }

int avs_gui_create_msg_listTemplate1(json_object *js_payload) { return 0; }

int avs_gui_create_msg_weatherTemplate(json_object *js_payload) { return 0; }

int avs_gui_rendertemplate_parse(json_object *js_payload)
{
    int ret = -1;

    if (js_payload) {
        char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
        if (type) {
            if (StrEq(type, TYPE_BODYTEMPLATE1)) {
                ret = avs_gui_create_msg_bodytemplate1(js_payload);
            } else if (StrEq(type, TYPE_BODYTEMPLATE2)) {
                ret = avs_gui_create_msg_bodytemplate2(js_payload);
            } else if (StrEq(type, TYPE_LISTTEMPLATE1)) {
                ret = avs_gui_create_msg_listTemplate1(js_payload);
            } else if (StrEq(type, TYPE_WEATHERTEMPLATE)) {
                ret = avs_gui_create_msg_weatherTemplate(js_payload);
            } else {
                ret = -1;
            }
        } else {
            ret = -1;
        }
    }

    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase gui template payload error\n");
    }
    return ret;
}

int avs_gui_renderplayerinfo_parse(json_object *js_payload)
{
    int ret = -1;
    int arrs_num = 0;
    int i = 0;
    int flags = 0;

    char *new_audioItemId = NULL;
    if (js_payload) {
        new_audioItemId = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_AUDIOITEMID);
        json_object *js_content = json_object_object_get(js_payload, PAYLOAD_CONTENT);
        char *header = NULL;
        char *header_sub1 = NULL;
        char *title = NULL;
        char *artist = NULL;
        char *album = NULL;
        long duration_ms = 0;
        char *art_url = NULL;
        char *provider = NULL;
        char *logo_url = NULL;

        if (js_content) {
            header = JSON_GET_STRING_FROM_OBJECT(js_content, CONTENT_HEADER);
            header_sub1 = JSON_GET_STRING_FROM_OBJECT(js_content, CONTENT_HEADERSUBTEXT1);
            title = JSON_GET_STRING_FROM_OBJECT(js_content, CONTENT_TITLE);
            artist = JSON_GET_STRING_FROM_OBJECT(js_content, CONTENT_TITLESUBTEXT1);
            album = JSON_GET_STRING_FROM_OBJECT(js_content, CONTENT_TITLESUBTEXT2);
            duration_ms = JSON_GET_LONG_FROM_OBJECT(js_content, CONTENT_MEDIALENGTHINMILLISENCONDS);

            // ret = check_is_old_token(new_audioItemId);
            if (StrSubStr(title, IVALIAD_TOKEN_TILE) || StrSubStr(title, BAD_URL_TILE) ||
                StrSubStr(title, IVALIAD_HOST_TILE) || avs_player_audio_invalid(new_audioItemId)) {
                printf("Monkey test error urls and tokens\n");
                return -1;
            }

            // get the logo url
            json_object *js_provider = json_object_object_get(js_content, CONTENT_PROVIDER);

            if (js_provider) {
                provider = JSON_GET_STRING_FROM_OBJECT(js_provider, PROVIDER_NAME);
                json_object *js_logo = json_object_object_get(js_provider, PROVIDER_LOGO);
                if (js_logo) {
                    json_object *js_logo_sources = json_object_object_get(js_logo, IMAGE_SOURCES);

                    if (js_logo_sources) {
                        arrs_num = json_object_array_length(js_logo_sources);
                        for (i = 0; i < arrs_num; i++) {
                            json_object *js_src = json_object_array_get_idx(js_logo_sources, i);
                            if (js_src) {
                                logo_url = JSON_GET_STRING_FROM_OBJECT(js_src, SOURCES_URL);
                                break;
                            }
                        }
                    }
                }
            }

            avs_player_set_provider(provider);

            // get the art url
            char *size = NULL;
            json_object *js_art = json_object_object_get(js_content, CONTENT_ART);
            if (js_art) {
                json_object *js_sources = json_object_object_get(js_art, IMAGE_SOURCES);
                if (js_sources) {
                    arrs_num = json_object_array_length(js_sources);
                    for (i = 0; i < arrs_num; i++) {
                        json_object *js_src = json_object_array_get_idx(js_sources, i);
                        if (js_src) {
                            size = JSON_GET_STRING_FROM_OBJECT(js_src, SOURCES_SIZE);
                            art_url = JSON_GET_STRING_FROM_OBJECT(js_src, SOURCES_URL);
                            if (StrEq(size,
                                      SIZE_LARGE)) { // choose one kind of size to display if exist
                                break;
                            }
                        }
                    }
                }
            }
        }

        // get the controls info
        json_object *js_controls = json_object_object_get(js_payload, PAYLOAD_CONTROLS);
        int controls_info = 0;
        if (js_controls) {
            arrs_num = json_object_array_length(js_controls);
            for (i = 0; i < arrs_num; i++) {
                json_object *js_ctrls = json_object_array_get_idx(js_controls, i);
                if (js_ctrls) {
                    char *ctl_name = JSON_GET_STRING_FROM_OBJECT(js_ctrls, CONTROLS_NAME);
                    bool ctl_enabled = JSON_GET_BOOL_FROM_OBJECT(js_ctrls, CONTROLS_ENABLED);
                    bool ctl_selected = JSON_GET_BOOL_FROM_OBJECT(js_ctrls, CONTROLS_SELECTED);
                    set_controls_info(&controls_info, ctl_name, ctl_enabled, ctl_selected);
                }
            }
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "js_controls:\n%s\n",
                        (char *)json_object_to_json_string(js_controls));
        } else {
            flags = 1;
        }

        rt_PlayerInfo_metadata_t *pinfo_metadate = NULL;
        int pinfo_metadate_size = 0;
        ret = set_playerinfo_matadata(&pinfo_metadate, &pinfo_metadate_size, header, header_sub1,
                                      title, artist, album, art_url, provider, logo_url,
                                      new_audioItemId, controls_info, duration_ms);
        printf("Cheryl Debug: sent metadata :provider_name=%s header = %s, title = %s, artist = %s "
               "controls_info=%d\n",
               pinfo_metadate->provider_name, pinfo_metadate->header, pinfo_metadate->title,
               pinfo_metadate->artist, pinfo_metadate->controls_info);

        SocketClientReadWriteMsg("/tmp/Requesta01controller", (char *)pinfo_metadate,
                                 pinfo_metadate_size, NULL, NULL, 0);
        free(pinfo_metadate);
        if (flags) {
            ret = 1;
        }
    }

    if (ret < 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase gui playerinfo payload error\n");
    }

    return ret;
}

// parse directive
int avs_gui_parse_directive(json_object *js_data)
{
    int ret = -1;

    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        if (json_header) {
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
            if (name) {
                if (StrEq(name, NAME_RENDERTEMPLATE)) {
                    ret = avs_gui_rendertemplate_parse(js_payload);
                } else if (StrEq(name, NAME_RENDERPLAYERINFO)) {
                    ret = avs_gui_renderplayerinfo_parse(js_payload);
                    if (ret == 1) {
                        ret = 0;
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "GUI DEBUG:\n%s\n",
                                    (char *)json_object_to_json_string(js_data));
                    }
                } else {
                    ret = -1;
                }
            }
        }
    }
    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase gui message error\n");
    }

    return ret;
}

// parse the cmd from other thread to avs event
// TODO: need free after used
char *avs_gui_parse_cmd(char *json_cmd_str) { return NULL; }

int avs_gui_state(json_object *js_context_list, char *event_name) { return 0; }

json_object *avs_gui_event_create(json_object *json_cmd) { return NULL; }
