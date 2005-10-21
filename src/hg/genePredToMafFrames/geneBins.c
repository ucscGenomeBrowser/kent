/* geneBins - objects used to hold gene related data */
#include "common.h"
#include "geneBins.h"
#include "genePred.h"
#include "genePredReader.h"
#include "chromBins.h"
#include "binRange.h"
#include "localmem.h"
#include "dnautil.h"
#include "maf.h"
#include "mafFrames.h"

static char *chromStrAlloc(struct geneBins *genes, char *chrom)
/* allocated a chrom name from the pool, caching the last allocated */
{
if ((genes->curChrom == NULL) || !sameString(chrom, genes->curChrom))
    genes->curChrom = lmCloneString(genes->memPool, chrom);
return genes->curChrom;
}

static void checkOverlap(struct geneBins *genes, struct binKeeper *chrBins,
                         struct genePred *gp, int start, int end)
/* check for a CDS exon overlapping any other */
{
struct binElement *el = binKeeperFindLowest(chrBins, start, end);
if (el != NULL)
    errAbort("CDS exon of %s overlaps %s, CDS must be single coverage",
             gp->name, ((struct cdsExon*)el->val)->gene->name);
}

static struct cdsExon *loadExon(struct gene *gene, struct binKeeper *chrBins,
                                struct genePred *gp, int iExon, int start, int end,
                                int cdsOff)
/* load information about an exon into various structures */
{
struct cdsExon *exon;
checkOverlap(gene->genes, chrBins, gp, start, end);
lmAllocVar(gene->genes->memPool, exon);
exon->gene = gene;
exon->chrom = chromStrAlloc(gene->genes, gp->chrom);
exon->chromStart = start;
exon->chromEnd = end;
exon->strand = gp->strand[0];
exon->frame = gp->exonFrames[iExon];
if (exon->strand == '+')
    exon->exonNum = iExon;
else
    exon->exonNum = (gp->exonCount-1) - iExon;
exon->cdsOff = cdsOff;
binKeeperAdd(chrBins, start, end, exon);
slAddHead(&gene->exons, exon);
return exon;
}

static void loadGene(struct geneBins *genes, struct genePred *gp)
/* break one gene into cds exons and add to bins. check for overlapping CDS */
{
struct binKeeper *chrBins = chromBinsGet(genes->bins, gp->chrom, TRUE);
struct gene *gene;
int iExon, start, end;
int cdsOff = 0;

lmAllocVar(genes->memPool, gene);
gene->genes = genes;
slAddHead(&genes->genes, gene);
gene->name = lmCloneString(genes->memPool, gp->name);

/* process in transcription order so we get the cdsOff set */
if (gp->strand[0] == '+')
    {
    for (iExon = 0; iExon < gp->exonCount; iExon++)
        if (genePredCdsExon(gp, iExon, &start, &end))
            {
            loadExon(gene, chrBins, gp, iExon, start, end, cdsOff);
            cdsOff += (end - start);
            }
    }
else
    {
    for (iExon = gp->exonCount-1; iExon >= 0; iExon--)
        if (genePredCdsExon(gp, iExon, &start, &end))
            {
            loadExon(gene, chrBins, gp, iExon, start, end, cdsOff);
            cdsOff += (end - start);
            }
    }
slReverse(&gene->exons);
}

static void loadGenes(struct geneBins *genes, char *genePredFile)
/* load genePred file into object */
{
struct genePredReader *gpr = genePredReaderFile(genePredFile, NULL);
struct genePred *gp;

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if ((gp->optFields & genePredExonFramesFld) == 0)
        genePredAddExonFrames(gp);
    loadGene(genes, gp);
    }
genePredReaderFree(&gpr);
}

struct geneBins *geneBinsNew(char *genePredFile)
/* construct a new geneBins object from the specified file */
{
struct geneBins *genes;
AllocVar(genes);
genes->bins = chromBinsNew(NULL);
genes->memPool = lmInit(1024*1024);
loadGenes(genes, genePredFile);
return genes;
}

