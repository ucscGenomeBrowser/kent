/* vcfBits.c/.h: Variant Call Format, analysis by bit maps.
 * The routines found here are dependent upon vcf.c/.h for accessing vcf records.
 * They allow analysis of a set of vcf records by bit maps with one bit map per variant
 * location and where each haplotype covered by the vcf record is represented by a single
 * bit (or pair of bits). Additional analysis can be performed by creating haplotype based
 * bit maps from variant bit maps.  There is one haplotype bit map for each haplotype
 * (subject chromosome) with one (or two) bits for each variant location in the set of records. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dnautil.h"
#include "errAbort.h"
#include <limits.h>
#include "localmem.h"
#include "net.h"
#include "regexHelper.h"
#include "vcf.h"
#include "vcfBits.h"


static struct variantBits *vcfOneRecordToVariantBits(struct vcfFile *vcff,struct vcfRecord *record,
                                                     boolean phasedOnly, boolean homozygousOnly)
// Returns bit array covering all genotypes/haplotype/alleles per record.  One slot per genotype
// containing 2 slots for haplotypes and 1 or 2 bits per allele. The normal (simple) case of
// 1 reference and 1 alternate allele results in 1 allele bit with 0:ref. Two or three alt alleles
// is represented by two bits per allele (>3 alternate alleles is unsupported).
// If phasedOnly, unphased haplotype bits will all be set only if all agree (00 is uninterpretable)
// Haploid genotypes (e.g. chrY) and homozygousOnly bitmaps contain 1 haplotype slot.
{
// parse genotypes if needed
if (record->genotypes == NULL)
    vcfParseGenotypes(record);
assert(vcff->genotypeCount > 0);
int ix = 0;

// allocate vBits struct
struct variantBits *vBits;
struct lm *lm = vcfFileLm(vcff);
lmAllocVar(lm,vBits);
vBits->genotypeSlots  = vcff->genotypeCount;
vBits->record         = record;
vBits->haplotypeSlots = 2;  // assume diploid, but then check for complete haploid
if (homozygousOnly)
    vBits->haplotypeSlots = 1;
else
    { // spin through all the subjects to see if all are haploid
    for (ix = 0; ix < vcff->genotypeCount && record->genotypes[ix].isHaploid;ix++)
        ;
    if (ix == vcff->genotypeCount)
        vBits->haplotypeSlots = 1;  // All are haploid: chrY
    assert(vBits->haplotypeSlots == 2 || record->chrom[strlen(record->chrom) - 1] == 'Y');
    }

// Type of variant?
assert(record->alleles != NULL);
if(record->alleles != NULL)
    {
    int nonRefLen = 0;
    assert(record->alleles[0] != NULL);
    int refLen = record->alleles[0] ? strlen(record->alleles[0]) : 0;
    for (ix = 1; ix < record->alleleCount; ix++)
        {
        assert(record->alleles[ix] != NULL);
        int thisLen = record->alleles[ix] && differentWord(record->alleles[ix],"<DEL>") ?
                                                                  strlen(record->alleles[ix]) : 0;
        if (nonRefLen < thisLen)
            nonRefLen = thisLen;
        }
    if (refLen == 1 && nonRefLen == 1)
        vBits->vType = vtSNP;
    else if (refLen < nonRefLen)
        vBits->vType = vtInsertion;
    else if (refLen > nonRefLen)
        vBits->vType = vtDeletion;
    // else unknown
    }

// allocate bits
assert(record->alleleCount < 4);
vBits->alleleSlots = (record->alleleCount <= 2 ? 1 : 2);
int bitCount = vBitsSlotCount(vBits);
Bits *bits = lmBitAlloc(lm,bitCount);
vBits->bits = bits;


// walk through genotypes and set the bit
for (ix = 0; ix < vcff->genotypeCount;ix++)
    {
    struct vcfGenotype *gt = gt = &(record->genotypes[ix]);

    boolean homozygous = (gt->isHaploid || gt->hapIxA == gt->hapIxB);

    if ((!phasedOnly || gt->isPhased || homozygous))
        {
        if ((!homozygousOnly || homozygous) && gt->hapIxA > 0)
            {
            switch (gt->hapIxA)
                {
                case 1: bitSetOne(bits,  vBitsSlot(vBits,ix,0,0));   vBits->bitsOn++;    break;
                case 2: bitSetOne(bits,  vBitsSlot(vBits,ix,0,1));   vBits->bitsOn++;    break;
                case 3: bitSetRange(bits,vBitsSlot(vBits,ix,0,0),2); vBits->bitsOn += 2; break;
                default:                                             break;
                }
            }
        if (!gt->isHaploid && !homozygousOnly && gt->hapIxB > 0)
            {
            switch (gt->hapIxB)
                {
                case 1: bitSetOne(bits,  vBitsSlot(vBits,ix,1,0));   vBits->bitsOn++;    break;
                case 2: bitSetOne(bits,  vBitsSlot(vBits,ix,1,1));   vBits->bitsOn++;    break;
                case 3: bitSetRange(bits,vBitsSlot(vBits,ix,1,0),2); vBits->bitsOn += 2; break;
                default:                                             break;
                }
            }
        }
    }

return vBits;
}

static Bits *vcfRecordUnphasedBits(struct vcfFile *vcff, struct vcfRecord *record)
// Returns array (1 bit per genotype) for each unphased genotype.
{
// allocate bits
Bits *bits = lmBitAlloc(vcfFileLm(vcff),vcff->genotypeCount);

// parse genotypes if needed
if (record->genotypes == NULL)
    vcfParseGenotypes(record);

// walk through genotypes and set the bit
int ix = 0;
for ( ; ix < vcff->genotypeCount;ix++)
    {
    if (record->genotypes[ix].isPhased == FALSE
    && record->genotypes[ix].isHaploid == FALSE)
        bitSetOne(bits,ix);
    }
return bits;
}

Bits *vcfRecordHaploidBits(struct vcfFile *vcff, struct vcfRecord *record)
// Returns array (1 bit per genotype) for each haploid genotype.
// This is useful for interpreting chrX.
{
// allocate bits
Bits *bits = lmBitAlloc(vcfFileLm(vcff),vcff->genotypeCount);

// parse genotypes if needed
if (record->genotypes == NULL)
    vcfParseGenotypes(record);

// walk through genotypes and set the bit
int ix = 0;
for ( ; ix < vcff->genotypeCount;ix++)
    {
    if (record->genotypes[ix].isHaploid)
        bitSetOne(bits,ix);
    }
return bits;
}

int vBitsHaploidCount(struct variantBits *vBits)
// Returns the number of subjects in the VCF dataset that are haploid at this location
{
int haploid = 0;  // Most are not haploid

char *chrom = vBits->record->chrom;
if (chrom[strlen(chrom) - 1] == 'Y')
    haploid = vcfBitsSubjectCount(vBits);
else if (chrom[strlen(chrom) - 1] == 'X')  // chrX is tricky: males are haploid except in PAR
    {
    struct vcfRecord *record = vBits->record;
    struct vcfFile *vcff = record->file;

    // parse genotypes if needed
    if (record->genotypes == NULL)
        vcfParseGenotypes(record);

    // walk through genotypes
    int ix = 0;
    for ( ; ix < vcff->genotypeCount;ix++)
        {
        if (record->genotypes[ix].isHaploid)
            haploid++;
        }
    }
return haploid;
}

int vBitsSubjectChromCount(struct variantBits *vBits)
// Returns the chromosomes in the VCF dataset that are covered at this location
{
int haploid = vBitsHaploidCount(vBits);
int diploid = vcfBitsSubjectCount(vBits) - haploid;
return ((diploid * 2) + haploid);
}

struct variantBits *vcfRecordsToVariantBits(struct vcfFile *vcff, struct vcfRecord *records,
                                            boolean phasedOnly, boolean homozygousOnly,
                                            boolean unphasedBits)
// Returns list of bit arrays covering all genotypes/haplotype/alleles per record for each record
// provided.  If records is NULL will use vcff->records. Bit map has one slot per genotype
// containing 2 slots for haplotypes and 1 or 2 bits per allele. The normal (simple) case of
// 1 reference and 1 alternate allele results in 1 allele bit with 0:ref. Two or three alt alleles
// is represented by two bits per allele (>3 non-reference alleles unsupported).
// If phasedOnly, unphased haplotype bits will be set only if both agree (00 is uninterpretable)
// Haploid genotypes (e.g. chrY) and homozygousOnly bitmaps contain 1 haplotype slot.
// If unphasedBits, then vBits->unphased will contain a bitmap with 1s for all unphased genotypes.
// NOTE: allocated from vcff pool, so closing file or flushing reusePool will invalidate this.
{
struct variantBits *vBitsList = NULL;

struct vcfRecord *record = records ? records : vcff->records;
for (;record != NULL; record = record->next)
    {
    struct variantBits *vBits = vcfOneRecordToVariantBits(vcff, record, phasedOnly, homozygousOnly);
    if (unphasedBits)
        vBits->unphased = vcfRecordUnphasedBits(vcff, record);
    slAddHead(&vBitsList,vBits);
    }
slReverse(&vBitsList);

return vBitsList;
}

int vcfVariantBitsDropSparse(struct variantBits **vBitsList, int haploGenomeMin,
                             boolean dropRefMissing)
// Drops vBits found in less than a minimum number of haplotype genomes.  Supplying 1 will
// drop variants found in no haplotype genomes.  Declaring dropRefMissing will drop variants
// in all haplotype genomes (variants where reference is not represented in dataset and
// *might* be in error). Returns count of vBits structure that were dropped.
{
struct variantBits *vBitsKeep = NULL;
struct variantBits *vBits = NULL;
int dropped = 0;
while ((vBits = slPopHead(vBitsList)) != NULL)
    {
    if (vBits->bitsOn >= haploGenomeMin)
        slAddHead(&vBitsKeep,vBits);
    else
        dropped++;  // if not demoted, drop memory, since this is on lm
    }
*vBitsList = vBitsKeep;

if (dropRefMissing)
    {
    vBitsKeep = NULL;
    while ((vBits = slPopHead(vBitsList)) != NULL)
        {
        boolean foundRef = FALSE;
        int genoIx = 0;
        for (; !foundRef && genoIx < vBits->genotypeSlots; genoIx++)
            {
            int haploIx = 0;
            for (; !foundRef && haploIx < vBits->haplotypeSlots; haploIx++)
                {
                if (bitCountRange(vBits->bits,
                                  vBitsSlot(vBits,genoIx,haploIx,0), vBits->alleleSlots) == 0)
                    foundRef = TRUE;
                }
            }

        if (foundRef)
            slAddHead(&vBitsKeep,vBits);
        else  // drop memory, since this is on lm
            dropped++;  // if not demoted, drop memory, since this is on lm
        }
    *vBitsList = vBitsKeep;
    }
else
    slReverse(vBitsList);

return dropped;
}

int vcfVariantBitsAlleleOccurs(struct variantBits *vBits, unsigned char alleleIx,
                               boolean homozygousOrHaploid)
// Counts the number of times a particular allele occurs in the subjects*chroms covered.
// If homozygousOrHaploid then the allele must occur in both chroms to be counted
{
assert((vBits->alleleSlots == 1 && alleleIx <= 1)
    || (vBits->alleleSlots == 2 && alleleIx <= 3));

// To count ref, we need to know the number of subjects the bitmap covers
// chrX is the culprit here since it is haploid/diploid
int subjTotal = vBits->genotypeSlots * vBits->haplotypeSlots;
if (alleleIx == 0)
    subjTotal = vBitsSubjectChromCount(vBits);

// Simple case can be taken care of easily
if (vBits->alleleSlots == 1
&&  (!homozygousOrHaploid || vBits->haplotypeSlots == 1))
    {
    if (alleleIx == 1)           // non-reference
        return vBits->bitsOn;
    else                         // reference
        return subjTotal - vBits->bitsOn;
    }

// Otherwise, we have to walk the bitmap
int occurs           = 0;
int occursHomozygous = 0;
int genoIx = 0;
for (; genoIx < vBits->genotypeSlots; genoIx++)
    {
    int haploIx = 0;
    int occursThisSubject = 0;
    for (; haploIx < vBits->haplotypeSlots; haploIx++)
        {
        int bitSlot = vBitsSlot(vBits,genoIx,haploIx,0);
        switch (alleleIx)
            {
            case 0: if (bitCountRange(vBits->bits,bitSlot, vBits->alleleSlots) != 0)
                        occursThisSubject++; // Any non-reference bit
                    break;
            case 1: if (vBits->alleleSlots == 1)
                        {
                        if (bitReadOne(vBits->bits,bitSlot))
                            occursThisSubject++;
                        }
                    else
                        {
                        if ( bitReadOne(vBits->bits,bitSlot)
                        &&  !bitReadOne(vBits->bits,bitSlot + 1) )
                            occursThisSubject++;
                        }
                    break;
            case 2: if (!bitReadOne(vBits->bits,bitSlot)
                    &&   bitReadOne(vBits->bits,bitSlot + 1) )
                        occursThisSubject++;
                    break;
            case 3: if (bitCountRange(vBits->bits,bitSlot, 2) == 2)
                        occursThisSubject++;
                    break;
            default:
                    break;
            }
        }
    occurs += occursThisSubject;
    if ((alleleIx == 0 && occursThisSubject == 0)
    ||  (alleleIx >  0 && vBits->haplotypeSlots == occursThisSubject))
        occursHomozygous++;
    }
if (alleleIx == 0)
    occurs = subjTotal - occurs;

return (homozygousOrHaploid ? occursHomozygous : occurs);
}

int vcfVariantBitsReferenceOccurs(struct vcfFile *vcff, struct variantBits *vBitsList,
                                  boolean homozygousOrHaploid)
// For an entire list of vBits structs, counts the times the reference allele occurs.
// If homozygousOrHaploid then the reference allele must occur in both chroms to be counted
{
assert(vBitsList != NULL);
struct variantBits *vBits = vBitsList;
// Allocate temporary vBits struct which can hold the 'OR' version of all bitmaps in list
struct lm *lm = vcfFileLm(vcff);
struct variantBits *vBitsTemp;
lmAllocVar(lm,vBitsTemp);
vBitsTemp->genotypeSlots  = vBits->genotypeSlots;
vBitsTemp->haplotypeSlots = vBits->haplotypeSlots;
vBitsTemp->alleleSlots    = vBits->alleleSlots;
vBitsTemp->record         = vBits->record;

int bitSlots = vBitsSlotCount(vBits);
vBitsTemp->bits = lmBitClone(lm,vBits->bits, bitSlots);


for (vBits = vBits->next; vBits != NULL; vBits = vBits->next)
    bitOr(vBitsTemp->bits, vBits->bits, bitSlots);

vBitsTemp->bitsOn = bitCountRange(vBitsTemp->bits,0,bitSlots);

// Note the memory will be leaked but it is lm
return vcfVariantBitsAlleleOccurs(vBitsTemp, 0, homozygousOrHaploid);
}

int vcfVariantMostPopularCmp(const void *va, const void *vb)
// Compare to sort variantBits based upon how many genomes/chrom has the variant
// This can be used to build haploBits in most populous order for tree building
{
const struct variantBits *a = *((struct variantBits **)va);
const struct variantBits *b = *((struct variantBits **)vb);
//int ret = memcmp(a->bits, b->bits, bitToByteSize(vBitsSlotCount(a)));
int ret = (b->bitsOn - a->bitsOn); // higher bits first (descending order)
if (ret == 0)
    { // sort deterministically
    ret = (a->record->chromStart - b->record->chromStart);
    if (ret == 0)
        ret = (a->record->chromEnd - b->record->chromEnd);
    }
return ret;
}

struct haploBits *vcfVariantBitsToHaploBits(struct vcfFile *vcff, struct variantBits *vBitsList,
                                            boolean ignoreReference)
// Converts a set of variant bits to haplotype bits, resulting in one bit struct
// per haplotype genome that has non-reference variations.  If ignoreReference, only
// haplotype genomes with at least one non-reference variant are returned.
// A haploBit array has one variant slot per vBit struct and one or more bits per allele.
// NOTE: allocated from vcff pool, so closing file or flushing reusePool will invalidate this.
{
// First determine dimensions
int variantSlots = 0;
unsigned char alleleSlots  = 1;
struct variantBits *vBits = vBitsList;
for ( ; vBits != NULL; vBits = vBits->next)
    {
    variantSlots++;
    if (vBits->alleleSlots == 2)
        alleleSlots = 2; // Will normalize allele slots across all variants.
    // This should catch any problems where chrX is haploid/diploid
    assert(vBitsList->haplotypeSlots == vBits->haplotypeSlots);
    }


// Now create a struct per haplotype
struct lm *lm = vcfFileLm(vcff);
struct haploBits *hBitsList = NULL;
int genoIx = 0;
for (; genoIx < vBitsList->genotypeSlots; genoIx++)
    {
    int haploIx = 0;
    for (; haploIx < vBitsList->haplotypeSlots; haploIx++)
        {
        // allocate struct
        struct haploBits *hBits;
        lmAllocVar(lm,hBits);
        hBits->variantSlots = variantSlots;
        hBits->alleleSlots  = alleleSlots;
        hBits->haploGenomes = 1;
        int bitCount = hBitsSlotCount(hBits);

        // allocate bits
        Bits *bits = lmBitAlloc(lm,bitCount);
        hBits->bits = bits;

        int variantIx = 0;
        for (vBits =  vBitsList; vBits != NULL; vBits = vBits->next, variantIx++)
            {
            if (bitReadOne(vBits->bits,vBitsSlot(vBits,genoIx,haploIx,0)))
                {
                bitSetOne(        bits,hBitsSlot(hBits,variantIx,     0));
                hBits->bitsOn++;
                }

            if (vBits->alleleSlots == 2) // (hBit->alleleSlot will also == 2)
                {
                if (bitReadOne(vBits->bits,vBitsSlot(vBits,genoIx,haploIx,1)))
                    {
                    bitSetOne(        bits,hBitsSlot(hBits,variantIx,     1));
                    hBits->bitsOn++;
                    }
                }
            }

        // gonna keep it? so flesh it out (ignoreRef means > 0 && < all)
        if (!ignoreReference || hBits->bitsOn)
            {
            hBits->genomeIx  = genoIx;
            hBits->haploidIx = haploIx;
            struct vcfRecord *record = vBitsList->record; // any will do
            struct vcfGenotype *gt = &(record->genotypes[genoIx]);

            if (hBits->bitsOn                   // if including reference, then chrX could
            ||  haploIx == 0 || !gt->isHaploid) // have unused diploid positions!
                {
                if (gt->isHaploid || vBitsList->haplotypeSlots == 1)
                    { // chrX will have haplotypeSlots==2 but be haploid for this subject.
                      // Meanwhile if vBits were for homozygous only,  haplotypeSlots==1
                    //assert(haploIx == 0);
                    hBits->ids = lmCloneString(lm,gt->id);
                    }
                else
                    {
                    int sz = strlen(gt->id) + 3;
                    hBits->ids = lmAlloc(lm,sz);
                    safef(hBits->ids,sz,"%s-%c",gt->id,'a' + haploIx);
                    }
                slAddHead(&hBitsList,hBits);
                }
            }
        //else
        //    haploBitsFree(&hBits); // lm so just abandon
        }
    }
slReverse(&hBitsList);

return hBitsList;
}

/*
int vcfHaploBitsOnCmp(const void *va, const void *vb)
// Compare to sort haploBits based upon how many bits are on
{
const struct haploBits *a = *((struct haploBits **)va);
const struct haploBits *b = *((struct haploBits **)vb);
int ret = (a->bitsOn - b->bitsOn);
if (ret == 0)
    { // sort deterministically
    ret = (a->genomeIx - b->genomeIx);
    if (ret == 0)
        ret = (a->haploidIx - b->haploidIx);
    }
return ret;
}
*/

