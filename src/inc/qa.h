/* qa - Module to help do testing, especially on html based apps. */

#ifndef QA_H
#define QA_H

#ifndef HTMLPAGE_H
#include "htmlPage.h"
#endif

char *qaStringBetween(char *text, char *startPattern, char *endPattern);
/* Return text that occurs between startPattern and endPattern,
 * or NULL if no startPattern.  (Will return up to 100 characters
 * after startPattern if there is no endPattern) */

char *qaScanForErrorMessage(char *text);
/* Scan text for error message.  If one exists then
 * return copy of it.  Else return NULL. */

int qaCountBetween(char *s, char *startPattern, char *endPattern, 
	char *midPattern);

struct slName *qaRandomSample(char *db, char *table, char *field, int count);
/* Get random sample from database. */


struct qaStatus
/* Timing and other info about fetching a web page. */
    {
    struct qaStatus *next;
    int milliTime;	/* Time page fetch took. */
    char *errMessage;	/* Error message if any. */
    boolean hardError;	/* Crash of some sort. */
    };

struct qaStatus *qaPageGet(char *url, struct htmlPage **retPage);
/* Get info on given url, (and return page if retPage non-null). */

struct qaStatus *qaPageFromForm(struct htmlPage *origPage, struct htmlForm *form, 
	char *buttonName, char *buttonVal, struct htmlPage **retPage);
/* Get update to form based on pressing a button. */

void qaStatusSoftError(struct qaStatus *qs, char *format, ...);
/* Add error message for something less than a crash. */

void qaStatusReportOne(FILE *f, struct qaStatus *qs, char *format, ...);
/* Report status */

struct qaStatistics
/* Stats on one set of tests. */
    {
    struct qaStatistics *next;
    int testCount;	/* Number of tests. */
    int softCount;	/* Soft error count. */
    int hardCount;	/* Hard error count. */
    long milliTotal;	/* Time tests took. */
    };

void qaStatisticsAdd(struct qaStatistics *stats, struct qaStatus *qs);
/* Add test results to totals */

void qaStatisticsReport(struct qaStatistics *stats, char *label, FILE *f);
/* Write a line of stats to file. */

#endif /* QA_H */

