/* Utility functions for web API programs */

#include "common.h"
#include "stdio.h"
#include "api.h"

void apiOut(char *text, char *jsonp)
/* Output content header and text to stdout */
{
// It's debatable whether the type should be text/plain, text/javascript or application/javascript;
// text/javascript works with all our supported browsers, so we are using that one.
puts("Content-Type:text/javascript\n");

if (jsonp)
    {
    printf("%s(%s)", jsonp, text);
    }
else
    {
    puts(text);
    }
}

void apiWarnAbortHandler(char *format, va_list args)
/* warnAbort handler that aborts with an HTTP 400 status code. */
{
puts("Status: 400\n\n");
vfprintf(stdout, format, args);
puts("\n");
exit(-1);
}

