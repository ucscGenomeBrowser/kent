#include "common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "portable.h"
#include "net.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "trans3.h"

int version = 14;
int maxSeqSize = 40000;
int maxAaSize = 8000;

int minMatch = gfMinMatch;	/* Can be overridden from command line. */
int tileSize = gfTileSize;	/* Can be overridden from command line. */
boolean doTrans = FALSE;	/* Do translation? */
boolean allowOneMismatch = FALSE; 
int repMatch = 1024;
int maxGap = gfMaxGap;
FILE *logFile = NULL;
boolean seqLog = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfServer - Make a server to quickly find where DNA occurs in genome.\n"
  "To set up a server:\n"
  "   gfServer start host port file(s).nib\n"
  "To remove a server:\n"
  "   gfServer stop host port\n"
  "To query a server with DNA sequence:\n"
  "   gfServer query host port probe.fa\n"
  "To query a server with protein sequence:\n"
  "   gfServer protQuery host port probe.fa\n"
  "To query a server with translated dna sequence:\n"
  "   gfServer transQuery host port probe.fa\n"
  "To process one probe fa file against a .nib format genome (not starting server):\n"
  "   gfServer direct probe.fa file(s).nib\n"
  "To figure out usage level\n"
  "   gfServer status host port\n"
  "To get input file list\n"
  "   gfServer files host port\n"
  "Options:\n"
  "   -tileSize=N size of n-mers to index.  Default is 11 for nucleotides, 4 for\n"
  "               proteins (or translated nucleotides).\n"
  "   -minMatch=N Number of n-mer matches that trigger detailed alignment\n"
  "               Default is 2 for nucleotides, 3 for protiens.\n"
  "   -maxGap=N   Number of insertions or deletions allowed between n-mers.\n"
  "               Default is 2 for nucleotides, 0 for protiens.\n"
  "   -trans  Translate database to protein in 6 frames.  Note: it is best\n"
  "           to run this on RepeatMasked data in this case.\n"
  "   -log=logFile keep a log file that records server requests.\n"
  "   -seqLog    Include sequences in log file\n"
  );

}

void genoFindDirect(char *probeName, int nibCount, char *nibFiles[])
/* Don't set up server - just directly look for matches. */
{
struct genoFind *gf = NULL;
static struct genoFind *transGf[2][3];
struct lineFile *lf = lineFileOpen(probeName, TRUE);
struct dnaSeq seq;
int hitCount = 0, clumpCount = 0, oneHit;
ZeroVar(&seq);

if (doTrans)
    errAbort("Don't support translated direct stuff currently, sorry");

gf = gfIndexNibs(nibCount, nibFiles, minMatch, maxGap, tileSize, repMatch, FALSE,
	allowOneMismatch);

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    struct lm *lm = lmInit(0);
    struct gfClump *clumpList = gfFindClumps(gf, &seq, lm, &oneHit), *clump;
    hitCount += oneHit;
    for (clump = clumpList; clump != NULL; clump = clump->next)
	{
	++clumpCount;
	printf("%s ", seq.name);
	gfClumpDump(gf, clump, stdout);
	}
    gfClumpFreeList(&clumpList);
    lmCleanup(&lm);
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

void logIt(char *format, ...)
/* Print message to log file. */
{
va_list args;
if (logFile != NULL)
    {
    fprintf(logFile, "%lu ", clock1000());
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fflush(logFile);
    }
}

/* Some variables to gather statistics on usage. */
long baseCount = 0, queryCount = 0, aaCount = 0;
int warnCount = 0;
int noSigCount = 0;
int missCount = 0;
int trimCount = 0;

void dnaQuery(struct genoFind *gf, struct dnaSeq *seq, 
	int connectionHandle, char buf[256])	
/* Handle a query for DNA/DNA match. */
{
struct gfClump *clumpList = NULL, *clump;
int limit = 100;
int clumpCount = 0, hitCount = -1;
struct lm *lm = lmInit(0);

clumpList = gfFindClumps(gf, seq, lm, &hitCount);
if (clumpList == NULL)
    ++missCount;
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct gfSeqSource *ss = clump->target;
    sprintf(buf, "%d\t%d\t%s\t%d\t%d\t%d", 
	clump->qStart, clump->qEnd, ss->fileName,
	clump->tStart-ss->start, clump->tEnd-ss->start, clump->hitCount);
    netSendString(connectionHandle, buf);
    ++clumpCount;
    if (--limit < 0)
	break;
    }
gfClumpFreeList(&clumpList);
lmCleanup(&lm);
logIt("%d clumps, %d hits\n", clumpCount, hitCount);
}

