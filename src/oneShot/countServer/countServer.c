/* countServer - A server that just returns a steadily increasing stream of numbers. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "countServer - A server that just returns a steadily increasing stream of numbers\n"
  "usage:\n"
  "   countServer port\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void countServer(char *portName)
/* countServer - A server that just returns a steadily increasing stream of numbers. */
{
int port = atoi(portName);
int ear, socket, size;
char buf[1024];
int count = 0;

ear = netAcceptingSocket(port, 1000);
if (ear < 0)
    errnoAbort("Couldn't open ear on %d", port);
socket = netAccept(ear);
if (socket < 0)
    errnoAbort("Couldn't accept first");
size = netReadAll(socket, buf, sizeof(buf));
close(socket);
if (size < 0)
    errnoAbort("Couldn't read first");
buf[size] = 0;
printf("Got first connection '%s'\n", buf);
++count;
sleep(10);
for (;;)
    {
    socket = netAccept(ear);
    if (socket < 0)
	errnoAbort("Couldn't accept first");
    size = netReadAll(socket, buf, sizeof(buf));
    close(socket);
    if (size < 0)
	errnoAbort("Couldn't read first");
    if (startsWith("quit", buf))
        break;
    ++count;
    }
printf("All done after %d\n", count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
countServer(argv[1]);
return 0;
}
