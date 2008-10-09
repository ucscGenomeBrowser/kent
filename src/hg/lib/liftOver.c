/* liftOver - Move annotations from one assembly to another. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chain.h"
#include "chainNetDbLoad.h"
#include "bed.h"
#include "genePred.h"
#include "sample.h"
#include "hdb.h"
#include "liftOverChain.h"
#include "liftOver.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: liftOver.c,v 1.41 2008/10/09 01:49:44 braney Exp $";

struct chromMap
/* Remapping information for one (old) chromosome */
    {
    char *name;                 /* Chromosome name. */
    struct binKeeper *bk;       /* Keyed by old position, values are chains. */
    };

static char otherStrand(char c)
/* Swap +/- */
{
if (c == '-')
    return '+';
else if (c == '+')
    return '-';
else
    return c;
}

void readLiftOverMap(char *fileName, struct hash *chainHash)
/* Read map file into hashes. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chain *chain;
struct chromMap *map;
int chainCount = 0;

while ((chain = chainRead(lf)) != NULL)
    {
    if ((map = hashFindVal(chainHash, chain->tName)) == NULL)
	{
	AllocVar(map);
	map->bk = binKeeperNew(0, chain->tSize);
	hashAddSaveName(chainHash, chain->tName, map, &map->name);
	}
    binKeeperAdd(map->bk, chain->tStart, chain->tEnd, chain);
    ++chainCount;
    }
}

static struct binElement *findRange(struct hash *chainHash, 
                                char *chrom, int start, int end)
/* Find elements that intersect range. */
{
struct chromMap *map = hashFindVal(chainHash, chrom);
if (map == NULL)
    return NULL;
return binKeeperFind(map->bk, start, end);
}

static int chainAliSize(struct chain *chain)
/* Return size of all blocks in chain. */
{
struct cBlock *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += b->qEnd - b->qStart;
return total;
}

static int aliIntersectSize(struct chain *chain, int tStart, int tEnd)
/* How many bases in chain intersect region from tStart to tEnd */
{
int total = 0, one;
struct cBlock *b;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    one = rangeIntersection(tStart, tEnd, b->tStart, b->tEnd);
    if (one > 0)
	total += one;
    }
return total;
}

static boolean mapThroughChain(struct chain *chain, double minRatio, 
                            int *pStart, int *pEnd, struct chain **subChainRet)
/* Map interval from start to end from target to query side of chain.
 * Return FALSE if not possible, otherwise update *pStart, *pEnd. */
{
struct chain *subChain, *freeChain;
int s = *pStart, e = *pEnd;
int oldSize = e - s;
int newCover = 0;
int ok = TRUE;

chainSubsetOnT(chain, s, e, &subChain, &freeChain);
if (subChain == NULL)
    return FALSE;
newCover = chainAliSize(subChain);
if (newCover < oldSize * minRatio)
    ok = FALSE;
else if (chain->qStrand == '+')
    {
    *pStart = subChain->qStart;
    *pEnd = subChain->qEnd;
    }
else
    {
    *pStart = subChain->qSize - subChain->qEnd;
    *pEnd = subChain->qSize - subChain->qStart;
    }
*subChainRet = subChain;
//chainFree(&freeChain);
return ok;
}

static char *remapRange(struct hash *chainHash, double minRatio, 
                        int minSizeT, int minSizeQ, 
                        int minChainSizeT, int minChainSizeQ, 
                        char *chrom, int s, int e, char strand,
			int thickStart, int thickEnd, bool useThick,
			double minMatch,
                        char *regionName, char *db, char *chainTableName,
                        struct bed **bedRet, struct bed **unmappedBedRet)
/* Remap a range through chain hash.  If all is well return NULL
 * and results in a BED (or a list of BED's, if regionName is set (-multiple).
 * Otherwise return a string describing the problem. 
 * note: minSizeT is currently NOT implemented. feel free to add it.
 *       (its not e-s, its the boundaries of what maps of chrom:s-e to Q)
 */
{
struct binElement *list = findRange(chainHash, chrom, s, e), *el;
struct chain *chainsHit = NULL, 
                *chainsPartial = NULL, 
                *chainsMissed = NULL, *chain;
struct bed *bedList = NULL, *unmappedBedList = NULL;
struct bed *bed = NULL;
/* initialize for single region case */
int start = s, end = e;
double minMatchSize = minMatch * (end - start);
int intersectSize;
int tStart;
bool multiple = (regionName != NULL);

verbose(2, "%s:%d-%d", chrom, s, e);
verbose(2, multiple ? "\t%s\n": "\n", regionName);
for (el = list; el != NULL; el = el->next)
    {
    chain = el->val;
    if (multiple)
        {
        if (chain->qEnd - chain->qStart < minChainSizeQ ||
            chain->tEnd - chain->tStart < minChainSizeT)
                continue;
        /* limit required match to chain range on target */
        end = min(e, chain->tEnd);
        start = max(s, chain->tStart);
        minMatchSize = minMatch *  (end - start);
        }
    intersectSize = aliIntersectSize(chain, start, end);
    if (intersectSize >= minMatchSize)
	slAddHead(&chainsHit, chain);
    else if (intersectSize > 0)
	{
	slAddHead(&chainsPartial, chain);
	}
    else
	{
        /* shouldn't happen ? */
	slAddHead(&chainsMissed, chain);
	}
    }
slFreeList(&list);

if (chainsHit == NULL)
    {
    if (chainsPartial == NULL)
	return "Deleted in new";
    else if (chainsPartial->next == NULL)
	return "Partially deleted in new";
    else
	return "Split in new";
    }
else if (chainsHit->next != NULL && !multiple)
    {
    return "Duplicated in new";
    }
/* sort chains by position in target to order subregions by orthology */
slSort(&chainsHit, chainCmpTarget);

tStart = s;
for (chain = chainsHit; chain != NULL; chain = chain->next)
    {
    struct chain *subChain = NULL;
    int start=s, end=e;
    verbose(3,"hit chain %s:%d %s:%d-%d %c (%d)\n",
        chain->tName, chain->tStart,  chain->qName, chain->qStart, chain->qEnd,
        chain->qStrand, chain->id);
    if (multiple)
        {
        /* no real need to verify ratio again (it would require
         * adjusting coords again). */
        minRatio = 0;
        if (db)
            {
            /* use full chain, not the possibly truncated chain
             * from the net */
            struct chain *next = chain->next;
            chain = chainLoadIdRange(db, chainTableName,
                                        chrom, s, e, chain->id);
            chain->next = next;
            verbose(3,"chain from db %s:%d %s:%d-%d %c (%d)\n",
                chain->tName, chain->tStart,  chain->qName, 
                chain->qStart, chain->qEnd, chain->qStrand, chain->id);
            }
        }
    if (!mapThroughChain(chain, minRatio, &start, &end, &subChain))
        errAbort("Chain mapping error: %s:%d-%d\n", chain->qName, start, end);
    if (chain->qStrand == '-')
	strand = otherStrand(strand);
    if (useThick)
	if (!mapThroughChain(chain, minRatio, &thickStart, &thickEnd, &subChain))
	    thickStart = thickEnd = start;
    verbose(3, "mapped %s:%d-%d\n", chain->qName, start, end);
    verbose(4, "Mapped! Target:\t%s\t%d\t%d\t%c\tQuery:\t%s\t%d\t%d\t%c\n",
	    chain->tName, subChain->tStart, subChain->tEnd, strand, 
	    chain->qName, subChain->qStart, subChain->qEnd, chain->qStrand);
    if (multiple && end - start < minSizeQ)
        {
        verbose(2,"dropping %s:%d-%d size %d (too small)\n", 
                       chain->qName, start, end, end - start);
        continue;
        }
    AllocVar(bed);
    bed->chrom = cloneString(chain->qName);
    bed->chromStart = start;
    bed->chromEnd = end;
    if (regionName)
        bed->name = cloneString(regionName);
    bed->strand[0] = strand;
    bed->strand[1] = 0;
    if (useThick)
	{
	bed->thickStart = thickStart;
	bed->thickEnd = thickEnd;
	}
    slAddHead(&bedList, bed);

    if (tStart < subChain->tStart)
        {
        /* unmapped portion of target */
        AllocVar(bed);
        bed->chrom = cloneString(chain->tName);
        bed->chromStart = tStart;
        bed->chromEnd = subChain->tStart;
        if (regionName)
            bed->name = cloneString(regionName);
        slAddHead(&unmappedBedList, bed);
        }
    tStart = subChain->tEnd;
    }
slReverse(&bedList);
*bedRet = bedList;
slReverse(&unmappedBedList);
if (unmappedBedRet)
    *unmappedBedRet = unmappedBedList;
if (bedList==NULL)
    return "Deleted in new";
return NULL;
}

