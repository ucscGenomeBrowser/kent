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
printf("Welcome to the prototype ENCODE Data Warehouse submission site.<BR>");
printf("Please enter your email address ");
cgiMakeTextVar("email", "", 32);
cgiMakeButton("getUrl", "submit");
}

void getUrl(struct sqlConnection *conn)
/* Put up URL. */
{
char *email = trimSpaces(cgiString("email"));
struct edwUser *user = edwMustGetUserFromEmail(conn, email);
cgiMakeHiddenVar("email", user->email);
printf("URL ");
cgiMakeTextVar("url", "", 80);
cgiMakeButton("submitUrl", "submit");
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
printf("Submission of %s is in progress....", url);
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

int showValidationOverview(struct sqlConnection *conn, struct edwSubmit *sub)
/* Show validation status overview and return number of newly valid items in this
 * submission */
{
printf("<B>Old files from previous submission:</B> %d<BR>\n", sub->oldFiles);
printf("<B>New files transferred so far:</B> %d<BR>\n", sub->newFiles);
char query[256];
safef(query, sizeof(query), 
    "select count(*) from edwFile where submitId=%u and tags != '' and tags is not null", sub->id);
int uploadCount = sqlQuickNum(conn, query);
safef(query, sizeof(query), 
    "select count(*) from edwFile e,edwValidFile v where e.id = v.fileId and e. submitId=%u",
    sub->id);
int validCount = sqlQuickNum(conn, query);
printf("<B>validated:</B> %d of %d new files<BR>\n", validCount, uploadCount);
safef(query, sizeof(query),
    "select count(*) from edwFile where errorMessage is not null and errorMessage != '' "
    "and submitId=%u", sub->id);
int errCount = sqlQuickNum(conn, query);
printf("<B>validation error count:</B> %d<BR>\n", errCount);
return validCount;
}

void showValidations(struct sqlConnection *conn)
{
printf("Theoretically I would be showing validations.  In real life advise you to go back.");
}

void monitorSubmission(struct sqlConnection *conn)
/* Write out information about submission. */
{
char *email = trimSpaces(cgiString("email"));
char *url = trimSpaces(cgiString("url"));
cgiMakeHiddenVar("url", url);
cgiMakeHiddenVar("email", email);
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
    printf("<B>Submission by %s is ", email);
    if (endUploadTime == 0)  
	printf("in progress...");
    else
        printf("uploaded.");
    printf("</B><BR>\n");

    /* Print URL and how far along we are at the file level. */
    printf("<B>url:</B> %s<BR>\n", sub->url);
    printf("<B>files count:</B> %d<BR>\n", sub->fileCount);
    if (sub->oldFiles > 0)
	printf("<B>files already in warehouse:</B> %u<BR>\n", sub->oldFiles);
    if (sub->oldFiles != sub->fileCount)
	{
	printf("<B>files transferred:</B> %u<BR>\n", sub->newFiles);
	printf("<B>files remaining:</B> %u<BR>\n", sub->fileCount - sub->oldFiles - sub->newFiles);
	}

    /* Print error message, and in case of error skip file-in-transfer info. */
    if (!isEmpty(sub->errorMessage))
        printf("<B>error:</B> %s<BR>\n", sub->errorMessage);
    else
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
    printf(" (%d%%)<BR>\n", (int)(100.0 * totalTransferred / sub->byteCount));

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
    cgiMakeButton("monitor", "refresh status");
    }
#ifdef SOON
uglyf("<BR><BR>--- The part down from here is really rough do not fret if it doesn't work ---<BR>\n");
int validCount = showValidationOverview(conn, sub);
if (validCount > 0)
    cgiMakeButton("validations", "see validation results");
#endif /* SOON */
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
if (cgiVarExists("getUrl"))
    getUrl(conn);
else if (cgiVarExists("submitUrl"))
    submitUrl(conn);
else if (cgiVarExists("monitor"))
    monitorSubmission(conn);
else if (cgiVarExists("validations"))
    showValidations(conn);
else
    logIn(conn);
printf("</FORM>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();

/* Put out HTTP header and HTML HEADER all the way through <BODY> */
puts("Content-Type:text/html");
puts("\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
	      "\"http://www.w3.org/TR/html4/loose.dtd\">");
puts("<HTML><HEAD><TITLE>ENCODE Data Warehouse</TITLE></HEAD><BODY>");

/* Call error handling wrapper that catches us so we write /BODY and /HTML to close up page
 * even through an errAbort. */
htmEmptyShell(doMiddle, NULL);
htmlEnd();
return 0;
}
