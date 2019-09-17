/* bedItemOverlapCount - count how many times a base is overlapped in a
	bed file */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "chromInfo.h"
#include "wiggle.h"
#include "hdb.h"


/* define unitSize to be a larger storage class if your counts
 * are overflowing. */
typedef unsigned int unitSize;

#define MAXCOUNT (unitSize)~0
#define MAXMESSAGE "Overflow of overlap counts. Max is %lu.  Recompile with bigger unitSize or use -max option"
#define INCWOVERFLOW(countArray,x) if(countArray[x] == MAXCOUNT) {if(!doMax) errAbort(MAXMESSAGE,(unsigned long)MAXCOUNT);} else countArray[x]++

/* Command line switches. */
static struct hash *chromHash = NULL;
static char *host = NULL;
static char *user = NULL;
static char *password = NULL;
char *chromSizes = NULL;  /* read chrom sizes from file instead of database . */
boolean doMax;   /* if overlap count will overflow, just keep max */
boolean doZero;  /* add blocks with 0 counts */
boolean doBed12;  /* expect bed12 and process block by block */
boolean doOutBounds;  /* output min/max to stderr */
unitSize overMin = ~1;
unitSize overMax = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"chromSize", OPTION_STRING},
    {"host", OPTION_STRING},
    {"user", OPTION_STRING},
    {"password", OPTION_STRING},
    {"max", OPTION_BOOLEAN},
    {"zero", OPTION_BOOLEAN},
    {"bed12", OPTION_BOOLEAN},
    {"outBounds", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedItemOverlapCount - count number of times a base is overlapped by the\n"
  "\titems in a bed file.  Output is bedGraph 4 to stdout.\n"
  "usage:\n"
  " sort bedFile.bed | bedItemOverlapCount [options] <database> stdin\n"
  "To create a bigWig file from this data to use in a custom track:\n"
  " sort -k1,1 bedFile.bed | bedItemOverlapCount [options] <database> stdin \\\n"
  "         > bedFile.bedGraph\n"
  " bedGraphToBigWig bedFile.bedGraph chrom.sizes bedFile.bw\n"
  "   where the chrom.sizes is obtained with the script: fetchChromSizes\n"
  "   See also:\n"
  " http://genome-test.gi.ucsc.edu/~kent/src/unzipped/utils/userApps/fetchChromSizes\n"
  "options:\n"
  "   -zero      add blocks with zero count, normally these are ommitted\n"
  "   -bed12     expect bed12 and count based on blocks\n"
  "              Without this option, only the first three fields are used.\n"
  "   -max       if counts per base overflows set to max (%lu) instead of exiting\n"
  "   -outBounds output min/max to stderr\n"
  "   -chromSize=sizefile\tRead chrom sizes from file instead of database\n"
  "             sizefile contains two white space separated fields per line:\n"
  "		chrom name and size\n"
  "   -host=hostname\tmysql host used to get chrom sizes\n"
  "   -user=username\tmysql user\n"
  "   -password=password\tmysql password\n\n"
  "Notes:\n"
  " * You may want to separate your + and - strand\n"
  "   items before sending into this program as it only looks at\n"
  "   the chrom, start and end columns of the bed file.\n"
  " * Program requires a <database> connection to lookup chrom sizes for a sanity\n"
  "   check of the incoming data.  Even when the -chromSize argument is used\n"
  "   the <database> must be present, but it will not be used.\n\n"
  " * The bed file *must* be sorted by chrom\n"
  " * Maximum count per base is %lu. Recompile with new unitSize to increase this",(unsigned long)MAXCOUNT, (unsigned long)MAXCOUNT
  );
}

