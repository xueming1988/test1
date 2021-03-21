#ifndef __ALEXA_COMMS_MACRO_H__
#define __ALEXA_COMMS_MACRO_H__

#define AVS_CLIENT ("/tmp/ALEXA_EVENT")

#define Key_Event ("event")
#define Key_Event_type ("event_type")
#define Key_Event_params ("params")

#define Type_SipPhone ("sip_phone")

#define Event_State_Updated ("state_updated")
#define Event_Stop_Ring ("stop_ring")
#define Event_Show_Loghts ("show_lights")
#define Event_Send_Events ("send_events")
#define Event_Get_State_Cxt ("Get_State_Cxt")
#define Event_Channel_Focus ("channel_focus")

#define Key_pre_state ("pre_state")
#define Key_cur_state ("cur_state")
#define Key_url ("url")
#define Key_path ("path")
#define Key_need_loop ("need_loop")
#define Key_loop_timeout ("loop_timeout")

#define Key_call_state ("state")

#define Key_event_name ("event_name")
#define Key_payload ("payload")

#define Key_state ("state")

#define Key_channel_on ("channel_on")
#define Key_channel_name ("channel_name")
#define Key_Stop ("Stop")
#define Key_Accept ("Accept")

#define STATE_ANY ("ANY")
#define STATE_UNREGISTERED ("UNREGISTERED")
#define STATE_IDLE ("IDLE")
#define STATE_TRYING ("TRYING")
#define STATE_OUTBOUND_LOCAL_RINGING ("OUTBOUND_LOCAL_RINGING")
#define STATE_OUTBOUND_PROVIDER_RINGING ("OUTBOUND_PROVIDER_RINGING")
#define STATE_ACTIVE ("ACTIVE")
#define STATE_INVITED ("INVITED")
#define STATE_INBOUND_RINGING ("INBOUND_RINGING")
#define STATE_STOPPING ("STOPPING")

#define LED_CALL_INBOUND_RINGING ("GNOTIFY=MCULEDCALI") /*INBOUND*/
#define LED_CALL_CONNECTING ("GNOTIFY=MCULEDCALO")      /*OUTBOUND*/
#define LED_CALL_CONNECTED ("GNOTIFY=MCULEDCALA")       /*ACTIVE*/
#define LED_CALL_DISCONNECTED ("GNOTIFY=MCULEDCALN")
#define LED_CALL_QUIT ("GNOTIFY=MCULEDCALQ") /*QUIT*/
//#define LED_CALL_NA               (NULL)
//#define LED_CALL_CALLING          ("GNOTIFY=MCULEDCALC")/*CALLING*/

#define RING_SRC_CALL_NA ("")
#define RING_SRC_CALL_URL ("")
#define RING_SRC_CALL_OUTBOUND ("system_comm_outbound_ringtone.mp3")
#define RING_SRC_CALL_DISCONNECTED ("system_comm_call_disconnected.mp3")
#define RING_SRC_DROP_IN_CONNECTED ("system_comm_drop_in_connected.mp3")
#define RING_SRC_CALL_CONNECTED ("system_comm_call_connected.mp3")
#define RING_SRC_CALL_INCOMING_RINGTONE_INTRO ("system_comm_call_incoming_ringtone_intro.mp3")

#define RING_DO_LOOP 1
#define RING_NO_LOOP 0
#endif
