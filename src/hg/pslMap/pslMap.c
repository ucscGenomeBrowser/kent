/* pslMap - map PSLs alignments to new targets using alignments of the old
 * target to the new target. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "dnautil.h"
#include "chain.h"

static char const rcsid[] = "$Id: pslMap.c,v 1.9 2006/01/11 22:37:25 markd Exp $";

/*
 * Notes:
 *  - This programs is used with both large and small mapping psls.  Large
 *    psls used for doing cross-species mappings and small psl are used for
 *    doing protein to mRNA mappings.  This introduces some speed issues.
 *    For chain mapping, a large amount of time is spent in getBlockMapping()
 *    searching for the starting point of a mapping.  However an optimization
 *    to find the starting point, such as a binKeeper, would be very
 *    inefficient for a large number of small psls.  Implementing such
 *    an optimzation would have to be dependent on the type of mapping.
 *    The code was made 16x faster for genome mappings by remembering the
 *    current location in the mapping psl between blocks (iMapBlkPtr arg).
 *    This will do for a while.
 */


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"suffix", OPTION_STRING},
    {"keepTranslated", OPTION_BOOLEAN},
    {"chainMapFile", OPTION_BOOLEAN},
    {"swapMap", OPTION_BOOLEAN},
    {"mapInfo", OPTION_STRING},
    {NULL, 0}
};

/* Values parsed from command line */
static char* suffix = NULL;
static boolean keepTranslated = FALSE;
static boolean chainMapFile = FALSE;
static boolean swapMap = FALSE;
static char* mapInfoFile = NULL;

/* count of non-fatal errors */
static int errCount = 0;

static char *mapInfoHdr =
    "#srcQName\t" "srcQStart\t" "srcQEnd\t" "srcQSize\t"
    "srcTName\t" "srcTStart\t" "srcTEnd\t"
    "srcStrand\t" "srcAligned\t"
    "mappingQName\t" "mappingQStart\t" "mappingQEnd\t"
    "mappingTName\t" "mappingTStart\t" "mappingTEnd\t"
    "mappingStrand\t" "mappingId\t"
    "mappedQName\t" "mappedQStart\t" "mappedQEnd\t"
    "mappedTName\t" "mappedTStart\t" "mappedTEnd\t"
    "mappedStrand\t"
    "mappedAligned\t" "qStartTrunc\t" "qEndTrunc\n";

struct block
/* coordinates of a block */
{
    int qStart;          /* Query start position. */
    int qEnd;            /* Query end position. */
    int tStart;          /* Target start position. */
    int tEnd;            /* Target end position. */
};

static void usage(char *msg)
/* usage msg and exit */
{
/* message got huge, so it's in a generate file */
static char *usageMsg =
#include "usage.msg"
    ;
errAbort("%s\n%s", msg, usageMsg);
}

struct mapAln
/* Mapping alignment, psl plus additional information. */
{
    struct mapAln *next;  /* link for entries for same query */
    struct psl *psl;  /* psl, maybe created from a chain */
    int id;           /* chain id, or psl file row  */
};

static struct mapAln *mapAlnNew(struct psl *psl, int id)
/* construct a new mapAln object */
{
struct mapAln *mapAln;
AllocVar(mapAln);
mapAln->psl = psl;
mapAln->id = id;
return mapAln;
}

static char pslTStrand(struct psl *psl)
/* get the target strand, return implied + for untranslated */
{
return ((psl->strand[1] == '-') ? '-' : '+');
}

static char pslQStrand(struct psl *psl)
/* get the query strand */
{
return psl->strand[0];
}

static void pslProtToNA(struct psl *psl)
/* convert a protein/NA alignment to a NA/NA alignment */
{
int iBlk;

psl->qStart *= 3;
psl->qEnd *= 3;
psl->qSize *= 3;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    psl->blockSizes[iBlk] *= 3;
    psl->qStarts[iBlk] *= 3;
    }
}

static void pslNAToProt(struct psl *psl)
/* undo pslProtToNA */
{
int iBlk;

psl->qStart /= 3;
psl->qEnd /= 3;
psl->qSize /= 3;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    psl->blockSizes[iBlk] /= 3;
    psl->qStarts[iBlk] /= 3;
    }
}

/* macro to swap to variables */
#define swapVars(a, b, tmp) ((tmp) = (a), (a) = (b), (b) = (tmp))

