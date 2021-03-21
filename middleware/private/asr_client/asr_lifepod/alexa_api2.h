#ifndef __ALEXA_API2_H__
#define __ALEXA_API2_H__

#define TAB_ALEXA_IP ("176.32.100.25")

/*
54.239.26.171
54.239.25.188
*/
#define ALEXA_SERVER_IP ("54.239.23.243")

/*
 *  UK: https://avs-alexa-eu.amazon.com
 */
#ifdef BAIDU_DUMI // BAIDU_DUMI

#define ALEXA_API2_HOST ("https://dueros-h2.baidu.com")
#define ALEXA_API2_VERSION ("v20160207")
#define ALEXA_API2_EVENTS ("events")
#define ALEXA_API2_DIRECTIVES ("directives")

#define ALEXA_SEND_EVENTS ("%s/%s/events")         /* host/{{API version}}/events */
#define ALEXA_SEND_DIRECTIVES ("%s/%s/directives") /* host/{{API version}}/directives */

#define ALEXA_SEND_EVENTS_URL ("https://dueros-h2.baidu.com/dcs/avs-compatible-v20160207/events")

#define ALEXA_SEND_DIRECTIVES_URL                                                                  \
    ("https://dueros-h2.baidu.com/dcs/avs-compatible-v20160207/directives")

#define ALEXA_SEND_PING_URL ("https://dueros-h2.baidu.com/ping")

#define ALEXA_HEAD_HOST ("Host: dueros-h2.baidu.com")

#define ALEXA_LOGIN_HOST ("Host: openapi.baidu.com")

#define ALEXA_PAYLOAD_JSON ("Content-Type: application/json; charset=utf-8")

#elif defined(TENCENT) // tencent

#define ALEXA_API2_HOST ("https://tvs.html5.qq.com")
#define ALEXA_API2_VERSION ("v20160207")
#define ALEXA_API2_EVENTS ("events")
#define ALEXA_API2_DIRECTIVES ("directives")

#define ALEXA_SEND_EVENTS ("%s/%s/events")         /* host/{{API version}}/events */
#define ALEXA_SEND_DIRECTIVES ("%s/%s/directives") /* host/{{API version}}/directives */

#define ALEXA_SEND_EVENTS_URL ("https://tvs.html5.qq.com/dcs/avs-compatible-v20160207/events")

#define ALEXA_SEND_DIRECTIVES_URL                                                                  \
    ("https://tvs.html5.qq.com/dcs/avs-compatible-v20160207/directives")

#define ALEXA_SEND_PING_URL ("https://tvs.html5.qq.com/ping")

#define ALEXA_HEAD_HOST ("Host: tvs.html5.qq.com")

// TODO: need to changed the login host
#define ALEXA_LOGIN_HOST ("Host: api.amazon.com")

#define ALEXA_PAYLOAD_JSON ("Content-Type: application/json; charset=utf-8")

#else

#define ALEXA_API2_HOST ("https://avs-alexa-na.amazon.com")
#define ALEXA_API2_VERSION ("v20160207")
#define ALEXA_API2_EVENTS ("events")
#define ALEXA_API2_DIRECTIVES ("directives")

#define ALEXA_SEND_EVENTS ("%s/%s/events")         /* host/{{API version}}/events */
#define ALEXA_SEND_DIRECTIVES ("%s/%s/directives") /* host/{{API version}}/directives */

#define ALEXA_SEND_EVENTS_URL ("https://avs-alexa-na.amazon.com/v20160207/events")

#define ALEXA_SEND_DIRECTIVES_URL ("https://avs-alexa-na.amazon.com/v20160207/directives")

#define ALEXA_SEND_PING_URL ("https://avs-alexa-na.amazon.com/ping")

#define ALEXA_HEAD_HOST ("Host: avs-alexa-na.amazon.com")

#define ALEXA_LOGIN_HOST ("Host: api.amazon.com")

#define ALEXA_PAYLOAD_JSON ("Content-Type: application/json")

#endif // AVS

#define ALEXA_AUTHORIZATION ("Authorization:Bearer %s")

#define ALEXA_MULTIPART_TYPE ("Content-Type: multipart/form-data")

#define ALEXA_POSTPART_METADATA ("metadata")

#define ALEXA_MULTIPART_JSON ("application/json;charset=UTF-8")

#define ALEXA_POSTPART_AUDIO ("audio")

#define ALEXA_MULTIPART_STREAM ("application/octet-stream")

#define ALEXA_CONTENT_STREAM ("Content-Type: application/octet-stream")

#define ALEXA_EVENT_SynchronizeState ("SynchronizeState")

#if ((LIBCURL_VERSION_MINOR >= 49) && USED_CURL)
#define ALEXA_HTTP2_200 ("HTTP/2 200")
#define ALEXA_HTTP2_403 ("HTTP/2 403")
#else
#define ALEXA_HTTP2_200 ("HTTP/2.0 200")
#define ALEXA_HTTP2_403 ("HTTP/2.0 403")
#endif

// time * ms
#define PING_TIME (5 * 60 * 1000L)

#define INATIVE_TIME ((3600 - 10) * 1000L)

#define DELAY_MIN_TIME (3000L)

#define ALEXA_TOKEN_HOST "Host: api.amazon.com"
#define LIFEPOD_TOKEN_URL ("https://avs-auth.lifepod.com/token")

#endif
