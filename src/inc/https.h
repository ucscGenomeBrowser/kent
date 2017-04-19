/* Connect via https. */

#ifndef NET_HTTPS_H
#define NET_HTTPS_H

int netConnectHttps(char *hostName, int port, boolean noProxy);
/* Return socket for https connection with server or -1 if error. */

#endif//ndef NET_HTTPS_H