static void swapPsl(struct psl *psl)
/* swap query and target in psl */
{
int i, itmp;
unsigned utmp;
char ctmp, *stmp; 
if (psl->strand[1] == '\0')
    psl->strand[1] = '+';  /* explict strand */
swapVars(psl->strand[0], psl->strand[1], ctmp);
swapVars(psl->qName, psl->tName, stmp);
swapVars(psl->qSize, psl->tSize, utmp);
swapVars(psl->qStart, psl->tStart, itmp);
swapVars(psl->qEnd, psl->tEnd, itmp);
for (i = 0; i < psl->blockCount; i++)
    swapVars(psl->qStarts[i], psl->tStarts[i], utmp);
}

static struct mapAln *chainToPsl(struct chain *ch)
/* convert a chain to a psl, ignoring match counts, etc */
{
struct psl *psl;
struct cBlock *cBlk;
int iBlk;
char strand[2];
strand[0] = ch->qStrand;
strand[1] = '\0';

psl = pslNew(ch->qName, ch->qSize, ch->qStart, ch->qEnd,
             ch->tName, ch->tSize, ch->tStart, ch->tEnd,
             strand, slCount(ch->blockList), 0);
for (cBlk = ch->blockList, iBlk = 0; cBlk != NULL; cBlk = cBlk->next, iBlk++)
    {
    psl->blockSizes[iBlk] = (cBlk->tEnd - cBlk->tStart);
    psl->qStarts[iBlk] = cBlk->qStart;
    psl->tStarts[iBlk] = cBlk->tStart;
    psl->match += psl->blockSizes[iBlk];
    }
psl->blockCount = iBlk;
if (swapMap)
    swapPsl(psl);
return mapAlnNew(psl, ch->id);
}

static struct hash* loadMapChains(char *chainFile)
/* read a chain file, convert to mapAln object and hash by query, linking multiple PSLs
 * for the same query.*/
{
struct hash* mapAlns = hashNew(20);
struct chain *ch;
struct lineFile *chLf = lineFileOpen(chainFile, TRUE);
struct hashEl *hel;
while ((ch = chainRead(chLf)) != NULL)
    {
    struct mapAln *mapAln = chainToPsl(ch);
    hel = hashStore(mapAlns, mapAln->psl->qName);
    slAddHead((struct mapAln**)&hel->val, mapAln);
    chainFree(&ch);
    }
lineFileClose(&chLf);
return mapAlns;
}

static struct hash* loadMapPsls(char *pslFile)
/* read a psl file and hash by query, linking multiple PSLs for the
 * same query.*/
{
struct hash* mapAlns = hashNew(20);
int id = 0;
struct psl* psl;
struct lineFile *pslLf = pslFileOpen(pslFile);
struct hashEl *hel;
while ((psl = pslNext(pslLf)) != NULL)
    {
    if (swapMap)
        swapPsl(psl);
    hel = hashStore(mapAlns, psl->qName);
    slSafeAddHead((struct mapAln**)&hel->val, mapAlnNew(psl, id));
    id++;
    }
lineFileClose(&pslLf);
return mapAlns;
}

static int pslAlignedBases(struct psl *psl)
/* get number of aligned bases in a psl.  Need because we can't set the stats
 * in the mapped psl */
{
int i, cnt = 0;
for (i = 0; i < psl->blockCount; i++)
    cnt += psl->blockSizes[i];
return cnt;
}

static void writeMapInfo(FILE* fh, struct psl *inPsl, struct mapAln *mapAln, 
                         struct psl* mappedPsl)
/* write mapInfo row */
{
/* srcQName srcQStart srcQEnd srcQSize */
fprintf(fh, "%s\t%d\t%d\t%d\t",
        inPsl->qName, inPsl->qStart, inPsl->qEnd, inPsl->qSize);
/* srcTName srcTStart srcTEnd srcStrand srcAligned */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t",
        inPsl->tName, inPsl->tStart, inPsl->tEnd, inPsl->strand,
        pslAlignedBases(inPsl));
/* mappingQName mappingQStart mappingQEnd */
fprintf(fh, "%s\t%d\t%d\t",
        mapAln->psl->qName, mapAln->psl->qStart, mapAln->psl->qEnd);
/* mappingTName mappingTStart mappingTEnd mappingStrand mappingId */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t",
        mapAln->psl->tName, mapAln->psl->tStart, mapAln->psl->tEnd,
        mapAln->psl->strand, mapAln->id);
