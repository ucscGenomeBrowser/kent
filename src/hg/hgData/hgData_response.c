/* hgData_response.c - code relating to the success or error http response . */
#include "common.h"
#include "hgData.h"

static char const rcsid[] = "$Id: hgData_response.c,v 1.1.2.7 2009/02/26 08:00:19 mikep Exp $";

char *http_status1xx[] = {"Continue", "Switching Protocols"};

char *http_status2xx[] = {"OK", "Created", "Accepted" , "Non-Authoritative Information", "No Content", "Reset Content", "Partial Content"};

char *http_status3xx[] = {"Multiple Choices", "Moved Permanently", "Found" , "See Other", "Not Modified", "Use Proxy", "(Unused)", "Temporary Redirect"};

char *http_status4xx[] = {"Bad Request", "Unauthorized", "Payment Required" , "Forbidden", "Not Found", "Method Not Allowed", "Not Acceptable", "Proxy Authentication Required", "Request Timeout", "Conflict", "Gone", "Length Required", "Precondition Failed", "Request Entity Too Large", "Request-URL Too Long", "Unsupported Media Type", "Requested Range Not Satisfiable", "Expectation Failed"};

char *http_status5xx[] = {"Internal Server Error", "Not Implemented", "Bad Gateway", "Service Unavailable", "Gateway Timeout", "HTTP Version Not Supported"};

boolean notModifiedResponse(char *reqEtag, time_t reqModified, time_t modified)
// Returns TRUE if request is not modified and sends a 304 Not Modified HTTP header
// Otherwise request is modified so return FALSE
{
if ( (reqEtag && sameOk(reqEtag, etag(modified))) 
  || (reqModified && reqModified == modified ) )
    {
    send3xxHeader(304, modified, 0, NULL);
    return TRUE;
    }
return FALSE;
}

static void sendEtagHeader(int status, char *message, time_t modified, int expireSecs, char *contentType)
{
printf("Status: %d %s\n", status, message);
if (expireSecs > 0)
    {
    char *d = gmtimeToHttpStr(time(NULL)+expireSecs);
    printf("Expires: %s\n", d);
    freez(&d);
    }
if (modified > 0)
    {
    printf("ETag: %s\n", etag(modified));
    if (status!=304)
	{
	char *d = gmtimeToHttpStr(modified);
	printf("Last-Modified: %s\n", d);
	freez(&d);
	}
    }
//printf("Accept-Encoding: compress, gzip\n"); // check x-gzip etc.
printf("Content-Type: %s\n", ((contentType) ? (contentType) : "application/json"));
printf("\n");
}

void send2xxHeader(int status, time_t modified, int expireSecs, char *contentType)
// Send a 2xx header
// If modified > 0 set Last-Modified date (and ETag based on this)
// if contentType is NULL, defaults to Content-Type: application/json
{
int status200 = status - 200;
if (status200 < 0 || status200 >= sizeof(http_status2xx))
    errAbort("Invalid 2xx status %d\n", status);
sendEtagHeader(status, http_status2xx[status200], modified, expireSecs, contentType);
}

void send3xxHeader(int status, time_t modified, int expireSecs, char *contentType)
// Send a 3xx header
// If modified > 0 set Last-Modified date (and ETag based on this)
// if contentType is NULL, defaults to Content-Type: application/json
{
int status300 = status - 300;
if (status300 < 0 || status300 >= sizeof(http_status3xx))
    errAbort("Invalid 3xx status %d\n", status);
sendEtagHeader(status, http_status3xx[status300], modified, expireSecs, contentType);
}

void okSendHeader(time_t modified, int expireSecs)
// Send a 200 OK header
// If modified > 0, set Last-Modified date (and ETag) based on this
// If expireSecs > 0, set Expires header to now+expireSecs
{
send2xxHeader(200, modified, expireSecs, NULL);
}

static void errClientArgs(int code, char *status, char *format, va_list args)
// create a custom HTTP response code, status, and json message 
// Note that you will get an additional "500 Server error" if you 
// have apache configured for error documents on the same code you set here
// To avoid this you can use an unreserved code like 420
{
char msg[2048];
struct json_object *err = json_object_new_object();
fprintf(stdout, "Status: %u %s\n", code, status);
fprintf(stdout, "Content-type: application/json\n\n");
if (format != NULL) {
    vsnprintf(msg, sizeof(msg), format, args);
    }
json_object_object_add(err, "error", json_object_new_string(msg));
fprintf(stdout, json_object_to_json_string(err));
fprintf(stdout, "\n");
fflush(stdout);
exit(0);
}

void errClientStatus(int code, char *status, char *format, ...)
// create a HTTP response code 400-499 and status, 
// with format specifying the error message content
{
va_list args;
va_start(args, format);
code = min(max(code, 400),499);
errClientArgs(code, status, format, args);
va_end(args);
}

void errClientCode(int code, char *format, ...)
// create a HTTP response code 400-499 with standard status,
// and format specifying the error message content
{
va_list args;
va_start(args, format);
int ix = max(code-400, 0);
if (ix >= sizeof(http_status4xx))
    ix = 0;
code = min(max(code, 400),499);
errClientArgs(code, http_status4xx[ix], format, args);
va_end(args);
}

void errClient(char *format, ...)
// create a HTTP response code "400 Bad Request",
// and format specifying the error message content
{
va_list args;
va_start(args, format);
errClientArgs(400, http_status4xx[0], format, args);
va_end(args);
}
