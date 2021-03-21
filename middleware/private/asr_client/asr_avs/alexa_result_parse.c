/*

*/
#include <ctype.h>

#include "json.h"
#include "caiman.h"
#include "wm_util.h"

#include "alexa.h"
#include "alexa_cmd.h"
#include "alexa_gui.h"
#include "alexa_alert.h"
#include "alexa_debug.h"
#include "alexa_system.h"
#include "alexa_bluetooth.h"
#include "alexa_json_common.h"

#include "alexa_result_parse.h"
#include "alexa_notification.h"
#include "avs_player.h"
#include "alexa_focus_mgmt.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#include "alexa_emplayer.h"
#include "alexa_input_controller.h"
#include "alexa_donotdisturb.h"
#include "wm_util.h"
#include "zlib.h"
#include "lpcfg.h"
extern WIIMU_CONTEXT *g_wiimu_shm;
extern share_context *g_mplayer_shmem;

/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -begin*/
/*{
    "event": {
        "header": {
            "namespace": "EqualizerController",
            "name": "EqualizerChanged",
            "messageId": "{{STRING}}"
        },
        "payload": {
            "bands": [
                {
                    "name": "{{STRING}}",
                    "level": {{INTEGER}}
                },
                // This should be a complete list of
                // supported equalizer bands.
            ],
            "mode": "{{STRING}}"
        }
    }
}*/
static int alexa_parse_equalizercontroller_mode_change_event(const char *str_mode)
{
    int ret = -1;
    json_object *js_payload = json_object_new_object();
    json_object *js_mode = json_object_new_string(str_mode);

    const char *pay_load_str = NULL;
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_parse_equalizercontroller_mode_change_event +++! \n");

    if (js_payload == NULL || js_mode == NULL) {
        json_object_put(js_payload);
        json_object_put(js_mode);

        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error Equalizer controllerCBL create event failed! \n");
        // out of memory
    } else {

        json_object_object_add(js_payload, NAME_MODE, js_mode);

        pay_load_str = json_object_to_json_string(js_payload);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "EqualizerChanged event: %s! \n", pay_load_str);

        ret = alexa_equalizer_controller_cmd(0, NAME_EQUALIZER_CHANGED, pay_load_str);

        // free json object
        json_object_put(js_payload);
    }
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_parse_equalizercontroller_mode_change_event ---! \n");

    return ret;
}

int alexa_parse_equalizercontroller_change_event(void)
{
    int ret = -1;
    json_object *js_payload = json_object_new_object();
    json_object *js_bands = json_object_new_array();
    json_object *js_band_treble = json_object_new_object();
    json_object *js_band_midrange = json_object_new_object();
    json_object *js_band_bass = json_object_new_object();

    json_object *js_bass = json_object_new_string(NAME_BASS);
    json_object *js_bass_val = json_object_new_int(g_wiimu_shm->bass);
    json_object *js_midrange = json_object_new_string(NAME_MIDRANGE);
    json_object *js_midrange_val = json_object_new_int(g_wiimu_shm->eq_midrange);
    json_object *js_treble = json_object_new_string(NAME_TREBLE);
    json_object *js_treble_val = json_object_new_int(g_wiimu_shm->treble);
    const char *pay_load_str = NULL;
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_parse_equalizercontroller_change_event +++! \n");

    if (js_payload == NULL || js_bands == NULL || js_band_treble == NULL ||
        js_band_midrange == NULL || js_band_bass == NULL || js_bass == NULL ||
        js_bass_val == NULL || js_midrange == NULL || js_midrange_val == NULL ||
        js_treble == NULL || js_treble_val == NULL) {
        json_object_put(js_payload);
        json_object_put(js_bands);
        json_object_put(js_band_treble);
        json_object_put(js_band_midrange);
        json_object_put(js_band_bass);
        json_object_put(js_bass);
        json_object_put(js_bass_val);
        json_object_put(js_midrange);
        json_object_put(js_midrange_val);
        json_object_put(js_treble);
        json_object_put(js_treble_val);

        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error Equalizer controllerCBL create event failed! \n");

        // out of memory
    } else {
        json_object_object_add(js_band_treble, NAME_NAME, js_treble);
        json_object_object_add(js_band_treble, NAME_LEVEL, js_treble_val);
        json_object_object_add(js_band_midrange, NAME_NAME, js_midrange);
        json_object_object_add(js_band_midrange, NAME_LEVEL, js_midrange_val);
        json_object_object_add(js_band_bass, NAME_NAME, js_bass);
        json_object_object_add(js_band_bass, NAME_LEVEL, js_bass_val);

        json_object_array_put_idx(js_bands, 0, js_band_treble);
        json_object_array_put_idx(js_bands, 1, js_band_midrange);
        json_object_array_put_idx(js_bands, 2, js_band_bass);

        // json_object_object_add(js_mode, "mode", "");

        json_object_object_add(js_payload, NAME_BANDS, js_bands);

        pay_load_str = json_object_to_json_string(js_payload);

        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "EqualizerChanged event: %s! \n", pay_load_str);

        ret = alexa_equalizer_controller_cmd(0, NAME_EQUALIZER_CHANGED, pay_load_str);

        // free json object
        json_object_put(js_payload);
    }
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_parse_equalizercontroller_change_event ---! \n");

    return ret;
}

