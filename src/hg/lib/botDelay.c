/* botDelay.c - contact bottleneck server and sleep
 * for a little bit if IP address looks like it is
 * being just too demanding. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "net.h"
#include "portable.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "hui.h"
#include "hCommon.h"
#include "botDelay.h"
#include "jsonWrite.h"

#define defaultDelayFrac 1.0   /* standard penalty for most CGIs */
#define defaultWarnMs 10000    /* warning at 10 to 20 second delay */
#define defaultExitMs 20000    /* error 429 Too Many Requests after 20+ second delay */

int botDelayWarnMs  = 0;       /* global so the previously used value can be retrieved */

int botDelayTime(char *host, int port, char *botCheckString)
/* Figure out suggested delay time for ip address in
 * milliseconds. */
{
int sd = netMustConnect(host, port);
char buf[256];
netSendString(sd, botCheckString);
netRecieveString(sd, buf);
close(sd);
return atoi(buf);
}

char *botDelayWarningMsg(char *ip, int millis)
/* return the string for the default botDelay message
 * not all users of botDelay want the message to go to stderr
 * return it for their own use case
 */
{
time_t now = time(NULL);
char *delayMsg = needMem(2048);
safef(delayMsg, 2048,
    "There is a very high volume of traffic coming from your "
    "site (IP address %s) as of %s (California time).  So that other "
    "users get a fair share "
    "of our bandwidth, we are putting in a delay of %3.1f seconds "
    "before we service your request.  This delay will slowly "
    "decrease over a half hour as activity returns to normal.  This "
    "high volume of traffic is likely due to program-driven rather than "
    "interactive access, or the submission of queries on a large "
    "number of sequences.  If you are making large batch queries, "
    "please write to our genome@soe.ucsc.edu public mailing list "
    "and inquire about more efficient ways to access our data.  "
    "If you are sharing an IP address with someone who is submitting "
    "large batch queries, we apologize for the "
    "inconvenience. "
    "To use the genome browser functionality from a Unix command line, "
    "please read <a href='http://genome.ucsc.edu/FAQ/FAQdownloads.html#download36'>our FAQ</a> on this topic. "
    "For further help on how to access our data from a command line, "
    "or if "
    "you think this delay is being imposed unfairly, please contact genome-www@soe.ucsc.edu.",
    ip, asctime(localtime(&now)), 0.001*millis);

return delayMsg;
}	/*	char *botDelayWarningMsg(char *ip, int millis)	*/

void botDelayMessage(char *ip, int millis)
/* Print out message saying why you are stalled. */
{
warn("%s", botDelayWarningMsg(ip, millis));
}

void botTerminateMessage(char *ip, int millis)
/* Print out message saying why you are terminated. */
{
time_t now = time(NULL);
hUserAbort("There is an exceedingly high volume of traffic coming from your "
       "site (IP address %s) as of %s (California time).  It looks like "
       "a web robot is launching queries quickly, and not even waiting for "
       "the results of one query to finish before launching another query. "
       "/* We cannot service requests from your IP address under */ these "
       "conditions.  (code %d)"
       "To use the genome browser functionality from a Unix command line, "
       "please read <a href='http://genome.ucsc.edu/FAQ/FAQdownloads.html#download36'>our FAQ</a> on this topic. "
       "For further help on how to access our data from a command line, "
       "or if "
       "you think this delay is being imposed unfairly, please contact genome-www@soe.ucsc.edu."
       , ip, asctime(localtime(&now)), millis);
}

static char *getCookieUser()
/* get user from hguid cookie */
{
char *user = NULL;
char *centralCookie = hUserCookie();

if (centralCookie)
    user = findCookieData(centralCookie);

return user;
}

char *getBotCheckString(char *ip, double fraction)
/* compose "user.ip fraction" string for bot check */
{
char *user = getCookieUser();
char *botCheckString = needMem(256);
if (user)
  safef(botCheckString, 256, "%s.%s %f", user, ip, fraction);
else
  safef(botCheckString, 256, "%s %f", ip, fraction);
return botCheckString;
}

boolean botException()
/* check if the remote ip address is on the exceptions list */
{
char *exceptIps = cfgOption("bottleneck.except");
if (exceptIps)
    {
    char *remoteAddr = getenv("REMOTE_ADDR");
    if (remoteAddr)
	{
	char *s = exceptIps;
	boolean found = FALSE;
	while (s && !found)
	    {
	    char *e = strchr(s, ' ');
	    if (e)
		*e = 0;
	    if (sameString(remoteAddr, s))
		found = TRUE;
	    if (e)
		*e++ = ' ';
	    s = e;
	    }
	if (found)
	    return TRUE;
	}
    }
return FALSE;
}

int hgBotDelayTime()
{
return hgBotDelayTimeFrac(defaultDelayFrac);
}

int hgBotDelayTimeFrac(double fraction)
/* Get suggested delay time from cgi using the standard penalty. */
{
char *ip = getenv("REMOTE_ADDR");
char *host = cfgOption("bottleneck.host");
char *port = cfgOption("bottleneck.port");

int delay = 0;
if (host != NULL && port != NULL && ip != NULL)
    {
    char *botCheckString = getBotCheckString(ip, fraction);
    delay = botDelayTime(host, atoi(port), botCheckString);
    freeMem(botCheckString);
    }
return delay;
}

#define err429  429
#define err429Msg       "Too Many Requests"
int botDelayMillis = 0;

