/* simpleS - simple server. */
#include "paraCommon.h"

void usage()
/* Explain usage and exit. */
{
errAbort("simpleS - simple server.\n"
         "usage:\n"
	 "    s socket\n");
}

void simpleS(char *sockName)
/* simpleS - a simple server. */
{
int sd, ns;
char buf[256];
static struct sockaddr sockaddr;
int fromlen;
int readSize;
int childCount = 0;

sd = socket(AF_UNIX, SOCK_STREAM, 0);
sockaddr.sa_family = AF_UNIX;
strcpy(sockaddr.sa_data, sockName);
bind(sd, &sockaddr, sizeof(sockaddr));
listen(sd, 1);

while (childCount < 4)
    {
    ns = accept(sd, &sockaddr, &fromlen);
    ++childCount;
#ifdef SOON
    if (fork() == 0)
	{
	/* child. */
	close(sd);
	readSize = read(ns, buf, sizeof(buf)-1);
	buf[readSize] = 0;
	printf("server read '%s'\n", buf);
	exit(0);
	}
#else
    readSize = read(ns, buf, sizeof(buf)-1);
    buf[readSize] = 0;
    printf("server read '%s'\n", buf);
#endif
    close(ns);
    }
remove(sockName);
uglyf("SimpleS all done\n");
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
simpleS(argv[1]);
}


