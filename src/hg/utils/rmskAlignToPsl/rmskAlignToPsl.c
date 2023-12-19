/* rmskAlignToPsl - convert repeatmasker alignment files to PSLs. */
#include "common.h"
#include "linefile.h"
#include "rmskAlign.h"
#include "bigRmskAlignBed.h"
#include "psl.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rmskAlignToPsl - convert repeatmasker alignments to PSLs\n"
  "\n"
  "usage:\n"
  "   rmskAlignToPsl rmskAlignTab rmskPslFile\n"
  "\n"
  "  -bigRmsk - input is the text version of bigRmskAlignBed files.\n"
  "  -repSizes=tab - two column tab file with repeat name and size.\n"
  "   Sometimes the repeat sizes are incorrect in the align file.\n"
  "   If a repeat alignment doesn't match the size here or is not\n"
  "   in the file it will be discarded.\n"
  "   \n"
  "  -dump - print alignments to stdout for debugging purposes\n"
  "\n"
  "This convert *.fa.align.tsv file, created by\n"
  "RepeatMasker/util/rmToUCSCTables.pl into a PSL file.\n"
  "Non-TE Repeats without consensus sequence are not included.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bigRmsk", OPTION_BOOLEAN},
   {"repSizes", OPTION_STRING},
   {"dump", OPTION_BOOLEAN},
   {NULL, 0},
};

static boolean bigRmsk = FALSE;
static boolean dump = FALSE;

/*
 * rmskAlign notes
 * 
 *  - genoStart - genoEnd are zero-base, half-open
 *  - repStart, repEnd - are one-based
 *  - rmskAlign records are multi-part, grouped by id.  
 *  - A zero-repeat length record in the middle of group
 *    will have repStart == repEnd, appearing to have a length of 1.
 *    It appears these all have a alignment starting with '-'
 * 
 * Alignment string format:
 *
 * from hg/hgc/rmskJoinedClick.c
 * Print RepeatMasker alignment data stored in RM's cAlign format.
 * The format is basically a lightly compressed diff format where
 * the query and subject are merged into one squence line. The
 * runs of exact matching sequences are interrupted by either
 * single base substitutions annotated as queryBase "/" subjectBase,
 * insertions in the subject annotated as "+ACTAT+", or deletions
 * in the query annotated as "-ACTTTG-".
 *
 * Alignment parts must be decoded separately and then concatenated.
 * as blocks starting with a '-' or '+' maybe terminated by the end of the string
 *
 * Alignments parts may overlap and must then be split.  A given id might
 * have multiple repNames and are split into multiple PSL.
 */


static struct hash* loadRepSizes(char *repSizesFile)
/* load repeat sizes file into a hash */
{
struct hash* repSizes = hashNew(20);
struct lineFile* lf = lineFileOpen(repSizesFile, TRUE);
char *row[2];
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    hashAddInt(repSizes, row[0], lineFileNeedNum(lf, row, 1));
lineFileClose(&lf);
return repSizes;
}

static unsigned getGenomeSize(struct rmskAlign *alignParts)
/* get the length or the chromosome */
{
return alignParts->genoEnd + alignParts->genoLeft;
}

static unsigned getRepeatSize(struct rmskAlign *alignParts)
/* Get the length of repeat sequence.  This is not all that easy. When a
 * repeat is split, CrossMatch reports the same repLeft for other alignments,
 * at least in some cases.  RepeatMasker compensates for this when creating
 * the .out. However, rmToUCSCTables.pl doesn't correct for this.  So we just
 * take the largest length from all alignments.
 *
 * Example .align problem:
 * 103 20.51 0.00 9.86 chr1 147061 147161 (248809261) C AluJr#SINE/Alu (146) 166 70 [m_b3s356i8] 1
 * 103 20.51 0.00 9.86 chr1 147481 147535 (248808887) C AluJr#SINE/Alu (146) 69 25 [m_b3s356i8] 1
 *
 * Still this doesn't always get the correct size.
 */
{
unsigned size = 0;
for (struct rmskAlign *rmskAlign = alignParts; rmskAlign != NULL; rmskAlign = rmskAlign->next)
    size = max(rmskAlign->repEnd + rmskAlign->repLeft, size);
return size;
}

static void rmskAlignToPairwise(struct rmskAlign *rmskAlign,
                                char *tStr, char *qStr)
