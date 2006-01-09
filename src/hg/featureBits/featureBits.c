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

static char const rcsid[] = "$Id: featureBits.c,v 1.40 2006/01/09 18:32:59 galt Exp $";

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
    {"minFeatureSize", OPTION_INT},
    {"enrichment", OPTION_BOOLEAN},
    {"where", OPTION_STRING},
    {"bin", OPTION_STRING},
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
boolean countGaps = FALSE;	/* Count gaps in denominator? */
boolean noRandom = FALSE;	/* Exclude _random chromosomes? */
boolean noHap = FALSE;	/* Exclude _hap chromosomes? */
boolean calcEnrichment = FALSE;	/* Calculate coverage/enrichment? */
int binSize = 500000;	/* Default bin size. */
int binOverlap = 250000;	/* Default bin size. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections. \n"
  "usage:\n"
  "   featureBits database table(s)\n"
  "This will return the number of bits in all the tables anded together\n"
  "Options:\n"
  "   -bed=output.bed   Put intersection into bed format\n"
  "   -fa=output.fa     Put sequence in intersection into .fa file\n"
  "   -faMerge          For fa output merge overlapping features.\n"
  "   -minSize=N        Minimum size to output (default 1)\n"
  "   -chrom=chrN       Restrict to one chromosome\n"
  "   -or               Or tables together instead of anding them\n"
  "   -not              Output negation of resulting bit set.\n"
  "   -countGaps        Count gaps in denominator\n"
  "   -noRandom         Don't include _random (or Un) chromosomes\n"
  "   -noHap            Don't include _hap chromosomes\n"
  "   -minFeatureSize=n Don't include bits of the track that are smaller than\n"
  "                     minFeatureSize, useful for differentiating between\n"
  "                     alignment gaps and introns.\n"
  "   -bin=output.bin   Put bin counts in output file\n"
  "   -binSize=N        Bin size for generating counts in bin file (default 500000)\n"
  "   -binOverlap=N     Bin overlap for generating counts in bin file (default 250000)\n"

  "   -bedRegionIn=input.bed   Read in a bed file for bin counts in specific regions and write to bedRegionsOut\n"
  "   -bedRegionOut=output.bed Write a bed file of bin counts in specific regions from bedRegionIn\n"

  "   -enrichment       Calculates coverage and enrichment assuming first table\n"
  "                     is reference gene track and second track something else\n"
  "   '-where=some sql pattern'  restrict to features matching some sql pattern\n"
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

boolean isFileType(char *file, char *suffix)
/* determine if file ends with .suffix, or .suffix.gz */
{
if (endsWith(file, suffix))
    return TRUE;
else
    {
    char sbuf[64];
    safef(sbuf, sizeof(sbuf), "%s.gz", suffix);
    return endsWith(file, sbuf);
    }
}

bool inclChrom(struct slName *chrom)
/* check if a chromosome should be included */
{
return  !((noRandom && (endsWith(chrom->name, "_random")
                        || startsWith("chrUn", chrom->name)
                        || sameWord("chrNA", chrom->name) /* danRer */
                        || sameWord("chrU", chrom->name)))  /* dm */
          || (noHap && stringIn( "_hap", chrom->name)));
}

void bitsToBed(Bits *bits, char *chrom, int chromSize, FILE *bed, FILE *fa, 
	int minSize)
