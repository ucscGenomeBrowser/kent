#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "genoFind.h"

int version = 2;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfServer - Make a server to quickly find where DNA occurs in genome.\n"
  "To set up a server:\n"
  "   gfServer start host port file(s).nib\n"
  "To remove a server:\n"
  "   gfServer stop host port\n"
  "To query a server:\n"
  "   gfServer query host port probe.fa\n"
  "To process one probe fa file against a .nib format genome (not starting server):\n"
  "   gfServer direct probe.fa file(s).nib\n"
  "To figure out usage level\n"
  "   gfServer status host port\n"
  "To get input file list\n"
  "   gfServer files host port\n");
}

void genoFindDirect(char *probeName, int nibCount, char *nibFiles[])
/* Don't set up server - just directly look for matches. */
{
struct genoFind *gf = gfIndexNibs(nibCount, nibFiles, gfMinMatch, gfMaxGap, gfTileSize, gfMaxTileUse);
struct lineFile *lf = lineFileOpen(probeName, TRUE);
struct dnaSeq seq;

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    struct gfClump *clumpList = gfFindClumps(gf, &seq), *clump;
    for (clump = clumpList; clump != NULL; clump = clump->next)
	{
	printf("%s ", seq.name);
	gfClumpDump(gf, clump, stdout);
	}
    gfClumpFreeList(&clumpList);
    }
lineFileClose(&lf);
genoFindFree(&gf);
}

int getPortIx(char *portName)
/* Convert from ascii to integer. */
{
if (!isdigit(portName[0]))
    errAbort("Expecting a port number got %s", portName);
return atoi(portName);
}

struct sockaddr_in sai;		/* Some system socket info. */

