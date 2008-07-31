/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "fa.h"
#include "bits.h"
#include "bed.h"
#include "psl.h"
#include "portable.h"
#include "featureBits.h"
#include "agpGap.h"
#include "chain.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: featureBits.c,v 1.52.6.1 2008/07/31 02:23:59 markd Exp $";

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"bed", OPTION_STRING},
    {"fa", OPTION_STRING},
    {"faMerge", OPTION_BOOLEAN},
    {"minSize", OPTION_INT},
    {"chrom", OPTION_STRING},
    {"or", OPTION_BOOLEAN},
    {"not", OPTION_BOOLEAN},
    {"countGaps", OPTION_BOOLEAN},
    {"noRandom", OPTION_BOOLEAN},
    {"noHap", OPTION_BOOLEAN},
    {"dots", OPTION_INT},
    {"minFeatureSize", OPTION_INT},
    {"enrichment", OPTION_BOOLEAN},
    {"where", OPTION_STRING},
    {"bin", OPTION_STRING},
    {"chromSize", OPTION_STRING},
    {"binSize", OPTION_INT},
    {"binOverlap", OPTION_INT},
    {"bedRegionIn", OPTION_STRING},
    {"bedRegionOut", OPTION_STRING},
    {NULL, 0}
};

int minSize = 1;	/* Minimum size of feature. */
char *clChrom = "all";	/* Which chromosome. */
boolean orLogic = FALSE;  /* Do ors instead of ands? */
boolean notResults = FALSE;   /* negate results? */
char *where = NULL;		/* Extra selection info. */
char *chromSizes = NULL;		/* read chrom sizes from file instead of database . */
boolean countGaps = FALSE;	/* Count gaps in denominator? */
boolean noRandom = FALSE;	/* Exclude _random chromosomes? */
boolean noHap = FALSE;	/* Exclude _hap chromosomes? */
int dots = 0;	/* print dots every N chroms (scaffolds) processed */
boolean calcEnrichment = FALSE;	/* Calculate coverage/enrichment? */
int binSize = 500000;	/* Default bin size. */
int binOverlap = 250000;	/* Default bin size. */

/* to process chroms without constantly looking up in chromInfo, create
 * this list of them from the chromInfo once.
 */
static struct chromInfo *chromInfoList = NULL;
static struct hash *gapHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections. \n"
  "usage:\n"
  "   featureBits database table(s)\n"
  "This will return the number of bits in all the tables anded together\n"
  "Pipe warning:  output goes to stderr.\n"
  "Options:\n"
  "   -bed=output.bed   Put intersection into bed format. Can use stdout.\n"
  "   -fa=output.fa     Put sequence in intersection into .fa file\n"
  "   -faMerge          For fa output merge overlapping features.\n"
  "   -minSize=N        Minimum size to output (default 1)\n"
  "   -chrom=chrN       Restrict to one chromosome\n"
  "   -chromSize=sizefile       Read chrom sizes from file instead of database.\n"
  "   -or               Or tables together instead of anding them\n"
  "   -not              Output negation of resulting bit set.\n"
  "   -countGaps        Count gaps in denominator\n"
  "   -noRandom         Don't include _random (or Un) chromosomes\n"
  "   -noHap            Don't include _hap chromosomes\n"
  "   -dots=N           Output dot every N chroms (scaffolds) processed\n"
  "   -minFeatureSize=n Don't include bits of the track that are smaller than\n"
  "                     minFeatureSize, useful for differentiating between\n"
  "                     alignment gaps and introns.\n"
  "   -bin=output.bin   Put bin counts in output file\n"
  "   -binSize=N        Bin size for generating counts in bin file (default 500000)\n"
  "   -binOverlap=N     Bin overlap for generating counts in bin file (default 250000)\n"
  "   -bedRegionIn=input.bed    Read in a bed file for bin counts in specific regions \n"
  "                     and write to bedRegionsOut\n"
  "   -bedRegionOut=output.bed  Write a bed file of bin counts in specific regions \n"
  "                     from bedRegionIn\n"
  "   -enrichment       Calculates coverage and enrichment assuming first table\n"
  "                     is reference gene track and second track something else\n"
  "   '-where=some sql pattern'  Restrict to features matching some sql pattern\n"
  "You can include a '!' before a table name to negate it.\n"
  "Some table names can be followed by modifiers such as:\n"
  "    :exon:N          Break into exons and add N to each end of each exon\n"
  "    :cds             Break into coding exons\n"
  "    :intron:N        Break into introns, remove N from each end\n"
  "    :utr5, :utr3     Break into 5' or 3' UTRs\n" 
  "    :upstream:N      Consider the region of N bases before region\n"
  "    :end:N           Consider the region of N bases after region\n"
  "    :score:N         Consider records with score >= N \n"
  "    :upstreamAll:N   Like upstream, but doesn't filter out genes that \n"
  "                     have txStart==cdsStart or txEnd==cdsEnd\n"
  "    :endAll:N        Like end, but doesn't filter out genes that \n"
  "                     have txStart==cdsStart or txEnd==cdsEnd\n"
  "The tables can be bed, psl, or chain files, or a directory full of\n"
  "such files as well as actual database tables.  To count the bits\n"
  "used in dir/chrN_something*.bed you'd do:\n"
  "   featureBits database dir/_something.bed\n"
  );
}

