/* qa - Modules to help do testing, especially on html based apps. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "portable.h"
#include "htmlPage.h"
#include "errabort.h"
#include "errCatch.h"
#include "htmshell.h"
#include "qa.h"

static char const rcsid[] = "$Id: qa.c,v 1.7 2008/09/17 17:56:38 kent Exp $";

char *qaStringBetween(char *text, char *startPattern, char *endPattern)
/* Return text that occurs between startPattern and endPattern,
 * or NULL if no startPattern.  (Will return up to 100 characters
 * after startPattern if there is no endPattern) */
{
char *startMid = stringIn(startPattern, text);
if (startMid != NULL)
    {
    char *endMid;
    int midSize;
    startMid += strlen(startPattern);
    endMid = stringIn(startMid, endPattern);
    if (endMid == NULL)
        {
	midSize = strlen(startMid);
	if (midSize > 100)
	    midSize = 100;
	}
    else
        midSize = endMid - startMid;
    return cloneStringZ(startMid, midSize);
    }
return NULL;
}

char *qaScanForErrorMessage(char *text)
/* Scan text for error message.  If one exists then
 * return copy of it.  Else return NULL. */
{
return qaStringBetween(text, htmlWarnStartPattern(), htmlWarnEndPattern());
}

int qaCountBetween(char *s, char *startPattern, char *endPattern, 
	char *midPattern)
/* Count the number of midPatterns that occur between start and end pattern. */
{
int count = 0;
char *e;
s = stringIn(startPattern, s);
if (s != NULL)
    {
    s += strlen(startPattern);
    e = stringIn(endPattern, s);
    while (s < e)
        {
	if (startsWith(midPattern, s))
	    ++count;
	s += 1;
	}
    }
return count;
}

void qaStatusReportOne(FILE *f, struct qaStatus *qs, char *format, ...)
/* Report status */
{
char *errMessage = qs->errMessage;
char *severity = "ok";
va_list args;
va_start(args, format);
if (errMessage == NULL)
    errMessage = "";
else
    {
    if (qs->hardError)
        severity = "hard";
    else
        severity = "soft";
    }
  
vfprintf(f, format, args);
fprintf(f, " %4.3fs (%s) %s\n", 0.001*qs->milliTime, severity, errMessage);
va_end(args);
}

static struct qaStatus *qaStatusOnPage(struct errCatch *errCatch, 
	struct htmlPage *page, long startTime, struct htmlPage **retPage)
/* Assuming you have fetched page with the given error catcher,
 * starting the fetch at the given startTime, then create a
 * qaStatus that describes how the fetch went.  If *retPage is non-null
 * then return the page there, otherwise free it. */
{
char *errMessage = NULL;
struct qaStatus *qs;
AllocVar(qs);
if (errCatch->gotError || page == NULL)
    {
    errMessage = errCatch->message->string;
    qs->hardError = TRUE;
    }
else
    {
    if (page->status->status != 200)
	{
	dyStringPrintf(errCatch->message, "HTTP status code %d\n", 
		page->status->status);
	errMessage = errCatch->message->string;
	qs->hardError = TRUE;
	htmlPageFree(&page);
	}
    else
        {
	errMessage = qaScanForErrorMessage(page->fullText);
	}
    }
qs->errMessage = cloneString(errMessage);
if (qs->errMessage != NULL)
    subChar(qs->errMessage, '\n', ' ');
qs->milliTime = clock1000() - startTime;
if (retPage != NULL)
    *retPage = page;
else
    htmlPageFree(&page);
return qs;
}

struct qaStatus *qaPageGet(char *url, struct htmlPage **retPage)
/* Get info on given url, (and return page if retPage non-null). */
{
struct errCatch *errCatch = errCatchNew();
struct qaStatus *qs;
struct htmlPage *page = NULL;
long startTime = clock1000();
if (errCatchStart(errCatch))
    {
    page = htmlPageGet(url);
    htmlPageValidateOrAbort(page);
    }
else
    {
    htmlPageFree(&page);
    }
errCatchEnd(errCatch);
qs = qaStatusOnPage(errCatch, page, startTime, retPage);
errCatchFree(&errCatch);
return qs;
}

struct qaStatus *qaPageFromForm(struct htmlPage *origPage, struct htmlForm *form, 
	char *buttonName, char *buttonVal, struct htmlPage **retPage)
/* Get update to form based on pressing a button. */
{
struct errCatch *errCatch = errCatchNew();
struct qaStatus *qs;
struct htmlPage *page = NULL;
long startTime = clock1000();
if (errCatchStart(errCatch))
    {
    page = htmlPageFromForm(origPage, form, buttonName, buttonVal);
    htmlPageValidateOrAbort(page);
    }
else
    {
    htmlPageFree(&page);
    }
errCatchEnd(errCatch);
qs = qaStatusOnPage(errCatch, page, startTime, retPage);
errCatchFree(&errCatch);
return qs;
}

void qaStatusSoftError(struct qaStatus *qs, char *format, ...)
/* Add error message for something less than a crash. */
{
struct dyString *dy = dyStringNew(0);
va_list args, args2;
va_start(args, format);
/* args can't be reused, so vaWarn needs its own va_list: */
va_start(args2, format);
vaWarn(format, args2);
if (qs->errMessage)
    {
    dyStringAppend(dy, qs->errMessage);
    dyStringAppendC(dy, '\n');
    }
dyStringVaPrintf(dy, format, args);
va_end(args);
va_end(args2);
freez(&qs->errMessage);
qs->errMessage = cloneString(dy->string);
dyStringFree(&dy);
}

void qaStatisticsAdd(struct qaStatistics *stats, struct qaStatus *qs)
/* Add test results to totals */
{
stats->testCount += 1;
stats->milliTotal += qs->milliTime;
if (qs->errMessage)
    {
    if (qs->hardError)
        stats->hardCount += 1;
    else
        stats->softCount += 1;
    }
}

void qaStatisticsReport(struct qaStatistics *stats, char *label, FILE *f)
/* Write a line of stats to file. */
{
fprintf(f, "%20s:  %3d tests, %2d soft errors, %2d hard errors, %5.2f seconds\n",
	label, stats->testCount, stats->softCount, stats->hardCount, 
	0.001 * stats->milliTotal);
}


