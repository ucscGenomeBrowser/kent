/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "psl.h"
#include "dnaseq.h"
#include "nibTwo.h"
#include "bed.h"
#include "genbank.h"
#include "genePred.h"
#include "gbRnaMapInfo.h"
#include "rangeTree.h"

/* Variables set from command line. */
int mergeMax = 5;	
boolean fixIntrons = TRUE;
int minSize = 18;
int minIntronSize=16;
FILE *unusualFile = NULL;
struct hash *cdsHash = NULL;
FILE *mapInfo = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txPsltoBed - Convert a psl to a bed file by projecting it onto its target\n"
  "sequence. Optionally merge adjacent blocks and trim to splice sites.\n"
  "usage:\n"
  "   txPsltoBed input.psl dnaPath output.bed\n"
  "where dnaPath is either a two bit file or a dir of nib files containing the DNA\n"
  "referenced on the target side of input.psl.\n"
  "options:\n"
  "   -cds=fileName - A three column file: qName,cdsStart,cdsEnd that describes\n"
  "                   The coding region. Will be used to set thickStart/thickEnd.\n"
  "   -mapInfo=fileName - Output info on how mapping went here\n"
  "   -mergeMax=N - merge small blocks separated by no more than N on either\n"
  "                 target or query. Default value is %d.\n"
  "   -minSize=N - suppress output of beds of less than this size (in exons)\n"
  "                Default value is %d.\n"
  "   -noFixIntrons - slide large gaps in target to seek for splice sites.\n"
  "   -minIntronSize - minimum size of an intron, default %d.\n"
  "   -unusual=fileName - put info about unusual splice sites etc. here\n"
  , mergeMax, minSize, minIntronSize
  );
}

static struct optionSpec options[] = {
   {"cds", OPTION_STRING},
   {"mapInfo", OPTION_STRING},
   {"mergeMax", OPTION_INT},
   {"minSize", OPTION_INT},
   {"noFixIntrons", OPTION_BOOLEAN},
   {"minIntronSize", OPTION_INT},
   {"unusual", OPTION_STRING},
   {NULL, 0},
};

void unusualPrint(char *format, ...)
/* Print out info to unusual file if it exists. */
{
if (unusualFile != NULL)
    {
    va_list args;
    va_start(args, format);
    vfprintf(unusualFile, format, args);
    va_end(args);
    }
}

int totalIntronLooking = 0;
int totalFunny = 0;
struct gbRnaMapInfo info;



struct hash *hashCdsFile(char *fileName)
/* Read in a CDS file and return hash of genbankCds keyed by accession */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(17);
while (lineFileRow(lf, row))
    {
    struct genbankCds *cds;
    char *cdsString = row[1];
    if (!stringIn("join", cdsString))
        {
	lmAllocVar(hash->lm, cds);
	if (genbankCdsParse(cdsString, cds))
	    {
	    hashAdd(hash, row[0], cds);
	    }
	else
	    verbose(2, "Couldn't parse cds string %s\n", cdsString);
	}
    }
return hash;
}

struct genbankCds *getCds(char *acc)
/* Get associated cdsInfo if any */
{
if (cdsHash != NULL)
    return hashFindVal(cdsHash, acc);
return NULL;
}