static int alexa_parse_equalizercontroller_add_event(json_object *js_payload, const char *name,
                                                     int val)
{
    int ret = -1;
    json_object *js_bands = NULL;

    if (js_payload != NULL || name != NULL) {
        json_object *js_item = json_object_new_object();
        json_object *js_value = json_object_new_int(val);
        json_object *js_name = json_object_new_string(name);

        if (js_item != NULL) {
            js_bands = json_object_object_get(js_payload, NAME_BANDS);
            // if without bands arry object, add it
            if (js_bands == NULL) {
                js_bands = json_object_new_array();
                json_object_object_add(js_payload, NAME_BANDS, js_bands);
            }

            if (js_bands != NULL) {
                // add object into bands array
                json_object_object_add(js_item, NAME_NAME, js_name);
                json_object_object_add(js_item, NAME_LEVEL, js_value);
                json_object_array_add(js_bands, js_item);

                ret = 0;
            } else {
                // error occured
                json_object_put(js_item);
                json_object_put(js_value);
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Equalizer js_bands == NULL !\n");
            }
        }
    }

    if (ret == -1) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Equalizer add event error: name:%s , val: %d! \n", name, val);
    }

    return ret;
}

/*
{
    "directive": {
        "header": {
            "namespace": "EqualizerController",
            "name": "SetMode",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "mode": "{{STRING}}"
        }
    }
}
*/
static int alexa_parse_equalizercontroller_set_mode(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_mode = json_object_object_get(js_payload, NAME_MODE);
            if (js_mode) {
                char cmd_buf[128] = {0};
                const char *str_mode = json_object_get_string(js_mode);
                if (str_mode) {
                    char read_buff[256] = {'\0'};
                    int len = 0;
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Directive change Equalizer mode to: %s \n",
                                str_mode);

                    // accept value is: "MOVIE", "MUSIC", "NIGHT", "SPORT", "TV"
                    snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:SetMode:%s", str_mode);
                    // snprintf(cmd_buf, sizeof(cmd_buf), "GNOTIFY=EQUALIZER:EQMode:%s", str_mode);
                    if (SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf, strlen(cmd_buf),
                                                 read_buff, &len, 0) > 0) {
                        if (strlen(read_buff) && (strcmp(read_buff, "OK"))) {
                            // todo: get mode value
                            // send event back to AVS
                            alexa_parse_equalizercontroller_mode_change_event(str_mode);
                            return 0;
                        }
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                    }
                } // if(str_mode)
            }
        }
    }

    return -1;
}