char *liftOverRemapRange(struct hash *chainHash, double minRatio,
                            char *chrom, int s, int e, char strand, 
                            double minMatch, char **retChrom, int *retStart, 
                            int *retEnd, char *retStrand)
/* Remap a range through chain hash.  If all is well return NULL
 * and results in retChrom, retStart, retEnd.  Otherwise
 * return a string describing the problem. */
{
struct bed *bed;
char *error;

error = remapRange(chainHash, minRatio, 0, 0, 0, 0, chrom, s, e, strand, 0, 0,
		   FALSE, minMatch, NULL, NULL, NULL, &bed, NULL);
if (error != NULL)
    return error;
if (retChrom)
    *retChrom = cloneString(bed->chrom);
if (retStart)
    *retStart = bed->chromStart;
if (retEnd)
    *retEnd = bed->chromEnd;
if (retStrand)
    *retStrand = bed->strand[0];
bedFree(&bed);
return NULL;
}

int chopLineBin(char *line, char *words[], int maxWord, 
                bool hasBin, bool tabSep)
/* Chop line by white space, leaving out bin value, if present */
{
int i;
int wordCount;
if (tabSep)
    wordCount = chopByChar(line, '\t', words, maxWord);
else
    wordCount = chopByWhite(line, words, maxWord);
if (hasBin)
    {
    wordCount--;
    for (i = 1; i <= wordCount; i++)
        words[i-1] = words[i];
    }
if (wordCount <= 0)
    return 0;
return wordCount;
}

int lineFileChopBin(struct lineFile *lf, char *words[], int maxWord, 
                        bool hasBin, bool tabSep)
/* Return next non-blank line that doesn't start with '#' chopped into words,
   leaving out the bin value, if present. */
{
int wordCount;
if (tabSep)
    wordCount = lineFileChopCharNext(lf, '\t', words, maxWord);
else
    wordCount = lineFileChopNext(lf, words, maxWord);
if (hasBin)
    {
    int i;
    wordCount--;
    for (i = 1; i <= wordCount; i++)
        words[i-1] = words[i];
    }
if (wordCount <= 0)
    return 0;
return wordCount;
}

static int bedOverSmall(struct lineFile *lf, int fieldCount, 
                        struct hash *chainHash, double minMatch, int minSizeT, 
                        int minSizeQ, int minChainT, int minChainQ, 
			FILE *mapped, FILE *unmapped, bool multiple, 
                        char *chainTable, int bedPlus, bool hasBin, 
                        bool tabSep, int *errCt)
