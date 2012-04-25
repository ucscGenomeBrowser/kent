/* Utility functions for web API programs */

void apiOut(char *text, char *jsonp);
/* Output content header and text to stdout */

void apiWarnAbortHandler(char *format, va_list args);
/* warnAbort handler that aborts with an HTTP 400 status code. */