/* mappedQName mappedQStart mappedQEnd mappedTName mappedTStart mappedTEnd mappedStrand */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t%d\t%s\t",
        mappedPsl->qName, mappedPsl->qStart, mappedPsl->qEnd, 
        mappedPsl->tName, mappedPsl->tStart, mappedPsl->tEnd,
        mappedPsl->strand);
/* mappedAligned qStartTrunc qEndTrunc */
fprintf(fh, "%d\t%d\t%d\n",
        pslAlignedBases(mappedPsl), 
        (mappedPsl->qStart-inPsl->qStart), (inPsl->qEnd-mappedPsl->qEnd));
}

static struct psl* createMappedPsl(struct psl* inPsl, struct mapAln *mapAln,
                                   int mappedPslMax)
/* setup a PSL for the output alignment */
{
struct psl* mappedPsl;
char qName[256], strand[3];
assert(pslTStrand(inPsl) == pslQStrand(mapAln->psl));

/* strand can be taken from both alignments, since common sequence is in same
 * orientation. */
strand[0] = pslQStrand(inPsl);
strand[1] = pslTStrand(mapAln->psl);
strand[2] = '\n';

if (suffix == NULL)
    safef(qName, sizeof(qName), "%s", inPsl->qName);
else
    safef(qName, sizeof(qName), "%s%s", inPsl->qName, suffix);
mappedPsl = pslNew(qName, inPsl->qSize, 0, 0,
                   mapAln->psl->tName, mapAln->psl->tSize, 0, 0,
                   strand, mappedPslMax, 0);
return mappedPsl;
}

static struct block blockFromPslBlock(struct psl* psl, int iBlock)
/* fill in a block object from a psl block */
{
struct block block;
block.qStart = psl->qStarts[iBlock];
block.qEnd = psl->qStarts[iBlock] + psl->blockSizes[iBlock];
block.tStart = psl->tStarts[iBlock];
block.tEnd = psl->tStarts[iBlock] + psl->blockSizes[iBlock];
return block;
}

static void addPslBlock(struct psl* psl, struct block* blk, int* pslMax)
/* add a block to a psl */
{
unsigned newIBlk = psl->blockCount;

assert((blk->qEnd - blk->qStart) == (blk->tEnd - blk->tStart));
if (newIBlk >= *pslMax)
    pslGrow(psl, pslMax);
psl->qStarts[newIBlk] = blk->qStart;
psl->tStarts[newIBlk] = blk->tStart;
psl->blockSizes[newIBlk] = blk->qEnd - blk->qStart;
/* lie about match counts. */
psl->match += psl->blockSizes[newIBlk];
psl->blockCount++;
}

static void setPslBounds(struct psl* mappedPsl)
/* set sequences bounds on mapped PSL */
{
int lastBlk = mappedPsl->blockCount-1;

/* set start/end of sequences */
mappedPsl->qStart = mappedPsl->qStarts[0];
mappedPsl->qEnd = mappedPsl->qStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslQStrand(mappedPsl) == '-')
    reverseIntRange(&mappedPsl->qStart, &mappedPsl->qEnd, mappedPsl->qSize);

mappedPsl->tStart = mappedPsl->tStarts[0];
mappedPsl->tEnd = mappedPsl->tStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslTStrand(mappedPsl) == '-')
    reverseIntRange(&mappedPsl->tStart, &mappedPsl->tEnd, mappedPsl->tSize);
}

static void adjustOrientation(struct psl *inPsl, char *mapPslOrigStrand,
                              struct psl* mappedPsl)
/* adjust strand, possibly reverse complementing, based on keepTranslated
 * option and input psls. */
{
if ((!keepTranslated) || ((inPsl->strand[1] == '\0') && (mapPslOrigStrand[1] == '\0')))
    {
    /* make untranslated */
    if (pslTStrand(mappedPsl) == '-')
        pslRc(mappedPsl);
    mappedPsl->strand[1] = '\0';  /* implied target strand */
    }
}

static struct block getBeforeBlockMapping(unsigned mqStart, unsigned mqEnd,
                                          struct block* align1Blk)
/* map part of an ungapped psl block that occurs before a mapAln psl block */
{
struct block mappedBlk;
ZeroVar(&mappedBlk);

/* mRNA start in genomic gap before this block, this will
 * be an inPsl insert */
unsigned size = (align1Blk->tEnd < mqStart)
    ? (align1Blk->tEnd - align1Blk->tStart)
    : (mqStart - align1Blk->tStart);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qStart + size;
return mappedBlk;
}