/*
{
    "directive": {
        "header": {
            "namespace": "EqualizerController",
            "name": "AdjustBands",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "bands": [
                {
                    "name": "{{STRING}}",
                    "levelDelta": {{INTEGER}},
                    "levelDirection": "{{STRING}}"
                }
            ]
        }
    }
}
*/
static int alexa_parse_equalizercontroller_adjust_bands(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_bands = json_object_object_get(js_payload, NAME_BANDS);
            if (js_bands) {
                char cmd_buf[128] = {0};
                json_object *js_item = NULL;
                json_object *js_name = NULL;
                json_object *js_level_delta = NULL;
                json_object *js_level_dir = NULL;
                json_object *js_event = json_object_new_object();
                const char *event_str = NULL;
                const char *str_name = NULL;
                const char *str_dir = NULL;
                int delta = -1;
                int adjust_delta = -1;
                int i, j;
                int eq_num = 0; // eqParamArray
                int solution_num = 0;
                int match_flag = FALSE;
                int match_index = BAND_INDEX_NONE;
                int eq_index = -1; // eqIndex
                int solution_index = -1;
                char *eq_asr_name = NULL; // eqAsrName
                char path_buff[LPCFG_MAX_PAH_LEN] = {'\0'};
                char path_head[LPCFG_MAX_PAH_LEN] = {'\0'};
                const char *solution_active = NULL;
                const char *solution_name = NULL;
                const char *head_str = "equallizer.eqSolutions.solutionsArray";
                int length = json_object_array_length(js_bands);

                for (i = 0; i < length; i++) {
                    js_item = json_object_array_get_idx(js_bands, i);
                    js_name = json_object_object_get(js_item, NAME_NAME);
                    js_level_delta = json_object_object_get(js_item, NAME_LEVELDELTA);
                    js_level_dir = json_object_object_get(js_item, NAME_LEVELIRECTION);

                    str_name = json_object_get_string(js_name);

                    /*modify by wieqiang.huang for MAR-385 2019-01-17 --begin*/
                    if (js_level_delta) {
                        delta = json_object_get_int(js_level_delta);
                    } else {
                        delta = 1;
                    }
                    /*modify by wieqiang.huang for MAR-385 2019-01-17 --end*/

                    str_dir = json_object_get_string(js_level_dir);

                    /* match lpcfg.json */
                    solution_index = 0; // default value;
                    memset(path_buff, 0, sizeof(path_buff));
                    snprintf(path_buff, sizeof(path_buff) - 1, "%s.%s.%s", "equallizer",
                             "eqSolutions", "solutionActive");
                    if (lpcfg_read_string(path_buff, (char **)&solution_active) == 0) {
                        solution_num = lpcfg_array_size(head_str);
                        for (i = 0; i < solution_num; i++) {
                            memset(path_buff, 0, sizeof(path_buff));
                            snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", head_str, i,
                                     "solutionName");
                            if (lpcfg_read_string(path_buff, (char **)&solution_name) == 0) {
                                if (strcmp(solution_active, solution_name) == 0) {
                                    solution_index = i;
                                    break;
                                }
                            }
                        } // for (..solution_num..);
                    }
                    memset(path_head, 0, sizeof(path_head));
                    snprintf(path_head, sizeof(path_head) - 1, "%s[%d].%s", head_str,
                             solution_index, "eqParamArray");

                    eq_num = lpcfg_array_size(path_head); // eq_num
                    for (j = 0; j < eq_num; j++) {
                        match_index = j;
                        memset(path_buff, 0, sizeof(path_buff));
                        snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", path_head,
                                 match_index, "eqAsrName");
                        if (lpcfg_read_string(path_buff, &eq_asr_name) ==
                            0) { // eqParamArray[i].eqAsrName
                            if (StrEq(str_name, eq_asr_name)) {
                                if (StrEq(str_dir, "UP")) {
                                    adjust_delta = delta;
                                } else if (StrEq(str_dir, "DOWN")) {
                                    adjust_delta = -delta;
                                }
                                memset(path_buff, 0, sizeof(path_buff));
                                snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", path_head,
                                         match_index, "eqIndex");
                                if (lpcfg_read_int(path_buff, &eq_index) == 0) {
                                    printf("eq_index: %d\n", eq_index);
                                } else {
                                    printf("eq_index fail");
                                }

                                match_flag = TRUE;
                                break;
                            }
                        } else {
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error, path:%s\n", path_buff);
                        }
                    } // for (j<eq_num)

                    if (TRUE == match_flag) {
                        match_flag = FALSE;
                        /* add event string */
                        switch (eq_index) {
                        case BAND_INDEX_BASS:
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->bass + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:%s:%d\n", str_name,
                                     g_wiimu_shm->bass + adjust_delta);
                            break;

                        case BAND_INDEX_MID:
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->eq_midrange + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:%s:%d\n", str_name,
                                     g_wiimu_shm->eq_midrange + adjust_delta);
                            break;

                        case BAND_INDEX_TREBLE:
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->treble + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:%s:%d\n", str_name,
                                     g_wiimu_shm->treble + adjust_delta);
                            break;

                        case BAND_INDEX_LOW:
                            break;

                        case BAND_INDEX_HIGH:
                            break;

                        default:
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error,eq_index:%d\n", eq_index);
                            break;
                        } // switch

                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error AdjustEQ receive unknow param: %s \n",
                                    str_name ? str_name : "get fail");
                    }

#if 0
                    if (StrEq(str_name, NAME_TREBLE) || StrEq(str_name, NAME_BASS) ||
                        StrEq(str_name, NAME_MIDRANGE)) {
                        if (StrEq(str_dir, "UP")) {
                            adjust_delta = delta;
                        } else if (StrEq(str_dir, "DOWN")) {
                            adjust_delta = -delta;
                        }

                        /* add event string */
                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        if (StrEq(str_name, NAME_TREBLE)) {
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->treble + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandTREBLE:%d\n",
                                     g_wiimu_shm->treble + adjust_delta);
                        } else if (StrEq(str_name, NAME_MIDRANGE)) {
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->eq_midrange + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandMID:%d\n",
                                     g_wiimu_shm->eq_midrange + adjust_delta);
                        } else if (StrEq(str_name, NAME_BASS)) {
                            alexa_parse_equalizercontroller_add_event(
                                js_event, str_name, g_wiimu_shm->bass + adjust_delta);
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandBASS:%d\n",
                                     g_wiimu_shm->bass + adjust_delta);
                        }

                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error AdjustEQ receive unknow param: %s \n",
                                    str_name);
                    }
#endif
                } // for (i < length)

                // send event back to AVS
                // alexa_parse_equalizercontroller_change_event();
                event_str = json_object_to_json_string(js_event);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "EqualizerChanged event: %s! \n", event_str);
                alexa_equalizer_controller_cmd(0, NAME_EQUALIZER_CHANGED, event_str);
                // free json object
                json_object_put(js_event);

                return 0;
            }
        }
    }

    return -1;
}

