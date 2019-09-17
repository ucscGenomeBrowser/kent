/* agpAlign - Given AGP files mapping A->C and B->C, produce PSL mapping A->B. */
#include "common.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "hash.h"
#include "hdb.h"
#include "linefile.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpAlign - Given AGP files mapping A->C and B->C, produce unscored PSL gapless\n"
  "blocks mapping A->B.  The PSL should subsequently be processed by axtChain.\n"
  "\n"
  "usage:\n"
  "   agpAlign hg19.comp.agp hg38.comp.agp hg19.chrom.sizes hg38.chrom.sizes hg19ToHg38.psl\n"
  "\n"
  "The NCBI .comp.agp files are generally found under\n"
  "  <assemblyDir>/Primary_Assembly/assembled_chromosomes/AGP/\n"
  "and map chromosomes to contigs ('components', AC/AF/AL/etc) instead of\n"
  "scaffolds (GL/KI/etc).  They use GenBank or RefSeq chrom IDs so they need\n"
  "to be piped through something that will swap in UCSC chr* names.\n"
  "\n"
  "The purpose of this is to make starter chains for liftOver chains.\n"
  "The aligned sequences are guaranteed to be identical and usually cover\n"
  "large continuous regions when chained.  Then the usual alignment-based\n"
  "process can find the best match for sequences from different contigs.\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line option definitions. */
static struct optionSpec options[] = {
   {NULL, 0},
};

INLINE boolean agpFragIsGap(struct agpFrag *agpFrag)
/* Return TRUE if agpFrag->type indicates that it's actually struct agpGap cast to agpFrag. */
{
return (agpFrag != NULL) && (agpFrag->type[0] == 'N');
}

static struct agpFrag *agpNextFragOrGapFromLf(struct lineFile *lf)
/* Figure out whether next line from lf is agpFrag or agpGap, load it accordingly and return it.
 * If it is agpGap, cast it to agpFrag so we can keep them in the same list. */
{
struct agpFrag *agp = NULL;
char *line = NULL;
if (lineFileNextReal(lf, &line))
    {
    char *words[10];
    int wordCount = chopLine(line, words);
    lineFileExpectAtLeast(lf, 8, wordCount);
    char *type = words[4];
    if (type[0] == 'N')
        {
        // Gap record -- cast to agpFrag but access only agpGap fields
        agp = (struct agpFrag *)agpGapLoad(words);
        // File coords are 1-based
        agp->chromStart--;
        }
    else
        {
        // Sequence fragment -- adjust both chromStart and fragStart
        lineFileExpectAtLeast(lf, 9, wordCount);
        agp = agpFragLoad(words);
        // File coords are 1-based
        agp->chromStart--;
        agp->fragStart--;
        }
    }
return agp;
}

struct hash *storeAgp(char *agpAFile)
/* Return a hash mapping AGP file's component IDs to AGP records that are linked in a list.
 * The list is reversed so we can check whether a frag is preceded by a gap. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(agpAFile, TRUE);
struct agpFrag *agpList = NULL, *agp = NULL;
while ((agp = agpNextFragOrGapFromLf(lf)) != NULL)
    {
    // Both frag and gap records are linked
    slAddHead(&agpList, agp);
    if (! agpFragIsGap(agp))
        // Only frag records are hashed
        hashAdd(hash, agp->frag, agp);
    }
// Don't reverse agpList, we use the links to look backwards.
lineFileClose(&lf);
return hash;
}

static void intersectRanges(unsigned aStart, unsigned aEnd, unsigned bStart, unsigned bEnd,
                            unsigned *retStart, unsigned *retEnd)
/* Intersect ranges A and B and put result in retStart, retEnd.  If the ranges are exclusive
 * then start = end = 0. */
{
unsigned iStart = (aStart > bStart) ? aStart : bStart;
unsigned iEnd = (aEnd < bEnd) ? aEnd : bEnd;
if (iEnd < iStart)
    iEnd = iStart = 0;
*retStart = iStart;
*retEnd = iEnd;
}

static void agpRangeFragToChrom(struct agpFrag *agp, unsigned fragStart, unsigned fragEnd,
                               int *retChromStart, int *retChromEnd)
/* Given AGP and a range in frag coordinates, put the corresponding chrom coords in
 * retChromStart, retChromEnd.  */
{
boolean isRc = (agp->strand[0] == '-');
if (isRc)
    {
    int offset = agp->chromStart + agp->fragEnd;
    *retChromStart = offset - fragEnd;
    *retChromEnd = offset - fragStart;
    }
else
    {
    int offset = agp->chromStart - agp->fragStart;
    *retChromStart = offset + fragStart;
    *retChromEnd = offset + fragEnd;
    }
}

