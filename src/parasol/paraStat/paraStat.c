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
errAbort("paraStat - query status of parasol node servers.\n"
         "usage:\n"
	 "    paraStat machineList\n");
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
    return -1;
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(paraPort);
memcpy(&sai.sin_addr.s_addr, host->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    return -1;
    }
if ((ns = connect(sd, &sai, sizeof(sai))) == -1)
    {
    close(sd);
    return -1;
    }
return sd;
}

void paraStat(char *machineList)
/* Stop node server on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
int sd;
char statCmd[256];
char status[256];
int size;

sprintf(statCmd, "%s status", paraSig);
readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    if ((sd = getSocket(name)) >= 0)
	{
	write(sd, statCmd, strlen(statCmd));
	size = read(sd, status, sizeof(status)-1);
	if (size >= 0)
	    {
	    status[size] = 0;
	    printf("%s %s\n", name, status);
	    }
	else
	    {
	    printf("%s no status return\n", name);
	    }
	close(sd);
	}
    else
	{
	printf("%s - no node server\n", name);
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
paraStat(argv[1]);
}