/*
{
    "directive": {
        "header": {
            "namespace": "EqualizerController",
            "name": "ResetBands",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "bands": [
                {
                    "name": "{{STRING}}"
                }
            ]
        }
    }
}
*/
static int alexa_parse_equalizercontroller_reset_bands(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_bands = json_object_object_get(js_payload, NAME_BANDS);
            if (js_bands) {
                char cmd_buf[128] = {0};
                json_object *js_item = NULL;
                json_object *js_name = NULL;
                json_object *js_event = json_object_new_object();
                const char *event_str = NULL;
                const char *str_name = NULL;
                int i, j;
                int eq_num = 0; // eqParamArray
                int match_flag = FALSE;
                int match_index = BAND_INDEX_NONE;
                char *eq_asr_name = NULL;
                char path_buff[LPCFG_MAX_PAH_LEN] = {'\0'};
                char path_head[LPCFG_MAX_PAH_LEN] = {'\0'};
                int solution_index = -1;
                int solution_num = 0;
                const char *solution_name = NULL;
                const char *solution_active = NULL;
                const char *head_str = "equallizer.eqSolutions.solutionsArray";
                int length = json_object_array_length(js_bands);

                for (i = 0; i < length; i++) {
                    js_item = json_object_array_get_idx(js_bands, i);
                    js_name = json_object_object_get(js_item, NAME_NAME);
                    str_name = json_object_get_string(js_name);

                    /* ********************************************** */
                    solution_index = 0;
                    memset(path_buff, 0, sizeof(path_buff));
                    snprintf(path_buff, sizeof(path_buff) - 1, "%s.%s.%s", "equallizer",
                             "eqSolutions", "solutionActive");
                    if (lpcfg_read_string(path_buff, (char **)&solution_active) == 0) {
                        solution_num = lpcfg_array_size(head_str);
                        for (i = 0; i < solution_num; i++) {
                            memset(path_buff, 0, sizeof(path_buff));
                            snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", head_str, i,
                                     "solutionName");
                            if (lpcfg_read_string(path_buff, (char **)&solution_name) == 0) {
                                if (strcmp(solution_active, solution_name) == 0) {
                                    solution_index = i;
                                    break;
                                }
                            }
                        } // for (..solution_num..);
                    }

                    memset(path_head, 0, sizeof(path_head));
                    snprintf(path_head, sizeof(path_head) - 1, "%s[%d].%s", head_str,
                             solution_index, "eqParamArray");

                    eq_num = lpcfg_array_size(path_head); // eq_num
                    for (j = 0; j < eq_num; j++) {
                        match_index = j;
                        memset(path_buff, 0, sizeof(path_buff));
                        snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", path_head,
                                 match_index, "eqAsrName");
                        if (lpcfg_read_string(path_buff, &eq_asr_name) ==
                            0) { // eqParamArray[i].eqAsrName
                            if (StrEq(str_name, eq_asr_name)) {
                                match_flag = TRUE;
                                break;
                            }
                        } else {
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error, path:%s\n", path_buff);
                        }
                    }

                    if (TRUE == match_flag) {
                        match_flag = FALSE;
                        /* add event string */
                        alexa_parse_equalizercontroller_add_event(js_event, str_name, 0);
                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:%s:%d\n", str_name, 0);
                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ receive unknow param: %s \n",
                                    str_name ? str_name : "get fail");
                    }
                } // for (..i < length..)

/* ********************************************** */
#if 0
                    /* add event string */
                    if (StrEq(str_name, NAME_TREBLE) || StrEq(str_name, NAME_BASS) ||
                        StrEq(str_name, NAME_MIDRANGE)) {
                        alexa_parse_equalizercontroller_add_event(js_event, str_name, 0);

                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        if (StrEq(str_name, NAME_TREBLE)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandTREBLE:%d\n", 0);
                        } else if (StrEq(str_name, NAME_MIDRANGE)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandMID:%d\n", 0);
                        } else if (StrEq(str_name, NAME_BASS)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandBASS:%d\n", 0);
                        }
                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }

                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error ResetEQ receive unknow param: %s \n",
                                    str_name);
                    }
#endif // 0

                // send event back to AVS
                // alexa_parse_equalizercontroller_change_event();
                event_str = json_object_to_json_string(js_event);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "EqualizerChanged event: %s! \n", event_str);
                alexa_equalizer_controller_cmd(0, NAME_EQUALIZER_CHANGED, event_str);
                // free json object
                json_object_put(js_event);

                return 0;
            }
        }
    }

    return -1;
}

