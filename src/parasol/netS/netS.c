/* netS - net server. */
#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "options.h"
#include "dystring.h"


int socketHandle;

void usage()
/* Explain usage and exit. */
{
errAbort("netS - net server.\n"
         "usage:\n"
	 "    netS now\n");
}

char *ipAddress = "10.1.255.255";

void netS()
/* netS - a net server. */
{
static struct sockaddr_in sai;
char buf[64*1024];
char *line;
int readSize;
struct hostent *hostent;
int port = 0x46DC;
char *command;

/* Set up socket and self to listen to it. */
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = inet_addr(ipAddress);
socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (bind(socketHandle, (struct sockaddr *)&sai, sizeof(sai)) == -1)
    errAbort("Couldn't bind socket");

#ifdef NEVER
/* Set timeouts just to experiment. */
    {
    struct timeval tv;
    int tvSize = sizeof(tv);
    int bufSize;
    int bufSizeSize = sizeof(bufSize);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, tvSize);
    getsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, &tvSize);
    printf("Read timeout = %lu.%lu\n",  tv.tv_sec, tv.tv_usec);
    getsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, &bufSize, &bufSizeSize);
    printf("Read buf size %d\n",  bufSize);
    }
#endif

for (;;)
    {
    int saiSize = sizeof(sai);
    ZeroVar(&sai);
    sai.sin_family = AF_INET;
    readSize = recvfrom(socketHandle, buf, sizeof(buf)-1, 0, (struct sockaddr *)&sai, &saiSize);
    if (readSize > 0)
	{
	buf[readSize] = 0;
	line = buf;
	printf("server read '%s'\n", line);
	command = nextWord(&line);
	if (sameString("quit", command))
	    break;
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
ipAddress = optionVal("ip", ipAddress);
netS();
return 0;
}