/* convert a rmsk alignment string to a pairwise string to print.  Output
 * tStr/qStr should be allocated to the total alignment string from all parts
 * plus one. */
{
/// see 'Alignment string format' above
char *t = tStr, *q = qStr;
char *a = rmskAlign->alignment;
while (*a != '\0')
    {
    if (*a == '+')
        {
        // query insert
        a++;
        while ((*a != '+') && (*a != '\0'))
            {
            *t++ = '-';
            *q++ = *a;
            a++;
            }
        }
    else if (*a == '-')
        {
        // target insert
        a++;
        while ((*a != '-') && (*a != '\0'))
            {
            *t++ = *a;
            *q++ = '-';
            a++;
            }
        }
    else
        {
        // aligned
        if (*(a+1) == '/')
            {
            *t++ = *a++;
            a++;  // skip '/'
            *q++ = *a;
            }
        else
            {
            *t++ = *a;
            *q++ = *a;
            }
        }
    if (*a == '\0')
        break;
    a++;
    }
*t = *q = '\0';
assert(strlen(tStr) == strlen(qStr));
assert(strlen(tStr) <= strlen(rmskAlign->alignment));
assert(strlen(qStr) <= strlen(rmskAlign->alignment));
}

static void rmskAlignPrint(struct rmskAlign *alignPart,
                           struct rmskAlign *alignParts,
                           FILE *fh)
/* print out pairwise rmskAlign with alignment string decoded
 * for debugging */
{
int alnLen = strlen(alignPart->alignment);
char tStr[alnLen + 1];
char qStr[alnLen + 1];
rmskAlignToPairwise(alignPart, tStr, qStr);

rmskAlignTabOut(alignPart, fh);
fprintf(fh, "    cAlign: %s\n", alignPart->alignment);
char *fmt = "    %-8s %8d %8d |%d| [%s]: %s\n";
fprintf(fh, fmt, alignPart->genoName, alignPart->genoStart, alignPart->genoEnd,
        alignPart->genoEnd + alignPart->genoLeft,  "+", tStr);
fprintf(fh, fmt, alignPart->repName, alignPart->repStart, alignPart->repEnd,
        getRepeatSize(alignParts), alignPart->strand, qStr);
}

static void rmskAlignListPrint(struct rmskAlign *alignParts,
                               FILE *fh)
/* print out alignments in list */
{
for (struct rmskAlign *ap = alignParts; ap != NULL; ap = ap->next)
    rmskAlignPrint(ap, alignParts, fh);
}

static boolean shouldConvert(struct rmskAlign *rmskAlign)
/* does this repeat type have a consensus and should be converted? */
{
return !sameString(rmskAlign->repClass, "Simple_repeat");
}

static struct rmskAlign **groupRmskAligns(struct rmskAlign *rmskAligns,
                                          unsigned *maxAlignIdPtr)
/* build array of alignment by ids */
{
unsigned maxAlignId = 0;
for (struct rmskAlign *ra = rmskAligns; ra != NULL; ra = ra->next)
    maxAlignId = max(maxAlignId, ra->id);

struct rmskAlign **rmskAlignsById = AllocN(struct rmskAlign *, maxAlignId + 1);
for (struct rmskAlign *ra = slPopHead(&rmskAligns); ra != NULL; ra = slPopHead(&rmskAligns))
    slAddTail((&rmskAlignsById[ra->id]), ra);  // keep in genomic order

*maxAlignIdPtr = maxAlignId;
return rmskAlignsById;
}

int rmskAlignGenomeCmp(const void *va, const void *vb)
/* sort in ascending genomic location  */
{
const struct rmskAlign *a = *((struct rmskAlign **)va);
const struct rmskAlign *b = *((struct rmskAlign **)vb);
int diff = strcmp(a->genoName, b->genoName);
if (diff != 0)
    return diff;
return a->genoStart - b->genoStart;
}

static struct rmskAlign **loadRmskAlign(char *rmskAlignFile,
                                        unsigned *maxAlignIdPtr)
/* load rmskAlign file into an array of lists by indexed by alignment id */
{
struct rmskAlign *rmskAligns = rmskAlignLoadAllByTab(rmskAlignFile);
slSort(&rmskAligns, rmskAlignGenomeCmp);

return groupRmskAligns(rmskAligns, maxAlignIdPtr);
}

