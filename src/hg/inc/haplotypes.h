// haplotypes.c/.h  Builds haploptypes from vcf data.  Initially this code is designed for
//                  Discovering and analyzing common gene haplotype alleles, but ultimately
//                  haplotypes can represent any collection of variant alleles that are found as a
//                  a set in on a single chromosome.

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef HAPLOTYPES_H
#define HAPLOTYPES_H

#include "common.h"
#include "dnautil.h"
#include "linefile.h"
#include "hash.h"
#include "vcfBits.h"

// TODO:
// - Does not handle intron edges because of the hgGenes difficulties of interpretation

// NOTE: Almost entirely replaced by vcf file info.  Kept for defaults when reading from file
// Note: chrX shows 525 males and 567 females.  But chrY shows 526 males!
#define THOUSAND_GENOME_SAMPLES        1092
#define THOUSAND_GENOME_Y_MALES        526
#define THOUSAND_GENOME_XY_MALES       525
#define THOUSAND_GENOME_XX_FEMALES     567
#define THOUSAND_GENOME_SUBJECTS(chr) \
                     ((chr)[3] == 'Y' ? THOUSAND_GENOME_Y_MALES : THOUSAND_GENOME_SAMPLES)
#define THOUSAND_GENOME_CHROMS(chr) \
    ( (chr)[3] == 'Y' ? THOUSAND_GENOME_Y_MALES \
    : (chr)[3] == 'X' ? THOUSAND_GENOME_XY_MALES + (THOUSAND_GENOME_XX_FEMALES * 2) \
    : (THOUSAND_GENOME_SAMPLES * 2) )
#define HAPLO_GENES_TABLE        "knownGene"
#define HAPLO_GENES_2ND_ID_TABLE "kgXref"
#define HAPLO_1000_GENOMES_TRACK "tgpPhase1"
#define HAPLO_VARIANT_MIN_PCT_DEFAULT 0
#define HAPLO_COMMON_VARIANT_MIN_PCT  1


// This structure is used exclusively for passing many options around between
// functions while building haplotypes
struct haploExtras {
    struct lm *lm;            // local memory pool means more efficient memory management
    char *db;                 // database: required command line arg
    char *chrom;              // chromsome: required command line arg
    unsigned chromStart;      // optional command line arg [chr1:0:10000]
    unsigned chromEnd;        // optional command line arg [chr1:0:10000]
    char *geneTable;          // UCSC knownGene table unless something else requested
    boolean canonical;        // Optionally restrict to canonical gene models only
    char *inFile;             // path to vcf file that must contain data for chrom
    char *vcfTrack;           // Track to use to discover VCF file
    boolean readBed;          // In file is a bed file which should contain gene Alleles already
    char *outTableOrFile;     // Name of table or file to output to
    boolean outToFile;        // Output is to file instead of table
    boolean append;           // CREATE or append to existing table/outFile
    boolean homozygousOnly;   // Include only variants that are found homozygous or on chrX/chrY
    boolean synonymous;       // Include synonymous SNPs in creating gene haplotypes
    float variantMinPct;      // include variants found in at least N % of haploid genomes
    int haploMin;             // include haplotypeMin found in at least N haploid genomes
    boolean justStats;        // Reads in outfile or table and prints stats
    boolean readable;         // If printing to file, make print readable and not overwhelming
    char *justModel;          // Name of a gene model to focus on
    int modelLimit;           // limit to the first N models with gene haplotypes
    boolean populationsToo;   // Include population distribution analysis and scoring
    boolean populationsMinor; // Show 1000 genome populations instead of subjects
    float populationMinPct;   // Only list populations with this minimum percentage of penetration
    boolean growTree;         // Use elmTree for haplotype relational naming
    boolean test;             // Test input but don't output
    char *tmpDir;             // location to create a temporary file
    struct hash *chrHash;     // Get chromInfo from DB
    struct sqlConnection *conn;  // Hold onto connection for efficiency
};

struct haploExtras *haplotypeExtrasDefault(char *db,int lmSize);
// Return a haploExtras support structure with all defaults set.  It will include
// local memory (lm) out of which almost all haplotype memory will be allocated.
// When done with haplotypes, free everything with haplotypeExtraFree().

void haplotypeExtrasFree(struct haploExtras **pHe);
// Releases a haploExtras structure which will release the llocal memory (lm) that contains
// almost all haplotype stuctures and values obtained with this token.
// WARNING: many pointers are rendered obsolete after this call.  Only do it at then of processing.

