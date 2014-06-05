/* edwWebSubmit - A small self-contained CGI for submitting data to the ENCODE Data Warehouse.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errAbort.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "portable.h"
#include "net.h"
#include "paraFetch.h"

char *userEmail;	/* Filled in by authentication system. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebSubmit - A small self-contained CGI for submitting data to the ENCODE Data Warehouse.\n"
  "usage:\n"
  "   edwWebSubmit cgiVar=value cgiVar2=value2\n"
  );
}

void logIn()
/* Put up name.  No password for now. */
{
printf("<DIV>");
printf("<H3>Welcome to the ENCODE data submission site</H3>");
printf("Please sign in using Persona&nbsp;");
printf("<INPUT TYPE=BUTTON NAME=\"signIn\" CLASS=\"btn\" VALUE=\"Sign in\" id=\"signin\"</H5>");
printf("</DIV>");
}

void getUrl(struct sqlConnection *conn)
/* Put up URL. */
{
printf("<H3>Submit data</H3>");

printf("<P CLASS='title'>Enter the URL of a validated manifest file:<P>");

puts("<DIV CLASS=\"input-append\">");
cgiMakeTextVar("url", emptyForNull(cgiOptionalString("url")), 80);
cgiMakeButton("submitUrl", "Submit");
puts("</DIV>");

puts("<DIV>");
cgiMakeCheckBox("update", FALSE);
printf(" Update information for files previously submitted");
puts("</DIV>");
}

static char *stopButtonName = "stopUpload";

long long paraFetchedSoFar(char *path)
/* Return amount fetched so far. */
{
struct parallelConn *pcList = NULL;
char *url = NULL;
off_t fileSize = 0;
char *dateString = NULL;
off_t totalDownloaded = 0;
paraFetchReadStatus(path, &pcList, &url, &fileSize, &dateString, &totalDownloaded);
return totalDownloaded;
}