/* Do a bed without a block-list.
 * NOTE: it would be preferable to have all of the lift
 * functions work at the line level, rather than the file level.
 * Multiple option can be used with bed3 -- it will write a list of
 * regions as a bed4, where score is the "part #". This is used for
 * ENCODE region mapping */  
{
int i, wordCount, s, e;
char *words[20], *chrom;
char strand = '.', strandString[2];
char *error;
int ct = 0;
int errs = 0;
struct bed *bedList = NULL, *unmappedBedList = NULL;
int totalUnmapped = 0;
double unmappedRatio;
int totalUnmappedAll = 0;
int totalBases = 0;
double mappedRatio;
char *region = NULL;   /* region name from BED file-- used with  -multiple */
char *db = NULL, *chainTableName = NULL;

if (chainTable)
    {
    chainTableName = chopPrefix(chainTable);
    db = chainTable;
    chopSuffix(chainTable);
    }
while ((wordCount = 
            lineFileChopBin(lf, words, ArraySize(words), hasBin, tabSep)) != 0)
    {
    FILE *f = mapped;
    chrom = words[0];
    s = lineFileNeedFullNum(lf, words, 1);
    e = lineFileNeedFullNum(lf, words, 2);
    bool useThick = FALSE;
    int thickStart = 0, thickEnd = 0;
    if (s > e)
	errAbort(
	"Start address is after end address on line %d of bed file %s", 
	    lf->lineIx, lf->fileName);
    if (multiple)
        {
        if (wordCount < 4 || wordCount > 6)
            errAbort("Can only lift BED4, BED5, BED6 to multiple regions");
        region = words[3];
        }
    if (wordCount >= 6 && (bedPlus == 0 || bedPlus >= 6))
	strand = words[5][0];
    if (wordCount >= 8 && (bedPlus == 0 || bedPlus >= 8))
	{
	useThick = TRUE;
	thickStart = lineFileNeedFullNum(lf, words, 6);
	thickEnd = lineFileNeedFullNum(lf, words, 7);
	}
    error = remapRange(chainHash, minMatch, minSizeT, minSizeQ, minChainT, 
		       minChainQ, chrom, s, e, strand,
		       thickStart, thickEnd, useThick, minMatch, 
		       region, db, chainTableName, &bedList, &unmappedBedList);
    if (error == NULL)
        {
        /* successfully mapped */
        int ix = 1;
        struct bed *bed, *next = bedList->next;
        for (bed = bedList; bed != NULL; bed = next)
            {
            if (hasBin)
                fprintf(f, "%d\t", 
                        binFromRange(bed->chromStart, bed->chromEnd));
            fprintf(f, "%s\t%d\t%d", bed->chrom, 
                                    bed->chromStart, bed->chromEnd);
            if (multiple)
                {
                /* region name and part number */
                fprintf(f, "\t%s\t%d", region, ix++);
                if (wordCount == 6)
                    fprintf(f, "\t%c", bed->strand[0]);
                }
            else
                {
                for (i=3; i<wordCount; ++i)
                    {
                    if (i == 5 && (bedPlus == 0 || bedPlus >= 6))
                        /* get strand from remap */
                        fprintf(f, "\t%c", bed->strand[0]);
		    else if (i == 6 && useThick)
                        /* get thickStart from remap */
                        fprintf(f, "\t%d", bed->thickStart);
		    else if (i == 7 && useThick)
                        /* get thickEnd from remap */
                        fprintf(f, "\t%d", bed->thickEnd);
                    else
                        /* everything else just passed through */
                        fprintf(f, "\t%s", words[i]);
                    }
                }
            fprintf(f, "\n");
            next = bed->next;
            bedFree(&bed);
            }
        /* track how many successfully mapped */
        ct++;

        totalUnmapped = 0;
        for (bed = unmappedBedList; bed != NULL; bed = bed->next)
            {
            int size = bed->chromEnd - bed->chromStart;
            totalUnmapped += size;
            verbose(2, "Unmapped: %s:%d-%d (size %d) %s\n",
                        bed->chrom, bed->chromStart, bed->chromEnd,
                        size, bed->name);
            }
        unmappedRatio = (double)(totalUnmapped * 100) / (e - s);
        verbose(2, "Unmapped total: %s\t%5.1f%%\t%7d\n", 
                            region, unmappedRatio, totalUnmapped);
        totalUnmappedAll += totalUnmapped;
        totalBases += (e - s);
        }
    else
	{
        /* couldn't map */
	f = unmapped;
        strandString[0] = strand;
        strandString[1] = 0;
        words[5] = strandString;
	fprintf(f, "#%s\n", error);
        fprintf(f, "%s\t%d\t%d", chrom, s, e);
        for (i=3; i<wordCount; ++i)
            fprintf(f, "\t%s", words[i]);
        fprintf(f, "\n");
        errs++;
        }
    }
if (errCt)
    *errCt = errs;
mappedRatio = (totalBases - totalUnmappedAll)*100.0 / totalBases;
verbose(2, "Mapped bases: \t%5.0f%%\n", mappedRatio);
return ct;
}

static void shortGffLine(struct lineFile *lf)
/* Complain about short line in GFF and abort. */
{
errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
}

static int gffNeedNum(struct lineFile *lf, char *s)
/* Convert s to an integer or die trying. */
{
char c = *s;
if (isdigit(c) || c == '-')
    return atoi(s);
else
    errAbort("Expecting number line %d of %s", lf->lineIx, lf->fileName);
return 0;
}

void liftOverGff(char *fileName, struct hash *chainHash, 
                                double minMatch, double minBlocks, 
                                FILE *mapped, FILE *unmapped)
/* Lift over GFF file */
{
char *error = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char c, *s, *line, *word;
char *seq, *source, *feature;
int start, end;
char *score, *strand;
FILE *f;

while (lineFileNext(lf, &line, NULL))
    {
    /* Pass through blank lines and those that start with a sharp. */
    s = skipLeadingSpaces(line);
    c = *s;
    if (c == '#' || c == 0)
	{
	fprintf(mapped, "%s\n", line);
	continue;
	}
    if ((seq = nextWord(&s)) == NULL)
	shortGffLine(lf);
    if ((source = nextWord(&s)) == NULL)
	shortGffLine(lf);
    if ((feature = nextWord(&s)) == NULL)
	shortGffLine(lf);
    if ((word = nextWord(&s)) == NULL)
	shortGffLine(lf);
    start = gffNeedNum(lf, word) - 1;
    if ((word = nextWord(&s)) == NULL)
	shortGffLine(lf);
    end = gffNeedNum(lf, word);
    if ((score = nextWord(&s)) == NULL)
	shortGffLine(lf);
    if ((strand = nextWord(&s)) == NULL)
	shortGffLine(lf);
    s = skipLeadingSpaces(s);

    /* Convert seq/start/end/strand. */
    error = liftOverRemapRange(chainHash, minMatch, seq, start, end, *strand,
	                minMatch, &seq, &start, &end, strand);
    f = mapped;
    if (error != NULL)
	{
	f = unmapped;
	fprintf(f, "# %s\n", error);
	}
    fprintf(f, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
	seq, source, feature, start+1, end, score, strand, s);
    }
}

struct liftRange
/* A start/stop pair. */
     {
     struct liftRange *next;
     int start;		/* Start 0 based */
     int end;		/* End, non-inclusive. */
     int val;		/* Some value (optional). */
     };

