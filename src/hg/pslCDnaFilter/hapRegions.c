/* hapRegions - mapping between sequences containing haplotype regions and
 * reference chromosomes */
#include "common.h"
#include "hapRegions.h"
#include "cDnaAligns.h"
#include "psl.h"
#include "pslTransMap.h"
#include "hash.h"

/* FIXME: doesn't currently handle case where gene is deleted on reference
 * chromosome.  Probably doesn't happen in human data.
 * FIXME: could keep more
 * partial alignments in hap regions if linking was done before min cover.
 */

struct refChrom
/* reference chromosome with links to haplotype regions. */
{
    char *chrom;                 /* chromosome name */
    struct hapChrom *hapChroms;  /* list of haplotype chromosomes */
};

struct hapChrom
/* haplotype pseudo-chromosome region in reference chromosome */
{
    struct hapChrom *next;       /* next for ref chrom */
    struct refChrom *refChrom;   /* link back to refChrom */
    unsigned refStart;           /* ref chrom region */
    unsigned refEnd;
    char *chrom;                 /* pseudo-chromosome name */
    struct psl *mappings;        /* list of haplotype alignments to ref chrom */
};

struct hapRegions
/* map haplotype regions to reference chromosome */
{
    struct hash *refMap;   /* chrom to refChrom object map */
    struct hash *hapMap;   /* hap chrom to haplotype object map */
};

static struct refChrom *refChromGet(struct hapRegions *hr, char *chrom)
/* get a refChrom object, return null if it doesn't exist */
{
return hashFindVal(hr->refMap, chrom);
}

static struct refChrom *refChromAdd(struct hapRegions *hr, char *chrom)
/* add a refChrom object */
{
struct hashEl *hel;
struct refChrom *refChrom;
lmAllocVar(hr->refMap->lm, refChrom);
hel = hashAdd(hr->refMap, chrom, refChrom);
refChrom->chrom = hel->name;
return refChrom;
}

static struct hapChrom *hapChromGet(struct hapRegions *hr, char *chrom)
/* get a hapChrom object, return null if it doesn't exist */
{
return hashFindVal(hr->hapMap, chrom);
}

static struct hapChrom *hapChromAdd(struct hapRegions *hr, struct refChrom *refChrom,
                                    char *chrom)
/* add a hapChrom object */
{
struct hashEl *hel;
struct hapChrom *hapChrom;
lmAllocVar(hr->hapMap->lm, hapChrom);
hel = hashAdd(hr->hapMap, chrom, hapChrom);
hapChrom->refChrom = refChrom;
hapChrom->chrom = hel->name;
slAddHead(&refChrom->hapChroms, hapChrom);
return hapChrom;
}

static void addHapMapping(struct hapRegions *hr, struct psl *mapping)
/* add a haplotype mapping alignment */
{
struct refChrom *refChrom;
struct hapChrom *hapChrom;

refChrom = refChromGet(hr, mapping->tName);
if (refChrom == NULL)
    refChrom = refChromAdd(hr, mapping->tName);
hapChrom = hapChromGet(hr, mapping->qName);
if (hapChrom == NULL)
    {
    hapChrom = hapChromAdd(hr, refChrom, mapping->qName);
    hapChrom->refStart = mapping->tStart;
    hapChrom->refEnd = mapping->tEnd;
    }
else
    {
    hapChrom->refStart = min(hapChrom->refStart, mapping->tStart);
    hapChrom->refEnd = max(mapping->tEnd, hapChrom->refEnd);
    }
slAddHead(&hapChrom->mappings, mapping);
}

struct hapRegions *hapRegionsNew(char *hapPslFile)
/* construct a new hapRegions object from PSL alignments of the haplotype
 * pseudo-chromosomes to the haplotype regions of the reference chromsomes. */
{
struct psl *mapping, *mappings = pslLoadAll(hapPslFile);
struct hapRegions *hr;
AllocVar(hr);
hr->refMap = hashNew(12);
hr->hapMap = hashNew(12);

while ((mapping = slPopHead(&mappings)) != NULL)
    addHapMapping(hr, mapping);
return hr;
}

void hapRegionsFree(struct hapRegions **hrPtr)
/* free a hapRegions object */
{
struct hapRegions *hr = *hrPtr;
if (hr != NULL)
    {
    hashFree(&hr->refMap);
    hashFree(&hr->hapMap);
    freeMem(hr);
    *hrPtr = NULL;
    }
}

boolean hapRegionsIsHapChrom(struct hapRegions *hr, char *chrom)
/* determine if a chromosome is a haplotype pseudo-chromosome */
{
return hashLookup(hr->hapMap, chrom) != NULL;
}