void monitorSubmission(struct sqlConnection *conn, boolean autoRefresh)
/* Write out information about submission. */
{
char *url = trimSpaces(cgiString("url"));
cgiMakeHiddenVar("url", url);
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
time_t startTime = 0, endTime = 0, endUploadTime = 0;
if (sub == NULL)
    {
    int posInQueue = edwSubmitPositionInQueue(conn, url, NULL);
    if (posInQueue == 0)
	printf("%s is first in the submission queue, but upload has not started<BR>\n", url);
    else if (posInQueue > 0)
        printf("%s is in submission queue with %d submissions ahead of it<BR>\n", url, posInQueue);
    else
	{
        printf("%s status unknown.", url);
	}
    }
else
    {
    startTime = sub->startUploadTime;
    endUploadTime = sub->endUploadTime;
    endTime = (endUploadTime ? endUploadTime : edwNow());
    int timeSpan = endTime - startTime;
    long long thisUploadSize = sub->byteCount - sub->oldBytes;
    long long curSize = 0;  // Amount of current file we know we've transferred.

    /* Print title letting them know if upload is done or in progress. */
    struct edwUser *user = edwUserFromId(conn, sub->userId);
    printf("<B>Submission by %s is ", user->email);
    if (!isEmpty(sub->errorMessage))
	{
	if (endUploadTime == 0)
	    printf("having problems...");
	else
	    printf("stopped by uploader request.");
	}
    else if (endUploadTime != 0)  
	{
        printf("uploaded.");
	}
    else
	printf("in progress...");
    printf("</B><BR>\n");

    /* Print URL and how far along we are at the file level. */
    if (!isEmpty(sub->errorMessage))
	{
	printf("<B>error:</B> %s<BR>\n", sub->errorMessage);
	cgiMakeButton("getUrl", "try submission again");
	printf("<BR>");
	}
    printf("<B>url:</B> %s<BR>\n", sub->url);
    printf("<B>files count:</B> %d<BR>\n", sub->fileCount);
    if (sub->oldFiles > 0)
	printf("<B>files already in warehouse:</B> %u<BR>\n", sub->oldFiles);
    if (sub->metaChangeCount > 0)
        printf("<B>old files with new tags in this submission</B> %d<BR>", sub->metaChangeCount);
    if (sub->oldFiles != sub->fileCount)
	{
	printf("<B>files transferred:</B> %u<BR>\n", sub->newFiles);
	printf("<B>files remaining:</B> %u<BR>\n", sub->fileCount - sub->oldFiles - sub->newFiles);
	}

    /* Report validation status */
    printf("<B>new files validated:</B> %u of %u<BR>\n", edwSubmitCountNewValid(sub, conn), 
	sub->newFiles);

    /* Print error message, and in case of error skip file-in-transfer info. */
    if (isEmpty(sub->errorMessage))
	{
	/* If possible print information about file en route */
	if (endUploadTime == 0)
	    {
	    struct edwFile *ef = edwFileInProgress(conn, sub->id);
	    if (ef != NULL)
		{
		char path[PATH_LEN];
		safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);
		if (ef->endUploadTime > 0)
		    curSize = ef->size;
		else
		    curSize = paraFetchedSoFar(path);
		printf("<B>file in route:</B> %s",  ef->submitFileName);
		printf(" (%d%% transferred)<BR>\n", (int)(100.0 * curSize / ef->size));
		}
	    }
	}
    /* Report bytes transferred */
    long long transferredThisTime = curSize + sub->newBytes;
    printf("<B>total bytes transferred:</B> ");
    long long totalTransferred = transferredThisTime + sub->oldBytes;
    printLongWithCommas(stdout, totalTransferred);
    printf(" of ");
    printLongWithCommas(stdout, sub->byteCount);
    if (sub->byteCount != 0)
	printf(" (%d%%)<BR>\n", (int)(100.0 * totalTransferred / sub->byteCount));
    else
        printf("<BR>\n");

    /* Report transfer speed if possible */
    if (isEmpty(sub->errorMessage))
	{
	if (timeSpan > 0)
	    {
	    printf("<B>transfer speed:</B> ");
	    printLongWithCommas(stdout, (curSize + sub->newBytes)/timeSpan);
	    printf(" bytes/sec<BR>\n");
	    }

	/* Report start time  and duration */
	printf("<B>submission started:</B> %s<BR>\n", ctime(&startTime));
	struct dyString *duration = edwFormatDuration(timeSpan);

	/* Try and give them an ETA if we aren't finished */
	if (endUploadTime == 0 && timeSpan > 0)
	    {
	    printf("<B>time so far:</B> %s<BR>\n", duration->string);
	    double bytesPerSecond = (double)transferredThisTime/timeSpan;
	    long long bytesRemaining = thisUploadSize - curSize - sub->newBytes;
	    if (bytesPerSecond > 0)
		{
		long long estimatedFinish = bytesRemaining/bytesPerSecond;
		struct dyString *eta = edwFormatDuration(estimatedFinish);
		printf("<B>estimated finish in:</B> %s<BR>\n", eta->string);
		}
	    }
	else
	    {
	    printf("<B>submission time:</B> %s<BR>\n", duration->string);
	    }
	}
    }
if (endUploadTime == 0 && isEmpty(sub->errorMessage))
    cgiMakeButton(stopButtonName, "stop upload");

if (autoRefresh && isEmpty(sub->errorMessage))
    {
    cgiMakeHiddenVar("monitor", "monitor");
    edwWebAutoRefresh(EDW_WEB_REFRESH_5_SEC);
    }
}