void geneBinsFree(struct geneBins **genesPtr)
/* free geneBins object */
{
struct geneBins *genes = *genesPtr;
if (genes != NULL)
    {
    chromBinsFree(&genes->bins);
    lmCleanup(&genes->memPool);
    freeMem(genes);
    *genesPtr = NULL;
    }
}

static char *skipSrcDb(char *src)
/* skip db. part of a src string, or an error */
{
char *p = strchr(src, '.');
if (p == NULL)
    errAbort("can't find \".\" in MAF component src name \"%s\"", src);

return p+1;
}

static int cmpExonsAscend(const void *va, const void *vb)
/* compare binElement referencing cdsExons into assending order.  Assumes
 * sames chrom. */
{
const struct cdsExon *a = (*((struct binElement **)va))->val;
const struct cdsExon *b = (*((struct binElement **)vb))->val;
int dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

static int cmpExonsDescend(const void *va, const void *vb)
/* compare binElement referencing cdsExons into descending order.  Assumes
 * sames chrom. */
{
return cmpExonsAscend(vb, va);
}

struct binElement *geneBinsFind(struct geneBins *genes, struct mafComp *comp,
                                int sortDir)
/* Return list of references to exons overlapping the specified component,
 * sorted into assending order sortDir is 1, descending if it's -1.
 * slFreeList the returned list. */
{
char *chrom = skipSrcDb(comp->src);
int start = comp->start;
int end = (comp->start + comp->size)-1;
struct binElement *exons;

/* get positive strand coordinate to match exon */
if (comp->strand == '-')
    reverseIntRange(&start, &end, comp->srcSize);

exons = chromBinsFind(genes->bins, chrom, start, end);
slSort(&exons, (sortDir > 0) ? cmpExonsAscend : cmpExonsDescend);
return exons;
}

static void cdsExonClone(struct cdsExon *exon, struct gene *gene2)
/* clone an exon and add to gene2 */
{
struct cdsExon *exon2;
lmAllocVar(gene2->genes->memPool, exon2);
exon2->gene = gene2;
exon2->chrom = exon->chrom;
exon2->chromStart = exon->chromStart;
exon2->chromEnd = exon->chromEnd;
exon2->strand = exon->strand;
exon2->frame = exon->frame;
exon2->exonNum = exon->exonNum;
exon2->cdsOff = exon->cdsOff;
slAddHead(&gene2->exons, exon2);
}

struct gene* geneClone(struct gene *gene)
/* clone a gene and it's exons.  Does not clone exonFrames.  Used when
 * spliting a gene mapped to two locations */
{
struct gene *gene2;
struct cdsExon *exon;
lmAllocVar(gene->genes->memPool, gene2);
gene2->genes = gene->genes;
slAddHead(&gene->genes->genes, gene2);
gene2->name = gene->name; /* memory is in pool */

for (exon = gene->exons; exon != NULL; exon = exon->next)
    cdsExonClone(exon, gene2);
slReverse(&gene2->exons);
return gene2;
}

struct cdsExon *geneGetExon(struct gene *gene, int exonNum)
/* get an exon from a gene by it's exon number, or an error */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    if (exon->exonNum == exonNum)
        return exon;
errAbort("geneGetExon: gene %s doesn't have exon %d", gene->name, exonNum);
return NULL;
}

static int exonFramesTargetOffCmp(const void *va, const void *vb)
/* compare exopFrames by target mapping in the direction of transcriptios and
 * offset in gene.
 */
{
struct exonFrames *a = *((struct exonFrames **)va);
struct exonFrames *b = *((struct exonFrames **)vb);
int dif = a->cdsStart - b->cdsStart;
if (dif == 0)
    dif = a->cdsEnd - b->cdsEnd;
if (dif == 0)
    {
    if (a->mf.strand[0] == '+')
        dif = a->mf.chromStart - b->mf.chromStart;
    else
        dif = b->mf.chromStart - a->mf.chromStart;
    }
return dif;
}

void geneSortFramesTargetOff(struct gene *gene)
/* sort the exonFrames in each exon into target transcription order then gene
 * offset. */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    slSort(&exon->frames, exonFramesTargetOffCmp);
}