/*
{
    "directive": {
        "header": {
            "namespace": "EqualizerController",
            "name": "SetBands",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "bands": [
                {
                    "name": "{{STRING}}",
                    "level": {{INTEGER}}
                }
            ]
        }
    }
}
*/
static int alexa_parse_equalizercontroller_set_bands(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_bands = json_object_object_get(js_payload, NAME_BANDS);
            if (js_bands) {
                char cmd_buf[128] = {0};
                json_object *js_item = NULL;
                json_object *js_name = NULL;
                json_object *js_level = NULL;
                json_object *js_event = json_object_new_object();
                const char *event_str = NULL;
                const char *str_name = NULL;
                int level = -1;
                int i, j;
                int eq_num = 0; // eqParamArray
                int match_flag = FALSE;
                int match_index = BAND_INDEX_NONE;
                int eq_range_min = 0; // range[x, y]
                char *eq_asr_name = NULL;
                char path_buff[LPCFG_MAX_PAH_LEN] = {'\0'};
                char path_head[LPCFG_MAX_PAH_LEN] = {'\0'};
                int length = json_object_array_length(js_bands);
                int solution_index = -1;
                int solution_num = 0;
                const char *solution_name = NULL;
                const char *solution_active = NULL;
                const char *head_str = "equallizer.eqSolutions.solutionsArray";
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "array length: %d\n", length);

                for (i = 0; i < length; i++) {
                    js_item = json_object_array_get_idx(js_bands, i);
                    js_name = json_object_object_get(js_item, NAME_NAME);
                    js_level = json_object_object_get(js_item, NAME_LEVEL);

                    str_name = json_object_get_string(js_name);
                    level = json_object_get_int(js_level); // level (0 ~ 10)

                    /* match lpcfg.json */
                    solution_index = 0; // default value
                    memset(path_buff, 0, sizeof(path_buff));
                    snprintf(path_buff, sizeof(path_buff) - 1, "%s.%s.%s", "equallizer",
                             "eqSolutions", "solutionActive");
                    if (lpcfg_read_string(path_buff, (char **)&solution_active) == 0) {
                        solution_num = lpcfg_array_size(head_str);
                        for (j = 0; j < solution_num; j++) {
                            memset(path_buff, 0, sizeof(path_buff));
                            snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", head_str, j,
                                     "solutionName");
                            if (lpcfg_read_string(path_buff, (char **)&solution_name) == 0) {
                                if (strcmp(solution_active, solution_name) == 0) {
                                    solution_index = j;
                                    break;
                                }
                            }
                        } // for (..solution_num..);
                    }
                    memset(path_head, 0, sizeof(path_head));
                    snprintf(path_head, sizeof(path_head) - 1, "%s[%d].%s", head_str,
                             solution_index, "eqParamArray");

                    eq_num = lpcfg_array_size(path_head); // eq_num
                    for (j = 0; j < eq_num; j++) {
                        match_index = j;
                        memset(path_buff, 0, sizeof(path_buff));
                        snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", path_head,
                                 match_index, "eqAsrName");
                        if (lpcfg_read_string(path_buff, &eq_asr_name) ==
                            0) { // eqParamArray[i].eqAsrName
                            if (StrEq(str_name, eq_asr_name)) {
                                memset(path_buff, 0, sizeof(path_buff));
                                snprintf(path_buff, sizeof(path_buff) - 1, "%s[%d].%s", path_head,
                                         match_index, "range[0]");
                                if (lpcfg_read_int(path_buff, &eq_range_min) == 0) {
                                    printf("eq_range_min:%d\n", eq_range_min);
                                } else {
                                    printf("eq_range_min fail");
                                }

                                match_flag = TRUE;
                                break;
                            }
                        } else {
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error, path:%s\n", path_buff);
                        }
                    }

                    if (TRUE == match_flag) {
                        match_flag = FALSE;
                        /* add event string */
                        alexa_parse_equalizercontroller_add_event(js_event, str_name,
                                                                  level); // 0 ~ 10
                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:%s:%d\n", str_name,
                                 level + eq_range_min); // -5 ~ 5
                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ receive unknow param: %s \n",
                                    str_name ? str_name : "get fail");
                    }
                } // for (i<length)

#if 0
                    if (StrEq(str_name, NAME_TREBLE) || StrEq(str_name, NAME_BASS) ||
                        StrEq(str_name, NAME_MIDRANGE)) {
                        alexa_parse_equalizercontroller_add_event(js_event, str_name, level);

                        // level (-5 ~ 5)
                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        if (StrEq(str_name, NAME_TREBLE)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandTREBLE:%d\n",
                                     level - 5);
                        } else if (StrEq(str_name, NAME_MIDRANGE)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandMID:%d\n",
                                     level - 5);
                        } else if (StrEq(str_name, NAME_BASS)) {
                            snprintf(cmd_buf, sizeof(cmd_buf), "EQValueSet:asr:BandBASS:%d\n",
                                     level - 5);
                        }
                        if (strlen(cmd_buf)) {
                            if (0 >= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf,
                                                              strlen(cmd_buf), NULL, NULL, 0)) {
                                ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ send cmd failed!\n");
                            }
                        }

                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Error SetEQ receive unknow param: %s \n",
                                    str_name);
                    }
#endif

                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "set band cmd OK !\n");
                // send event back to AVS
                // alexa_parse_equalizercontroller_change_event();
                event_str = json_object_to_json_string(js_event);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "EqualizerChanged event: %s! \n", event_str);
                alexa_equalizer_controller_cmd(0, NAME_EQUALIZER_CHANGED, event_str);
                // free json object
                json_object_put(js_event);

                return 0;
            }
        }
    }

    return -1;
}
/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -end*/

