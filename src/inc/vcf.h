/* VCF: Variant Call Format, version 4.0 / 4.1
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-40
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-41
 * The vcfFile object borrows many memory handling and error reporting tricks from MarkD's
 * gff3File; any local deficiencies are not to reflect poorly on Mark's fine work! :) */

#ifndef vcf_h
#define vcf_h

#include "limits.h"
#include "hash.h"
#include "linefile.h"
#include "asParse.h"
#include "bits.h"
#include "elmTree.h"

enum vcfInfoType
/* VCF header defines INFO column components; each component has one of these types: */
    {
    vcfInfoNoType,	// uninitialized value (0) or unrecognized type name
    vcfInfoInteger,
    vcfInfoFloat,
    vcfInfoFlag,
    vcfInfoCharacter,
    vcfInfoString,
    };

union vcfDatum
/* Container for a value whose type is specified by an enum vcfInfoType. */
    {
    int datInt;
    double datFloat;
    boolean datFlag;
    char datChar;
    char *datString;
    };

struct vcfInfoDef
/* Definition of INFO column component from VCF header: */
    {
    struct vcfInfoDef *next;
    char *key;			// A short identifier, e.g. MQ for mapping quality
    int fieldCount;		// The number of values to follow the id, or -1 if it varies
    enum vcfInfoType type;	// The type of values that follow the id
    char *description;		// Brief description of info
    };

struct vcfInfoElement
/* A single INFO column component; each row's INFO column may contain multiple components. */
    {
    char *key;			// An identifier described by a struct vcfInfoDef
    int count;			// Number of data values following id
    union vcfDatum *values;	// Array of data values following id
    bool *missingData;		// Array of flags for missing data values ("." instead of number)
    };

struct vcfGenotype
/* A single component of the optional GENOTYPE column. */
    {
    char *id;			// Name of individual/sample (pointer to vcfFile genotypeIds) or .
    char hapIxA;		// Index of one haplotype's allele: 0=reference, 1=alt, 2=other alt
				// *or* if negative, missing data
    char hapIxB;		// Index of other haplotype's allele, or if negative, missing data
    bool isPhased;		// True if haplotypes are phased
    bool isHaploid;		// True if there is only one haplotype (e.g. chrY)
    int infoCount;		// Number of components named in FORMAT column
    struct vcfInfoElement *infoElements;	// Array of info components for this genotype call
    };

struct vcfRecord
/* A VCF data row (or list of rows). */
{
    struct vcfRecord *next;
    char *chrom;		// Reference assembly sequence name
    unsigned int chromStart;	// Start offset in chrom
    unsigned int chromEnd;	// End offset in chrom
    char *name;			// Variant name from ID column
    int alleleCount;		// Number of alleles (reference + alternates)
    char **alleles;		// Alleles: reference first then alternate alleles
    char *qual;			// . or Phred-scaled score, i.e. -10log_10 P(call in ALT is wrong)
    int filterCount;		// Number of ;-separated filter codes in FILTER column
    char **filters;		// Code(s) described in header for failed filters (or PASS or .)
    int infoCount;		// Number of components of INFO column
    struct vcfInfoElement *infoElements;	// Array of INFO column components
    char *format;		// Optional column containing ordered list of genotype components
    char **genotypeUnparsedStrings;	// Temporary array of unparsed optional genotype columns
    struct vcfGenotype *genotypes;	// If built, array of parsed genotype components;
					// call vcfParseGenotypes(record) to build.
    struct vcfFile *file;	// Pointer back to parent vcfFile
};

struct vcfFile
/* Info extracted from a VCF file.  Manages all memory for contents.
 * Clearly borrowing structure from MarkD's gff3File. :) */
{
    char *fileOrUrl;		// VCF local file path or URL
    char *headerString;		// Complete original header including newlines.
    int majorVersion;		// 4 etc.
    int minorVersion;		// 0, 1 etc.
    struct vcfInfoDef *infoDefs;	// Header's definitions of INFO column components
    struct vcfInfoDef *filterDefs;	// Header's definitions of FILTER column failure codes
    struct vcfInfoDef *altDefs;	// Header's defs of symbolic alternate alleles (e.g. DEL, INS)
    struct vcfInfoDef *gtFormatDefs;	// Header's defs of GENOTYPE compnts. listed in FORMAT col.
    int genotypeCount;		// Number of optional genotype columns described in header
    char **genotypeIds;		// Array of optional genotype column names described in header
    struct vcfRecord *records;	// VCF data rows, sorted by position
    struct hash *byName;		// Hash records by name -- not populated until needed.
    struct hash *pool;		// Used to allocate string values that tend to
				// be repeated in the files.  hash's localMem is also used to
				// allocated memory for all other objects (if recordPool null)
    struct lm *reusePool;       // If created with vcfFileMakeReusePool, non-shared record data is
                                // allocated from this pool. Useful when walking through huge files.
    struct lineFile *lf;	// Used only during parsing
    int maxErr;			// Maximum number of errors before aborting
    int errCnt;			// Error count
};

