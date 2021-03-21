#include <stdio.h>
#include <stdlib.h>

#include "json.h"
#include "asr_json_common.h"

#include "clova_context.h"

int clova_clova_location_state(json_object *context_list)
{
    json_object *location_payload = NULL;
    char *latitude = NULL;
    char *longitude = NULL;
    char *refreshedAt = NULL;
    int ret = -1;

    if (context_list == NULL) {
        goto EXIT;
    }

    location_payload = json_object_new_object();
    if (location_payload == NULL) {
        goto EXIT;
    }

    clova_get_location_info_for_context(location_payload);

    ret = asr_context_list_add(context_list, CLOVA_HEADER_CLOVA_NAMESPACE,
                               CLOVA_HEADER_LOCATION_NAME, location_payload);

EXIT:

    if (ret == -1 && location_payload != NULL) {
        json_object_put(location_payload);
    }

    return ret;
}

int clova_device_display_state(json_object *context_list)
{
    json_object *display_payload = NULL;
    int ret = -1;

    if (context_list == NULL) {
        goto EXIT;
    }

    display_payload = json_object_new_object();
    if (display_payload == NULL) {
        goto EXIT;
    }

    json_object_object_add(display_payload, CLOVA_PAYLOAD_SIZE_NAME,
                           json_object_new_string(CLOVA_PAYLOAD_SIZE_VALUE_CUSTOM));

    ret = asr_context_list_add(context_list, CLOVA_HEADER_DEVICE_NAMESPACE,
                               CLOVA_HEADER_DEVICE_DISPLAY_NAME, display_payload);

EXIT:

    if (ret == -1 && display_payload != NULL) {
        json_object_put(display_payload);
    }

    return ret;
}

int clova_clova_saved_place_state(json_object *context_list)
{
    int ret = -1;
    int rv = 0;
    json_object *saved_place_payload = NULL;
    json_object *saved_place_array = NULL;
    json_object *saved_place_array_item = NULL;

    if (context_list == NULL) {
        goto EXIT;
    }

    saved_place_payload = json_object_new_object();
    if (saved_place_payload == NULL) {
        rv = -1;
        goto EXIT;
    }

    saved_place_array = json_object_new_array();
    if (saved_place_array == NULL) {
        rv = -1;
        goto EXIT;
    }

    saved_place_array_item = json_object_new_object();
    if (saved_place_array_item == NULL) {
        rv = -1;
        goto EXIT;
    }

    json_object_object_add(saved_place_payload, CLOVA_PAYLOAD_PLACES_NAME, saved_place_array);

    rv = json_object_array_add(saved_place_array, saved_place_array_item);
    if (rv != 0) {
        rv = -1;
        goto EXIT;
    }

    json_object_object_add(saved_place_array_item, CLOVA_PAYLOAD_LATITUDE_NAME,
                           json_object_new_string(""));
    json_object_object_add(saved_place_array_item, CLOVA_PAYLOAD_LONGITUDE_NAME,
                           json_object_new_string(""));
    json_object_object_add(saved_place_array_item, CLOVA_PAYLOAD_REFRESHEDAT_NAME,
                           json_object_new_string(""));
    json_object_object_add(saved_place_array_item, CLOVA_PAYLOAD_NAME_NAME,
                           json_object_new_string(""));

    ret = asr_context_list_add(context_list, CLOVA_HEADER_CLOVA_NAMESPACE,
                               CLOVA_HEADER_SAVED_PLACE_NAME, saved_place_payload);

EXIT:

    if (rv != 0) {
        if (saved_place_array_item) {
            json_object_put(saved_place_array_item);
        }
        if (saved_place_array) {
            json_object_put(saved_place_array);
        }
        if (saved_place_payload) {
            json_object_put(saved_place_payload);
        }
    } else if (ret != 0) {
        if (saved_place_payload) {
            json_object_put(saved_place_payload);
        }
    }

    return ret;
}

int clova_speech_synthesizer_state(json_object *context_list)
{
    json_object *speech_synthesizer_payload = NULL;
    int ret = -1;

    if (context_list == NULL) {
        goto EXIT;
    }

    speech_synthesizer_payload = json_object_new_object();
    if (speech_synthesizer_payload == NULL) {
        goto EXIT;
    }

    json_object_object_add(speech_synthesizer_payload, CLOVA_PAYLOAD_SPEECH_TOKEN_NAME,
                           json_object_new_string(""));
    json_object_object_add(
        speech_synthesizer_payload, CLOVA_PAYLOAD_SPEECH_PLAYERACTIVITY_NAME,
        json_object_new_string(CLOVA_PAYLOAD_SPEECH_PLAYERACTIVITY_VALUE_FINISHED));

    ret = asr_context_list_add(context_list, CLOVA_HEADER_SPEECHSYNTHESIZER_NAMESPACE,
                               CLOVA_HEADER_SPEECH_STATE_NAME, speech_synthesizer_payload);

EXIT:

    if (ret == -1 && speech_synthesizer_payload != NULL) {
        json_object_put(speech_synthesizer_payload);
    }

    return ret;
}