/*weiqiang.huang add for AVS Input Controller (v3.0) and AVS alert 1.3 2018-07-30 -- begin*/
/*parse directive Alert adjust volume */
static int alexa_parse_alert_adjust_volume(json_object *js_obj)
{

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *j_volume = json_object_object_get(js_payload, PAYLOAD_VOLUME);
            if (j_volume) {
                char in_buf[101] = {0};
                char out_buf[101] = {0};
                int outlen = -1;
                // get volume variable value
                int32_t volume = json_object_get_int(j_volume);

                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Adjust alert volume: %d", volume);

                // send msg to alert module to modify volume
                snprintf(in_buf, sizeof(in_buf), "ALT+VOL+ADJ+%d", volume);
                if (0 <= SocketClientReadWriteMsg(ALERT_SOCKET_NODE, in_buf, strlen(in_buf),
                                                  out_buf, &outlen, 0)) {

                    // send VolumeChanged event back to AVS
                    snprintf(in_buf, 100, "{\"volume\":\"%s\"}", out_buf);
                    alexa_alerts_cmd(0, NAME_ALERT_VOLUME_CHANGED,
                                     json_object_to_json_string(j_volume));
                }

                return 0;
            }
        }
    }

    return -1;
}

/*parse directive Alert set volume */
static int alexa_parse_alert_set_volume(json_object *js_obj)
{

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *j_volume = json_object_object_get(js_payload, PAYLOAD_VOLUME);
            if (j_volume) {
                char in_buf[101] = {0};
                char out_buf[101] = {0};
                int outlen = -1;
                // get volume variable value
                int32_t volume = json_object_get_int(j_volume);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Set alert volume to: %d", volume);

                // send msg to alert module to set volume
                snprintf(in_buf, sizeof(in_buf), "ALT+VOL+SET+%d", volume);
                if (0 <= SocketClientReadWriteMsg(ALERT_SOCKET_NODE, in_buf, strlen(in_buf),
                                                  out_buf, &outlen, 0)) {

                    // send VolumeChanged event back to AVS
                    alexa_alerts_cmd(0, NAME_ALERT_VOLUME_CHANGED,
                                     json_object_to_json_string(js_payload));
                }
                return 0;
            }
        }
    }
    return -1;
}

/*parse directive DeleteAlerts */
static int alexa_parse_delete_alerts(json_object *js_obj)
{

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_tokens = json_object_object_get(js_payload, PAYLOAD_TOKENS);

            // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "tokens: %s \n",
            // json_object_to_json_string(js_tokens));
            // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Json tokens type: %d \n",
            // (int)json_object_get_type(js_tokens));
            if (js_tokens) {
                json_object *jobj = NULL;
                const char *str_token = NULL;
                int i;
                int length = json_object_array_length(js_tokens);

                // do delete alerts
                for (i = 0; i < length; i++) {
                    jobj = json_object_array_get_idx(js_tokens, i);
                    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Array %d address is %d\n", i, (int)jobj);
                    str_token = json_object_get_string(jobj);
                    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Delete the %d of %d alerts: %s\n", i,
                    // length,
                    // (char*)str_token);
                    alert_delete(str_token);
                }
                // send event back to AVS
                alexa_alerts_cmd(0, NAME_DELETE_ALERTS_SUCCEEDED,
                                 json_object_to_json_string(js_tokens));
            }
        }
        return 0;
    }

    return -1;
}
/*weiqiang.huang add for AVS Input Controller (v3.0) and AVS alert 1.3 2018-07-30 -- end*/

static int alexa_parse_set_alert(json_object *js_obj)
{
    if (js_obj) {
        char _is_named_timer_reminder = NO;
        const char *json_string = NULL;
        json_string = json_object_to_json_string(js_obj);

        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
            char *time = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_SCHEDULEDTIME);

            /* Just Parse Assets Num*/
            json_object *temp1 = NULL;
            temp1 = json_object_object_get(js_payload, PAYLOAD_Assets);
            if (temp1) {
                _is_named_timer_reminder = YES;
            }

            if (token && type && time) {
                int ret = 0;
                if (_is_named_timer_reminder) {
                    ret = alert_set_ForNamedTimerAndReminder(token, type, time, json_string);
                } else {
                    ret = alert_set(token, type, time);
                }
                if (ret == 0) {
                    alexa_alerts_cmd(0, NAME_SETALERT_SUCCEEDED, token);
                } else {
                    alexa_alerts_cmd(0, NAME_SETALERT_FAILED, token);
                }
            }
        }

        return 0;
    }

    return -1;
}

static int alexa_parse_del_alert(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            if (token) {
                alert_stop(token);
                focus_mgmt_activity_status_set(Alerts, UNFOCUS);
                alexa_alerts_cmd(0, NAME_ALERT_STOPPED, token);
                alexa_alerts_cmd(0, NAME_DELETE_ALERT_SUCCEEDED, token);
            }
        }
        return 0;
    }

    return -1;
}