boolean fixOrientation(struct psl *psl, struct dnaSeq *chrom)
/* If the transcript is spliced, use this to override the
 * input orientation.  Also write info about unusual splice
 * sites to unusual file.  Return TRUE if fix required. */
{
int inputOrientation = (psl->strand[0] == '-' ? -1 : 1);
int totalOrientation = 0;
int outputOrientation;
int i;
int lastBlock = psl->blockCount-1;
for (i=0; i<lastBlock; ++i)
    {
    int blockSize = psl->blockSizes[i];
    int tStart = psl->tStarts[i] + blockSize;
    int qStart = psl->qStarts[i] + blockSize;
    int tEnd = psl->tStarts[i+1];
    int qEnd = psl->qStarts[i+1];
    if (qStart == qEnd && tEnd-tStart >= minIntronSize)
        {
	++totalIntronLooking;
	char *intronStart = chrom->dna + tStart;
	char *intronEnd = chrom->dna + tEnd;
	int orientation = intronOrientationMinSize(intronStart, intronEnd, minIntronSize);
	totalOrientation += orientation;
	if (orientation == 0)
	    {
	    totalFunny += 1;
	    unusualPrint("site %c%c/%c%c %s %s %d %s:%d-%d\n", intronStart[0], intronStart[1],
	    	intronEnd[-2], intronEnd[-1], psl->strand, psl->qName,
		i+1, psl->tName, psl->tStart+1, psl->tEnd); 
	    }
	}
    }
if (totalOrientation > 0)
    outputOrientation = 1;
else if (totalOrientation < 0)
    outputOrientation = -1;
else
    outputOrientation = inputOrientation;
if (outputOrientation != inputOrientation)
    {
    unusualPrint("flip %s %d %d %d %s:%d-%d\n", psl->qName, inputOrientation,
    	outputOrientation, totalOrientation, psl->tName, psl->tStart+1, psl->tEnd);
    if (psl->strand[0] == '+')
        psl->strand[0] = '-';
    else
        psl->strand[0] = '+';
    return TRUE;
    }
else
    return FALSE;
}

boolean fixPslIntrons(struct psl *psl, struct dnaSeq *chrom)
/* Go through gaps in psl.  For ones the right size to be introns,
 * see if ends are introns.  If not see if adding or subtracting
 * a base could turn them into intron ends.  Return TRUE if
 * it did a fix. */
{
boolean didFix = FALSE;
int start = psl->tStarts[0];
int size = psl->blockSizes[0];
int i;
char *dna = chrom->dna;
int orientation = (psl->strand[0] == '-' ? -1 : 1);
for (i=1; i<psl->blockCount; ++i)
    {
    int end = start+size;
    size = psl->blockSizes[i];
    start = psl->tStarts[i];
    char *intronStart = dna+end;
    char *intronEnd = dna+start;
    if (intronOrientationMinSize(intronStart, intronEnd, minIntronSize) != orientation)
        {
	if (orientation > 0)
	    {
	    if (memcmp(intronStart, "gt", 2) != 0)
		{
		if (memcmp(intronStart-1, "gt", 2) == 0)
		    {
		    psl->blockSizes[i-1] -= 1;
		    didFix = TRUE;
		    }
		else if (memcmp(intronStart+1, "gt", 2) == 0)
		    {
		    psl->blockSizes[i-1] += 1;
		    didFix = TRUE;
		    }
		}
	    if (memcmp(intronEnd-2, "ag", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ag", 2) == 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    psl->qStarts[i] -= 1;
		    didFix = TRUE;
		    }
		else if (memcmp(intronEnd-2+1, "ag", 2) == 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
		    psl->qStarts[i] += 1;
		    didFix = TRUE;
		    }
		}
	    }
	else
	    {
	    if (memcmp(intronStart, "ct", 2) != 0)
		{
		if (memcmp(intronStart-1, "ct", 2) == 0)
		    {
		    psl->blockSizes[i-1] -= 1;
		    didFix = TRUE;
		    }
		else if (memcmp(intronStart+1, "ct", 2) == 0)
		    {
		    psl->blockSizes[i-1] += 1;
		    didFix = TRUE;
		    }
		}
	    if (memcmp(intronEnd-2, "ac", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ac", 2) == 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    psl->qStarts[i] -= 1;
		    didFix = TRUE;
		    }
		else if (memcmp(intronEnd-2+1, "ac", 2) == 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
		    psl->qStarts[i] += 1;
		    didFix = TRUE;
		    }
		}
	    }
	}
    }
if (didFix)
    verbose(3, "Did little intron fix on %s\n", psl->qName);
return didFix;
}

