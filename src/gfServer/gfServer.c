/* gfServer - set up an index of the genome in memory and
 * respond to search requests. */
/* Copyright 2001-2003 Jim Kent.  All rights reserved. */
#include "common.h"
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "portable.h"
#include "filePath.h"
#include "net.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "fa.h"
#include "dystring.h"
#include "errAbort.h"
#include "memalloc.h"
#include "genoFind.h"
#include "options.h"
#include "trans3.h"
#include "log.h"
#include "internet.h"
#include "hash.h"


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
    {"debugLog", OPTION_BOOLEAN},
    {"stepSize", OPTION_INT},
    {"tileSize", OPTION_INT},
    {"trans", OPTION_BOOLEAN},
    {"syslog", OPTION_BOOLEAN},
    {"perSeqMax", OPTION_STRING},
    {"noSimpRepMask", OPTION_BOOLEAN},
    {"indexFile", OPTION_STRING},
    {"genome", OPTION_STRING},
    {"genomeDataDir", OPTION_STRING},
    {"timeout", OPTION_INT},
    {NULL, 0}
};


int maxNtSize = 40000;
int maxAaSize = 8000;

int minMatch = gfMinMatch;	/* Can be overridden from command line. */
int tileSize = gfTileSize;	/* Can be overridden from command line. */
int stepSize = 0;		/* Can be overridden from command line. */
boolean doTrans = FALSE;	/* Do translation? */
boolean allowOneMismatch = FALSE; 
boolean noSimpRepMask = FALSE;
int repMatch = 1024;    /* Can be overridden from command line. */
int maxDnaHits = 100;   /* Can be overridden from command line. */
int maxTransHits = 200; /* Can be overridden from command line. */
int maxGap = gfMaxGap;
boolean seqLog = FALSE;
boolean ipLog = FALSE;
boolean doMask = FALSE;
boolean canStop = FALSE;
char *indexFile = NULL;
char *genome = NULL;
char *genomeDataDir = NULL;