static struct liftRange *bedToRangeList(struct bed *bed)
/* Convert blocks in bed to a range list. */
{
struct liftRange *range, *rangeList = NULL;
int bedStart = bed->chromStart;
int i, count = bed->blockCount, start;
for (i=0; i<count; ++i)
    {
    AllocVar(range);
    start = bedStart + bed->chromStarts[i];
    range->start = start;
    range->end = start + bed->blockSizes[i];
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}

static struct liftRange *tPslToRangeList(struct psl *psl)
/* Convert target blocks in psl to a range list. */
{
struct liftRange *range, *rangeList = NULL;
int i, count = psl->blockCount, start;
for (i=0; i<count; ++i)
    {
    AllocVar(range);
    start = psl->tStarts[i];
    range->start = start;
    range->end = start + psl->blockSizes[i];
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}

#if 0 /* not used */
static struct liftRange *qPslToRangeList(struct psl *psl)
/* Convert query blocks in psl to a range list. */
{
struct liftRange *range, *rangeList = NULL;
int pslStart = psl->qStart;
int i, count = psl->blockCount, start;
for (i=0; i<count; ++i)
    {
    AllocVar(range);
    start = pslStart + psl->qStarts[i];
    range->start = start;
    range->end = start + psl->blockSizes[i];
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}
#endif

static int chainRangeIntersection(struct chain *chain, struct liftRange *rangeList)
/* Return chain/rangeList intersection size. */
{
struct cBlock *b = chain->blockList;
struct liftRange *r = rangeList;
int one, total = 0;


if (b == NULL || r == NULL)
    return 0;
for (;;)
    {
    while (b->tEnd < r->start)
	{
	b = b->next;
	if (b == NULL)
	    return total;
	}
    while (r->end < b->tStart)
	{
	r = r->next;
	if (r == NULL)
	    return total;
	}
    one = rangeIntersection(b->tStart, b->tEnd, r->start, r->end);
    if (one > 0)
	total += one;
    if (b->tEnd <= r->end)
	{
	b = b->next;
	if (b == NULL)
	    return total;
	}
    else
	{
	r = r->next;
	if (r == NULL)
	    return total;
	}
    }
}

static void remapRangeList(struct chain *chain, struct liftRange **pRangeList,
            int *pThickStart, int *pThickEnd, double minBlocks, bool fudgeThick,
            struct liftRange **retGood, struct liftRange **retBad, char **retError)
/* Remap range list through chain.  Return error message on failure,
 * NULL on success. */
{
struct cBlock *b = chain->blockList;
struct liftRange *r = *pRangeList, *nextR, *goodList = NULL, *badList = NULL;
int bDiff, rStart = 0;
bool gotStart = FALSE;
int rCount = slCount(r), goodCount = 0;
int thickStart = *pThickStart, thickEnd = *pThickEnd;
int fudgeThickStart = 0, fudgeThickEnd = 0;
bool gotThickStart = FALSE, gotThickEnd = FALSE;
bool gotFudgeThickStart = FALSE;
bool needThick = (thickStart != thickEnd);
boolean done = FALSE;
static char bErr[512];
char *err = NULL;

*pRangeList = NULL;
if (r == NULL)
    {
    *retGood = *retBad = NULL;
    *retError = NULL;
    return;
    }
if (b == NULL)
    {
    *retGood = NULL;
    *retBad = r;
    *retError = "Empty block list in intersecting chain";
    return;
    }
nextR = r->next;
for (;;)
    {
    /* Skip over chain blocks that end before range starts. */
    while (b->tEnd <= r->start)
	{
	b = b->next;
	if (b == NULL)
	    {
	    done = TRUE;
	    break;
	    }
	}
    if (done) 
	break;

    /* Put any range blocks that end before block starts on badList */
    while (r->end <= b->tStart)
	{
	slAddHead(&badList, r);
	r = nextR;
	if (r == NULL)
	    {
	    done = TRUE;
	    break;
	    }
	nextR = r->next;
	gotStart = FALSE;
	}
    if (done) 
	break;

    /* Map start and end of 'thick area' in a slightly picky fashion. */
    if (needThick)
	{
	if (b->tStart <= thickStart && thickStart < b->tEnd)
	    {
	    *pThickStart = thickStart + b->qStart - b->tStart;
	    gotThickStart = TRUE;
	    fudgeThickStart = *pThickStart;
	    gotFudgeThickStart = TRUE;
	    }
	if (b->tStart <= thickEnd && thickEnd <= b->tEnd)
	    {
	    *pThickEnd = thickEnd + b->qStart - b->tStart;
	    gotThickEnd = TRUE;
	    fudgeThickEnd = *pThickEnd;
	    }
	if (!gotFudgeThickStart && thickStart < b->tEnd)
	    {
	    fudgeThickStart = b->qStart;
	    gotFudgeThickStart = TRUE;
	    }
	if (b->tEnd <= thickEnd)
	    {
	    fudgeThickEnd = b->qEnd;
	    }
	}

    if (b->tStart <= r->start && r->start < b->tEnd && !gotStart)
	{
	gotStart = TRUE;
	bDiff = b->qStart - b->tStart;
	rStart = r->start + bDiff;
	}
    if (b->tStart < r->end && r->end <= b->tEnd)
	{
	bDiff = b->qStart - b->tStart;
	if (gotStart)
	    {
	    r->start = rStart;
	    r->end += bDiff;
	    slAddHead(&goodList, r);
	    ++goodCount;
	    }
	else
	    {
	    slAddHead(&badList, r);
	    }
	r = nextR;
	if (r == NULL)
	    {
	    done = TRUE;
	    break;
	    }
	nextR = r->next;
	gotStart = FALSE;
	}
    if (done) 
	break;
    if (b->tEnd <= r->end)
	{
	b = b->next;
	if (b == NULL)
	    {
	    done = TRUE;
	    break;
	    }
	}
    if (done) 
	break;
    }
slReverse(&goodList);
slReverse(&badList);
if (needThick)
    {
    if (goodList != NULL && !gotFudgeThickStart)
	fudgeThickStart = fudgeThickEnd = goodList->start;
    if (!gotThickStart)
	{
	if (fudgeThick)
	    {
	    if (goodList != NULL)
		*pThickStart = fudgeThickStart;
	    }
	else
	    err = "Can't find thickStart/thickEnd";
	}
    if (!gotThickEnd)
	{
	if (fudgeThick)
	    {
	    if (goodList != NULL)
		*pThickEnd = fudgeThickEnd;
	    }
	else
	    err = "Can't find thickStart/thickEnd";
	}
    }
else
    {
    if (goodList != NULL)
        *pThickStart = *pThickEnd = goodList->start;
    }
if (goodCount != rCount)
    {
    double goodRatio = (double)goodCount / rCount;
    if (goodRatio < minBlocks)
	{
	safef(bErr, sizeof(bErr),
	      "Boundary problem: need %d, got %d, diff %d, mapped %.1f",
	      rCount, goodCount, rCount - goodCount, goodRatio);
	err = bErr;
	}
    }
*retGood = goodList;
*retBad = badList;
*retError = err;
}

#ifdef DEBUG
static void dumpRangeList(struct liftRange *rangeList, FILE *f)
/* Write out range list to file. */
{
struct liftRange *range;
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->end - range->start);
fprintf(f, "\n");
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->start);
fprintf(f, "\n");
}
#endif /* DEBUG */

static int sumBedBlocks(struct bed *bed)
/* Calculate sum of all block sizes in bed. */
{
int i, total = 0;
for (i=0; i<bed->blockCount; ++i)
    total += bed->blockSizes[i];
return total;
}

static int sumPslBlocks(struct psl *psl)
/* Calculate sum of all block sizes in psl. */
{
int i, total = 0;
for (i=0; i<psl->blockCount; ++i)
    total += psl->blockSizes[i];
return total;
}

static struct liftRange *reverseRangeList(struct liftRange *rangeList, int chromSize)
/* Return reverse-complemented rangeList. */
{
struct liftRange *range;
slReverse(&rangeList);
for (range = rangeList; range != NULL; range = range->next)
    reverseIntRange(&range->start, &range->end, chromSize);
return rangeList;
}

static char *remapBlockedBed(struct hash *chainHash, struct bed *bed, 
                            double minMatch, double minBlocks, bool fudgeThick)
/* Remap blocks in bed, and also chromStart/chromEnd.  
 * Return NULL on success, an error string on failure. */
{
struct chain *chainList = NULL,  *chain;
int bedSize = sumBedBlocks(bed);
struct binElement *binList;
struct binElement *el;
struct liftRange *rangeList, *badRanges = NULL, *range;
char *error = NULL;
int i, start, end = 0;
int thickStart = bed->thickStart;
int thickEnd = bed->thickEnd;

binList = findRange(chainHash, bed->chrom, bed->chromStart, bed->chromEnd);
if (binList == NULL)
    return "Deleted in new";

/* Convert bed blocks to range list. */
rangeList = bedToRangeList(bed);

/* Evaluate all intersecting chains and sort so best is on top. */
for (el = binList; el != NULL; el = el->next)
    {
    chain = el->val;
    chain->score = chainRangeIntersection(chain, rangeList);
    slAddHead(&chainList, chain);
    }
slSort(&chainList, chainCmpScore);

/* See if duplicated. */
chain = chainList->next;
if (chain != NULL && chain->score == chainList->score)
    error = "Duplicated in new";
chain = chainList;

/* See if best one is good enough. */
if (chain->score  < minMatch * bedSize)
    error = "Partially deleted in new";


/* Call subroutine to remap range list. */
if (error == NULL)
    {
    remapRangeList(chain, &rangeList, &thickStart, &thickEnd, 
                        minBlocks, fudgeThick,
    	                &rangeList, &badRanges, &error);
    }

/* Convert rangeList back to bed blocks.  Also calculate start and end. */
if (error == NULL)
    {
    if (chain->qStrand == '-')
	{
	rangeList = reverseRangeList(rangeList, chain->qSize);
	reverseIntRange(&thickStart, &thickEnd, chain->qSize);
	bed->strand[0] = otherStrand(bed->strand[0]);
	}
    bed->chromStart = start = rangeList->start;
    bed->blockCount = slCount(rangeList);
    for (i=0, range = rangeList; range != NULL; range = range->next, ++i)
	{
	end = range->end;
	bed->blockSizes[i] = end - range->start;
	bed->chromStarts[i] = range->start - start;
	}
    if (!sameString(chain->qName, chain->tName))
	{
	freeMem(bed->chrom);
	bed->chrom = cloneString(chain->qName);
	}
    bed->chromEnd = end;
    bed->thickStart = thickStart;
    bed->thickEnd = thickEnd;
    }
slFreeList(&rangeList);
slFreeList(&badRanges);
slFreeList(&binList);
return error;
}

static int bedOverBig(struct lineFile *lf, int refCount, 
                    struct hash *chainHash, double minMatch, double minBlocks,
                    bool fudgeThick, FILE *mapped, FILE *unmapped, int bedPlus,
                    bool hasBin, bool tabSep, int *errCt)
/* Do a bed with block-list. */
{
int wordCount, bedCount;
char *line, *words[64];
char *whyNot = NULL;
int ct = 0;
int errs = 0;
int i;

while (lineFileNextReal(lf, &line))
    {
    struct bed *bed;
    wordCount = chopLineBin(line, words, ArraySize(words), hasBin, tabSep);
    if (refCount != wordCount)
	lineFileExpectWords(lf, refCount, wordCount);
    if (wordCount == bedPlus)
        bedPlus = 0;    /* no extra fields */
    bedCount = (bedPlus ? bedPlus : wordCount);
    bed = bedLoadN(words, bedCount);
    whyNot = remapBlockedBed(chainHash, bed, minMatch, minBlocks, fudgeThick);
    if (whyNot == NULL)
	{
        if (hasBin)
            fprintf(mapped, "%d\t", 
                        binFromRange(bed->chromStart, bed->chromEnd));
	bedOutputN(bed, bedCount, mapped, '\t', 
                (bedCount != wordCount) ? '\t':'\n');
        /* print extra "non-bed" fields in line */
        for (i = bedCount; i < wordCount; i++)
            fprintf(mapped, "%s%c", words[i], (i == wordCount-1) ? '\n':'\t');
        ct++;
	}
    else
	{
	fprintf(unmapped, "#%s\n", whyNot);
	bedOutputN(bed, bedCount, unmapped, '\t', 
                (bedCount != wordCount) ? '\t':'\n');
        /* print extra "non-bed" fields in line */
        for (i = bedCount; i < wordCount; i++)
            fprintf(unmapped, "%s%c", words[i], 
                                (i == wordCount-1) ? '\n':'\t');
        errs++;
	}
    bedFree(&bed);
    }
if (errCt)
    *errCt = errs;
return ct;
}

int liftOverBedPlus(char *fileName, struct hash *chainHash, double minMatch,  
                    double minBlocks, int minSizeT, int minSizeQ, int minChainT,
                    int minChainQ, bool fudgeThick, FILE *f, FILE *unmapped, 
                    bool multiple, char *chainTable, int bedPlus, bool hasBin, 
                    bool tabSep, int *errCt)
/* Lift bed N+ file.
 * Return the number of records successfully converted */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
int bedFieldCount = bedPlus;
char *line;
char *words[64];
int ct = 0;

if (lineFileNextReal(lf, &line))
    {
    line = cloneString(line);
    if (tabSep)
        wordCount = chopByChar(line, '\t', words, ArraySize(words));
    else
        wordCount = chopLine(line, words);
    if (hasBin)
        wordCount--;
    lineFileReuse(lf);
    freez(&line);
    if (wordCount < 3)
	 errAbort("Data format error: expecting at least 3 fields in BED file (%s)", fileName);
    if (bedFieldCount == 0)
        bedFieldCount = wordCount;
    if (bedFieldCount <= 10)
	 ct = bedOverSmall(lf, wordCount, chainHash, minMatch, 
                        minSizeT, minSizeQ, minChainT, minChainQ, f, unmapped, 
                        multiple, chainTable, bedPlus, hasBin, tabSep, errCt);
    else
	 ct = bedOverBig(lf, wordCount, chainHash, minMatch, minBlocks, 
                        fudgeThick, f, unmapped, bedPlus, hasBin, tabSep, errCt);
    }
lineFileClose(&lf);
return ct;
}