static void jsonHogExit(char *cgiExitName, long enteredMainTime, char *hogHost,
    int retryAfterSeconds)
/* err429 Too Many Requests to be returned as JSON data */
{
puts("Content-Type:application/json");
printf("Status: %d %s\n", err429, err429Msg);
if (retryAfterSeconds > 0)
    printf("Retry-After: %d", retryAfterSeconds);
puts("\n");	/* blank line between header and body */

struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "error", err429Msg);
jsonWriteNumber(jw, "statusCode", err429);

char msg[1024];

safef(msg, sizeof(msg), "Your host, %s, has been sending too many requests "
       "lately and is unfairly loading our site, impacting performance for "
       "other users.  Please contact genome-www@soe.ucsc.edu to ask that your site "
       "be reenabled.  Also, please consider downloading sequence and/or "
       "annotations in bulk -- see http://genome.ucsc.edu/downloads.html.",
       hogHost);

jsonWriteString(jw, "statusMessage", msg);
if (retryAfterSeconds > 0)
    jsonWriteNumber(jw, "retryAfterSeconds", retryAfterSeconds);

jsonWriteObjectEnd(jw);

puts(jw->dy->string);
}

static void hogExit(char *cgiName, long enteredMainTime, char *exitType,
    int retryAfterSeconds)
/* earlyBotCheck requests exit before CGI has done any output or
 * setups of any kind.  HTML output has not yet started.
 */
{
char *hogHost = getenv("REMOTE_ADDR");
char cgiExitName[1024];
safef(cgiExitName, ArraySize(cgiExitName), "%s hogExit", cgiName);

if (sameOk("json", exitType))
   jsonHogExit(cgiExitName, enteredMainTime, hogHost, retryAfterSeconds);
else
    {

    puts("Content-Type:text/html");
    printf("Status: %d %s\n", err429, err429Msg);
    if (retryAfterSeconds > 0)
        printf("Retry-After: %d", retryAfterSeconds);
    puts("\n");	/* blank line between header and body */

    puts("<!DOCTYPE HTML 4.01 Transitional>\n");
    puts("<html lang='en'>");
    puts("<head>");
    puts("<meta charset=\"utf-8\">");
    printf("<title>Status %d: %s</title></head>\n", err429, err429Msg);

    printf("<body><h1>Status %d: %s</h1><p>\n", err429, err429Msg);
    time_t now = time(NULL);
    printf("There is an exceedingly high volume of traffic coming from your "
           "site (IP address %s) as of %s (California time).  It looks like "
           "a web robot is launching queries quickly, and not even waiting for "
           "the results of one query to finish before launching another query. "
           "<b>We cannot service requests from your IP address under</b> these "
           "conditions.  (code %d) "
           "To use the genome browser functionality from a Unix command line, "
           "please read <a href='http://genome.ucsc.edu/FAQ/FAQdownloads.html#download36'>our FAQ</a> on this topic. "
           "For further help on how to access our data from a command line, "
           "or if "
           "you think this delay is being imposed unfairly, please contact genome-www@soe.ucsc.edu."
           ,hogHost, asctime(localtime(&now)), botDelayMillis);
    puts("</body></html>");
    sleep(15);
    }
cgiExitTime(cgiExitName, enteredMainTime);
exit(0);
}       /*      static void hogExit()   */

boolean earlyBotCheck(long enteredMainTime, char *cgiName, double delayFrac, int warnMs, int exitMs, char *exitType)
/* replaces the former botDelayCgi now in use before the CGI has started any
 * output or setup the cart of done any MySQL operations.  The boolean
 * return is used later in the CGI after it has done all its setups and
 * started output so it can issue the warning.  Pass in delayFrac 0.0
 * to use the default 1.0, pass in 0 for warnMs and exitMs to use defaults,
 * and exitType is either 'html' or 'json' to do that type of exit output in
 * the case of hogExit();
 */
{
boolean issueWarning = FALSE;

if (botException())	/* don't do this if caller is on the exception list */
    return issueWarning;

if (delayFrac < 0.000001) /* passed in zero, use default */
    delayFrac = defaultDelayFrac;

botDelayWarnMs = warnMs;
if (botDelayWarnMs < 1)	/* passed in zero, use default */
    botDelayWarnMs = defaultWarnMs;

if (exitMs < 1)	/* passed in zero, use default */
    exitMs = defaultExitMs;

botDelayMillis = hgBotDelayTimeFrac(delayFrac);
if (botDelayMillis > 0)
    {
    if (botDelayMillis > botDelayWarnMs)
	{
	if (botDelayMillis > exitMs) /* returning immediately */
            {
            int msAboveWarning = botDelayMillis - botDelayWarnMs;
            int retryAfterSeconds = 0;
            if (msAboveWarning > 0)
               retryAfterSeconds = 1 + (msAboveWarning / 10);
	    hogExit(cgiName, enteredMainTime, exitType, retryAfterSeconds);
            }
	else
	    issueWarning = TRUE;

        sleep1000(botDelayMillis); /* sleep when > botDelayWarnMs and < exitMs */
	}
    }
return issueWarning;	/* caller can decide on their type of warning */
}	/*	boolean earlyBotCheck()	*/

int hgBotDelayCurrWarnMs()
/* get number of millis that are tolerated until a warning is shown on the most recent call to earlyBotCheck */
{
    return botDelayWarnMs;
}

