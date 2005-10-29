/* pslMap - map PSLs alignments to new targets using alignments of the old
 * target to the new target. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: pslMap.c,v 1.5 2005/10/29 02:54:59 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"suffix", OPTION_STRING},
    {"keepTranslated", OPTION_BOOLEAN},
    {NULL, 0}
};

/* Values parsed from command line */
static char* suffix = NULL;
static boolean keepTranslated = FALSE;

/* count of non-fatal errors */
static int errCount = 0;

struct block
/* coordinates of a block */
{
    int qStart;          /* Query start position. */
    int qEnd;            /* Query end position. */
    int tStart;          /* Target start position. */
    int tEnd;            /* Target end position. */
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslMap - map PSLs alignments to new targets using alignments of\n"
  "the old target to the new target.  Given inPsl and mapPsl, where\n"
  "the target of inPsl is the query of mapPsl, create a new PSL with\n"
  "the query of inPsl aligned to the target of mapPsl.  If inPsl is a\n"
  "protein to nucleotide alignment and mapPsl is a nucleotide to\n"
  "nucleotide alignment, the resulting alignment is nucleotide to\n"
  "nucleotide alignment of a hypothetical mRNA that would code for\n"
  "the protein.  This is useful as it gives base alignments of\n"
  "spliced codons.\n"
  "\n"
  "usage:\n"
  "   pslMap [options] inPsl mapPsl outPsl\n"
  "\n"
  "Options:\n"
  "  -suffix=str - append str to the query ids in the output alignment.\n"
  "   Useful with protein alignments, where the result is not actually\n"
  "   and alignment of the protein.\n"
  "  -keepTranslated - if either psl is translated, the output psl will\n"
  "   be translated (both strands explicted).  Normally an untranslated\n"
  "   psl will always be created\n"
  );
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

void pslProtToNA(struct psl *psl)
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

void pslNAToProt(struct psl *psl)
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

struct hash* hashPsls(char *pslFile)
/* read a psl file and hash by query, linking multiple PSLs for the
 * same query.*/
{
struct hash* hash = hashNew(20);
struct psl* psl;
struct lineFile *pslLf = pslFileOpen(pslFile);
while ((psl = pslNext(pslLf)) != NULL)
    {
    struct hashEl *hel = hashLookup(hash, psl->qName);
    if (hel == NULL)
        hashAdd(hash, psl->qName, psl);
    else
        slAddHead((struct psl**)&hel->val, psl);
    }
lineFileClose(&pslLf);
return hash;
}

struct psl* createMappedPsl(struct psl* inPsl, struct psl* mapPsl,
                            int mappedPslMax)
/* setup a PSL for the output alignment */
{
struct psl* mappedPsl;
int len;
int newSize = (mappedPslMax * sizeof(unsigned));
assert(pslTStrand(inPsl) == pslQStrand(mapPsl));

AllocVar(mappedPsl);
len = strlen(inPsl->qName) + 1;
if (suffix != NULL)
    len += strlen(suffix);
mappedPsl->qName = needMem(len);
strcpy(mappedPsl->qName, inPsl->qName);
if (suffix != NULL)
    strcat(mappedPsl->qName, suffix);
mappedPsl->qSize = inPsl->qSize;
mappedPsl->tName = cloneString(mapPsl->tName);
mappedPsl->tSize = mapPsl->tSize;

/* strand can be taken from both alignments, since common sequence is in same
 * orientation. */
mappedPsl->strand[0] = pslQStrand(inPsl);
mappedPsl->strand[1] = pslTStrand(mapPsl);

/* allocate initial array space */
mappedPsl->blockSizes = needMem(newSize);
mappedPsl->qStarts = needMem(newSize);
mappedPsl->tStarts = needMem(newSize);
return mappedPsl;
}

struct block blockFromPslBlock(struct psl* psl, int iBlock)
/* fill in a block object from a psl block */
{
struct block block;
block.qStart = psl->qStarts[iBlock];
block.qEnd = psl->qStarts[iBlock] + psl->blockSizes[iBlock];
block.tStart = psl->tStarts[iBlock];
block.tEnd = psl->tStarts[iBlock] + psl->blockSizes[iBlock];
return block;
}

