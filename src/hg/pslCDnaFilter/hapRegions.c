/* hapRegions - mapping between sequences containing haplotype regions and
 * reference chromosomes */
#include "common.h"
#include "hapRegions.h"
#include "cDnaAligns.h"
#include "psl.h"
#include "pslTransMap.h"
#include "hash.h"

/* Note: doesn't currently handle case where gene is deleted on reference
 * chromosome.  This would require all against all pairwise alignments
 * of regions.
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
    FILE *hapRefMappedFh;
    FILE *hapRefCDnaFh;
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

struct hapRegions *hapRegionsNew(char *hapPslFile, FILE *hapRefMappedFh, FILE *hapRefCDnaFh)
/* construct a new hapRegions object from PSL alignments of the haplotype
 * pseudo-chromosomes to the haplotype regions of the reference chromsomes. */
{
struct psl *mapping, *mappings = pslLoadAll(hapPslFile);
struct hapRegions *hr;
AllocVar(hr);
hr->refMap = hashNew(12);
hr->hapMap = hashNew(12);
hr->hapRefMappedFh = hapRefMappedFh;
hr->hapRefCDnaFh = hapRefCDnaFh;

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

static boolean inHapRegion(struct hapRegions *hr, char *chrom, int start, int end)
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

static boolean alnInHapRegion(struct hapRegions *hr, struct cDnaAlign *refAln)
/* determine if an alignment is in a haplotype region of a reference chromosome */
{
return inHapRegion(hr, refAln->psl->tName, refAln->psl->tStart, refAln->psl->tEnd);
}

static void mapToRefWithMapping(struct hapRegions *hr, struct cDnaAlign *hapAln, struct psl *mapping, struct psl **mappedHaps)
/* map an alignment on a haplotype chromosome using one mapping */
{
struct psl *m;
struct psl *mapped = pslTransMap(pslTransMapNoOpts, hapAln->psl, mapping);
while ((m = slPopHead(&mapped)) != NULL)
    {
    slAddHead(&mappedHaps, m);
    if (hr->hapRefMappedFh != NULL)
        pslTabOut(m, hr->hapRefMappedFh);
    }
}

static struct psl *mapToRef(struct hapRegions *hr, struct refChrom *refChrom, struct cDnaAlign *hapAln, struct hapChrom *hapChrom)
/* map an alignment on a haplotype chromosome to a list of mapped alignments.
 * Return NULL in can't be mapped. */
{
char qName[512];
struct psl *mapping, *mappedHaps = NULL;
safef(qName, sizeof(qName), "%s#%d", hapAln->psl->qName, hapAln->alnId);

for (mapping = hapChrom->mappings; mapping != NULL; mapping = mapping->next)
    {
    if (sameString(mapping->tName, refChrom->chrom))
        mapToRefWithMapping(hr, hapAln, mapping, &mappedHaps);
    }
return mappedHaps;
}

static struct psl *mapCDnaCDnaAln(struct hapRegions *hr, struct cDnaAlign *refAln, struct psl *mappedHap)
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
    if ((hr->hapRefCDnaFh != NULL) && (cDnaCDnaAln != NULL))
        cDnaAlignPslOut(cDnaCDnaAln, refAln->alnId, hr->hapRefCDnaFh);
    }
return cDnaCDnaAln;
}

static int getHapQRangePartContainedStart(unsigned hapTStart, struct psl *refPsl)
/* find starting position for hap region overlap.  hapTStart is in strand
 * coordinates.  */
{
int iBlk;
for (iBlk = 0; iBlk < refPsl->blockCount; iBlk++)
    {
    unsigned tEnd = refPsl->tStarts[iBlk]+refPsl->blockSizes[iBlk];
    if (tEnd > hapTStart)
        {
        if (refPsl->tStarts[iBlk] >= hapTStart)
            {
            /* before block */
            return refPsl->qStarts[iBlk];
            }
        else
            {
             /* in block */
            return refPsl->qStarts[iBlk] + (hapTStart-refPsl->tStarts[iBlk]);
            }
        }
    }
assert(FALSE);  // should never happen
return 0;
}

