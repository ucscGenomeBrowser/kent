/* gfServer - set up an index of the genome in memory and
 * respond to search requests. */
/* Copyright 2001-2003 Jim Kent.  All rights reserved. */
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
#include "twoBit.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "memalloc.h"
#include "genoFind.h"
#include "options.h"
#include "trans3.h"
#include "log.h"
#include "internet.h"

static char const rcsid[] = "$Id: gfServer.c,v 1.57 2009/10/09 19:36:03 kent Exp $";

static struct optionSpec optionSpecs[] = {
    {"canStop", OPTION_BOOLEAN},
    {"log", OPTION_STRING},
    {"logFacility", OPTION_STRING},
    {"mask", OPTION_BOOLEAN},
    {"maxAaSize", OPTION_INT},
    {"maxDnaHits", OPTION_INT},
    {"maxGap", OPTION_INT},
    {"maxNtSize", OPTION_INT},
    {"maxTransHits", OPTION_INT},
    {"minMatch", OPTION_INT},
    {"repMatch", OPTION_INT},
    {"seqLog", OPTION_BOOLEAN},
    {"ipLog", OPTION_BOOLEAN},
    {"stepSize", OPTION_INT},
    {"tileSize", OPTION_INT},
    {"trans", OPTION_BOOLEAN},
    {"syslog", OPTION_BOOLEAN},
    {NULL, 0}
};


int maxNtSize = 40000;
int maxAaSize = 8000;

int minMatch = gfMinMatch;	/* Can be overridden from command line. */
int tileSize = gfTileSize;	/* Can be overridden from command line. */
int stepSize = 0;		/* Can be overridden from command line. */
boolean doTrans = FALSE;	/* Do translation? */
boolean allowOneMismatch = FALSE; 
int repMatch = 1024;    /* Can be overridden from command line. */
int maxDnaHits = 100;   /* Can be overridden from command line. */
int maxTransHits = 200; /* Can be overridden from command line. */
int maxGap = gfMaxGap;
boolean seqLog = FALSE;
boolean ipLog = FALSE;
boolean doMask = FALSE;
boolean canStop = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfServer v %s - Make a server to quickly find where DNA occurs in genome.\n"
  "To set up a server:\n"
  "   gfServer start host port file(s)\n"
  "   Where the files are in .nib or .2bit format\n"
  "To remove a server:\n"
  "   gfServer stop host port\n"
  "To query a server with DNA sequence:\n"
  "   gfServer query host port probe.fa\n"
  "To query a server with protein sequence:\n"
  "   gfServer protQuery host port probe.fa\n"
  "To query a server with translated dna sequence:\n"
  "   gfServer transQuery host port probe.fa\n"
  "To query server with PCR primers\n"
  "   gfServer pcr host port fPrimer rPrimer maxDistance\n"
  "To process one probe fa file against a .nib format genome (not starting server):\n"
  "   gfServer direct probe.fa file(s).nib\n"
  "To test pcr without starting server:\n"
  "   gfServer pcrDirect fPrimer rPrimer file(s).nib\n"
  "To figure out usage level\n"
  "   gfServer status host port\n"
  "To get input file list\n"
  "   gfServer files host port\n"
  "Options:\n"
  "   -tileSize=N size of n-mers to index.  Default is 11 for nucleotides, 4 for\n"
  "               proteins (or translated nucleotides).\n"
  "   -stepSize=N spacing between tiles. Default is tileSize.\n"
  "   -minMatch=N Number of n-mer matches that trigger detailed alignment\n"
  "               Default is 2 for nucleotides, 3 for protiens.\n"
  "   -maxGap=N   Number of insertions or deletions allowed between n-mers.\n"
  "               Default is 2 for nucleotides, 0 for protiens.\n"
  "   -trans  Translate database to protein in 6 frames.  Note: it is best\n"
  "           to run this on RepeatMasked data in this case.\n"
  "   -log=logFile keep a log file that records server requests.\n"
  "   -seqLog    Include sequences in log file (not logged with -syslog)\n"
  "   -ipLog     Include user's IP in log file (not logged with -syslog)\n"
  "   -syslog    Log to syslog\n"
  "   -logFacility=facility log to the specified syslog facility - default local0.\n"
  "   -mask      Use masking from nib file.\n"
  "   -repMatch=N Number of occurrences of a tile (nmer) that trigger repeat masking the tile.\n"
  "               Default is %d.\n"
  "   -maxDnaHits=N Maximum number of hits for a dna query that are sent from the server.\n"
  "               Default is %d.\n"
  "   -maxTransHits=N Maximum number of hits for a translated query that are sent from the server.\n"
  "               Default is %d.\n"
  "   -maxNtSize=N Maximum size of untranslated DNA query sequence\n"
  "               Default is %d\n"
  "   -maxAsSize=N Maximum size of protein or translated DNA queries\n"
  "               Default is %d\n"
  "   -canStop If set then a quit message will actually take down the\n"
  "            server\n"
  ,	gfVersion, repMatch, maxDnaHits, maxTransHits, maxNtSize, maxAaSize
  );

}

