/* netS - net server. */
#include "paraCommon.h"
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
int bufSize = 1024*1024;

/* Set up socket and self to listen to it. */
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = 0;
socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
if (bind(socketHandle, (struct sockaddr *)&sai, sizeof(sai)) == -1)
    errAbort("Couldn't bind socket");
printf("Hit enter to continue\n");
gets(buf);
printf("Ok, recieving\n");
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


