/*
 * TLS/SSL Protocol
 * Copyright (c) 2011 Martin Storsjo
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avformat.h"
#include "url.h"
#include "libavutil/avstring.h"
#if CONFIG_GNUTLS
#include <gnutls/gnutls.h>
#define TLS_read(c, buf, size)  gnutls_record_recv(c->session, buf, size)
#define TLS_write(c, buf, size) gnutls_record_send(c->session, buf, size)
#define TLS_shutdown(c)         gnutls_bye(c->session, GNUTLS_SHUT_RDWR)
#define TLS_free(c) do { \
        if (c->session) \
            gnutls_deinit(c->session); \
        if (c->cred) \
            gnutls_certificate_free_credentials(c->cred); \
    } while (0)
#elif CONFIG_OPENSSL
#include <errno.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define TLS_read(c, buf, size)  SSL_read(c->ssl,  buf, size)
#define TLS_write(c, buf, size) SSL_write(c->ssl, buf, size)
#define TLS_shutdown(c)         SSL_shutdown(c->ssl)
#define TLS_free(c) do { \
        if (c->ssl) \
            SSL_free(c->ssl); \
        if (c->ctx) \
            SSL_CTX_free(c->ctx); \
    } while (0)
#endif
#include "network.h"
#include "os_support.h"
#include "internal.h"
#if HAVE_POLL_H
#include <poll.h>
#endif

typedef struct {
    const AVClass *class;
    URLContext *tcp;
#if CONFIG_GNUTLS
    gnutls_session_t session;
    gnutls_certificate_credentials_t cred;
#elif CONFIG_OPENSSL
    SSL_CTX *ctx;
    SSL *ssl;
	BIO *client_bio;
#endif
    int fd;
} TLSContext;

extern long long tickSincePowerOn(void);

#define TLS_TIMEOUT
#define TIME_OUT    (50*2)

static int do_tls_poll_timeout(URLContext *h, int ret, int timeout)
{
    TLSContext *c = h->priv_data;
    struct pollfd p = { c->fd, 0, 0 };

#if CONFIG_GNUTLS
    if (ret != GNUTLS_E_AGAIN && ret != GNUTLS_E_INTERRUPTED) {
        av_log(h, AV_LOG_ERROR, "%s\n", gnutls_strerror(ret));
        return AVERROR(EIO);
    }
    if (gnutls_record_get_direction(c->session))
        p.events = POLLOUT;
    else
        p.events = POLLIN;
#elif CONFIG_OPENSSL
    ret = SSL_get_error(c->ssl, ret);
    if (ret == SSL_ERROR_WANT_READ ) {
        p.events = POLLIN;
	} else if (ret == SSL_ERROR_WANT_WRITE) {
        p.events = POLLOUT;
    } else {
        av_log(h, AV_LOG_ERROR, "[imzuop] do_tls_poll_timeout error ret=%d errno %d msg %s ssl_msg:%s\n", \
                    ret, errno, strerror(errno), ERR_error_string(ERR_get_error(), NULL));
        return AVERROR(EIO);
    }
#endif
    if (h->flags & AVIO_FLAG_NONBLOCK) {
        av_log(h, AV_LOG_ERROR, "[imzuop] do_tls_poll_timeout AVIO_FLAG_NONBLOCK is set\n");
        return AVERROR(EAGAIN);
    }

    if (timeout > 0) {
        /* wait until we are connected or until abort */
        while(timeout --) {
            if (ff_check_interrupt(&h->interrupt_callback)) {
                ret = -1;
                goto fail;
            }
            ret = poll(&p, 1, 100);
			//av_log(h, AV_LOG_ERROR, " poll %d \r\n",ret);
            if (ret > 0)
                break;
        }
        if (ret <= 0) {
            ret = -1;
            goto fail;
        }
    } else {
        while (1) {
            int n = poll(&p, 1, 100);
            if (n > 0)
                break;
            if (ff_check_interrupt(&h->interrupt_callback))
                return AVERROR(EINTR);
        }
    }
    
    return 0;

