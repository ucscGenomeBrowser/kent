/* bedItemOverlapCount - count how many times a base is overlapped in a
	bed file */
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

static char const rcsid[] = "$Id: bedItemOverlapCount.c,v 1.18 2009/09/29 18:15:31 galt Exp $";

/* define unitSize to be a larger storage class if your counts
 * are overflowing. */
typedef unsigned int unitSize;

#define MAXCOUNT (unitSize)~0
#define MAXMESSAGE "Overflow of overlap counts. Max is %lu.  Recompile with bigger unitSize or use -max option"
#define INCWOVERFLOW(x) if(counts[x] == MAXCOUNT) {if(!doMax) errAbort(MAXMESSAGE,(unsigned long)MAXCOUNT);} else counts[x]++

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
  "or all the way to wiggle track data:\n"
  " sort bedFile.bed | bedItemOverlapCount [options] <database> stdin \\\n"
  "         | wigEncode stdin data.wig data.wib\n"
  "options:\n"
  "   -zero      add blocks with zero count, normally these are ommitted\n"
  "   -bed12     expect bed12 and count based on blocks\n"
  "              normally only first three fields are used.\n"
  "   -max       if counts per base overflows set to max (%lu) instead of exiting\n"
  "   -outBounds ouput min/max to stderr\n"
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
  "   check of the incoming data (unless you use -chromSize argument).\n\n"
  " * The bed file *must* be sorted by chrom\n"
  " * Maximum count per base is %lu. Recompile with new unitSize to increase this\n",(unsigned long)MAXCOUNT, (unsigned long)MAXCOUNT
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

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    if (el->size > max) max = el->size;
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)el->size);
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
unitSize *start = counts;
unitSize *lastCount = &counts[size];
unitSize *ptr;

for(ptr=counts; ptr < lastCount - 1; ptr++)
    {
    if (doOutBounds)
	{
	if (ptr[1] < overMin)
	    overMin = ptr[1];
	if (ptr[1] > overMax)
	    overMax = ptr[1];
	}
    if (ptr[0] != ptr[1])
	{
	if (doZero || (ptr[0] != 0))
	    printf("%s\t%lu\t%lu\t%lu\n", 
                chrom, (unsigned long)(start - counts), (unsigned long)(ptr - counts + 1), (unsigned long)ptr[0]);
	start = ptr + 1;
	}
    }

if (doZero || (ptr[0] != 0))
    printf("%s\t%lu\t%lu\t%lu\n", 
	chrom, (unsigned long)(start - counts), (unsigned long)(ptr - counts + 1), (unsigned long)ptr[0]);

}

static void bedItemOverlapCount(char *database, int fileCount, char *fileList[])
{
unsigned maxChromSize = 0;
int i;
unitSize *counts = (unitSize)NULL;

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
		INCWOVERFLOW(i);
	    for (i = 0; i < (bed->chromEnd - chromSize); ++i)
		INCWOVERFLOW(i);
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
		    INCWOVERFLOW(i);
		}
	    }
	else
	    {
	    for (i = bed->chromStart; i < bed->chromEnd; ++i)
		INCWOVERFLOW(i);
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

if (argc < 2)
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
