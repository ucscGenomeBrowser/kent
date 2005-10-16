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
                                struct genePred *gp, int iExon, int start, int end)
/* load informat about an exon into various structures */
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
exon->iExon = iExon;
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

lmAllocVar(genes->memPool, gene);
gene->genes = genes;
slAddHead(&genes->genes, gene);
gene->name = lmCloneString(genes->memPool, gp->name);

for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if (genePredCdsExon(gp, iExon, &start, &end))
        loadExon(gene, chrBins, gp, iExon, start, end);
    }
if (gp->strand[0] == '+')
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

static int cmpExonsPos(const void *va, const void *vb)
/* compare binElement containing cds exon for forward strand sort.
 * Assumes sames chrom. */
{
const struct cdsExon *a = (*((struct binElement **)va))->val;
const struct cdsExon *b = (*((struct binElement **)vb))->val;
int dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

static int cmpExonsNeg(const void *va, const void *vb)
/* compare binElement containing cds exon for reverse strand sort. */
{
return !cmpExonsPos(va, vb);
}

struct binElement *geneBinsFind(struct geneBins *genes, struct mafComp *comp)
/* Return list of references to exons overlapping the specified component,
 * sorted into the assending order of the component. slFree returned list. */
{
char *chrom = skipSrcDb(comp->src);
int start = comp->start;
int end = (comp->start + comp->size)-1;
struct binElement *exons;
if (comp->strand == '-')
    reverseIntRange(&start, &end, comp->srcSize);

exons = chromBinsFind(genes->bins, chrom, start, end);
slSort(&exons, ((comp->strand == '+') ? cmpExonsPos : cmpExonsNeg));
return exons;
}

struct binElement *geneBinsChromExons(struct geneBins *genes, char *chrom)
/* Return list of references to all exons on a chromosome, sorted in
 * assending target order. slFreeList returned list. */
{
struct binKeeper *chromBins = chromBinsGet(genes->bins, chrom, FALSE);
if (chromBins != NULL)
    return binKeeperFindAll(chromBins);
else
    return NULL;
}

struct exonFrames *cdsExonAddFrames(struct cdsExon *exon,
                                    char *src, int qStart, int qEnd,
                                    char *tName, int tStart, int tEnd,
                                    char frame, char strand)
/* allocate a new mafFrames object and link it exon */
{
struct exonFrames *ef;
lmAllocVar(exon->gene->genes->memPool, ef);
ef->exon = exon;

/* fill in query part */
ef->mf.src = lmCloneString(exon->gene->genes->memPool, src);
ef->queryStart = qStart;
ef->queryEnd = qEnd;

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
return ef;
}

