/* bottleneck - A server that helps slow down hyperactive web robots. */
#include "common.h"
#include <sys/wait.h>
#include "localmem.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "internet.h"
#include "net.h"

static char const rcsid[] = "$Id: bottleneck.c,v 1.2 2004/01/31 21:22:50 kent Exp $";

int port = 17776;	/* Default bottleneck port. */
char *host = "localhost";   /* Default host. */
int penalty = 150;	    /* Penalty in milliseconds per access. */
int recovery = 10;	    /* Recovery in milliseconds per second. */
char *subnet = NULL;        /* Subnet as dotted quads. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bottleneck - A server that helps slow down hyperactive web robots\n"
  "usage:\n"
  "   bottleneck start\n"
  "Start up bottleneck server\n"
  "   bottleneck query ip-address [count]\n"
  "Ask bottleneck server how long to wait to service ip-address\n"
  "   bottleneck list\n"
  "List accessing sites\n"
  "options:\n"
  "   -port=XXXX - Use specific tcp/ip port. Default %d.\n"
  "   -host=XXXXX - Use specific host.  Default %s.\n"
  "   -subnet=WWW.XXX.YYY.ZZZ Restrict access to subnet (example 192.168.255.255)\n"
  "   -penalty=N - Penalty (in milliseconds) for each access, default %d\n"
  "   -recovery=N - Amount to recover (in milliseconds) for each second\n"
  "                 between accesses.  Default %d\n"
  "Note penalty and recovery if moved from defaults should balance\n"
  "At the default settings an equilibrium will be achieved when queries\n"
  "are spaced 15 seconds apart.  The maximum delay should thus be 15 seconds\n"
  "It will take 25 minutes of idleness for the maximum delay to decay back to\n"
  "zero.\n"
  , port, host, penalty, recovery
  );
}

static struct optionSpec options[] = {
   {"post", OPTION_INT},
   {"host", OPTION_STRING},
   {"subnet", OPTION_STRING},
   {"penalty", OPTION_INT},
   {"recovery", OPTION_INT},
   {NULL, 0},
};

struct tracker
/* Keep track of IP addresses. */
    {
    struct tracker *next;	/* Next in list. */
    char *name;	/* Name (IP address).  Not allocated here. */
    time_t lastAccess;		/* Time of last access. */
    int curDelay;		/* Current delay in 1000ths of second. */
    int accessCount;		/* Total number of accesses since started tracking. */
    };

struct hash *trackerHash = NULL;	/* Hash of all trackers. */
struct tracker *trackerList;		/* List of all trackers. */

int calcDelay(struct tracker *tracker, time_t now)
/* Figure out recommended delay. */
{
int timeDiff = now - tracker->lastAccess;
int delay;
if (timeDiff < 0)
    timeDiff = 0;
delay = tracker->curDelay + penalty - timeDiff*recovery;
if (delay < 0)
    delay = 0;
return delay;
}

void returnList(int socket)
/* Send list of all things tracked down socket. */
{
struct tracker *tracker;
char buf[256];
time_t now = time(NULL);
int timeDiff;
for (tracker = trackerList; tracker != NULL; tracker = tracker->next)
    {
    timeDiff = now - tracker->lastAccess;
    safef(buf, sizeof(buf), "%s %d %d %d", 
    	tracker->name, tracker->accessCount, timeDiff, calcDelay(tracker, now));
    if (!netSendString(socket, buf))
        break;
    }
netSendString(socket, "");
}

void clearZombies()
/* Clear any zombie processes */
{
int stat;
for (;;)
    {
    if (waitpid(-1, &stat, WNOHANG) <= 0)
	break;
    }
}

void forkOutList(int socket)
/* Fork instance of self to send list of all things tracked
 * down socket. */
{
if (fork() == 0)
    {
    returnList(socket);
    exit(0);
    }
else
    {
    clearZombies();
    }
}

void startServer()
/* Start up bottleneck server. */
{
int acceptor;
unsigned char parsedSubnetBuf[4];	/* Buffer for holding parsed subnet. */
unsigned char *parsedSubnet = NULL;	/* Parsed subnet. */
if (subnet != NULL)
    {
    internetParseDottedQuad(subnet, parsedSubnetBuf);
    parsedSubnet = parsedSubnetBuf;
    }
trackerHash = newHash(18);
acceptor = netAcceptingSocket(port, 64);
for (;;)
    {
    int socket = netAcceptFrom(acceptor, parsedSubnet);
    char buf[256], *s;
    s = netGetString(socket, buf);
    if (s != NULL)
	{
	if (s[0] == '?')
	    forkOutList(socket);
	else
	    {
	    time_t now = time(NULL);
	    struct tracker *tracker = hashFindVal(trackerHash, s);
	    if (tracker == NULL)
		{
		lmAllocVar(trackerHash->lm, tracker);
		hashAddSaveName(trackerHash, s, tracker, &tracker->name);
		tracker->lastAccess = now;
		tracker->curDelay = -penalty;
		slAddHead(&trackerList, tracker);
		}
	    tracker->accessCount += 1;
	    tracker->curDelay = calcDelay(tracker, now);
	    tracker->lastAccess = now;
	    safef(buf, sizeof(buf), "%d", tracker->curDelay);
	    netSendString(socket, buf);
	    }
	}
    close(socket);
    }
}

void forkOffServer()
/* Close standard file handles and fork to be a better daemon. */
{
/* Close standard file handles. */
close(0);
close(1);
close(2);

if (fork() == 0)
    {
    /* Execute daemon. */
    startServer();
    }
}

void queryServer(char *ip, int count)
/* Query bottleneck server - just for testing.
 * Main query is over ip port. */
{
int i;
for (i=0; i<count; ++i)
    {
    int socket = netMustConnect(host, port);
    char buf[256], *s;
    netSendString(socket, ip);
    s = netGetString(socket, buf);
    if (s == NULL)
        errAbort("Shut out by bottleneck server %s:%d", host, port);
    printf("%s millisecond delay recommended\n", s);
    close(socket);
    }
}

void listAll()
/* Ask server for info on all IP addresses. */
{
int socket = netMustConnect(host, port);
char buf[256], *s;
netSendString(socket, "?");
for (;;)
    {
    s = netGetString(socket, buf);
    if (s == NULL || s[0] == 0)
        break;
    printf("%s\n", buf);
    }
close(socket);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
port = optionInt("port", port);
host = optionVal("host", host);
penalty = optionInt("penalty", penalty);
recovery = optionInt("recovery", recovery);
subnet = optionVal("subnet", subnet);
command = argv[1];
if (sameString(command, "start"))
    {
    if (argc != 2)
        usage();
    forkOffServer();
    }
else if (sameString(command, "query"))
    {
    int count = 1;
    if (argc > 3)
	count = atoi(argv[3]);
    queryServer(argv[2], count);
    }
else if (sameString(command, "list"))
    {
    listAll();
    }
else
    {
    usage();
    }
return 0;
}
