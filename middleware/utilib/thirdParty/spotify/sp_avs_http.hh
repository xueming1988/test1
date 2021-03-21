#ifndef __SP_AVS_HTTP_H__
#define __SP_AVS_HTTP_H__

#include <thread>
#include <curl/curl.h>

class CURL_CLIENT
{
  private:
    int is_running;
    int count;
    CURLM *multi_handle;
    struct curl_slist *easy_header = nullptr;
    std::string post_body;
    std::thread t;

    void loop();
    void add_easy_handle();
    void disp(std::string &&);

  public:
    CURL_CLIENT() = delete;
    CURL_CLIENT(const char *, const char *, const char *);
    ~CURL_CLIENT();

    int start_request();
};

#endif