static struct hash *loadAllChromInfo(char *database, unsigned *largest)
/* Load up all chromosome infos, return largest one if asked to do so. */
{
struct chromInfo *el;
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;
unsigned max = 0;

if(host)
    {
    conn = sqlConnectRemote(host, user, password, database);
    }
else
    {
    conn = sqlConnect(database);
    }

ret = newHash(0);

sr = sqlGetResult(conn, NOSQLINJ "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    if (el->size > max) max = el->size;
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
if (largest) *largest = max;
return ret;
}

static unsigned chromosomeSize(char *chromosome)
/* Return full extents of chromosome.  Warn and fill in if none. */
{
struct hashEl *el = hashLookup(chromHash,chromosome);

if (el == NULL)
    errAbort("Couldn't find size of chromosome %s", chromosome);
return *(unsigned *)el->val;
}

static void outputCounts(unitSize *counts, char *chrom, unsigned size)
{
if (size == 0)
    errAbort("got 0 for size of chrom %s\n", chrom);

if (doOutBounds)
    {
    if (counts[0] < overMin)
	overMin = counts[0];
    if (counts[0] > overMax)
	overMax = counts[0];
    }

int ii;
int prevValue = counts[0];
int startPoint = 0;
for(ii=1; ii < size; ii++)
    {
    if (doOutBounds)
	{
	if (counts[ii] < overMin)
	    overMin = counts[ii];
	if (counts[ii] > overMax)
	    overMax = counts[ii];
	}
    if (counts[ii] != prevValue)
	{
	if (doZero || (prevValue != 0))
	    printf("%s\t%u\t%u\t%u\n", chrom, startPoint, ii, prevValue);
	startPoint = ii;
	prevValue = counts[ii];
	}
    }

if (doZero || (prevValue != 0))
    printf("%s\t%u\t%u\t%u\n", chrom, startPoint, ii, prevValue);
}

static void bedItemOverlapCount(char *database, int fileCount, char *fileList[])
{
unsigned maxChromSize = 0;
int i;
unitSize *counts = (unitSize *)NULL;

if (chromSizes != NULL)
    {
    chromHash = newHash(0);
    struct lineFile *lf = lineFileOpen(chromSizes, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
        unsigned *ptr;
        AllocVar(ptr);
        *ptr = sqlUnsigned(row[1]);
        maxChromSize = max(*ptr, maxChromSize);
        hashAdd(chromHash, row[0], ptr);
        }
    lineFileClose(&lf);
    }
else
    {
    chromHash = loadAllChromInfo(database, &maxChromSize);
    }

verbose(2,"#\tmaxChromSize: %u\n", maxChromSize);
if (maxChromSize < 1)
    errAbort("maxChromSize is zero ?");

/*	Allocate just once for the largest chrom and reuse this array */
counts = needHugeMem(sizeof(unitSize) * maxChromSize);

/*	Reset the array to be zero to be reused */
memset((void *)counts, 0, sizeof(unitSize)*(size_t)maxChromSize);

unsigned chromSize = 0;
char *prevChrom = (char *)NULL;
boolean outputToDo = FALSE;
struct hash *seenHash = newHash(5);

for (i=0; i<fileCount; ++i)
    {
    struct lineFile *bf = lineFileOpen(fileList[i] , TRUE);
    struct bed *bed = (struct bed *)NULL;
    char *row[12];
    int numFields = doBed12 ? 12 : 3;

    while (lineFileNextRow(bf,row, numFields))
	{
	int i;
	bed = bedLoadN(row, numFields);

	verbose(3,"#\t%s\t%d\t%d\n",bed->chrom,bed->chromStart, bed->chromEnd);

	if (prevChrom && differentWord(bed->chrom,prevChrom)) // End a chr
	    {
	    verbose(2,"#\tchrom %s done, size %d\n", prevChrom, chromSize);
	    if (outputToDo)
		outputCounts(counts, prevChrom, chromSize);
	    outputToDo = FALSE;
	    memset((void *)counts, 0,
		sizeof(unitSize)*(size_t)maxChromSize); /* zero counts */
	    freez(&prevChrom); 
	    // prevChrom is now NULL so it will be caught by next if!
	    }
	if ((char *)NULL == prevChrom)  // begin a chr
	    {
	    if (hashLookup(seenHash, bed->chrom))
		errAbort("ERROR:input file not sorted. %s seen before on line %d\n",
		    bed->chrom, bf->lineIx);

	    hashAdd(seenHash, bed->chrom, NULL);
	    prevChrom = cloneString(bed->chrom);
	    chromSize = chromosomeSize(prevChrom);
	    verbose(2,"#\tchrom %s starting, size %d\n", prevChrom,chromSize);
	    }
	if (bed->chromEnd > chromSize)
	    {
	    // check for circular chrM
	    if (doBed12 || bed->chromStart>=chromSize 
		|| differentWord(bed->chrom,"chrM")) 
		{
		warn("ERROR: %s\t%d\t%d", bed->chrom, bed->chromStart,
		bed->chromEnd);
		errAbort("chromEnd > chromSize ?  %d > %d", 
		    bed->chromEnd,chromSize);
		}

	    for (i = bed->chromStart; i < chromSize; ++i)
		INCWOVERFLOW(counts,i);
	    for (i = 0; i < (bed->chromEnd - chromSize); ++i)
		INCWOVERFLOW(counts,i);
	    }
	else if (doBed12)
	    {
	    int *starts = bed->chromStarts;
	    int *sizes = bed->blockSizes;
	    int *endStarts = &bed->chromStarts[bed->blockCount];

	    for(; starts < endStarts; starts++, sizes++)
		{
		unsigned int end = *starts + *sizes + bed->chromStart;
		for (i = *starts + bed->chromStart; i < end; ++i)
		    INCWOVERFLOW(counts,i);
		}
	    }
	else
	    {
	    for (i = bed->chromStart; i < bed->chromEnd; ++i)
		INCWOVERFLOW(counts, i);
	    }
	outputToDo = TRUE;
	bedFree(&bed); // plug the memory leak
	}

    lineFileClose(&bf);
    // Note, next file could be on same chr!
    }

if (outputToDo)
    outputCounts(counts, prevChrom, chromSize);

if (doOutBounds)
    fprintf(stderr, "min %lu max %lu\n", (unsigned long)overMin, (unsigned long)overMax);

verbose(2,"#\tchrom %s done, size %d\n", prevChrom, chromSize);
freeMem(counts);
freez(&prevChrom);
// hashFreeWithVals(&chromHash, freez);
freeHash(&seenHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

if (argc < 3)
    usage();
host = optionVal("host", NULL);
user = optionVal("user", NULL);
password = optionVal("password", NULL);
chromSizes = optionVal("chromSize", NULL);
doMax = optionExists("max");
doBed12 = optionExists("bed12");
doZero = optionExists("zero");
doOutBounds = optionExists("outBounds");
verbose(2, "#\tworking on database: %s\n", argv[1]);
bedItemOverlapCount(argv[1], argc-2, argv+2);
optionFree();
return 0;
}
