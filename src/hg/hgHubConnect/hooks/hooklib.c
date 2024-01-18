/* hooklib.c - functions common to all the different tusd hooks */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "wikiLink.h"
#include "customTrack.h"
#include "userdata.h"
#include "jsonQuery.h"
#include "jsHelper.h"
#include "errCatch.h"
#include "obscure.h"
#include "hooklib.h"

void fillOutHttpResponseError()
{
fprintf(stderr, "http responde error!\n");
}

void fillOutHttpResponseSuccess()
{
fprintf(stderr, "http response success!\n");
}

void rejectUpload(struct jsonElement *response, char *msg, ...)
/* Set the keys for stopping an upload */
{
// first set the necessary keys to reject the request
jsonObjectAdd(response, REJECT_SETTING, newJsonBoolean(TRUE));
jsonObjectAdd(response, STOP_SETTING, newJsonBoolean(TRUE));

// now format the message
va_list args;
va_start(args, msg);
struct dyString *ds = dyStringNew(0);
dyStringVaPrintf(ds, msg, args);
va_end(args);
jsonObjectAdd(response, ERR_MSG, newJsonString(dyStringCannibalize(&ds)));
}