int liftOverBed(char *fileName, struct hash *chainHash, 
                        double minMatch,  double minBlocks, 
                        int minSizeT, int minSizeQ,
                        int minChainT, int minChainQ,
                        bool fudgeThick, FILE *f, FILE *unmapped, 
                        bool multiple, char *chainTable, int *errCt)
/* Open up file, decide what type of bed it is, and lift it. 
 * Return the number of records successfully converted */
{
return liftOverBedPlus(fileName, chainHash, minMatch, minBlocks,
                        minSizeT, minSizeQ, minChainT, minChainQ,
                        fudgeThick, f, unmapped, multiple, chainTable,
                        0, FALSE, FALSE, errCt);
}

#define LIFTOVER_FILE_PREFIX    "liftOver"
#define BEDSTART_TO_POSITION(coord)     (coord+1)

int liftOverPositions(char *fileName, struct hash *chainHash, 
                        double minMatch,  double minBlocks, 
                        int minSizeT, int minSizeQ,
                        int minChainT, int minChainQ,
		        bool fudgeThick, FILE *mapped, FILE *unmapped, 
		        bool multiple, char *chainTable, int *errCt)
/* Create bed file from positions (chrom:start-end) and lift.
 * Then convert back to positions.  (TODO: line-by-line instead of by file)
 * Return the number of records successfully converted */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *words[32];
