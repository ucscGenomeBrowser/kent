/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. */
#include "common.h"
#include "internet.h"
#include <sys/time.h>
#include <pthread.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"

int burstPort = 12354;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly\n"
  "usage:\n"
  "   burstServer start\n"
  "options:\n"
  "   -xxx=XXX\n"
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
    char payload[16];
    };

struct countMessage messages[10000];   /* Array of messages. */
int messageCount;	/* The number of messages we've received. */
struct timeval startTime;


#ifdef OLD
void burstServer(char *command)
/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. */
{
int port = burstPort;
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
    printf("#%d recieved at %f seconds. Payload '%s'\n", 
    	receiveMessage.count, 0.000001 * timeDiff(&startTime, &tv), receiveMessage.payload);
    gettimeofday(&tv, NULL);
    if (!receiveMessage.message)
         break;
    }
close(ear);
printf("All done after %d\n", count);
}
#endif /* OLD */

struct socketSuckerInput
/* Input structure for socket sucker. */
     {
     int socket;
     };

void *suckSocket(void *vptr)
/* Suck down stuff from socket and add it to message list. */
{
struct socketSuckerInput *ssi = vptr;
int err;
int i;
struct sockaddr_in sai;

ZeroVar(&sai);
sai.sin_family = AF_INET;
for (i=0; i<ArraySize(messages); ++i)
    {
    int saiSize = sizeof(sai);
    messages[i];
    err = recvfrom(ssi->socket, &messages[i], sizeof(messages[i]), 
    	0, (struct sockaddr *)&sai, &saiSize);
    if (err < 0)
	{
        warn("couldn't receive %s", strerror(errno));
	continue;
	}
    if (err != sizeof(messages[i]))
        {
	warn("Message truncated");
	continue;
	}
    ++messageCount;
    }
return NULL;
}

void burstServer(char *command)
/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. 
 * This routine sets up the high priority thread to grab stuff off of the port, and
 * then reads through the saved message list at it's leisure. */
{
pthread_t socketSucker;
int err;
struct sockaddr_in sai;
struct socketSuckerInput *ssi;
int outIx = 0;

/* Initialize socket etc. */
AllocVar(ssi);
ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(burstPort);
sai.sin_addr.s_addr = INADDR_ANY;
ssi->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (bind(ssi->socket, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind ear");
gettimeofday(&startTime, NULL);

/* Start thread to suck stuff out of socket. */
err = pthread_create(&socketSucker, NULL, suckSocket, ssi);

/* Go into loop that prints things out 50 per second max. */
for (;;)
    {
    sleep1000(20);
    if (messageCount > outIx)
        {
	printf("#%d sent at %f seconds. Payload '%s'\n", 
	    messages[outIx].count, 0.000001 * messages[outIx].time, messages[outIx].payload);
	++outIx;
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
burstServer(argv[1]);
return 0;
}