boolean okToResubmitThisUrl(struct sqlConnection *conn, char *url)
/* Return true if the user is an administrator or the original submitter */
{
if (edwUserIsAdmin(conn,userEmail)) 
    return TRUE; 
if (!sqlRowExists(conn, "edwSubmit", "url", url))
    return TRUE;
/* url exists, find out who is the original submitter */
int uId = edwFindUserIdFromEmail(conn, userEmail);
char  query[256];
sqlSafef(query, sizeof(query),
    "select distinct userId from edwSubmit where url='%s'", url);
struct slInt *idList = sqlQuickNumList(conn, query);
struct slInt *id;
for (id = idList; id != NULL; id = id->next)
    {
    if (id->val == uId)
    return TRUE;
    }
return FALSE;
}

void submitUrl(struct sqlConnection *conn)
/* Submit validated manifest if it is not already in process. */
{
/* Parse email and URL out of CGI vars. Do a tiny bit of error checking. */
char *url = trimSpaces(cgiString("url"));
if (!stringIn("://", url))
    errAbort("%s doesn't seem to be a valid URL, no '://'", url);

/* Do some reality checks that email and URL actually exist. */
edwMustGetUserFromEmail(conn, userEmail);
int sd = netUrlMustOpenPastHeader(url);
close(sd);

/* Check is it OK for the user to resubmit the url or update its information */
if (!okToResubmitThisUrl(conn, url))
    errAbort("The url was previously submitted by another user, you cannot submit it again nor update its information.");

edwAddSubmitJob(conn, userEmail, url, cgiBoolean("update"));
}

void stopUpload(struct sqlConnection *conn)
/* Try and stop current upload. */
{
char *url = trimSpaces(cgiString("url"));
cgiMakeHiddenVar("url", url);
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
if (sub == NULL)
    {
    /* Submission hasn't happened yet - remove it from job queue. */
    unsigned edwSubmitJobId = 0;
    int posInQueue = edwSubmitPositionInQueue(conn, url, &edwSubmitJobId);
    if (posInQueue >= 0)
        {
	char query[256];
	sqlSafef(query, sizeof(query), "delete from edwSubmitJob where id=%u", edwSubmitJobId);
	sqlUpdate(conn, query);
	printf("Removed submission from %s from job queue\n", url);
	}
    }
else
    {
    char query[256];
    sqlSafef(query, sizeof(query), 
	"update edwSubmit set errorMessage='Stopped by user.' where id=%u", sub->id);
    sqlUpdate(conn, query);
    }
}


static void localWarn(char *format, va_list args)
/* A little warning handler to override the one with the button that goes nowhere. */
{
printf("<B>Error:</B> ");
vfprintf(stdout, format, args);
printf("<BR>Please use the back button on your web browser, correct the error, and resubmit.");
}

void doMiddle()
/* doMiddle - put up middle part of web page, not including http and html headers/footers */
{
pushWarnHandler(localWarn);
edwWebSubmitMenuItem(TRUE);
printf("<FORM ACTION=\"../cgi-bin/edwWebSubmit\" METHOD=GET>\n");
struct sqlConnection *conn = edwConnectReadWrite(edwDatabase);
userEmail = edwGetEmailAndVerify();
if (userEmail == NULL)
    {
    edwWebSubmitMenuItem(FALSE);
    edwWebBrowseMenuItem(FALSE);
    logIn();
    }
else if (cgiVarExists(stopButtonName))
    {
    stopUpload(conn);
    monitorSubmission(conn, FALSE);
    }
else if (cgiVarExists("submitUrl"))
    {
    submitUrl(conn);
    sleep1000(1000);
    monitorSubmission(conn, TRUE);
    }
else if (cgiVarExists("monitor"))
    {
    monitorSubmission(conn, TRUE);
    }
else
    {
    getUrl(conn);
    edwWebSubmitMenuItem(FALSE);
    }
printf("</FORM>");
}


int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();

/* Put out HTTP header and HTML HEADER all the way through <BODY> */
edwWebHeaderWithPersona("");

/* Call error handling wrapper that catches us so we write /BODY and /HTML to close up page
 * even through an errAbort. */
htmEmptyShell(doMiddle, NULL);

edwWebFooterWithPersona();
return 0;
}