void transQuery(struct genoFind *transGf[2][3], aaSeq *seq, 
	int connectionHandle, char buf[256])	
/* Handle a query for protein/translated DNA match. */
{
struct gfClump *clumps[3], *clump;
int isRc, frame;
char strand;
struct dyString *dy  = newDyString(1024);
struct gfHit *hit;
int clumpCount = 0, hitCount = 0, oneHit;
struct lm *lm = lmInit(0);

sprintf(buf, "tileSize %d", tileSize);
netSendString(connectionHandle, buf);
for (frame = 0; frame < 3; ++frame)
    clumps[frame] = NULL;
for (isRc = 0; isRc <= 1; ++isRc)
    {
    strand = (isRc ? '-' : '+');
    gfTransFindClumps(transGf[isRc], seq, clumps, lm, &oneHit);
    hitCount += oneHit;
    for (frame = 0; frame < 3; ++frame)
        {
	int limit = 200;
	for (clump = clumps[frame]; clump != NULL; clump = clump->next)
	    {
	    struct gfSeqSource *ss = clump->target;
	    sprintf(buf, "%d\t%d\t%s\t%d\t%d\t%d\t%c\t%d", 
		clump->qStart, clump->qEnd, ss->fileName,
		clump->tStart-ss->start, clump->tEnd-ss->start, clump->hitCount,
		strand, frame);
	    netSendString(connectionHandle, buf);
	    dyStringClear(dy);
	    for (hit = clump->hitList; hit != NULL; hit = hit->next)
	        dyStringPrintf(dy, " %d %d", hit->qStart, hit->tStart - ss->start);
	    netSendLongString(connectionHandle, dy->string);
	    ++clumpCount;
	    if (--limit < 0)
		break;
	    }
	gfClumpFreeList(&clumps[frame]);
	}
    }
if (clumpCount == 0)
    ++missCount;
freeDyString(&dy);
lmCleanup(&lm);
logIt("%d clumps, %d hits\n", clumpCount, hitCount);
}

void transTransQuery(struct genoFind *transGf[2][3], struct dnaSeq *seq, 
	int connectionHandle, char buf[256])	
/* Handle a query for protein/translated DNA match. */
{
struct gfClump *clumps[3][3], *clump;
int isRc, qFrame, tFrame;
char strand;
struct trans3 *t3 = trans3New(seq);
struct dyString *dy  = newDyString(1024);
struct gfHit *hit;
int clumpCount = 0, hitCount = 0, oneCount;

sprintf(buf, "tileSize %d", tileSize);
netSendString(connectionHandle, buf);
for (qFrame = 0; qFrame<3; ++qFrame)
    for (tFrame=0; tFrame<3; ++tFrame)
	clumps[qFrame][tFrame] = NULL;
for (isRc = 0; isRc <= 1; ++isRc)
    {
    struct lm *lm = lmInit(0);
    strand = (isRc ? '-' : '+');
    gfTransTransFindClumps(transGf[isRc], t3->trans, clumps, lm, &oneCount);
    hitCount += oneCount;
    for (qFrame = 0; qFrame<3; ++qFrame)
	{
	for (tFrame=0; tFrame<3; ++tFrame)
	    {
	    int limit = 200;
	    for (clump = clumps[qFrame][tFrame]; clump != NULL; clump = clump->next)
		{
		struct gfSeqSource *ss = clump->target;
		sprintf(buf, "%d\t%d\t%s\t%d\t%d\t%d\t%c\t%d\t%d", 
		    clump->qStart, clump->qEnd, ss->fileName,
		    clump->tStart-ss->start, clump->tEnd-ss->start, clump->hitCount,
		    strand, qFrame, tFrame);
		netSendString(connectionHandle, buf);
		dyStringClear(dy);
		for (hit = clump->hitList; hit != NULL; hit = hit->next)
		    {
		    dyStringPrintf(dy, " %d %d", hit->qStart, hit->tStart - ss->start);
		    }
		netSendLongString(connectionHandle, dy->string);
		++clumpCount;
		if (--limit < 0)
		    break;
		}
	    gfClumpFreeList(&clumps[qFrame][tFrame]);
	    }
	}
    lmCleanup(&lm);
    }
trans3Free(&t3);
if (clumpCount == 0)
    ++missCount;
logIt("%d clumps, %d hits\n", clumpCount, hitCount);
}

