/* liftOver - Move annotations from one assembly to another. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chain.h"
#include "bed.h"
#include "genePred.h"

double minMatch = 0.95;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftOver - Move annotations from one assembly to another\n"
  "usage:\n"
  "   liftOver oldFile map.chain newFile unMapped\n"
  "oldFile and newFile are in bed format by default, but can be in GFF and\n"
  "maybe eventually others with the appropriate flags below.\n"
  "options:\n"
  "   -minMatch=0.N Minimum ratio of bases that must remap. Default %3.2f\n"
  "   -gff  File is in gff/gtf format.  Note that the gff lines are converted\n"
  "         separately.  It would be good to have a separate check after this\n"
  "         that the lines that make up a gene model still make a plausible gene\n"
  "         after liftOver\n"
  "   -genePred - File is in genePred format\n"
  , minMatch
  );
}

struct chromMap
/* Remapping information for one (old) chromosome */
    {
    char *name;			/* Chromosome name. */
    struct binKeeper *bk;	/* Keyed by old position, values are chains. */
    };

char otherStrand(char c)
/* Swap +/- */
{
if (c == '-')
    return '+';
else if (c == '+')
    return '-';
else
    return c;
}

void readMap(char *fileName, struct hash *newChromHash, struct hash *chainHash)
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

struct binElement *findRange(struct hash *chainHash, char *chrom, int start, int end)
/* Find elements that intersect range. */
{
struct chromMap *map = hashMustFindVal(chainHash, chrom);
return binKeeperFind(map->bk, start, end);
}

int chainAliSize(struct chain *chain)
/* Return size of all blocks in chain. */
{
struct boxIn *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += b->qEnd - b->qStart;
return total;
}

int aliIntersectSize(struct chain *chain, int tStart, int tEnd)
/* How many bases in chain intersect region from tStart to tEnd */
{
int total = 0, one;
struct boxIn *b;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    one = rangeIntersection(tStart, tEnd, b->tStart, b->tEnd);
    if (one > 0)
	total += one;
    }
return total;
}

boolean mapThroughChain(struct chain *chain, double minRatio, int *pStart, int *pEnd)
/* Map interval from start to end from target to query side of chain.
 * Return FALSE if not possible, otherwise update *pStart, *pEnd. */
{
struct chain *subChain, *freeChain;
int s = *pStart, e = *pEnd;
int oldSize = e - s;
int newCover = 0;
int ok = TRUE;

chainSubsetOnT(chain, s, e, &subChain, &freeChain);
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
chainFree(&freeChain);
return ok;
}

char *remapRange(struct hash *chainHash, double minRatio,
	char *chrom, int s, int e, char strand,
	char **retChrom, int *retStart, int *retEnd, char *retStrand)
