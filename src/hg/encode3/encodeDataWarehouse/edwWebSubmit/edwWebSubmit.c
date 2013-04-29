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
printf("Welcome to the prototype ENCODE Data Warehouse submission site.<BR>");
printf("Please sign in via Persona");
printf("<INPUT TYPE=BUTTON NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
}

void getUrl(struct sqlConnection *conn)
/* Put up URL. */
{
edwMustGetUserFromEmail(conn, userEmail);
printf("Please enter a URL for a validated manifest file:<BR>");
printf("URL ");
cgiMakeTextVar("url", emptyForNull(cgiOptionalString("url")), 80);
cgiMakeButton("submitUrl", "submit");
printf("<BR>Submission by %s", userEmail);
edwPrintLogOutButton();
}

struct edwFile *edwFileInProgress(struct sqlConnection *conn, int submitId)
/* Return file in submission in process of being uploaded if any. */
{
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where submitId=%u and endUploadTime=0 limit 1", submitId);
return edwFileLoadByQuery(conn, query);
}

int positionInQueue(struct sqlConnection *conn, char *url)
/* Return position of our URL in submission queue */
{
char query[256];
safef(query, sizeof(query), "select commandLine from edwSubmitJob where startTime = 0");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int aheadOfUs = -1;
int pos = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *line = row[0];
    char *edwSubmit = nextQuotedWord(&line);
    char *lineUrl = nextQuotedWord(&line);
    if (sameOk(edwSubmit, "edwSubmit") && sameOk(url, lineUrl))
        {
	aheadOfUs = pos;
	break;
	}
    ++pos;
    }
sqlFreeResult(&sr);
return aheadOfUs;
}

void monitorSubmission(struct sqlConnection *conn)
/* Write out information about submission. */
{
char *url = trimSpaces(cgiString("url"));
cgiMakeHiddenVar("url", url);
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
if (sub == NULL)
    {
    int posInQueue = positionInQueue(conn, url);
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
    time_t startTime = sub->startUploadTime;
    time_t endUploadTime = sub->endUploadTime;
    time_t endTime = (endUploadTime ? endUploadTime : edwNow());
    int timeSpan = endTime - startTime;
    long long thisUploadSize = sub->byteCount - sub->oldBytes;
    long long curSize = 0;  // Amount of current file we know we've transferred.

    /* Print title letting them know if upload is done or in progress. */
    printf("<B>Submission by %s is ", userEmail);
    if (endUploadTime != 0)  
        printf("uploaded.");
    else if (!isEmpty(sub->errorMessage))
	{
        printf("having problems...");
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
		curSize = fileSize(path);
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
	cgiMakeButton("getUrl", "submit another data set");
	}
    }
cgiMakeButton("monitor", "refresh status");
printf(" <input type=\"button\" value=\"browse submissions\" "
       "onclick=\"window.location.href='edwWebBrowse';\">\n");

edwPrintLogOutButton();
}

void submitUrl(struct sqlConnection *conn)
/* Submit validated manifest if it is not already in process.  Show
 * progress once it is in progress. */
{
/* Parse email and URL out of CGI vars. Do a tiny bit of error checking. */
char *url = trimSpaces(cgiString("url"));
if (!stringIn("://", url))
    errAbort("%s doesn't seem to be a valid URL, no '://'", url);

/* Do some reality checks that email and URL actually exist. */
edwMustGetUserFromEmail(conn, userEmail);
int sd = netUrlMustOpenPastHeader(url);
close(sd);

/* Create command and add it to edwSubmitJob table. */
char command[strlen(url) + strlen(userEmail) + 256];
safef(command, sizeof(command), "edwSubmit '%s' %s", url, userEmail);
char escapedCommand[2*strlen(command) + 1];
sqlEscapeString2(escapedCommand, command);
char query[strlen(escapedCommand)+128];
safef(query, sizeof(query), "insert edwSubmitJob (commandLine) values('%s')", escapedCommand);
sqlUpdate(conn, query);

/* Write sync signal (any string ending with newline) to fifo to wake up daemon. */
FILE *fifo = mustOpen("../userdata/edwSubmit.fifo", "w");
fputc('\n', fifo);
carefulClose(&fifo);

/* Give the system a half second to react and then put up status info about submission */
sleep1000(500);
monitorSubmission(conn);

#ifdef OLD
/* Give system a second to react, and then try to put up status info about submission. */
/* consider replacing this with direct call to monitorSubmission. */
printf("Submission of %s is in progress....", url);
cgiMakeButton("monitor", "monitor submission");
edwPrintLogOutButton();
cgiMakeHiddenVar("url", url);
#endif /* OLD */
}

static void localWarn(char *format, va_list args)
/* A little warning handler to override the one with the button that goes nowhere. */
{
printf("<B>Error:</B> ");
vfprintf(stdout, format, args);
printf("<BR>Please use the back button on your web browser, correct the error, and resubmit.");
}

void doMiddle()
/* testSimpleCgi - A simple cgi script.. */
{
pushWarnHandler(localWarn);
printf("<FORM ACTION=\"../cgi-bin/edwWebSubmit\" METHOD=GET>\n");
struct sqlConnection *conn = edwConnectReadWrite(edwDatabase);
userEmail = edwGetEmailAndVerify();
if (userEmail == NULL)
    logIn();
else if (cgiVarExists("submitUrl"))
    submitUrl(conn);
else if (cgiVarExists("monitor"))
    monitorSubmission(conn);
else
    getUrl(conn);
printf("</FORM>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();

/* Put out HTTP header and HTML HEADER all the way through <BODY> */
edwWebHeaderWithPersona("Submit data to ENCODE Data Warehouse");

/* Call error handling wrapper that catches us so we write /BODY and /HTML to close up page
 * even through an errAbort. */
htmEmptyShell(doMiddle, NULL);

edwWebFooterWithPersona();
return 0;
}