fail:
    av_log(h, AV_LOG_ERROR, "[imzuop] do_tls_poll_timeout timeout(%d) ret(%d)\n", \
        timeout, ret);

    return ret;
}

#if 0
static int do_tls_poll(URLContext *h, int ret)
{
    TLSContext *c = h->priv_data;
    struct pollfd p = { c->fd, 0, 0 };

#if CONFIG_GNUTLS
    if (ret != GNUTLS_E_AGAIN && ret != GNUTLS_E_INTERRUPTED) {
        av_log(h, AV_LOG_ERROR, "%s\n", gnutls_strerror(ret));
        return AVERROR(EIO);
    }
    if (gnutls_record_get_direction(c->session))
        p.events = POLLOUT;
    else
        p.events = POLLIN;
#elif CONFIG_OPENSSL
    ret = SSL_get_error(c->ssl, ret);
    if (ret == SSL_ERROR_WANT_READ) {
        p.events = POLLIN;
    } else if (ret == SSL_ERROR_WANT_WRITE) {
        p.events = POLLOUT;
    } else {
        av_log(h, AV_LOG_ERROR, "[imzuop] do_tls_poll error ret=%d errno %d msg %s ssl_msg:%s\n", \
                    ret, errno, strerror(errno), ERR_error_string(ERR_get_error(), NULL));
        return AVERROR(EIO);
    }
#endif
    if (h->flags & AVIO_FLAG_NONBLOCK) {
        av_log(h, AV_LOG_ERROR, "[imzuop] do_tls_poll AVIO_FLAG_NONBLOCK is set\n");
        return AVERROR(EAGAIN);
    }

    while (1) {
        int n = poll(&p, 1, 100);
        if (n > 0)
            break;
        if (ff_check_interrupt(&h->interrupt_callback))
            return AVERROR(EINTR);
    }

    return 0;
}
#endif

#ifdef ASR_ALEXA2
#undef USED_CERT
#endif

#ifdef USED_CERT
#define CERT_FILE               ("/etc/ssl/certs/ca-certificates.crt")
#define KEY_FILE                ("/system/workdir/misc/key.pem")
#endif
static int tls_open(URLContext *h, const char *uri, int flags)
{
    TLSContext *c = h->priv_data;
    int ret;
    int port;
    char buf[200], host[200];
    int numerichost = 0;
    struct addrinfo hints = { 0 }, *ai = NULL;
    const char *proxy_path;
    int use_proxy;
    long long conn_time = 0;
	int tryConnect = 4;
    ff_tls_init();
	
	c->ssl = NULL;
	c->ctx = NULL;
	c->tcp = NULL;
	av_log(h, AV_LOG_ERROR, "[imzuop %lld] tls_open %s\n", tickSincePowerOn(), uri);
	
    proxy_path = getenv("http_proxy");
    use_proxy = (proxy_path != NULL) && !getenv("no_proxy") &&
        av_strstart(proxy_path, "http://", NULL);

    av_url_split(NULL, 0, NULL, 0, host, sizeof(host), &port, NULL, 0, uri);
    ff_url_join(buf, sizeof(buf), "tcp", NULL, host, port, NULL);
	hints.ai_family =  AF_INET;
    hints.ai_flags = AI_NUMERICHOST;

    long long test_time = tickSincePowerOn();
    if (!getaddrinfo(host, NULL, &hints, &ai)) {
        numerichost = 1;
        freeaddrinfo(ai);
    }

    if (use_proxy) {
        char proxy_host[200], proxy_auth[200], dest[200];
        int proxy_port;
        av_url_split(NULL, 0, proxy_auth, sizeof(proxy_auth),
                     proxy_host, sizeof(proxy_host), &proxy_port, NULL, 0,
                     proxy_path);
        ff_url_join(dest, sizeof(dest), NULL, NULL, host, port, NULL);
        ff_url_join(buf, sizeof(buf), "httpproxy", proxy_auth, proxy_host,
                    proxy_port, "/%s", dest);
    }
	
    ret = ffurl_open(&c->tcp, buf, AVIO_FLAG_READ_WRITE,
                     &h->interrupt_callback, NULL);
    if (ret)
        goto fail;
    c->fd = ffurl_get_file_handle(c->tcp);

#if CONFIG_GNUTLS
    gnutls_init(&c->session, GNUTLS_CLIENT);
    if (!numerichost)
        gnutls_server_name_set(c->session, GNUTLS_NAME_DNS, host, strlen(host));
    gnutls_certificate_allocate_credentials(&c->cred);
    gnutls_certificate_set_verify_flags(c->cred, 0);
    gnutls_credentials_set(c->session, GNUTLS_CRD_CERTIFICATE, c->cred);
    gnutls_transport_set_ptr(c->session, (gnutls_transport_ptr_t)
                                         (intptr_t) c->fd);
    gnutls_priority_set_direct(c->session, "NORMAL", NULL);
    while (1) {
        ret = gnutls_handshake(c->session);
        if (ret == 0)
            break;
        if ((ret = do_tls_poll_timeout(h, ret, 0)) < 0)
            goto fail;
    }
#elif CONFIG_OPENSSL
    // may need to configure ssl_ctx
    c->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!c->ctx) {
        av_log(h, AV_LOG_ERROR, "[imzuop]SSL_CTX_new %s\n", ERR_error_string(ERR_get_error(), NULL));
        ret = AVERROR(EIO);
        goto fail;
    }