static struct rmskAlign *bigRmskAlignToRmskAlign(struct bigRmskAlignBed* bigRmskAlign)
/* convert a bigRmskAlign record to an rmskAlign record */
{
struct rmskAlign *rmskAlign;
AllocVar(rmskAlign);

rmskAlign->swScore = bigRmskAlign->score;
rmskAlign->milliDiv = 1000 * bigRmskAlign->percSubst;
rmskAlign->milliDel = 1000 * bigRmskAlign->percDel;
rmskAlign->milliIns = 1000 * bigRmskAlign->percIns;
rmskAlign->genoName = cloneString(bigRmskAlign->chrom);
rmskAlign->genoStart = bigRmskAlign->chromStart;
rmskAlign->genoEnd = bigRmskAlign->chromEnd;
rmskAlign->genoLeft = bigRmskAlign->chromRemain;
rmskAlign->strand[0] = bigRmskAlign->strand[0];
rmskAlign->repName = cloneString(bigRmskAlign->repName);
rmskAlign->repClass = cloneString(bigRmskAlign->repType);
rmskAlign->repFamily = cloneString(bigRmskAlign->repSubtype);
rmskAlign->repStart = bigRmskAlign->repStart;
rmskAlign->repEnd = bigRmskAlign->repEnd;
rmskAlign->repLeft = bigRmskAlign->repRemain;
rmskAlign->id = bigRmskAlign->id;
rmskAlign->alignment = cloneString(bigRmskAlign->calignData);

return rmskAlign;
}

static struct rmskAlign **loadBigRmskAlign(char *bigRmskAlignFile,
                                           unsigned *maxAlignIdPtr)
/* load bigRmskAlignBed file into an array of lists by indexed by alignment id */
{
struct bigRmskAlignBed *bigRmskAligns = bigRmskAlignBedLoadAllByTab(bigRmskAlignFile);
struct rmskAlign *rmskAligns = NULL;
struct bigRmskAlignBed *bigRmskAlign;

for (bigRmskAlign = slPopHead(&bigRmskAligns); bigRmskAlign != NULL; bigRmskAlign = slPopHead(&bigRmskAligns))
    {
    slAddHead(&rmskAligns, bigRmskAlignToRmskAlign(bigRmskAlign));
    bigRmskAlignBedFree(&bigRmskAlign);
    }

slSort(&rmskAligns, rmskAlignGenomeCmp);
return groupRmskAligns(rmskAligns, maxAlignIdPtr);
}

static boolean rmskLinear(struct rmskAlign *prev,
                          struct rmskAlign *next)
/* are the two alignments linear? assumes genomic sort */
{
// rmskAlign 1-based and not in strand-specific order
if (prev->strand[0] == '+')
    return next->repStart >= prev->repEnd;
else
    return prev->repEnd > next->repStart;
}

static boolean rmskOverlap(struct rmskAlign *rm1,
                           struct rmskAlign *rm2)
/* do they overlap in genomic or repeat coordinates */
{
return ((rm1->genoStart < rm2->genoEnd) && (rm1->genoEnd > rm2->genoStart))
    || ((rm1->repStart - 1 < rm2->repEnd) && (rm1->repEnd > rm2->repStart - 1));
}

static boolean anyOverlap(struct rmskAlign *alignNext,
                          struct rmskAlign *alignParts)
/* Check if the next entry overlaps any of the parts in genomic
 * or repeat */
{
for (struct rmskAlign *ap = alignParts; ap != NULL; ap = ap->next)
    {
    if (rmskOverlap(ap, alignNext))
        return TRUE;
    }
return FALSE;
}

static boolean poppedAlignProcess(struct rmskAlign *nextAlign,
                                  struct rmskAlign **alignPartsPtr,
                                  struct rmskAlign **skippedAlignsPtr)
/* Do something with alignment from a group, either keep or add to skped list.
, Return FALSE if we should not look for any more for this PSL */
{
struct rmskAlign *prevAlign = *alignPartsPtr;  // previous saved
if (!(sameString(nextAlign->repName, prevAlign->repName)
      && sameString(nextAlign->strand, prevAlign->strand)))
    {
    // different name, strand not, can't be in same PSL
    slAddHead(skippedAlignsPtr, nextAlign);
    return TRUE;
    }
if ((!rmskLinear(prevAlign, nextAlign)) || anyOverlap(nextAlign, *alignPartsPtr))
    {
    // hit an overlap or non-linear, give up on this PSL
    slAddHead(skippedAlignsPtr, nextAlign);
    return FALSE;
    }

slAddHead(alignPartsPtr, nextAlign);
return TRUE;
}