// ------------------------------------
// haplotypes variants support routines
// ------------------------------------
struct hapVar {
// haplotypes are made up of one or more variants
    // slList Only used for sorting! Always stored as arrays because of multiple uses!
    struct hapVar *next;   // Only used for sorting!
    int chromStart;         // start
    int chromEnd;           // end
    int origStart;          // UI needs original boundaries
    int origEnd;            // UI needs original boundaries
    enum variantType vType; // vtSNP, vtInsertion or vtDeletion relative to reference
    char *id;               // id of variant
    boolean reversed;       // true if the sequence reflects the negative strand
    DNA *val;               // non reference value.  Typically a single base for a SNP
    int dnaOffset;          // May get filled with offset into haplotype's dnaSeq
    int dnaLen;             // May get filled with length of haplotype's dnaSeq that is replaced
    AA *aaVal;              // May get filled with amino acid value resulting from variance
    int aaOffset;           // May get filled with offset into haplotype's aaSeq
    int aaLen;              // May get filled with length of haplotype's aaSeq that is replaced
    double frequency;       // frequency (1=100%) of variant allele in dataset
};


struct haploOptionals {
// haplotype optional fields.  Optionally generated by request
    // Population distribution is used by CGI and selected scoring runs
    struct slPair *popGroups; // Population Groups with counts in (int)(void *val)
    char *popDist;            // String of comma separated population IDs with percentages
    double popScore;           // This hap is scored by the SD of occurrances on populations
    // DNA and AA sequences are used by CGI display
    DNA *dnaSeq;              // May be filled with DNA sequence for model's coding region
    int dnaLen;               // May be filled with length of DNA sequence for model coding region
    AA *aaSeq;                // May get filled with translated AA sequence for model
    int aaLen;                // May get filled with length of AA sequence for model
};

struct haplotype {
// gene allele (haplotype) structure that will be organized in a tree using parent pointer
    struct haplotype *next;
    char *suffixId;           // Together with hapSet->commonId, uniquely identifies gene allele.
                              // Also defines relationships (e.g. uc002zlh.1:ab is the
                              // second variant of first non-ref gene allele of uc002zlh.1).
    int variantCount;         // Count of non-reference variants in this haplotype
    struct hapVar **variants; // Array of variants from vcf records (shares structs NOT slList)
                              // When build complete, each variant in the reference haplotype
                              // will be the head of the sList for variants at location
    int variantsCovered;      // Count of non-reference variants across all haplotypes per model
    int subjects;             // Count of haploid genomes that have this haplotype
    char *subjectIds;         // Haploid genome Ids from VCF data (comma separated list)
    double haploScore;        // Score of haploptype (based upon probability of occurance)
    double homScore;          // Score of homozygous or haploid subjects (based upon subjects)
    int homCount;             // Count of homozygous or haploid subjects
    int bitCount;             // Size of bits (no longer optional)
    Bits *bits;               // Hold onto bits from haploBits (no longer optional)
    short generations;        // count of generations when organized by tree (letters in suffix)
    short descendants;        // count of direct descendants when organized by tree
    struct haploOptionals *ho;// Optionally generated characteristics of this haplotype
};

struct hapBlocks {
// haplotype sequence blocks
    // slList Only used for sorting! Always stored as arrays because of multiple uses!
    struct hapBlocks *next; // Only used for sorting!
    int ix;                 // ix of refHaplotype variant or -1 for invariant block
    int beg;                // (0 based) beginning of block, relative to var or ref sequence.
    int len;                // length (in chars) of this block
};
#define IS_INVARIANT(block) ((block)->ix == -1)

struct haplotypeSet {
// A set of haplotypes for one model.  Typically a gene model and haplotypes are gene alleles
    struct haplotypeSet *next;
    char *chrom;              // Haplotype set is on one chrom
    unsigned chromStart;      // haplotype region start (e.g. genPred->cdsStart)
    unsigned chromEnd;        // haplotype region end   (e.g. genPred->cdsEnd  )
    char strand[2];           // + or - for strand
    char *commonId;           // used to uniquely identify this haplotype set (e.g. accessionId)
    char *secondId;           // Typically gene/protein id if known
    int variantsCovered;      // Count of variants found across this model
    int bitsPerVariant;       // Haplotype bitmaps may use 1 or 2 bits per variant
    struct haplotype *haplos; // Link list of haplotypes
    struct haplotype *refHap; // hasplotype of reference genome which contains all reference vars
                              // The refHap will be in the haplos list ONLY if found in VCF data
    unsigned wideStart;       // If gene model, then wideStart == gp->chromStart
    unsigned wideEnd;         // If gene model, then wideStart == gp->chromEnd
    boolean isGenePred;       // True if genePred model.  model should point to genePred struct.
    void *model;              // Typically the genePred gene model but could be something else
    int subjectN;             // Number of subjects in dataset that was used to build haplotypes
    int chromN;               // Number of chromosomes in dataset that was used to build haplotypes
    struct hapBlocks *hapBlocks; // Contains invaraint interleaved with varaint blocks in
                                 // Filled in if haplotypes are being converted to sequence
};

#define haploIsReference(haplo) ((haplo)->variantCount == 0)

// --------------------------------
// Lower level haplotypes functions
// --------------------------------
char *haplotypesDiscoverVcfFile(struct haploExtras *he, char *chrom);
// Discovers, if necessary and returns the VCF File to use

