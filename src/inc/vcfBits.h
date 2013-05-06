/* vcfBits.c/.h: Variant Call Format, analysis by bit maps.
 * The routines found here are dependent upon vcf.c/.h for accessing vcf records.
 * They allow analysis of a set of vcf records by bit maps with one bit map per variant
 * location and where each haplotype covered by the vcf record is represented by a single
 * bit (or pair of bits). Additional analysis can be performed by creating haplotype based
 * bit maps from variant bit maps.  There is one haplotype bit map for each haplotype
 * (subject chromosome) with one (or two) bits for each variant location in the set of records. */

#ifndef vcfBits_h
#define vcfBits_h

#include "vcf.h"
#include "bits.h"
#include "elmTree.h"

enum variantType
/* VCF header defines INFO column components; each component has one of these types: */
    {
    vtNoType    = 0,   // uninitialized value (0) or unrecognized type name
    vtSNP       = 1,   // Single Nucleotide polymorphism (most)
    vtInsertion = 2,   // Insertion relative to reference genome
    vtDeletion  = 3,   // Insertion relative to reference genome
    };

struct variantBits
// all genotypes/haplotypes/alleles for one record are converted to a bit map
// One struct per variant record in vcff->records.  One slot per genotype containing
// 2 slots for haplotypes and then 1 or 2 bits per allele.
    {
    struct variantBits *next;
    struct vcfRecord *record;     // keep track of record for later interpretation
    enum variantType vType;       // Might as well know this up front
    int genotypeSlots;            // subjects covered in vcf file
    unsigned char haplotypeSlots; // 2 unless haploid or homozygous only
    unsigned char alleleSlots;    // 1 for 1 alt allele, 2 for 2 or 3 alt alleles >3 unsupported
    int bitsOn;                   // count of bits on.
    Bits *bits;                   // allele bits genotype x haplotype x allele
    Bits *unphased;               // unphased bits (1 bit per genotype) if requested, else NULL
    void **variants;              // special purposes array of variants filled and used by caller
    };

#define genoIxFromGenoHapIx(vBits,genoHaploIx)  (genoHaploIx / vBits->haplotypeSlots)
#define hapIxFromGenoHapIx(vBits,genoHaploIx)   (genoHaploIx % vBits->haplotypeSlots)
#define genoHapIx(vBits,genoIx,hapIx)          ((genoIx * vBits->haplotypeSlots) + hapIx)
#define vBitsSlot(vBits,genoIx,hapIx,variantIx) \
        ( (genoHapIx(vBits,genoIx,hapIx) * vBits->alleleSlots) + variantIx)
#define vBitsSlotCount(vBits) \
        ((vBits)->genotypeSlots * (vBits)->haplotypeSlots * (vBits)->alleleSlots)
#define vcfBitsSubjectCount(vBits) ((vBits)->genotypeSlots)

struct variantBits *vcfRecordsToVariantBits(struct vcfFile *vcff, struct vcfRecord *records,
                                            boolean phasedOnly, boolean homozygousOnly,
                                            boolean unphasedBits);
// Returns list of bit arrays covering all genotypes/haplotype/alleles per record for each record
// provided.  If records is NULL will use vcff->records. Bit map has one slot per genotype
// containing 2 slots for haplotypes and 1 or 2 bits per allele. The normal (simple) case of
// 1 reference and 1 alternate allele results in 1 allele bit with 0:ref. Two or three alt alleles
// is represented by two bits per allele (>3 non-reference alleles unsupported).
// If phasedOnly, unphased haplotype bits will be set only if both agree (00 is uninterpretable)
// Haploid genotypes (e.g. chrY) and homozygousOnly bitmaps contain 1 haplotype slot.
// If unphasedBits, then vBits->unphased will contain a bitmap with 1s for all unphased genotypes.
// NOTE: allocated from vcff pool, so closing file or flushing reusePool will invalidate this.

Bits *vcfRecordHaploidBits(struct vcfFile *vcff, struct vcfRecord *record);
// Returns array (1 bit per genotype) for each haploid genotype.
// This is useful for interpreting chrX.

int vBitsHaploidCount(struct variantBits *vBits);
// Returns the number of subjects in the VCF dataset that are haploid at this location

//#define vBitsSubjectChromCount(vBits) ((vBits)->genotypeSlots * (vBits)->haplotypeSlots)
int vBitsSubjectChromCount(struct variantBits *vBits);
// Returns the chromosomes in the VCF dataset that are covered at this location

int vcfVariantBitsDropSparse(struct variantBits **vBitsList, int haploGenomeMin,
                             boolean dropRefMissing);
