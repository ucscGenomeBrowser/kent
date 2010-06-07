/* countServer - A server that just returns a steadily increasing stream of numbers. */
#include "common.h"
#include "internet.h"
#include <sys/time.h>
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
  "   -tcp\n"
  "   -udp\n"
  );
}

bits32 timeDiff(struct timeval *past, struct timeval *now)
/* Return different between now and past. */
{
bits32 diff = now->tv_sec - past->tv_sec;
diff *= 1000000;
diff += now->tv_usec - past->tv_usec;
return diff;
}

struct countMessage
/* What to transmit. */
    {
    bits32 time;
    bits32 echoTime;
    bits32 count;
    bits32 message;
    // char payload[512-16];
    };

void tcpCountServer(char *portName)
/* tcpCountServer - A server that just returns a steadily increasing stream of numbers. 
 * This one is based on tcp */
{
int port = atoi(portName);
int ear, socket, size;
char buf[1024];
int count = 0;
struct timeval startTime, tv;
struct countMessage sendMessage, receiveMessage;

ear = netAcceptingSocket(port, 100);
gettimeofday(&startTime, NULL);
for (;;)
    {
    socket = netAccept(ear);
    if (socket < 0)
	errnoAbort("Couldn't accept first");
    size = netReadAll(socket, &receiveMessage, sizeof(receiveMessage));
    if (size < sizeof(receiveMessage))
        continue;
    gettimeofday(&tv, NULL);
    sendMessage.time = timeDiff(&startTime, &tv);
    sendMessage.echoTime = receiveMessage.time;
    sendMessage.count = ++count;
    sendMessage.message = receiveMessage.message + 256;
    write(socket, &sendMessage, sizeof(sendMessage));
    close(socket);
    if (size < 0)
	errnoAbort("Couldn't read first");
    if (!receiveMessage.message)
         break;
    }
printf("All done after %d\n", count);
}

void udpCountServer(char *portName)
/* countServer - A server that just returns a steadily increasing stream of numbers. */
{
int port = atoi(portName);
int ear, size;
char buf[1024];
int count = 0;
struct timeval startTime, tv;
struct countMessage sendMessage, receiveMessage;
struct sockaddr_in sai;


ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = INADDR_ANY;
ear = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (bind(ear, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind ear");
gettimeofday(&startTime, NULL);
for (;;)
    {
    int err;
    int saiSize = sizeof(sai);
    ZeroVar(&sai);
    sai.sin_family = AF_INET;
    err = recvfrom(ear, &receiveMessage, sizeof(receiveMessage), 
    	0, (struct sockaddr *)&sai, &saiSize);
    if (err < 0)
	{
        warn("couldn't receive %s", strerror(errno));
	continue;
	}
    if (err != sizeof(receiveMessage))
        {
	warn("Message truncated");
	continue;
	}
    gettimeofday(&tv, NULL);
    sendMessage.time = timeDiff(&startTime, &tv);
    sendMessage.echoTime = receiveMessage.time;
    sendMessage.count = ++count;
    sendMessage.message = receiveMessage.message + 256;
    sendto(ear, &sendMessage, sizeof(sendMessage), 0, &sai, sizeof(sai));
    if (!receiveMessage.message)
         break;
    }
close(ear);
printf("All done after %d\n", count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
if (optionExists("tcp"))
    tcpCountServer(argv[1]);
else
    udpCountServer(argv[1]);
return 0;
}
