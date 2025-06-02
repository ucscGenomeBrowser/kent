/* botDelay - contact bottleneck server and sleep 
 * for a little bit if IP address looks like it is
 * being just too demanding. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

int botDelayTime(char *host, int port, char *ip);
/* Figure out suggested delay time for ip address in
 * milliseconds. */

char *botDelayWarningMsg(char *ip, int millis);
/* return the string for the default botDelay message
 * not all users of botDelay want the message to go to stderr
 * return it for their own use case
 */

void botDelayMessage(char *ip, int millis);
/* Print out message saying why you are stalled. */

int hgBotDelayTime();
/* Get suggested delay time from cgi using the standard penalty. */

int hgBotDelayTimeFrac(double fraction);
/* Get suggested delay time from cgi using the specified fraction of the standard penalty. */

extern int botDelayMillis;

boolean earlyBotCheck(long enteredMainTime, char *cgiName, double delayFrac, int warnMs, int exitMs, char *exitType);
/* for use before the CGI has started any
 * output or setup the cart of done any MySQL operations.  The boolean
 * return is used later in the CGI after it has done all its setups and
 * started output so it can issue the warning.  Pass in delayFrac 0.0
 * to use the default 1.0, pass in 0 for warnMs and exitMs to use defaults,
 * and exitType is either 'html' or 'json' to do that type of exit output in
 * the case of hogExit();
 */

int hgBotDelayCurrWarnMs();
/* get number of millis that are tolerated until a warning is shown on the most recent call to earlyBotCheck */

boolean botException();
/* check if the remote ip address is on the exceptions list */