static int getHapQRangePartContainedEnd(unsigned hapTEnd, struct psl *refPsl)
/* find ending position for hap region overlap.  hapTEnd is in strand
 * coordinates.  */
{
int iBlk;
for (iBlk = refPsl->blockCount-1; iBlk >= 0; iBlk--)
    {
    unsigned tEnd = refPsl->tStarts[iBlk]+refPsl->blockSizes[iBlk];
    if (refPsl->tStarts[iBlk] <= hapTEnd)
        {
        if (tEnd <= hapTEnd)
            {
            /* after block */
            return refPsl->qStarts[iBlk] + refPsl->blockSizes[iBlk];
            }
        else
            {
             /* in block */
            return refPsl->qStarts[iBlk] + (hapTEnd-refPsl->tStarts[iBlk]);
            }
        }
    }
assert(FALSE);  // should never happen
return 0;
}

static struct range getHapQRangePartContained(struct hapChrom *hapChrom, struct psl *refPsl)
/* find the range of an mRNA that is aligned to a haplotype region of a ref chrom when
 * not completely contained in haplotype range */
{
struct range qRange = {0, 0};
unsigned hapTStart = hapChrom->refStart, hapTEnd = hapChrom->refEnd;
if (refPsl->strand[1] == '-')
    reverseUnsignedRange(&hapTStart, &hapTEnd, refPsl->tSize);
qRange.start = getHapQRangePartContainedStart(hapTStart, refPsl);
qRange.end = getHapQRangePartContainedEnd(hapTEnd, refPsl);
if (refPsl->strand[0] == '-')
    reverseIntRange(&qRange.start, &qRange.end, refPsl->qSize);
return qRange;
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
    qRange = getHapQRangePartContained(hapChrom, refPsl);
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

static void compareHapRefPair(struct hapRegions *hr, struct psl *mappedHap, struct cDnaAlign *refAln,
                              UBYTE *bases)
/* map cDNA to cDNA alignments and flagged mapped bases in base array */
{
struct psl *cDnaCDnaAln = mapCDnaCDnaAln(hr, refAln, mappedHap);
if (cDnaCDnaAln != NULL)
    {
    markMappedSameBases(cDnaCDnaAln, bases);
    pslFree(&cDnaCDnaAln);
    }
}

static float calcHapRefPairScore(struct hapRegions *hr, struct cDnaAlign *refAln, struct refChrom *refChrom, struct cDnaAlign *hapAln, struct hapChrom *hapChrom, struct psl *mappedHaps)
/* Score a hapAln with resulting list of mappings refAln based on mapping a mapping to the ref chrom and
 * then a mapping of the two. */
{
struct range qRange = getHapQRange(hapChrom, refAln);
struct psl *mappedHap;
int i, cnt = 0;
UBYTE *bases = clearBaseArray(hapAln);

for (mappedHap = mappedHaps; mappedHap != NULL; mappedHap = mappedHap->next)
    compareHapRefPair(hr, mappedHap, refAln, bases);
for (i = 0; i < refAln->psl->qSize; i++)
    {
    if (bases[i])
        cnt++;
    }
return ((float)cnt)/(float)(qRange.end-qRange.start);
}

static float scoreHapRefPair(struct hapRegions *hr, struct cDnaAlign *refAln, struct refChrom *refChrom, struct cDnaAlign *hapAln, struct hapChrom *hapChrom)
/* Score a hapAln with refAln based on mapping a mapping to the ref chrom and
 * then a mapping of the two.  An issue is that the mapping a given hapAln to
 * the reference chromosome might be fragmented into multiple alignments due
 * to multiple mapping alignments.  So we count the total number of
 * non-redundent bases that are aligned. */
{
float score = -1.0;
struct psl *mappedHaps = mapToRef(hr, refChrom, hapAln, hapChrom);
if (mappedHaps != NULL)
    {
    score = calcHapRefPairScore(hr, refAln, refChrom, hapAln, hapChrom, mappedHaps);
    pslFreeList(&mappedHaps);
    }
return score;
}


#if UNUSED
static boolean isHapRegionAln(struct hapRegions *hr, struct hapChrom *hapChrom, struct cDnaAlign *refAln)
/* test if aln overlaps the hap region for the specified hapChrom */
{
return alnInHapRegion(hr, refAln)
    && sameString(refAln->psl->tName, hapChrom->refChrom->chrom)
    && positiveRangeIntersection(refAln->psl->tStart, refAln->psl->tEnd, 
                                 hapChrom->refStart, hapChrom->refEnd);
}
#endif

static struct cDnaAlign *getBestHapMatch(struct hapRegions *hr, struct cDnaQuery *cdna, struct cDnaAlign *refAln, struct refChrom *refChrom, struct hapChrom *hapChrom)
/* get the best-matching haplotype for a reference region with haplotypes on a
 * given haplotype pseudo-chrom. */
{
struct cDnaAlign *bestHapAln = NULL, *hapAln;
float bestScore = 0.0;
// scan all aligns on this hapChrom that have not been linked to another alignment
for (hapAln = cdna->alns; hapAln != NULL; hapAln = hapAln->next)
    {
    if ((!hapAln->drop) && (hapAln->hapRefAln == NULL) && sameString(hapAln->psl->tName, hapChrom->chrom))
        {
        float score = scoreHapRefPair(hr, refAln, refChrom, hapAln, hapChrom);
        if ((bestHapAln == NULL) || (score > bestScore))
            {
            bestHapAln = hapAln;
            bestScore = score;
            }
        }
    }
return bestHapAln;
}

static void verboseLink(struct hapRegions *hr, struct cDnaAlign *refAln, struct cDnaAlign *hapAln)
/* verbose output on linking a refAln with a hapAln */
{
if (verboseLevel() >= 5)
    {
    verbose(5, "link refAln ");
    cDnaAlignVerbLoc(5, refAln);
    verbose(5, " to hapAln ");
    cDnaAlignVerbLoc(5, hapAln);
    verbose(5, "\n");
    }
}

static void linkRefToHapChromAln(struct hapRegions *hr, struct cDnaQuery *cdna, struct cDnaAlign *refAln, struct refChrom *refChrom,
                                 struct hapChrom *hapChrom)
/* link best scoring haplotype alignment from a hapChrom to a given reference
 * chrome alignment */
{
struct cDnaAlign *hapAln = getBestHapMatch(hr, cdna, refAln, refChrom, hapChrom);
if (hapAln != NULL)
    {
    slAddHead(&refAln->hapAlns, cDnaAlignRefNew(hapAln));
    hapAln->hapRefAln = refAln;
    verboseLink(hr, refAln, hapAln);
    }
}

static void linkRefToHaps(struct hapRegions *hr, struct cDnaQuery *cdna, struct cDnaAlign *refAln)
/* link best scoring haplotype alignments to a given reference chrome
 * alignment */
{
struct refChrom *refChrom = refChromGet(hr, refAln->psl->tName);
struct hapChrom *hapChrom;
for (hapChrom = refChrom->hapChroms; hapChrom != NULL; hapChrom = hapChrom->next)
    linkRefToHapChromAln(hr, cdna, refAln, refChrom, hapChrom);
}

static void linkRefsToHaps(struct hapRegions *hr, struct cDnaQuery *cdna)
/* link best scoring haplotype alignments to reference alignments */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && alnInHapRegion(hr, aln))
        linkRefToHaps(hr, cdna, aln);
    }
}

void hapRegionsLinkHaps(struct hapRegions *hr, struct cDnaQuery *cdna)
/* Link a haplotype chrom alignments to reference chrom alignments if
 * they can be mapped. */
{
linkRefsToHaps(hr, cdna);
hapRegionsBuildHapSets(cdna);
}

void hapRegionsBuildHapSets(struct cDnaQuery *cdna)
/* build links to all alignments that are not haplotypes linked to reference
 * forming haplotype sets.  These includes unlinked haplotypes.
 * this should also be called when there are no hapRegions alignments to build
 * default hapSets.
 */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    // reference and unlinked haps have NULL hapRefAln
    if (!aln->drop && (aln->hapRefAln == NULL))
        slAddHead(&cdna->hapSets, cDnaAlignRefNew(aln));
    }
}

