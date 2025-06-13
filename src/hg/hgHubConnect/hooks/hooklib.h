/* hooklib - Common routines for all the tusd hooks */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HOOKLIB_H
#define HOOKLIB_H
#define ERR_MSG "errorMessage"
#define REJECT_SETTING "RejectUpload"
#define STOP_SETTING "StopUpload"
#define HTTP_NAME "HTTPResponse"
#define HTTP_STATUS "StatusCode"
#define HTTP_BODY "Body"
#define HTTP_HEADER "Header"
#define HTTP_CONTENT_TYPE "Content-Type"
#define HTTP_CONTENT_TYPE_STR "application/json"


char *prettyFileSize(long size);
/* Return a string representing the size of a file */

char *encodePath(char *path);
/* Return a string where each individual component of a '/' separated
 * string has been cgiEncoded, but not the '/' chars themselves */

char *setUploadPath(char *userName, char *fileName, char *parentDir, boolean forceOverwrite);
/* return the path, relative to hg.conf tusdDataDir, where we will store this upload
 * the full path would be /tusdDataDir/userNamePrefix/userName/parentDir/fileName */

void fillOutHttpResponseError(struct jsonElement *response);

void fillOutHttpResponseSuccess(struct jsonElement *response);

struct jsonElement *makeDefaultResponse();
/* Create the default response json with some fields pre-filled */

void rejectUpload(struct jsonElement *response, char *msg, ...);
/* Set the keys for stopping an upload */

boolean isFileTypeRecognized(char *fileName);
/* Return true if this file one of our recognized types */

boolean isExistingHubForUser(char *userName, char *reqHubName);
/* Return true if hubName is an existing hub for this user */

char *genomeForHub(char *userName, char *reqHubName);
/* Return the genome associated with reqHubName for user */

#endif /* HOOKLIB_H */