int setupSocket(char *portName, char *hostName)
/* Set up our socket. */
{
int port = getPortIx(portName);
struct hostent *hostent;
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    herror("");
    errAbort("Couldn't find host %s. h_errno %d", hostName, h_errno);
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
memcpy(&sai.sin_addr.s_addr, hostent->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
return socket(AF_INET, SOCK_STREAM, 0);
}

void startServer(char *hostName, char *portName, int nibCount, char *nibFiles[])
/* Load up index and hang out in RAM. */
{
struct genoFind *gf = gfIndexNibs(nibCount, nibFiles, gfMinMatch, gfMaxGap, gfTileSize, gfMaxTileUse);
char buf[256];
char *line, *command;
int fromLen, readSize;
int socketHandle = 0, connectionHandle = 0;
long baseCount = 0, queryCount = 0;
int warnCount = 0;
int noSigCount = 0;

/* Set up socket.  Get ready to listen to it. */
socketHandle = setupSocket(portName, hostName);
if (bind(socketHandle, &sai, sizeof(sai)) == -1)
     errAbort("Couldn't bind to %s port %s", hostName, portName);
listen(socketHandle, 100);

printf("Server ready for queries!\n");
for (;;)
    {
    connectionHandle = accept(socketHandle, &sai, &fromLen);
    readSize = read(connectionHandle, buf, sizeof(buf)-1);
    buf[readSize] = 0;
    if (!startsWith(gfSignature(), buf))
        {
	++noSigCount;
	continue;
	}
    line = buf + strlen(gfSignature());
    command = nextWord(&line);
    if (sameString("quit", command))
        {
	printf("Quitting genoFind server\n");
	break;
	}
    else if (sameString("status", command))
        {
	sprintf(buf, "version %d", version);
	gfSendString(connectionHandle, buf);
	sprintf(buf, "requests %ld", queryCount);
	gfSendString(connectionHandle, buf);
	sprintf(buf, "bases %ld", baseCount);
	gfSendString(connectionHandle, buf);
	sprintf(buf, "noSig %d", noSigCount);
	gfSendString(connectionHandle, buf);
	sprintf(buf, "warnings %d", warnCount);
	gfSendString(connectionHandle, buf);
	gfSendString(connectionHandle, "end");
	}
    else if (sameString("query", command))
        {
	int querySize;
	char *s = nextWord(&line);
	if (s == NULL || !isdigit(s[0]))
	    {
	    warn("Expecting query size after query command");
	    ++warnCount;
	    }
	else
	    {
	    struct dnaSeq seq;

	    seq.size = atoi(s);
	    buf[0] = 'Y';
	    write(connectionHandle, buf, 1);
	    seq.name = NULL;
	    if (seq.size > 0)
		{
		++queryCount;
		baseCount += seq.size;
		seq.dna = needLargeMem(seq.size);
		if (gfReadMulti(connectionHandle, seq.dna, seq.size) != seq.size)
		    {
		    warn("Didn't sockRecieveString all %d bytes of query sequence", seq.size);
		    ++warnCount;
		    }
		else
		    {
		    struct gfClump *clumpList = gfFindClumps(gf, &seq), *clump;
		    int limit = 100;

		    for (clump = clumpList; clump != NULL; clump = clump->next)
			{
			struct gfSeqSource *ss = clump->target;
			sprintf(buf, "%d\t%d\t%s\t%d\t%d\t%d", 
			    clump->qStart, clump->qEnd, ss->fileName,
			    clump->tStart-ss->start, clump->tEnd-ss->start, clump->hitCount);
			gfSendString(connectionHandle, buf);
			if (--limit < 0)
			    break;
			}
		    gfClumpFreeList(&clumpList);
		    }
		freez(&seq.dna);
		}
	    gfSendString(connectionHandle, "end");
	    }
	}
    else if (sameString("files", command))
        {
	struct gfSeqSource *ss;
	int i;
	sprintf(buf, "%d", gf->sourceCount);
	gfSendString(connectionHandle, buf);
	for (i=0; i<gf->sourceCount; ++i)
	    {
	    ss = gf->sources + i;
	    sprintf(buf, "%s\t%d", ss->fileName, ss->end - ss->start);
	    gfSendString(connectionHandle, buf);
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
close(socketHandle);
}

void stopServer(char *hostName, char *portName)
/* Send stop message to server. */
{
char buf[256];
int sd = 0;


/* Set up socket.  Get ready to listen to it. */
sd = setupSocket(portName, hostName);
if (connect(sd, &sai, sizeof(sai)) == -1)
     errAbort("Couldn't connect to %s port %s", hostName, portName);

/* Put together quit command. */
sprintf(buf, "%squit", gfSignature());
write(sd, buf, strlen(buf));

close(sd);
printf("sent stop message to server\n");
}

void statusServer(char *hostName, char *portName)
/* Send status message to server arnd report result. */
{
char buf[256];
char *line, *command;
int fromLen, readSize;
int sd = 0;
int fileCount;
int i;

/* Set up socket.  Get ready to listen to it. */
sd = setupSocket(portName, hostName);
if (connect(sd, &sai, sizeof(sai)) == -1)
     errAbort("Couldn't connect to %s port %s", hostName, portName);

/* Put together command. */
sprintf(buf, "%sstatus", gfSignature());
write(sd, buf, strlen(buf));

for (;;)
    {
    if (gfGetString(sd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
        break;
    else
        printf("%s\n", buf);
    }
close(sd);
}

void queryServer(char *hostName, char *portName, char *faName)
/* Send simple query to server and report results. */
{
char buf[256];
char *line, *command;
int fromLen, readSize;
int sd = 0;
struct dnaSeq *seq = faReadDna(faName);
int matchCount = 0;

/* Set up socket.  Get ready to listen to it. */
sd = setupSocket(portName, hostName);
if (connect(sd, &sai, sizeof(sai)) == -1)
     errAbort("Couldn't connect to %s port %s", hostName, portName);

/* Put together query command. */
sprintf(buf, "%squery %d", gfSignature(), seq->size);
write(sd, buf, strlen(buf));

read(sd, buf, 1);
if (buf[0] != 'Y')
    errAbort("Expecting 'Y' from server, got %c", buf[0]);
write(sd, seq->dna, seq->size);

for (;;)
    {
    if (gfGetString(sd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
	{
	printf("%d matches\n", matchCount);
	break;
	}
    else
        printf("%s\n", buf);
    ++matchCount;
    }
close(sd);
}

void getFileList(char *hostName, char *portName)
/* Get and display input file list. */
{
char buf[256];
char *line, *command;
int fromLen, readSize;
int sd = 0;
int fileCount;
int i;

/* Set up socket.  Get ready to listen to it. */
sd = setupSocket(portName, hostName);
if (connect(sd, &sai, sizeof(sai)) == -1)
     errAbort("Couldn't connect to %s port %s", hostName, portName);

/* Put together command. */
sprintf(buf, "%sfiles", gfSignature());
write(sd, buf, strlen(buf));

/* Get count of files, and then each file name. */
if (gfGetString(sd, buf) != NULL)
    {
    fileCount = atoi(buf);
    for (i=0; i<fileCount; ++i)
	{
	printf("%s\n", gfRecieveString(sd, buf));
	}
    }
close(sd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;

if (argc < 2)
    usage();
command = argv[1];
if (sameWord(command, "direct"))
    {
    if (argc < 4)
        usage();
    genoFindDirect(argv[2], argc-3, argv+3);
    }
else if (sameWord(command, "start"))
    {
    if (argc < 5)
        usage();
    startServer(argv[2], argv[3], argc-4, argv+4);
    }
else if (sameWord(command, "stop"))
    {
    if (argc != 4)
	usage();
    stopServer(argv[2], argv[3]);
    }
else if (sameWord(command, "query"))
    {
    if (argc != 5)
	usage();
    queryServer(argv[2], argv[3], argv[4]);
    }
else if (sameWord(command, "status"))
    {
    if (argc != 4)
	usage();
    statusServer(argv[2], argv[3]);
    }
else if (sameWord(command, "files"))
    {
    if (argc != 4)
	usage();
    getFileList(argv[2], argv[3]);
    }
else
    {
    usage();
    }
return 0;
}
