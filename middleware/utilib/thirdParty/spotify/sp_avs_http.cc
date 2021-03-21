#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "spotify_embedded.h"
#include "sp_avs_http.hh"

pthread_cond_t *curl_cond = nullptr;
pthread_mutex_t *curl_mutex = nullptr;

std::unique_ptr<CURL_CLIENT> curl_client = nullptr;

template <typename T, typename... Ts> std::unique_ptr<T> make_unique(Ts &&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

CURL_CLIENT::CURL_CLIENT(const char *access_token, const char *action, const char *device_id)
    : is_running(1), count(5)
{
    multi_handle = curl_multi_init();

    std::string header;
    header.append("Authorization: Bearer ").append(access_token);
    easy_header = curl_slist_append(easy_header, "Content-Type: application/json");
    easy_header = curl_slist_append(easy_header, header.c_str());

    post_body.append("{\"action\":\"")
        .append(action)
        .append("\"")
        .append(",\"device_id\":\"")
        .append(device_id)
        .append("\"}");

    disp("client created");
    std::cout << "access_token: " << access_token << std::endl;
    std::cout << "post_body: " << post_body << std::endl;
}

CURL_CLIENT::~CURL_CLIENT()
{
    if (easy_header) {
        curl_slist_free_all(easy_header);
    }

    if (multi_handle) {
        curl_multi_cleanup(multi_handle);
    }

    disp("client destroyed");
}

void CURL_CLIENT::loop()
{
    disp("enter loop");

    curl_mutex = new pthread_mutex_t;
    curl_cond = new pthread_cond_t;
    pthread_mutex_init(curl_mutex, nullptr);
    pthread_cond_init(curl_cond, nullptr);

    add_easy_handle();

    while (is_running) {
        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        int maxfd = -1;
        int rc;

        long curl_timeo;
        struct timeval timeout;

        curl_multi_timeout(multi_handle, &curl_timeo);
        if (curl_timeo < 0)
            curl_timeo = 1000;

        timeout.tv_sec = curl_timeo / 1000;
        timeout.tv_usec = (curl_timeo % 1000) * 1000;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        /* get file descriptors from the transfers */
        curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

        if (maxfd == -1) {
            usleep(100000);
            rc = 0;
        } else {
            rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }

        switch (rc) {
        case -1:
            /* select error */
            break;
        case 0:
        default:
            /* timeout or readable/writable sockets */
            int still_running;
            curl_multi_perform(multi_handle, &still_running);

            CURLMsg *message;
            do {
                int msgq;
                message = curl_multi_info_read(multi_handle, &msgq);
                if (message && (message->msg == CURLMSG_DONE)) {
                    CURL *easy_handle = message->easy_handle;
                    long http_ret;
                    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &http_ret);
                    std::cout << std::endl << "http_code = " << http_ret << std::endl;
                    curl_multi_remove_handle(multi_handle, easy_handle);
                    curl_easy_cleanup(easy_handle);
                    if (http_ret != 200) {
                        if (count) {
                            --count;
                        } else {
                            is_running = 0;
                        }
                        sleep(1);
                        add_easy_handle();
                    } else {
                        disp("wait result");
                        pthread_mutex_lock(curl_mutex);
                        struct timeval time_now;
                        struct timespec time_out;
                        gettimeofday(&time_now, nullptr);
                        time_out.tv_sec = time_now.tv_sec + 5;
                        time_out.tv_nsec = time_now.tv_usec * 1000;
                        int err = pthread_cond_timedwait(curl_cond, curl_mutex, &time_out);
                        pthread_mutex_unlock(curl_mutex);
                        std::cout << "result = " << err << std::endl;

                        if (err == ETIMEDOUT) {
                            is_running = 0;
                        } else if (err == 0) {
                            disp("need retry");
                            sleep(1);
                            add_easy_handle();
                        }
                    }
                }
            } while (message);
            break;
        }
    }

    pthread_mutex_destroy(curl_mutex);
    pthread_cond_destroy(curl_cond);
    delete curl_mutex;
    delete curl_cond;
    curl_mutex = nullptr;
    curl_cond = nullptr;

    disp("quit loop");

    curl_client = nullptr;
}

void CURL_CLIENT::add_easy_handle()
{
    CURL *easy_handle = nullptr;

    easy_handle = curl_easy_init();
    if (easy_handle) {
        curl_easy_setopt(easy_handle, CURLOPT_URL,
                         "https://api-partner.spotify.com/v1/natural-language/action");
        curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, easy_header);
        curl_easy_setopt(easy_handle, CURLOPT_POST, 1L);
        curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDS, post_body.c_str());
        curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(post_body.length()));
        curl_easy_setopt(easy_handle, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
        // curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, 1);
    }

    curl_multi_add_handle(multi_handle, easy_handle);
}

int CURL_CLIENT::start_request()
{
    t = std::thread([this] { this->loop(); });
    t.detach();

    return 0;
}

inline void CURL_CLIENT::disp(std::string &&msg)
{
    std::cout << "[CURL_CLIENT] " << msg << std::endl;
    std::cout.flush();
}

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int start_request(const char *, const char *, const char *);
#ifdef __cplusplus
}
#endif /* __cplusplus */
int start_request(const char *token, const char *action, const char *device_id)
{
    if (token && action && device_id) {
        curl_client = make_unique<CURL_CLIENT>(token, action, device_id);
        curl_client->start_request();
    }

    return 0;
}