struct bed *rangeListToBed(struct range *rangeList, struct psl *psl, 
	struct genbankCds *cds, int score)
/* Convert a list of ranges to a bed12. */
{
if (rangeList == NULL)
    return NULL;

/* Figure out number of blocks and overall bounds. */
int blockCount = 0;
int totalSize = 0;
struct range *range = NULL, *lastRange = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    if (range->end > range->start)
	{
	++blockCount;
	totalSize += range->end - range->start;
	lastRange = range;
	}
    }

if (totalSize < minSize)
    return NULL;

int chromStart = rangeList->start;
int chromEnd = lastRange->end;

/* Allocate bed and fill in most of it. */
struct bed *bed;
AllocVar(bed);
bed->chrom = cloneString(psl->tName);
bed->chromStart = chromStart;
bed->chromEnd = chromEnd;
bed->name = cloneString(psl->qName);
bed->score = score;
bed->strand[0] = psl->strand[0];
bed->blockCount = blockCount;

/* Fill in sizes and starts arrays */
int *sizes = AllocArray(bed->blockSizes, blockCount);
int *starts = AllocArray(bed->chromStarts, blockCount);
int i = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    if (range->end > range->start)
	{
	sizes[i] = range->end - range->start;
	starts[i] = range->start - chromStart;
	++i;
	}
    }

/* If there's a cds to map, try to map it via genePred code. */
if (cds != NULL)
    {
    struct genePred gp;
    struct genbankCds lcds = *cds;  // genePredAddGenbank might eat, so make copy
    ZeroVar(&gp);
    gp.name = bed->name;
    gp.chrom = bed->chrom;
    gp.strand[0] = bed->strand[0];
    gp.txStart = bed->chromStart;
    gp.txEnd = bed->chromEnd;
    gp.optFields = genePredCdsStatFld;
    genePredAddGenbankCds(psl, &lcds, &gp);
    if (gp.cdsStartStat == cdsComplete && gp.cdsEndStat == cdsComplete)
        {
	if (bed->chromStart <= gp.cdsStart && bed->chromEnd >= gp.cdsEnd)
	    {
	    bed->thickStart = gp.cdsStart;
	    bed->thickEnd = gp.cdsEnd;
	    info.outputCds = "CDS";
	    }
	else
	    {
	    info.cdsOutside="OUT";
	    }
	}
    else
        {
	info.cdsOutside="OUT2";
	}
    }
return bed;
}