static struct chromInfo *createChromInfoList(char *name, char *database)
/* Load up all chromosome infos. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;
int loaded=0;
struct chromInfo *ret = NULL;
unsigned totalSize = 0;

if (sameWord(name, "all"))
    sr = sqlGetResult(conn, "select * from chromInfo");
else
    {
    char select[256];
    safef(select, ArraySize(select), "select * from chromInfo where chrom='%s'",
	name);
    sr = sqlGetResult(conn, select);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *el;
    struct chromInfo *ci;
    AllocVar(ci);

    el = chromInfoLoad(row);
    ci->chrom = cloneString(el->chrom);
    ci->size = el->size;
    totalSize += el->size;
    slAddHead(&ret, ci);
    ++loaded;
    }
if (loaded < 1)
    errAbort("ERROR: can not find chrom name: '%s'\n", name);
slReverse(&ret);
if (sameWord(name, "all"))
    verbose(2, "#\tloaded size info for %d chroms, total size: %u\n",
	loaded, totalSize);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}

static boolean hasSuffixCompress(char *file, char *suffix, char *compSuffix)
/* determine if file ends with .suffix.compSuffix */
{
char sbuf[64];
safef(sbuf, sizeof(sbuf), "%s.%s", suffix, compSuffix);
return endsWith(file, sbuf);
}

boolean isFileType(char *file, char *suffix)
/* determine if file ends with .suffix, or .suffix.{gz,Z,bz2} */
{
char cleaned[PATH_LEN], *p;

/* remove any : qualifiers */
strcpy(cleaned, file);
p = strrchr (cleaned, '/');
if (p == NULL)
    p = cleaned;
p = strchr(p, ':');
if (p != NULL)
    *p = '\0';

if (endsWith(cleaned, suffix))
    return TRUE;
else if (hasSuffixCompress(cleaned, suffix, "gz"))
    return TRUE;
else if (hasSuffixCompress(cleaned, suffix, "Z"))
    return TRUE;
else if (hasSuffixCompress(cleaned, suffix, "bz2"))
    return TRUE;
else
    return FALSE;
}

bool inclChrom(char *name)
/* check if a chromosome should be included */
{
return  !((noRandom && (endsWith(name, "_random")
                        || startsWith("chrUn", name)
                        || sameWord("chrNA", name) /* danRer */
                        || sameWord("chrU", name)))  /* dm */
          || (noHap && stringIn( "_hap", name)));
}

void bitsToBins(Bits *bits, char *chrom, int chromSize, FILE *binFile, int binSize, int binOverlap)
/* Write out binned counts of bits. */
{
int bin, count;

if (!binFile)
    return;
for (bin=0; bin+binSize<chromSize; bin=bin+binOverlap)
    {
    count = bitCountRange(bits, bin, binSize);
    fprintf(binFile, "%s\t%d\t%d\t%d\t%s.%d\n", chrom, bin, bin+binSize, count, chrom, bin/binOverlap+1);
    }
count = bitCountRange(bits, bin, chromSize-bin);
fprintf(binFile, "%s\t%d\t%d\t%d\t%s.%d\n", chrom, bin, chromSize, count, chrom, bin/binOverlap+1);
}

void bitsToRegions(Bits *bits, char *chrom, int chromSize, struct bed *bedList, 
		   FILE *bedOutFile)
