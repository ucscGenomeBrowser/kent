/* simpleC - simple client. */
#include "paraCommon.h"

void usage()
/* Explain usage and exit. */
{
errAbort("simpleC - simple client.\n"
         "usage:\n"
	 "    simpleC socket\n");
}

void simpleC(char *sockName)
/* simpleC - a simple client. */
{
int sd;
static struct sockaddr sockaddr;
int fromlen;
int childCount = 0;
char *s = "Hello Mr. Socket";

sd = socket(AF_UNIX, SOCK_STREAM, 0);
sockaddr.sa_family = AF_UNIX;
strcpy(sockaddr.sa_data, sockName);
if (connect(sd, &sockaddr, sizeof(sockaddr)) == -1)
    {
    errAbort("Couldn't connect to %s", sockName);
    }
if (write(sd, s, strlen(s)) < 0)
    {
    perror("");
    errAbort("Couldn't write to socket %s", sockName);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
simpleC(argv[1]);
}


