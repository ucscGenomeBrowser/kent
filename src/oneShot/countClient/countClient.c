/* countClient - Connect repeatedly to the count server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "countClient - Connect repeatedly to the count server\n"
  "usage:\n"
  "   countClient host port count\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void countClient(char *host, char *portName, char *countString)
/* countClient - Connect repeatedly to the count server. */
{
int i, count = atoi(countString);
int socket;
char buf[16];

for (i=0; i<count; ++i)
    {
    socket = netMustConnectTo(host, portName);
    snprintf(buf, sizeof(buf), "%d", i);
    write(socket, buf, strlen(buf));
    close(socket);
    }
socket = netMustConnectTo(host, portName);
strcpy(buf, "quit");
write(socket, buf, strlen(buf));
close(socket);
printf("Bye");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
countClient(argv[1], argv[2], argv[3]);
return 0;
}