/* Write out counts of bits in regions defined by bed elements. */
{
struct bed *bl=NULL;
int count, i=0;

if (!bedOutFile)
    return;
for (bl=bedList; bl!=NULL; bl=bl->next)
    {
    if(differentString(bl->chrom,chrom))
	continue;
    count = bitCountRange(bits, bl->chromStart, bl->chromEnd-bl->chromStart);
    fprintf(bedOutFile, "%s\t%d\t%d\t%d\t%s.%d\n", chrom, bl->chromStart, bl->chromEnd, count, chrom, ++i);
    }
}

void check(struct sqlConnection *conn, char *table)
/* Check it's as planned. */
{
char query[256], **row;
struct sqlResult *sr;
int lastEnd = -1, lastStart = -1, start, end;
sprintf(query, "select chromStart,chromEnd from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = atoi(row[0]);
    end = atoi(row[1]);
    if (start < lastStart)
        fprintf(stderr,"Out of order: %d,%d\n", lastStart, start);
    if (rangeIntersection(lastStart, lastEnd, start-1, end) > 0)
        fprintf(stderr,"Overlapping: (%d %d) (%d %d)\n", lastStart, lastEnd, start, end);
    lastStart = start;
    lastEnd = end;
    }
sqlFreeResult(&sr);
errAbort("All for now");
}

char *chromFileName(char *track, char *chrom, char fileName[512])
/* Return chromosome specific version of file if it exists. */
{
if (fileExists(track))
    {
    strncpy(fileName, track, 512);
    }
else
    {
    char dir[256], root[128], ext[65];
    int len;
    splitPath(track, dir, root, ext);
    /* Chop trailing / off of dir. */
    len = strlen(dir);
    if (len > 0 && dir[len-1] == '/')
        dir[len-1] = 0;
    safef(fileName, 512, "%s/%s%s%s", dir, chrom, root, ext);
    if (!fileExists(fileName))
	{
        warn("Couldn't find %s or %s", track, fileName);
	strncpy(fileName, track, 512);
	}
    }
return fileName;
}

void outOfRange(struct lineFile *lf, char *chrom, int chromSize)
/* Complain that coordinate is out of range. */
{
errAbort("Coordinate out of allowed range [%d,%d) for %s near line %d of %s",
	0, chromSize, chrom, lf->lineIx, lf->fileName);
}

void setPslBits(struct lineFile *lf, 
	Bits *bits, struct psl *psl, int winStart, int winEnd)
/* Set bits that are in psl. */
{
int i, s, e, w, blockCount = psl->blockCount;
boolean isRev = (psl->strand[1] == '-');
for (i=0; i<blockCount; ++i)
    {
    s = psl->tStarts[i];
    e = s + psl->blockSizes[i];
    if (isRev)
       {
       /* Use w as a temp variable to reverse coordinates s&e. */
       w = psl->tSize - e;
       e = psl->tSize - s;
       s = w;
       }
    /* Clip, and if anything left set it. */
    if (s < winStart) outOfRange(lf, psl->tName, psl->tSize);
    if (e > winEnd) outOfRange(lf, psl->tName, psl->tSize);
    w = e - s;
    if (w > 0)
	bitSetRange(bits, s, w);
    }
}

void fbOrPsl(Bits *acc, char *track, char *chrom, int chromSize)
/* Or in bits of psl file that correspond to chrom. */
{
struct lineFile *lf;
char fileName[512];
struct psl *psl;

chromFileName(track, chrom, fileName);
if (!fileExists(fileName))
    return;
lf = pslFileOpen(fileName);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->tName, chrom))
	setPslBits(lf, acc, psl, 0, chromSize);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void fbOrBed(Bits *acc, char *track, char *chrom, int chromSize)
/* Or in bits of psl file that correspond to chrom. */
{
struct lineFile *lf;
char fileName[512];
char *row[3];
int s, e, w;

chromFileName(track, chrom, fileName);
if (!fileExists(fileName))
    return;
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    if (sameString(row[0], chrom))
        {
	s = lineFileNeedNum(lf, row, 1);
	if (s < 0) outOfRange(lf, chrom, chromSize);
	e = lineFileNeedNum(lf, row, 2);
	if (e > chromSize) outOfRange(lf, chrom, chromSize);
	w = e - s;
	if (w > 0)
	    bitSetRange(acc, s, w);
	}
    }
lineFileClose(&lf);
}

