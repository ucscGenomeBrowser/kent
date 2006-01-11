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

void rtdbWarning(char *format, ...)
/* Simplify the warning calls that also update counter. */
{
va_list args;
va_start(args, format);
warn(format, args);
va_end(args);
++warnCount;
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
/* This part is still pretty weak. */
/* It'll be a system() mostly, writing the information to stdout, */
/* which should then be picked up by the read end of the pipe in */
/* the parent process. */
sleep(20);
logInfo("Ended update in child");
}

int getSocket(char *hostName, int port)
/* Run a few initializing steps to get the server going. */
{
int socketHandle = 0;
netBlockBrokenPipes();
logInfo("rtdbServer on host %s, port %d", hostName, port);
/* Set up socket.  Get ready to listen to it. */
socketHandle = netAcceptingSocketFrom(port, 5, "localhost");
if (socketHandle == -1)
    {
    logError("Couldn't get the socket.");
    errAbort("Couldn't get the socket.");
    }
logInfo("Server ready for queries!");
printf("Server ready for queries!\n");
return socketHandle;
}

int getConnection(int socketHandle)
/* Get an accept() from the socket and return the connection */
/* handler. */
{
int fromLen;
int connectionHandle;
connectionHandle = accept(socketHandle, NULL, &fromLen);
if (connectionHandle < 0)
    rtdbWarning("Error accepting the connection");
return connectionHandle;
}

char *getCommandFromSocket(int connectionHandle)
/* Just return the command from the client or NULL if unrecognized. */
/* Free this later. */
{
char buf[256], *line;
int readSize = read(connectionHandle, buf, sizeof(buf)-1);
if (readSize < 0)
    {
    rtdbWarning("Error reading from socket: %s", strerror(errno));
    close(connectionHandle);
    return NULL;
    }
if (readSize == 0)
    {
    rtdbWarning("Zero sized query");
    close(connectionHandle);
    return NULL;
    }
buf[readSize] = 0;
logDebug("%s", buf);
line = buf;
return cloneString(nextWord(&line));
}

void doStatus(int connectionHandle, char *hostName, int port)
/* Return some status information to the client. */
{
char buf[256];
netSendString(connectionHandle, "RTDB Server");
safef(buf, 256, "host %s", hostName);
netSendString(connectionHandle, buf);
safef(buf, 256, "port %d", port);
netSendString(connectionHandle, buf);
safef(buf, 256, "warnings %d", warnCount);
netSendString(connectionHandle, buf);
netSendString(connectionHandle, "end");
}

boolean canQuit(int childPipe)
/* Quit if possible and log things. */
{
if (childPipe == -1)
    rtdbWarning("Attempted to quit server while updating");
else if (canStop == FALSE)
    rtdbWarning("Attempted to quit while canStop=FALSE.  Ignoring.");
else 
    return TRUE;
return FALSE;
}

int doUpdate(int connectionHandle)
/* fork off a child to run update. */
{
int pid, childPipe[2];
if (pipe(childPipe) != 0)
    errAbort("There was a problem creating a pipe.");
pid = fork();
if (pid < 0)
    errAbort("Couldn't fork()!");
if (pid  == 0)
    {
    close(childPipe[0]);
    update();
    exit(0);
    }
close(childPipe[1]);
return childPipe[0];
}

void startServer(char *hostName, char *portName)
/* Load up index and hang out in RAM. */
{
int socketHandle = 0;
int port = atoi(portName);
int childPipe = -1;
socketHandle = getSocket(hostName, port);
for (;;)
    {
    fd_set filedes;
    int maxfd;
    FD_ZERO(&filedes);
    FD_SET(socketHandle, &filedes);
    if (childPipe > -1)
	FD_SET(childPipe, &filedes);
    maxfd = max(childPipe, socketHandle) + 1;    
    select(maxfd, &filedes, NULL, NULL, NULL);
    if (FD_ISSET(socketHandle, &filedes))
	{
	int connectionHandle = getConnection(socketHandle);
	char *command = getCommandFromSocket(connectionHandle);
	if (command == NULL)
	    continue;
	if (sameString(command, "quit") && canQuit(childPipe))
	    break;
	else if (sameString(command, "status"))
	    doStatus(connectionHandle, hostName, port);
	else if (sameString(command, "update"))
	    {
	    if (childPipe != -1)
		rtdbWarning("Attempted to update while updating.");
	    else 
		childPipe = doUpdate(connectionHandle);
	    }
	else
	    rtdbWarning("Unknown command %s", command);
	freeMem(command);
	close(connectionHandle);
	}
    /* Handles pipe's EOF signal when child process ends. */
    if ((childPipe != -1) && FD_ISSET(childPipe, &filedes))
	{
	logInfo("Completed update, received EOF from child pipe, reset pipe.");
	childPipe = -1;
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
	rtdbWarning("Error reading status information from %s:%s",hostName,portName);
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
