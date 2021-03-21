/*
 *created by richard.huang 2018-07-27
 *for Input Controller (v3.0)
 *amazone reference document :
 *https://developer.amazon.com/zh/docs/alexa-voice-service/inputcontroller.html
 */
#ifndef __ALEXA_INPUT_CONTROLLER_H__
#define __ALEXA_INPUT_CONTROLLER_H__

typedef struct json_object json_object;

#ifdef __cplusplus
extern "C" {
#endif

void avs_input_controller_do_cmd(void);

int avs_input_controller_parse_directive(json_object *js_data);

#ifdef __cplusplus
}
#endif

#endif
