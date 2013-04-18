/* edwWebSubmit - A small self-contained CGI for submitting data to the ENCODE Data Warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "portable.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebSubmit - A small self-contained CGI for submitting data to the ENCODE Data Warehouse.\n"
  "usage:\n"
  "   edwWebSubmit cgiVar=value cgiVar2=value2\n"
  );
}

void logIn(struct sqlConnection *conn)
/* Put up name.  No password for now. */
{
printf("Welcome to prototype ENCODE Data Warehouse submission site.<BR>");
printf("Please enter your email address ");
cgiMakeTextVar("email", "", 32);
cgiMakeSubmitButton();
}

void getUrl(struct sqlConnection *conn)
/* Put up URL. */
{
char *email = trimSpaces(cgiString("email"));
struct edwUser *user = edwMustGetUserFromEmail(conn, email);
cgiMakeHiddenVar("email", user->email);
printf("URL ");
cgiMakeTextVar("url", "", 80);
cgiMakeSubmitButton();
}

void submitUrl(struct sqlConnection *conn)
/* Submit validated manifest if it is not already in process.  Show
 * progress once it is in progress. */
{
/* Parse email and URL out of CGI vars. Do a tiny bit of error checking. */
char *email = trimSpaces(cgiString("email"));
char *url = trimSpaces(cgiString("url"));
if (!stringIn("://", url))
    errAbort("%s doesn't seem to be a valid URL, no '://'", url);
if (!stringIn("@", email))
    errAbort("%s doesn't seem to be a valid email, no '@'", email);

/* Do some reality checks that email and URL actually exist. */
edwMustGetUserFromEmail(conn, email);
int sd = netUrlMustOpenPastHeader(url);
close(sd);

/* Write email and URL to fifo used by submission spooler. */
FILE *fifo = mustOpen("edwSubmit.fifo", "w");
fprintf(fifo, "%s %s\n", email, url);
carefulClose(&fifo);

/* Give system a second to react, and then try to put up status info about submission. */
printf("Submission of %s progress....", url);
cgiMakeButton("monitor", "monitor submission");
cgiMakeHiddenVar("email", email);
cgiMakeHiddenVar("url", url);
}

struct dyString *formatDuration(long long seconds)
/* Convert seconds to days/hours/minutes. */
{
struct dyString *dy = dyStringNew(0);
int days = seconds/(3600*24);
if (days > 0)
    dyStringPrintf(dy, "%d days, ", days);
seconds -= days*3600*24;

int hours = seconds/3600;
if (hours > 0)
    dyStringPrintf(dy, "%d hours, ", hours);
seconds -= hours*3600;

int minutes = seconds/60;
if (minutes > 0)
    dyStringPrintf(dy, "%d minutes, ", minutes);

seconds -= minutes*60;
dyStringPrintf(dy, "%d seconds", (int)seconds);
return dy;
}

struct edwFile *edwFileInProgress(struct sqlConnection *conn, int submitId)
/* Return file in submission in process of being uploaded if any. */
{
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where submitId=%u and endUploadTime=0 limit 1", submitId);
return edwFileLoadByQuery(conn, query);
}

void monitorSubmission(struct sqlConnection *conn)
/* Write out information about submission. */
{
char *email = trimSpaces(cgiString("email"));
char *url = trimSpaces(cgiString("url"));
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
if (sub == NULL)
    {
    printf("%s is in submission queue, but upload has not started", url);
    }
else
    {
    printf("<B>url:</B> %s<BR>\n", sub->url);
    if (!isEmpty(sub->errorMessage))
        printf("<B>error:</B> %s<BR>\n", sub->errorMessage);
    else
	{
	printf("<B>files finished:</B> %u of %u<BR>\n", 
	    sub->newFiles + sub->oldFiles, sub->fileCount);
	time_t startTime = sub->startUploadTime;
	time_t endTime = sub->endUploadTime;
	if (endTime == 0)
	    {
	    long long timeSoFar = edwNow() - startTime;
	    struct edwFile *ef = edwFileInProgress(conn, sub->id);
	    long long curSize = 0;
	    if (ef != NULL)
		{
		printf("<B>file in route:</B> %s<BR>\n",  ef->submitFileName);
		printf("<B>file bytes transferred:</B> ");
		char path[PATH_LEN];
		safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);
		curSize = fileSize(path);
		printLongWithCommas(stdout, curSize);
		printf(" of ");
		printLongWithCommas(stdout, ef->size);
		printf(" (%d%%)<BR>\n", (int)(100.0 * curSize / ef->size));
		printf("<B>total bytes transferred:</B> ");
		long long totalTransferred = curSize + sub->oldBytes + sub->newBytes;
		printLongWithCommas(stdout, totalTransferred);
		printf(" of ");
		printLongWithCommas(stdout, sub->byteCount);
		printf(" (%d%%)<BR>\n", (int)(100.0 * totalTransferred / sub->byteCount));
		long long thisUploadSize = sub->byteCount - sub->oldBytes;
		if (timeSoFar > 0)
		    {
		    long long transferredThisTime = curSize + sub->newBytes;
		    double bytesPerSecond = transferredThisTime/timeSoFar;
		    printf("<B>transfer speed:</B> ");
		    printLongWithCommas(stdout, bytesPerSecond);
		    printf(" bytes/sec<BR>\n");
		    struct dyString *duration = formatDuration(timeSoFar);
		    printf("<B>submission started:</B> %s<BR>\n", ctime(&startTime));
		    printf("<B>submit time so far:</B> %s<BR>\n", duration->string);
		    long long bytesRemaining = thisUploadSize - curSize - sub->newBytes;
		    if (bytesPerSecond > 0)
			{
			long long estimatedFinish = bytesRemaining/bytesPerSecond;
			struct dyString *eta = formatDuration(estimatedFinish);
			printf("<B>estimated finish in:</B> %s<BR>\n", eta->string);
			}
		    }
		}
	    else
		 printf("<B>checking MD5...</B><BR>\n");
	    }
	else
	    {
	    struct dyString *duration = formatDuration(endTime - startTime);
	    printf("<B>submission started:</B> %s<BR>\n", ctime(&startTime));
	    printf("<B>finished submission in :</B> %s<BR>\n", duration->string);
	    printf("<B>total bytes in submission: </B>");
	    printLongWithCommas(stdout, sub->byteCount);
	    printf("<BR>\n");
	    printf("<B>transfer speed:</B> ");
	    printLongWithCommas(stdout, sub->newBytes/(endTime - startTime));
	    printf(" bytes/sec<BR>\n");
	    }
	}
    }
cgiMakeHiddenVar("email", email);
cgiMakeHiddenVar("url", url);
cgiMakeButton("monitor", "refresh status");
}

void doMiddle()
/* testSimpleCgi - A simple cgi script.. */
{
printf("<FORM ACTION=\"../cgi-bin/edwWebSubmit\" METHOD=GET>\n");
struct sqlConnection *conn = sqlConnect(edwDatabase);
if (!cgiVarExists("email"))
    logIn(conn);
else if (!cgiVarExists("url"))
    getUrl(conn);
else if (!cgiVarExists("monitor"))
    submitUrl(conn);
else
    monitorSubmission(conn);
printf("</FORM>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
htmShell("ENCODE Data Warehouse", doMiddle, NULL);
return 0;
}
