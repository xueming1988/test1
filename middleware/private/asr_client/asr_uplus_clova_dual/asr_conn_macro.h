
#ifndef __ASR_CONN_MACRO_H__
#define __ASR_CONN_MACRO_H__

// { For CLOVA

#define PING_PATH ("/ping")
#define EVENTS_PATH ("/v1/events")
#define DIRECTIVES_PATH ("/v1/directives")
#define EVENT_CONTENTTYPE ("multipart/form-data; boundary=--abcdefg123456")

#define EVENT_PART                                                                                 \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"metadata\"\r\n"                                       \
     "Content-Type: application/json; charset=UTF-8\r\n\r\n"                                       \
     "%s\r\n----abcdefg123456--\r\n")

#define RECON_PART                                                                                 \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"metadata\"\r\n"                                       \
     "Content-Type: application/json; charset=UTF-8\r\n\r\n"                                       \
     "%s\r\n\r\n----abcdefg123456\r\n"                                                             \
     "Content-Disposition: form-data; name=\"audio\"\r\n"                                          \
     "Content-Type: application/octet-stream\r\n\r\n")

#define CHUNCK_OCTET_STREAM ("application/octet-stream")
#define CHUNCK_JSON ("application/json; charset=utf-8")
#define HEADER_AUTH ("Bearer %s")
#define HOST_URL ("prod-ni-cic.clova.ai")

#define REQUEST_USER_AGENT ("CLOVA/DONUT/%s (Android 7.1.1;qcom;clovasdk=1.6.0)")

#define CONTENT_STREAM ("Content-Type: application/octet-stream")

// } For CLOVA

// { For LGU+

#define LGUP_PING_PATH ("/aiif/ping/ping.do")
#define LGUP_EVENTS_PATH ("/aiif/event/event.do")
#define LGUP_DIRECTIVES_PATH ("/aiif/directive/directive.do")
#define LGUP_TTS_PATH ("/aiif/directive/speechSynthesize.do")

#define LGUP_EVENT_PART                                                                            \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"jsondata\"\r\n"                                       \
     "Content-Type: application/json\r\n\r\n"                                                      \
     "%s\r\n\r\n----abcdefg123456--\r\n")

#define LGUP_RECON_PART                                                                            \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"jsondata\"\r\n"                                       \
     "Content-Type: application/json\r\n\r\n"                                                      \
     "%s\r\n\r\n----abcdefg123456\r\n"                                                             \
     "Content-Disposition: form-data; name=\"pcm\"; filename=\"trid_seq.pcm\"\r\n"                 \
     "Content-Type: audio/x-wav; codec=pcm; rate=16000\r\n\r\n")

#define LGUP_CHUNCK_OCTET_STREAM ("audio/x-wav;codec=pcm;rate=22050")
#define LGUP_CHUNCK_JSON ("application/json; charset=UTF-8")
#define LGUP_HEADER_AUTH ("TOKEN_TYPE_API %s")
#define LGUP_HOST_URL ("daiif.ai.uplus.co.kr")

#define LGUPLUS_RECORD_LEN (20480)
#define LGUPLUS_BUFFER_LEN (20480 + 23)
#define ASR_RECON_PART_END ("\r\n----abcdefg123456--\r\n")

// } For LGU+

// { COMM

#define BOUNDARY ("boundary=")
#define CONTENT_TYPE ("Content-Type")
#define CONTENT_ID ("Content-Id")
#define CONTENT_JSON (0)
#define CONTENT_AUDIO (1)

// } COMM

#endif
