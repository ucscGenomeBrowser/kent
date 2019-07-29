/* edwScriptSubmitStatus - Programatically check status of submission.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dystring.h"
#include "errAbort.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwScriptSubmitStatus - Programatically check status of submission.\n"
  "This program is meant to be called as a CGI from a web server using HTTPS.");
}

void addErrFile(struct dyString *dy, int errCount, char *fileName, char *err)
/* Add file and error message to JSON in dy */
{
if (errCount > 0)
   dyStringPrintf(dy, ",\n");
dyStringPrintf(dy, "        ");
dyStringPrintf(dy, "{\"file\": \"%s\", \"error\": \"%s\"}", fileName, err);
}

void edwScriptSubmitStatus()
/* edwScriptSubmitStatus - Programatically check status of submission.. */
{
/* Pause a second - prevent inadvertent harsh denial of service from scripts. */
sleep(2);

edwScriptRegistryFromCgi();

/* Get submission from url. */
struct sqlConnection *conn = edwConnect();
char query[512];
char *url = cgiString("url");
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
char *status = NULL;
if (sub == NULL)
    {
    int posInQueue = edwSubmitPositionInQueue(conn, url, NULL);
    if (posInQueue == -1)
         errAbort("%s has not been submitted", url);
    else
         status = "pending";
    }
else
    {
    time_t endUploadTime = sub->endUploadTime;
    if (!isEmpty(sub->errorMessage))
        {
	status = "error";
	}
    else if (endUploadTime == 0)  
	{
	status = "uploading";
	}
    else
        {
	sqlSafef(query, sizeof(query), 
	    "select count(*) from edwFile where submitId=%u and errorMessage != ''",
	    sub->id);
	int errCount = sqlQuickNum(conn, query);
	int newValid = edwSubmitCountNewValid(sub, conn);
	if (newValid + errCount < sub->newFiles)
	    status = "validating";
	else if (errCount > 0)
	    status = "error";
	else
	    status = "success";
	}
    }

/* Construct JSON result */
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "{\n");
dyStringPrintf(dy, "    \"status\": \"%s\"", status);
if (sameString(status, "error"))
    {
    dyStringPrintf(dy, ",\n");
    dyStringPrintf(dy, "    \"errors\": [\n");
    int errCount = 0;
    if (!isEmpty(sub->errorMessage))
        {
	addErrFile(dy, errCount, sub->url, sub->errorMessage);
	++errCount;
	}
    sqlSafef(query, sizeof(query), "select * from edwFile where submitId=%u and errorMessage != ''",
	sub->id);
    struct edwFile *file, *fileList = edwFileLoadByQuery(conn, query);
    for (file = fileList; file != NULL; file = file->next)
        {
	addErrFile(dy, errCount, file->submitFileName, file->errorMessage);
	++errCount;
	}
    dyStringPrintf(dy, "\n    ]\n");
    dyStringPrintf(dy, "}\n");
    }
else
    {
    dyStringPrintf(dy, "\n}\n");
    }

/* Write out HTTP response */
printf("Content-Length: %ld\r\n", dy->stringSize);
puts("Content-Type: application/json; charset=UTF-8\r");
puts("\r");
printf("%s", dy->string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
   usage();
cgiSpoof(&argc, argv);
pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);
edwScriptSubmitStatus();
return 0;
}
