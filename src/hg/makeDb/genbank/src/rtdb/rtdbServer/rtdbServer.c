/* rtdbServer - set up a server 
 * respond to search requests. */

#include "common.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "portable.h"
#include "net.h"
#include "dystring.h"
#include "errabort.h"
#include "memalloc.h"
#include "options.h"
#include "log.h"

static struct optionSpec optionSpecs[] = {
    {"canStop", OPTION_BOOLEAN},
    {"log", OPTION_STRING},
    {"logFacility", OPTION_STRING},
    {"syslog", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean canStop = FALSE;
int warnCount = 0;
struct sockaddr_in sai;		/* Some system socket info. */
volatile boolean pipeBroke = FALSE;	/* Flag broken pipes here. */

void usage()
/* Explain usage and exit. */
{
errAbort("rtdbServer - Server to run an update to the RTDB.\n"
	 "Options:\n"
	 "   -log=logFile keep a log file that records server requests.\n"
	 "   -seqLog    Include sequences in log file (not logged with -syslog)\n"
	 "   -syslog    Log to syslog\n"
	 "   -logFacility=facility log to the specified syslog facility - default local0.\n"
	 "   -canStop If set then a quit message will actually take down the\n"
	 "            server");
}

static void rtdbPipeHandler(int sigNum)
/* Set broken pipe flag. */
{
pipeBroke = TRUE;
}

void rtdbCatchPipes()
/* Set up to catch broken pipe signals. */
{
signal(SIGPIPE, rtdbPipeHandler);
}

void update()
/* Run the update. First fork off a child and deal with it there. */
{
logInfo("Beginning update in child");
sleep(20);
logInfo("Ended update in child");
}

void startServer(char *hostName, char *portName)
/* Load up index and hang out in RAM. */
{
char buf[256];
char *line, *command;
int fromLen, readSize;
int socketHandle = 0, connectionHandle = 0;
int port = atoi(portName);
int childPipe[2];
fd_set filedes;
boolean updating = FALSE;
int rc; /* return codes */
int pid = 1;

FD_ZERO(&filedes);

netBlockBrokenPipes();

rc = pipe(childPipe);
logInfo("rtdbServer on host %s, port %s", hostName, portName);

/* Set up socket.  Get ready to listen to it. */
socketHandle = netAcceptingSocket(port, 5);

logInfo("Server ready for queries!");
printf("Server ready for queries!\n");
for (;;)
    {
    int maxfd;
    /* Add read end of pipe to file descriptor set. */
    FD_SET(childPipe[0], &filedes);
    /* Add socket to file descriptor set. */
    FD_SET(socketHandle, &filedes);
    maxfd = max(childPipe[0], socketHandle) + 1;
    select(maxfd, &filedes, NULL, NULL, NULL);
    if (FD_ISSET(socketHandle, &filedes))
	{
	connectionHandle = accept(socketHandle, NULL, &fromLen);
	if (connectionHandle < 0)
	    {
	    warn("Error accepting the connection");
	    ++warnCount;
	    continue;
	    }
	readSize = read(connectionHandle, buf, sizeof(buf)-1);
	if (readSize < 0)
	    {
	    warn("Error reading from socket: %s", strerror(errno));
	    ++warnCount;
	    close(connectionHandle);
	    continue;
	    }
	if (readSize == 0)
	    {
	    warn("Zero sized query");
	    ++warnCount;
	    close(connectionHandle);
	    continue;
	    }
	buf[readSize] = 0;
	logDebug("%s", buf);
	line = buf;
	command = nextWord(&line);
	if (sameString("quit", command))
	    {
	    if (canStop)
		break;
	    else
		logError("Ignoring quit message");
	    }
	else if (sameString("status", command))
	    {
	    netSendString(connectionHandle, "RTDB Server");
	    safef(buf, 256, "host %s", hostName);
	    netSendString(connectionHandle, buf);
	    safef(buf, 256, "port %s", portName);
	    netSendString(connectionHandle, buf);
	    safef(buf, 256, "warnings %d", warnCount);
	    netSendString(connectionHandle, buf);
	    netSendString(connectionHandle, "end");
	    }
	else if (sameString("update", command))
	    {
	    if (updating)
		{
		warn("Attempted to update while updating.");
		++warnCount;
		}
	    else
		{
		pid = fork();
		if (pid < 0)
		    errAbort("Couldn't fork()");
		else if (pid == 0)
		    {
		    /* We're in the child. */
		    /* Close the read end of the pipe. */
		    close(childPipe[0]);
		    update();
		    exit(0);
		    }
		else
		    {
		    updating = TRUE;
		    close(childPipe[1]);
		    }
		}
	    }
	else
	    {
	    warn("Unknown command %s", command);
	    ++warnCount;
	    }
	close(connectionHandle);
	connectionHandle = 0;
	}
    if (FD_ISSET(childPipe[0], &filedes))
	{
	logInfo("Completed update, received EOF from child pipe, creating new pipe.");
	close(childPipe[0]);
	pipe(childPipe);
	updating = FALSE;
	}
    }
close(socketHandle);
}

void stopServer(char *hostName, char *portName)
/* Send stop message to server. */
{
char buf[256];
int sd = 0;

sd = netMustConnectTo(hostName, portName);
safef(buf, 256, "quit");
write(sd, buf, strlen(buf));
close(sd);
printf("sent stop message to server\n");
}

int statusServer(char *hostName, char *portName)
/* Send status message to server arnd report result. */
{
char buf[256];
int sd = 0;
int ret = 0;

/* Put together command. */
sd = netMustConnectTo(hostName, portName);
safef(buf, 256, "status");
write(sd, buf, strlen(buf));

for (;;)
    {
    if (netGetString(sd, buf) == NULL)
	{
	warn("Error reading status information from %s:%s",hostName,portName);
	ret = -1;
        break;
	}
    if (sameString(buf, "end"))
        break;
    else
        printf("%s\n", buf);
    }
close(sd);
return(ret); 
}

void updateServer(char *hostName, char *portName)
/* Send stop message to server. */
{
char buf[256];
int sd = 0;
sd = netMustConnectTo(hostName, portName);
safef(buf, 256, "update");
write(sd, buf, strlen(buf));
close(sd);
printf("sent update message to server\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
rtdbCatchPipes();
optionInit(&argc, argv, optionSpecs);
command = argv[1];
canStop = optionExists("canStop");
if (argc < 2)
    usage();
if (optionExists("log"))
    logOpenFile(argv[0], optionVal("log", NULL));
if (optionExists("syslog"))
    logOpenSyslog(argv[0], optionVal("logFacility", NULL));
else if (sameWord(command, "start"))
    {
    if (argc != 4)
        usage();
    startServer(argv[2], argv[3]);
    }
else if (sameWord(command, "stop"))
    {
    if (argc != 4)
	usage();
    stopServer(argv[2], argv[3]);
    }
else if (sameWord(command, "update"))
    {
    if (argc != 4)
	usage();
    updateServer(argv[2], argv[3]);
    }
else if (sameWord(command, "status"))
    {
    if (argc != 4)
	usage();
    if (statusServer(argv[2], argv[3]))
	{
	exit(-1);
	}
    }
else
    {
    usage();
    }
return 0;
}