void genoFindDirect(char *probeName, int fileCount, char *seqFiles[])
/* Don't set up server - just directly look for matches. */
{
struct genoFind *gf = NULL;
struct lineFile *lf = lineFileOpen(probeName, TRUE);
struct dnaSeq seq;
int hitCount = 0, clumpCount = 0, oneHit;
ZeroVar(&seq);

if (doTrans)
    errAbort("Don't support translated direct stuff currently, sorry");

gf = gfIndexNibsAndTwoBits(fileCount, seqFiles, minMatch, maxGap, 
	tileSize, repMatch, FALSE,
	allowOneMismatch, stepSize);

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

void genoPcrDirect(char *fPrimer, char *rPrimer, int fileCount, char *seqFiles[])
/* Do direct PCR for testing purposes. */
{
struct genoFind *gf = NULL;
int fPrimerSize = strlen(fPrimer);
int rPrimerSize = strlen(rPrimer);
struct gfClump *clumpList, *clump;
time_t startTime, endTime;

startTime = clock1000();
gf = gfIndexNibsAndTwoBits(fileCount, seqFiles, minMatch, maxGap, 
	tileSize, repMatch, FALSE,
	allowOneMismatch, stepSize);
endTime = clock1000();
printf("Index built in %4.3f seconds\n", 0.001 * (endTime - startTime));

printf("plus strand:\n");
startTime = clock1000();
clumpList = gfPcrClumps(gf, fPrimer, fPrimerSize, rPrimer, rPrimerSize, 0, 4*1024);
endTime = clock1000();
printf("Index searched in %4.3f seconds\n", 0.001 * (endTime - startTime));
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    /* Clumps from gfPcrClumps have already had target->start subtracted out 
     * of their coords, but gfClumpDump assumes they have not and does the 
     * subtraction; rather than write a new gfClumpDump, tweak here: */
    clump->tStart += clump->target->start;
    clump->tEnd += clump->target->start;
    gfClumpDump(gf, clump, stdout);
    }
printf("minus strand:\n");
startTime = clock1000();
clumpList = gfPcrClumps(gf, rPrimer, rPrimerSize, fPrimer, fPrimerSize, 0, 4*1024);
endTime = clock1000();
printf("Index searched in %4.3f seconds\n", 0.001 * (endTime - startTime));
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    /* Same as above, tweak before gfClumpDump: */
    clump->tStart += clump->target->start;
    clump->tEnd += clump->target->start;
    gfClumpDump(gf, clump, stdout);
    }

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

/* Some variables to gather statistics on usage. */
long baseCount = 0, blatCount = 0, aaCount = 0, pcrCount = 0;
int warnCount = 0;
int noSigCount = 0;
int missCount = 0;
int trimCount = 0;

void dnaQuery(struct genoFind *gf, struct dnaSeq *seq, 
	int connectionHandle, char buf[256])	
/* Handle a query for DNA/DNA match. */
{
struct gfClump *clumpList = NULL, *clump;
int limit = 1000;
int clumpCount = 0, hitCount = -1;
struct lm *lm = lmInit(0);

if (seq->size > gf->tileSize + gf->stepSize + gf->stepSize)
     limit = maxDnaHits;
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
logDebug("%lu %d clumps, %d hits", clock1000(), clumpCount, hitCount);
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
	int limit = maxTransHits;
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
logDebug("%lu %d clumps, %d hits", clock1000(), clumpCount, hitCount);
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
	    int limit = maxTransHits;
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
logDebug("%lu %d clumps, %d hits", clock1000(), clumpCount, hitCount);
}

static void pcrQuery(struct genoFind *gf, char *fPrimer, char *rPrimer, 
	int maxDistance, int connectionHandle)