static struct rmskAlign *popAlignParts(struct rmskAlign **rmskAlignsPtr)
/* Remove alignments from a id group that can be turned into a PSL.  Stop if
 * an overlapping alignments, name mismatch, strand mismatch, or non-linear
 * alignment is found. These are returned in the next call.  Alignments in a
 * give repeat masker alignment set may not be linear in some cases.
 * Results will in genomic order (as must be input). */
{
if (*rmskAlignsPtr == NULL)
    return NULL;

// first part
struct rmskAlign *alignParts = slPopHead(rmskAlignsPtr);
struct rmskAlign *skippedAligns = NULL;

// find other parts
while (*rmskAlignsPtr != NULL)
    {
    struct rmskAlign *nextAlign = slPopHead(rmskAlignsPtr);
    if (!poppedAlignProcess(nextAlign, &alignParts, &skippedAligns))
        break;
    }

// restore skipped
slReverse(&skippedAligns);
*rmskAlignsPtr = slCat(skippedAligns, *rmskAlignsPtr);

slReverse(&alignParts);
return alignParts;
}

struct blkCoord
/* coordinates and count of bases in a block or gap from alignment string */
{
    struct blkCoord *next;
    int qStart;           // query coords
    int qSize;            // zero if tInsert
    int tStart;           // target coords
    int tSize;            // zero if qInsert
    unsigned matches;     // match count
    unsigned mismatches;  // mismatch count
};

static struct blkCoord *blkCoordNew(int qStart, int tStart)
/* allocate a new blkCoord object */
{
struct blkCoord *cnts;
AllocVar(cnts);
cnts->qStart = qStart;
cnts->tStart = tStart;
return cnts;
}

static struct blkCoord *blkCoordNewAdd(struct blkCoord **bcList,
                                       int qStart, int tStart)
/* allocate a new blkCoord object and add to the list */
{
struct blkCoord *cnts = blkCoordNew(qStart, tStart);
slAddHead(bcList, cnts);
return cnts;
}

static void blkCoordListPrint(struct blkCoord *blkCoords,
                              FILE *fh)
/* print a list of blkCoord for debugging */
{
fprintf(fh, "Blocks:\n");
int i = 0;
for (struct blkCoord *blk = blkCoords; blk != NULL; blk = blk->next, i++)
    fprintf(fh, "\t%d %d-%d [%d] <=> %d-%d [%d]: %u %u\n", i,
            blk->qStart, blk->qStart + blk->qSize, blk->qSize,
            blk->tStart, blk->tStart + blk->tSize, blk->tSize,
            blk->matches, blk->mismatches);
}

static struct blkCoord *parseCAlign(struct rmskAlign *rmskAlign,
                                    unsigned repSize)
/* Parse an alignment into list of counts. Warn and return NULL if the parse
 * alignment is invalid */
{
struct blkCoord *blkCoords = NULL;
struct blkCoord *cur = NULL;
int tNext = rmskAlign->genoStart;
int tEnd = rmskAlign->genoEnd;
int qNext = rmskAlign->repStart - 1;
int qEnd = rmskAlign->repEnd;
if (rmskAlign->strand[0] == '-')
    reverseIntRange(&qNext, &qEnd, repSize);
char *a = rmskAlign->alignment;
while (*a != '\0')
    {
    if (*a == '+')
        {
        // query insert
        a++;
        cur = blkCoordNewAdd(&blkCoords, qNext, tNext);
        while ((*a != '+') && (*a != '\0'))
            {
            qNext++;
            cur->qSize++;
            a++;
            }
        cur = NULL;
        }
    else if (*a == '-')
        {
        // target insert
        a++;
        cur = blkCoordNewAdd(&blkCoords, qNext, tNext);
        while ((*a != '-') && (*a != '\0'))
            {
            tNext++;
            cur->tSize++;
            a++;
            }
        cur = NULL;
        }
    else
        {
        // aligned
        if (cur == NULL)
            cur = blkCoordNewAdd(&blkCoords, qNext, tNext);
        if (*(a+1) == '/')
            {
            cur->mismatches++;
            a += 2;
            }
        else
            {
            cur->matches++;
            }
        cur->qSize++;
        cur->tSize++;
        tNext++;
        qNext++;
        }
    if (*a == '\0')
        break;
    a++;
    }
slReverse(&blkCoords);
if ((tNext > tEnd) || (qNext > qEnd))
    {
    fprintf(stderr, "Note: can't convert %s:%d-%d to %s:%d-%d, id %d, parsed alignment length exceeds bounds\n",
            rmskAlign->repName, rmskAlign->repStart, rmskAlign->repEnd,
            rmskAlign->genoName, rmskAlign->genoStart, rmskAlign->genoEnd, rmskAlign->id);
    slFreeList(&blkCoords);
    return NULL;
    }

return blkCoords;
}

