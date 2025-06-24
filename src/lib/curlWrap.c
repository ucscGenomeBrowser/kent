/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* thin wrapper around curl, to make it easier to request strings over HTTP */

#include <curl/curl.h>
#include <common.h>

struct curlString {
    char *ptr;
    size_t len;
};
void init_string(struct curlString *s) {
    s->len = 0;
    s->ptr = malloc(1);
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, void *userData) {
    struct curlString *s = (struct curlString *)userData;
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

char* curlPostUrl(char *url, char *data)
/* post data to URL and return as string. Must be freed. */
{
CURL *curl = curl_easy_init();
if (!curl) 
    errAbort("Cannot init curl library");

struct curlString response;
init_string(&response);

curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
curl_easy_perform(curl);
curl_easy_cleanup(curl);

char *resp = cloneString(response.ptr);
free(response.ptr);
return resp;
}