/* Do PCR query and report results down socket. */
{
int fPrimerSize = strlen(fPrimer);
int rPrimerSize = strlen(rPrimer);
struct gfClump *clumpList, *clump;
int clumpCount = 0;
char buf[256];

clumpList = gfPcrClumps(gf, fPrimer, fPrimerSize, rPrimer, rPrimerSize, 0, maxDistance);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct gfSeqSource *ss = clump->target;
    safef(buf, sizeof(buf), "%s\t%d\t%d\t+", ss->fileName, 
        clump->tStart, clump->tEnd);
    netSendString(connectionHandle, buf);
    ++clumpCount;
    }
gfClumpFreeList(&clumpList);

clumpList = gfPcrClumps(gf, rPrimer, rPrimerSize, fPrimer, fPrimerSize, 0, maxDistance);

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct gfSeqSource *ss = clump->target;
    safef(buf, sizeof(buf), "%s\t%d\t%d\t-", ss->fileName, 
        clump->tStart, clump->tEnd);
    netSendString(connectionHandle, buf);
    ++clumpCount;
    }
gfClumpFreeList(&clumpList);
netSendString(connectionHandle, "end");
logDebug("%lu PCR %s %s %d clumps\n", clock1000(), fPrimer, rPrimer, clumpCount);
}


static jmp_buf gfRecover;
static char *ripCord = NULL;	/* A little memory to give back to system
                                 * during error recovery. */

static void gfAbort()
/* Abort query. */
{
freez(&ripCord);
longjmp(gfRecover, -1);
}

static void errorSafeSetup()
/* Start up error safe stuff. */
{
pushAbortHandler(gfAbort); // must come before memTracker
memTrackerStart();
ripCord = needMem(64*1024); /* Memory for error recovery. memTrackerEnd frees */
}

static void errorSafeCleanup()
/* Clean up and report problem. */
{
memTrackerEnd();
popAbortHandler();  // must come after memTracker
}

static void errorSafeCleanupMess(int connectionHandle, char *message)
/* Clean up and report problem. */
{
errorSafeCleanup();
logError("Recovering from error via longjmp");
netSendString(connectionHandle, message);
}

static void errorSafeQuery(boolean doTrans, boolean queryIsProt, 
	struct dnaSeq *seq, struct genoFind *gf, struct genoFind *transGf[2][3], 
	int connectionHandle, char *buf)
/* Wrap error handling code around index query. */
{
int status;
errorSafeSetup();
status = setjmp(gfRecover);
if (status == 0)    /* Always true except after long jump. */
    {
    if (doTrans)
       {
       if (queryIsProt)
	    transQuery(transGf, seq, connectionHandle, buf);
       else
	    transTransQuery(transGf, seq, 
		connectionHandle, buf);
       }
    else
	dnaQuery(gf, seq, connectionHandle, buf);
    errorSafeCleanup();
    }
else    /* They long jumped here because of an error. */
    {
    errorSafeCleanupMess(connectionHandle, 
    	"Error: gfServer out of memory. Try reducing size of query.");
    }
}

static void errorSafePcr(struct genoFind *gf, char *fPrimer, char *rPrimer, 
	int maxDistance, int connectionHandle)
/* Wrap error handling around pcr index query. */
{
int status;
errorSafeSetup();
status = setjmp(gfRecover);
if (status == 0)    /* Always true except after long jump. */
    {
    pcrQuery(gf, fPrimer, rPrimer, maxDistance, connectionHandle);
    errorSafeCleanup();
    }
else    /* They long jumped here because of an error. */
    {
    errorSafeCleanupMess(connectionHandle, 
    	"Error: gfServer out of memory."); 
    }
}

boolean badPcrPrimerSeq(char *s)
/* Return TRUE if have a character we can't handle in sequence. */
{
unsigned char c;
while ((c = *s++) != 0)
    {
    if (ntVal[c] < 0)
        return TRUE;
    }
return FALSE;
}

void startServer(char *hostName, char *portName, int fileCount, 
	char *seqFiles[])
