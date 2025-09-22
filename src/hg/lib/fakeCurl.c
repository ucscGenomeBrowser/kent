/* FakeCurl - replacements for a handful of functions in the Curl library.  Sometimes we want to use
 * external code blocks or libraries that rely on the curl library, but we want to replace those
 * calls with our * own UDC local-caching system without needing to significantly modify those
 * calls.  This provides a drop-in replacement that effectively implements (a small subset of) the
 * curl library functions using UDC.
 * Note that user-agent and redirect-following settings are ignored here - UDC doesn't currently
 * support user-agent strings in requests and always follows redirects.
 */

/* Copyright (C) 2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "udc.h"
#include "fakeCurl.h"

CURL *curl_easy_init(void)
/* Create a new fakeCurl object.  Dispose of this with curl_easy_cleanup().
 */
{
    CURL *new = (CURL*) malloc (sizeof(CURL));
    new->url = NULL;
    new->range = NULL;
    new->writeBuffer = NULL;
    new->WriteFunction = NULL;
    new->HeaderFunction = NULL;
    new->failonerror = 0;
    new->udc = NULL;
    new->udcSize = 0;
    return new;
}

void curl_easy_cleanup(CURL *curl)
/* Free a curl object allocated with curl_easy_init().  Do not attempt
 * to use the pointer's value after calling this function on it.
 */
{
    if (curl != NULL)
    {
        if (curl->udc)
            udcFileClose(&(curl->udc));
        // clear the local copies of URL strings
        if (curl->url)
            freeMem(curl->url);
        if (curl->range)
            freeMem(curl->range);
        free(curl);
    }
}


CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...)
/* Configure a variety of options on a CURL object.  This supports a very small subset
 * of the options provided by the real curl library - the supported list right now is just:
 * CURLOPT_WRITEDATA, CURLOPT_RANGE, CURLOPT_WRITEFUNCTION, CURLOPT_HEADERFUNCTION, and
 * CURLOPT_URL.
 *
 * The header function will be invoked, but only on a faked subset of what the actual
 * header content would be - just a content range string.
 *
 * The CURLOPT_FOLLOWLOCATION and CURLOPT_USERAGENT settings are also accepted, but
 * the values are ignored (the udc implementation here always follows redirects and lacks
 * support for user-agent strings).
 */
{
    va_list args;
    va_start(args, option);
    char *newUrl = NULL;
    switch (option) {
        case CURLOPT_WRITEDATA:
            curl->writeBuffer = va_arg(args, void *);
            break;
        case CURLOPT_RANGE:
            if (curl->range)
                freeMem(curl->range);
            curl->range = cloneString(va_arg(args,char *));
            break;
        case CURLOPT_WRITEFUNCTION:
            curl->WriteFunction = va_arg(args, curl_write_callback);
            break;
        case CURLOPT_URL:
            newUrl = va_arg(args, char*);
            if (curl->url && sameString(curl->url, newUrl))
                break;
            if (curl->udc)
                udcFileClose(&(curl->udc));
            if (curl->url)
                freeMem(curl->url);
            curl->url = cloneString(newUrl);
            break;
        case CURLOPT_FOLLOWLOCATION:
            // ignored
            break;
        case CURLOPT_USERAGENT:
            // ignored
            break;
        case CURLOPT_HEADERFUNCTION:
            curl->HeaderFunction = va_arg(args, curl_write_callback);
            break;
        case CURLOPT_FAILONERROR:
            curl->failonerror = 1;
            break;
        default:
            errAbort("Unexpected curl option supplied to fakeCurl"); 
    }
    va_end(args); 
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *curl)
/* Perform a fake curl operation via UDC, using the settings establised in the CURL object
 * via calls to curl_easy_setopt().  The return value will be either CURLE_OK (for success)
 * or CURLE_NOTOK (for failure).
 *
 * As noted in curl_easy_setopt(), the content provided to any supplied header function is
 * a faked subset of actual header content - just a "Content-Range" string.
 */
{
    // Open the file using UDC, returning an error if that fails
    if (curl->udc == NULL)
        {
        curl->udc = udcFileMayOpen(curl->url, NULL);
        curl->udcSize = (long) udcFileSize(curl->url);
        }
    if (curl->udc == NULL)
        return CURLE_NOTOK;

    // Set up the seek offset if there's a range supplied
    long start = 0;
    long end = curl->udcSize;
    if (curl->range != NULL)
    {
        start = atol(curl->range);
        char *end_pos = strrchr(curl->range, '-');
        if (end_pos != NULL && *(end_pos+1) != 0)
            end = atol(end_pos+1);
    }

    if (end >= curl->udcSize)
        end = curl->udcSize-1;

    char headerbuf[4096];

    if (start < 0 || start >= curl->udcSize)
    {
        // We need to gin up a 416 error response here and abort if FAILONERROR is set.
        // Otherwise our next step is udcSeek, which will just attempt an lseek
        // and then errAbort when the requested range isn't within the file bounds.
        safef(headerbuf, sizeof(headerbuf),
                "HTTP/1.1 416 Requested Range Not Satisfiable\nContent-Range: bytes */%ld", curl->udcSize);
        if (curl->HeaderFunction != NULL)
        {
            char buf[4096];
            curl->HeaderFunction(buf, strlen(buf), 1, NULL);
        }
        if (curl->failonerror)
            return CURLE_NOTOK;
        return CURLE_OK;
    }

    // Fake up a Content-Range string in a header in case there's a header function to call.
    safef(headerbuf, sizeof(headerbuf), "Content-Range: bytes %ld-%ld/%ld", start, end, curl->udcSize);
    if (curl->HeaderFunction != NULL)
    {
        char buf[4096];
        curl->HeaderFunction(buf, strlen(buf), 1, NULL);
    }

    char *readBuffer = (char*) malloc(end-start);
    udcSeek(curl->udc, start);
    long bytesRead = udcRead(curl->udc, readBuffer, end-start);
    // Technically we should pay attention to the value returned by udcRead, as it might indicate
    // that fewer bytes than requested were actually read.  Worry about that a bit later.

    // If writefunction is defined, then call that on the buffer of data we got from udc.
    // Otherwise, put it into the supplied writebuffer (which must exist).
    if (curl->WriteFunction != NULL)
    {
        curl->WriteFunction(readBuffer, bytesRead, 1, curl->writeBuffer);
    }
    else
    {
        if (curl->writeBuffer == NULL)
            errAbort("Attempting to fakeCurl fetch without specifying a write buffer first");
        fwrite(readBuffer, bytesRead, 1, curl->writeBuffer);
    }
    free(readBuffer);

    return CURLE_OK;
}


const char *curl_easy_strerror(CURLcode errornum)
/* This converts a CURLcode error code into an associated error message string.  As this
 * is a very barebones implementation, the only options are "Ok" for CURLE_OK and
 * "fakeCurl failed" for CURLE_NOTOK.
 */
{
    switch (errornum) {
        case CURLE_OK:
            return "Ok";
            break;
        default:
            return "fakeCurl failed";
    }
}