static void trimLeadingIndels(struct blkCoord **blkCoordsPtr)
/* drop blocks blocks with indels at the start */
{
struct blkCoord *blkCoords = *blkCoordsPtr;
while ((blkCoords != NULL) && ((blkCoords->qSize == 0) || (blkCoords->tSize == 0)))
    freeMem(slPopHead(&blkCoords));
*blkCoordsPtr = blkCoords;
}

static struct blkCoord *parseCAligns(struct rmskAlign *alignParts)
/* parses alignment strings from all parts to go into a PSL  */
{
unsigned repSize = getRepeatSize(alignParts);

// build list
struct blkCoord *blkCoords = NULL;
for (struct rmskAlign *alignPart = alignParts; alignPart != NULL; alignPart = alignPart->next)
    {
    struct blkCoord *blkCoordsPart = parseCAlign(alignPart, repSize);
    if (blkCoordsPart == NULL)
        {
        slFreeList(&blkCoords);
        return NULL;
        }
    blkCoords = slCat(blkCoords, blkCoordsPart);
    }

// trim leading and trailing indels
trimLeadingIndels(&blkCoords);
slReverse(&blkCoords);
trimLeadingIndels(&blkCoords);
slReverse(&blkCoords);
return blkCoords;
}

static void addPslBlocks(struct blkCoord *blkCoords,
                         struct psl *psl,
                         int *blockSpacePtr)
/* add blocks to PSL */
{
for (struct blkCoord *blk = blkCoords; blk != NULL; blk = blk->next)
    {
    if (blk->qSize == 0)
        {
        psl->tNumInsert++;
        psl->tBaseInsert += blk->tSize;
        }
    else if (blk->tSize == 0)
        {
        psl->qNumInsert++;
        psl->qBaseInsert += blk->qSize;
        }
    else
        {
        assert(blk->qSize == blk->tSize);
        psl->repMatch += blk->matches;  // all matches are repeat matches
        psl->misMatch += blk->mismatches;
        if (psl->blockCount >= *blockSpacePtr)
            pslGrow(psl, blockSpacePtr);
        psl->tStarts[psl->blockCount] = blk->tStart;
        psl->qStarts[psl->blockCount] = blk->qStart;
        psl->blockSizes[psl->blockCount] = blk->tSize;
        psl->blockCount += 1;
        }
    }
}

static struct psl *convertBlocksToPsl(struct rmskAlign *alignParts,
                                      struct blkCoord *blkCoords)
/* create a PSL from a repeat masker alignment */
{
/* must use coordinates from blocks, as end-indels have been trimmed */
struct blkCoord *blkCountN = slLastEl(blkCoords);

unsigned qSize = getRepeatSize(alignParts);
unsigned qStart = blkCoords->qStart;
unsigned qEnd = blkCountN->qStart + blkCountN->qSize;
if (alignParts->strand[0] == '-')
    reverseUnsignedRange(&qStart, &qEnd, qSize);

unsigned tSize = getGenomeSize(alignParts);
unsigned tStart = blkCoords->tStart;
unsigned tEnd = blkCountN->tStart + blkCountN->tSize;


int blockSpace = slCount(blkCoords);
struct psl *psl = pslNew(alignParts->repName, qSize, qStart, qEnd,
                         alignParts->genoName, tSize, tStart, tEnd, 
                         alignParts->strand, blockSpace, 0);
addPslBlocks(blkCoords, psl, &blockSpace);
pslComputeInsertCounts(psl);
slFreeList(&blkCoords);
return psl;
}



