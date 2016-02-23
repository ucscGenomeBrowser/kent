/* Utility functions for web API programs */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
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
