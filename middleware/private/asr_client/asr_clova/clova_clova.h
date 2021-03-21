#ifndef __CLOVA_CLOVA__
#define __CLOVA_CLOVA__

#define CLOVA_HEADER_EXPECTLOGIN "ExpectLogin"

#define CLOVA_HEADER_HANDLEDELEGATEDEVENT "HandleDelegatedEvent"

#define CLOVA_HEADER_HELLO "Hello"

#define CLOVA_HEADER_HELP "Help"

#define CLOVA_HEADER_PROCESSDELEGATEDEVENT "ProcessDelegatedEvent"

#define CLOVA_HEADER_RENDERTEMPLATE "RenderTemplate"

#define CLOVA_HEADER_RENDERTEXT "RenderText"

#define CLOVA_HEADER_STARTEXTENSION "StartExtension"

#define CLOVA_HEADER_FINISHEXTENSION "FinishExtension"

#define CLOVA_PAYLOAD_KEY_EXTENSION "extension"

#define CLOVA_PAYLOAD_VAL_FREETALKING "freetalking"

#define CLOVA_PAYLOAD_VAL_DELEGATIONID "delegationId"

#define CLOVA_PAYLOAD_VOICETYPE "voiceType"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int clova_parse_clova_directive(json_object *json_directive);

int clova_get_extension_state(void);

int clova_clova_extension_state(json_object *context_list);

void clova_clear_extension_state(void);

int clova_set_voice_type(char *voice_type);

#ifdef __cplusplus
}
#endif

#endif