/* Write out runs of bits of at least minSize as items in a bed file. */
{
int i;
boolean thisBit, lastBit = FALSE;
int start = 0;
int id = 0;

for (i=0; i<chromSize; ++i)
    {
    thisBit = bitReadOne(bits, i);
    if (thisBit)
	{
	if (!lastBit)
	    start = i;
	}
    else
        {
	if (lastBit && i-start >= minSize)
	    {
	    if (bed)
		fprintf(bed, "%s\t%d\t%d\t%s.%d\n", chrom, start, i, chrom, ++id);
	    if (fa)
	        {
		char name[256];
		struct dnaSeq *seq = hDnaFromSeq(chrom, start, i, dnaLower);
		sprintf(name, "%s:%d-%d", chrom, start, i);
		faWriteNext(fa, name, seq->dna, seq->size);
		freeDnaSeq(&seq);
		}
	    }
	}
    lastBit = thisBit;
    }
if (lastBit && i-start >= minSize)
    {
    if (bed)
	fprintf(bed, "%s\t%d\t%d\t%s.%d\n", chrom, start, i, chrom, ++id);
    if (fa)
	{
	char name[256];
	struct dnaSeq *seq = hDnaFromSeq(chrom, start, i, dnaLower);
	sprintf(name, "%s:%d-%d", chrom, start, i);
	faWriteNext(fa, name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
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
    strcpy(fileName, track);
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
    sprintf(fileName, "%s/%s%s%s", dir, chrom, root, ext);
    if (!fileExists(fileName))
	{
        warn("Couldn't find %s or %s", track, fileName);
	strcpy(fileName, track);
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
strcpy(track, spec);
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

void orTable(Bits *acc, char *track, char *chrom, 
	int chromSize, struct sqlConnection *conn)
/* Or in table if it exists.  Else do nothing. */
{
char t[512], *s;
char table[512];

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
    boolean isFound = hFindSplitTable(chrom, t, table, &hasBin);
    if (isFound)
	fbOrTableBitsQueryMinSize(acc, track, chrom, chromSize, conn, where,
			   TRUE, TRUE, minFeatureSize);
    }
}


void chromFeatureBits(struct sqlConnection *conn,
	char *chrom, int tableCount, char *tables[],
	FILE *bedFile, FILE *faFile, FILE *binFile,
        struct bed *bedRegionList, FILE *bedOutFile,
	int *retChromSize, int *retChromBits,
	int *retFirstTableBits, int *retSecondTableBits)
/* featureBits - Correlate tables via bitmap projections and booleans
 * on one chromosome. */
{
int chromSize, i;
Bits *acc = NULL;
Bits *bits = NULL;
char *table;

*retChromSize = chromSize = hChromSize(chrom);
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
	orTable(acc, table, chrom, chromSize, conn);
	if (not)
	   bitNot(acc, chromSize);
	if (retFirstTableBits != NULL)
	   *retFirstTableBits = bitCountRange(acc, 0, chromSize);
	}
    else
	{
	bitClear(bits, chromSize);
	orTable(bits, table, chrom, chromSize, conn);
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
    bitsToBed(acc, chrom, chromSize, bedFile, faFile, minSize);
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
	char *chrom, char *trackSpec,
	FILE *bedFile, FILE *faFile,
	int *retItemCount, int *retBaseCount)
/* Write out sequence file for features from one chromosome.
 * This separate routine handles the non-merged case.  It's
 * reason for being is so that the feature names get preserved. */
{
boolean hasBin;
char t[512], *s = NULL;
char table[512];
boolean isSplit;
struct featureBits *fbList = NULL, *fb;

if (trackSpec[0] == '!')
   errAbort("Sorry, '!' not available with fa output unless you use faMerge");
isolateTrackPartOfSpec(trackSpec, t);
s = strchr(t, '.');
if (s != NULL)
    errAbort("Sorry, only database (not file) tracks allowed with "
             "fa output unless you use faMerge");
isSplit = hFindSplitTable(chrom, t, table, &hasBin);
fbList = fbGetRangeQuery(trackSpec, chrom, 0, hChromSize(chrom),
			 where, TRUE, TRUE);
for (fb = fbList; fb != NULL; fb = fb->next)
    {
    int s = fb->start, e = fb->end;
    if (bedFile != NULL)
	{
	fprintf(bedFile, "%s\t%d\t%d\t%s", 
	    fb->chrom, fb->start, fb->end, fb->name);
	if (fb->strand != '?')
	    fprintf(bedFile, "\t%c", fb->strand);
	fprintf(bedFile, "\n");
	}
    if (faFile != NULL)
        {
	struct dnaSeq *seq = hDnaFromSeq(chrom, s, e, dnaLower);
	if (fb->strand == '-')
	    reverseComplement(seq->dna, seq->size);
	faWriteNext(faFile, fb->name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
featureBitsFreeList(&fbList);
}

int countBases(struct sqlConnection *conn, char *chrom, int chromSize)
/* Count bases, generally not including gaps, in chromosome. */
{
int size, totalGaps = 0;
struct sqlResult *sr;
char **row;
int rowOffset;

if (countGaps)
    return chromSize;
sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    agpGapStaticLoad(row+rowOffset, &gap);
    size = gap.chromEnd - gap.chromStart;
    totalGaps += size;
    }
sqlFreeResult(&sr);
return chromSize - totalGaps;
}

void checkInputExists(struct sqlConnection *conn, struct slName *allChroms, int tableCount, char *tables[])
/* check input tables/files exist, especially to handle split tables */
{
struct slName *chrom = NULL;
char *track=NULL;
int i = 0, missing=0;
char t[512], *s=NULL;
char table[512];
char fileName[512];
boolean found = FALSE;
for (i=0; i<tableCount; ++i)
    {
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
    for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
	{
	if (inclChrom(chrom))
	    {
	    if (s)
		{
		chromFileName(t, chrom->name, fileName);
		if (fileExists(fileName))
		    {
		    found = TRUE;
		    break;
		    }
		}
	    else
		{
		boolean hasBin;
		if (hFindSplitTable(chrom->name, t, table, &hasBin))
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
struct slName *allChroms = NULL, *chrom = NULL;
struct bed *bedRegionList = NULL;
boolean faIndependent = FALSE;

hSetDb(database);
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

if (sameWord(clChrom, "all"))
    allChroms = hAllChromNames();
else
    allChroms = newSlName(clChrom);
conn = hAllocConn();

checkInputExists(conn, allChroms, tableCount, tables);

if (!faIndependent)
    {
    double totalBases = 0, totalBits = 0;
    int firstTableBits = 0, secondTableBits = 0;
    int *pFirstTableBits = NULL, *pSecondTableBits = NULL;
    double totalFirstBits = 0, totalSecondBits = 0;

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
	    if (startsWith(row[0],"#"))
		continue;
	    bed = bedLoad3(row);
	    slAddHead(&bedRegionList, bed);
	    }
	lineFileClose(&lf);
	slReverse(bedRegionList);
	}
    for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
	{
	if (inclChrom(chrom))
	    {
	    int chromSize, chromBitSize;
	    chromFeatureBits(conn, chrom->name, tableCount, tables,
		bedFile, faFile, binFile, bedRegionList, bedRegionOutFile, 
		&chromSize, &chromBitSize, pFirstTableBits, pSecondTableBits
		);
	    totalBases += countBases(conn, chrom->name, chromSize);
	    totalBits += chromBitSize;
	    totalFirstBits += firstTableBits;
	    totalSecondBits += secondTableBits;
	    }
	}
    if (calcEnrichment)
        printf("%s %5.3f%%, %s %5.3f%%, both %5.3f%%, cover %4.2f%%, enrich %4.2fx\n",
		tables[0], 
		100.0 * totalFirstBits/totalBases,
		tables[1],
		100.0 * totalSecondBits/totalBases,
		100.0 * totalBits/totalBases,
		100.0 * totalBits / totalFirstBits,
		(totalBits/totalSecondBits) / (totalFirstBits/totalBases) );
    else
	printf("%1.0f bases of %1.0f (%4.3f%%) in intersection\n",
	    totalBits, totalBases, 100.0*totalBits/totalBases);
    }
else
    {
    int totalItems = 0;
    double totalBases = 0;
    int itemCount, baseCount;
    for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
        {
	if (inclChrom(chrom))
	    {
	    chromFeatureSeq(conn, chrom->name, tables[0],
		    bedFile, faFile, &itemCount, &baseCount);
	    totalBases += countBases(conn, chrom->name, baseCount);
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
orLogic = optionExists("or");
notResults = optionExists("not");
countGaps = optionExists("countGaps");
noRandom = optionExists("noRandom");
noHap = optionExists("noHap");
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