int wordCount;
int ct = 0;
struct tempName bedTn, mappedBedTn, unmappedBedTn;
FILE *bedFile;
char *chrom;
int start, end;
FILE *mappedBed, *unmappedBed;

/* OK to use forCgi here ?? What if used from command-line ? */
makeTempName(&bedTn, LIFTOVER_FILE_PREFIX, ".bed");
bedFile = mustOpen(bedTn.forCgi, "w");

/* Convert input to bed file */
while (lineFileNextReal(lf, &line))
    {
    line = stripCommas(line);
    if (hgParseChromRange(NULL, line, &chrom, &start, &end))
        fprintf(bedFile, "%s\t%d\t%d\n", chrom, start, end);
    else
        {
        /* let the bed parser worry about it */
        // line = trimSpaces(line);
        fprintf(bedFile, "%s\n", line);
        }
    freez(&line);
    }
carefulClose(&bedFile);
chmod(bedTn.forCgi, 0666);
lineFileClose(&lf);

/* Set up temp bed files for output, and lift to those */
makeTempName(&mappedBedTn, LIFTOVER_FILE_PREFIX, ".bedmapped");
makeTempName(&unmappedBedTn, LIFTOVER_FILE_PREFIX, ".bedunmapped");
mappedBed = mustOpen(mappedBedTn.forCgi, "w");
unmappedBed = mustOpen(unmappedBedTn.forCgi, "w");
ct = liftOverBed(bedTn.forCgi, chainHash, minMatch, minBlocks, minSizeT, minSizeQ, minChainT, minChainQ, fudgeThick, 
                        mappedBed, unmappedBed, multiple, chainTable, errCt);
carefulClose(&mappedBed);
chmod(mappedBedTn.forCgi, 0666);
carefulClose(&unmappedBed);
chmod(unmappedBedTn.forCgi, 0666);
lineFileClose(&lf);

/* Convert output files back to positions */
lf = lineFileOpen(mappedBedTn.forCgi, TRUE);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    chrom = words[0];
    start = lineFileNeedNum(lf, words, 1);
    end = lineFileNeedNum(lf, words, 2);
    fprintf(mapped, "%s:%d-%d\n", chrom, BEDSTART_TO_POSITION(start), end);
    }
carefulClose(&mapped);
lineFileClose(&lf);

lf = lineFileOpen(unmappedBedTn.forCgi, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#')
        fprintf(unmapped, "%s\n", line);
    else
        {
        wordCount = chopLine(line, words);
        chrom = words[0];
        start = lineFileNeedNum(lf, words, 1);
        end = lineFileNeedNum(lf, words, 2);
        fprintf(unmapped, "%s:%d-%d\n", chrom, 
                        BEDSTART_TO_POSITION(start), end);
        }
    }
carefulClose(&unmapped);
lineFileClose(&lf);
return ct;
}

static char *remapBlockedPsl(struct hash *chainHash, struct psl *psl, 
                            double minMatch, double minBlocks, bool fudgeThick)
/* Remap blocks in psl, and also chromStart/chromEnd.  
 * Return NULL on success, an error string on failure. */
{
struct chain *chainList = NULL,  *chain;
int pslSize = sumPslBlocks(psl);
struct binElement *binList;
struct binElement *el;
struct liftRange *rangeList, *badRanges = NULL, *range;
char *error = NULL;
int i, start, end = 0;
//int pslStart = psl->tStart;
//int pslEnd = psl->tEnd;
int thick = 0;

binList = findRange(chainHash, psl->tName, psl->tStart, psl->tEnd);
if (binList == NULL)
    return "Deleted in new";

/* Convert psl target  blocks to range list. */
rangeList = tPslToRangeList(psl);

/* Evaluate all intersecting chains and sort so best is on top. */
for (el = binList; el != NULL; el = el->next)
    {
    chain = el->val;
    chain->score = chainRangeIntersection(chain, rangeList);
    slAddHead(&chainList, chain);
    }
slSort(&chainList, chainCmpScore);

/* See if duplicated. */
chain = chainList->next;
if (chain != NULL && chain->score == chainList->score)
    error = "Duplicated in new";
chain = chainList;

/* See if best one is good enough. */
if (chain->score  < minMatch * pslSize)
    error = "Partially deleted in new";

/* Call subroutine to remap range list. */
if (error == NULL)
    {
    remapRangeList(chain, &rangeList, &thick, &thick, minBlocks, fudgeThick,
    	                &rangeList, &badRanges, &error);
    }


/* Convert rangeList back to psl blocks.  Also calculate start and end. */
if (error == NULL)
    {
    if (chain->qStrand == '-')
	{
	rangeList = reverseRangeList(rangeList, chain->qSize);
//	reverseIntRange(&pslStart, &pslEnd, chain->qSize);
	psl->strand[0] = otherStrand(psl->strand[0]);
	}
    psl->tStart = start = rangeList->start;
    psl->blockCount = slCount(rangeList);
    for (i=0, range = rangeList; range != NULL; range = range->next, ++i)
	{
	end = range->end;
	psl->blockSizes[i] = end - range->start;
	psl->tStarts[i] = range->start;
	}
    if (!sameString(chain->qName, chain->tName))
	{
	freeMem(psl->tName);
	psl->tName = cloneString(chain->qName);
	}
    psl->tSize = chain->qSize;
    psl->tEnd = end;
//    psl->tStart = pslStart;
//    psl->tEnd = pslEnd;
    }
slFreeList(&rangeList);
slFreeList(&badRanges);
slFreeList(&binList);
return error;
}