/* Reserved but optional INFO keys: */
extern const char *vcfInfoAncestralAllele;
extern const char *vcfInfoPerAlleleGtCount;	// allele count in genotypes, for each ALT allele,
						// in the same order as listed
extern const char *vcfInfoAlleleFrequency;  	// allele frequency for each ALT allele in the same
						// order as listed: use this when estimated from
						// primary data, not called genotypes
extern const char *vcfInfoNumAlleles;		// total number of alleles in called genotypes
extern const char *vcfInfoBaseQuality;		// RMS base quality at this position
extern const char *vcfInfoCigar;		// cigar string describing how to align an
						// alternate allele to the reference allele
extern const char *vcfInfoIsDbSnp;		// dbSNP membership
extern const char *vcfInfoDepth;		// combined depth across samples, e.g. DP=154
extern const char *vcfInfoEnd;			// end position of the variant described in this
						// record (esp. for CNVs)
extern const char *vcfInfoIsHapMap2;		// membership in hapmap2
extern const char *vcfInfoIsHapMap3;		// membership in hapmap3
extern const char *vcfInfoIs1000Genomes;	// membership in 1000 Genomes
extern const char *vcfInfoMappingQuality;	// RMS mapping quality, e.g. MQ=52
extern const char *vcfInfoMapQual0Count;	// number of MAPQ == 0 reads covering this record
extern const char *vcfInfoNumSamples;		// Number of samples with data
extern const char *vcfInfoStrandBias;		// strand bias at this position
extern const char *vcfInfoIsSomatic;		// indicates that the record is a somatic mutation,
						// for cancer genomics
extern const char *vcfInfoIsValidated;		// validated by follow-up experiment

/* Reserved but optional per-genotype keys: */
extern const char *vcfGtGenotype;	// numeric allele values separated by "/" (unphased)
					// or "|" (phased). Allele values are 0 for
					// reference allele, 1 for the first allele in ALT,
					// 2 for the second allele in ALT and so on.
extern const char *vcfGtDepth;		// read depth at this position for this sample
extern const char *vcfGtFilter;		// analogous to variant's FILTER field
extern const char *vcfGtLikelihoods;	// three floating point log10-scaled likelihoods for
					// AA,AB,BB genotypes where A=ref and B=alt;
					// not applicable if site is not biallelic.
extern const char *vcfGtPhred;		// Phred-scaled genotype likelihoods rounded to closest int
extern const char *vcfGtConditionalQual;	// Conditional genotype quality
					// i.e. phred quality -10log_10 P(genotype call is wrong,
					// conditioned on the site's being variant)
extern const char *vcfGtHaplotypeQualities;	// Two phred qualities comma separated
extern const char *vcfGtPhaseSet;	// Set of phased genotypes to which this genotype belongs
extern const char *vcfGtPhasingQuality;	// Phred-scaled P(alleles ordered wrongly in heterozygote)
extern const char *vcfGtExpectedAltAlleleCount;	// Typically used in association analyses

INLINE void vcfPrintDatum(FILE *f, const union vcfDatum datum, const enum vcfInfoType type)
/* Print datum to f in the format determined by type. */
{
switch (type)
    {
    case vcfInfoInteger:
	fprintf(f, "%d", datum.datInt);
	break;
    case vcfInfoFloat:
	fprintf(f, "%f", datum.datFloat);
	break;
    case vcfInfoFlag:
	fprintf(f, "%s", datum.datString); // Flags could have values in older VCF
	break;
    case vcfInfoCharacter:
	fprintf(f, "%c", datum.datChar);
	break;
    case vcfInfoString:
	fprintf(f, "%s", datum.datString);
	break;
    default:
	errAbort("vcfPrintDatum: Unrecognized type %d", type);
	break;
    }
}

#define VCF_IGNORE_ERRS (INT_MAX - 1)

struct vcfFile *vcfFileMayOpen(char *fileOrUrl, int maxErr, int maxRecords, boolean parseAll);
/* Open fileOrUrl and parse VCF header; return NULL if unable.
 * If parseAll, then read in all lines, parse and store in
 * vcff->records; if maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence. */

struct vcfFile *vcfTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
				    int maxErr, int maxRecords);
/* Open a VCF file that has been compressed and indexed by tabix and
 * parse VCF header, or return NULL if unable.  If chrom is non-NULL,
 * seek to the position range and parse all lines in range into
 * vcff->records.  If maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence. */

int vcfTabixBatchRead(struct vcfFile *vcff, char *chrom, int start, int end,
                      int maxErr, int maxRecords);