/*
SpeechRecognizer :
StopCapture Directive
{
  "directive": {
        "header": {
            "namespace": "SpeechRecognizer",
            "name": "StopCapture",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
        }
    }
}

This directive instructs your client to stop capturing a user's speech after
AVS has identified the user's intent or when end of speech is detected. When
this directive is received, your client must immediately close the microphone
and stop listening for the user's speech.

Note: StopCapture is sent to your client on the downchannel stream and may be
received while speech is still being streamed to AVS. To receive the
StopCapture directive, you must use a profile in your Recognize event that
supports cloud-endpointing, such as NEAR_FIELD or FAR_FIELD.
*/
static int alexa_parse_stop_capture(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        // TODO:  close the microphone and stop listening
        if (ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
            NOTIFY_CMD(ASR_TTS, VAD_FINISHED);
        }

        return 0;
    }

    return -1;
}

#ifdef AVS_MRM_ENABLE
int is_avs_mrm_accepted_name_space(char *name_space)
{
    int res = 0;
    char *msg = NULL;
    static char cached_acceptable_namespace[256] = {0}; /* cached namespace */

    if (AlexaDisableMrm()) {
        return 0;
    }

    if (name_space && 0 == strcmp(cached_acceptable_namespace, name_space)) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "name space [%s] is acceptable.\n", name_space);
        return 1;
    }

    asprintf(&msg, "GNOTIFY=isMrmAcceptedNameSpace:%s", name_space);

    if (msg) {
        char recv_msg[32] = {0};
        SocketClientReadWriteMsg("/tmp/alexa_mrm", msg, strlen(msg), recv_msg, &res, 1);

        free(msg);
        msg = NULL;

        if (res > 0) {
            /* only cache acceptable namespace */
            memset(cached_acceptable_namespace, 0, sizeof(cached_acceptable_namespace));
            strncpy(cached_acceptable_namespace, name_space,
                    sizeof(cached_acceptable_namespace) - 1);

            return atoi(recv_msg);
        }
    }

    return 0; /* default false */
}
#endif

int alexa_result_json_parse(char *message, int is_next)
{
    json_object *json_message = NULL;

    json_message = json_tokener_parse(message);
    if (json_message) {
        json_object *json_directive = json_object_object_get(json_message, KEY_DIRECTIVE);
        if (json_directive) {
            json_object *json_header = json_object_object_get(json_directive, KEY_HEADER);
            if (json_header) {
                char *name_space = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
                char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
                if (name && name_space) {
                    if (StrEq(name_space, NAMESPACE_TEMPLATERUNTIME)) {
                        ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[LOG_CHECK <%s>]%.100s\r\n", name,
                                       message);
                    } else {
#ifdef EN_AVS_COMMS
                        ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[LOG_CHECK <%s>]%.500s\r\n", name,
                                       message);
#else
                        ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[LOG_CHECK <%s>]%s\r\n", name, message);
#endif
                    }
                    if (!avs_emplayer_directive_check(name_space)) {
                        avs_emplayer_parse_directive(json_directive);

                    } else if (StrEq(name_space, NAMESPACE_TEMPLATERUNTIME)) {
                        avs_gui_parse_directive(json_directive);
#ifdef BLE_NOTIFY_ENABLE
                        char *msgContent = NULL;
                        unsigned char buf[10240] = {0};
                        char base_buf[10240] = {0};
                        unsigned long len = 10240;
                        unsigned long mlen = strlen(message);
                        compress(buf, &len, (unsigned char *)message, mlen);
                        base64_encode_string((unsigned char *)base_buf, buf, len);
                        asprintf(&msgContent, "%s:%s", "SPRINT_NOTIFY_TEMPLATE", base_buf);

                        if (msgContent) {
                            NOTIFY_CMD(A01Controller, msgContent);
                            free(msgContent);
                            msgContent = NULL;
                        }
#endif
                    } else if (StrEq(name_space, NAMESPACE_NOTIFICATIONS)) {
                        avs_notificaion_parse_directive(json_directive);
#ifdef EN_AVS_BLE
                    } else if (StrEq(name_space, NAMESPACE_Bluetooth)) {
                        avs_bluetooth_parse_directive(json_directive);
#endif
                    } else if (StrEq(name_space, NAMESPACE_AUDIOPLAYER) ||
                               StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER) ||
                               StrEq(name_space, NAMESPACE_SPEAKER) ||
                               StrEq(name, NAME_EXPECTSPEECH)) {
                        avs_player_parse_directvie(json_directive, is_next);

#ifdef EN_AVS_COMMS
                    } else if (StrEq(name_space, NAMESPACE_SIPUSERAGENT)) {
                        avs_comms_parse_directive(json_directive);

#endif
                        /*weiqiang.huang add for Input Controller (v3.0) -2018-07-30 -- begin*/
                    } else if (StrEq(name_space, NAMESPACE_INPUTCONTROLLER)) {
                        // AVS Input Controller (v3.0)
                        avs_input_controller_parse_directive(json_directive);
                        /*weiqiang.huang add for Input Controller (v3.0) -2018-07-30 -- end*/

                        /*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30
                         * -begin*/
                    } else if (StrEq(name_space, NAMESPACE_DONOTDISTURB)) {
                        avs_donot_disturb_parse_directive(json_directive);
                    } else if (StrEq(name_space, NAMESPACE_EQUALIZERCONTROLLER)) {

                        if (StrEq(name, NAME_SET_BANDS)) {
                            alexa_parse_equalizercontroller_set_bands(json_directive);
                        } else if (StrEq(name, NAME_ADJUST_BANDS)) {
                            alexa_parse_equalizercontroller_adjust_bands(json_directive);
                        } else if (StrEq(name, NAME_RESET_BANDS)) {
                            alexa_parse_equalizercontroller_reset_bands(json_directive);
                        } else if (StrEq(name, NAME_SET_MODE)) {
                            alexa_parse_equalizercontroller_set_mode(json_directive);
                        }
                        /*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30
                         * -end*/
                    } else if (StrEq(name_space, NAMESPACE_ALERTS) &&
                               StrEq(name, NAME_DELETE_ALERTS)) {
                        // AVS alert 1.3
                        alexa_parse_delete_alerts(json_directive);
                    } else if (StrEq(name_space, NAMESPACE_ALERTS) &&
                               StrEq(name, NAME_ALERT_SET_VOLUME)) {
                        // AVS alert 1.3
                        alexa_parse_alert_set_volume(json_directive);
                    } else if (StrEq(name_space, NAMESPACE_ALERTS) &&
                               StrEq(name, NAME_ALERT_ADJUST_VOLUME)) {
                        alexa_parse_alert_adjust_volume(json_directive);
                        /*weiqiang.huang add for Input Controller (v3.0) and AVS alert 1.3
                         * 2018-07-30 -- end*/
                    } else if (StrEq(name_space, NAMESPACE_SYSTEM)) {
                        avs_system_parse_directive(json_directive);
                    } else if (StrEq(name, NAME_SETALERT)) {
                        alexa_parse_set_alert(json_directive);

                    } else if (StrEq(name, NAME_DELETE_ALERT)) {
                        alexa_parse_del_alert(json_directive);

                    } else if (StrEq(name, NAME_STOPCAPTURE)) {
                        alexa_parse_stop_capture(json_directive);
                        /*add by weiqiang.huang for Routines 2018-11-08 -begin*/
                    } else if (StrEq(name, "NewDialogRequest")) {
                        // do nothing, just avoid send ExceptionEncountered event to AVS
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Rountines command will receive (%s:%s)\n",
                                    name_space, name);
                        /*add by weiqiang.huang for Routines 2018-11-08 -end*/
                    } else {
#ifdef AVS_MRM_ENABLE
                        if (is_avs_mrm_accepted_name_space(name_space)) {
                            char *msgContent = NULL;
                            asprintf(&msgContent, "%s=%s", "DirectiveNotify", message);

                            if (msgContent) {
                                NOTIFY_CMD("/tmp/alexa_mrm", msgContent);
                                free(msgContent);
                                msgContent = NULL;
                            }
                        } else
#endif
                        {
                            alexa_system_cmd(0, NAME_ExceptionEncountered, 0, NULL, message);
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "unknown directive (%s:%s)\n", name_space,
                                        name);
                        }
                    }
                }
            }
        }

        json_object_put(json_message);
        return 0;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase message error\n");

    return -1;
}

