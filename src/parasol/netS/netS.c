/* netS - net server. */
#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dystring.h"

int socketHandle;
int connectionHandle;
char signature[] = "0d2f070562685f29";
char execCommand[512];

void usage()
/* Explain usage and exit. */
{
errAbort("netS - net server.\n"
         "usage:\n"
	 "    netS socket\n");
}

boolean busy = FALSE;
boolean gotZombie = FALSE;
int childId;
int zombieStatus;
int zombieX;

void childSignalHandler(int x)
/* Handle child died signal. */
{
zombieX = x;
gotZombie = TRUE;
}

void clearZombies()
/* Wait on any leftover zombie. */
{
if (gotZombie)
    {
    int pid, status;
    uglyf("Waiting on zombie\n");
    pid = wait(&status);
    if (pid == childId)
	{
	zombieStatus = status;
	childId = 0;
	busy = FALSE;
	gotZombie = FALSE;
	execCommand[0] = 0;
	}
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
	clearZombies();
    printf("executing %s\n", line);
    strcpy(execCommand, line);
    exe = nextWord(&line);
    signal(SIGCHLD, childSignalHandler);
    if ((childId = fork()) == 0)
	{
	close(socketHandle);
	close(connectionHandle);
	if (execlp(exe, exe, line, NULL) < 0)
	    {
	    perror("");
	    errAbort("Error execlp'ing %s %s", exe, line);
	    }
	errAbort("I should never get here!");
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

void netS(char *hostName)
/* netS - a net server. */
{
struct sockaddr_in sai;
char buf[512];
char *line;
int fromLen, readSize;
int childCount = 0;
struct hostent *hostent;
int port = 0x46DC;
char *command;

/* Set up socket and self to listen to it. */
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    herror("");
    errAbort("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
memcpy(&sai.sin_addr.s_addr, hostent->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
socketHandle = socket(AF_INET, SOCK_STREAM, 0);
if (bind(socketHandle, &sai, sizeof(sai)) == -1)
    errAbort("Couldn't bind to %s", hostName);
listen(socketHandle, 10);

for (;;)
    {
    connectionHandle = accept(socketHandle, &sai, &fromLen);
    clearZombies();
    ++childCount;
    readSize = read(connectionHandle, buf, sizeof(buf)-1);
    buf[readSize] = 0;
    if (!startsWith(signature, buf))
	{
	warn("Command without signature\n%s", buf);
	continue;
	}
    line = buf + sizeof(signature);
    printf("server read '%s'\n", line);
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
netS(argv[1]);
}