int timeout = 90;  // default timeout in seconds


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfServer v %s - Make a server to quickly find where DNA occurs in genome\n"
  "   To set up a server:\n"
  "      gfServer start host port file(s)\n"
  "      where the files are .2bit or .nib format files specified relative to the current directory\n"
  "   To remove a server:\n"
  "      gfServer stop host port\n"
  "   To query a server with DNA sequence:\n"
  "      gfServer query host port probe.fa\n"
  "   To query a server with protein sequence:\n"
  "      gfServer protQuery host port probe.fa\n"
  "   To query a server with translated DNA sequence:\n"
  "      gfServer transQuery host port probe.fa\n"
  "   To query server with PCR primers:\n"
  "      gfServer pcr host port fPrimer rPrimer maxDistance\n"
  "   To process one probe fa file against a .2bit format genome (not starting server):\n"
  "      gfServer direct probe.fa file(s).2bit\n"
  "   To test PCR without starting server:\n"
  "      gfServer pcrDirect fPrimer rPrimer file(s).2bit\n"
  "   To figure out if server is alive, on static instances get usage statics as well:\n"
  "      gfServer status host port\n"
  "     For dynamic gfServer instances, specify -genome and optionally the -genomeDataDir\n"
  "     to get information on an untranslated genome index. Include -trans to get about information\n"
  "     about a translated genome index\n"
  "   To get input file list:\n"
  "      gfServer files host port\n"
  "   To generate a precomputed index:\n"
  "      gfServer index gfidx file(s)\n"
  "     where the files are .2bit or .nib format files.  Separate indexes are\n"
  "     be created for untranslated and translated queries.  These can be used\n"
  "     with a persistent server as with 'start -indexFile or a dynamic server.\n"
  "     They must follow the naming convention for for dynamic servers.\n"
  "   To run a dynamic server (usually called by xinetd):\n"
  "      gfServer dynserver rootdir\n"
  "     Data files for genomes are found relative to the root directory.\n"
  "     Queries are made using the prefix of the file path relative to the root\n"
  "     directory.  The files $genome.2bit, $genome.untrans.gfidx, and\n"
  "     $genome.trans.gfidx are required. Typically the structure will be in\n"
  "     the form:\n"
  "         $rootdir/$genomeDataDir/$genome.2bit\n"
  "         $rootdir/$genomeDataDir/$genome.untrans.gfidx\n"
  "         $rootdir/$genomeDataDir/$genome.trans.gfidx\n"
  "     in this case, one would call gfClient with \n"
  "         -genome=$genome -genomeDataDir=$genomeDataDir\n"
  "     Often $genomeDataDir will be the same name as $genome, however it\n"
  "     can be a multi-level path. For instance:\n"
  "          GCA/902/686/455/GCA_902686455.1_mSciVul1.1/\n"
  "     The translated or untranslated index maybe omitted if there is no\n"
  "     need to handle that type of request.\n"
  "     The -perSeqMax functionality can be implemented by creating a file\n"
  "         $rootdir/$genomeDataDir/$genome.perseqmax\n"
  "\n"
  "options:\n"
  "   -tileSize=N     Size of n-mers to index.  Default is 11 for nucleotides, 4 for\n"
  "                   proteins (or translated nucleotides).\n"
  "   -stepSize=N     Spacing between tiles. Default is tileSize.\n"
  "   -minMatch=N     Number of n-mer matches that trigger detailed alignment.\n"
  "                   Default is 2 for nucleotides, 3 for proteins.\n"
  "   -maxGap=N       Number of insertions or deletions allowed between n-mers.\n"
  "                   Default is 2 for nucleotides, 0 for proteins.\n"
  "   -trans          Translate database to protein in 6 frames.  Note: it is best\n"
  "                   to run this on RepeatMasked data in this case.\n"
  "   -log=logFile    Keep a log file that records server requests.\n"
  "   -seqLog         Include sequences in log file (not logged with -syslog).\n"
  "   -ipLog          Include user's IP in log file (not logged with -syslog).\n"
  "   -debugLog       Include debugging info in log file.\n"
  "   -syslog         Log to syslog.\n"
  "   -logFacility=facility  Log to the specified syslog facility - default local0.\n"
  "   -mask           Use masking from .2bit file.\n"
  "   -repMatch=N     Number of occurrences of a tile (n-mer) that triggers repeat masking the\n"
  "                   tile. Default is %d.\n"
  "   -noSimpRepMask  Suppresses simple repeat masking.\n"
  "   -maxDnaHits=N   Maximum number of hits for a DNA query that are sent from the server.\n"
  "                   Default is %d.\n"
  "   -maxTransHits=N Maximum number of hits for a translated query that are sent from the server.\n"
  "                   Default is %d.\n"
  "   -maxNtSize=N    Maximum size of untranslated DNA query sequence.\n"
  "                   Default is %d.\n"
  "   -maxAaSize=N    Maximum size of protein or translated DNA queries.\n"
  "                   Default is %d.\n"
  "   -perSeqMax=file File contains one seq filename (possibly with ':seq' suffix) per line.\n"
  "                   -maxDnaHits will be applied to each filename[:seq] separately: each may\n"
  "                   have at most maxDnaHits/2 hits.  The filename MUST not include the directory.\n"
  "                   Useful for assemblies with many alternate/patch sequences.\n"
  "   -canStop        If set, a quit message will actually take down the server.\n"
  "   -indexFile      Index file create by `gfServer index'. Saving index can speed up\n"
  "                   gfServer startup by two orders of magnitude.  The parameters must\n"
  "                   exactly match the parameters when the file is written or bad things\n"
  "                   will happen.\n"
  "   -timeout=N      Timeout in seconds.\n"
  "                   Default is %d.\n"
  ,	gfVersion, repMatch, maxDnaHits, maxTransHits, maxNtSize, maxAaSize, timeout
  );

}
/*
  Note about file(s) specified in the start command:
      The path(s) specified here are sent back exactly as-is
      to clients such as gfClient, hgBlat, webBlat.
      It is intended that relative paths are used.
      Absolute paths starting with '/' tend not to work
      unless the client is on the same machine as the server.
      For use with hgBlat and webBlat, cd to the directory where the file is
      and use the plain file name with no slashes.
        hgBlat will append the path(s) given to dbDb.nibPath.
       webBlat will append the path(s) given to path specified in webBlat.cfg.
      gfClient will append the path(s) given to the seqDir path specified.
*/

static void setSocketTimeout(int sockfd, int delayInSeconds)
// put socket read and write timeout so it will not take forever to timeout during a read or write
{
struct timeval tv;
tv.tv_sec = delayInSeconds;
tv.tv_usec = 0;
setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
}

static boolean sendOk = TRUE;

void setSendOk()
// Reset to OK to send
{
sendOk = TRUE;
}

void errSendString(int sd, char *s)
// Send string. If not OK, remember we had an error, do not try to write anything more on this connection.
{

if (sendOk) sendOk = netSendString(sd, s);
}

void errSendLongString(int sd, char *s)
// Send string unless we had an error already on the connection.
{
if (sendOk) sendOk = netSendLongString(sd, s);
}

