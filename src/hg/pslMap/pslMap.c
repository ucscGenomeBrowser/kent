/* pslMap - map PSLs alignments to new targets using alignments of the old
 * target to the new target. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: pslMap.c,v 1.3 2003/12/10 23:17:14 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"suffix", OPTION_STRING},
    {NULL, 0}
};

/* Values parsed from command line */
static char* suffix = NULL;

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
  "the old target to the new target.  Given psl1 and psl2, where\n"
  "the target of psl1 is the query of psl2, create a new PSL with\n"
  "the query of psl1 aligned to the target of psl2.  If psl1 is a\n"
  "protein to nucleotide alignment and psl2 is a nucleotide to\n"
  "nucleotide alignment, the resulting alignment is nucleotide to\n"
  "nucleotide alignment of a hypothetical mRNA that would code for\n"
  "the protein.  This is useful as it gives base alignments of\n"
  "spliced codons.\n"
  "\n"
  "usage:\n"
  "   pslMap [options] psl1 psl2 outPsl\n"
  "\n"
  "Options:\n"
  "  -suffix=str - append str to the query ids in the output alignment.\n"
  "   Useful with protein alignments, where the result is not actually\n"
  "   and alignment of the protein.\n"
  );
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

struct psl* createMappedPsl(struct psl* psl1, struct psl* psl2,
                            int mappedPslMax)
/* setup a PSL for the output alignment */
{
struct psl* mappedPsl;
int len;
int newSize = (mappedPslMax * sizeof(unsigned));
AllocVar(mappedPsl);
len = strlen(psl1->qName) + 1;
if (suffix != NULL)
    len += strlen(suffix);
mappedPsl->qName = needMem(len);
strcpy(mappedPsl->qName, psl1->qName);
if (suffix != NULL)
    strcat(mappedPsl->qName, suffix);
mappedPsl->qSize = psl1->qSize;

mappedPsl->tName = cloneString(psl2->tName);
mappedPsl->tSize = psl2->tSize;
strcpy(mappedPsl->strand, psl2->strand);

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

void finishMappedPsl(struct psl* mappedPsl)
/* finish filling in mapped PSL */
{
int lastBlk = mappedPsl->blockCount-1;

/* set start/end of sequences */
mappedPsl->qStart = mappedPsl->qStarts[0];
mappedPsl->qEnd = mappedPsl->qStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (mappedPsl->strand[0] == '-')
    reverseUnsignedRange(&mappedPsl->qStart, &mappedPsl->qEnd, mappedPsl->qSize);

mappedPsl->tStart = mappedPsl->tStarts[0];
mappedPsl->tEnd = mappedPsl->tStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (mappedPsl->strand[1] == '-')
    reverseUnsignedRange(&mappedPsl->tStart, &mappedPsl->tEnd, mappedPsl->tSize);

}

struct block findBlockMapping(struct psl* psl1, struct psl* psl2,
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
for (iBlk = 0; iBlk < psl2->blockCount; iBlk++)
    {
    unsigned mStart = psl2->qStarts[iBlk];
    unsigned mEnd = psl2->qStarts[iBlk]+psl2->blockSizes[iBlk];
    unsigned gStart = psl2->tStarts[iBlk];
    if (align1Blk->tStart < mStart)
        {
        /* mRNA start in genomic gap before this block, this will
         * be a psl1 insert */
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

boolean mapBlock(struct psl *psl1, struct psl* psl2,
                 struct block *align1Blk, struct psl* mappedPsl,
                 int* mappedPslMax)
/* Add a PSL block from a ungapped portion of an alignment alignment, mapping
 * it to the genome.  If the started of the psl1 block maps to a part of the
 * psl2 that is aligned, it is added as a PSL block, otherwise the gap is
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
mappedBlk = findBlockMapping(psl1, psl2, align1Blk);

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

struct psl* mapAlignment(struct psl *psl1, struct psl* psl2)
/* map one pair of alignments. */
{
int mappedPslMax = 8; /* allocated space in output psl */
char psl1TStrand = (psl1->strand[1] == '-') ? '-' : '+';
char psl2QStrand = psl2->strand[0];
boolean cnv1 = (pslIsProtein(psl1) && !pslIsProtein(psl2));
boolean cnv2 = (pslIsProtein(psl2) && !pslIsProtein(psl1));
int iBlock;
struct psl* mappedPsl;


if (psl1->tSize != psl2->qSize)
    {
    fprintf(stderr, "Non-fatal error: psl1 %s tSize (%d) != psl2 %s qSize (%d)\n",
            psl1->tName, psl1->tSize, psl2->qName, psl2->qSize);
    errCount++;
    return NULL;
    }
if (cnv1)
    pslProtToNA(psl1);
if (cnv2)
    pslProtToNA(psl2);

/* need to ensure common sequence in same orientation */
if (psl1TStrand != psl2QStrand)
    pslRcBoth(psl2);

mappedPsl = createMappedPsl(psl1, psl2, mappedPslMax);

/* Fill in ungapped blocks.  */
for (iBlock = 0; iBlock < psl1->blockCount; iBlock++)
    {
    struct block align1Blk = blockFromPslBlock(psl1, iBlock);
    while (mapBlock(psl1, psl2, &align1Blk, mappedPsl, &mappedPslMax))
        continue;
    }
if (psl1TStrand != psl2QStrand)
    {
    pslRcBoth(psl2);
    pslRcBoth(mappedPsl);
    }

finishMappedPsl(mappedPsl);
if (cnv1)
    pslNAToProt(psl1);
if (cnv2)
    pslNAToProt(psl2);

return mappedPsl;
}

void mapPslPair(struct psl *psl1, struct psl *psl2, FILE* outPslFh)
/* map one pair of query and target PSL */
{
struct psl* mappedPsl = mapAlignment(psl1, psl2);

/* only output if blocks were actually mapped */
if ((mappedPsl != NULL) && (mappedPsl->blockCount > 0))
    {
    pslTabOut(mappedPsl, outPslFh);
    pslFree(&mappedPsl);
    }
}

void mapQueryPsl(struct psl* psl1, struct hash *psl2Hash, FILE* outPslFh)
/* map a query psl to all targets  */
{
struct psl *psl2;

for (psl2 = hashFindVal(psl2Hash, psl1->tName); psl2 != NULL; psl2 = psl2->next)
    mapPslPair(psl1, psl2, outPslFh);
}

void pslMap(char* psl1File, char *psl2File, char *outPslFile)
/* map SNPs to proteins */
{
struct hash *psl2Hash = hashPsls(psl2File);
struct psl* psl1;
struct lineFile* psl1Lf = pslFileOpen(psl1File);
FILE *outPslFh = mustOpen(outPslFile, "w");

while ((psl1 = pslNext(psl1Lf)) != NULL)
    {
    mapQueryPsl(psl1, psl2Hash, outPslFh);
    pslFree(&psl1);
    }
carefulClose(&outPslFh);
lineFileClose(&psl1Lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
suffix = optionVal("suffix", NULL);

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