int vcfHaploBitsMemCmp(const void *va, const void *vb)
// Compare to sort haploBits based on bit by bit memory compare
// When hBits has been created from vBits sorted by popularity,
// a mem sort will then allow elmTreeGrow() to add popular variants first.
{
const struct haploBits *a = *((struct haploBits **)va);
const struct haploBits *b = *((struct haploBits **)vb);
int ret = memcmp(a->bits, b->bits, bitToByteSize(hBitsSlotCount(a)));
if (ret == 0)
    { // sort deterministically
    ret = (a->genomeIx - b->genomeIx);
    if (ret == 0)
        ret = (a->haploidIx - b->haploidIx);
    }
return ret;
}

static struct haploBits *hBitsCollapsePair(struct lm *lm, struct haploBits *hBits,
                                           struct haploBits *dupMap)
// Adds Ids of duplicate bitmap to hBits and abandons the duplicate.
{
char *ids = lmAlloc(lm,strlen(dupMap->ids) + strlen(hBits->ids) + 2);
// NOTE: Putting Dup Ids first could leave these in lowest genomeIx/haploidIx order
sprintf(ids,"%s,%s",dupMap->ids,hBits->ids);
hBits->ids = ids;
hBits->haploGenomes++;
hBits->genomeIx  = dupMap->genomeIx;  // Arbitrary but could leave lowest first
hBits->haploidIx = dupMap->haploidIx;
return hBits;
}