int haplotypePopularityCmp(const void *va, const void *vb);
// Compare to sort haplotypes by frequency of occurrence

int hapSetSecondIdCmp(const void *va, const void *vb);
// Compare to sort haplotypes in secondId, then haplotype Id order


// ---------------------------
// Tune-ups for haplotype Sets
// ---------------------------
boolean haploSetIncludesNonRefHaps(struct haplotypeSet *hapSet);
// returns TRUE if the haplotypeSet has atleast 1 non-ref haplotype

int haploSetsNonRefCount(struct haplotypeSet *haploSets);
// returns the number of haplotypeSets that include at least 1 non-ref haplotype

int haploSetsHaplotypeCount(struct haplotypeSet *haploSets, boolean nonRefOnly);
// returns the number of haplotypes in the haplotypes set list

int haploSetStrandOrient(struct haploExtras *he,struct haplotypeSet *hapSet);
// Orients all haplotypes in a set to the negative strand, if appropriate.
// The bitmap and variants are reversed.
// Returns a count of haplotype structures that have been altered


// ----------------------------------------------------
// Code for converting haplotypes to DNA or AA sequence
// ----------------------------------------------------

char *haploVarId(struct hapVar *var);
// Returns a string to use a the class or ID for a given variant

DNA *haploDnaSequence(struct haploExtras *he, struct haplotypeSet *hapSet,
                        struct haplotype *haplo, boolean alignmentGaps, boolean hilite);
// Generate and return the full DNA CODING sequence of a haplotype.
// When haplo is NULL, returns reference sequence.
// If no alignmentGaps or hilites then raw sequence is saved in haplo->ho->dnaSeq.

void geneHapSetAaVariants(struct haploExtras *he,
                          struct haplotypeSet *hapSet, struct haplotype *haplo);
// Determine haplotype AA variants using actual AA sequence
// If non-reference haplotype is NULL, then refHap variants AA are set.
// This routine will generate the AA seq if not found, but dnaSeq must exist

AA *haploAaSequence(struct haploExtras *he, struct haplotypeSet *hapSet,
                    struct haplotype *haplo, boolean triplet, boolean markVars);
// returns AA sequence for a haplotype.  if haplo is NULL returns AA seq for reference.
// If triplet view, then AAs are padded to align with DNA sequence as "L..P..".
// If markVars, then a single AA at the start of variant position is wrapped with a <span>
// with class matching the variantId. Only use markVars with refHap!
// If not triplet or marVars, then the raw sequence is stored in haplo->ho->aaSeq.

// --------------------------------
// gene haplotype alleles HTML code
// --------------------------------
#define HAPLO_TABLE       "haplotypes"
#define HAPLO_RARE_VAR    HAPLO_TABLE "_rareVar"
#define HAPLO_DNA_VIEW    HAPLO_TABLE "_dnaView"
#define HAPLO_FULL_SEQ    HAPLO_TABLE "_fullSeq"
#define HAPLO_MAJOR_DIST  HAPLO_TABLE "_distMajor"
#define HAPLO_MINOR_DIST  HAPLO_TABLE "_distMinor"
#define HAPLO_TRIPLETS    HAPLO_TABLE "_triplets"
#define HAPLO_RARE_HAPS   HAPLO_TABLE "_rareHaps"
#define HAPLO_SHOW_SCORES HAPLO_TABLE "_score"
#define HAPLO_RESET_ALL   HAPLO_TABLE "_reset"
#define HAPLO_CART_TIMEOUT 72

void geneAllelesTableAndControls(struct cart *cart, char *geneId,
                                 struct haploExtras *he, struct haplotypeSet *hapSet);
// Outputs the Gene Haplotype Alleles HTML table with button controls and AJAX replaceable div.


// -------------------------------------------
// gene haplotype reading and writing routines
// -------------------------------------------
int haploSetsLoadFromFile(char *inFile ,struct haploExtras *he,
                          struct haplotypeSet **pHaploSets);
// Loads a haplotypes bed file (bed12_5)
// Returns count of rows read.  The pGeneHapList will be returned in file order


void haploSetsPrintStats(struct haploExtras *he, FILE *out, char *commonId,
                         struct haplotypeSet *haploSets, int limitModels);
// verbose simple stats

int haploSetsPrint(struct haploExtras *he, char *outFile, boolean append,
                   struct haplotypeSet *haploSets, boolean forLoadBed);
// Print gene alleles to file.  Returns count of gene haplotypes printed

int haploSetsWriteToTable(struct haploExtras *he, struct haplotypeSet *haploSets);
// Load database table from bedList.

// -------------------------
// gene model level routines
// -------------------------
struct haplotypeSet *geneHapSetForOneModel(struct haploExtras *he,struct vcfFile *vcff,
                                           struct genePred *gp, boolean excludeRefOnly);
// Generates a haplotype set with one or more gene alleles (haplotypes) for one gene model

#endif //ndef HAPLOTYPES_H
