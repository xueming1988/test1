#ifndef _ALEXA_MRM_H_
#define _ALEXA_MRM_H_

#ifdef __cplusplus
extern "C" {
#endif

int avs_mrm_parse_cmd(char *json_cmd_str);

int avs_mrm_cluster_context_state(json_object *js_context_list);

int avs_mrm_notify_network_state_change(int online);

#ifdef __cplusplus
}
#endif

#endif /* _ALEXA_MRM_H_ */