/* Remap a range through chain hash.  If all is well return NULL
 * and results in retChrom, retStart, retEnd.  Otherwise
 * return a string describing the problem. */
{
int size = e - s;
int minMatchSize =  minMatch * size;
int intersectSize;
struct binElement *list = findRange(chainHash, chrom, s, e), *el;
struct chain *chainsHit = NULL, *chainsPartial = NULL, *chainsMissed = NULL, *chain;

for (el = list; el != NULL; el = el->next)
    {
    chain = el->val;
    intersectSize = aliIntersectSize(chain, s, e);
    if (intersectSize >= minMatchSize)
	{
	slAddHead(&chainsHit, chain);
	}
    else if (intersectSize > 0)
	{
	slAddHead(&chainsPartial, chain);
	}
    else
	{
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
else if (chainsHit->next != NULL)
    {
    return "Duplicated in new";
    }
else if (!mapThroughChain(chainsHit, minRatio, &s, &e))
    {
    return "Incomplete in new";
    }
else
    {
    struct chain *chain = chainsHit;
    chrom = chain->qName;
    if (chain->qStrand == '-')
	{
	strand = otherStrand(strand);
	}
    *retChrom = chrom;
    *retStart = s;
    *retEnd = e;
    *retStrand = strand;
    return NULL;
    }
}


void bedOverSmall(struct lineFile *lf, int fieldCount, struct hash *chainHash, FILE *mapped, 
	FILE *unmapped)
/* Do a bed without a block-list. */
{
int i, wordCount, s, e;
char *words[20], *chrom;
char strand = '.', strandString[2];
char *whyNot;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    FILE *f = mapped;
    chrom = words[0];
    s = lineFileNeedNum(lf, words, 1);
    e = lineFileNeedNum(lf, words, 2);
    if (s > e)
	errAbort("Start after end line %d of %s", lf->lineIx, lf->fileName);
    if (wordCount >= 6)
	strand = words[5][0];
    whyNot = remapRange(chainHash, minMatch, chrom, s, e, strand, &chrom, &s, &e, &strand);
    strandString[0] = strand;
    strandString[1] = 0;
    words[5] = strandString;
    if (whyNot != NULL)
	{
	f = unmapped;
	fprintf(f, "#%s\n", whyNot);
	}
    fprintf(f, "%s\t%d\t%d", chrom, s, e);
    for (i=3; i<wordCount; ++i)
	fprintf(f, "\t%s", words[i]);
    fprintf(f, "\n");
    }
}

void shortGffLine(struct lineFile *lf)
/* Complain about short line in GFF and abort. */
{
errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
}

int gffNeedNum(struct lineFile *lf, char *s)
/* Convert s to an integer or die trying. */
{
char c = *s;
if (isdigit(c) || c == '-')
    return atoi(s);
else
    errAbort("Expecting number line %d of %s", lf->lineIx, lf->fileName);
return 0;
}

void liftOverGff(char *fileName, struct hash *chainHash, FILE *mapped, FILE *unmapped)
/* Lift over GFF file */
{
char *error = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char c, *s, *line, *word;
char *seq, *source, *feature;
int start, end;
char *score, *strand, *frame;
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
    error = remapRange(chainHash, minMatch, seq, start, end, *strand,
	&seq, &start, &end, strand);

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

struct range
/* A start/stop pair. */
     {
     struct range *next;
     int start;		/* Start 0 based */
     int end;		/* End, non-inclusive. */
     };

struct range *bedToRangeList(struct bed *bed)
/* Convert blocks in bed to a range list. */
{
struct range *range, *rangeList = NULL;
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

int chainRangeIntersection(struct chain *chain, struct range *rangeList)
/* Return chain/rangeList intersection size. */
{
struct boxIn *b = chain->blockList;
struct range *r = rangeList;
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

char *remapRangeList(struct chain *chain, struct range *rangeList,
	int *pThickStart, int *pThickEnd)
/* Remap range list through chain.  Return error message on failure,
 * NULL on success. */
{
struct boxIn *b = chain->blockList;
struct range *r = rangeList;
int bDiff, rStart = 0;
bool gotStart = FALSE;
int startCount = 0, endCount = 0;
int rCount = slCount(rangeList);
int thickStart = *pThickStart, thickEnd = *pThickEnd;
bool gotThickStart = FALSE, gotThickEnd = FALSE;
bool needThick = (thickStart != thickEnd);
boolean done = FALSE;

if (b == NULL || r == NULL)
    return NULL;
for (;;)
    {
    // uglyf("b: %d %d (%d %d),  r: %d %d\n", b->tStart, b->tEnd, b->qStart, b->qEnd, r->start, r->end);

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
    while (r->end <= b->tStart)
	{
	r = r->next;
	if (r == NULL)
	    {
	    done = TRUE;
	    break;
	    }
	gotStart = FALSE;
	}
    if (done) 
	break;
    if (needThick)
	{
	if (b->tStart <= thickStart && thickStart < b->tEnd)
	    {
	    *pThickStart = thickStart + b->qStart - b->tStart;
	    gotThickStart = TRUE;
	    }
	if (b->tStart <= thickEnd && thickEnd <= b->tEnd)
	    {
	    *pThickEnd = thickEnd + b->qStart - b->tStart;
	    gotThickEnd = TRUE;
	    }
	}
    if (b->tStart <= r->start && r->start < b->tEnd && !gotStart)
	{
	gotStart = TRUE;
	bDiff = b->qStart - b->tStart;
	rStart = r->start + bDiff;
	++startCount;
	}
    if (b->tStart < r->end && r->end <= b->tEnd)
	{
	bDiff = b->qStart - b->tStart;
	if (gotStart)
	    {
	    r->start = rStart;
	    r->end += bDiff;
	    ++endCount;
	    }
	r = r->next;
	gotStart = FALSE;
	if (r == NULL)
	    {
	    done = TRUE;
	    break;
	    }
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
if (needThick)
    {
    if (!gotThickStart || !gotThickEnd)
	return "Can't find thickStart/thickEnd";
    }
else
    {
    *pThickStart = *pThickEnd = rangeList->start;
    }
if (!( startCount == rCount && endCount == rCount))
    return "Boundary problem";
return NULL;
}

#ifdef DEBUG
void dumpRangeList(struct range *rangeList, FILE *f)
/* Write out range list to file. */
{
struct range *range;
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->end - range->start);
fprintf(f, "\n");
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->start);
fprintf(f, "\n");
}
#endif /* DEBUG */

int sumBedBlocks(struct bed *bed)
/* Calculate sum of all block sizes in bed. */
{
int i, total = 0;
for (i=0; i<bed->blockCount; ++i)
    total += bed->blockSizes[i];
return total;
}

char *remapBlockedBed(struct hash *chainHash, struct bed *bed)
/* Remap blocks in bed, and also chromStart/chromEnd.  
 * Return NULL on success, an error string on failure. */
{
struct chain *chainList = NULL,  *chain, *subChain, *freeChain;
int bedSize = sumBedBlocks(bed);
struct binElement *binList;
struct binElement *el;
struct range *rangeList, *range;
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
    error = remapRangeList(chain, rangeList, &thickStart, &thickEnd);


/* Convert rangeList back to bed blocks.  Also calculate start and end. */
if (error == NULL)
    {
    if (chain->qStrand == '-')
	{
	struct range *range;
	slReverse(&rangeList);
	for (range = rangeList; range != NULL; range = range->next)
	    reverseIntRange(&range->start, &range->end, chain->qSize);
	reverseIntRange(&thickStart, &thickEnd, chain->qSize);
	bed->strand[0] = otherStrand(bed->strand[0]);
	}
    bed->chromStart = start = rangeList->start;
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
slFreeList(&binList);
return error;
}

void bedOverBig(struct lineFile *lf, int refCount, struct hash *chainHash, FILE *mapped,
	FILE *unmapped)
/* Do a bed with block-list. */
{
int i, wordCount, s, e;
char *line, *words[20], *chrom;
char strand = '.', strandString[2];
char *whyNot = NULL;
boolean gotThick;


while (lineFileNextReal(lf, &line))
    {
    struct bed *bed;
    wordCount = chopLine(line, words);
    if (refCount != wordCount)
	lineFileExpectWords(lf, refCount, wordCount);
    bed = bedLoadN(words, wordCount);
    whyNot = remapBlockedBed(chainHash, bed);
    if (whyNot == NULL)
	{
	bedTabOutN(bed, wordCount, mapped);
	}
    else
	{
	fprintf(unmapped, "#%s\n", whyNot);
	bedTabOutN(bed, wordCount, unmapped);
	}
    bedFree(&bed);
    }
}

void liftOverBed(char *fileName, struct hash *chainHash, FILE *f, FILE *unmapped)
/* Open up file, decide what type of bed it is, and lift it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *line;
char *words[16];

if (lineFileNextReal(lf, &line))
    {
    line = cloneString(line);
    wordCount = chopLine(line, words);
    lineFileReuse(lf);
    freez(&line);
    if (wordCount < 3)
	 errAbort("Expecting at least 3 fields in %s", fileName);
    if (wordCount <= 10)
	 bedOverSmall(lf, wordCount, chainHash, f, unmapped);
    else
	 bedOverBig(lf, wordCount, chainHash, f, unmapped);
    }
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

void liftOverGenePred(char *fileName, struct hash *chainHash, FILE *mapped, FILE *unmapped)
/* Lift over file in genePred format. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[10];
struct bed *bed;
struct genePred *gp;
char *error;
FILE *f;

ZeroVar(&bed);
while (lineFileRow(lf, row))
    {
    gp = genePredLoad(row);
    uglyf("%s %s %d %d %s\n", gp->name, gp->chrom, gp->txStart, gp->txEnd, gp->strand);
    f = mapped;
    bed = genePredToBed(gp);
    error = remapBlockedBed(chainHash, bed);
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
    fflush(f);		//uglyf

    bedFree(&bed);
    genePredFree(&gp);
    }
}

void liftOver(char *oldFile, char *mapFile, char *newFile, char *unmappedFile)
/* liftOver - Move annotations from one assembly to another. */
{
struct hash *newChromHash = newHash(0);		/* Name keyed, chrom valued. */
struct hash *chainHash = newHash(0);		/* Old chromosome name keyed, chromMap valued. */
FILE *mapped = mustOpen(newFile, "w");
FILE *unmapped = mustOpen(unmappedFile, "w");
readMap(mapFile, newChromHash, chainHash);
if (optionExists("gff"))
    liftOverGff(oldFile, chainHash, mapped, unmapped);
else if (optionExists("genePred"))
    liftOverGenePred(oldFile, chainHash, mapped, unmapped);
else
    liftOverBed(oldFile, chainHash, mapped, unmapped);
carefulClose(&mapped);
carefulClose(&unmapped);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minMatch = optionFloat("minMatch", minMatch);
if (argc != 5)
    usage();
liftOver(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