void logGenoFind(struct genoFind *gf)
/* debug log the genoFind parameters */
{
logDebug("gf->isMapped: %d", gf->isMapped);
logDebug("gf->maxPat: %d", gf->maxPat);
logDebug("gf->minMatch: %d", gf->minMatch);
logDebug("gf->maxGap: %d", gf->maxGap);
logDebug("gf->tileSize: %d", gf->tileSize);
logDebug("gf->stepSize: %d", gf->stepSize);
logDebug("gf->tileSpaceSize: %d", gf->tileSpaceSize);
logDebug("gf->tileMask: %d", gf->tileMask);
logDebug("gf->sourceCount: %d", gf->sourceCount);
logDebug("gf->isPep: %d", gf->isPep);
logDebug("gf->allowOneMismatch: %d", gf->allowOneMismatch);
logDebug("gf->noSimpRepMask: %d", gf->noSimpRepMask);
logDebug("gf->segSize: %d", gf->segSize);
logDebug("gf->totalSeqSize: %d", gf->totalSeqSize);
}

void logGenoFindIndex(struct genoFindIndex *gfIdx)
/* debug log the genoFind parameters in an genoFindIndex */
{
logDebug("gfIdx->isTrans: %d", gfIdx->isTrans);
logDebug("gfIdx->noSimpRepMask: %d", gfIdx->noSimpRepMask);
if (gfIdx->untransGf != NULL)
    logGenoFind(gfIdx->untransGf);
else
    logGenoFind(gfIdx->transGf[0][0]);
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
	allowOneMismatch, stepSize, noSimpRepMask);

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
	allowOneMismatch, stepSize, noSimpRepMask);
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


/* Some variables to gather statistics on usage. */
long baseCount = 0, blatCount = 0, aaCount = 0, pcrCount = 0;
int warnCount = 0;
int noSigCount = 0;
int missCount = 0;
int trimCount = 0;

void dnaQuery(struct genoFind *gf, struct dnaSeq *seq, 
              int connectionHandle, struct hash *perSeqMaxHash)
/* Handle a query for DNA/DNA match. */
{
char buf[256];
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
    errSendString(connectionHandle, buf);
    ++clumpCount;
    int perSeqCount = -1;
    if (perSeqMaxHash &&
        ((perSeqCount = hashIntValDefault(perSeqMaxHash, ss->fileName, -1)) >= 0))
        {
        if (perSeqCount >= (maxDnaHits / 2))
            break;
        hashIncInt(perSeqMaxHash, ss->fileName);
        }
    else if (--limit < 0)
	break;
    }
gfClumpFreeList(&clumpList);
lmCleanup(&lm);
logDebug("%lu %d clumps, %d hits", clock1000(), clumpCount, hitCount);
}

void transQuery(struct genoFind *transGf[2][3], aaSeq *seq, 
	int connectionHandle)
/* Handle a query for protein/translated DNA match. */
{
char buf[256];
struct gfClump *clumps[3], *clump;
int isRc, frame;
char strand;
struct dyString *dy  = newDyString(1024);
struct gfHit *hit;
int clumpCount = 0, hitCount = 0, oneHit;
struct lm *lm = lmInit(0);

sprintf(buf, "tileSize %d", tileSize);
errSendString(connectionHandle, buf);
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
	    errSendString(connectionHandle, buf);
	    dyStringClear(dy);
	    for (hit = clump->hitList; hit != NULL; hit = hit->next)
	        dyStringPrintf(dy, " %d %d", hit->qStart, hit->tStart - ss->start);
	    errSendLongString(connectionHandle, dy->string);
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
	int connectionHandle)
/* Handle a query for protein/translated DNA match. */
{
char buf[256];
struct gfClump *clumps[3][3], *clump;
int isRc, qFrame, tFrame;
char strand;
struct trans3 *t3 = trans3New(seq);
struct dyString *dy  = newDyString(1024);
struct gfHit *hit;
int clumpCount = 0, hitCount = 0, oneCount;

sprintf(buf, "tileSize %d", tileSize);
errSendString(connectionHandle, buf);
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
		errSendString(connectionHandle, buf);
		dyStringClear(dy);
		for (hit = clump->hitList; hit != NULL; hit = hit->next)
		    {
		    dyStringPrintf(dy, " %d %d", hit->qStart, hit->tStart - ss->start);
		    }
		errSendLongString(connectionHandle, dy->string);
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
    errSendString(connectionHandle, buf);
    ++clumpCount;
    }
gfClumpFreeList(&clumpList);