/* Load up index and hang out in RAM. */
{
struct genoFind *gf = NULL;
static struct genoFind *transGf[2][3];
char buf[256];
char *line, *command;
struct sockaddr_in fromAddr;
socklen_t fromLen;
int readSize;
int socketHandle = 0, connectionHandle = 0;
int port = atoi(portName);
time_t curtime;
struct tm *loctime;
char timestr[256];

netBlockBrokenPipes();

curtime = time (NULL);           /* Get the current time. */
loctime = localtime (&curtime);  /* Convert it to local time representation. */
strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M", loctime); /* formate datetime as string */
								
logInfo("gfServer version %s on host %s, port %s  (%s)", gfVersion, 
	hostName, portName, timestr);
if (doTrans)
    {
    uglyf("starting translated server...\n");
    logInfo("setting up translated index");
    gfIndexTransNibsAndTwoBits(transGf, fileCount, seqFiles, 
    	minMatch, maxGap, tileSize, repMatch, NULL, allowOneMismatch, 
	doMask, stepSize);
    }
else
    {
    uglyf("starting untranslated server...\n");
    logInfo("setting up untranslated index");
    gf = gfIndexNibsAndTwoBits(fileCount, seqFiles, minMatch, 
    	maxGap, tileSize, repMatch, NULL, allowOneMismatch, stepSize);
    }
logInfo("indexing complete");

/* Set up socket.  Get ready to listen to it. */
socketHandle = netAcceptingSocket(port, 100);
if (socketHandle < 0)
    errAbort("Fatal Error: Unable to open listening socket on port %d.", port);

logInfo("Server ready for queries!");
printf("Server ready for queries!\n");
int connectFailCount = 0;
for (;;)
    {
    ZeroVar(&fromAddr);
    fromLen = sizeof(fromAddr);
    connectionHandle = accept(socketHandle, (struct sockaddr*)&fromAddr, &fromLen);
    if (connectionHandle < 0)
        {
	warn("Error accepting the connection");
	++warnCount;
        ++connectFailCount;
        if (connectFailCount >= 100)
	    errAbort("100 continuous connection failures, no point in filling up the log in an infinite loop.");
	continue;
	}
    else
	{
	connectFailCount = 0;
	}
    if (ipLog)
	{
        if (fromAddr.sin_family == AF_INET)
	    {
	    char dottedQuad[17];
	    internetIpToDottedQuad(ntohl(fromAddr.sin_addr.s_addr), dottedQuad);
	    logInfo("gfServer version %s on host %s, port %s connection from %s", 
		gfVersion, hostName, portName, dottedQuad);
	    }
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
	if (canStop)
	    break;
	else
	    logError("Ignoring quit message");
	}
    else if (sameString("status", command))
        {
	sprintf(buf, "version %s", gfVersion);
	netSendString(connectionHandle, buf);
	sprintf(buf, "type %s", (doTrans ? "translated" : "nucleotide"));
	netSendString(connectionHandle, buf);
	sprintf(buf, "host %s", hostName);
	netSendString(connectionHandle, buf);
	sprintf(buf, "port %s", portName);
	netSendString(connectionHandle, buf);
	sprintf(buf, "tileSize %d", tileSize);
	netSendString(connectionHandle, buf);
	sprintf(buf, "stepSize %d", stepSize);
	netSendString(connectionHandle, buf);
	sprintf(buf, "minMatch %d", minMatch);
	netSendString(connectionHandle, buf);
	sprintf(buf, "pcr requests %ld", pcrCount);
	netSendString(connectionHandle, buf);
	sprintf(buf, "blat requests %ld", blatCount);
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
            ZeroVar(&seq);

	    if (queryIsProt && !doTrans)
	        {
		warn("protein query sent to nucleotide server");
		++warnCount;
		queryIsProt = FALSE;
		}
	    else
		{
		buf[0] = 'Y';
		if (write(connectionHandle, buf, 1) == 1)
		    {
		    seq.size = atoi(s);
		    seq.name = NULL;
		    if (seq.size > 0)
			{
			++blatCount;
			seq.dna = needLargeMem(seq.size+1);
			if (gfReadMulti(connectionHandle, seq.dna, seq.size) != seq.size)
			    {
			    warn("Didn't sockRecieveString all %d bytes of query sequence", seq.size);
			    ++warnCount;
			    }
			else
			    {
			    int maxSize = (doTrans ? maxAaSize : maxNtSize);

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
                            if (seqLog && (logGetFile() != NULL))
				{
                                FILE *lf = logGetFile();
                                faWriteNext(lf, "query", seq.dna, seq.size);
				fflush(lf);
				}
			    errorSafeQuery(doTrans, queryIsProt, &seq, gf, 
				    transGf, connectionHandle, buf);
			    }
			freez(&seq.dna);
			}
		    netSendString(connectionHandle, "end");
		    }
		}
	    }
	}
    else if (sameString("pcr", command))
        {
	char *f = nextWord(&line);
	char *r = nextWord(&line);
	char *s = nextWord(&line);
	int maxDistance;
	++pcrCount;
	if (s == NULL || !isdigit(s[0]))
	    {
	    warn("Badly formatted pcr command");
	    ++warnCount;
	    }
	else if (doTrans)
	    {
	    warn("Can't pcr on translated server");
	    ++warnCount;
	    }
	else if (badPcrPrimerSeq(f) || badPcrPrimerSeq(r))
	    {
	    warn("Can only handle ACGT in primer sequences.");
	    ++warnCount;
	    }
	else
	    {
	    maxDistance = atoi(s);
	    errorSafePcr(gf, f, r, maxDistance, connectionHandle);
	    }
	}
    else if (sameString("files", command))
        {
	int i;
	sprintf(buf, "%d", fileCount);
	netSendString(connectionHandle, buf);
	for (i=0; i<fileCount; ++i)
	    {
	    sprintf(buf, "%s", seqFiles[i]);
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
mustWriteFd(sd, buf, strlen(buf));
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
sprintf(buf, "%sstatus", gfSignature());
mustWriteFd(sd, buf, strlen(buf));

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

void queryServer(char *type, 
	char *hostName, char *portName, char *faName, boolean complex, boolean isProt)
/* Send simple query to server and report results. */
{
char buf[256];
int sd = 0;
bioSeq *seq = faReadSeq(faName, !isProt);
int matchCount = 0;

/* Put together query command. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%s%s %d", gfSignature(), type, seq->size);
mustWriteFd(sd, buf, strlen(buf));

if (read(sd, buf, 1) < 0)
    errAbort("queryServer: read failed: %s", strerror(errno));
if (buf[0] != 'Y')
    errAbort("Expecting 'Y' from server, got %c", buf[0]);
mustWriteFd(sd, seq->dna, seq->size);

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
    else if (startsWith("Error:", buf))
       {
       errAbort("%s", buf);
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

void pcrServer(char *hostName, char *portName, char *fPrimer, char *rPrimer, int maxSize)
/* Do a PCR query to server daemon. */
{
char buf[256];
int sd = 0;

/* Put together query command and send. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%spcr %s %s %d", gfSignature(), fPrimer, rPrimer, maxSize);
mustWriteFd(sd, buf, strlen(buf));

/* Fetch and display results. */
for (;;)
    {
    if (netGetString(sd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
	break;
    else if (startsWith("Error:", buf))
	{
	errAbort("%s", buf);
	break;
	}
    else
	{
        printf("%s\n", buf);
	}
    }
close(sd);
}


void getFileList(char *hostName, char *portName)
/* Get and display input file list. */
{
char buf[256];
int sd = 0;
int fileCount;
int i;

/* Put together command. */
sd = netMustConnectTo(hostName, portName);
sprintf(buf, "%sfiles", gfSignature());
mustWriteFd(sd, buf, strlen(buf));

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
dnaUtilOpen();
optionInit(&argc, argv, optionSpecs);
command = argv[1];
if (optionExists("trans"))
    {
    doTrans = TRUE;
    tileSize = 4;
    minMatch = 3;
    maxGap = 0;
    repMatch = gfPepMaxTileUse;
    }
tileSize = optionInt("tileSize", tileSize);
stepSize = optionInt("stepSize", tileSize);
if (optionExists("repMatch"))
    repMatch = optionInt("repMatch", 0);
else
    repMatch = gfDefaultRepMatch(tileSize, stepSize, doTrans);
minMatch = optionInt("minMatch", minMatch);
maxDnaHits = optionInt("maxDnaHits", maxDnaHits);
maxTransHits = optionInt("maxTransHits", maxTransHits);
maxNtSize = optionInt("maxNtSize", maxNtSize);
maxAaSize = optionInt("maxAaSize", maxAaSize);
seqLog = optionExists("seqLog");
ipLog = optionExists("ipLog");
doMask = optionExists("mask");
canStop = optionExists("canStop");
if (argc < 2)
    usage();
if (optionExists("log"))
    logOpenFile(argv[0], optionVal("log", NULL));
if (optionExists("syslog"))
    logOpenSyslog(argv[0], optionVal("logFacility", NULL));

if (sameWord(command, "direct"))
    {
    if (argc < 4)
        usage();
    genoFindDirect(argv[2], argc-3, argv+3);
    }
else if (sameWord(command, "pcrDirect"))
    {
    if (argc < 5)
        usage();
    genoPcrDirect(argv[2], argv[3], argc-4, argv+4);
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
else if (sameWord(command, "pcr"))
    {
    if (argc != 7)
        usage();
    pcrServer(argv[2], argv[3], argv[4], argv[5], atoi(argv[6]));
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
