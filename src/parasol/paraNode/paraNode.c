/* paraNode - parasol node server. */
#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dystring.h"
#include "paraLib.h"

int socketHandle;		/* Handle to socket to hub. */
int connectionHandle;	        /* Connection to singel hub request. */
char execCommand[512];          /* Name of command currently executing. */

boolean busy = FALSE;           /* True if executing a command. */
boolean gotZombie = FALSE;      /* True if command finished. */
int childId;                    /* PID of child. */
int grandId;			/* PID of grandchild. */

void usage()
/* Explain usage and exit. */
{
errAbort("paraNode - parasol node serve.\n"
         "usage:\n"
	 "    paraNode start\n");
}


void childSignalHandler(int x)
/* Handle child died signal. */
{
gotZombie = TRUE;
}

void clearZombie()
/* Wait on any leftover zombie. */
{
if (gotZombie)
    {
    int pid, status;
    uglyf("Waiting on zombie\n");
    pid = wait(&status);
    if (pid == childId)
	{
	childId = 0;
	busy = FALSE;
	gotZombie = FALSE;
	execCommand[0] = 0;
	}
    }
}

void handleTerm(int x)
/* Handle SIGTERM for manager process - kill grandchild. */
{
if (grandId != 0)
    {
    int id = grandId;
    grandId = 0;
    kill(id, SIGTERM);
    }
exit(-1);
}

void manageExec(char *exe, char *params)
/* This routine is the child process of doExec.
 * It spawns a grandchild that actually does the
 * work and waits on it.  It then lets the hub
 * know when the exec is done. */
{
if ((grandId = fork()) == 0)
    {
    close(socketHandle);
    close(connectionHandle);
    if (execlp(exe, exe, params, NULL) < 0)
	{
	perror("");
	errAbort("Error execlp'ing %s %s", exe, params);
	}
    // Never gets here because of execlp.
    errAbort("Must have skipped execlp!");
    }
else
    {
    int status;
    char buf[16];

    signal(SIGTERM, handleTerm);
    wait(&status);
    sprintf(buf, "%d", status);
    write(connectionHandle, buf, strlen(buf));
    exit(0);
    }
}

void doExec(char *line)
/* Execute string. */
{
if (line == NULL)
    warn("Executing nothing...");
else if (!busy || gotZombie)
    {
    char *exe;
    if (gotZombie)
	clearZombie();
    printf("executing %s\n", line);
    strcpy(execCommand, line);
    exe = nextWord(&line);
    signal(SIGCHLD, childSignalHandler);
    if ((childId = fork()) == 0)
	{
	manageExec(exe, line);
	}
    else
	{
	busy = TRUE;
	}
    }
else
    {
    warn("Trying to execute when busy.");
    }
}

void doKill()
/* Kill current job if any. */
{
if (childId != 0)
    {
    uglyf("Killing %s\n", execCommand);
    kill(childId, SIGTERM);
    clearZombie();
    }
else
    {
    warn("Nothing to kill\n");
    }
}


void doStatus()
/* Report status. */
{
char *status = (busy ? "busy" : "free");
struct dyString *dy = newDyString(256);
dyStringAppend(dy, status);
uglyf("status %s\n", (busy ? "busy" : "free"));
if (busy)
    {
    uglyf("Executing %s\n", execCommand);
    dyStringPrintf(dy, " %s", execCommand);
    }
write(connectionHandle, dy->string, dy->stringSize);
}

void paraNode(char *xxx)
/* paraNode - a net server. */
{
struct sockaddr_in sai;
char buf[512];
char *line;
int fromLen, readSize;
int childCount = 0;
struct hostent *hostent;
char *command;
int sigLen = strlen(paraSig);
char *hostName = getenv("HOST");
if (hostName == NULL)
    hostName = "localhost";

/* Set up socket and self to listen to it. */
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    herror("");
    errAbort("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(paraPort);
memcpy(&sai.sin_addr.s_addr, hostent->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
socketHandle = socket(AF_INET, SOCK_STREAM, 0);
if (bind(socketHandle, &sai, sizeof(sai)) == -1)
    errAbort("Couldn't bind to %s", hostName);
listen(socketHandle, 10);
for (;;)
    {
    connectionHandle = accept(socketHandle, &sai, &fromLen);
    readSize = read(connectionHandle, buf, sizeof(buf)-1);
    clearZombie();
    ++childCount;
    buf[readSize] = 0;
    if (!startsWith(paraSig, buf))
	{
	warn("Command without signature\n%s", buf);
	continue;
	}
    line = buf + sigLen;
    uglyf("server read '%s'\n", line);
    command = nextWord(&line);
    if (sameString("quit", command))
	break;
    else if (sameString("exec", command))
	doExec(line);
    else if (sameString("status", command))
	doStatus();
    else if (sameString("kill", command))
	doKill();
    else
	warn("Unrecognised command %s", command);
    close(connectionHandle);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraNode(argv[1]);
}