void fbOrChain(Bits *acc, char *track, char *chrom, int chromSize)
/* Or in a chain file. */
{
struct lineFile *lf;
char fileName[512];
struct chain *chain;
struct cBlock *b;

chromFileName(track, chrom, fileName);
if (!fileExists(fileName))
    return;
lf = lineFileOpen(fileName, TRUE);
while ((chain = chainRead(lf)) != NULL)
    {
    for (b = chain->blockList; b != NULL; b = b->next)
        {
	int s = b->tStart, e = b->tEnd;
	if (s < 0) outOfRange(lf, chrom, chromSize);
	if (e > chromSize) outOfRange(lf, chrom, chromSize);
	bitSetRange(acc, b->tStart, b->tEnd - b->tStart);
	}
    chainFree(&chain);
    }
}


void isolateTrackPartOfSpec(char *spec, char track[512])
/* Convert something like track:exon to just track */
{
char *s;
strncpy(track, spec, 512);
s = strchr(track, ':');
if (s != NULL) *s = 0;
}

void orFile(Bits *acc, char *track, char *chrom, int chromSize)
/* Or some sort of file into bits. */
{
if (isFileType(track, "psl"))
    {
    fbOrPsl(acc, track, chrom, chromSize);
    }
else if (isFileType(track, "bed"))
    {
    fbOrBed(acc, track, chrom, chromSize);
    }
else if (isFileType(track, "chain"))
    {
    fbOrChain(acc, track, chrom, chromSize);
    }
else  
    errAbort("can't determine file type of: %s", track);
}

void orTable(char *database, Bits *acc, char *track, char *chrom, 
	int chromSize, struct sqlConnection *conn)
/* Or in table if it exists.  Else do nothing. */
{
char t[512], *s;
char table[HDB_MAX_TABLE_STRING];

isolateTrackPartOfSpec(track, t);
s = strrchr(t, '.');
if (s != NULL)
    {
    orFile(acc, track, chrom, chromSize);
    }
else
    {
    boolean hasBin;
    int minFeatureSize = optionInt("minFeatureSize", 0);
    boolean isSplit = hFindSplitTable(database, chrom, t, table, &hasBin);
    boolean isFound = hTableExists(database, table);
    verbose(3,"orTable: db: %s isFound: %s isSplit: %s %s %s %s\n", database,
	isFound ? "TRUE" : "FALSE",
	    isSplit ? "TRUE" : "FALSE", chrom, t, table );
    if (isFound)
	fbOrTableBitsQueryMinSize(database, acc, track, chrom, chromSize, conn, where,
		   TRUE, TRUE, minFeatureSize);
    }
}


void chromFeatureBits(struct sqlConnection *conn,char *database, 
	char *chrom, int tableCount, char *tables[],
	FILE *bedFile, FILE *faFile, FILE *binFile,
        struct bed *bedRegionList, FILE *bedOutFile,
	int chromSize, int *retChromBits,
	int *retFirstTableBits, int *retSecondTableBits)
/* featureBits - Correlate tables via bitmap projections and booleans
 * on one chromosome. */
{
int i;
Bits *acc = NULL;
Bits *bits = NULL;
char *table;

acc = bitAlloc(chromSize);
bits = bitAlloc(chromSize);
for (i=0; i<tableCount; ++i)
    {
    boolean not = FALSE;
    table = tables[i];
    if (table[0] == '!')
        {
	not = TRUE;
	++table;
	}
    if (i == 0)
        {
	orTable(database, acc, table, chrom, chromSize, conn);
	if (not)
	   bitNot(acc, chromSize);
	if (retFirstTableBits != NULL)
	   *retFirstTableBits = bitCountRange(acc, 0, chromSize);
	}
    else
	{
	bitClear(bits, chromSize);
	orTable(database, bits, table, chrom, chromSize, conn);
	if (not)
	   bitNot(bits, chromSize);
	if (i == 1 && retSecondTableBits != NULL)
	   *retSecondTableBits = bitCountRange(bits, 0, chromSize);
	/* feature/bug - the above does not respect minSize */
	if (orLogic)
	    bitOr(acc, bits, chromSize);
	else
	    bitAnd(acc, bits, chromSize);
	}
    }
if (notResults)
    bitNot(acc, chromSize);    
*retChromBits = bitCountRange(acc, 0, chromSize);
if (bedFile != NULL || faFile != NULL)
    {
    minSize = optionInt("minSize", minSize);
    bitsToBed(database, acc, chrom, chromSize, bedFile, faFile, minSize);
    }
if (binFile != NULL)
    {
    binSize = optionInt("binSize", binSize);
    binOverlap = optionInt("binOverlap", binOverlap);
    bitsToBins(acc, chrom, chromSize, binFile, binSize, binOverlap);
    }
if (bedOutFile != NULL)
    bitsToRegions(acc, chrom, chromSize, bedRegionList, bedOutFile);
bitFree(&acc);
bitFree(&bits);
}

