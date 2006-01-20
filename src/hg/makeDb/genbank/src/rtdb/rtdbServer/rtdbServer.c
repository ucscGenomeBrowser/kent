/* rtdbServer - server udpates RTDB when it gets update requests. */

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
#include "linefile.h"

/* Globals */

static struct optionSpec optionSpecs[] = {
    {"log", OPTION_STRING},
    {"logFacility", OPTION_STRING},
    {"syslog", OPTION_BOOLEAN},
    {"noVerbose", OPTION_BOOLEAN},
    {"noForce", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean force = TRUE;
boolean verBose = TRUE;
int warnCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort("rtdbServer - Server to run an update to the RTDB.\n"
	 "Options:\n"
	 "   -log=logFile keep a log file that records server requests.\n"
	 "   -syslog    Log to syslog\n"
	 "   -logFacility=facility log to the specified syslog facility - default local0.\n"
	 "   -noVerbose   Run ./bin/rtdbUpdate script without the -verbose option.\n"
	 "                (not recommened)\n"
	 "   -noForce     Run ./bin/rtdbUpdate script without the -force option.\n"
	 "                (not recommened)\n"
	 "*NOTE* Start server in parent dir of where the data/logs go\n"
	 "       e.g. /cluster/data/genbank");
}

/*** Various functions for making things easier. ***/

void rtdbWarning(char *format, ...)
/* Simplify the warning calls that also update counter. */
{
va_list args;
va_start(args, format);
warn(format, args);
va_end(args);
++warnCount;
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

/*** Update/child process related functions ***/

void update(char *database, int connectionHandle)
/* Run the update. This is only stuff that the child process executes. */
/* Basically, run the script and capture the script's output. */
{
FILE *scriptOutput = NULL;
char command[256], buf[2048];
char *curChar = command;
char *words[2];
/* Build the rtdbUpdate command. */
safef(command, ArraySize(command), "./bin/rtdbUpdate ");
curChar += strlen(command);
if (verBose)
    {
    safef(curChar, ArraySize(command)-(curChar-command), "-verbose ");
    curChar = command + strlen(command);
    }
if (force)
    {
    safef(curChar, ArraySize(command)-(curChar-command), "-force ");
    curChar = command + strlen(command);
    }
safef(curChar, ArraySize(command)-(curChar-command), "%s", database);
/* Run the command and save the output. */
scriptOutput = popen(command, "r");
fgets(buf, ArraySize(buf), scriptOutput);
if (chopLine(buf, words) != 2)
    logError("For some reason the script didn't create a log like it should.  Check to be sure the server was started correctly.");
else
    {
    netSendString(connectionHandle, "Update successful!");
    netSendString(connectionHandle, "$end$");
    }
pclose(scriptOutput);
}

int doUpdate(int connectionHandle, char *command)
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
    char *database = chopPrefix(command);
    close(childPipe[0]);
    update(database, connectionHandle);
    _exit(0);
    }
else
    logInfo("Starting update...");
close(childPipe[1]);
return childPipe[0];
}

void waitStatus()
/* Wait for the child and deal with errors. */
{
int status = 0;
pid_t child = wait(&status);
if (child == -1)
    logError("child process didn't finish like it should.  SIGCHILD not caught.  Or something.");

else if (!WIFEXITED(status))
    logError("SIGCHILD caught but child didn't exit normally.");
}

/*** Server functions ***/

void doStatus(int connectionHandle, char *hostName, int port)
/* Return some status information to the client. */
{
char buf[256];
logInfo("Status requested.");
netSendString(connectionHandle, "RTDB Server");
safef(buf, ArraySize(buf), "host %s", hostName);
netSendString(connectionHandle, buf);
safef(buf, ArraySize(buf), "port %d", port);
netSendString(connectionHandle, buf);
safef(buf, ArraySize(buf), "warnings %d", warnCount);
netSendString(connectionHandle, buf);
netSendString(connectionHandle, "end");
}

void startServer(char *hostName, char *portName)
/* Load up index and hang out in RAM. */
{
int socketHandle = 0;
int port = atoi(portName);
int childPipe = -1;
logDaemonize("rtdbServer");
socketHandle = getSocket(hostName, port);
for (;;)
    {
    fd_set filedes;
    int maxfd;
    int error = 0;
    FD_ZERO(&filedes);
    FD_SET(socketHandle, &filedes);
    if (childPipe > -1)
	FD_SET(childPipe, &filedes);
    maxfd = max(childPipe, socketHandle) + 1;    
    error = select(maxfd, &filedes, NULL, NULL, NULL);
    if (error == -1)
	errAbort("select() failed!!");
    else 
	{
	if (FD_ISSET(socketHandle, &filedes))
	    {
	    int connectionHandle = getConnection(socketHandle);
	    char command[256];
	    if (netGetString(connectionHandle, command) == NULL)
		continue;	
	    else if (sameString(command, "status"))
		doStatus(connectionHandle, hostName, port);
	    else if (startsWith("update", command))
		{
		logInfo("Update requested.");
		if (childPipe != -1)
		    rtdbWarning("Attempted to update while updating.");
		else 
		    childPipe = doUpdate(connectionHandle, command);
		}
	    else
		rtdbWarning("Unknown command %s", command);
	    close(connectionHandle);
	    }
	/* Handles pipe's EOF signal when child process ends. */
	if ((childPipe != -1) && FD_ISSET(childPipe, &filedes))
	    {
	    waitStatus();	
	    logInfo("Completed update.");
	    childPipe = -1;
	    }
	}
    }
close(socketHandle);
}

/**** Client functions ****/

int statusServer(char *hostName, char *portName)
/* Send status message to server arnd report result. */
{
char buf[256];
int sd = 0;
int ret = 0;
/* Put together command. */
sd = netMustConnectTo(hostName, portName);
safef(buf, ArraySize(buf), "status");
netSendString(sd, buf);
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

void updateServer(char *hostName, char *portName, char *command)
/* Send update.database message to server from the client. */
{
char buf[256];
int sd = 0;
sd = netMustConnectTo(hostName, portName);
safef(buf, ArraySize(buf), command);
netSendString(sd, buf);
for (;;)
    {
    if (netGetString(sd, buf) == NULL)
	{
	rtdbWarning("Error reading update information from %s:%s",hostName,portName);
	break;
	}
    if (sameString(buf, "$end$"))
        break;
    else
        printf("%s\n", buf);
    }
close(sd);
printf("sent update message to server\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
optionInit(&argc, argv, optionSpecs);
command = argv[1];
if (argc < 2)
    usage();
if (optionExists("noVerbose"))
    verBose = FALSE;
if (optionExists("noForce"))
    force = FALSE;
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
else if (startsWith("update", command))
    {
    if (argc != 4)
	usage();
    updateServer(argv[2], argv[3], command);
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
    usage();
return 0;
}
