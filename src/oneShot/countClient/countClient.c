/* countClient - Connect repeatedly to the count server. */
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "internet.h"
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


void tcpCountClient(char *host, char *portName, char *countString)
/* countClient - Connect repeatedly to the count server. */
{
int i, size, count = atoi(countString);
int socket;
struct countMessage sendMessage, receiveMessage;
struct timeval startTime, tv;
struct sockaddr_in outAddress, inAddress;

gettimeofday(&startTime, NULL);
for (i=0; i<count; ++i)
    {
    gettimeofday(&tv, NULL);
    socket = netMustConnectTo(host, portName);
    sendMessage.time = timeDiff(&startTime, &tv);
    sendMessage.echoTime = 0;
    sendMessage.count = i;
    sendMessage.message = 1;
    write(socket, &sendMessage, sizeof(sendMessage));
    size = netReadAll(socket, &receiveMessage, sizeof(receiveMessage));
    close(socket);
    if (size == sizeof(receiveMessage))
	{
	bits32 t, roundTripTime;
	gettimeofday(&tv, NULL);
	t = timeDiff(&startTime, &tv);
	roundTripTime = t - receiveMessage.echoTime;
	printf("roundTripTime %u, i %d, count %d\n", roundTripTime, i, receiveMessage.count);
	}
    }
gettimeofday(&tv, NULL);
sendMessage.time = timeDiff(&startTime, &tv);
sendMessage.echoTime = 0;
sendMessage.count = i;
sendMessage.message = 0;
socket = netMustConnectTo(host, portName);
write(socket, &sendMessage, sizeof(sendMessage));
close(socket);
printf("Bye");
}

void udpCountClient(char *host, char *portName, char *countString)
/* countClient - Connect repeatedly to the count server. */
{
int i, size, count = atoi(countString);
int port = atoi(portName);
int sd, err;
struct countMessage sendMessage, receiveMessage;
struct timeval startTime, tv;
struct sockaddr_in outAddress, inAddress;

uglyf("udpCountClient %s %s %s\n", host, portName, countString);
if (!internetFillInAddress(host, port, &outAddress))
    errAbort("sorry, bye");
sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
gettimeofday(&startTime, NULL);
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
    ZeroVar(&inAddress);
    inAddress.sin_family = AF_INET;
    size = recvfrom(sd, &receiveMessage, sizeof(receiveMessage), 
    	0, (struct sockaddr *)&inAddress, &inAddressSize);
    if (size == sizeof(receiveMessage))
	{
	bits32 t, roundTripTime;
	gettimeofday(&tv, NULL);
	t = timeDiff(&startTime, &tv);
	roundTripTime = t - receiveMessage.echoTime;
	printf("roundTripTime %u, i %d, count %d\n", roundTripTime, i, receiveMessage.count);
	}
    }
gettimeofday(&tv, NULL);
sendMessage.time = timeDiff(&startTime, &tv);
sendMessage.echoTime = 0;
sendMessage.count = i;
sendMessage.message = 0;
sendto(sd, &sendMessage, sizeof(sendMessage), 0, &outAddress, 
    sizeof(outAddress));
close(sd);
printf("Bye");
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
if (optionExists("tcp"))
    tcpCountClient(argv[1], argv[2], argv[3]);
else
    udpCountClient(argv[1], argv[2], argv[3]);
return 0;
}
