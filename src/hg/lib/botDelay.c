/* botDelay.c - contact bottleneck server and sleep 
 * for a little bit if IP address looks like it is
 * being just too demanding. */

#include "common.h"
#include "net.h"
#include "portable.h"
#include "hgConfig.h"
#include "botDelay.h"

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
	    printf("<BR>There is a very high volume of traffic coming from your "
	           "site (IP address %s).  So that other users get a fair share "
		   "of our bandwidth we are putting in a delay of %3.1f seconds "
		   "before we service your request.  This delay will slowly "
		   "decrease as activity returns to normal.  This high volume "
		   "of traffic is likely to be due to program-driven rather than "
		   "interactive access.  If it is your program please write "
		   "genome@cse.ucsc.edu and inquire to see if there are more "
		   "efficient ways to access our data.  If you are sharing an IP "
		   "address with someone else's program we apologize for the "
		   "inconvenience. Please contact genome-www@cse.ucsc.edu if "
		   "you think this delay is being imposed unfairly.<BR><HR>", 
		   	ip, 0.001*millis);
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
int delay;
if (host != NULL && port != NULL)
    botDelayCgi(host, atoi(port));
}
