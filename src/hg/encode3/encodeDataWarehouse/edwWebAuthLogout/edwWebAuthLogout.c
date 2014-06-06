/* edwWebAuthLogout - A tiny little program to help manage Persona logouts.  Specifically
 * a little CGI that unsets the email cookie. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

int main(int argc, char *argv[])
/* Process command line. */
{
printf("Content-Type:text/plain\r\n");
printf("Set-Cookie: email=nobody; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
printf("Set-Cookie: sid=x; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
printf("\r\n");
printf("logged out\n");
return 0;
}