void startServer(char *hostName, char *portName, int nibCount, char *nibFiles[])
/* Load up index and hang out in RAM. */
{
struct genoFind *gf = NULL;
static struct genoFind *transGf[2][3];
char buf[256];
char *line, *command;
int fromLen, readSize, res;
int socketHandle = 0, connectionHandle = 0;
char *logFileName = cgiOptionalString("log");
int port = atoi(portName);

netBlockBrokenPipes();
if (logFileName != NULL)
    logFile = mustOpen(logFileName, "a");
logIt("gfServer version %d on host %s, port %s\n", version, hostName, portName);
if (doTrans)
    {
    logIt("setting up translated index\n");
    gfIndexTransNibs(transGf, nibCount, nibFiles, 
    	minMatch, maxGap, tileSize, repMatch, NULL, allowOneMismatch);
    }
else
    {
    gf = gfIndexNibs(nibCount, nibFiles, minMatch, 
    	maxGap, tileSize, repMatch, NULL, allowOneMismatch);
    }
logIt("indexing complete\n");

/* Set up socket.  Get ready to listen to it. */
socketHandle = netAcceptingSocket(port, 100);

logIt("Server ready for queries!\n");
printf("Server ready for queries!\n");
for (;;)
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
    logIt("%s\n", buf);
    if (!startsWith(gfSignature(), buf))
        {
	++noSigCount;
	close(connectionHandle);
	continue;
	}
    line = buf + strlen(gfSignature());
    command = nextWord(&line);
    if (sameString("quit", command))
        {
	logIt("Ignoring quit message\n");
	break;
	}
    else if (sameString("status", command))
        {
	sprintf(buf, "version %d", version);
	netSendString(connectionHandle, buf);
	sprintf(buf, "type %s", (doTrans ? "translated" : "nucleotide"));
	netSendString(connectionHandle, buf);
	sprintf(buf, "host %s", hostName);
	netSendString(connectionHandle, buf);
	sprintf(buf, "port %s", portName);
	netSendString(connectionHandle, buf);
	sprintf(buf, "tileSize %d", tileSize);
	netSendString(connectionHandle, buf);
	sprintf(buf, "minMatch %d", minMatch);
	netSendString(connectionHandle, buf);
	sprintf(buf, "requests %ld", queryCount);
	netSendString(connectionHandle, buf);
	sprintf(buf, "bases %ld", baseCount);
	netSendString(connectionHandle, buf);
	if (doTrans)
	    {
	    sprintf(buf, "aa %ld", aaCount);
	    netSendString(connectionHandle, buf);
	    }
	sprintf(buf, "misses %d", missCount);
	netSendString(connectionHandle, buf);
	sprintf(buf, "noSig %d", noSigCount);
	netSendString(connectionHandle, buf);
	sprintf(buf, "trimmed %d", trimCount);
	netSendString(connectionHandle, buf);
	sprintf(buf, "warnings %d", warnCount);
	netSendString(connectionHandle, buf);
	netSendString(connectionHandle, "end");
	}
    else if (sameString("query", command) || 
    	sameString("protQuery", command) || sameString("transQuery", command))
        {
	int querySize;
	boolean queryIsProt = sameString(command, "protQuery");
	char *s = nextWord(&line);
	if (s == NULL || !isdigit(s[0]))
	    {
	    warn("Expecting query size after query command");
	    ++warnCount;
	    }
	else
	    {
	    struct dnaSeq seq;
	    char *type = NULL;
            ZeroVar(&seq);

	    if (queryIsProt && !doTrans)
	        {
		warn("protein query sent to nucleotide server");
		++warnCount;
		queryIsProt = FALSE;
		}
	    buf[0] = 'Y';
	    if (write(connectionHandle, buf, 1) == 1)
		{
		seq.size = atoi(s);
		seq.name = NULL;
		if (seq.size > 0)
		    {
		    ++queryCount;
		    seq.dna = needLargeMem(seq.size+1);
		    if (gfReadMulti(connectionHandle, seq.dna, seq.size) != seq.size)
			{
			warn("Didn't sockRecieveString all %d bytes of query sequence", seq.size);
			++warnCount;
			}
		    else
			{
			int maxSize = (doTrans ? maxAaSize : maxSeqSize);

			seq.dna[seq.size] = 0;
			if (queryIsProt)
			    {
			    seq.size = aaFilteredSize(seq.dna);
			    aaFilter(seq.dna, seq.dna);
			    }
			else
			    {
			    seq.size = dnaFilteredSize(seq.dna);
			    dnaFilter(seq.dna, seq.dna);
			    }
			if (seq.size > maxSize)
			    {
			    ++trimCount;
			    seq.size = maxSize;
			    seq.dna[maxSize] = 0;
			    }
			if (queryIsProt)
			    aaCount += seq.size;
			else
			    baseCount += seq.size;
			if (seqLog && logFile != NULL)
			    {
			    faWriteNext(logFile, "query", seq.dna, seq.size);
			    fflush(logFile);
			    }
			if (doTrans)
			   {
			   if (queryIsProt)
				transQuery(transGf, &seq, connectionHandle, buf);
			   else
				transTransQuery(transGf, &seq, 
				    connectionHandle, buf);
			   }
			else
			    dnaQuery(gf, &seq, connectionHandle, buf);
			}
		    freez(&seq.dna);
		    }
		netSendString(connectionHandle, "end");
		}
	    }
	}
    else if (sameString("files", command))
        {
	struct gfSeqSource *ss;
	int i;
	sprintf(buf, "%d", nibCount);
	netSendString(connectionHandle, buf);
	for (i=0; i<nibCount; ++i)
	    {
	    sprintf(buf, "%s", nibFiles[i]);
	    netSendString(connectionHandle, buf);
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

sd = netMustConnectTo(hostName, portName);
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

/* Put together command. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%sstatus", gfSignature());
write(sd, buf, strlen(buf));

for (;;)
    {
    if (netGetString(sd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
        break;
    else
        printf("%s\n", buf);
    }
close(sd);
}

void queryServer(char *type, 
	char *hostName, char *portName, char *faName, boolean complex, boolean isProt)
/* Send simple query to server and report results. */
{
char buf[256];
char *line, *command;
int fromLen, readSize;
int sd = 0;
bioSeq *seq = faReadSeq(faName, !isProt);
int matchCount = 0;

/* Put together query command. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%s%s %d", gfSignature(), type, seq->size);
write(sd, buf, strlen(buf));

read(sd, buf, 1);
if (buf[0] != 'Y')
    errAbort("Expecting 'Y' from server, got %c", buf[0]);
write(sd, seq->dna, seq->size);

if (complex)
    {
    char *s = netRecieveString(sd, buf);
    printf("%s\n", s);
    }

for (;;)
    {
    if (netGetString(sd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
	{
	printf("%d matches\n", matchCount);
	break;
	}
    else
	{
        printf("%s\n", buf);
	if (complex)
	    {
	    char *s = netGetLongString(sd);
	    if (s == NULL)
	        break;
	    printf("%s\n", s);
	    freeMem(s);
	    }
	}
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

/* Put together command. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%sfiles", gfSignature());
write(sd, buf, strlen(buf));

/* Get count of files, and then each file name. */
if (netGetString(sd, buf) != NULL)
    {
    fileCount = atoi(buf);
    for (i=0; i<fileCount; ++i)
	{
	printf("%s\n", netRecieveString(sd, buf));
	}
    }
close(sd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;

gfCatchPipes();
cgiSpoof(&argc, argv);
command = argv[1];
if (cgiBoolean("trans"))
    {
    doTrans = TRUE;
    tileSize = 4;
    minMatch = 3;
    maxGap = 0;
    repMatch = gfPepMaxTileUse;
    }
tileSize = cgiOptionalInt("tileSize", tileSize);
minMatch = cgiOptionalInt("minMatch", minMatch);
repMatch = cgiOptionalInt("repMatch", repMatch);
seqLog = cgiBoolean("seqLog");
if (argc < 2)
    usage();
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
    queryServer(command, argv[2], argv[3], argv[4], FALSE, FALSE);
    }
else if (sameWord(command, "protQuery"))
    {
    if (argc != 5)
	usage();
    queryServer(command, argv[2], argv[3], argv[4], TRUE, TRUE);
    }
else if (sameWord(command, "transQuery"))
    {
    if (argc != 5)
	usage();
    queryServer(command, argv[2], argv[3], argv[4], TRUE, FALSE);
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
