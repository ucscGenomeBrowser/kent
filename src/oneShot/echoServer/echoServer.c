/* echoServer - A little echo server to compare C vs. Python implementation. */
#include "common.h"
#include "net.h"
#include "pthreadWrap.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "echoServer - A little echo server to compare C vs. Python implementation\n"
  "usage:\n"
  "   echoServer port\n"
  );
}

char *now()
/* Return current time as a string. */
{
time_t t = time(NULL);
return ctime(&t);
}

static void *echoThread(void *vptr)
/* Write out what we got, with 'Echo=>' prepended */
{
int *pConn = vptr;
int conn = *pConn;
sleep(5);
for (;;)
    {
    static char buf1[1024], buf2[1024+6];
    int size = read(conn, buf1, sizeof(buf1)-1);
    if (size <= 0) break;
    buf1[size] = 0;
    safef(buf2, sizeof(buf2), "Echo=>%s", buf1);
    write(conn, buf2, strlen(buf2));
    }
close(conn);
return NULL;
}

void echoServer(char *port)
/* echoServer - A little echo server to compare C vs. Python implementation. */
{
int portNum = atoi(port);
int sock = netAcceptingSocket(portNum, 5);
for (;;)
   {
   int conn = netAccept(sock);
   pthread_t thread;
   printf("Server connected at %s", now());
   pthreadCreate(&thread, NULL, echoThread, &conn);
   }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
echoServer(argv[1]);
return 0;
}
