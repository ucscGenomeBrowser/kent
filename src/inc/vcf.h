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
        if (startsWith("http", datum.datString))
            fprintf(f, "<a target=_blank href='%s'>%s</a>", datum.datString, datum.datString);
        else
            fprintf(f, "%s", datum.datString);
	break;
    default:
	errAbort("vcfPrintDatum: Unrecognized type %d", type);
	break;
    }
}

#define VCF_IGNORE_ERRS (INT_MAX - 1)

struct vcfFile *vcfFileNew();
/* Return a new, empty vcfFile object. */

struct vcfFile *vcfFileFromHeader(char *name, char *headerString, int maxErr);
/* Parse the VCF header string into a vcfFile object with no rows.
 * name is for error reporting.
 * If maxErr is non-negative then continue to parse until maxErr+1 errors have been found.
 * A maxErr less than zero does not stop and reports all errors.
 * Set maxErr to VCF_IGNORE_ERRS for silence. */

struct vcfFile *vcfFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
			       int maxErr, int maxRecords, boolean parseAll);
/* Open fileOrUrl and parse VCF header; return NULL if unable.
 * If chrom is non-NULL, scan past any variants that precede {chrom, chromStart}.
 * Note: this is very inefficient -- it's better to use vcfTabix if possible!
 * If parseAll, then read in all lines in region, parse and store in
 * vcff->records; if maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence. */

struct vcfFile *vcfTabixFileAndIndexMayOpen(char *fileOrUrl, char *tbiFileOrUrl, char *chrom, int start, int end,
				    int maxErr, int maxRecords);
/* Open a VCF file that has been compressed and indexed by tabix and
 * parse VCF header, or return NULL if unable. tbiFileOrUrl can be NULL.
 * If chrom is non-NULL, seek to the position range and parse all lines in
 * range into vcff->records.  If maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence */

struct vcfFile *vcfTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
				    int maxErr, int maxRecords);
/* Open a VCF file that has been compressed and indexed by tabix and
 * parse VCF header, or return NULL if unable.  If chrom is non-NULL,
 * seek to the position range and parse all lines in range into
 * vcff->records.  If maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence. */

long long vcfTabixItemCount(char *fileOrUrl, char *tbiFileOrUrl);
/* Return the total number of items across all sequences in fileOrUrl, using index file.
 * If tbiFileOrUrl is NULL, the index file is assumed to be fileOrUrl.tbi.
 * NOTE: not all tabix index files include mapped item counts, so this may return 0 even for
 * large files. */

int vcfTabixBatchRead(struct vcfFile *vcff, char *chrom, int start, int end,
                      int maxErr, int maxRecords);
// Reads a batch of records from an opened and indexed VCF file, adding them to
// vcff->records and returning the count of new records added in this batch.
// Note: vcff->records will continue to be sorted, even if batches are loaded
// out of order.  Additionally, resulting vcff->records will contain no duplicates
// so returned count refects only the new records added, as opposed to all records
// in range.  If maxErr >= zero, then continue to parse until there are maxErr+1
// errors.  A maxErr less than zero does not stop and reports all errors.  Set
// maxErr to VCF_IGNORE_ERRS for silence.

void vcfFileMakeReusePool(struct vcfFile *vcff, int initialSize);
// Creates a separate memory pool for records.  Establishing this pool allows
// using vcfFileFlushRecords to abandon previously read records and free
// the associated memory. Very useful when reading an entire file in batches.
#define vcfFileLm(vcff) ((vcff)->reusePool ? (vcff)->reusePool : (vcff)->pool->lm)

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

unsigned int vcfRecordTrimAllelesRight(struct vcfRecord *rec);
/* Some tools output indels with extra base to the right, for example ref=ACC, alt=ACCC
 * which should be ref=A, alt=AC.  When the extra bases make the variant extend from an
 * intron (or gap) into an exon, it can cause a false appearance of a frameshift.
 * To avoid this, when all alleles have identical base(s) at the end, trim all of them,
 * and update rec->chromEnd.
 * For hgTracks' mapBox we need the correct chromStart for identifying the record in hgc,
 * so return the original chromEnd. */

int vcfRecordCmp(const void *va, const void *vb);
/* Compare to sort based on position. */

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

const struct vcfInfoElement *vcfGenotypeFindInfo(const struct vcfGenotype *gt, char *key);
/* Find the genotype infoElement for key, or return NULL. */

char *vcfFilePooledStr(struct vcfFile *vcff, char *str);
/* Allocate memory for a string from vcff's shared string pool. */

#define VCF_NUM_COLS 10

struct asObject *vcfAsObj();
// Return asObject describing fields of VCF

char *vcfGetSlashSepAllelesFromWords(char **words, struct dyString *dy);
/* Overwrite dy with a /-separated allele string from VCF words,
 * skipping the extra initial base that VCF requires for indel alleles if necessary.
 * Return dy->string for convenience. */

void vcfRecordWriteNoGt(FILE *f, struct vcfRecord *rec);
/* Write the first 8 columns of VCF rec to f.  Genotype data will be ignored if present. */

#endif // vcf_h
