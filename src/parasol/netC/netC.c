/* netC - net client. */
#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

char signature[] = "0d2f070562685f29";

void usage()
/* Explain usage and exit. */
{
errAbort("netC - net client.\n"
         "usage:\n"
	 "    netC host command parameter(s)\n");
}

void netC(char *hostName, int argc, char *argv[])
/* netC - a net client. */
{
int sd;
struct sockaddr_in sai;
int fromlen;
int childCount = 0;
struct hostent *host;
char *alias;
UBYTE *addr;
int i;
int port = 0x46DC;
char buf[512];
int bufPos = 0;
char *command = argv[0];

/* Set up connection. */
host = gethostbyname(hostName);
if (host == NULL)
    {
    herror("");
    errAbort("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
memcpy(&sai.sin_addr.s_addr, host->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
sd = socket(AF_INET, SOCK_STREAM, 0);
if (connect(sd, &sai, sizeof(sai)) == -1)
    errAbort("Couldn't connect to %s", hostName);

/* Put command line back together as one string. */
memcpy(buf, signature, sizeof(signature));
bufPos = sizeof(signature);
for (i=0; i<argc; ++i)
    {
    char *s = argv[i];
    int len = strlen(s);
    if (bufPos + len + 1 > sizeof(buf))
	errAbort("Command too long");
    memcpy(buf + bufPos, s, len);
    bufPos += len;
    if (i != argc-1)
	buf[bufPos++] = ' ';
    }

/* Issue command. */
if (write(sd, buf, bufPos) < 0)
    {
    perror("");
    errAbort("Couldn't write to %s", hostName);
    }

/* Perhaps read back results. */
if (sameString(command, "status"))
    {
    bufPos = read(sd, buf, sizeof(buf));
    buf[bufPos] = 0;
    printf("Status %s\n", buf);
    }
close(sd);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
netC(argv[1], argc-2, argv+2);
}


