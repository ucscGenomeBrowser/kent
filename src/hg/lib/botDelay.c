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
	sleep1000(millis);
    }
}

void hgBotDelay()
/* High level bot delay call - looks up bottleneck server
 * in hg.conf. */
{
char *host = cfgVal("bottleneck.host");
char *port = cfgVal("bottleneck.port");
botDelayCgi(host, atoi(port));
}
