/* netS - net server. */
#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dystring.h"

int socketHandle;
char signature[] = "0d2f";

void usage()
/* Explain usage and exit. */
{
errAbort("netS - net server.\n"
         "usage:\n"
	 "    netS now\n");
}


void netS()
/* netS - a net server. */
{
static struct sockaddr_in sai;
char buf[513];
char *line;
int readSize;
struct hostent *hostent;
int port = 0x46DC;
char *command;

/* Set up socket and self to listen to it. */
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = INADDR_ANY;
socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
if (bind(socketHandle, (struct sockaddr *)&sai, sizeof(sai)) == -1)
    errAbort("Couldn't bind socket");

for (;;)
    {
    readSize = recv(socketHandle, buf, sizeof(buf)-1, 0);
    buf[readSize] = 0;
    if (!startsWith(signature, buf))
	{
	warn("Command without signature\n%s", buf);
	continue;
	}
    line = buf + sizeof(signature);
    printf("server read '%s'\n", line);
    command = nextWord(&line);
    if (sameString("quit", command))
	break;
    else
	warn("Unrecognised command %s", command);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
netS();
}