static struct block getOverBlockMapping(unsigned mqStart, unsigned mqEnd,
                                        unsigned mtStart, struct block* align1Blk)
/* map part of an ungapped psl block that overlapps a mapAln psl block. */
{
struct block mappedBlk;
ZeroVar(&mappedBlk);

/* common sequence start contained in this block, this handles aligned
 * and genomic inserts */
unsigned off = align1Blk->tStart - mqStart;
unsigned size = (align1Blk->tEnd > mqEnd)
    ? (mqEnd - align1Blk->tStart)
    : (align1Blk->tEnd - align1Blk->tStart);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qStart + size;
mappedBlk.tStart = mtStart + off;
mappedBlk.tEnd = mtStart + off + size;
return mappedBlk;
}

static struct block getBlockMapping(struct psl* inPsl, struct mapAln *mapAln,
                                    int *iMapBlkPtr, struct block* align1Blk)
/* Map part or all of a ungapped psl block to the genome.  This returns the
 * coordinates of the sub-block starting at the beginning of the align1Blk.
 * If this is a gap, either the target or query coords are zero.  This works
 * in genomic strand space.  The search starts at the specified map block,
 * which is updated to prevent rescanning large psls.
 */
{
int iBlk;
struct block mappedBlk;

/* search for block or gap containing start of mrna block */
for (iBlk = *iMapBlkPtr; iBlk < mapAln->psl->blockCount; iBlk++)
    {
    unsigned mqStart = mapAln->psl->qStarts[iBlk];
    unsigned mqEnd = mapAln->psl->qStarts[iBlk]+mapAln->psl->blockSizes[iBlk];
    if (align1Blk->tStart < mqStart)
        {
        *iMapBlkPtr = iBlk;
        return getBeforeBlockMapping(mqStart, mqEnd, align1Blk);
        }
    if ((align1Blk->tStart >= mqStart) && (align1Blk->tStart < mqEnd))
        {
        *iMapBlkPtr = iBlk;
        return getOverBlockMapping(mqStart, mqEnd, mapAln->psl->tStarts[iBlk], align1Blk);
        }
    }

/* reached the end of the mRNA->genome alignment, finish off the 
 * rest of the the protein as an insert */
ZeroVar(&mappedBlk);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qEnd;
*iMapBlkPtr = iBlk;
return mappedBlk;
}

static boolean mapBlock(struct psl *inPsl, struct mapAln *mapAln, int *iMapBlkPtr,
                        struct block *align1Blk, struct psl* mappedPsl,
                        int* mappedPslMax)
/* Add a PSL block from a ungapped portion of an alignment, mapping it to the
 * genome.  If the started of the inPsl block maps to a part of the mapAln psl
 * that is aligned, it is added as a PSL block, otherwise the gap is skipped.
 * Block starts are adjusted for next call.  Return FALSE if the end of the
 * alignment is reached. */
{
struct block mappedBlk;
unsigned size;
assert(align1Blk->qStart <= align1Blk->qEnd);
assert(align1Blk->tStart <= align1Blk->tEnd);
assert((align1Blk->qEnd - align1Blk->qStart) == (align1Blk->tEnd - align1Blk->tStart));

if ((align1Blk->qStart >= align1Blk->qEnd) || (align1Blk->tStart >= align1Blk->tEnd))
    return FALSE;  /* end of ungapped block. */

/* find block or gap with start coordinates of mrna */
mappedBlk = getBlockMapping(inPsl, mapAln, iMapBlkPtr, align1Blk);

if ((mappedBlk.qEnd != 0) && (mappedBlk.tEnd != 0))
    addPslBlock(mappedPsl, &mappedBlk, mappedPslMax);

/* advance past block or gap */
size = (mappedBlk.qEnd != 0)
    ? (mappedBlk.qEnd - mappedBlk.qStart)
    : (mappedBlk.tEnd - mappedBlk.tStart);
align1Blk->qStart += size;
align1Blk->tStart += size;

return TRUE;
}