// Drops vBits found in less than a minimum number of haplotype genomes.  Supplying 1 will
// drop variants found in no haplotype genomes.  Declaring dropRefMissing will drop variants
// in all haplotype genomes (variants where reference is not represented in dataset and
// *might* be in error). Returns count of vBits structure that were dropped.

int vcfVariantBitsAlleleOccurs(struct variantBits *vBits, unsigned char alleleIx,
                               boolean homozygousOrHaploid);
// Counts the number of times a particular allele occurs in the subjects*chroms covered.
// If homozygousOrHaploid then the allele must occur in both chroms to be counted

int vcfVariantBitsReferenceOccurs(struct vcfFile *vcff, struct variantBits *vBitsList,
                                  boolean homozygousOrHaploid);
// For an entire list of vBits structs, counts the times the reference allele occurs.
// If homozygousOrHaploid then the reference allele must occur in both chroms to be counted


int vcfVariantMostPopularCmp(const void *va, const void *vb);
// Compare to sort variantBits based upon how many genomes/chrom has the variant
// This can be used to build haploBits in most populous order for tree building

struct haploBits
// all variants/haplotypes/genotypes for a set of records are converted to a bit map
// One struct per haplotype genome covering vcff->records.  One slot per variant
// and 1 or 2 bits per allele.  NOTE: variant slots will all be normalized to max.
    {
    struct haploBits *next;
    char *ids;                 // comma separated lists of genotype names and haplotypes
    int haploGenomes;          // count of haploid genomes this structure covers
    int genomeIx;              // genome sample index (allows later lookups)
    unsigned char haploidIx;   // haploid index [0,1] (allows later  lookups)
    int variantSlots;          // count of variants covered in set of vcf records
    unsigned char alleleSlots; // 1 for 1 alt allele, 2 for 2 or 3 alt alleles >3 unsupported
    int bitsOn;                // count of bits on.
    Bits *bits;                // allele bits variant x allele
    };

#define vcfRecordIxFromBitIx(hBits,bitIx)  (bitIx / hBits->alleleSlots)
#define variantSlotFromBitIx(hBits,bitIx)  (vcfRecordIxFromBitIx(hBits,bitIx) * hBits->alleleSlots)
#define variantNextFromBitIx(hBits,bitIx)  (variantSlotFromBitIx(hBits,bitIx) + hBits->alleleSlots)
#define hBitsSlot(hBits,variantIx,alleleIx) ((hBits->alleleSlots * variantIx) + alleleIx)
#define hBitsSlotCount(hBits) ((hBits)->variantSlots * (hBits)->alleleSlots)

// An hBits struct is "Real" if it is generated from variants.  It may also be a subset.
#define hBitsIsSubset(hBits)    ((hBits)->haploGenomes == 0)
#define hBitsIsReal(hBits)      ((hBits)->haploGenomes >  0)

struct haploBits *vcfVariantBitsToHaploBits(struct vcfFile *vcff, struct variantBits *vBitsList,
                                            boolean ignoreReference);
// Converts a set of variant bits to haplotype bits, resulting in one bit struct
// per haplotype genome that has non-reference variations.  If ignoreReference, only
// haplotype genomes with at lone non-reference variant are returned.
// A haploBit array has one variant slot per vBit struct and one or more bits per allele.
// NOTE: allocated from vcff pool, so closing file or flushing reusePool will invalidate this.

int vcfHaploBitsListCollapseIdentical(struct vcfFile *vcff, struct haploBits **phBitsList,
                                      int haploGenomeMin);
// Collapses a list of haploBits based upon identical bit arrays.
// If haploGenomeMin > 1, will drop all hBits structs covering less than N haploGenomes.
// Returns count of hBit structs removed.

INLINE struct variantBits *vcfHaploBitIxToVariantBits(struct haploBits *hBits, int bitIx,
                                                      struct variantBits *vBitsList)
// Returns appropriate vBits from vBits list associated with a given bit in an hBits struct.
// Assumes vBitsList is in same order as hBits bit array.  Note vBits->record has full vcf details.
{
return slElementFromIx(vBitsList,vcfRecordIxFromBitIx(hBits,bitIx));
}

unsigned char vcfHaploBitsToVariantAlleleIx(struct haploBits *hBits,int bitIx);
// Given a hBits struct and bitIx, what is the actual variant allele ix
// to use when accessing the vcfRecord?

enum elmNodeOverlap vcfHaploBitsCmp(const struct slList *elA, const struct slList *elB,
                                    int *matchWeight, void *extra);
// HaploBits compare routine for building tree of relations using elmTreeGrow().

struct slList *vcfHaploBitsMatching(const struct slList *elA, const struct slList *elB,
                                    void *extra);
// Returns a HaploBits structure representing the common parts of elements A and B.
// Used with elmTreeGrow() to create nodes that are the common parts between leaves/branches.

#endif // vcfBits_h