static void pslOver(struct lineFile *lf, struct hash *chainHash, 
                    double minMatch, double minBlocks, bool fudgeThick,
                    FILE *mapped, FILE *unmapped)
/* Do a psl with block-list. */
{
char *whyNot = NULL;
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    whyNot = remapBlockedPsl(chainHash, psl, minMatch, minBlocks, fudgeThick);
    if (whyNot == NULL)
	{
	pslTabOut(psl, mapped);
	}
    else
	{
	fprintf(unmapped, "#%s\n", whyNot);
	pslTabOut(psl, unmapped);
	}
    pslFree(&psl);
    }
}

void liftOverPsl(char *fileName, struct hash *chainHash, 
                                double minMatch, double minBlocks, bool fudgeThick,
                                FILE *f, FILE *unmapped)
/* Open up file, and lift it. */
{
struct lineFile *lf = pslFileOpen(fileName);

pslOver(lf, chainHash, minMatch, minBlocks, fudgeThick, f, unmapped);
lineFileClose(&lf);
}

struct bed *genePredToBed(struct genePred *gp)
/* Convert genePred to bed.  */
{
struct bed *bed;
int count, i, start;

AllocVar(bed);
bed->chrom = cloneString(gp->chrom);
bed->chromStart = start = gp->txStart;
bed->chromEnd = gp->txEnd;
bed->name = cloneString(gp->name);
bed->strand[0] = gp->strand[0];
bed->thickStart = gp->cdsStart;
bed->thickEnd = gp->cdsEnd;
bed->blockCount = count = gp->exonCount;
AllocArray(bed->blockSizes, count);
AllocArray(bed->chromStarts, count);
for (i=0; i<count; ++i)
    {
    int s = gp->exonStarts[i];
    int e = gp->exonEnds[i];
    bed->blockSizes[i] = e - s;
    bed->chromStarts[i] = s - start;
    }
return bed;
}

void liftOverGenePred(char *fileName, struct hash *chainHash, 
                        double minMatch, double minBlocks, bool fudgeThick,
                        FILE *mapped, FILE *unmapped)
/* Lift over file in genePred format. */
{
struct bed *bed;
struct genePred *gp = NULL;
char *error;
FILE *f;
struct genePred *gpList = genePredExtLoadAll(fileName);
for (gp = gpList ; gp != NULL ; gp = gp->next)
    {
    // uglyf("%s %s %d %d %s\n", gp->name, gp->chrom, gp->txStart, gp->txEnd, gp->strand);
    f = mapped;
    bed = genePredToBed(gp);
    error = remapBlockedBed(chainHash, bed, minMatch, minBlocks, fudgeThick);
    if (error)
	{
	f = unmapped;
	fprintf(unmapped, "# %s\n", error);
	}
   else
	{
	int count, i, start;
	freeMem(gp->chrom);
	gp->chrom = cloneString(bed->chrom);
	gp->txStart = start = bed->chromStart;
	gp->txEnd = bed->chromEnd;
	gp->strand[0] = bed->strand[0];
	gp->cdsStart = bed->thickStart;
	gp->cdsEnd = bed->thickEnd;
	gp->exonCount = count = bed->blockCount;
	for (i=0; i<count; ++i)
	    {
	    int s = start + bed->chromStarts[i];
	    int e = s + bed->blockSizes[i];
	    gp->exonStarts[i] = s;
	    gp->exonEnds[i] = e;
	    }
	}
    genePredTabOut(gp, f);
    bedFree(&bed);
//    genePredFree(&gp);
    }
}

#ifdef example
static char *remapBlockedBed(struct hash *chainHash, struct bed *bed)
/* Remap blocks in bed, and also chromStart/chromEnd.  
 * Return NULL on success, an error string on failure. */
{
struct chain *chainList = NULL,  *chain, *subChain, *freeChain;
int bedSize = sumBedBlocks(bed);
struct binElement *binList, *el;
struct liftRange *rangeList, *badRanges = NULL, *range;
char *error = NULL;
int i, start, end = 0;
int thickStart = bed->thickStart;
int thickEnd = bed->thickEnd;

binList = findRange(chainHash, bed->chrom, bed->chromStart, bed->chromEnd);
if (binList == NULL)
    return "Deleted in new";

/* Convert bed blocks to range list. */
rangeList = bedToRangeList(bed);

/* Evaluate all intersecting chains and sort so best is on top. */
for (el = binList; el != NULL; el = el->next)
    {
    chain = el->val;
    chain->score = chainRangeIntersection(chain, rangeList);
    slAddHead(&chainList, chain);
    }
slSort(&chainList, chainCmpScore);

/* See if duplicated. */
chain = chainList->next;
if (chain != NULL && chain->score == chainList->score)
    error = "Duplicated in new";
chain = chainList;

/* See if best one is good enough. */
if (chain->score  < minMatch * bedSize)
    error = "Partially deleted in new";


/* Call subroutine to remap range list. */
if (error == NULL)
    {
    remapRangeList(chain, &rangeList, &thickStart, &thickEnd, minBlocks,
    	                &rangeList, &badRanges, &error);
    }

/* Convert rangeList back to bed blocks.  Also calculate start and end. */
if (error == NULL)
    {
    if (chain->qStrand == '-')
	{
	struct liftRange *range;
	slReverse(&rangeList);
	for (range = rangeList; range != NULL; range = range->next)
	    reverseIntRange(&range->start, &range->end, chain->qSize);
	reverseIntRange(&thickStart, &thickEnd, chain->qSize);
	bed->strand[0] = otherStrand(bed->strand[0]);
	}
    bed->chromStart = start = rangeList->start;
    bed->blockCount = slCount(rangeList);
    for (i=0, range = rangeList; range != NULL; range = range->next, ++i)
	{
	end = range->end;
	bed->blockSizes[i] = end - range->start;
	bed->chromStarts[i] = range->start - start;
	}
    if (!sameString(chain->qName, chain->tName))
	{
	freeMem(bed->chrom);
	bed->chrom = cloneString(chain->qName);
	}
    bed->chromEnd = end;
    bed->thickStart = thickStart;
    bed->thickEnd = thickEnd;
    }
slFreeList(&rangeList);
slFreeList(&badRanges);
slFreeList(&binList);
return error;
}
#endif