int vcfHaploBitsListCollapseIdentical(struct vcfFile *vcff, struct haploBits **phBitsList,
                                      int haploGenomeMin)
// Collapses a list of haploBits based upon identical bit arrays.
// If haploGenomeMin > 1, will drop all hBits structs covering less than N haploGenomes.
// Returns count of hBit structs removed.
{
struct lm *lm = vcfFileLm(vcff);
int collapsed = 0;
struct haploBits *hBitsPassed = NULL;
struct haploBits *hBitsToTest = *phBitsList;
if (hBitsToTest->next == NULL)
    return 0;

// Sort early bits first
slSort(&hBitsToTest, vcfHaploBitsMemCmp);

// Walk through those of matching size and compare
int bitCount = hBitsSlotCount(hBitsToTest);
int memSize = bitToByteSize(bitCount);
struct haploBits *hBitsQ = NULL;
while ((hBitsQ = slPopHead(&hBitsToTest)) != NULL)
    {
    struct haploBits *hBitsT = hBitsToTest; //hBitsQ->next; is already null.
    for (;hBitsT != NULL;hBitsT = hBitsT->next)
        {
        // If the 2 bitmaps do not have the same number of bits on, they are not identical
        if (hBitsT->bitsOn != hBitsQ->bitsOn)
            break;

        // If 2 bitmaps are identical, then collapse into one
        if (memcmp(hBitsT->bits,hBitsQ->bits,memSize) == 0)
            {     // collapse
            hBitsCollapsePair(lm,hBitsT,hBitsQ);
            hBitsQ = NULL; // abandon lm memory
            collapsed++;
            break;  // hBitsQ used up so make hBitsT the new Q.
            }
        }

    // If hBitsQ survived the collapsing, then don't loose it.
    if (hBitsQ != NULL)
        slAddHead(&hBitsPassed,hBitsQ);
    }

// Now that we have collapsed hBits, we can drop any that are not found in enough subjects
if (haploGenomeMin > 1)
    {
    hBitsToTest = hBitsPassed;
    hBitsPassed = NULL;
    while ((hBitsQ = slPopHead(hBitsToTest)) != NULL)
        {
        if (hBitsQ->haploGenomes >= haploGenomeMin)
            slAddHead(&hBitsPassed,hBitsQ);
        else  // drop memory, since this is on lm
            collapsed++;
        }
    // double pop passes means no reverse required.
    }
else
    slReverse(&hBitsPassed);

*phBitsList = hBitsPassed;

return collapsed;
}