struct bed *pslToBedList(struct psl *psl, struct dnaSeq *chrom, int mergeMax, boolean fixedOrientation)
/* Convert a psl to a list of beds, breaking up at gaps that are bigger than mergeMax,
 * and not introns. */
{
struct bed *bedList = NULL, *bed;
struct lm *lm = lmInit(0);
struct range *list = NULL, *el;
char strand = psl->strand[0];

/* Try and find genbank cds (unless we already had to flipped sequence, in which
 * case the CDS is pretty suspect!) */
struct genbankCds *cds = getCds(psl->qName);
if (cds != NULL)
    {
    info.genbankCds = "CDS";
    if (cds->startComplete && cds->endComplete && !cds->complement)
	info.genbankFullCds = "FULL";
    }

if (fixedOrientation) 
    cds = NULL;


/* Create first range from first block, and put it on list. */
int tStart = psl->tStarts[0];
int qStart = psl->qStarts[0];
int blockSize = psl->blockSizes[0];
lmAllocVar(lm, list);
list->start = tStart;
list->end = tStart + blockSize;

int i;
for (i=1; i<psl->blockCount; ++i)
    {
    /* Loop through.  At each position have three choices:
     *   1) Normal - add new range to existing list
     *   2) Merge - merge in block with current range
     *   3) Break - output range list so far and start new one with 
     *      this block. */
    int tEnd = tStart + blockSize;
    int qEnd = qStart + blockSize;
    tStart = psl->tStarts[i];
    qStart = psl->qStarts[i];
    int qGapStart = qEnd, qGapEnd = qStart;
    int tGapStart = tEnd, tGapEnd = tStart;
    int qGapSize = qGapEnd - qGapStart;
    int tGapSize = tGapEnd - tGapStart;
    char *iStart = chrom->dna + tEnd;
    char *iEnd = chrom->dna + tStart - 2;
    blockSize = psl->blockSizes[i];
    if (psl->strand[0] == '-')
        {
	reverseIntRange(&qGapStart, &qGapEnd, psl->qSize);
	}
    if (tGapSize <= mergeMax && qGapSize <= mergeMax)	
	{
	/* merge case - just extend previous block. */
	list->end = tStart + blockSize;
	info.mergedSmallGap = "GAP<7";

	/* If we're in the CDS region we might need to invalidate CDS. */
	if (cds != NULL && rangeIntersection(cds->start, cds->end, qGapStart, qGapEnd)>0)
	    {
	    /* If the actual CDS start/end is in a gap, then invalidate CDS. */
	    if ((qGapStart <= cds->start && cds->start < qGapEnd)
		|| (qGapStart <= cds->end && cds->end < qGapEnd))
		{
		cds = NULL;
		info.smallGapBustsFrame = "BUST<7a";
		verbose(3, "cds start/end in short gap for %s\n", psl->qName);
		}
	    else
	        {
		/* If number of bases inserted/deleted relative in genome relative
		 * to RNA is not multiple of 3, invalidate CDS. */
		int sizeDiff = qGapSize - tGapSize;
		if (sizeDiff < 0) 
			sizeDiff = -sizeDiff;
		if (sizeDiff % 3 != 0)
		    {
		    info.smallGapBustsFrame = "BUST<7b";
		    verbose(3, "small gap in %s introduces frame shift\n", psl->qName);
		    cds = NULL;
		    }
		}
	    }
	}
    else if (qGapSize > 1 || qGapSize < -1 || tGapSize < minIntronSize ||
        (strand == '+' && (iStart[0] != 'g' || (iStart[1] != 'c' && iStart[1] != 't') 
		|| iEnd[0] != 'a' || iEnd[1] != 'g' )) ||
        (strand == '-' && (iStart[0] != 'c' || iStart[1] != 't' 
		|| (iEnd[0] != 'a' && iEnd[0] != 'g') || iEnd[1] != 'c' )))
	 {
	 info.splitLargeGap = "SPLIT";
	 /* Break case. Not a short break or a gt/ag or gc/ag intron on either strand */
	 /* We might need to invalidate CDS. */
	 if (cds != NULL)
	    {
	    if (rangeIntersection(cds->start, cds->end, qGapStart, qGapEnd)>0)
		{
		/* Invalidate CDS if that's where break occurs and query gap not a multiple of 3. */
		if ((qGapStart <= cds->start && cds->start < qGapEnd)
		    || (qGapStart <= cds->end && cds->end < qGapEnd))
		    {
		    cds = NULL;
		    verbose(3, "cds start/end in long gap for %s\n", psl->qName);
		    info.largeGapBustsFrame = "BUST>7a";
		    }
		else if (qGapSize%3 != 0)
		    {
		    cds = NULL;
		    verbose(3, "large gap in %s introduces frame shift\n", psl->qName);
		    info.largeGapBustsFrame = "BUST>7b";
		    }
		}
	    }

	 slReverse(&list);
	 bed = rangeListToBed(list, psl, cds, 0);
	 if (bed)
	     slAddHead(&bedList, bed);
	 lmAllocVar(lm, list);
	 list->start = tStart;
	 list->end = tStart + blockSize;
	 }
    else
         {
	 if (cds != NULL && qGapSize != 0)
	    {
	    /* Here we probably "fixed" an intron at expense of CDS. */
	    if (rangeIntersection(cds->start, cds->end, qGapStart-1, qGapStart+1)>0)
		{
		cds = NULL;
		info.intronNudgeBustsFrame = "BUST<2";
		verbose(3, "fixed intron but broke CDS in %s\n", psl->qName);
		}
	    }
	 /* Normal case. */
	 lmAllocVar(lm, el);
	 el->start = tStart;
	 el->end = tStart + blockSize;
	 slAddHead(&list, el);
	 }
    }
slReverse(&list);
bed = rangeListToBed(list, psl, cds, 0);
if (bed)
    slAddHead(&bedList, bed);

lmCleanup(&lm);
slReverse(&bedList);
return bedList;
}

