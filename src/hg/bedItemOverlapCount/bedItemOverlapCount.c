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

static char const rcsid[] = "$Id: bedItemOverlapCount.c,v 1.12 2009/04/21 23:02:25 larrym Exp $";

/* Command line switches. */
//static char *strand = (char *)NULL;	/* strand to process, default +	*/
		/* this is not implemented, the user can filter + and - */

static struct hash *chromHash = NULL;
static char *host = NULL;
static char *user = NULL;
static char *password = NULL;
char *chromSizes = NULL;		/* read chrom sizes from file instead of database . */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"chromSize", OPTION_STRING},
    {"host", OPTION_STRING},
    {"user", OPTION_STRING},
    {"password", OPTION_STRING},
    {"strand", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedItemOverlapCount - count number of times a base is overlapped by the\n"
  "\titems in a bed file.  Output is bedGraph 4 to stdout.\n"
  "usage:\n"
  " sort -k1,1 -k2,2n bedFile.bed | bedItemOverlapCount [options] <database> stdin\n"
  "or all the way to wiggle track data:\n"
  " sort -k1,1 -k2,2n bedFile.bed \\\n"
  "     | bedItemOverlapCount [options] <database> stdin \\\n"
  "         | wigEncode stdin data.wig data.wib\n"
  "options:\n"
  "   -chromSize=sizefile\tRead chrom sizes from file instead of database\n"
  "   -host=hostname\tmysql host used to get chrom sizes\n"
  "   -user=username\tmysql user\n"
  "   -password=password\tmysql password\n\n"
  "\tchromSize file is three white space separated fields per line: chrom name, size, and dummy value\n"
  "\tYou will want to separate your + and - strand\n"
  "\titems before sending into this program as it only looks at\n"
  "\tthe chrom, start and end columns of the bed file.\n"
  "\tIt requires a <database> connection to lookup chrom sizes for a sanity\n"
  "\tcheck of the incoming data (unless you use -chromSize argument).\n\n"
  "The bed file must be sorted at least by chrom since the processing is\n"
  "\tgoing to be chrom by chrom with no going back.\n"
  " *** AND *** this is only for simple bed files without multiple blocks. ***"
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

static void outputCounts(unsigned short *counts, char *chrom, unsigned size)
{
int start = 0;
int end = 0;
int prevCount = 0;
unsigned long i;
for (i = 0; i < size; ++i)
    {
    if (counts[i])
	{
	if (0 == prevCount)	/*	starting an element	*/
	    {
	    start = i;
	    end = start+1;
	    }
	else if (prevCount == counts[i])	/* continuing an element */
	    {
	    ++end;
	    }
	else		/*	prevCount != counts[i] -> finished an element */
	    {
	    printf("%s\t%d\t%d\t%d\n", chrom, start, end, prevCount);
	    start = i;
	    end = start+1;
	    }
	}
    else if (prevCount)		/*	finished an element	*/
	{
	printf("%s\t%d\t%d\t%d\n", chrom, start, end, prevCount);
	start = i;
	end = start+1;
	}
    prevCount = counts[i];
    }
if (prevCount)		/*	last element at end of chrom	*/
    printf("%s\t%d\t%d\t%d\n", chrom, start, end, prevCount);
}

static void bedItemOverlapCount(char *database, int fileCount, char *fileList[])
{
unsigned maxChromSize = 0;
int i;
unsigned short *counts = (unsigned short *)NULL;
char *prevChrom = (char *)NULL;
boolean outputToDo = FALSE;
unsigned chromSize = 0;

if (chromSizes != NULL)
    {
    chromHash = newHash(0);
    // unfortunately, chromInfoLoadAll requires that the file have three fields (I don't know why),
    // so that's why we require a dummy third column in the chromInfo file.
    struct chromInfo *el = chromInfoLoadAll(chromSizes);
    for(;el != NULL;el=el->next)
        {
        if (el->size > maxChromSize) maxChromSize = el->size;
        verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
        hashAdd(chromHash, el->chrom, (void *)(& el->size));
        }
    }
else
    {
    chromHash = loadAllChromInfo(database, &maxChromSize);
    }

verbose(2,"#\tmaxChromSize: %u\n", maxChromSize);
if (maxChromSize < 1)
    errAbort("maxChromSize is zero ?");

/*	Allocate just once for the largest chrom and reuse this array */
counts = needHugeMem(sizeof(unsigned short) * maxChromSize);

/*	Reset the array to be zero to be reused */
memset((void *)counts, 0, sizeof(unsigned short)*(size_t)maxChromSize);

for (i=0; i<fileCount; ++i)
{
    struct lineFile *bf = lineFileOpen(fileList[i] , TRUE);
    unsigned thisChromSize = 0;
    struct bed *bed = (struct bed *)NULL;
    char *row[3];

    while (lineFileNextRow(bf,row,ArraySize(row)))
	{
	int i;
	bed = bedLoadN(row, 3);
	verbose(3,"#\t%s\t%d\t%d\n",bed->chrom,bed->chromStart, bed->chromEnd);
	if (prevChrom && differentWord(bed->chrom,prevChrom))
	    {
	    chromSize = chromosomeSize(prevChrom);
	    verbose(2,"#\tchrom %s done, size %d\n", prevChrom, chromSize);
	    if (outputToDo)
		outputCounts(counts, prevChrom, chromSize);
	    outputToDo = FALSE;
	    memset((void *)counts, 0,
		sizeof(unsigned short)*(size_t)maxChromSize); /* zero counts */
	    freeMem(prevChrom);
	    prevChrom = cloneString(bed->chrom);
	    thisChromSize = chromosomeSize(prevChrom);
	    verbose(2,"#\tchrom %s starting, size %d\n", prevChrom,
		thisChromSize);
	    }
	else if ((char *)NULL == prevChrom)
	    {
	    prevChrom = cloneString(bed->chrom);
	    chromSize = chromosomeSize(prevChrom);
	    thisChromSize = chromosomeSize(prevChrom);
	    verbose(2,"#\tchrom %s starting, size %d\n", prevChrom,
		thisChromSize);
	    }
    if (bed->chromEnd > thisChromSize)
        {
        if (bed->chromStart >= thisChromSize || differentWord(bed->chrom,"chrM")) // circular chrom
            {
            warn("ERROR: %s\t%d\t%d", bed->chrom, bed->chromStart,
            bed->chromEnd);
            errAbort("chromEnd > chromSize ?  %d > %d", bed->chromEnd,
                thisChromSize);
            }
        for (i = bed->chromStart; i < thisChromSize; ++i)
            counts[i]++;
        for (i = 0; i < (bed->chromEnd - thisChromSize); ++i)
            counts[i]++;
        }
    else
        {
        for (i = bed->chromStart; i < bed->chromEnd; ++i)
            counts[i]++;
        }
	outputToDo = TRUE;
	bedFree(&bed); // plug the memory leak
	}
    verbose(2,"#\tchrom %s done, size %d\n", prevChrom, thisChromSize);
    lineFileClose(&bf);
}
if (outputToDo)
    outputCounts(counts, prevChrom, chromSize);
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
verbose(2, "#\tworking on database: %s\n", argv[1]);
bedItemOverlapCount(argv[1], argc-2, argv+2);
return 0;
}
