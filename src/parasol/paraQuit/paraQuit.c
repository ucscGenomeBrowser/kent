/* paraHub - parasol hub server. */
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "paraLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraQuit - kill parasol node servers.\n"
         "usage:\n"
	 "    paraQuit machineList\n");
}

int getSocket(char *hostName)
/* Try and fill in socket of mach. */
{
int sd, ns;
struct sockaddr_in sai;
int fromlen;
struct hostent *host;

/* Set up connection. */
host = gethostbyname(hostName);
if (host == NULL)
    {
    herror("");
    warn("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    return -1;
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(paraPort);
memcpy(&sai.sin_addr.s_addr, host->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    warn("Couldn't make socket to %s", hostName);
    return -1;
    }
if ((ns = connect(sd, &sai, sizeof(sai))) == -1)
    {
    warn("Couldn't connect to %s", hostName);
    close(sd);
    return -1;
    }
return sd;
}

void paraQuit(char *machineList)
/* Stop node server on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
int sd;
char buf[256];

sprintf(buf, "%s quit", paraSig);
readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    uglyf("Telling %s to quit \n", name);
    if ((sd = getSocket(name)) >= 0)
	{
	write(sd, buf, strlen(buf));
	close(sd);
	}
    }
freeMem(mNames);
freeMem(mBuf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraQuit(argv[1]);
}