clumpList = gfPcrClumps(gf, rPrimer, rPrimerSize, fPrimer, fPrimerSize, 0, maxDistance);

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct gfSeqSource *ss = clump->target;
    safef(buf, sizeof(buf), "%s\t%d\t%d\t-", ss->fileName, 
        clump->tStart, clump->tEnd);
    errSendString(connectionHandle, buf);
    ++clumpCount;
    }
gfClumpFreeList(&clumpList);
errSendString(connectionHandle, "end");
logDebug("%lu PCR %s %s %d clumps", clock1000(), fPrimer, rPrimer, clumpCount);
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
errSendString(connectionHandle, message);
}

static void errorSafeQuery(boolean doTrans, boolean queryIsProt, 
	struct dnaSeq *seq, struct genoFindIndex *gfIdx, 
	int connectionHandle, char *buf, struct hash *perSeqMaxHash)
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
	    transQuery(gfIdx->transGf, seq, connectionHandle);
       else
	    transTransQuery(gfIdx->transGf, seq, connectionHandle);
       }
    else
	dnaQuery(gfIdx->untransGf, seq, connectionHandle, perSeqMaxHash);
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

static boolean haveFileBaseName(char *baseName, int fileCount, char *seqFiles[])
/* check if the file list contains the base name of the per-seq max spec */
{
int i;
for (i = 0; i < fileCount; i++)
    if (sameString(findTail(seqFiles[i], '/'), baseName))
        return TRUE;
return FALSE;
}

static struct hash *buildPerSeqMax(int fileCount, char *seqFiles[], char* perSeqMaxFile)
/* do work of building perSeqMaxhash */
{
struct hash *perSeqMaxHash = hashNew(0);
struct lineFile *lf = lineFileOpen(perSeqMaxFile, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    // Make sure line contains a valid seq filename (before optional ':seq'), directories are ignored
    char *seqFile = findTail(trimSpaces(line), '/');
    char copy[strlen(seqFile)+1];
    safecpy(copy, sizeof copy, seqFile);
    char *colon = strrchr(copy, ':');
    if (colon)
        *colon = '\0';
    if (haveFileBaseName(copy, fileCount, seqFiles) < 0)
        lineFileAbort(lf, "'%s' does not appear to be a sequence file from the "
                      "command line", copy);
    hashAddInt(perSeqMaxHash, seqFile, 0);
    }
lineFileClose(&lf);
return perSeqMaxHash;
}
    

static struct hash *maybePerSeqMax(int fileCount, char *seqFiles[])
/* If options include -perSeqMax=file, then read the sequences named in the file into a hash
 * for testing membership in the set of sequences to exclude from -maxDnaHits accounting. */
{
char *fileName = optionVal("perSeqMax", NULL);
if (isNotEmpty(fileName))
    return buildPerSeqMax(fileCount, seqFiles, fileName);
else
    return NULL;
}

static void hashZeroVals(struct hash *hash)
/* Set the value of every element of hash to NULL (0 for ints). */
{
struct hashEl *hel;
struct hashCookie cookie = hashFirst(hash);
while ((hel = hashNext(&cookie)) != NULL)
    hel->val = 0;
}

void startServer(char *hostName, char *portName, int fileCount, 
	char *seqFiles[])