unsigned char vcfHaploBitsToVariantAlleleIx(struct haploBits *hBits,int bitIx)
// Given a hBits struct and bitIx, what is the actual variant allele ix
// to use when accessing the vcfRecord?  NOTE: here 0 is reference allele
{
if (hBits->alleleSlots == 1)
    return bitReadOne(hBits->bits,bitIx); // 0 or 1
else
    {
    assert(hBits->alleleSlots == 2); // Only supports 1-3 variants encoded as 2 bits
    unsigned char varIx = 0;
    int bit = variantSlotFromBitIx(hBits,bitIx);
    if (bitReadOne(hBits->bits,bit++))
        varIx = 1;  // Note that bitmaps represent 1,2,3 as 10,01,11
    if (bitReadOne(hBits->bits,bit))
        varIx += 2;
    return varIx;
    }
}

enum elmNodeOverlap vcfHaploBitsCmp(const struct slList *elA, const struct slList *elB,
                                    int *matchWeight, void *extra)
// HaploBits compare routine for building tree of relations using elmTreeGrow().
{
const struct haploBits *a = (struct haploBits *)elA;
const struct haploBits *b = (struct haploBits *)elB;

int bitCount = hBitsSlotCount(a);
int and = bitAndCount(a->bits,b->bits,bitCount);
if (matchWeight)
    *matchWeight = and;
if (and == 0)// && a->bitsOn > 0)
    return enoExcluding; // nothing in common
if (a->bitsOn == and)
    {
    if (b->bitsOn == and)
        return enoEqual; // perfect match
    else
        return enoSubset;
    }
// super and mixed?
if (b->bitsOn == and)
    return enoSuperset;
return enoMixed;
}

