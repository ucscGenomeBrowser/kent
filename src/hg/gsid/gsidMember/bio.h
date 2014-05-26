/* bio.h - routines to fetch https url using openssl */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BIO_H
#define BIO_H

#include "dystring.h"

struct dyString *bio(char *site, char *url, char *certFile, char *certPath);
/*
This SSL/TLS client example, attempts to retrieve a page from an SSL/TLS web server. 
The I/O routines are identical to those of the unencrypted example in BIO_s_connect(3).
*/

#endif