static struct psl *pslSingleBlock(char *qName, int qSize, int qStart, int qEnd,
                                  char *tName, int tSize, int tStart, int tEnd,
                                  char *strand)
/* Alloc & return a psl with a single gapless block. */
{
unsigned opts = 0;
struct psl *psl = pslNew(qName, qSize, qStart, qEnd, tName, tSize, tStart, tEnd,
                         strand, 1, opts);
psl->blockCount = 1;
psl->blockSizes[0] = tEnd - tStart;
psl->tStarts[0] = tStart;
psl->qStarts[0] = strand[0] == '+' ? qStart : (qSize - qEnd);
return psl;
}

void checkPrecedingGaps(struct agpGap *prevGapList, struct agpFrag *agpB, char *aChrom,
                        int aChromSize, int bChromSize, FILE *outF)
/* If both agpA and agpB are preceded by gap(s) of equal size, consider those aligned too. */
{
if (prevGapList && agpB && agpFragIsGap(agpB->next))
    {
    struct agpGap *aGap = prevGapList;
    struct agpGap *bGap = (struct agpGap *)agpB->next;
    char *bChrom = agpB->chrom;
    if (aGap && bGap && agpFragIsGap((struct agpFrag *) bGap) &&
        aGap->size == bGap->size &&
        sameString(aGap->chrom, aChrom) && sameString(bGap->chrom, bChrom))
        {
        // Check for matching gaps preceding these matching gaps
        if (bGap->next && sameString(bGap->next->chrom, bChrom))
            checkPrecedingGaps(aGap->next, agpB->next, aChrom, aChromSize, bChromSize, outF);
        struct psl *psl = pslSingleBlock(bChrom, bChromSize, bGap->chromStart, bGap->chromEnd,
                                         aChrom, aChromSize, aGap->chromStart, aGap->chromEnd,
                                         "+");
        pslTabOut(psl, outF);
        pslFree(&psl);
        }
    }
}

void agpAlign(char *agpAFile, char *agpBFile, char *aChromSizeFile, char *bChromSizeFile,
              char *outFile)
/* agpAlign - Given AGP files mapping A->C and B->C, produce PSL chunks mapping A->B. */
{
// Die early if there are any file opening problems:
FILE *outF = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(agpAFile, TRUE);
struct hash *bToC = storeAgp(agpBFile);
struct hash *aChromSizes = hChromSizeHashFromFile(aChromSizeFile);
struct hash *bChromSizes = hChromSizeHashFromFile(bChromSizeFile);
struct agpGap *prevGapList = NULL;
struct agpFrag *agpA;
while ((agpA = agpNextFragOrGapFromLf(lf)) != NULL)
    {
    if (agpFragIsGap(agpA))
        {
        // Gap record -- keep in reversed list so we can look for gap before frag later
        slAddHead(&prevGapList, agpA);
        }
    else
        {
        // Sequence fragment -- look for match in aToC
        int aChromSize = hashIntVal(aChromSizes, agpA->chrom);
        struct hashEl *helList = hashLookup(bToC, agpA->frag), *hel;
        for (hel = helList;  hel != NULL;  hel = hel->next)
            {
            struct agpFrag *agpB = hel->val;
            if (sameString(agpA->frag, agpB->frag))
                {
                unsigned intxStart = 0, intxEnd = 0;
                intersectRanges(agpA->fragStart, agpA->fragEnd, agpB->fragStart, agpB->fragEnd,
                                &intxStart, &intxEnd);
                if (intxEnd > intxStart)
                    {
                    int bChromSize = hashIntVal(bChromSizes, agpB->chrom);
                    checkPrecedingGaps(prevGapList, agpB, agpA->chrom, aChromSize, bChromSize,
                                       outF);
                    verbose(2, "%s[%u,%u] & [%u,%u] --> [%d,%d]; qCStart=%u, tCStart=%u\n",
                            agpB->frag, agpB->fragStart, agpB->fragEnd,
                            agpA->fragStart, agpA->fragEnd, intxStart, intxEnd,
                            agpB->chromStart, agpA->chromStart);
                    int aStart, aEnd, bStart, bEnd;
                    agpRangeFragToChrom(agpA, intxStart, intxEnd, &aStart, &aEnd);
                    agpRangeFragToChrom(agpB, intxStart, intxEnd, &bStart, &bEnd);
                    char *strand = (agpA->strand[0] == agpB->strand[0]) ? "+" : "-";
                    struct psl *psl = pslSingleBlock(agpB->chrom, bChromSize, bStart, bEnd,
                                                     agpA->chrom, aChromSize, aStart, aEnd,
                                                     strand);
                    pslTabOut(psl, outF);
                    pslFree(&psl);
                    }
                }
            }
        // Not a gap, so reset prevGapList.
        agpGapFreeList(&prevGapList);
        }
    }
lineFileClose(&lf);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
agpAlign(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
