/* paraNode - parasol node server. */
#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dystring.h"
#include "paraLib.h"
#include "net.h"

int socketHandle;		/* Handle to socket to hub. */
int connectionHandle;	        /* Connection to singel hub request. */
char execCommand[16*1024];          /* Name of command currently executing. */

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

void manageExec(char *managingHost, char *jobId, char *exe, char *params[])
/* This routine is the child process of doExec.
 * It spawns a grandchild that actually does the
 * work and waits on it.  It then lets the hub
 * know when the exec is done. */
{
close(connectionHandle);
if ((grandId = fork()) == 0)
    {
    close(socketHandle);
    uglyf("node: manageExec(%s, %s %s)\n", exe, params[0], params[1]);
    if (execvp(exe, params) < 0)
	{
	perror("");
	errAbort("Error execlp'ing %s %s", exe, params);
	}
    // Never gets here because of execlp.
    errAbort("Must have skipped execlp!");
    }
else
    {
    /* Wait on executed job and send jobID and status back to whoever
     * started the job. */
    int status;
    int sd;
    char buf[128];
    signal(SIGTERM, handleTerm);
    wait(&status);
    sd = netConnPort(managingHost, paraPort);
    if (sd > 0)
        {
	sprintf(buf, "jobDone %s %d", jobId, status);
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, buf);
	close(sd);
	}
    exit(0);
    }
}

void doRun(char *line)
/* Execute command. */
{
static char *args[1024];
char *managingHost, *jobId, *user, *dir, *in, *out, *err, *cmd;
int argCount;
int commandOffset;
if (line == NULL)
    warn("Executing nothing...");
else if (!busy || gotZombie)
    {
    char *exe;
    if (gotZombie)
	clearZombie();
    printf("node: executing %s\n", line);
    strcpy(execCommand, line);
    argCount = chopLine(line, args);
    if (argCount >= ArraySize(args))
        errAbort("Too many arguments");
    args[argCount] = NULL;
    managingHost = args[0];
    jobId = args[1];
    user = args[2];
    dir = args[3];
    in = args[4];
    out = args[5];
    err = args[6];
    commandOffset = 7;
    signal(SIGCHLD, childSignalHandler);
    if ((childId = fork()) == 0)
	{
	manageExec(managingHost, jobId, args[commandOffset], args+commandOffset);
	}
    else
	{
	busy = TRUE;
	}
    }
else
    {
    warn("Trying to run when busy.");
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
    uglyf("executing %s\n", execCommand);
    dyStringPrintf(dy, " %s", execCommand);
    }
write(connectionHandle, dy->string, dy->stringSize);
}

void paraNode()
/* paraNode - a net server. */
{
struct sockaddr_in sai;
char *buf = NULL, *line;
int fromLen, readSize;
int childCount = 0;
struct hostent *hostent;
char *command;
char signature[20];
int sigLen = strlen(paraSig);
char *hostName = getenv("HOST");
if (hostName == NULL)
    hostName = "localhost";

/* Precompute some signature stuff. */
assert(sigLen < sizeof(signature));
signature[sigLen] = 0;

/* Set up socket and self to listen to it. */
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    herror("");
    errAbort("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    }
ZeroVar(&sai);
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
    if (netMustReadAll(connectionHandle, signature, sigLen))
	{
	if (sameString(paraSig, signature))
	    {
	    line = buf = netGetLongString(connectionHandle);
	    uglyf("paraNode %s: read '%s'\n", hostName, line);
	    clearZombie();
	    ++childCount;
	    command = nextWord(&line);
	    if (sameString("quit", command))
		break;
	    else if (sameString("run", command))
		doRun(line);
	    else if (sameString("status", command))
		doStatus();
	    else if (sameString("kill", command))
		doKill();
	    freez(&buf);
	    }
	}
    if (connectionHandle != 0)
	{
	close(connectionHandle);
	connectionHandle = 0;
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
if (fork() == 0)
    paraNode();
return 0;
}