static struct psl *convertToPsl(struct rmskAlign *alignParts)
/* create a PSL from a repeat masker alignment, return NULL if fails */
{

struct blkCoord *blkCoords = parseCAligns(alignParts);
if (blkCoords == NULL)
    return NULL;

if (dump)
    blkCoordListPrint(blkCoords, stderr);

return convertBlocksToPsl(alignParts, blkCoords);
}

static struct psl *alignToPsl(struct rmskAlign *alignParts)
/* convert and output one set of alignment parts */
{
struct psl* psl = convertToPsl(alignParts);
if ((psl != NULL) && (pslCheck("rmskAlign", stderr, psl) != 0))
    {
    fprintf(stderr, "Invalid PSL created\n");
    pslTabOut(psl, stderr);
    rmskAlignListPrint(alignParts, stderr);
    errAbort("BUG invalid PSL created");
    }
return psl;
}

static void convertAlignGroup(struct rmskAlign **rmskAlignGroupPtr,
                              FILE* outFh)
/* convert an alignment group */
{
struct rmskAlign *alignParts;
while ((alignParts = popAlignParts(rmskAlignGroupPtr)) != NULL)
    {
    if (dump)
        rmskAlignListPrint(alignParts, stderr);

    if (shouldConvert(alignParts))
        {
        struct psl *psl = alignToPsl(alignParts);
        if (psl != NULL)
            {
            pslTabOut(psl, outFh);
            pslFree(&psl);
            }
        }
    }
}


static boolean checkSizeWithExpected(struct rmskAlign* rmskAlignGroup,
                                     struct hash *repSizes,
                                     struct hash *repSizeWarned)
/* Check if we have an expected repSize for this and they match query.  If not, warn
 * for first occurrence.  These are skipped */
{
if (hashLookup(repSizes, rmskAlignGroup->repName) == NULL)
    {
    if (hashLookup(repSizeWarned, rmskAlignGroup->repName) == NULL)
        {
        fprintf(stderr, "Warning: '%s' expected not found, skipping\n", rmskAlignGroup->repName);
        hashAddInt(repSizeWarned, rmskAlignGroup->repName, TRUE);
        }
    return FALSE;
    }
int gotRepSize = getRepeatSize(rmskAlignGroup);
int expectRepSize = hashIntVal(repSizes, rmskAlignGroup->repName);
if (gotRepSize != expectRepSize)
    {
    if (hashLookup(repSizeWarned, rmskAlignGroup->repName) == NULL)
        {
        fprintf(stderr, "Warning: '%s' size %d does not match expected size %d, skipping\n",
                rmskAlignGroup->repName, gotRepSize, expectRepSize);
        hashAddInt(repSizeWarned, rmskAlignGroup->repName, TRUE);
        }
    return FALSE;
    }
return TRUE;
}

static void rmskAlignToPsl(char *rmskAlignFile, char *rmskPslFile,
                           struct hash* repSizes)
/* rmskAlignToPsl - convert repeatmasker alignment files to PSLs. */
{
// load all, so we can join ones split by other insertions by id
// don't bother freeing
struct rmskAlign **rmskAlignGroups = NULL;
unsigned maxAlignId = 0;
if (bigRmsk)
    rmskAlignGroups = loadBigRmskAlign(rmskAlignFile, &maxAlignId);
else
    rmskAlignGroups = loadRmskAlign(rmskAlignFile, &maxAlignId);

struct hash* repSizeWarned = hashNew(12);  // don't warn multiple times on same sequence

FILE* outFh = mustOpen(rmskPslFile, "w");
for (unsigned id = 0; id <= maxAlignId; id++)
    {
    if (rmskAlignGroups[id] != NULL)
        {
        if ((repSizes == NULL) || checkSizeWithExpected(rmskAlignGroups[id], repSizes, repSizeWarned))
            convertAlignGroup(&(rmskAlignGroups[id]), outFh);
        }
    }
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bigRmsk = optionExists("bigRmsk");
dump = optionExists("dump");
struct hash* repSizes = NULL;
if (optionExists("repSizes"))
    repSizes = loadRepSizes(optionVal("repSizes", NULL));
rmskAlignToPsl(argv[1], argv[2], repSizes);
return 0;
}
