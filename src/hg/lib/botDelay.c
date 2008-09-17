/* botDelay.c - contact bottleneck server and sleep 
 * for a little bit if IP address looks like it is
 * being just too demanding. */

#include "common.h"
#include "net.h"
#include "portable.h"
#include "hgConfig.h"
#include "botDelay.h"

static char const rcsid[] = "$Id: botDelay.c,v 1.12 2008/09/17 18:10:13 kent Exp $";

int botDelayTime(char *host, int port, char *ip)
/* Figure out suggested delay time for ip address in
 * milliseconds. */
{
int sd = netMustConnect(host, port);
char buf[256];
netSendString(sd, ip);
netRecieveString(sd, buf);
close(sd);
return atoi(buf);
}

void botDelayMessage(char *ip, int millis)
/* Print out message saying why you are stalled. */
{
time_t now = time(NULL);
printf("<BR>There is a very high volume of traffic coming from your "
       "site (IP address %s) as of %s (California time).  So that other "
       "users get a fair share "
       "of our bandwidth, we are putting in a delay of %3.1f seconds "
       "before we service your request.  This delay will slowly "
       "decrease over a half hour as activity returns to normal.  This "
       "high volume of traffic is likely due to program-driven rather than "
       "interactive access, or the submission of queries on a large "
       "number of sequences.  If you are making large batch queries, "
       "please write to our genome@cse.ucsc.edu public mailing list "
       "and inquire about more efficient ways to access our data.  "
       "If you are sharing an IP address with someone who is submitting "
       "large batch queries, we apologize for the "
       "inconvenience. Please contact genome-www@cse.ucsc.edu if "
       "you think this delay is being imposed unfairly.<BR><HR>", 
	    ip, asctime(localtime(&now)), .001*millis);
}

void botTerminateMessage(char *ip, int millis)
/* Print out message saying why you are terminated. */
{
time_t now = time(NULL);
errAbort("There is an exceedingly high volume of traffic coming from your "
       "site (IP address %s) as of %s (California time).  It looks like "
       "a web robot is launching queries quickly, and not even waiting for "
       "the results of one query to finish before launching another query. "
       "We cannot service requests from your IP address under these "
       "conditions.  (code %d)", ip, asctime(localtime(&now)), millis);
}

void botDelayCgi(char *host, int port)
/* Connect with bottleneck server and sleep the
 * amount it suggests for IP address calling CGI script. */
{
int millis;
char *ip = getenv("REMOTE_ADDR");
if (ip != NULL)
    {
    millis = botDelayTime(host, port, ip);
    if (millis > 0)
	{
	if (millis > 10000)
	    {
	    if (millis > 20000)
	        botTerminateMessage(ip, millis);
	    else
		botDelayMessage(ip, millis);
	    }
	sleep1000(millis);
	}
    }
}

void hgBotDelay()
/* High level bot delay call - looks up bottleneck server
 * in hg.conf. */
{
char *host = cfgOption("bottleneck.host");
char *port = cfgOption("bottleneck.port");
if (host != NULL && port != NULL)
    botDelayCgi(host, atoi(port));
}

int hgBotDelayTime()
/* Get suggested delay time from cgi. */
{
char *ip = getenv("REMOTE_ADDR");
char *host = cfgOption("bottleneck.host");
char *port = cfgOption("bottleneck.port");
int delay = 0;
if (host != NULL && port != NULL && ip != NULL)
    delay =  botDelayTime(host, atoi(port), ip);
return delay;
}