void chromFeatureSeq(struct sqlConnection *conn, 
	char *database, char *chrom, char *trackSpec,
	FILE *bedFile, FILE *faFile,
	int *retItemCount, int *retBaseCount)
/* Write out sequence file for features from one chromosome.
 * This separate routine handles the non-merged case.  It's
 * reason for being is so that the feature names get preserved. */
{
boolean hasBin;
char t[512], *s = NULL;
char table[HDB_MAX_TABLE_STRING];
boolean isSplit;
struct featureBits *fbList = NULL, *fb;

if (trackSpec[0] == '!')
   errAbort("Sorry, '!' not available with fa output unless you use faMerge");
isolateTrackPartOfSpec(trackSpec, t);
s = strchr(t, '.');
if (s != NULL)
    errAbort("Sorry, only database (not file) tracks allowed with "
             "fa output unless you use faMerge");
isSplit = hFindSplitTable(database, chrom, t, table, &hasBin);
fbList = fbGetRangeQuery(database, trackSpec, chrom, 0, hChromSize(database, chrom),
			 where, TRUE, TRUE);
for (fb = fbList; fb != NULL; fb = fb->next)
    {
    int s = fb->start, e = fb->end;
    if (bedFile != NULL)
	{
	fprintf(bedFile, "%s\t%d\t%d\t%s", 
	    fb->chrom, fb->start, fb->end, fb->name);
	if (fb->strand != '?')
	    fprintf(bedFile, "\t0\t%c", fb->strand);
	fprintf(bedFile, "\n");
	}
    if (faFile != NULL)
        {
	struct dnaSeq *seq = hDnaFromSeq(database, chrom, s, e, dnaLower);
	if (fb->strand == '-')
	    reverseComplement(seq->dna, seq->size);
	faWriteNext(faFile, fb->name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
featureBitsFreeList(&fbList);
}

static struct hash *loadAllGaps(struct sqlConnection *conn, char *db)
/*	working on all chroms, fetch all per-chrom gap counts at once
 *	returns hash by chrom name to gap counts for that chrom
 */
{ 
struct chromInfo *cInfo;
struct sqlResult *sr;
char **row;
struct hash *ret;
int totalGapSize = 0;
int gapCount = 0;

ret = newHash(0);

/*	If not split, read in whole gulp, create per-chrom hash of sizes */
if (hTableExists(db, "gap"))
    {
    char *prevChrom = NULL;
    int totalGapsThisChrom = 0;
    
    sr = sqlGetResult(conn,
	"select chrom,chromStart,chromEnd from gap order by chrom");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	int gapSize = sqlUnsigned(row[2]) - sqlUnsigned(row[1]);
	++gapCount;
	if (prevChrom && sameWord(prevChrom,row[0]))
	    {
	    totalGapsThisChrom += gapSize;
	    totalGapSize += gapSize;
	    }
	else
	    {
	    if (prevChrom)
		{
		hashAddInt(ret, prevChrom, totalGapsThisChrom);
		freeMem(prevChrom);
		prevChrom = cloneString(row[0]);
		totalGapsThisChrom = gapSize;
		totalGapSize += gapSize;
		}
	    else
		{
		prevChrom = cloneString(row[0]);
		totalGapsThisChrom = gapSize;
		totalGapSize += gapSize;
		}
	    }
	}
	/*	and the last one	*/
	if (prevChrom && (totalGapsThisChrom > 0))
	    {
	    hashAddInt(ret, prevChrom, totalGapsThisChrom);
	    freeMem(prevChrom);
	    }
    sqlFreeResult(&sr);
    }
else
    {
    /*	for each chrom name, fetch the gap count	*/
    for (cInfo = chromInfoList; cInfo != NULL; cInfo = cInfo->next)
	{
	int rowOffset;
	int totalGapsThisChrom = 0;
	sr = hChromQuery(conn, "gap", cInfo->chrom, NULL, &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int gapSize;
	    struct agpGap gap;
	    ++gapCount;
	    agpGapStaticLoad(row+rowOffset, &gap);
	    gapSize = gap.chromEnd - gap.chromStart;
	    totalGapsThisChrom += gapSize;
	    totalGapSize += gapSize;
	    }
	sqlFreeResult(&sr);
	hashAddInt(ret, cInfo->chrom, totalGapsThisChrom);
	}
    }
verbose(2,"#\tloaded %d gaps covering %d bases\n", gapCount, totalGapSize);
return ret;
}

int countBases(struct sqlConnection *conn, char *chrom, int chromSize,
    char *database)
/* Count bases, generally not including gaps, in chromosome. */
{
static boolean gapsLoaded = FALSE;
struct sqlResult *sr;
int totalGaps = 0;
char **row;
int rowOffset;

if (countGaps)
    return chromSize;

/*	If doing all chroms, then load up all the gaps and be done with
 *	it instead of re-reading the gap table for every chrom
 */
if (sameWord(clChrom,"all"))
    {
    if (!gapsLoaded)
	gapHash = loadAllGaps(conn, database);
    gapsLoaded = TRUE;
    totalGaps = hashIntValDefault(gapHash, chrom, 0);
    }
else
    {
    sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	int gapSize;
	struct agpGap gap;
	agpGapStaticLoad(row+rowOffset, &gap);
	gapSize = gap.chromEnd - gap.chromStart;
	totalGaps += gapSize;
	}
    sqlFreeResult(&sr);
    }
return chromSize - totalGaps;
}

void checkInputExists(struct sqlConnection *conn,char *database, 
	struct chromInfo *chromInfoList, int tableCount, char *tables[])
/* check input tables/files exist, especially to handle split tables */
{
char *track=NULL;
int i = 0, missing=0;
char t[512], *s=NULL;
char table[HDB_MAX_TABLE_STRING];
char fileName[512];
boolean found = FALSE;

for (i=0; i<tableCount; ++i)
    {
    struct chromInfo *cInfo;

    track = tables[i];
    if (track[0] == '!')
	{
	++track;
	}
    isolateTrackPartOfSpec(track, t);
    s = strrchr(t, '.');
    if (s)
	{
	if (fileExists(t))
	    continue;
	}
    else
	{
	if (sqlTableExists(conn, t))
	    continue;
	}
    found = FALSE;
    for (cInfo = chromInfoList; cInfo != NULL; cInfo = cInfo->next)
	{
	if (inclChrom(cInfo->chrom))
	    {
	    if (s)
		{
		chromFileName(t, cInfo->chrom, fileName);
		if (fileExists(fileName))
		    {
		    found = TRUE;
		    break;
		    }
		}
	    else
		{
		boolean hasBin;
		if (hFindSplitTable(database, cInfo->chrom, t, table, &hasBin))
		    {
		    found = TRUE;
		    break;
		    }
		}
	    }
	}
    if (!found)
	{
	if (s)
	    warn("file %s not found for any chroms", t);
	else
	    warn("table %s not found for any chroms", t);
	++missing;	    
	}
    }
if (missing>0)
    errAbort("Error: %d input table(s)/file(s) do not exist for any of the chroms specified",missing);
}


void featureBits(char *database, int tableCount, char *tables[])
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
struct sqlConnection *conn = NULL;
char *bedName = optionVal("bed", NULL), *faName = optionVal("fa", NULL);
char *binName = optionVal("bin", NULL);
char *bedRegionInName = optionVal("bedRegionIn", NULL);
char *bedRegionOutName = optionVal("bedRegionOut", NULL);
FILE *bedFile = NULL, *faFile = NULL, *binFile = NULL;
FILE *bedRegionOutFile = NULL;
struct bed *bedRegionList = NULL;
boolean faIndependent = FALSE;
struct chromInfo *cInfo;

if (bedName)
    bedFile = mustOpen(bedName, "w");
if (binName)
    binFile = mustOpen(binName, "w");
if ((bedRegionInName && !bedRegionOutName) || (!bedRegionInName && bedRegionOutName))
    errAbort("bedRegionIn and bedRegionOut must both be specified");
if (faName)
    {
    boolean faMerge = optionExists("faMerge");
    faFile = mustOpen(faName, "w");
    if (tableCount > 1)
        {
	if (!faMerge)
	    errAbort("For fa output of multiple tables you must use the "
	             "faMerge option");
	}
    faIndependent = (!faMerge);
    }

if (chromSizes != NULL)
    chromInfoList = chromInfoLoadAll(chromSizes);
else
    chromInfoList = createChromInfoList(clChrom, database);

conn = hAllocConn(database);
checkInputExists(conn, database, chromInfoList, tableCount, tables);

if (!faIndependent)
    {
    double totalBases = 0, totalBits = 0;
    int firstTableBits = 0, secondTableBits = 0;
    int *pFirstTableBits = NULL, *pSecondTableBits = NULL;
    double totalFirstBits = 0, totalSecondBits = 0;
    static int dotClock = 1;

    if (calcEnrichment)
        {
	pFirstTableBits = &firstTableBits;
	pSecondTableBits = &secondTableBits;
	}
    if (bedRegionInName)
	{
	struct lineFile *lf = lineFileOpen(bedRegionInName, TRUE);
	struct bed *bed;
	char *row[3];
	
	bedRegionOutFile = mustOpen(bedRegionOutName, "w");
	while (lineFileRow(lf, row))
	    {
	    if (startsWith(row[0],"#")||startsWith(row[0],"chrom"))
		continue;
	    bed = bedLoad3(row);
	    slAddHead(&bedRegionList, bed);
	    }
	lineFileClose(&lf);
	slReverse(&bedRegionList);
	}
    for (cInfo = chromInfoList; cInfo != NULL; cInfo = cInfo->next)
	{
	if (inclChrom(cInfo->chrom))
	    {
	    int chromBitSize;
	    int chromSize = cInfo->size;
	    verbose(3,"chromFeatureBits(%s)\n", cInfo->chrom);
	    chromFeatureBits(conn, database, cInfo->chrom, tableCount, tables,
		bedFile, faFile, binFile, bedRegionList, bedRegionOutFile, 
		chromSize, &chromBitSize, pFirstTableBits, pSecondTableBits
		);
	    totalBases += countBases(conn, cInfo->chrom, chromSize, database);
	    totalBits += chromBitSize;
	    totalFirstBits += firstTableBits;
	    totalSecondBits += secondTableBits;
	    if (dots > 0)
		{
		if (--dotClock <= 0)
		    {
		    fputc('.', stdout);
		    fflush(stdout);
		    dotClock = dots;
		    }
		}
	    }
	}
	if (dots > 0)
	    {
	    fputc('\n', stdout);
	    fflush(stdout);
	    }
    if (calcEnrichment)
        fprintf(stderr,"%s %5.3f%%, %s %5.3f%%, both %5.3f%%, cover %4.2f%%, enrich %4.2fx\n",
		tables[0], 
		100.0 * totalFirstBits/totalBases,
		tables[1],
		100.0 * totalSecondBits/totalBases,
		100.0 * totalBits/totalBases,
		100.0 * totalBits / totalFirstBits,
		(totalBits/totalSecondBits) / (totalFirstBits/totalBases) );
    else
	fprintf(stderr,"%1.0f bases of %1.0f (%4.3f%%) in intersection\n",
	    totalBits, totalBases, 100.0*totalBits/totalBases);
    }
else
    {
    int totalItems = 0;
    double totalBases = 0;
    int itemCount, baseCount;
    for (cInfo = chromInfoList; cInfo != NULL; cInfo = cInfo->next)
        {
	if (inclChrom(cInfo->chrom))
	    {
	    chromFeatureSeq(conn, database, cInfo->chrom, tables[0],
		    bedFile, faFile, &itemCount, &baseCount);
	    totalBases += countBases(conn, cInfo->chrom, baseCount, database);
	    totalItems += itemCount;
	    }
	}
    }
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
clChrom = optionVal("chrom", clChrom);
chromSizes = optionVal("chromSize", NULL);
orLogic = optionExists("or");
notResults = optionExists("not");
countGaps = optionExists("countGaps");
noRandom = optionExists("noRandom");
noHap = optionExists("noHap");
dots = optionInt("dots", dots);
where = optionVal("where", NULL);
calcEnrichment = optionExists("enrichment");
if (calcEnrichment && argc != 4)
    errAbort("You must specify two tables with -enrichment");
if (calcEnrichment && orLogic)
    errAbort("You can't use -or with -enrichment");
if (calcEnrichment && notResults)
    errAbort("You can't use -not with -enrichment");
if (notResults && optionExists("fa") && !optionExists("faMerge"))
    errAbort("Must specify -faMerge if using -not with -fa");

featureBits(argv[1], argc-2, argv+2);
return 0;
}