// Reads a batch of records from an opened and indexed VCF file, returning number
// of records in batch.  Seeks to the start position and parses all lines in range,
// adding them to vcff->records.  Note: vcff->records will continue to be sorted,
// even if batches are loaded out of order.  If maxErr >= zero, then continue to
// parse until there are maxErr+1 errors.  A maxErr less than zero does not stop
// and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence.

void vcfFileMakeReusePool(struct vcfFile *vcff, int initialSize);
// Creates a separate memory pool for records.  Establishing this pool allows
// using vcfFileFlushRecords to abandon previously read records and free
// the associated memory. Very useful when reading an entire file in batches.
#define vcfFileLm(vcff) ((vcff)->reusePool ? (vcff)->reusePool : (vcff)->pool->lm)

void vcfFileAbandonReusePool(struct vcfFile *vcff);
// Abandons all previously allocated data from the reuse pool and reverts to
// common pool. The vcf->records set will also be abandoned as pointers are invalid.
// USE WITH CAUTION.  All previously allocated pointers from this pool are now invalid.

void vcfFileFlushRecords(struct vcfFile *vcff);
// Abandons all previously read vcff->records and flushes the reuse pool (if it exists).
// USE WITH CAUTION.  All previously allocated record pointers are now invalid.

struct vcfRecord *vcfNextRecord(struct vcfFile *vcff);
/* Parse the words in the next line from vcff into a vcfRecord. Return NULL at end of file.
 * Note: this does not store record in vcff->records! */

struct vcfRecord *vcfRecordFromRow(struct vcfFile *vcff, char **words);
/* Parse words from a VCF data line into a VCF record structure. */

unsigned int vcfRecordTrimIndelLeftBase(struct vcfRecord *rec);
/* For indels, VCF includes the left neighboring base; for example, if the alleles are
 * AA/- following a G base, then the VCF record will start one base to the left and have
 * "GAA" and "G" as the alleles.  That is not nice for display for two reasons:
 * 1. Indels appear one base wider than their dbSNP entries.
 * 2. In pgSnp display mode, the two alleles are always the same color.
 * However, for hgTracks' mapBox we need the correct chromStart for identifying the
 * record in hgc -- so return the original chromStart. */

void vcfFileFree(struct vcfFile **vcffPtr);
/* Free a vcfFile object. */

const struct vcfRecord *vcfFileFindVariant(struct vcfFile *vcff, char *variantId);
/* Return all records with name=variantId, or NULL if not found. */

const struct vcfInfoElement *vcfRecordFindInfo(const struct vcfRecord *record, char *key);
/* Find an INFO element, or NULL. */

struct vcfInfoDef *vcfInfoDefForKey(struct vcfFile *vcff, const char *key);
/* Return infoDef for key, or NULL if it wasn't specified in the header or VCF spec. */

void vcfParseGenotypes(struct vcfRecord *record);
/* Translate record->genotypesUnparsedStrings[] into proper struct vcfGenotype[].
 * This destroys genotypesUnparsedStrings. */

const struct vcfGenotype *vcfRecordFindGenotype(struct vcfRecord *record, char *sampleId);
/* Find the genotype and associated info for the individual, or return NULL.
 * This calls vcfParseGenotypes if it has not already been called. */

struct vcfInfoDef *vcfInfoDefForGtKey(struct vcfFile *vcff, const char *key);
/* Look up the type of genotype FORMAT component key, in the definitions from the header,
 * and failing that, from the keys reserved in the spec. */

char *vcfFilePooledStr(struct vcfFile *vcff, char *str);
/* Allocate memory for a string from vcff's shared string pool. */

#define VCF_NUM_COLS 10

struct asObject *vcfAsObj();
// Return asObject describing fields of VCF


// - - - - - - Support for bit map based analysis of variants - - - - - -
struct variantBits
// all genotypes/haplotypes/alleles for one record are converted to a bit map
// One struct per variant record in vcff->records.  One slot per genotype containing
// 2 slots for haplotypes and then 1 or 2 bits per allele.
    {
    struct variantBits *next;
    struct vcfRecord *record;     // keep track of record for later interpretation
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

int vcfVariantBitsDropSparse(struct variantBits **vBitsList, int haploGenomeMin);
// Drops vBits found in less than a minimum number of haplotype genomes.
// Returns count of vBits structure that were dropped.

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

unsigned char vcfHaploBitsToVariantIx(struct haploBits *hBits,int bitIx);
// Given a hBits struct and bitIx, what is the actual variant allele ix
// to use when accessing the vcfRecord?

enum elmNodeOverlap vcfHaploBitsCmp(const struct slList *elA, const struct slList *elB,
                                    int *matchWeight, void *extra);
// HaploBits compare routine for building tree of relations using elmTreeGrow().

struct slList *vcfHaploBitsMatching(const struct slList *elA, const struct slList *elB,
                                    void *extra);
// Returns a HaploBits structure representing the common parts of elements A and B.
// Used with elmTreeGrow() to create nodes that are the common parts between leaves/branches.

#endif // vcf_h
