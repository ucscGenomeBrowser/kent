/* gfNet.c - Network dependent stuff for blat server. */
#include "common.h"
#include "errabort.h"
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "genoFind.h"

static struct sockaddr_in sai;		/* Some system socket info. */

static int setupSocket(char *hostName, char *portName)
/* Set up our socket. */
{
int port;
int sd;
struct hostent *hostent;

if (!isdigit(portName[0]))
    errAbort("Expecting a port number got %s", portName);
port = atoi(portName);
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    errAbort("Couldn't find host %s. h_errno %d", hostName, h_errno);
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
memcpy(&sai.sin_addr.s_addr, hostent->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    {
    errnoAbort("Couldn't setup socket %s %s", hostName, portName);
    }
return sd;
}

int gfConnect(char *hostName, char *portName)
/* Start connection with server. */
{
/* Connect to server. */
int sd = setupSocket(hostName, portName);
if (connect(sd, &sai, sizeof(sai)) == -1)
    {
    errnoAbort("Sorry, the BLAT server seems to be down.  Please try "
               "again later.");
    }
return sd;
}