void growPsl(struct psl* psl, int* pslMax)
/* grow psl block space */
{
int newMax = 2 * *pslMax;
int oldSize = (psl->blockCount * sizeof(unsigned));
int newSize = (newMax * sizeof(unsigned));
psl->blockSizes = needMoreMem(psl->blockSizes, oldSize, newSize);
psl->qStarts = needMoreMem(psl->qStarts, oldSize, newSize);
psl->tStarts = needMoreMem(psl->tStarts, oldSize, newSize);
*pslMax = newMax;
}

void addPslBlock(struct psl* psl, struct block* blk, int* pslMax)
/* add a block to a psl */
{
unsigned newIBlk = psl->blockCount;

assert((blk->qEnd - blk->qStart) == (blk->tEnd - blk->tStart));
if (newIBlk >= *pslMax)
    growPsl(psl, pslMax);
psl->qStarts[newIBlk] = blk->qStart;
psl->tStarts[newIBlk] = blk->tStart;
psl->blockSizes[newIBlk] = blk->qEnd - blk->qStart;
/* lie about match counts. */
psl->match += psl->blockSizes[newIBlk];
psl->blockCount++;
}

void setPslBounds(struct psl* mappedPsl)
/* set sequences bounds on mapped PSL */
{
int lastBlk = mappedPsl->blockCount-1;

/* set start/end of sequences */
mappedPsl->qStart = mappedPsl->qStarts[0];
mappedPsl->qEnd = mappedPsl->qStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslQStrand(mappedPsl) == '-')
    reverseUnsignedRange(&mappedPsl->qStart, &mappedPsl->qEnd, mappedPsl->qSize);

mappedPsl->tStart = mappedPsl->tStarts[0];
mappedPsl->tEnd = mappedPsl->tStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslTStrand(mappedPsl) == '-')
    reverseUnsignedRange(&mappedPsl->tStart, &mappedPsl->tEnd, mappedPsl->tSize);
}