struct slList *vcfHaploBitsMatching(const struct slList *elA, const struct slList *elB,
                                    void *extra)
// Returns a HaploBits structure representing the common parts of elements A and B.
// Used with elmTreeGrow() to create nodes that are the common parts between leaves/branches.
{
struct haploBits *a = (struct haploBits *)elA;
struct haploBits *b = (struct haploBits *)elB;
struct lm *lm = extra;

// If the 2 bitmaps are equal, then collapse into a single struct
if (enoEqual == vcfHaploBitsCmp(elA,elB,NULL,extra))
    {
    if (hBitsIsReal(a)) // bitmap represents variants on at least one subject chromosome
        {
        assert(!hBitsIsReal(b));  // Won't always work but hBits have been precollapsed
        if (hBitsIsReal(b)) // Should not be if these have already been collapsed.
            {
            hBitsCollapsePair(lm,a,b);    // a is real, b is real so collapse into one
            //haploBitsFreeList(&b);   // lm so just abandon
            return (struct slList *)a;
            }
        else
            return (struct slList *)a; // a is real, b is not so just use a
        }
    else if (hBitsIsReal(b))
        return (struct slList *)b;    // b is real, a is not so just use b
    }

// Must make non "real" bitmap which is the common bits of a and b
struct haploBits *match;
lmAllocVar(lm,match);
match->alleleSlots  = a->alleleSlots;
match->variantSlots = a->variantSlots;
match->haploGenomes = 0;  // Marking this as a subset

int bitCount  = hBitsSlotCount(match);
match->bits = lmBitClone(lm,a->bits, bitCount);
bitAnd(match->bits, b->bits, bitCount);
match->bitsOn = bitCountRange(match->bits, 0, bitCount);

return (struct slList *)match;
}