static int exonFramesOffTargetCmp(const void *va, const void *vb)
/* compare exopFrames by offset in gene and target mapping in the direction of
 * transcriptios. */
{
struct exonFrames *a = *((struct exonFrames **)va);
struct exonFrames *b = *((struct exonFrames **)vb);
int dif = a->cdsStart - b->cdsStart;
if (dif == 0)
    dif = a->cdsEnd - b->cdsEnd;
if (dif == 0)
    {
    if (a->mf.strand[0] == '+')
        dif = a->mf.chromStart - b->mf.chromStart;
    else
        dif = b->mf.chromStart - a->mf.chromStart;
    }
return dif;
}

void geneSortFramesOffTarget(struct gene *gene)
/* sort the exonFrames in each exon into gene offset then target transcription
 * order */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    slSort(&exon->frames, exonFramesOffTargetCmp);
}

struct exonFrames *cdsExonAddFrames(struct cdsExon *exon,
                                    char *src, int qStart, int qEnd,
                                    char *tName, int tStart, int tEnd,
                                    char frame, char strand, int cdsOff)
/* allocate a new mafFrames object and link it exon */
{
struct exonFrames *ef;
lmAllocVar(exon->gene->genes->memPool, ef);
ef->exon = exon;

/* fill in query part */
ef->mf.src = lmCloneString(exon->gene->genes->memPool, src);
ef->srcStart = qStart;
ef->srcEnd = qEnd;
ef->cdsStart = cdsOff;
ef->cdsEnd = cdsOff + (qEnd - qStart);

/* fill in mafFrames part */
ef->mf.chrom = lmCloneString(exon->gene->genes->memPool, tName);
ef->mf.chromStart = tStart;
ef->mf.chromEnd = tEnd;
ef->mf.frame = frame;
ef->mf.strand[0] = strand;
ef->mf.name = exon->gene->name;
ef->mf.prevFramePos = -1;
ef->mf.nextFramePos = -1;

slAddHead(&exon->frames, ef);
exon->gene->numExonFrames++;
return ef;
}

struct exonFrames *geneFirstExonFrames(struct gene *gene)
/* find the first exons frames object, or error if not found */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    if (exon->frames != NULL)
        return exon->frames;
    }
errAbort("no exonFrames found for %s", gene->name);
return NULL;
}

struct exonFrames *exonFramesNext(struct exonFrames *ef)
/* get the next exonFrames object, moving on to the next exon with exonFrames
 * after the current one */
{
if (ef->next != NULL)
    return ef->next;
else
    {
    struct cdsExon *exon;
    for (exon = ef->exon->next; exon != NULL; exon = exon->next)
        {
        if (exon->frames != NULL)
            return exon->frames;
        }
    }
return NULL;
}

void exonFramesDump(FILE *fh, struct exonFrames *ef)
/* dump contents of an exonFrame for debugging purposes */
{
fprintf(fh, "    ef: %d src: %d-%d cds: %d-%d dest:%s:%d-%d(%s) len: %d fm: %d prev/next: %d/%d\n",
        ef->exon->exonNum,
        ef->srcStart, ef->srcEnd, ef->cdsStart, ef->cdsEnd,
        ef->mf.chrom, ef->mf.chromStart, ef->mf.chromEnd, ef->mf.strand,
        (ef->mf.chromEnd - ef->mf.chromStart),
        ef->mf.frame, ef->mf.prevFramePos, ef->mf.nextFramePos);
}

void cdsExonDump(FILE *fh, struct cdsExon *exon)
/* dump contents of a cdsExon for debugging purposes */
{
struct exonFrames *ef;
fprintf(fh, "  exon %d src: %s:%d-%d(%c) fr: %d off: %d\n",
        exon->exonNum, exon->chrom, exon->chromStart, exon->chromEnd,
        exon->strand, exon->frame, exon->cdsOff);
for (ef = exon->frames; ef != NULL; ef = ef->next)
    exonFramesDump(fh, ef);
}

void geneDump(FILE *fh, struct gene *gene)
/* dump contents of a gene for debugging purposes */
{
struct cdsExon *exon;
fprintf(fh, "%s\n", gene->name);
for (exon = gene->exons; exon != NULL; exon = exon->next)
    cdsExonDump(fh, exon);
}