#ifdef USED_CERT
    SSL_CTX_set_verify(c->ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_load_verify_locations(c->ctx, CERT_FILE, NULL);
    SSL_CTX_use_certificate_file(c->ctx, CERT_FILE, SSL_FILETYPE_PEM);
    //SSL_CTX_use_PrivateKey_file(c->ctx, KEY_FILE, SSL_FILETYPE_PEM);
    //SSL_CTX_check_private_key(c->ctx);
#endif
    c->ssl = SSL_new(c->ctx);
    if (!c->ssl) {
        av_log(h, AV_LOG_ERROR, "[imzuop]SSL_new %s\n", ERR_error_string(ERR_get_error(), NULL));
        ret = AVERROR(EIO);
        goto fail;
    }

//#ifdef TLS_TIMEOUT
//    SSL_set_connect_state(c->ssl);
//#endif
	c->client_bio = BIO_new_socket(c->fd, BIO_NOCLOSE);
	SSL_set_bio(c->ssl, c->client_bio, c->client_bio);
	BIO_set_nbio(c->client_bio, 1); /* nonblocking */

  //  SSL_set_fd(c->ssl, c->fd);
    if (!numerichost)
        SSL_set_tlsext_host_name(c->ssl, host);
    conn_time = tickSincePowerOn();
    
    while (tryConnect--) {
        long long conn_time1 = tickSincePowerOn();
//#ifdef TLS_TIMEOUT
//        ret = SSL_do_handshake(c->ssl);
//#else
		//av_log(h, AV_LOG_ERROR, "SSL_connect start\n");
        ret = SSL_connect(c->ssl);
//#endif
        if (ret > 0) {
			av_log(h, AV_LOG_ERROR, "[imzuop %lld] SSL_connect OK usedT(%lld)\n", tickSincePowerOn(), (tickSincePowerOn() - conn_time1) );
            break;
        }
        if (ret == 0) {
            av_log(h, AV_LOG_ERROR, "[imzuop] SSL_connect Unable to negotiate TLS/SSL session\n");
            ret = AVERROR(EIO);
            goto fail;
        }
		
		av_log(h, AV_LOG_ERROR, "[imzuop %lld] SSL_connect... retrying\n", tickSincePowerOn());
#ifdef TLS_TIMEOUT
        ret = do_tls_poll_timeout(h, ret, TIME_OUT);
#else
        ret = do_tls_poll_timeout(h, ret, 0);
#endif
        if (ret < 0) {
            av_log(h, AV_LOG_ERROR, "[imzuop %lld] tls_open timeout usedT(%lld)\n", \
                tickSincePowerOn(), (tickSincePowerOn() - conn_time1));
            goto fail;
        }
    }
#ifdef USED_CERT
    int lerr = SSL_get_verify_result(c->ssl);
    if (lerr != X509_V_OK) {
        av_log(h, AV_LOG_ERROR, "SSL certificate problem: %s\n", X509_verify_cert_error_string(lerr));
    } else {
        av_log(h, AV_LOG_ERROR, "[imzuop %lld] SSL certificate OK\n", tickSincePowerOn());
    }
#endif
#endif
    return 0;
fail:
    TLS_free(c);
    if (c->tcp)
        ffurl_close(c->tcp);
    ff_tls_deinit();
    av_log(h, AV_LOG_DEBUG, "[imzuop] tls_open ret(%d) failed usedT(%lld) conn usedT(%lld)\n", \
        ret, (tickSincePowerOn() - test_time), (tickSincePowerOn() - conn_time));
    return ret;
}