boolean hapRegionsInHapRegion(struct hapRegions *hr, char *chrom, int start, int end)
/* determine if chrom range is in a haplotype region of a reference chromosome */
{
struct refChrom *rc = hashFindVal(hr->refMap, chrom);
if (rc != NULL)
    {
    struct hapChrom *hc;
    for (hc = rc->hapChroms; hc != NULL; hc = hc->next)
        {
        if ((end > hc->refStart) && (start < hc->refEnd))
            return TRUE;
        }
    }
return FALSE;
}

static struct psl *mapToRef(struct hapChrom *hapChrom, struct cDnaAlign *hapAln,
                            FILE *hapRefMappedFh)
/* map an alignment on a haplotype chromosome to a list of mapped alignments.
 * Return NULL in can't be mapped.  qName of mapped will contain alnId of
 * hapAln if alnIdQNameMode is set. */
{
char qName[512];
struct psl *mapping, *m, *mappedHaps = NULL;
safef(qName, sizeof(qName), "%s#%d", hapAln->psl->qName, hapAln->alnId);

for (mapping = hapChrom->mappings; mapping != NULL; mapping = mapping->next)
    {
    struct psl *mapped = pslTransMap(pslTransMapNoOpts, hapAln->psl, mapping);
    while ((m = slPopHead(&mapped)) != NULL)
        {
        if (alnIdQNameMode)
            {
            freeMem(m->qName);
            m->qName = cloneString(qName);
            }
        slAddHead(&mappedHaps, m);
        if (hapRefMappedFh != NULL)
            pslTabOut(m, hapRefMappedFh);
        }
    }
return mappedHaps;
}

static struct psl *mapCDnaCDnaAln(struct cDnaAlign *refAln, struct psl *mappedHap,
                                  FILE *hapRefCDnaFh)
/* create cdna to cdna alignments from mappedHap and refAln, return
 * NULL if can't be mapped */
{
struct psl *cDnaCDnaAln = NULL;
if (sameString(refAln->psl->tName, mappedHap->tName)
    && rangeIntersection(refAln->psl->tStart, refAln->psl->tEnd,
                         mappedHap->tStart, mappedHap->tEnd))
    {
    pslSwap(mappedHap, FALSE);
    cDnaCDnaAln = pslTransMap(pslTransMapNoOpts, refAln->psl, mappedHap);
    pslSwap(mappedHap, FALSE);
    if ((hapRefCDnaFh != NULL) && (cDnaCDnaAln != NULL))
        cDnaAlignPslOut(cDnaCDnaAln, refAln->alnId, hapRefCDnaFh);
    }
return cDnaCDnaAln;
}

static struct range getHapQRange(struct hapChrom *hapChrom, struct cDnaAlign *refAln)
/* find the range of an mRNA that is aligned to a haplotype region of a ref chrom */
{

struct psl *refPsl = refAln->psl;
struct range qRange = {0, 0};

if ((refPsl->tStart >= hapChrom->refStart) && (refPsl->tEnd <= hapChrom->refEnd))
    {
    /* fast-path, completely contained */
    qRange.end = refPsl->qSize;
    }
else
    {
    /* extends outside of region */
    unsigned hapTStart = hapChrom->refStart, hapTEnd = hapChrom->refEnd;
    int iBlk;
    if (refPsl->strand[1] == '-')
        reverseUnsignedRange(&hapTStart, &hapTEnd, refPsl->tSize);

    /* find start of alignment in region */
    for (iBlk = 0; iBlk < refPsl->blockCount; iBlk++)
        {
        unsigned tEnd = refPsl->tStarts[iBlk]+refPsl->blockSizes[iBlk];
        if (tEnd > hapTStart)
            {
            if (refPsl->tStarts[iBlk] >= hapTStart)
                {
                /* before block */
                qRange.start = refPsl->qStarts[iBlk];
                }
            else
                {
                 /* in block */
                qRange.start = refPsl->qStarts[iBlk] + (hapTStart-refPsl->tStarts[iBlk]);
                }
            break;
            }
        }
    
    /* find end of alignment in region. */
    for (iBlk = refPsl->blockCount-1; iBlk >= 0; iBlk--)
        {
        unsigned tEnd = refPsl->tStarts[iBlk]+refPsl->blockSizes[iBlk];
        if (refPsl->tStarts[iBlk] <= hapTEnd)
            {
            if (tEnd <= hapTEnd)
                {
                /* after block */
                qRange.end = refPsl->qStarts[iBlk] + refPsl->blockSizes[iBlk];
                }
            else
                {
                 /* in block */
                qRange.end = refPsl->qStarts[iBlk] + (hapTEnd-refPsl->tStarts[iBlk]);
                }
            break;
            }
        }
    if (refPsl->strand[0] == '-')
        reverseUnsignedRange(&qRange.start, &qRange.end, refPsl->qSize);
    }
assert(qRange.start < qRange.end);
return qRange;
}

