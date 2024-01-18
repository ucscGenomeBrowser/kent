/* hooklib - Common routines for all the tusd hooks */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HOOKLIB_H
#define HOOKLIB_H
#define ERR_MSG "errorMessage"
#define REJECT_SETTING "RejectUpload"
#define STOP_SETTING "StopUpload"
#define HTTP_NAME "HTTPResponse"
#define MAX_QUOTA_BYTES 1000000000
#define MAX_QUOTA MAX_QUOTA_BYTES

void fillOutHttpResponseError();

void fillOutHttpResponseSuccess();

void rejectUpload(struct jsonElement *response, char *msg, ...);
/* Set the keys for stopping an upload */

#endif /* HOOKLIB_H */
