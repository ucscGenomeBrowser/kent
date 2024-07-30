#ifndef FAKE_CURL
#define FAKE_CURL

typedef enum {
    CURLOPT_WRITEDATA,
    CURLOPT_RANGE,
    CURLOPT_WRITEFUNCTION,
    CURLOPT_URL,
    CURLOPT_FOLLOWLOCATION,
    CURLOPT_USERAGENT,
    CURLOPT_HEADERFUNCTION,
    CURLOPT_FAILONERROR
} CURLoption;

typedef enum {
    CURLE_OK,
    CURLE_NOTOK
} CURLcode;

typedef struct {
    char *url;
    char *range;
    void *writeBuffer;
    size_t (*WriteFunction) (char *buffer, size_t size, size_t nitems, void *outstream);
    size_t (*HeaderFunction) (char *buffer, size_t size, size_t nitems, void *outstream);
    int failonerror;
    // currently ignoring follow location setting and user agent string - we always follow, no string
} CURL;

typedef size_t (*curl_write_callback)(char *buffer, size_t size, size_t nitems, void *outstream);

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...);
CURLcode curl_easy_perform(CURL *curl);
const char *curl_easy_strerror(CURLcode errornum);
CURL *curl_easy_init(void);
void curl_easy_cleanup(CURL *curl);

#endif