static UBYTE *clearBaseArray(struct cDnaAlign *aln)
/* get cleared per-base array, growing if needed. Warning: this static */
{
static UBYTE *bases = NULL;
static unsigned maxBases = 0;
if (aln->psl->qSize > maxBases)
    {
    maxBases = 2*aln->psl->qSize;
    bases = needLargeMemResize(bases, maxBases);
    }
zeroBytes(bases, maxBases);
return bases;
}

static void markMappedSameBases(struct psl *cDnaCDnaAln, UBYTE *bases)
/* flag aligned same bases in cDNA-cDNA mapped alignment */
{
int iBlk, i;
for (iBlk = 0; iBlk < cDnaCDnaAln->blockCount; iBlk++)
    {
    if (cDnaCDnaAln->qStarts[iBlk] == cDnaCDnaAln->tStarts[iBlk])
        {
        UBYTE *b = bases + cDnaCDnaAln->qStarts[iBlk];
        for (i = 0; i < cDnaCDnaAln->blockSizes[iBlk]; i++)
            *b++ = TRUE;
        }
    }
}

static void compareHapRefPair(struct psl *mappedHap, struct cDnaAlign *refAln,
                              UBYTE *bases, FILE *hapRefCDnaFh)
/* map cDNA to cDNA alignments and flagged mapped bases in base array */
{
struct psl *cDnaCDnaAln = mapCDnaCDnaAln(refAln, mappedHap, hapRefCDnaFh);
if (cDnaCDnaAln != NULL)
    {
    markMappedSameBases(cDnaCDnaAln, bases);
    pslFree(&cDnaCDnaAln);
    }
}

static float scoreHapRefPair(struct cDnaAlign *hapAln, struct hapChrom *hapChrom,
                             struct psl *mappedHaps, struct cDnaAlign *refAln,
                             FILE *hapRefCDnaFh)
/* Score a hapAln with refAln based on mapping a maping to the ref chrom and
 * then a mapping of the two .  An issue is that the mapping a given hapAln to
 * the reference chromosome might be fragmented into multiple alignments due
 * to multiple mapping alignments.  So we count the total number of
 * non-redundent bases that are aligned. */
{
struct range qRange =  getHapQRange(hapChrom, refAln);
struct psl *mappedHap;
int i, cnt = 0;
UBYTE *bases = clearBaseArray(hapAln);

for (mappedHap = mappedHaps; mappedHap != NULL; mappedHap = mappedHap->next)
    compareHapRefPair(mappedHap, refAln, bases, hapRefCDnaFh);
for (i = 0; i < refAln->psl->qSize; i++)
    {
    if (bases[i])
        cnt++;
    }
return ((float)cnt)/(float)(qRange.end-qRange.start);
}

static void linkRefToHap(struct cDnaAlign *refAln, struct cDnaAlign *hapAln,
                         float score)
/* link a refAln ot a hapAln */
{
cDnaAlnLinkHapAln(refAln, hapAln, score);
if (verboseLevel() >= 5)
    {
    verbose(5, "link refAln ");
    cDnaAlignVerbLoc(5, refAln);
    verbose(5, " to hapAln ");
    cDnaAlignVerbLoc(5, hapAln);
    verbose(5, " score: %0.3f\n", score);
    }
}

static void mapHapAlign(struct hapRegions *hr, struct cDnaAlign *hapAln,
                        FILE *hapRefMappedFh, FILE *hapRefCDnaFh)
/* map a haplotype chrom alignment to reference chrom alignments and link if
 * they can be mapped. */
{
struct hapChrom *hapChrom = hapChromGet(hr, hapAln->psl->tName);
struct psl *mappedHaps = mapToRef(hapChrom, hapAln, hapRefMappedFh);
struct cDnaAlign *aln;

for (aln = hapAln->cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && aln->isHapRegion)
        {
        float score = scoreHapRefPair(hapAln, hapChrom, mappedHaps, aln, hapRefCDnaFh);
        if (score > 0)
            linkRefToHap(aln, hapAln, score);
        }
    }
pslFreeList(&mappedHaps);
}

void hapRegionsLink(struct hapRegions *hr, struct cDnaQuery *cdna,
                    FILE *hapRefMappedFh, FILE *hapRefCDnaFh)
/* Link a haplotype chrom alignments to reference chrom alignments if
 * they can be mapped. */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && aln->isHapChrom)
        mapHapAlign(hr, aln, hapRefMappedFh, hapRefCDnaFh);
    }
}