void txPsltoBed(char *inPsl, char *dnaPath, char *outBed)
/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
{
FILE *f = mustOpen(outBed, "w");
struct psl *psl, *pslList  = pslLoadAll(inPsl);
char *chromName = "";
struct nibTwoCache *ntc = nibTwoCacheNew(dnaPath);
struct dnaSeq *chrom = NULL;


verbose(2, "Loaded %d psls\n", slCount(pslList));

slSort(&pslList, pslCmpTarget);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    verbose(3, "Processing %s\n", psl->qName);

    /* Start recording our decision making here. */
    ZeroVar(&info);

    info.chrom = psl->tName;
    info.chromStart = psl->tStart;
    info.chromEnd = psl->tEnd;
    info.accession = psl->qName;
    info.genbankCds = ".";
    info.genbankFullCds = ".";
    info.intronNudged = ".";
    info.directionFlipped = ".";
    info.mergedSmallGap = ".";
    info.splitLargeGap = ".";
    info.splitLargGapInCds = ".";
    info.smallGapBustsFrame = ".";
    info.largeGapBustsFrame = ".";
    info.intronNudgeBustsFrame = ".";
    info.cdsOutside=".";
    info.outputCds = ".";

    if (!sameString(chromName, psl->tName))
	{
	dnaSeqFree(&chrom);
	chrom = nibTwoCacheSeq(ntc, psl->tName);
	toLowerN(chrom->dna, chrom->size);
	chromName = psl->tName;
	verbose(2, "Loaded %d bases in %s\n", chrom->size, chromName);
	}
    if (psl->tSize != chrom->size)
	errAbort("DNA and PSL out of sync. %s thinks %s is %d bases, but %s thinks it's %d.",
		 inPsl, chromName, psl->tSize, dnaPath, chrom->size);
    boolean fixedOrientation = fixOrientation(psl, chrom);
    if (fixedOrientation)
	info.directionFlipped = "FLIP";
    if (fixIntrons)
	{
	if (fixPslIntrons(psl, chrom))
	    info.intronNudged = "NUDGE";
	}
    struct bed *bedList = pslToBedList(psl, chrom, mergeMax, fixedOrientation);
    struct bed *bed;
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	bedTabOutN(bed, 12, f);
	}
    bedFreeList(&bedList);
    if (mapInfo != NULL)
        gbRnaMapInfoTabOut(&info, mapInfo);
    }
unusualPrint("total intron-looking %d, total funny-looking %d\n", 
	totalIntronLooking, totalFunny);
carefulClose(&f);
dnaSeqFree(&chrom);
nibTwoCacheFree(&ntc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
mergeMax = optionInt("mergeMax", mergeMax);
minSize = optionInt("minSize", minSize);
fixIntrons = !optionExists("noFixIntrons");
minIntronSize = optionInt("minIntronSize", minIntronSize);
if (optionExists("unusual"))
    unusualFile = mustOpen(optionVal("unusual", NULL), "w");
if (argc != 4)
    usage();
char *cdsFile = optionVal("cds", NULL);
if (cdsFile != NULL)
    cdsHash = hashCdsFile(cdsFile);
char *mapInfoFile = optionVal("mapInfo", NULL);
if (mapInfoFile != NULL)
    mapInfo = mustOpen(mapInfoFile, "w");
txPsltoBed(argv[1], argv[2], argv[3]);
carefulClose(&unusualFile);
carefulClose(&mapInfo);
return 0;
}
