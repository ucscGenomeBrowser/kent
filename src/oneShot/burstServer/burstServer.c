/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. */
#include "common.h"
#include "internet.h"
#include <sys/time.h>
#include <pthread.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "rudp.h"
#include "portable.h"

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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

struct timeval startTime;



void mutexLock(pthread_mutex_t *mutex)
/* Lock a mutex or die trying. */
{
int err = pthread_mutex_lock(mutex);
if (err != 0)
    errAbort("Couldn't lock mutex: %s", strerror(err));
}

void mutexUnlock(pthread_mutex_t *mutex)
/* Lock a mutex or die trying. */
{
int err = pthread_mutex_unlock(mutex);
if (err != 0)
    errAbort("Couldn't lock mutex: %s", strerror(err));
}

void condSignal(pthread_cond_t *cond)
/* Set conditional signal */
{
int err = pthread_cond_signal(cond);
if (err != 0)
    errAbort("Couldn't condSignal: %s", strerror(err));
}

void condWait(pthread_cond_t *cond, pthread_mutex_t *mutex)
/* Wait for conditional signal. */
{
int err = pthread_cond_wait(cond, mutex);
if (err != 0)
    errAbort("Couldn't contWait: %s", strerror(err));
}

void *suckSocket(void *vptr)
/* Suck down stuff from socket and add it to message list. */
{
struct rudp *ru = vptr;
int err;
int i;
struct sockaddr_in sai;

ZeroVar(&sai);
sai.sin_family = AF_INET;
for (i=0; i<ArraySize(messages); ++i)
    {
    int saiSize = sizeof(sai);
    messages[i];
    err = rudpReceive(ru, &messages[i], sizeof(messages[i]));
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
    mutexLock(&mutex);
    ++messageCount;
    condSignal(&ready);
    mutexUnlock(&mutex);
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
struct rudp *ru;
int ear;
int outIx = 0;

/* Initialize socket etc. */
ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(burstPort);
sai.sin_addr.s_addr = INADDR_ANY;
ear = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (bind(ear, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind ear");
ru = rudpNew(ear);
gettimeofday(&startTime, NULL);

/* Start thread to suck stuff out of socket. */
err = pthread_create(&socketSucker, NULL, suckSocket, ru);

/* Go into loop that prints things out 50 per second max. */
for (;;)
    {
    int inIx;
    mutexLock(&mutex);
    while (messageCount <= outIx)
        condWait(&ready, &mutex);
    inIx = messageCount;
    mutexUnlock(&mutex);
    if (inIx > outIx)
        {
	printf("#%d sent at %f seconds. Payload '%s'\n", 
	    messages[outIx].count, 0.000001 * messages[outIx].time, messages[outIx].payload);
	if (messages[outIx].message == 0)
	    break;
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