static int tls_open2(URLContext *h, const char *uri, int flags)
{
    int ret = -1;
    int retry_cnt = 2;
    long long test_time = tickSincePowerOn();

retry:
    ret = tls_open(h, uri, flags);
    if (ret != 0 && retry_cnt > 0) {
        retry_cnt --;
        goto retry;
    }

    if (ret < 0) {
        av_log(h, AV_LOG_DEBUG, "[imzuop] tls_open2 ret(%d) retry_cnt(%d) usedT(%lld)\n", \
            ret, retry_cnt, (tickSincePowerOn() - test_time));
    }

    return ret;
}

static int tls_read(URLContext *h, uint8_t *buf, int size)
{
	int err = 0;
    TLSContext *c = h->priv_data;
//	av_log(h, AV_LOG_ERROR, "tls_read++ \r\n");
    while (1) {
        ERR_clear_error();
        int ret = TLS_read(c, buf, size);
        if (ret < 0 && ret != -1) {
            int err = SSL_get_error(c, ret);
            if (err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ) {
                av_log(h, AV_LOG_ERROR, "[mplay] tls_read error ret=%d err=%d errno %d msg %s\n", \
                    ret, err, errno, strerror(errno));
            }
        }
        if (ret > 0)
        {
		//	av_log(h, AV_LOG_ERROR, "tls_read-- \r\n");
            return ret;
        }
        if (ret == 0)
            return AVERROR(EIO);
        if ((ret = do_tls_poll_timeout(h, ret,TIME_OUT*3)) < 0)
        {
        	av_log(h, AV_LOG_ERROR, "tls_read timeout \r\n");
            return ret;
        }
    }

    return 0;
}

static int tls_write(URLContext *h, const uint8_t *buf, int size)
{
    TLSContext *c = h->priv_data;
    while (1) {
        ERR_clear_error();
        int ret = TLS_write(c, buf, size);
        if (ret < 0) {
            int err = SSL_get_error(c, ret);
            if (err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ) {
                av_log(h, AV_LOG_ERROR, "[imzuop] tls_write error ret=%d err=%d errno %d msg %s\n", \
                    ret, err, errno, strerror(errno));
            }
        }
        if (ret > 0)
            return ret;
        if (ret == 0)
            return AVERROR(EIO);
        if ((ret = do_tls_poll_timeout(h, ret,TIME_OUT*3)) < 0)
            return ret;
    }

    return 0;
}

static int tls_close(URLContext *h)
{
    TLSContext *c = h->priv_data;
    TLS_shutdown(c);
    TLS_free(c);
    ffurl_close(c->tcp);
    ff_tls_deinit();
    return 0;
}

URLProtocol ff_tls_protocol = {
    .name           = "tls",
    .url_open       = tls_open2,
    .url_read       = tls_read,
    .url_write      = tls_write,
    .url_close      = tls_close,
    .priv_data_size = sizeof(TLSContext),
    .flags          = URL_PROTOCOL_FLAG_NETWORK,
};
