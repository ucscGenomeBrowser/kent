/* Utility functions for web API programs */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

void apiOut(char *text, char *jsonp);
/* Output content header and text to stdout */

void apiWarnAbortHandler(char *format, va_list args);
/* warnAbort handler that aborts with an HTTP 400 status code. */