/* Load up index and hang out in RAM. */
{
struct genoFindIndex *gfIdx = NULL;
char buf[256];
char *line, *command;
struct sockaddr_in6 fromAddr;
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
struct hash *perSeqMaxHash = maybePerSeqMax(fileCount, seqFiles);

time_t startIndexTime = clock1000();
if (indexFile == NULL)
    {
    char *desc = doTrans ? "translated" : "untranslated";
    uglyf("starting %s server...\n", desc);
    logInfo("setting up %s index", desc);
    gfIdx = genoFindIndexBuild(fileCount, seqFiles, minMatch, maxGap, tileSize, repMatch, doTrans, NULL,
                               allowOneMismatch, doMask, stepSize, noSimpRepMask);
    logInfo("index building completed in %4.3f seconds", 0.001 * (clock1000() - startIndexTime));
    }
else
    {
    gfIdx = genoFindIndexLoad(indexFile, doTrans);
    logInfo("index loading completed in %4.3f seconds", 0.001 * (clock1000() - startIndexTime));
    }
logGenoFindIndex(gfIdx);

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
    setSendOk();
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
    setSocketTimeout(connectionHandle, timeout);
    if (ipLog)
	{
	struct sockaddr_in6 clientAddr;
	unsigned int addrlen=sizeof(clientAddr);
	getpeername(connectionHandle, (struct sockaddr *)&clientAddr, &addrlen);
	char ipStr[NI_MAXHOST];
	getAddrAsString6n4((struct sockaddr_storage *)&clientAddr, ipStr, sizeof ipStr);
	logInfo("gfServer version %s on host %s, port %s connection from %s", 
	    gfVersion, hostName, portName, ipStr);
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
    else if (sameString("status", command) || sameString("transInfo", command)
             || sameString("untransInfo", command))
        {
	sprintf(buf, "version %s", gfVersion);
	errSendString(connectionHandle, buf);
        errSendString(connectionHandle, "serverType static");
	errSendString(connectionHandle, buf);
	sprintf(buf, "type %s", (doTrans ? "translated" : "nucleotide"));
	errSendString(connectionHandle, buf);
	sprintf(buf, "host %s", hostName);
	errSendString(connectionHandle, buf);
	sprintf(buf, "port %s", portName);
	errSendString(connectionHandle, buf);
	sprintf(buf, "tileSize %d", tileSize);
	errSendString(connectionHandle, buf);
	sprintf(buf, "stepSize %d", stepSize);
	errSendString(connectionHandle, buf);
	sprintf(buf, "minMatch %d", minMatch);
	errSendString(connectionHandle, buf);
	sprintf(buf, "pcr requests %ld", pcrCount);
	errSendString(connectionHandle, buf);
	sprintf(buf, "blat requests %ld", blatCount);
	errSendString(connectionHandle, buf);
	sprintf(buf, "bases %ld", baseCount);
	errSendString(connectionHandle, buf);
	if (doTrans)
	    {
	    sprintf(buf, "aa %ld", aaCount);
	    errSendString(connectionHandle, buf);
	    }
	sprintf(buf, "misses %d", missCount);
	errSendString(connectionHandle, buf);
	sprintf(buf, "noSig %d", noSigCount);
	errSendString(connectionHandle, buf);
	sprintf(buf, "trimmed %d", trimCount);
	errSendString(connectionHandle, buf);
	sprintf(buf, "warnings %d", warnCount);
	errSendString(connectionHandle, buf);
	errSendString(connectionHandle, "end");
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
			    errorSafeQuery(doTrans, queryIsProt, &seq, gfIdx, 
                                           connectionHandle, buf, perSeqMaxHash);
                            if (perSeqMaxHash)
                                hashZeroVals(perSeqMaxHash);
			    }
			freez(&seq.dna);
			}
		    errSendString(connectionHandle, "end");
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
	    errorSafePcr(gfIdx->untransGf, f, r, maxDistance, connectionHandle);
	    }
	}
    else if (sameString("files", command))
        {
	int i;
	sprintf(buf, "%d", fileCount);
	errSendString(connectionHandle, buf);
	for (i=0; i<fileCount; ++i)
	    {
	    sprintf(buf, "%s", seqFiles[i]);
	    errSendString(connectionHandle, buf);
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
if (genome == NULL)
    sprintf(buf, "%sstatus", gfSignature());
else
    sprintf(buf, "%s%s %s %s", gfSignature(), (doTrans ? "transInfo" : "untransInfo"),
            genome, genomeDataDir);

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

static void checkIndexFileName(char *gfxFile, char *seqFile)
/* validate that index matches conventions, as base name is stored in index */
{
char seqBaseName[FILENAME_LEN], seqExt[FILEEXT_LEN];
splitPath(seqFile, NULL, seqBaseName, seqExt);
if ((strlen(seqBaseName) == 0) || !sameString(seqExt, ".2bit"))
    errAbort("gfServer index requires a two-bit genome file with a base name of `myGenome.2bit`, got %s%s",
             seqBaseName, seqExt);

char gfxBaseName[FILENAME_LEN], gfxExt[FILEEXT_LEN];
splitPath(gfxFile, NULL, gfxBaseName, gfxExt);
if (!sameString(gfxExt, ".gfidx"))
    errAbort("gfServer index must have an file extension of '.gfidx', got %s", gfxExt);
char expectBaseName[FILENAME_LEN];
safef(expectBaseName, sizeof(expectBaseName), "%s.%s", seqBaseName,
      (doTrans ? "trans" : "untrans"));
if (!sameString(gfxBaseName, expectBaseName))
    errAbort("%s index file base name must be '%s.gfidx', got %s%s",
             (doTrans ? "translated" : "untranslated"), expectBaseName, gfxBaseName, gfxExt);
}

static void buildIndex(char *gfxFile, int fileCount, char *seqFiles[])
/* build pre-computed index for seqFiles and write to gfxFile */
{
if (fileCount > 1)
    errAbort("gfServer index only works with a single genome file");
checkIndexFileName(gfxFile, seqFiles[0]);

struct genoFindIndex *gfIdx = genoFindIndexBuild(fileCount, seqFiles, minMatch, maxGap, tileSize,
                                                 repMatch, doTrans, NULL, allowOneMismatch, doMask, stepSize,
                                                 noSimpRepMask);
genoFindIndexWrite(gfIdx, gfxFile);
}

static void dynWarnErrorVa(char* msg, va_list args)
/* warnHandler to log and send back an error response */
{
char buf[4096];
int msgLen = vsnprintf(buf, sizeof(buf) - 1, msg, args);
buf[msgLen] = '\0';
logError("%s", buf);
printf("Error: %s\n", buf);
}

struct dynSession
/* information on dynamic server connection session.  This is all data
 * currently cached.  If is not changed if the genome and query mode is the
 * same as the previous request.
 */
{
    boolean isTrans;              // translated 
    char genome[256];             // genome name
    struct hash *perSeqMaxHash;   // max hits per sequence
    struct genoFindIndex *gfIdx;  // index
};

static struct genoFindIndex *loadGfIndex(char *gfIdxFile, boolean isTrans)
/* load index and set globals from it */
{
struct genoFindIndex *gfIdx = genoFindIndexLoad(gfIdxFile, isTrans);
struct genoFind *gf = isTrans ? gfIdx->transGf[0][0] : gfIdx->untransGf;
minMatch = gf->minMatch;
maxGap = gf->maxGap;
tileSize = gf->tileSize;
noSimpRepMask = gf->noSimpRepMask;
allowOneMismatch = gf->allowOneMismatch;
stepSize = gf->stepSize;
logGenoFindIndex(gfIdx);
return gfIdx;
}

static void dynWarnHandler(char *format, va_list args)
/* log error warning and error message, along with printing */
{
logErrorVa(format, args);
vfprintf(stderr, format, args);
fputc('\n', stderr);
}

static void dynSessionInit(struct dynSession *dynSession, char *rootDir,
                           char *genome, char *genomeDataDir, boolean isTrans)
/* Initialize or reinitialize a dynSession object */
{
if ((!isSafeRelativePath(genome)) || (strchr(genome, '/') != NULL))
    errAbort("genome argument can't contain '/' or '..': %s", genome);
if (!isSafeRelativePath(genomeDataDir))
    errAbort("genomeDataDir argument must be a relative path without '..' elements: %s", genomeDataDir);

// will free current content if initialized
genoFindIndexFree(&dynSession->gfIdx);
hashFree(&dynSession->perSeqMaxHash);

time_t startTime = clock1000();
dynSession->isTrans = isTrans;
safecpy(dynSession->genome, sizeof(dynSession->genome), genome);

// construct path to sequence and index files
char seqFileDir[PATH_LEN];
safef(seqFileDir, sizeof(seqFileDir), "%s/%s", rootDir, genomeDataDir);
    
char seqFile[PATH_LEN];
safef(seqFile, PATH_LEN, "%s/%s.2bit", seqFileDir, genome);
if (!fileExists(seqFile))
    errAbort("sequence file for %s does not exist: %s", genome, seqFile);

char gfIdxFile[PATH_LEN];
safef(gfIdxFile, PATH_LEN, "%s/%s.%s.gfidx", seqFileDir, genome, isTrans ? "trans" : "untrans");
if (!fileExists(gfIdxFile))
    errAbort("gf index file for %s does not exist: %s", genome, gfIdxFile);
dynSession->gfIdx = loadGfIndex(gfIdxFile, isTrans);

char perSeqMaxFile[PATH_LEN];
safef(perSeqMaxFile, PATH_LEN, "%s/%s.perseqmax", seqFileDir, genome);
if (fileExists(perSeqMaxFile))
    {
    /* only the basename of the file is saved in the index */
    char *slash = strrchr(seqFile, '/');
    char *seqFiles[1] = {(slash != NULL) ? slash + 1 : seqFile};
    dynSession->perSeqMaxHash = buildPerSeqMax(1, seqFiles, perSeqMaxFile);
    }
logInfo("dynserver: index loading completed in %4.3f seconds", 0.001 * (clock1000() - startTime));
}

static char *dynReadCommand(char* rootDir)
/* read command and log, NULL if no more */
{
char buf[PATH_LEN];
int readSize = read(STDIN_FILENO, buf, sizeof(buf)-1);
if (readSize < 0)
    errAbort("EOF from client");
if (readSize == 0)
    return NULL;
buf[readSize] = '\0';
if (!startsWith(gfSignature(), buf))
    errAbort("query does not start with signature, got '%s'", buf);
char *cmd = cloneString(buf + strlen(gfSignature()));
logInfo("dynserver: %s", cmd);
return cmd;
}

static const int DYN_CMD_MAX_ARGS = 8;  // more than needed to check for junk

static int dynNextCommand(char* rootDir, struct dynSession *dynSession, char **args)
/* Read query request from stdin and (re)initialize session to match
 * parameters.  Return number of arguments or zero on EOF
 *
 * Commands are in the format:
 *  signature+command genome genomeDataDir [arg ...]
 *  signature+status
 */
{
char *cmdStr = dynReadCommand(rootDir);
if (cmdStr == NULL)
    return 0;

int numArgs = chopByWhite(cmdStr, args, DYN_CMD_MAX_ARGS);
if (numArgs == 0)
    errAbort("empty command");
if (sameWord(args[0], "status"))
    return numArgs;  // special case; does not use an index.

if (numArgs < 3)
    errAbort("expected at least 3 arguments for a dynamic server command");
boolean isTrans = sameString("protQuery", args[0]) || sameString("transQuery", args[0])
    || sameString("transInfo", args[0]);

// initialize session if new or changed
if ((dynSession->isTrans != isTrans) || (!sameString(dynSession->genome, args[1])))
    dynSessionInit(dynSession, rootDir, args[1], args[2], isTrans);
return numArgs;
}

static struct dnaSeq* dynReadQuerySeq(int qSize, boolean isTrans, boolean queryIsProt)
/* read the DNA sequence from the query, filtering junk  */
{
struct dnaSeq *seq;
AllocVar(seq);
seq->size = qSize;
seq->dna = needLargeMem(qSize+1);
if (gfReadMulti(STDIN_FILENO, seq->dna, qSize) != qSize)
    errAbort("read of %d bytes of query sequence failed", qSize);
seq->dna[qSize] = '\0';

if (queryIsProt)
    {
    seq->size = aaFilteredSize(seq->dna);
    aaFilter(seq->dna, seq->dna);
    }
else
    {
    seq->size = dnaFilteredSize(seq->dna);
    dnaFilter(seq->dna, seq->dna);
    }
int maxSize = (isTrans ? maxAaSize : maxNtSize);
if (seq->size > maxSize)
    {
    seq->size = maxSize;
    seq->dna[maxSize] = 0;
    }

return seq;
}

static void dynamicServerQuery(struct dynSession *dynSession, int numArgs, char **args)
/* handle search queries
 *
 *  signature+command genome genomeDataDir qsize
 */
{
struct genoFindIndex *gfIdx = dynSession->gfIdx;
if (numArgs != 4)
    errAbort("expected 4 words in %s command, got %d", args[0], numArgs);
int qSize = atoi(args[3]);

boolean queryIsProt = sameString(args[0], "protQuery");
mustWriteFd(STDOUT_FILENO, "Y", 1);
struct dnaSeq* seq = dynReadQuerySeq(qSize, gfIdx->isTrans, queryIsProt);
if (gfIdx->isTrans)
    {
    if (queryIsProt)
        transQuery(gfIdx->transGf, seq, STDOUT_FILENO);
    else
        transTransQuery(gfIdx->transGf, seq, STDOUT_FILENO);
    }
else
    {
    dnaQuery(gfIdx->untransGf, seq, STDOUT_FILENO, dynSession->perSeqMaxHash);
    }
netSendString(STDOUT_FILENO, "end");
}

static void dynamicServerInfo(struct dynSession *dynSession, int numArgs, char **args)
/* handle one of the info or status commands commands
 *
 *  signature+untransInfo genome genomeDataDir
 *  signature+transInfo genome genomeDataDir
 */
{
struct genoFindIndex *gfIdx = dynSession->gfIdx;
if (numArgs != 3)
    errAbort("expected 3 words in %s command, got %d", args[0], numArgs);

char buf[256];
struct genoFind *gf = gfIdx->isTrans ? gfIdx->transGf[0][0] : gfIdx->untransGf;
sprintf(buf, "version %s", gfVersion);
netSendString(STDOUT_FILENO, buf);
netSendString(STDOUT_FILENO, "serverType dynamic");
sprintf(buf, "type %s", (gfIdx->isTrans ? "translated" : "nucleotide"));
netSendString(STDOUT_FILENO, buf);
sprintf(buf, "tileSize %d", gf->tileSize);
netSendString(STDOUT_FILENO, buf);
sprintf(buf, "stepSize %d", gf->stepSize);
netSendString(STDOUT_FILENO, buf);
sprintf(buf, "minMatch %d", gf->minMatch);
netSendString(STDOUT_FILENO, buf);
netSendString(STDOUT_FILENO, "end");
}

static void dynamicServerStatus(int numArgs, char **args)
/* return enough information to indicate server is working without opening
 * a genome index.
 *
 *  signature+status
 */
{
if (numArgs != 1)
    errAbort("expected 1 word in %s command, got %d", args[0], numArgs);
char buf[256];
sprintf(buf, "version %s", gfVersion);
netSendString(STDOUT_FILENO, buf);
netSendString(STDOUT_FILENO, "serverType dynamic");
netSendString(STDOUT_FILENO, "end");
}

static void dynamicServerPcr(struct dynSession *dynSession, int numArgs, char **args)
/* Execute a PCR query
 *
 *  signature+command genome genomeDataDir forward reverse maxDistance
 */
{
struct genoFindIndex *gfIdx = dynSession->gfIdx;
if (numArgs != 6)
    errAbort("expected 6 words in %s command, got %d", args[0], numArgs);
char *fPrimer = args[3];
char *rPrimer = args[4];
int maxDistance = atoi(args[5]);
if (badPcrPrimerSeq(fPrimer) || badPcrPrimerSeq(rPrimer))
    errAbort("Can only handle ACGT in primer sequences.");
pcrQuery(gfIdx->untransGf, fPrimer, rPrimer, maxDistance, STDOUT_FILENO);
}

static bool dynamicServerCommand(char* rootDir, struct dynSession* dynSession)
/* Execute one command from stdin, (re)initializing session as needed */
{
time_t startTime = clock1000();
char *args[DYN_CMD_MAX_ARGS];
int numArgs = dynNextCommand(rootDir, dynSession, args);
if (numArgs == 0)
    return FALSE;
if (sameString("query", args[0]) || sameString("protQuery", args[0])
    || sameString("transQuery", args[0]))
    {
    dynamicServerQuery(dynSession, numArgs, args);
    }
else if (sameString("status", args[0]))
    {
    dynamicServerStatus(numArgs, args);
    }
else if (sameString("untransInfo", args[0]) || sameString("transInfo", args[0]))
    {
    dynamicServerInfo(dynSession, numArgs, args);
    }
else if (sameString("pcr", args[0]))
    {
    dynamicServerPcr(dynSession, numArgs, args);
    }
else
    errAbort("invalid command '%s'", args[0]);

logInfo("dynserver: %s completed in %4.3f seconds", args[0], 0.001 * (clock1000() - startTime));
freeMem(args[0]);
return TRUE;
}

static void dynamicServer(char* rootDir)
/* dynamic server for inetd. Read query from stdin, open index, query, respond, exit.
 * only one query at a time */
{
pushWarnHandler(dynWarnHandler);
logDebug("dynamicServer connect");
struct runTimes startTimes = getTimesInSeconds();

// make sure errors are logged
pushWarnHandler(dynWarnErrorVa);
struct dynSession dynSession;
ZeroVar(&dynSession);

while (dynamicServerCommand(rootDir, &dynSession))
    continue;

struct runTimes endTimes = getTimesInSeconds();
logInfo("dynserver: exit: clock: %0.4f user: %0.4f system: %0.4f (seconds)",
        endTimes.clockSecs - startTimes.clockSecs,
        endTimes.userSecs - startTimes.userSecs,
        endTimes.sysSecs - startTimes.sysSecs);

logDebug("dynamicServer disconnect");
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
noSimpRepMask = optionExists("noSimpRepMask");
indexFile = optionVal("indexFile", NULL);
genome = optionVal("genome", NULL);
genomeDataDir = optionVal("genomeDataDir", NULL);
if ((genomeDataDir != NULL) && (genome == NULL))
    errAbort("-genomeDataDir requires the -genome option");
if ((genome != NULL) && (genomeDataDir == NULL))
    genomeDataDir = ".";
timeout = optionInt("timeout", timeout);
if (argc < 2)
    usage();
if (optionExists("log"))
    logOpenFile(argv[0], optionVal("log", NULL));
if (optionExists("syslog"))
    logOpenSyslog(argv[0], optionVal("logFacility", NULL));
if (optionExists("debugLog"))
    logSetMinPriority("debug");

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
else if (sameWord(command, "index"))
    {
    if (argc < 4)
        usage();
    buildIndex(argv[2], argc-3, argv+3);
    }
else if (sameWord(command, "dynserver"))
    {
    if (argc < 3)
        usage();
    dynamicServer(argv[2]);
    }
else
    {
    usage();
    }
return 0;
}
