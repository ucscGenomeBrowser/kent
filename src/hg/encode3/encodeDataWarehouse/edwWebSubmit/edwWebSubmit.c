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

/* Write email and URL to fifo used by submission spooler. */
FILE *fifo = mustOpen("../userdata/edwSubmit.fifo", "w");
fprintf(fifo, "%s %s\n", userEmail, url);
carefulClose(&fifo);

/* Give system a second to react, and then try to put up status info about submission. */
printf("Submission of %s is in progress....", url);
cgiMakeButton("monitor", "monitor submission");
edwPrintLogOutButton();
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

int countNewValid(struct sqlConnection *conn, struct edwSubmit *sub)
/* Count number of new files in submission that have been validated. */
{
char query[256];
safef(query, sizeof(query), 
    "select count(*) from edwFile e,edwValidFile v where e.id = v.fileId and e.submitId=%u",
    sub->id);
return sqlQuickNum(conn, query);
}


void monitorSubmission(struct sqlConnection *conn)
/* Write out information about submission. */
{
char *url = trimSpaces(cgiString("url"));
cgiMakeHiddenVar("url", url);
struct edwSubmit *sub = edwMostRecentSubmission(conn, url);
if (sub == NULL)
    {
    /* Would be nice to have a way to check this rather than blindly
     * reassuring user.  Also to give them their queue position.  Since
     * the queue is a unix fifo though I'm not sure if we can easily
     * figure out what's piled in it.  I just did a test and it 
     * looks like the file size according to 'ls' at least doesn't change
     * when a fifo has stuff pending in it. It's one argument in favor of
     * shifting the queue to the database like the validation queue.
     * It's so nice not to have to poll though,  which the fifo gets you 
     * out of. */
    printf("%s is in submission queue, but upload has not started", url);
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
    printf("<B>new files validated:</B> %u of %u<BR>\n", countNewValid(conn, sub), sub->newFiles);

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
    struct dyString *duration = formatDuration(timeSpan);

    /* Try and give them an ETA if we aren't finished */
    if (endUploadTime == 0 && timeSpan > 0)
	{
	printf("<B>time so far:</B> %s<BR>\n", duration->string);
	double bytesPerSecond = (double)transferredThisTime/timeSpan;
	long long bytesRemaining = thisUploadSize - curSize - sub->newBytes;
	if (bytesPerSecond > 0)
	    {
	    long long estimatedFinish = bytesRemaining/bytesPerSecond;
	    struct dyString *eta = formatDuration(estimatedFinish);
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
struct sqlConnection *conn = sqlConnect(edwDatabase);
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