void alexa_clear_authinfo(void)
{
    int len = 0;
    char recv_buff[128] = {0};

    NOTIFY_CMD(ASR_TTS, AVS_UNAUTH);
    SocketClientReadWriteMsg(AMAZON_TTS, AVS_LOGOUT, strlen(AVS_LOGOUT), recv_buff, &len, 0);
#ifdef S11_EVB_EUFY_DOT
    NOTIFY_CMD(NET_GUARD, Alexa_Logout);
#endif

#ifdef AVS_IOT_CTRL
    NOTIFY_CMD(AWS_CLIENT, Alexa_Logout);
#endif
}

int alexa_unauthor_json_parse(char *message)
{
    int ret = -1;
    json_object *json_message = NULL;
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "alexa_unauthor_json_parse ++++\n");

    json_message = json_tokener_parse(message);
    if (json_message) {
        json_object *js_payload = json_object_object_get(json_message, KEY_PAYLOAD);
        if (js_payload) {
            char *res = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_CODE);
            if (res) {
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "alexa_unauthor_json_parse:%s\n", res);
                if (StrInclude(res, PAYLOAD_UNAUTHORIZED_REQUEST_EXCEPTION)) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "into this function++++\n");
                    alexa_clear_authinfo();
                    ret = 0;
                }
            }
        } else {
            char *res = JSON_GET_STRING_FROM_OBJECT(json_message, PAYLOAD_ERROR);
            if (res) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_unauthor_json_parse %s:\n", res);
                // modify by weiqiang.huang for STAB-275 2018-08-28 -- begin*/
                if (StrInclude(res, ERROR_INVALID_GRANT) ||
                    StrInclude(res, ERROR_UNAUTHORIZED_CLIENT) ||
                    StrInclude(res, ERROR_INVALID_REQUEST)) {
                    /*modify by weiqiang.huang for STAB-275 2018-08-28 -- end*/
                    alexa_clear_authinfo();
                    ret = 0;
                }
            }
        }
        json_object_put(json_message);
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "alexa_unauthor_json_parse ----\n");
    return ret;
}
