/* botDelay - contact bottleneck server and sleep 
 * for a little bit if IP address looks like it is
 * being just too demanding. */

int botDelayTime(char *host, int port, char *ip);
/* Figure out suggested delay time for ip address in
 * milliseconds. */

void botDelayCgi(char *host, int port);
/* Connect with bottleneck server and sleep the
 * amount it suggests for IP address calling CGI script. */

void hgBotDelay();
/* High level bot delay call - looks up bottleneck server
 * in hg.conf. */
