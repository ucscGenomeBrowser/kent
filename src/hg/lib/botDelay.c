/* botDelay.c - contact bottleneck server and sleep 
 * for a little bit if IP address looks like it is
 * being just too demanding. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "net.h"
#include "portable.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "hui.h"
#include "hCommon.h"
#include "botDelay.h"


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

void botDelayMessage(char *ip, int millis)
/* Print out message saying why you are stalled. */
{
time_t now = time(NULL);
warn("There is a very high volume of traffic coming from your "
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
	    ip, asctime(localtime(&now)), .001*millis);
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

static char *getBotCheckString(char *ip, double fraction)
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

void botDelayCgi(char *host, int port, boolean noWarn, double fraction)
/* Connect with bottleneck server and sleep the
 * amount it suggests for IP address calling CGI script,
 * after imposing the specified fraction of the access penalty. */
{
int millis;
char *ip = getenv("REMOTE_ADDR");
if (ip != NULL)
    {
    char *botCheckString = getBotCheckString(ip, fraction);    
    millis = botDelayTime(host, port, botCheckString);
    freeMem(botCheckString);
    if (millis > 0)
	{
	if (millis > 10000)
	    {
	    if (millis > 20000)
	        botTerminateMessage(ip, millis);
	    else
		{
		if (!noWarn)
		    botDelayMessage(ip, millis);
		}
	    }
	sleep1000(millis);
	}
    }
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

static void hgBotDelayExt(boolean noWarn, double fraction)
/* High level bot delay call - looks up bottleneck server
 * in hg.conf. */
{
if (botException())
    return;

char *host = cfgOption("bottleneck.host");
char *port = cfgOption("bottleneck.port");

if (host != NULL && port != NULL)
    botDelayCgi(host, atoi(port), noWarn, fraction);
}

void hgBotDelay()
/* High level bot delay call - for use with regular webpage output */ 
{
hgBotDelayExt(FALSE, 1.0);
}

void hgBotDelayFrac(double fraction)
/* Like hgBotDelay, but imposes a fraction of the standard access penalty */ 
{
hgBotDelayExt(FALSE, fraction);
}

void hgBotDelayNoWarn()
/* High level bot delay call without warning - for use with non-webpage outputs */
{
hgBotDelayExt(TRUE, 1.0);
}

void hgBotDelayNoWarnFrac(double fraction)
/* Like hgBotDelayNoWarn, but imposes a fraction of the standard access penalty */
{
hgBotDelayExt(TRUE, fraction);
}

int hgBotDelayTime()
{
return hgBotDelayTimeFrac(1.0);
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

