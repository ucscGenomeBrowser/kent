/* burstClient - A udp client that sends a burst of data to the server. */
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "internet.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"


int burstPort = 12354;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "burstClient - A udp client that sends a burst of data to the server\n"
  "usage:\n"
  "   burstClient host count\n"
  "If count is 0 then server will be sent a quit message\n"
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


void burstClient(char *host, char *countString)
/* burstClient - A udp client that sends a burst of data to the server. */
{
int i, size, count = atoi(countString);
int port = burstPort;
int sd, err;
struct countMessage sendMessage, receiveMessage;
struct timeval startTime, tv;
struct sockaddr_in outAddress, inAddress;

uglyf("udpCountClient %s %d %s\n", host, port, countString);
if (!internetFillInAddress(host, port, &outAddress))
    errAbort("sorry, bye");
sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
gettimeofday(&startTime, NULL);
strcpy(sendMessage.payload, "G-day matey ho!");
for (i=0; i<count; ++i)
    {
    int inAddressSize = sizeof(inAddressSize);
    gettimeofday(&tv, NULL);
    sendMessage.time = timeDiff(&startTime, &tv);
    sendMessage.echoTime = 0;
    sendMessage.count = i;
    sendMessage.message = 1;
    err = sendto(sd, &sendMessage, sizeof(sendMessage), 0, &outAddress, 
    	sizeof(outAddress));
    if (err < 0)
        errnoAbort("Couldn't sendto");
    sleep1000(1);
    }
if (count == 0)
    {
    printf("Closing server\n");
    gettimeofday(&tv, NULL);
    sendMessage.time = timeDiff(&startTime, &tv);
    sendMessage.echoTime = 0;
    sendMessage.count = i;
    sendMessage.message = 0;
    sendto(sd, &sendMessage, sizeof(sendMessage), 0, &outAddress, 
	sizeof(outAddress));
    }
close(sd);
printf("Bye");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
burstClient(argv[1], argv[2]);
return 0;
}
