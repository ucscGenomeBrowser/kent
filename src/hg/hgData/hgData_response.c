/* hgData_response.c - code relating to the success or error http response . */
#include "common.h"
#include "hgData.h"

static char const rcsid[] = "$Id: hgData_response.c,v 1.1.2.2 2009/02/03 22:13:14 mikep Exp $";

char *http_status[] = {"Bad Request", "Unauthorized", "Payment Required" , "Forbidden", "Not Found", "Method Not Allowed", "Not Acceptable", "Proxy Authentication Required", "Request Timeout", "Conflict", "Gone", "Length Required", "Precondition Failed", "Request Entity Too Large", "Request-URL Too Long", "Unsupported Media Type", "Requested Range Not Satisfiable", "Expectation Failed"};


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
if (ix >= sizeof(http_status))
    ix = 0;
code = min(max(code, 400),499);
errClientArgs(code, http_status[ix], format, args);
va_end(args);
}

void errClient(char *format, ...)
// create a HTTP response code "400 Bad Request",
// and format specifying the error message content
{
va_list args;
va_start(args, format);
errClientArgs(400, http_status[0], format, args);
va_end(args);
}