static struct liftRange *sampleToRangeList(struct sample *sample, int sizeOne)
/* Make a range list corresponding to sample. */
{
int i;
struct liftRange *rangeList = NULL, *range;
for (i=0; i<sample->sampleCount; ++i)
    {
    AllocVar(range);
    range->start = range->end = sample->chromStart + sample->samplePosition[i];
    range->end += sizeOne;
    range->val = sample->sampleHeight[i];
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}

static struct sample *rangeListToSample(struct liftRange *rangeList, 
                                                char *chrom, char *name,
	unsigned score, char strand[3])
/* Make sample based on range list and other parameters. */
{
struct liftRange *range;
struct sample *sample;
int sampleCount = slCount(rangeList);
int  i, chromStart, chromEnd;

if (sampleCount == 0)
    return NULL;
chromStart = rangeList->start;
chromEnd = rangeList->end;
for (range = rangeList->next; range != NULL; range = range->next)
    chromEnd = range->end;

AllocVar(sample);
sample->chrom = cloneString(chrom);
sample->chromStart = chromStart;
sample->chromEnd = chromEnd;
sample->name = cloneString(name);
sample->score = score;
strncpy(sample->strand, strand, sizeof(sample->strand));
sample->sampleCount = sampleCount;
AllocArray(sample->samplePosition, sampleCount);
AllocArray(sample->sampleHeight, sampleCount);
sample->sampleCount = sampleCount;

for (range = rangeList, i=0; range != NULL; range = range->next, ++i)
    {
    sample->samplePosition[i] = range->start - chromStart;
    sample->sampleHeight[i] = range->val;
    }
return sample;
}

static void remapSample(struct hash *chainHash, struct sample *sample, 
                double minBlocks, bool fudgeThick, FILE *mapped, FILE *unmapped)
/* Remap a single sample and output it. */
{
struct binElement *binList, *el;
struct liftRange *rangeList, *goodList = NULL;
struct chain *chain;
struct sample *ns;
char *error = NULL;
int thick = 0;

binList = findRange(chainHash, sample->chrom, sample->chromStart, sample->chromEnd);
rangeList = sampleToRangeList(sample, 1);
for (el = binList; el != NULL && rangeList != NULL; el = el->next)
    {
    chain = el->val;
    remapRangeList(chain, &rangeList, &thick, &thick, minBlocks, fudgeThick,
    	                &goodList, &rangeList, &error);
    if (goodList != NULL)
        {
	if (chain->qStrand == '-')
	     goodList = reverseRangeList(goodList, chain->qSize);
	ns = rangeListToSample(goodList, chain->qName, sample->name, 
		sample->score, sample->strand);
	sampleTabOut(ns, mapped);
	sampleFree(&ns);
	slFreeList(&goodList);
	}
    }
if (rangeList != NULL)
    {
    ns = rangeListToSample(rangeList, sample->chrom, sample->name,
    	sample->score, sample->strand);
    fprintf(unmapped, "# Leftover %d of %d\n", ns->sampleCount, sample->sampleCount);
    sampleTabOut(ns, unmapped);
    sampleFree(&ns);
    slFreeList(&rangeList);
    }
slFreeList(&binList);
}

void liftOverSample(char *fileName, struct hash *chainHash, 
                        double minMatch, double minBlocks, bool fudgeThick,
                        FILE *mapped, FILE *unmapped)
/* Open up file, decide what type of bed it is, and lift it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
struct sample *sample;

while (lineFileRow(lf, row))
    {
    sample = sampleLoad(row);
    remapSample(chainHash, sample, minBlocks, fudgeThick, mapped, unmapped);
    sampleFree(&sample);
    }
lineFileClose(&lf);
}

struct liftOverChain *liftOverChainList()
/* Get list of all liftOver chains in the central database */
{
struct sqlConnection *conn = hConnectCentral();
struct liftOverChain *list = NULL;

list = liftOverChainLoadByQuery(conn, "select * from liftOverChain");
hDisconnectCentral(&conn);
return list;
}

void filterOutMissingChains(struct liftOverChain **pChainList) 
/* Filter out chains that don't exist.  Helps partially mirrored sites. */
{
while(*pChainList)
    {
    if (fileSize((*pChainList)->path)==-1)
	{
	struct liftOverChain *temp = *pChainList;
	*pChainList = (*pChainList)->next;
	liftOverChainFree(&temp);
	}
    else
	{
	pChainList = &((*pChainList)->next);
	}
    }
}

struct liftOverChain *liftOverChainListFiltered()
/* Get list of all liftOver chains in the central database
 * filtered to include only those chains whose liftover files exist.
 * This helps partially mirrored sites */
{
struct liftOverChain *list = liftOverChainList();
filterOutMissingChains(&list); 
return list;
}

struct liftOverChain *liftOverChainForDb(char *fromDb)
/* Return list of liftOverChains for this database. */
{
struct sqlConnection *conn = hConnectCentral();
struct liftOverChain *list = NULL;
char query[256];
safef(query, sizeof(query), "select * from liftOverChain where fromDb='%s'",
	fromDb);
list = liftOverChainLoadByQuery(conn, query);
hDisconnectCentral(&conn);
return list;
}

char *liftOverChainFile(char *fromDb, char *toDb)
/* Get filename of liftOver chain */
{
struct sqlConnection *conn = hConnectCentral();
struct liftOverChain *chain = NULL;
char query[1024];
char *path = NULL;

if (conn)
    {
    safef(query, sizeof(query), 
            "select * from liftOverChain where fromDb='%s' and toDb='%s'",
                        fromDb, toDb);
    chain = liftOverChainLoadByQuery(conn, query);
    if (chain != NULL)
        {
        path = cloneString(chain->path);
        liftOverChainFree(&chain);
        }
    hDisconnectCentral(&conn);
    }
return path;
}

char *liftOverErrHelp()
/* Help message explaining liftOver failures */
{
    return
    "Deleted in new:\n"
    "    None of sequence intersects with any alignment chain for the region\n"
    "Partially deleted in new:\n"
    "    Sequence intersects with part of a single alignment chain in the region\n"
    "Split in new:\n"
    "    Sequence partially intersects multiple chains in the region\n"
    "Duplicated in new:\n"
    "    Sequence completely intersects multiple chains in the region\n"
    "Boundary problem:\n"
    "    Missing start or end base in an exon\n";
}
