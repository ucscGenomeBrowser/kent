/* apacheLog - stuff to parse out apache web server logs, currently
 * just the access log. */

#ifndef APACHELOG_H
#define APACHELOG_H

struct apacheAccessLog
/* Parsed out apache access log line */
    {
    struct apacheAccessLog *next;
    char *buf;		/* All memory for apacheAccessLog fields is allocated at once here. */
    char *ip;		/* IP Address: dotted quad of numbers, or xxx.com. */
    char *dash1;	/* Unknown, usually a dash */
    char *dash2;	/* Unknown, usually a dash */
    char *timeStamp;	/* Time stamp like 23/Nov/2003:04:21:08 */
    char *timeZone;	/* Extra number after timeStamp, usually -0800 */
    char *method;	/* GET/POST etc. */
    char *url;		/* Requested URL */
    char *httpVersion;  /* Something like HTTP/1.1 */
    int status;		/* Status code - 200 is good! */
    char *num1;		/* Some number, I'm not sure what it is. */
    char *referrer;	/* Referring URL, may be NULL. */
    char *program;	/* Requesting program,  often Mozilla 4.0 */
    time_t tick;	/* Unix tick (seconds since 1970) - derived from timeStamp. */
    int runTime;	/* Overall time (optional) in milliseconds */
    };

struct apacheAccessLog *apacheAccessLogParse(char *line, 
	char *fileName, int lineIx);
/* Return a apacheAccessLog from line.  Return NULL if there's a parsing 
 * problem, but don't abort. */

void apacheAccessLogFree(struct apacheAccessLog **pLl);
/* Free up apacheAccessLog. */

time_t apacheAccessLogTimeToTick(char *timeStamp);
/* Convert something like 27/Aug/2009:09:25:32 to Unix timestamp (seconds since 1970).
 * On error returns zero. */

int apacheAccessLogCmpTick(const void *va, const void *vb);
/* Compare items to sort by tick (which tracks timestamp) */

#endif /* APACHELOG_H */