static struct psl* mapAlignment(struct psl *inPsl, struct mapAln *mapAln)
/* map one pair of alignments. */
{
int mappedPslMax = 8; /* allocated space in output psl */
int iMapBlk = 0;
char mapPslOrigStrand[3];
boolean rcMapPsl = (pslTStrand(inPsl) != pslQStrand(mapAln->psl));
boolean cnv1 = (pslIsProtein(inPsl) && !pslIsProtein(mapAln->psl));
boolean cnv2 = (pslIsProtein(mapAln->psl) && !pslIsProtein(inPsl));
int iBlock;
struct psl* mappedPsl;


if (inPsl->tSize != mapAln->psl->qSize)
    {
    fprintf(stderr, "Non-fatal error: inPsl %s tSize (%d) != mapPsl %s qSize (%d)\n",
            inPsl->tName, inPsl->tSize, mapAln->psl->qName, mapAln->psl->qSize);
    errCount++;
    return NULL;
    }

/* convert protein PSLs */
if (cnv1)
    pslProtToNA(inPsl);
if (cnv2)
    pslProtToNA(mapAln->psl);

/* need to ensure common sequence in same orientation, save strand for later */
safef(mapPslOrigStrand, sizeof(mapPslOrigStrand), "%s", mapAln->psl->strand);
if (rcMapPsl)
    pslRc(mapAln->psl);

mappedPsl = createMappedPsl(inPsl, mapAln, mappedPslMax);

/* Fill in ungapped blocks.  */
for (iBlock = 0; iBlock < inPsl->blockCount; iBlock++)
    {
    struct block align1Blk = blockFromPslBlock(inPsl, iBlock);
    while (mapBlock(inPsl, mapAln, &iMapBlk, &align1Blk, mappedPsl,
                    &mappedPslMax))
        continue;
    }

/* finish up psl */
setPslBounds(mappedPsl);
adjustOrientation(inPsl, mapPslOrigStrand, mappedPsl);

/* restore input */
if (rcMapPsl)
    {
    pslRc(mapAln->psl);
    strcpy(mapAln->psl->strand, mapPslOrigStrand);
    }
if (cnv1)
    pslNAToProt(inPsl);
if (cnv2)
    pslNAToProt(mapAln->psl);

return mappedPsl;
}

static void mapPslPair(struct psl *inPsl, struct mapAln *mapAln,
                       FILE* outPslFh, FILE *mapInfoFh)
/* map one pair of query and target PSL */
{
struct psl* mappedPsl = mapAlignment(inPsl, mapAln);

/* only output if blocks were actually mapped */
if ((mappedPsl != NULL) && (mappedPsl->blockCount > 0))
    {
    pslTabOut(mappedPsl, outPslFh);
    if (mapInfoFh != NULL)
        writeMapInfo(mapInfoFh, inPsl, mapAln, mappedPsl);
    }
pslFree(&mappedPsl);
}

static void mapQueryPsl(struct psl* inPsl, struct hash *mapAlns,
                        FILE* outPslFh, FILE *mapInfoFh)
/* map a query psl to all targets  */
{
struct mapAln *mapAln;

for (mapAln = hashFindVal(mapAlns, inPsl->tName); mapAln != NULL; mapAln = mapAln->next)
    mapPslPair(inPsl, mapAln, outPslFh, mapInfoFh);
}

static void pslMap(char* inPslFile, char *mapFile, char *outPslFile)
/* map SNPs to proteins */
{
struct hash *mapAlns;
struct psl* inPsl;
struct lineFile* inPslLf = pslFileOpen(inPslFile);
FILE *outPslFh, *mapInfoFh = NULL;

if (chainMapFile)
    mapAlns = loadMapChains(mapFile);
else
    mapAlns = loadMapPsls(mapFile);

outPslFh = mustOpen(outPslFile, "w");
if (mapInfoFile != NULL)
    {
    mapInfoFh = mustOpen(mapInfoFile, "w");
    fputs(mapInfoHdr, mapInfoFh);
    }
while ((inPsl = pslNext(inPslLf)) != NULL)
    {
    mapQueryPsl(inPsl, mapAlns, outPslFh, mapInfoFh);
    pslFree(&inPsl);
    }
carefulClose(&mapInfoFh);
carefulClose(&outPslFh);
lineFileClose(&inPslLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("Error: wrong number of arguments");
suffix = optionVal("suffix", NULL);
keepTranslated = optionExists("keepTranslated");
chainMapFile = optionExists("chainMapFile");
swapMap = optionExists("swapMap");
mapInfoFile = optionVal("mapInfo", NULL);
pslMap(argv[1], argv[2], argv[3]);

if (errCount > 0)
    {
    fprintf(stderr, "Error: %d non-fatal errors\n", errCount);
    return 1;
    }
else
    return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