void adjustOrientation(struct psl *inPsl, char *mapPslOrigStrand,
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

struct block findBlockMapping(struct psl* inPsl, struct psl* mapPsl,
                              struct block* align1Blk)
/* Map part or all of a ungapped psl block to the genome.  This returns the
 * coordinates of the sub-block starting at the beginning of the align1Blk.
 * If this is a gap, either the target or query coords are zero.  This works
 * in genomic strand space.
 */
{
int iBlk;
struct block mappedBlk;
ZeroVar(&mappedBlk);

/* search for block or gap containing start of mrna block */
for (iBlk = 0; iBlk < mapPsl->blockCount; iBlk++)
    {
    unsigned mStart = mapPsl->qStarts[iBlk];
    unsigned mEnd = mapPsl->qStarts[iBlk]+mapPsl->blockSizes[iBlk];
    unsigned gStart = mapPsl->tStarts[iBlk];
    if (align1Blk->tStart < mStart)
        {
        /* mRNA start in genomic gap before this block, this will
         * be a inPsl insert */
        unsigned size = (align1Blk->tEnd < mStart)
            ? (align1Blk->tEnd - align1Blk->tStart)
            : (mStart - align1Blk->tStart);
        mappedBlk.qStart = align1Blk->qStart;
        mappedBlk.qEnd = align1Blk->qStart + size;
        return mappedBlk;
        }
    if ((align1Blk->tStart >= mStart) && (align1Blk->tStart < mEnd))
        {
        /* common sequence start contained in this block, this handles aligned
         * and genomic inserts */
        unsigned off = align1Blk->tStart - mStart;
        unsigned size = (align1Blk->tEnd > mEnd)
            ? (mEnd - align1Blk->tStart)
            : (align1Blk->tEnd - align1Blk->tStart);
        mappedBlk.qStart = align1Blk->qStart;
        mappedBlk.qEnd = align1Blk->qStart + size;
        mappedBlk.tStart = gStart + off;
        mappedBlk.tEnd = gStart + off + size;
        return mappedBlk;
        }
    }

/* reached the end of the mRNA->genome alignment, finish off the 
 * rest of the the protein as an insert */
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qEnd;
return mappedBlk;
}

boolean mapBlock(struct psl *inPsl, struct psl* mapPsl,
                 struct block *align1Blk, struct psl* mappedPsl,
                 int* mappedPslMax)
/* Add a PSL block from a ungapped portion of an alignment alignment, mapping
 * it to the genome.  If the started of the inPsl block maps to a part of the
 * mapPsl that is aligned, it is added as a PSL block, otherwise the gap is
 * skipped.  Block starts are adjusted for next call.  Return FALSE if the end
 * of the alignment is reached. */
{
struct block mappedBlk;
unsigned size;
assert(align1Blk->qStart <= align1Blk->qEnd);
assert(align1Blk->tStart <= align1Blk->tEnd);
assert((align1Blk->qEnd - align1Blk->qStart) == (align1Blk->tEnd - align1Blk->tStart));

if ((align1Blk->qStart >= align1Blk->qEnd) || (align1Blk->tStart >= align1Blk->tEnd))
    return FALSE;  /* end of ungapped block. */

/* find block or gap with start coordinates of mrna */
mappedBlk = findBlockMapping(inPsl, mapPsl, align1Blk);

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

struct psl* mapAlignment(struct psl *inPsl, struct psl *mapPsl)
/* map one pair of alignments. */
{
int mappedPslMax = 8; /* allocated space in output psl */
char mapPslOrigStrand[3];
boolean rcMapPsl = (pslTStrand(inPsl) != pslQStrand(mapPsl));
boolean cnv1 = (pslIsProtein(inPsl) && !pslIsProtein(mapPsl));
boolean cnv2 = (pslIsProtein(mapPsl) && !pslIsProtein(inPsl));
int iBlock;
struct psl* mappedPsl;


if (inPsl->tSize != mapPsl->qSize)
    {
    fprintf(stderr, "Non-fatal error: inPsl %s tSize (%d) != mapPsl %s qSize (%d)\n",
            inPsl->tName, inPsl->tSize, mapPsl->qName, mapPsl->qSize);
    errCount++;
    return NULL;
    }

/* convert protein PSLs */
if (cnv1)
    pslProtToNA(inPsl);
if (cnv2)
    pslProtToNA(mapPsl);

/* need to ensure common sequence in same orientation, save strand for later */
safef(mapPslOrigStrand, sizeof(mapPslOrigStrand), "%s", mapPsl->strand);
if (rcMapPsl)
    pslRc(mapPsl);

mappedPsl = createMappedPsl(inPsl, mapPsl, mappedPslMax);

/* Fill in ungapped blocks.  */
for (iBlock = 0; iBlock < inPsl->blockCount; iBlock++)
    {
    struct block align1Blk = blockFromPslBlock(inPsl, iBlock);
    while (mapBlock(inPsl, mapPsl, &align1Blk, mappedPsl, &mappedPslMax))
        continue;
    }

/* finish up psl */
setPslBounds(mappedPsl);
adjustOrientation(inPsl, mapPslOrigStrand, mappedPsl);

/* restore input */
if (rcMapPsl)
    {
    pslRc(mapPsl);
    strcpy(mapPsl->strand, mapPslOrigStrand);
    }
if (cnv1)
    pslNAToProt(inPsl);
if (cnv2)
    pslNAToProt(mapPsl);


return mappedPsl;
}

void mapPslPair(struct psl *inPsl, struct psl *mapPsl, FILE* outPslFh)
/* map one pair of query and target PSL */
{
struct psl* mappedPsl = mapAlignment(inPsl, mapPsl);

/* only output if blocks were actually mapped */
if ((mappedPsl != NULL) && (mappedPsl->blockCount > 0))
    {
    pslTabOut(mappedPsl, outPslFh);
    pslFree(&mappedPsl);
    }
}

void mapQueryPsl(struct psl* inPsl, struct hash *mapPslHash, FILE* outPslFh)
/* map a query psl to all targets  */
{
struct psl *mapPsl;

for (mapPsl = hashFindVal(mapPslHash, inPsl->tName); mapPsl != NULL; mapPsl = mapPsl->next)
    mapPslPair(inPsl, mapPsl, outPslFh);
}

void pslMap(char* inPslFile, char *mapPslFile, char *outPslFile)
/* map SNPs to proteins */
{
struct hash *mapPslHash = hashPsls(mapPslFile);
struct psl* inPsl;
struct lineFile* inPslLf = pslFileOpen(inPslFile);
FILE *outPslFh = mustOpen(outPslFile, "w");

while ((inPsl = pslNext(inPslLf)) != NULL)
    {
    mapQueryPsl(inPsl, mapPslHash, outPslFh);
    pslFree(&inPsl);
    }
carefulClose(&outPslFh);
lineFileClose(&inPslLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
suffix = optionVal("suffix", NULL);
keepTranslated = optionExists("keepTranslated");

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

