#ifndef FAKE_CURL
#define FAKE_CURL

#define CURL fakeCURL
#define curl_easy_init fake_curl_easy_init
#define curl_easy_setopt fake_curl_easy_setopt
#define curl_easy_perform fake_curl_easy_perform
#define curl_easy_strerror fake_curl_easy_strerror
#define curl_easy_cleanup fake_curl_easy_cleanup
#define CURLoption fakeCURLoption
#define CURLOPT_URL fakeCURLOPT_URL
#define CURLOPT_FOLLOWLOCATION fakeCURLOPT_FOLLOWLOCATION
#define CURLOPT_USERAGENT fakeCURLOPT_USERAGENT
#define CURLOPT_WRITEFUNCTION fakeCURLOPT_WRITEFUNCTION
#define CURLOPT_WRITEDATA fakeCURLOPT_WRITEDATA
#define CURLOPT_HEADERFUNCTION fakeCURLOPT_HEADERFUNCTION
#define CURLOPT_RANGE fakeCURLOPT_RANGE
#define CURLOPT_FAILONERROR fakeCURLOPT_FAILONERROR
#define CURLcode fakeCURLcode
#define CURLE_OK fakeCURLE_OK
#define CURLE_NOTOK fakeCURLE_NOTOK

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
    struct udcFile *udc; // As long as we're querying the same URL, keep the udcFile open and just seek
    long udcSize; // to simplify repeated checks against udcFileSize
} CURL;

typedef size_t (*curl_write_callback)(char *buffer, size_t size, size_t nitems, void *outstream);

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...);
CURLcode curl_easy_perform(CURL *curl);
const char *curl_easy_strerror(CURLcode errornum);
CURL *curl_easy_init(void);
void curl_easy_cleanup(CURL *curl);

#endif
