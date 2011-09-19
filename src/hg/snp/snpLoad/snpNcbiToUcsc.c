/* snpNcbiToUcsc - Reformat NCBI SNP field values into UCSC, and flag exceptions.. */
#include "common.h"
#include "basicBed.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "dystring.h"
#include "hash.h"
#include "linefile.h"
#include "memalloc.h"
#include "obscure.h"
#include "options.h"
#include "sqlList.h"
#include "sqlNum.h"
#include "twoBit.h"
#include <regex.h>

static char const rcsid[] = "$Id: snpNcbiToUcsc.c,v 1.12 2010/06/04 05:36:03 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"snpNcbiToUcsc - Reformat NCBI SNP field values into UCSC, and flag exceptions.\n"
"usage:\n"
"   snpNcbiToUcsc ucscNcbiSnp.bed db.2bit snpNNN\n"
"options:\n"
"   -1000GenomesRsIds=file      Text file with one number per line, numerically\n"
"                               sorted, specifying RS IDs for submissions by the\n"
"                               1000 Genomes Project.\n"
"   -snp132Ext                  Add columns extracted from human 132 & later:\n"
"                               exceptions,\n"
"                               submitterCount, submitters,\n"
"                               alleleFreqCount, alleles, alleleNs, alleleFreqs\n"
"                               bitfields\n"
"                               Since exceptions are now in a column in snpNNN,\n"
"                               there is no separate snpNNNExceptions table.\n"
"  -par=file                    Read Pseudo-Autosomal Region (PAR) coordinates\n"
"                               from bed4 columns in file, and don't count single\n"
"                               mappings to different chromosomes' PARs as\n"
"                               multiple mappings to the reference genome.\n"
"\n"
"Converts NCBI's representation of SNP data into UCSC's, and checks for\n"
"consistency.  Typically NNN is the dbSNP release number; snpNNN is the\n"
"SNP track's main table name.  This program was written for dbSNP 128, and\n"
"should be updated if future versions have any added or changed encodings.\n"
"\n"
"Unexpected conditions are handled in one of these three ways, depending\n"
"on severity:\n"
"1. errAbort: the inconsistency is so severe that it is likely that this\n"
"program (and possibly hgTracks/hgc/hgTrackUi) will need to be modified.\n"
"2. data error: the SNP is incomplete or inconsistent with itself enough\n"
"to warrant excluding from output.  A bed4 + description is written to \n"
"snpNNNErrors.bed.\n"
"3. exception: the SNP has an oddity worth noting.  A bed4 + description\n"
"is written to snpNNNExceptions.bed.\n"
"\n"
"The following output files are generated:\n"
"  snpNNN.bed                Main SNP track table data.\n"
"  snpNNN.sql                SQL for above, with enums and sets generated\n"
"                            from the latest encodings.\n"
"  snpNNNExceptions.bed      Notable (often dubious) conditions detected for\n"
"                            this SNP.  Not generated if -snp132Ext.\n"
"  snpNNNExceptionDesc.tab   Descriptions and counts of each type of notable\n"
"                            condition.\n"
"  snpNNNErrors.bed          Data errors -- this should be empty.\n"
"\n"
  );
}

static struct optionSpec options[] = {
    {"1000GenomesRsIds", OPTION_STRING},
    {"snp132Ext", OPTION_BOOLEAN},
    {"par", OPTION_STRING},
    {NULL, 0},
};

char *oneKGenomesRsIds = NULL;
boolean snp132Ext = FALSE;
char *parFile = NULL;

#define UPDATE_CODE "update this program and possibly hgTracks/hgc/hgTrackUi."

/*************************** IMPORTANT *********************************
 * NCBI gives integer codes for several fields that we translate into
 * (lists of) string values.  These string values are used by hgTracks 
 * and hgc to display the track data.  If there are any additions or
 * changes in NCBI's encoding, then not only this program, but also the
 * CGIs (hgTrackUi too) should be examined to tell how to handle the
 * new representation (ignore/fold into old representation or add new
 * display functionality).
 */

/* $ftpBcp below refers to
 * ftp://ftp.ncbi.nih.gov/snp/organisms/human_9606/database/shared_data */

/* These class strings and their positions must correspond to the values in
 * $ftpBcp/SnpClassCode.bcp.gz
 * -- see also snpClass in the ASN:
 * ftp://ftp.ncbi.nih.gov/snp/specs/docsum_2005.asn */
/* They are determined by data submitters, and should be consistent with 
 * observed alleles. */
#define MAX_CLASS 8
char *classStrings[] = {
    "unknown",		/* UCSC addition */
			/* "code" here means NCBI's SnpClassCode.bcp.gz: */
    "single",		/* code: single base	ASN: snp */
    "in-del",		/* code: dips		ASN: in-del */
    "het",		/* code: HETEROZYGOUS	ASN: heterozygous */
    "microsatellite",	/* code: Microsatellite	ASN: microsatellite */
    "named",		/* code: Named snp	ASN: named-locus */
    "no var",		/* code: NOVARIATION	ASN: no-variation */
    "mixed",		/* code: mixed		ASN: mixed */
    "mnp",		/* code: multi-base	ASN: multinucleotide-polymorphism */
    /* UCSC adds these to make in-del more specific using other info.
     * Note: this ins/del is the opposite of locType below. */
    "insertion",	/* SNP is an insertion; reference has deletion */
    "deletion",		/* SNP is a deletion; reference has insertion */
};
boolean classStringsUsed[MAX_CLASS+2+1];


/* These strings and their positions must correspond to the values in
 * $ftpBcp/LocTypeCode.bcp.gz 
 * -- see also locType in the ASN:
 * ftp://ftp.ncbi.nih.gov/snp/specs/docsum_2005.asn */
/***************************** IMPORTANT *********************************:
 * As of snp128, NCBI still aligns flanking sequences concatenated
 * with only one IUPAC-ambiguous-coded base to represent the SNP --
 * even if observed alleles are longer.  Multi-base SNPs are coded as
 * a single N.  IMHO that renders locTypes 1-3 unreliable and causes a
 * lot of alignment problems, increasing the number of locTypes 4-6
 * and conflicts between observed & reference allele.  So we have a
 * lot of related exceptions to report...  This use of a single IUPAC
 * base for SNP of any size is described on this page:
 * http://www.ncbi.nlm.nih.gov/books/bv.fcgi?indexed=google&rid=helpsnpfaq.section.Build.The_dbSNP_Mapping_Pr
 * -- expand the "Reassigning loctype" section.
 */
/* locType is determined by NCBI, from alignments to the reference genome
 * sequence.  The first 3 types (range, exact, between) tell us only
 * whether the alignment spans multiple bases, one base, or 0 bases
 * respectively -- so they are redundant with chrEnd-chrStart.  
 * The latter 3 types (range{Insertion,Substitution,Deletion}) are 
 * used when the alignment of flanking sequence immediately adjacent to
 * the IUPAC code for the SNP, and the code itself, does not match the
 * genome.  This is often a flag for a suboptimal alignment or a 
 * flanking sequence with a lot of ambiguous bases near the coded base. */

#define MAX_LOCTYPE 6
char *locTypeStrings[] = {
    "unknown",		/* UCSC addition */
			/* "code" here means NCBI's LocTypeCode.bcp.gz: */
    "range",		/* code: InsOnCtg	ASN: insertion */
    "exact",		/* code: trueSNP	ASN: exact */
    "between",		/* code: DelOnCtg	ASN: deletion */
    "rangeInsertion",	/* code: LongerOnCtg	ASN: range-ins */
    "rangeSubstitution",/* code: EqualOnCtg	ASN: range-exact */
    "rangeDeletion",	/* code: ShorterOnCtg	ASN: range-del */
};
boolean locTypeStringsUsed[MAX_LOCTYPE+1];

/* These strings and their positions must correspond to the values in
 * $ftpBcp/SnpValidationCode.bcp.gz / ASN. */
#define VALID_BITS 6
#define VALID_1KG 5
#define MAX_VALID ((1<<VALID_BITS)-1)
char *validBitStrings[] = {
"by-cluster",
"by-frequency",
"by-submitter",
"by-2hit-2allele",
"by-hapmap",
"by-1000genomes",
};
boolean validBitStringsUsed[VALID_BITS];

/* These strings and their positions must correspond to the values in
 * $ftpBcp/SnpFunctionCode.bcp.gz / ASN (but ASN doesn't have
 * the newer encodings (> 10). */
/* See also
 * http://www.ncbi.nlm.nih.gov/books/bv.fcgi?rid=helpsnpfaq.section.Build.Data_Changes_that_Oc */
#define MAX_FUNC 75
char *functionStrings[] = {
/*  0 */ "unknown",
/*  1 */ "locus",
/*  2 */ "coding",
/*  3 */ "coding-synon",
/*  4 */ "coding-nonsynon",
/*  5 */ "untranslated",
/*  6 */ "intron",
/*  7 */ "splice-site",
/*  8 */ "cds-reference",
/*  9 */ "coding-synonymy-unknown",
/* teens: extensions of 1 above (locus): */
/* 10 */ NULL,
/* 11 */ "gene-segment",
/* 12 */ NULL,
/* 13 */ "near-gene-3",
/* 14 */ NULL,
/* 15 */ "near-gene-5",
/*16*/NULL,/*17*/NULL,/*18*/NULL,/*19*/NULL,
/*20*/NULL,/*21*/NULL,/*22*/NULL,/*23*/NULL,/*24*/NULL,
/*25*/NULL,/*26*/NULL,/*27*/NULL,/*28*/NULL,/*29*/NULL,
/*30*/NULL,/*31*/NULL,/*32*/NULL,/*33*/NULL,/*34*/NULL,
/*35*/NULL,/*36*/NULL,/*37*/NULL,/*38*/NULL,/*39*/NULL,
/* forties: extensions of 4 above (coding-nonsynon): */
/* 40 */ NULL,
/* 41 */ "nonsense",
/* 42 */ "missense",
/* 43 */ "stop-loss",
/* 44 */ "frameshift",
/* 45 */ "cds-indel",
/*46*/NULL,/*47*/NULL,/*48*/NULL,/*49*/NULL,
/* fifties: extensions of 5 above (untranslated): */
/*50*/NULL,/*51*/NULL,/*52*/NULL,
/* 53 */ "untranslated-3",
/* 54 */ NULL,
/* 55 */ "untranslated-5",
/*56*/NULL,/*57*/NULL,/*58*/NULL,/*59*/NULL,
/*60*/NULL,/*61*/NULL,/*62*/NULL,/*63*/NULL,/*64*/NULL,
/*65*/NULL,/*66*/NULL,/*67*/NULL,/*68*/NULL,/*69*/NULL,
/* seventies: extensions of 7 above (splice-site): */
/*70*/NULL,/*71*/NULL,/*72*/NULL,
/* 73 */ "splice-3",
/* 74 */ NULL,
/* 75 */ "splice-5",
};
boolean functionStringsUsed[MAX_FUNC+1];

#define MAX_WEIGHT 10

#define MAX_MOLTYPE 3
char *molTypeStrings[] = {
    "unknown",
    "genomic",
    "cDNA",
    "mito"
};
boolean molTypeStringsUsed[MAX_MOLTYPE+1];


// dbSNP's SNP_bitfield table contains various annotations encoded as bitmasks
// -- see ftp://ftp.ncbi.nlm.nih.gov/snp/specs/dbSNP_BitField_v5.pdf
// -- but note that not all of the spec'd bitmasks are actually populated,
// and some are incomplete encodings (e.g. 2 bits for weight, but it can be
// 0, 1, 2, 3 or 10), so we can't make use of all fields that appear useful.
#define MAX_BITFIELDS 9
char *bitfieldsStrings[] = {
    "unknown",
    // "Clinical" bit from link_prop_b2 (link properties, byte 2):
    "clinically-assoc",
    // >5% MAF in some or all populations from freq_prop:
    "maf-5-some-pop",
    "maf-5-all-pops",
    // Known phenotype sources from pheno_prop:
    "has-omim-omia",
    "microattr-tpa",
    "submitted-by-lsdb",
    // dbSNP's quality_check:
    "genotype-conflict",
    "rs-cluster-nonoverlapping-alleles",
    "observed-mismatch",
};
boolean bitfieldsStringsUsed[MAX_BITFIELDS+1];

// Byte offset is index of column pulled in from SNP_bitfields table;
// bit offset is for masking the byte to get the bitfield named above.
struct byteBitOffsets
    {
    unsigned short byte;
    unsigned short bit;
    };
struct byteBitOffsets bitfieldsOffsets[] = {
    // placeholder for unknown:
    {0, 15},
    // link_prop_b2:
    {0, 6}, // clinical
    // freq_prop:
    {1, 0}, // >5% in at least one pop
    {1, 1}, // >5% in all pops
    // pheno_prop:
    {2, 0}, // has OMIM or OMIA
    {2, 1}, // has microattribution or third-party annotation
    {2, 4}, // submitted by LSDB
    // quality_check:
    {3, 0}, // genotype conflict (same individual, different genotypes)
    {3, 2}, // rs cluster has non-overlapping allele sets (should be similar to tri & quad allelic)
    {3, 4}, // contig allele of at least one mapping is not present in snp allele list
            // (mostly matches ObservedMismatch)
};

// Most of those we just pass through, but we add exceptions for the bits in quality_check:
#define BITFIELDS_QUAL_CHECK_BYTE 3
#define BITFIELDS_GENOTYPE_CONFLICT 0
#define BITFIELDS_NON_OV_ALLELES 2
#define BITFIELDS_REF_MISMATCH 4


void initStringsUsed()
/* Set the arrays of flags that indicate whether an encoding has been used. */
{
int i;
for (i = 0;  i < MAX_MOLTYPE+1;  i++)
    molTypeStringsUsed[i] = FALSE;
for (i = 0;  i < MAX_CLASS+2+1;  i++)
    classStringsUsed[i] = FALSE;
for (i = 0;  i < MAX_LOCTYPE+1;  i++)
    locTypeStringsUsed[i] = FALSE;
for (i = 0;  i < VALID_BITS;  i++)
    validBitStringsUsed[i] = FALSE;
for (i = 0;  i < MAX_FUNC+1;  i++)
    functionStringsUsed[i] = FALSE;
}

#define isMissing(val) (sameString("MISSING", val) || sameString("NULL", val))

void tallyMoltype(struct lineFile *lf, char *molType)
/* Keep track of what molTypes are actually used. */
{
int i;
if (isEmpty(molType))
    lineFileAbort(lf, "Empty (as opposed to missing) molType value.\n");
for (i = 0;  i < MAX_MOLTYPE+1;  i++)
    if (sameString(molType, molTypeStrings[i]))
	{
	molTypeStringsUsed[i] = TRUE;
	return;
	}
if (isMissing(molType))
    molTypeStringsUsed[0] = TRUE;
else
    lineFileAbort(lf, "Unrecognized molType %s", molType);
}


void writeCodes(FILE *f, char *strings[], boolean stringsUsed[], int count)
/* Print out a comma-sep list of single-quoted values from one of the
 * above arrays. */
{
int i;
boolean needComma = FALSE;
for (i = 0;  i < count;  i++)
    {
    if (i == 0 && differentStringNullOk(strings[i], "unknown"))
	{
	fprintf(f, "'unknown'");
	needComma = TRUE;
	}
    if (isNotEmpty(strings[i]) && stringsUsed[i])
	{
	fprintf(f, "%s'%s'", (needComma ? "," : ""), strings[i]);
	needComma = TRUE;
	}
    }
}

// using prototype here because the code depends on exception types and
// variables defined further on.
void writeExceptionCodes(FILE *f);
/* snp132Ext: Write out a comma-sep list of single-quoted exceptions for
 * mysql column definition (type=set).
 * Set can be empty so don't add 'unknown' at beginning as writeCodes does. */

void writeSnpSql(char *outRoot)
/* Write outRoot.sql, translating the above codes into enums and sets. */
{
char fileName[2048];
safef(fileName, sizeof(fileName), "%s.sql", outRoot);
FILE *f = mustOpen(fileName, "w");

fprintf(f,
"CREATE TABLE %s (\n"
"  bin smallint(5) unsigned NOT NULL,\n"
"  chrom varchar(31) NOT NULL,\n"
"  chromStart int(10) unsigned NOT NULL,\n"
"  chromEnd int(10) unsigned NOT NULL,\n"
"  name varchar(15) NOT NULL,\n"
"  score smallint(5) unsigned NOT NULL,\n"
"  strand enum('+','-') NOT NULL,\n"
"  refNCBI blob NOT NULL,\n"
"  refUCSC blob NOT NULL,\n"
"  observed varchar(255) NOT NULL,\n"
"  molType enum(", outRoot);
writeCodes(f, molTypeStrings, molTypeStringsUsed, MAX_MOLTYPE+1);
fprintf(f,
") NOT NULL,\n"
"  class enum(");
writeCodes(f, classStrings, classStringsUsed, MAX_CLASS+2+1);
fprintf(f,
") NOT NULL,\n"
"  valid set(");
writeCodes(f, validBitStrings, validBitStringsUsed, VALID_BITS+1);
fprintf(f,
") NOT NULL,\n"
"  avHet float NOT NULL,\n"
"  avHetSE float NOT NULL,\n"
"  func set(");
writeCodes(f, functionStrings, functionStringsUsed, MAX_FUNC+1);
fprintf(f,
") NOT NULL,\n"
"  locType enum(");
writeCodes(f, locTypeStrings, locTypeStringsUsed, MAX_LOCTYPE+1);
fprintf(f,
") NOT NULL,\n"
"  weight int(10) unsigned NOT NULL,\n");
if (snp132Ext)
    {
    fprintf(f,
  "  exceptions set(");
    writeExceptionCodes(f);
    fprintf(f,
") NOT NULL,\n"
"  submitterCount smallint unsigned NOT NULL,\n"
"  submitters longblob NOT NULL,\n"
"  alleleFreqCount smallint unsigned NOT NULL,\n"
"  alleles longblob NOT NULL,\n"
"  alleleNs longblob NOT NULL,\n"
"  alleleFreqs longblob NOT NULL,\n"
"  bitfields set (");
    writeCodes(f, bitfieldsStrings, bitfieldsStringsUsed, MAX_BITFIELDS+1);
    fprintf(f,
") NOT NULL,\n"
	    );
    }
fprintf(f,
"  INDEX name (name),\n"
"  INDEX chrom (chrom,bin)\n"
");\n");
}


/****** Globals for the bed4 fields which are used all over the place: ******/
char *chr = NULL;
int chrStart = 0;
int chrEnd = 0;
int rsId = 0;
unsigned int exceptionBits = 0;

// Fudge factor for checking allele frequency properties:
#define ALLELE_FREQ_ROUNDING_ERROR 0.01
#define ALLELE_N_ROUNDING_ERROR 0.01


/******************* Reporting of errors and exceptions *********************/
/* The following exceptions that appeared in snp126 have been 
 * significantly changed:
 * - RefAlleleNotRevComp --> RefAlleleRevComp 
 *   -- because NCBI's convention changed.
 * - RefAlleleWrongSize: promoted to error (would require investigation)
 * - SingleClassBetweenLocType --> SingleClassZeroSpan
 *   SingleClassRangeLocType --> SingleClassLongerSpan
 *   NamedClassWrongLocType --> NamedDeletionZeroSpan,
 *                              NamedInsertionNonzeroSpan
 *   -- to remove dependence on locTypes 1-3 which are IMHO unreliable for
 *      multi-base SNPs.
 * - ObservedWrongFormat: only used for class=single now; otherwise we 
 *   write an error or abort because an unrecognized format deserves 
 *   developer attention.  IUPAC bases in other classes get their own
 *   exception, ObservedContainsIupac -- good to highlight, but not as
 *   bad as a real format error.
 * - ObservedWrongSize: removed.  IMHO it's locType, not observed, that 
 *   most likely has the problem.
 * - RangeSubstitutionLocTypeExactMatch: removed -- I think rangeSubstitution
 *   was misinterpreted.
 * - ObservedNotAvailable: missing observed promoted to error (skip
 *   this snp) -- not much to show, and these seem to be deleted SNPs in
 *   general.  lengthTooLong gets its own exception, ObservedTooLong.
 * New exceptions not named above:
 * - FlankMismatchGenome{Longer,Equal,Shorter} -- When we see locType
 *   4, 5, 6 / range{Insertion,Substitution,Deletion} respectively,
 *   alert the user (probably a problem with alignment and/or flanking
 *   sequences).
 * New exceptions as of snp132Ext:
 * - NonIntegerChromCount: dbSNP's SNPAlleleFreq table has a chr_cnt
 *   column which is not a true count but rather an extrapolation:
 *   reported allele frequency * reported total sample size.  Sometimes
 *   these end up as fractional numbers, in which case the reported 
 *   sample size could not have been the denominator used to compute the
 *   reported allele frequency.  Would be nice if dbSNP could catch this
 *   discrepancy in incoming submissions.
 * - AlleleFreqSumNot1: only 1 case of this in human 132: SNPAlleleFreq
 *   has a row with an allele_id not found in Allele, which causes us to
 *   lose that row of SNPAlleleFreq (perhaps should do a left join to
 *   make ucscAlleleFreq in doDbSnp.pl, but then we'd have to deal with
 *   missing values in the middle of float arrays).
 */

/* Use an enum that indexes arrays of char * to avoid spelling errors. */
enum exceptionType
    {
    /* processUcscAllele() */
    RefAlleleMismatch,
    RefAlleleRevComp,
    /* reportCluster() */
    DuplicateObserved,
    MixedObserved,
    /* checkLocType() */
    FlankMismatchGenomeLonger,
    FlankMismatchGenomeEqual,
    FlankMismatchGenomeShorter,
    /* checkClass() */
    NamedDeletionZeroSpan,
    NamedInsertionNonzeroSpan,
    SingleClassLongerSpan,
    SingleClassZeroSpan,
    /* checkObservedFormat() */
    SingleClassTriAllelic,
    SingleClassQuadAllelic,
    ObservedWrongFormat,
    ObservedTooLong,
    ObservedContainsIupac,
    /* checkObservedAgainstReference() */
    ObservedMismatch,
    /* reportMultipleMappings() */
    MultipleAlignments,
    /* processAlleleFreqs() */
    NonIntegerChromCount,
    AlleleFreqSumNot1,
    /* Keep this one last as a count of enum values: */
    exceptionTypeCount
    };

FILE *fErr = NULL;
// Without -snp132Ext, we write exceptions to file fExc:
FILE *fExc = NULL;
// with -snp132Ext, we set exception flag bits on the first pass
// through input and then add to main SNP table in the second pass.

// Global flag that is set if we find an inconsistency bad enough to reject this SNP:
boolean skipIt = FALSE;
int skipCount = 0;

char **exceptionNames = NULL;
char **exceptionDescs = NULL;
int   *exceptionCounts = NULL;

void initErrException(char *outRoot)
/* Open output file handles for errors and exceptions and set up exception
 * names, descriptions and counts. */
{
char fileName[2048];
safef(fileName, sizeof(fileName), "%sErrors.bed", outRoot);
fErr = mustOpen(fileName, "w");
if (! snp132Ext)
    {
    safef(fileName, sizeof(fileName), "%sExceptions.bed", outRoot);
    fExc = mustOpen(fileName, "w");
    }

AllocArray(exceptionNames, exceptionTypeCount);
/* processUcscAllele() */
exceptionNames[RefAlleleMismatch] = "RefAlleleMismatch";
exceptionNames[RefAlleleRevComp] = "RefAlleleRevComp";
/* reportCluster() */
exceptionNames[DuplicateObserved] = "DuplicateObserved";
exceptionNames[MixedObserved] = "MixedObserved";
/* checkLocType() */
exceptionNames[FlankMismatchGenomeLonger] = "FlankMismatchGenomeLonger";
exceptionNames[FlankMismatchGenomeEqual] = "FlankMismatchGenomeEqual";
exceptionNames[FlankMismatchGenomeShorter] = "FlankMismatchGenomeShorter";
/* checkClass() */
exceptionNames[NamedDeletionZeroSpan] = "NamedDeletionZeroSpan";
exceptionNames[NamedInsertionNonzeroSpan] = "NamedInsertionNonzeroSpan";
exceptionNames[SingleClassLongerSpan] = "SingleClassLongerSpan";
exceptionNames[SingleClassZeroSpan] = "SingleClassZeroSpan";
/* checkObservedFormat() */
exceptionNames[SingleClassQuadAllelic] = "SingleClassQuadAllelic";
exceptionNames[SingleClassTriAllelic] = "SingleClassTriAllelic";
exceptionNames[ObservedWrongFormat] = "ObservedWrongFormat";
exceptionNames[ObservedTooLong] = "ObservedTooLong";
exceptionNames[ObservedContainsIupac] = "ObservedContainsIupac";
/* checkObservedAgainstReference() */
exceptionNames[ObservedMismatch] = "ObservedMismatch";
/* reportMultipleMappings() */
exceptionNames[MultipleAlignments] = "MultipleAlignments";
/* processAlleleFreqs() */
exceptionNames[NonIntegerChromCount] = "NonIntegerChromCount";
exceptionNames[AlleleFreqSumNot1] = "AlleleFreqSumNot1";


/* Many of these conditions imply a problem with NCBI's alignment and/or the
 * flanking sequences.  hgc shows an axtAffine re-alignment of the flanking
 * sequences, which isn't always exactly the same as NCBI's, but often it
 * can shed some light. */
#define NCBI_ALI_PROBLEM "NCBI's alignment of the flanking sequences " \
                         "had at least one mismatch or gap.  "
#define SEE_ALIGNMENT "(UCSC's re-alignment of flanking sequences " \
                      "to the genome may be informative -- see below.)"

AllocArray(exceptionDescs, exceptionTypeCount);
/* processUcscAllele() */
exceptionDescs[RefAlleleMismatch] =
    "The reference allele from dbSNP does not match the UCSC reference "
    "allele.";
exceptionDescs[RefAlleleRevComp] =
    "The reference allele from dbSNP matches the reverse complement of the "
    "UCSC reference allele.";
/* reportCluster() */
exceptionDescs[DuplicateObserved] =
    "There are other rsIds at this position with identical variation.";
exceptionDescs[MixedObserved] =
    "There are other rsIds at this position with different variation.";
/* checkLocType() */
exceptionDescs[FlankMismatchGenomeLonger] =
    NCBI_ALI_PROBLEM
    SEE_ALIGNMENT;
exceptionDescs[FlankMismatchGenomeEqual] =
    NCBI_ALI_PROBLEM
    SEE_ALIGNMENT;
exceptionDescs[FlankMismatchGenomeShorter] =
    NCBI_ALI_PROBLEM
    SEE_ALIGNMENT;
/* checkClass() */
exceptionDescs[NamedDeletionZeroSpan] =
    "A deletion (from the genome) was observed but the annotation "
    "spans 0 bases.  "
    SEE_ALIGNMENT;
exceptionDescs[NamedInsertionNonzeroSpan] =
    "An insertion (into the genome) was observed but the annotation "
    "spans more than 0 bases.  "
    SEE_ALIGNMENT;
exceptionDescs[SingleClassLongerSpan] =
    "All observed alleles are single-base, but the annotation spans more "
    "than 1 base.  "
    SEE_ALIGNMENT;
exceptionDescs[SingleClassZeroSpan] =
    "All observed alleles are single-base, but the annotation spans 0 bases.  "
    SEE_ALIGNMENT;
/* checkObservedFormat() */
exceptionDescs[SingleClassTriAllelic] =
    "This single-base substitution is tri-allelic.";
exceptionDescs[SingleClassQuadAllelic] =
    "This single-base substitution is quad-allelic.";
exceptionDescs[ObservedWrongFormat] =
    "Observed allele(s) from dbSNP have unexpected format for the "
    "given class.";
exceptionDescs[ObservedTooLong] =
    "Observed allele not given (length too long).";
exceptionDescs[ObservedContainsIupac] =
    "Observed allele(s) from dbSNP contain IUPAC ambiguous bases.";
/* checkObservedAgainstReference() */
exceptionDescs[ObservedMismatch] =
    "UCSC reference allele does not match any observed allele from dbSNP.";
/* reportMultipleMappings() */
exceptionDescs[MultipleAlignments] =
    "This variant aligns in more than one location.";
/* processAlleleFreqs() */
char buf[256];
safef(buf, sizeof(buf),
    "At least one allele frequency corresponds to a non-integer (+-%f) "
    "count of chromosomes on which the allele was observed."
    "  The reported total sample count for this SNP is probably incorrect.",
      ALLELE_N_ROUNDING_ERROR);
exceptionDescs[NonIntegerChromCount] = cloneString(buf);
safef(buf, sizeof(buf),
    "Allele frequencies do not sum to 1.0 (+-%f)."
    "  This SNP's allele frequency data are probably incomplete.",
      ALLELE_FREQ_ROUNDING_ERROR);
exceptionDescs[AlleleFreqSumNot1] = cloneString(buf);

AllocArray(exceptionCounts, exceptionTypeCount);

/* Make sure things look complete... consistency is still up to developer: */
int i;
for (i = 0;  i < exceptionTypeCount;  i++)
    {
    if (isEmpty(exceptionNames[i]))
	errAbort("Missing name for exceptionType %d", i);
    if (isEmpty(exceptionDescs[i]))
	errAbort("Edit me and add a description for %s", exceptionNames[i]);
    }
}

void vaWriteError(char *format, va_list args)
/* Write SNP bed4 and the reason for exclusion to the error output file. */
{
if (format != NULL)
    {
    fprintf(fErr, "%s\t%d\t%d\trs%d\t",
	    chr, chrStart, chrEnd, rsId);
    vfprintf(fErr, format, args);
    if (! endsWith("\n", format))
	fprintf(fErr, "\n");
    }
skipIt = TRUE;
skipCount++;
}

void writeError(char *format, ...)
/* Write SNP bed4 and the reason for exclusion to the error output file. */
{
va_list args;
va_start(args, format);
vaWriteError(format, args);
va_end(args);
}

void writeExceptionToFile(char *dChr, int dStart, int dEnd, int dRsId,
			  enum exceptionType exc)
/* Write SNP bed4 and the unusual condition to the exception output file. */
{
fprintf(fExc, "%s\t%d\t%d\trs%d\t%s\n", dChr, dStart, dEnd, dRsId,
	exceptionNames[exc]);
}

void writeException(enum exceptionType exc)
/* Write _current_ SNP bed4 and the unusual condition to the exception output file,
 * or if snp132Ext, accumulate into the global exceptionBits. */
{
if (snp132Ext)
    exceptionBits |= (1 << exc);
else
    writeExceptionToFile(chr, chrStart, chrEnd, rsId, exc);
exceptionCounts[exc]++;
}

// Using prototype here because the code depends on struct coord defs below:
void setExceptionBit(char *eChr, int eStart, int eEnd, int eRsId, enum exceptionType exc);
/* Look up mapping record by id and position, and set bit exc in map->exceptions.
 * Naturally this must be called only after the mapping has been stored. */

void writeExceptionRetro(char *rChr, int rStart, int rEnd, int rRsId, enum exceptionType exc)
/* Retroactively (because it is no longer the current SNP) write a specific SNP 
 * mapping as bed4 plus the unusual condition to the exception output file,
 * or if snp132Ext, set the bit for exc in the SNP's stored mapping record. */
{
if (snp132Ext)
    setExceptionBit(rChr, rStart, rEnd, rRsId, exc);
else
    writeExceptionToFile(rChr, rStart, rEnd, rRsId, exc);
exceptionCounts[exc]++;
}

void finishErrException(char *outRoot)
/* Close exception and error file handles.
 * Write out outRootExceptionDesc.tab with exception counts. */
{
fclose(fErr);
if (! snp132Ext)
    fclose(fExc);
fErr = fExc = NULL;

char fileName[2048];
safef(fileName, sizeof(fileName), "%sExceptionDesc.tab", outRoot);
FILE *fExcDesc = mustOpen(fileName, "w");
int i;
for (i = 0;  i < exceptionTypeCount;  i++)
    {
    fprintf(fExcDesc, "%s\t%d\t%s\n",
	    exceptionNames[i], exceptionCounts[i], exceptionDescs[i]);
    }
fclose(fExcDesc);
}

void writeExceptionCodes(FILE *f)
/* snp132Ext: Write out a comma-sep list of single-quoted exceptions for
 * mysql column definition (type=set).
 * Set can be empty so don't add 'unknown' at beginning as writeCodes does. */
{
int i;
boolean needComma = FALSE;
for (i = 0;  i < exceptionTypeCount;  i++)
    if (exceptionCounts[i] > 0)
	{
	fprintf(f, "%s'%s'", (needComma ? "," : ""), exceptionNames[i]);
	needComma = TRUE;
	}
}

/******************* Field processing and simple checks *********************/

char processStrand(struct lineFile *lf, int strandNum)
/* Translate and check strand. */
{
char strand = (strandNum == 0) ? '+' : '-';
if (strandNum != 0 && strandNum != 1)
    lineFileAbort(lf, "Strand must be 0 or 1, but got %d", strandNum);
return strand;
}

void processClass(struct lineFile *lf, int classNum,
		  char class[], size_t classSize)
/* Translate and check class. */
{
if (classNum < 0)
    classNum = 0;
if (classNum > MAX_CLASS)
    lineFileAbort(lf, "Unrecognized class number %d (max is %d).\n"
		  "If %d is valid now, "UPDATE_CODE,
		  classNum, MAX_CLASS, classNum);
safef(class, classSize, "%s", classStrings[classNum]);
classStringsUsed[classNum] = TRUE;
}

static void chopTrailingComma(char *str)
/* MySQL doesn't like trailing commas in set values, so if present, chop it. */
{
int last = strlen(str) - 1;
if (last >= 0 && str[last] == ',')
    str[last] = '\0';
}

#define ONEKG_MAXCOUNT (8 * 1024 * 1024)

int *oneKGIds = NULL;
int oneKGIdCount = 0;

void read1KGIds()
/* Slurp in file of numerically sorted rs IDs into array of ints. */
{
struct lineFile *lf = lineFileOpen(oneKGenomesRsIds, TRUE);
char *words[1];
oneKGIds = needMem(ONEKG_MAXCOUNT * sizeof(int));
while (lineFileRow(lf, words))
    {
    oneKGIds[oneKGIdCount++] = lineFileNeedFullNum(lf, words, 0);
    if (oneKGIdCount >= 2 && oneKGIds[oneKGIdCount-1] <= oneKGIds[oneKGIdCount-2])
	lineFileAbort(lf, "Error: IDs must be sorted numerically and unique (use sort -nu).");
    }
}

boolean binSearchInt(int *target, int targetSize, int query)
/* Perform binary search in sorted array of integers, return TRUE if found. */
{
int low = 0, high = targetSize-1;
while (high >= low)
    {
    int mid = (low + high) >> 1;
    if (query < target[mid])
	high = mid-1;
    else if (query > target[mid])
	low = mid+1;
    else
	return TRUE;
    }
return FALSE;
}

static boolean is1000Genomes(int rsId)
/* Return TRUE if rsID appears in numerically sorted file named by -1000GenomesRsIds. */
{
if (oneKGIds == NULL)
    read1KGIds();
return binSearchInt(oneKGIds, oneKGIdCount, rsId);
}

void processValid(struct lineFile *lf, int validNum,
		  char valid[], size_t validSize, int rsId)
/* Translate and check validation status bit flags. */
{
static struct dyString *validList = NULL;
if (validNum < 0)
    validNum = 0;
if (validNum > MAX_VALID)
    lineFileAbort(lf, "Unrecognized validation number %d (max is %d).\n"
		  "If %d is valid now, "UPDATE_CODE,
		  validNum, MAX_VALID, validNum);
if (validList == NULL)
    validList = dyStringNew(validSize);
dyStringClear(validList);
if (oneKGenomesRsIds != NULL && is1000Genomes(rsId))
    {
    dyStringPrintf(validList, "%s,", validBitStrings[VALID_1KG]); // UCSC "by-1000genomes"
    validBitStringsUsed[VALID_1KG] = TRUE;
    }
if (validNum > 0)
    {
    /* Mask off each bit, most significant first.  If set, append 
     * string value to form a comma-separated list. */
    int bit;
    for (bit = VALID_BITS - 1;  bit >= 0;  bit--)
	{
	if ((validNum >> bit) & 0x1)
	    {
	    dyStringPrintf(validList, "%s,", validBitStrings[bit]);
	    validBitStringsUsed[bit] = TRUE;
	    }
	}
    }
if (isEmpty(validList->string))
    safecpy(valid, validSize, "unknown");
else
    safef(valid, validSize, "%s", validList->string);
chopTrailingComma(valid);
}

void processFunc(struct lineFile *lf, char *funcCode,
		 char func[], size_t funcSize)
/* Translate and check list of function codes. */
{
static struct dyString *funcList = NULL;
if (funcList == NULL)
    funcList = dyStringNew(funcSize);
dyStringClear(funcList);

if (isEmpty(funcCode))
    lineFileAbort(lf, "Empty (as opposed to missing) function value.\n");
if (isMissing(funcCode))
    {
    dyStringAppend(funcList, functionStrings[0]);
    functionStringsUsed[0] = TRUE;
    }
else
    {
    unsigned codes[MAX_FUNC+1];
    int codeCount = sqlUnsignedArray(funcCode, codes, ArraySize(codes));
    int i;
    for (i = 0;  i < codeCount;  i++)
	{
	if (codes[i] > MAX_FUNC)
	    lineFileAbort(lf, "Unrecognized function number %d (max is %d).\n"
			  "If %d is valid now, "UPDATE_CODE,
			  codes[i], MAX_FUNC, codes[i]);
	dyStringPrintf(funcList, "%s,", functionStrings[codes[i]]);
	functionStringsUsed[codes[i]] = TRUE;
	}
    }
safef(func, funcSize, "%s", funcList->string);
chopTrailingComma(func);
}

void processLocType(struct lineFile *lf, int locTypeNum,
		    char locType[], size_t locTypeSize)
/* Translate and check locType. */
{
if (locTypeNum < 0)
    locTypeNum = 0;
if (locTypeNum > MAX_LOCTYPE)
    lineFileAbort(lf, "Unrecognized locType number %d (max is %d).\n"
		  "If %d is valid now, "UPDATE_CODE,
		  locTypeNum, MAX_LOCTYPE, locTypeNum);
safef(locType, locTypeSize, "%s", locTypeStrings[locTypeNum]);
locTypeStringsUsed[locTypeNum] = TRUE;
}

void compileRegex(regex_t *regCompiled, const char *regexStr, int flags)
/* Compile regular expression string regexStr into regCompiled. */
{
int err = regcomp(regCompiled, regexStr, REG_EXTENDED | flags);
if (err)
    {
    char errMessage[2048];
    regerror(err, regCompiled, errMessage, sizeof(errMessage));
    errAbort("Compilation of regular expression failed.  Regex:\n%s"
	     "\nError message:\n%s", regexStr, errMessage);
    }
}

/* This regular expression describes the format of refNCBI 
 * (ContigLoc.allele), which may have multiple (base)number parts that we 
 * expand into that number of base.  The regex matches a single part. */
/* man 3 regex for POSIX lib usage; man 7 regex for regular expression info. */
const char *expansionUnit = 
    "^([ACGTN]+)?"	/* Regular bases and/or N, unless line starts with ( */
     "\\(([ACGTN])\\)"	/* A single base, in parentheses. */
     "([0-9]+)"		/* A number (how many times to repeat the base). */
     "([ACGTN]*)";	/* Maybe some regular bases after that. */

/* This is 1 (whole match) + number of () atoms above plus 1 extra (should
 * be empty): */
#define MAX_MATCHES 1 + 4 + 1

int expandUnit(struct lineFile *lf, char *refNcbiEnc,
	       regmatch_t *matches, boolean firstMatch,
	       struct dyString *expanded)
/* Given a string and an array of matches corresponding to expansionUnit and
 * its () sub-expressions, build up the expanded version of refNCBI. 
 * Returns the size of the whole expansionUnit match. */
{
char base;
char numBuf[16];
int count;
int j;
/* matches[0] is the whole expansionUnit match, but we want its 
 * components (left-flanking seq, base, number, right-flanking seq): */
regmatch_t *rm = &(matches[1]);
/* Do lots of paranoid checks to make sure that the regex is consistent
 * with our expectations and with the actual refNcbiEnc... */
if (rm->rm_so == 0)
    {
    if (firstMatch)
	dyStringAppendN(expanded, refNcbiEnc + rm->rm_so,
			rm->rm_eo - rm->rm_so);
    else
	lineFileAbort(lf, "Got left-flanking sequence for a subsequent "
		      "expansionUnit match...?  %d..%d %s",
		      (int)rm->rm_so, (int)rm->rm_eo, refNcbiEnc + rm->rm_so);
    }
else if (rm->rm_so > 0)
    lineFileAbort(lf, "How can matches[1].rm_so be %d (> 0)?", (int)rm->rm_so);
/* Get base: */
rm = &(matches[2]);
if (rm->rm_so < 0)
    errAbort("Out of sync with matches? Null match for base.");
if (rm->rm_eo != rm->rm_so + 1)
    errAbort("Length > 1 for base in ()'s ? %d..%d %s",
	     (int)rm->rm_so, (int)rm->rm_eo, refNcbiEnc + rm->rm_so);
base = refNcbiEnc[rm->rm_so];
/* Get number: */
rm = &(matches[3]);
if (rm->rm_so < 0)
    errAbort("Out of sync with matches? Null match for number.");
if (rm->rm_eo - rm->rm_so > 3)
    errAbort("Way too large number: %s", refNcbiEnc + rm->rm_so);
strncpy(numBuf, refNcbiEnc + rm->rm_so, rm->rm_eo - rm->rm_so);
numBuf[rm->rm_eo - rm->rm_so] = '\0';
count = atoi(numBuf);
if (count < 2)
    lineFileAbort(lf, "Got weird count %d from %s",
		  count, refNcbiEnc + rm->rm_so);
/* Do the expansion: */
for (j = 0;  j < count; j++)
    dyStringAppendC(expanded, base);
/* Append tail, if any. */
rm = &(matches[4]);
if (rm->rm_so >= 0)
    dyStringAppendN(expanded, refNcbiEnc + rm->rm_so,
		    rm->rm_eo - rm->rm_so);
rm = &(matches[5]);
if (rm->rm_so >= 0)
    lineFileAbort(lf, "Got too many matches for expansionUnit: %d..%d %s",
		  (int)rm->rm_so, (int)rm->rm_eo, refNcbiEnc + rm->rm_so);
return matches[0].rm_eo;
}

char *processRefAllele(struct lineFile *lf, char *refNcbiEnc, char *locType)
/* Expand compact notation in refNCBI (ContigLoc.allele).  
 * Do not free the return value! */
{
static regex_t eUnit;
static struct dyString *expanded = NULL;
regmatch_t matches[MAX_MATCHES];
if (expanded == NULL)
    {
    expanded = dyStringNew(1024);
    compileRegex(&eUnit, expansionUnit, 0);
    }

dyStringClear(expanded);
/* Common case: no expansion required; don't bother with regex. */
if (isEmpty(refNcbiEnc) || isMissing(refNcbiEnc))
    {
    dyStringAppend(expanded, "unknown");
    writeError("Missing refNCBI");
    return expanded->string;
    }
if (sameString(refNcbiEnc, "-") || !strchr(refNcbiEnc, '(') ||
    stringIn("bp insertion", refNcbiEnc))
    {
    dyStringPrintf(expanded, "%s", refNcbiEnc);
    return expanded->string;
    }

if (sameString("exact", locType) || sameString("between", locType))
    lineFileAbort(lf, "How did we get a fancy allele format with locType %s?",
		  locType);

/* Use the regular expression to parse out the pieces, then piece them
 * together, expanding where necessary. */
if (regexec(&eUnit, refNcbiEnc, MAX_MATCHES, matches, 0) == 0)
    {
    int expectedExpansions = chopString(refNcbiEnc, "(", NULL, 0) - 1;
    int expansionCount = 1;
    int matchSize = expandUnit(lf, refNcbiEnc, matches, TRUE, expanded);
    char *nextUnit = refNcbiEnc + matchSize;
    if (startsWith("(", refNcbiEnc))
	expectedExpansions++;
    while (regexec(&eUnit, nextUnit, MAX_MATCHES, matches, 0) == 0)
	{
	expansionCount++;
	matchSize = expandUnit(lf, nextUnit, matches, FALSE, expanded);
	nextUnit += matchSize;
	}
    if (! sameString(nextUnit, ""))
	lineFileAbort(lf, "Subsequent match %d failed.", expansionCount);
    if (expansionCount != expectedExpansions)
	lineFileAbort(lf, "Expected %d expansions, got %d",
		      expectedExpansions, expansionCount);
    if (verboseLevel() >= 2 || expansionCount > 1)
	verbose(2, "%d expansions: %s --> %s\n",
		expansionCount, refNcbiEnc, expanded->string);
    }
else
    lineFileAbort(lf, "Unrecognized format for refNCBI/allele: \"%s\"",
		  refNcbiEnc);
return expanded->string;
}

void checkNcbiChrStart(int ncbiChrStart)
/* Compare NCBI's lifted chrom start to ours: */
{
if (ncbiChrStart != -1 && !strstr(chr, "_hap") &&
    ncbiChrStart != chrStart)
    writeError("chromStart (%d) does not match phys_pos_from (%d).",
	       chrStart, ncbiChrStart);
}

void adjustCoords(struct lineFile *lf, int locTypeNum, char *locType,
		  char *refNCBI)
/* Using locType and the ()-expanded refNCBI, check our assumptions about
 * NCBI's 0-based, fully-closed coords and conventions for various locTypes,
 * and modify chrStart/chrEnd to match our 0-based, half-open coords and
 * conventions. */
{
if (chrEnd < chrStart)
    {
    writeError("Unexpected coords for locType \"%s\" (%d) -- "
	       "NCBI's chrStart is > chrEnd.", locType, locTypeNum);
    }
else if (sameString(locType, "between"))
    {
    /* dbSNP insertions have end=start+1 -- 2 bases long in their 0-based,
     * fully-closed coords.  We increment start so end=start -- 0 bases long
     * in our 0-based, half-open coords.. */
    if (chrEnd != chrStart + 1)
	writeError("Unexpected coords for locType \"%s\" (%d) -- "
		   "expected NCBI's chrEnd = chrStart+1.",
		   locType, locTypeNum);
    else if (! sameString(refNCBI, "-"))
	writeError("Unexpected refNCBI \"%s\" for locType \"%s\" (%d) -- "
		   "expected \"-\"", refNCBI, locType, locTypeNum);
    else
	chrStart++;
    }
else if (sameString(locType, "exact"))
    {
    /* dbSNP single-base SNPs have start=end (0-based, fully closed coords).
     * We increment end so length is 1 (in our 0-based, half open coords). */
    if (chrEnd != chrStart)
	writeError("Unexpected coords for locType \"%s\" (%d) -- "
		   "expected NCBI's chrEnd = chrStart.", locType, locTypeNum);
    else if (strlen(refNCBI) != 1)
	writeError("Expected refNCBI to be single-base for locType \"%s\" "
		   "(%d) but got \"%s\"", locType, locTypeNum, refNCBI);
    else
	chrEnd++;
    }
else if (sameString(locType, "range"))
    {
    /* dbSNP multi-base SNPs have start<end (0-based, fully closed coords).
     * We increment end to convert to our 0-based, half open coords. */
    if (chrEnd <= chrStart)
	writeError("Unexpected coords for locType \"%s\" (%d) -- "
		   "expected NCBI's chrEnd > chrStart.", locType, locTypeNum);
    else if (strlen(refNCBI) <= 1)
	writeError("Expected refNCBI to be single-base for locType \"%s\" "
		   "(%d) but got \"%s\"", locType, locTypeNum, refNCBI);
    else
	chrEnd++;
    }
else
    {
    /* The range{Insertion,Substitution,Deletion} locTypes don't have a
     * constraint on size, but do need to be converted. */
    if (!stringIn("bp insertion", refNCBI) &&
	strlen(refNCBI) != chrEnd - chrStart + 1)
	writeError("Unexpected coords for locType \"%s\" (%d) -- "
		   "expected NCBI's chrEnd == chrStart + strlen(refNCBI) - 1.",
		   locType, locTypeNum);
    else
	chrEnd ++;
    }
}

#define MAX_SNPSIZE 16 * 1024

char *processUcscAllele(struct lineFile *lf, char *refNCBI, char strand,
			struct twoBitFile *twoBit)
/* If this is an insertion, just use refNCBI; else get sequence from 
 * assembly.
 * Do not free the return value! */
{
static char refUCSC[MAX_SNPSIZE+1];
static char prevChr[256];
static struct dnaSeq *chrSeq = NULL;
static int chrSize = 0;
int snpSize = chrEnd - chrStart;
if (chrSeq == NULL || !sameString(chr, prevChr))
    {
    safef(prevChr, sizeof(prevChr), "%s", chr);
    dnaSeqFree(&chrSeq);
    chrSize = twoBitSeqSize(twoBit, chr);
    chrSeq = twoBitReadSeqFragExt(twoBit, chr, 0, 0, FALSE, NULL);
    touppers(chrSeq->dna);
    }
if (chrStart >= chrSize || chrEnd > chrSize)
    writeError("rs%d has coord > chromSize: %s:%d-%d, chromSize %d",
	       rsId, chr, chrStart, chrEnd, chrSize);
if (stringIn("bp insertion", refNCBI))
    return refNCBI;
if (snpSize > MAX_SNPSIZE)
    lineFileAbort(lf, "MAX_SNPSIZE %d exceeded: rs%d %s:%d-%d (%d)",
		  MAX_SNPSIZE, rsId, chr, chrStart, chrEnd, snpSize);
if (chrStart == chrEnd)
    /* copy whatever convention dbSNP uses for insertion */
    return refNCBI;
else
    {
    if (strlen(refNCBI) != snpSize)
	writeError("rs%d is %d bases long but refNCBI is different length: %s",
		   rsId, snpSize, refNCBI);
    strncpy(refUCSC, chrSeq->dna + chrStart, snpSize);
    refUCSC[snpSize] = '\0';
    if (! sameString(refNCBI, refUCSC))
	writeException(RefAlleleMismatch);

    /* Before dbSNP127, NCBI's ref alleles were rev-comped if strand was -.
     * Now, they should not be rev-comped, but it's interesting to note 
     * if they are. (could happen by chance if wrong single base, but still) */
    char refUCSCRC[MAX_SNPSIZE+1];
    strcpy(refUCSCRC, refUCSC);
    reverseComplement(refUCSCRC, strlen(refUCSCRC));
    if (sameString(refNCBI, refUCSCRC) &&
	!sameString(refNCBI, refUCSC))  /* Don't flag if seq = revcomp seq */
	writeException(RefAlleleRevComp);
    }
return refUCSC;
}

char *reverseComplementObserved(char *observed)
/* Rev-comp the sequence portion of an observed string. *
 * Do not free the return value! */
{
static struct dyString *myDy = NULL;

if (! startsWith("-/", observed))
    errAbort("This shouldn't be called on an observed that doesn't start "
	     "with -/ .");

if (myDy == NULL)
    myDy = dyStringNew(512);
dyStringClear(myDy);
dyStringAppend(myDy, observed);

reverseComplement(myDy->string + 2, strlen(myDy->string + 2));

return myDy->string;
}

boolean listAllEqual(struct slName *list)
/* return TRUE if list has >= 2 elements and all elements in the list are
 * the same */
{
struct slName *element = NULL;
if (list == NULL || list->next == NULL)
    return FALSE;
for (element = list->next;  element !=NULL;  element = element->next)
    if (!sameString(element->name, list->name))
        return FALSE;
return TRUE;
}

void reportCluster(char *clusterChr, int clusterPos,
		   struct slInt *idList, struct slName *observedList)
/* Determine whether the observed sequences are all the same, and print
 * the appropriate exception for all SNPs in cluster. */
{
enum exceptionType exc = listAllEqual(observedList) ? DuplicateObserved :
						      MixedObserved;
struct slInt *el;
for (el = idList;  el != NULL;  el = el->next)
    writeExceptionRetro(clusterChr, clusterPos, clusterPos, el->val, exc);
}

void checkLocType(struct lineFile *lf, char *locType)
/* Write exception if locType indicates a suboptimal alignment. */
{
if (sameString(locType, "rangeInsertion"))
    writeException(FlankMismatchGenomeLonger);
else if (sameString(locType, "rangeSubstitution"))
    writeException(FlankMismatchGenomeEqual);
else if (sameString(locType, "rangeDeletion"))
    writeException(FlankMismatchGenomeShorter);
}

void measureObserved(char *observed, char *refAllele,
		     int *retMinLen, int *retMaxLen,
		     boolean *retGotRefAllele)
/* NOTE: Results are not valid if observed isn't ^[IUPAC/-]+$ . */
/* Parse observed into /-separated sequence (or -) chunks, and note a
 * few properties of those chunks.  
 * If one of the chunks is the refAllele, its length is ignored!  --
 * so we can compare lengths of other alleles to ref.  */
{
int minLen = BIGNUM, maxLen = 0;
boolean gotRefAllele = FALSE;
char *words[128];
int wordCount, i;
char buf[2048];
safecpy(buf, sizeof(buf), observed);
wordCount = chopString(buf, "/", words, ArraySize(words));
if (wordCount < 2)
    {
    minLen = maxLen = strlen(observed);
    gotRefAllele = sameString(observed, refAllele);
    }
else
    {
    for (i=0;  i < wordCount;  i++)
	{
	if (sameString(words[i], refAllele))
	    {
	    gotRefAllele = TRUE;
	    continue;
	    }
	int len = strlen(words[i]);
	if (sameString(words[i], "-"))
	    len = 0;
	if (len > maxLen)
	    maxLen = len;
	if (len < minLen)
	    minLen = len;
	}
    }
if (retMinLen != NULL)
    *retMinLen = minLen;
if (retMaxLen != NULL)
    *retMaxLen = maxLen;
if (retGotRefAllele != NULL)
    *retGotRefAllele = gotRefAllele;
}

void checkClass(struct lineFile *lf, char *class, char *observed,
		char *refUCSC)
/* Look for some anomalous cases where the alignment's genomic span seems 
 * inconsistent with the observed alleles. */
{
if (sameString(class, "single"))
    {
    if (chrEnd == chrStart)
	writeException(SingleClassZeroSpan);
    else if (chrEnd > chrStart + 1)
	writeException(SingleClassLongerSpan);
    }
else if (sameString(class, "named"))
    {
    /* Insertion snp ==> deletion in genome & vice versa.  But watch out for
     * cases where there's a large in-del, but also the ref allele, among the
     * observed alleles. */
    char *rightParen = strstr(observed, ")/-/");
    boolean obsGotRef = FALSE;
    if (rightParen)
	measureObserved(rightParen+2, refUCSC, NULL, NULL, &obsGotRef);
    if (! obsGotRef)
	{
	if (strstr(observed, "INSERT") && chrEnd != chrStart)
	    writeException(NamedInsertionNonzeroSpan);
	else if (strstr(observed, "DELET") && chrEnd == chrStart)
	    writeException(NamedDeletionZeroSpan);
	}
    }
}


boolean isBiallelic(char *observed)
{
if (sameString(observed, "A/C")) return TRUE;
if (sameString(observed, "A/G")) return TRUE;
if (sameString(observed, "A/T")) return TRUE;
if (sameString(observed, "C/G")) return TRUE;
if (sameString(observed, "C/T")) return TRUE;
if (sameString(observed, "G/T")) return TRUE;
return FALSE;
}

boolean isTriallelic(char *observed)
{
if (sameString(observed, "A/C/G")) return TRUE;
if (sameString(observed, "A/C/T")) return TRUE;
if (sameString(observed, "A/G/T")) return TRUE;
if (sameString(observed, "C/G/T")) return TRUE;
return FALSE;
}

boolean isQuadallelic(char *observed)
{
if (sameString(observed, "A/C/G/T")) return TRUE;
return FALSE;
}

void flagIupac(char *observed)
/* Write an FYI exception if observed contains any of the 11 IUPAC
 * ambiguous base characters.  This assumes that observed is not 
 * one of the named/het/microsat/etc classes -- just bases, /, - etc. */
{
static char *iupacAmbigBases = "RYMKSWBDHVN";
#define IUPAC_AMBIG_BASE_COUNT 11
int i;
for (i = 0;  i < IUPAC_AMBIG_BASE_COUNT;  i++)
    if (strchr(observed, iupacAmbigBases[i]) != NULL)
	{
	writeException(ObservedContainsIupac);
	return;
	}
}

/* A few regexes to aid in checking observed format validity: */
#define IUPAC "ACGTRYMKSWBDHVN"
/* class=single (1) is so restrictive, it's actually better to use the 
 * is*allelic routines. */
const char *observedIndelFormat =
    "^(-|[AGCT]+)(\\/["IUPAC" ]+)*$";
const char *observedHetFormat =
    "^\\(HETEROZYGOUS\\)(\\/[ACGT])*$";
const char *observedMicrosatFormat =
    "^(\\/?\\(["IUPAC"]+\\)[0-9]*)+(\\/[0-9]+)*(\\/-)?(\\/[ACGTN]+)*$";
const char *observedNamedFormat =
    "^\\((LARGE(INSERTION|DELETION))|"
    "[0-9]+ ?BP ((INDEL|INSERTION|DELETED))?\\)"
    "\\/-(\\/[ACGT]+)*$";
const char *observedNamedOddballFormat =
    "^\\([A-Z0-9_ .:=-]+\\)?(\\/\\([A-Z0-9_ ]+\\))*" /* there's all sorts of stuff in there now */
    "(\\/-)?(\\/\\(?[A-Z0-9 ]+)*\\)*$";
/* class=no-var (6): no SNPs use this class (intended for null results). */
const char *observedMixedFormat =
    "^-\\/[ACGTN]+(\\/["IUPAC"]+)+$";
const char *observedMnpFormat =
    "^[ACGTN]+(\\/["IUPAC"]+)+$";
/* there is only one instance of iupac ambiguous */

boolean checkObservedFormat(struct lineFile *lf, char *class, char *observed)
/* For each value of class, make sure observed fits the expected pattern.
 * If not, abort because there may be something new that the developer 
 * should look into.  If we see a known oddity, write an exception. */
{
static regex_t obsIndel, obsHet, obsMicrosat, obsNamed, obsNamedOddball;
static regex_t obsMixed, obsMnp;
static boolean compiled = FALSE;
if (! compiled)
    {
    compileRegex(&obsIndel, observedIndelFormat, REG_NOSUB);
    compileRegex(&obsHet, observedHetFormat, REG_NOSUB);
    compileRegex(&obsMicrosat, observedMicrosatFormat, REG_NOSUB);
    compileRegex(&obsNamed, observedNamedFormat, REG_NOSUB);
    compileRegex(&obsNamedOddball, observedNamedOddballFormat, REG_NOSUB);
    compileRegex(&obsMixed, observedMixedFormat, REG_NOSUB);
    compileRegex(&obsMnp, observedMnpFormat, REG_NOSUB);
    compiled = TRUE;
    }

if (sameString(class, "single"))
    {
    if (isQuadallelic(observed))
        writeException(SingleClassQuadAllelic);
    else if (isTriallelic(observed))
        writeException(SingleClassTriAllelic);
    else if (! isBiallelic(observed))
	{
	writeException(ObservedWrongFormat);
	return FALSE;
	}
    }
else if (sameString(class, "deletion") || sameString(class, "insertion") ||
	 sameString(class, "in-del"))
    {
    if (sameString(observed, "lengthTooLong"))
	{
	writeException(ObservedTooLong);
	return FALSE;
	}
    else if (regexec(&obsIndel, observed, 0, NULL, 0) == 0)
	{
	/* The regex tolerates some stuff worth flagging/fixing: */
	if (strchr(observed, ' '))
	    {
	    stripChar(observed, ' ');
	    /* Since we fix this, an exception would be confusing.
	     * SNP128 & onward has only one instance of this, so just report it
	     * on stderr and tell NCBI after more serious bugs have been fixed. */
	    verbose(2, "spaces stripped from observed:\n%s\t%d\t%d\trs%d",
		    chr, chrStart, chrEnd, rsId);
	    }
	flagIupac(observed);
	}
    else
	lineFileAbort(lf, "Encountered something that doesn't fit "
		      "observedIndelFormat: %s", observed);
    }
else if (sameString(class, "het"))
    {
    if (regexec(&obsHet, observed, 0, NULL, 0) != 0)
	lineFileAbort(lf, "Encountered something that doesn't fit "
		      "observedHetFormat: %s", observed);
    }
else if (sameString(class, "microsatellite"))
    {
    if (sameString(observed, "lengthTooLong"))
	{
	writeException(ObservedTooLong);
	return FALSE;
	}
    else if (regexec(&obsMicrosat, observed, 0, NULL, 0) != 0)
	lineFileAbort(lf, "Encountered something that doesn't fit "
		      "observedMicrosatFormat: %s", observed);
    }
else if (sameString(class, "named"))
    {
    if (sameString(observed, "lengthTooLong"))
	{
	writeException(ObservedTooLong);
	return FALSE;
	}
    else if (regexec(&obsNamed, observed, 0, NULL, 0) != 0)
	{
	if (regexec(&obsNamedOddball, observed, 0, NULL, 0) == 0)
	    {
	    /* These formats, while rare, are actually OK. */
	    }
	else
	    lineFileAbort(lf, "Encountered something that doesn't fit "
			  "observedNamedFormat or observedNamedOddballFormat: "
			  "%s", observed);
	}
    }
else if (sameString(class, "no-var"))
    {
    lineFileAbort(lf, "Wow, we never got a class no-var before.  What do we "
		  "do with it?  May need to "UPDATE_CODE);
    }
else if (sameString(class, "mixed"))
    {
    if (sameString(observed, "lengthTooLong"))
	{
	writeException(ObservedTooLong);
	return FALSE;
	}
    else if (regexec(&obsMixed, observed, 0, NULL, 0) != 0)
	// Single oddball in human 130 -- not worth adding to regex and masking.
	warn("Line %d of %s: Encountered something that doesn't fit observedMixedFormat: %s",
	     lf->lineIx, lf->fileName, observed);
    flagIupac(observed);
    }
else if (sameString(class, "mnp"))
    {
    if (sameString(observed, "lengthTooLong"))
	{
	writeException(ObservedTooLong);
	return FALSE;
	}
    else if (regexec(&obsMnp, observed, 0, NULL, 0) != 0)
	lineFileAbort(lf, "Encountered something that doesn't fit "
		      "observedMnpFormat: %s", observed);
    flagIupac(observed);
    }
else if (! sameString(class, "unknown"))
    {
    lineFileAbort(lf, "Unrecognized class %s -- may need to "UPDATE_CODE,
		  class);
    }
return TRUE;
}

void checkObservedAgainstReference(struct lineFile *lf,
				   char *class, char *observed,
				   char *refAllele)
/* For relevant classes, check whether refAllele is included in
 * observed values. */
{
if ((sameString(class, "single") || sameString(class, "in-del") ||
     sameString(class, "mixed") || sameString(class, "mnp")))
    {
    boolean obsContainsRefAllele;
    measureObserved(observed, refAllele, NULL, NULL,
		    &obsContainsRefAllele);
    if (!obsContainsRefAllele)
	writeException(ObservedMismatch);
    }
}

boolean checkObserved(struct lineFile *lf, char *class, char *observed,
		      char *refAllele)
/* If there is a formatting problem, don't proceed with other checks. */
{
if (isEmpty(observed))
    lineFileAbort(lf, "Empty (as opposed to missing) observed value.\n");
if (isMissing(observed))
    {
    writeError("Missing observed value (deleted SNP?).");
    return FALSE;
    }
if (! checkObservedFormat(lf, class, observed))
    return FALSE;
checkObservedAgainstReference(lf, class, observed, refAllele);
return TRUE;
}

void adjustClass(struct lineFile *lf,
		 char *class, size_t classSize, char *observed,
		 char *locType, char *refAllele)
/* If class is in-del, and if we have enough information to conclude that
 * the SNP is an insertion or deletion, change class to insertion or
 * deletion. */
{
if (! sameString(class, "in-del"))
    errAbort("Don't call adjustClass unless class is in-del.  (got %s)",
	     class);
int span = chrEnd - chrStart;
int obsMinLen, obsMaxLen;
measureObserved(observed, refAllele, &obsMinLen, &obsMaxLen, NULL);

if (!sameString(locType, "rangeInsertion") &&
    !sameString(locType, "rangeSubstitution") &&
    !sameString(locType, "rangeDeletion"))
    {
    if (obsMaxLen < span)
	{
	/* All observed alleles (except observed refAllele) are shorter
	 * than the genomic segment ==> SNP is a deletion. */
	safecpy(class, classSize, "deletion");
	classStringsUsed[stringArrayIx("deletion", classStrings,
				       ArraySize(classStrings))] = TRUE;
	}
    else if (obsMinLen > span)
	{
	/* All observed alleles (except observed refAllele) are longer
	 * than the genomic segment ==> SNP is an insertion. */
	safecpy(class, classSize, "insertion");
	classStringsUsed[stringArrayIx("insertion", classStrings,
				       ArraySize(classStrings))] = TRUE;
	}
    }
}

void checkCluster(struct lineFile *lf, char strand, char *observed)
/* For each cluster of insertion SNPs at the same position, print an 
 * exception indicating whether the observed sequences are all the same
 * (Duplicate) or not (Mixed). */
/* This keeps internal state as static var's.  At end of program, call
 * once more with observed=NULL to handle the last cluster. */
{
#define PREVCHRSIZE 128
static char *prevChr = NULL;
static int prevPos = 0;
static struct slInt *idList = NULL;
static struct slName *observedList = NULL;
boolean sameChr = FALSE;
if (prevChr == NULL)
    {
    prevChr = needMem(PREVCHRSIZE);
    prevChr[0] = '\0';
    }

if (observed != NULL)
    {
    /* Following snpCheckCluster.c's lead, ignore if observed != -/[AGCT]+: */
    if ((!startsWith("-/", observed) ||
	 chopString(observed, "/", NULL, 0) > 2))
	return;
    if (strand == '-')
	observed = reverseComplementObserved(observed);
    if (chrStart != chrEnd)
	lineFileAbort(lf, "Expected 0-length position for insertion");
    sameChr = isNotEmpty(prevChr) && sameString(prevChr, chr);
    if (sameChr && chrStart < prevPos && observed != NULL)
	lineFileAbort(lf, "Input must be sorted by position.");
    }
if (!sameChr || chrStart > prevPos || observed == NULL)
    {
    /* New position or end of file: report previous cluster (if it was
     * really a cluster) and reset cluster state. */
    if (slCount(idList) >= 2)
	reportCluster(prevChr, prevPos, idList, observedList);
    slFreeList(&idList);
    slNameFreeList(&observedList);
    if (observed != NULL)
	{
	idList = slIntNew(rsId);
	observedList = slNameNew(observed);
	}
    }
else
    {
    /* Same position, new SNP -- accumulate cluster state. */
    struct slInt *thisId = slIntNew(rsId);
    struct slName *thisObs = slNameNew(observed);
    slAddHead(&idList, thisId);
    slAddHead(&observedList, thisObs);
    }
if (!sameChr && observed != NULL)
    {
    safecpy(prevChr, PREVCHRSIZE, chr);
    }
prevPos = chrStart;
}


void checkSubmitters(struct lineFile *lf, int submitterCount, char *submitters)
/* Make sure submitterCount matches count of comma-sep'd strings in submitters. */
{
// subtract one because of comma at end:
int checkCount = chopCommas(submitters, NULL) - 1;
if (checkCount != submitterCount)
    lineFileAbort(lf, "submitterCount %d does not match number of comma-separated "
		  "strings %d in submitters (%s).  Check doDbSnp.pl's code that "
		  "processes submitter data into ucscHandles.",
		  submitterCount, checkCount, submitters);
}

void processAlleleFreqs(struct lineFile *lf, int *pAlleleFreqCount, char *alleles,
			float *alleleNs, float *alleleFreqs)
/* Make sure alleleFreqCount matches count of comma-sep'd strings in alleles.
 * If any alleleNs are non-integer, flag exception.  If alleleFreqs don't sum to
 * 1.0, flag an error. */
{
if (*pAlleleFreqCount < 0)
    {
    *pAlleleFreqCount = 0;
    alleles[0] = '\0';
    return;
    }
int checkCount = chopCommas(alleles, NULL) - 1;
if (checkCount != *pAlleleFreqCount)
    lineFileAbort(lf, "alleleFreqCount %d does not match number of comma-separated "
		  "strings %d in alleles (%s)", *pAlleleFreqCount, checkCount, alleles);
// Length of alleleNs and alleleFreqs was already checked during parsing.
int i;
double total = 0.0;
for (i=0;  i < *pAlleleFreqCount;  i++)
    {
    double leftover = alleleNs[i] - trunc(alleleNs[i]);
    if (leftover > ALLELE_N_ROUNDING_ERROR && leftover < 1.0-ALLELE_N_ROUNDING_ERROR)
	writeException(NonIntegerChromCount);
    total += alleleFreqs[i];
    }
if (total < 1.0-ALLELE_FREQ_ROUNDING_ERROR || total > 1.0+ALLELE_FREQ_ROUNDING_ERROR)
    writeException(AlleleFreqSumNot1);
}

void processBitfields(struct lineFile *lf, int bytes[], char *bitfieldsStr, size_t size)
/* Convert link properties byte 2 bits of interest into comma-sep'd list (mysql set). */
{
if (bytes[0] < 0)
    {
    // missing data -> "unknown"
    safecpy(bitfieldsStr, size, bitfieldsStrings[0]);
    bitfieldsStringsUsed[0] = TRUE;
    }
else
    {
    static struct dyString *dy = NULL;
    if (dy == NULL)
	dy = dyStringNew(0);
    dyStringClear(dy);
    int i;
    for (i = 1;  i < MAX_BITFIELDS+1;  i++)
	{
	struct byteBitOffsets off = bitfieldsOffsets[i];
	if (bytes[off.byte] & (1 << off.bit))
	    {
	    if (dy->stringSize > 0)
		dyStringAppend(dy, ",");
	    dyStringAppend(dy, bitfieldsStrings[i]);
	    bitfieldsStringsUsed[i] = TRUE;
	    }
	}
    safecpy(bitfieldsStr, size, dy->string);
    int qualCheck = bytes[BITFIELDS_QUAL_CHECK_BYTE];
    // This should be redundant w/ObservedMismatch:
    if ((qualCheck & (1 << BITFIELDS_REF_MISMATCH)) &&
	!(exceptionBits & (1 << ObservedMismatch)))
	{
	static int complainCount = 0;
	if (complainCount < 10)
	    warn("bitfields says observed mismatch but we don't: rs%d %s %d",
		 rsId, chr, chrStart);
	complainCount++;
	}
    if (!(qualCheck & (1 << BITFIELDS_REF_MISMATCH)) &&
	(exceptionBits & (1 << ObservedMismatch)))
	{
	static int complainCount = 0;
	if (complainCount < 10)
	    warn("we say ref observed mismatch but bitfields doesn't: rs%d %s %d",
		 rsId, chr, chrStart);
	complainCount++;
	}
    }
}


// Pseudo-Autosomal Regions:
struct bed4 *parList = NULL;

void initPar()
/* If -par=file is given, parse it to get pseudoautosomal region coords. */
{
if (parFile == NULL)
    return;
struct lineFile *lf = lineFileOpen(parFile, TRUE);
char *words[5];
int wordCount;
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount < 4)
	lineFileAbort(lf, "Expecting BED with at least 4 columns; line has %d columns.",
		      wordCount);
    struct bed4 *bed = NULL;
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    bed->name = cloneString(words[3]);
    slAddHead(&parList, bed);
    }
lineFileClose(&lf);
}

/* Use an array[MAX_SNPID] of coord lists to detect multiply-mapped SNPs and to store 
 * exceptions. */
/* Hashing might be slightly more memory efficient, but this is easier and it still works. */
/* SNP130: now 18M items, max ID 74315166. */
/* SNP132: 30M items, max ID 121909398 */
/* SNP134: 62M items, max ID 179363897 */
#define MAX_SNPID 180 * 1024 * 1024
struct coords
    {
    struct coords *next;
    int chrId;
    int start;
    int end;
    unsigned int exceptions;
    };

int lastRsId = 0;
struct coords **mappings = NULL;

struct hash *chrIds = NULL;
char **idChrs = NULL;
int nextChrId = 0;
int maxChrId = 2048;

int idForChr(char *chrom)
/* To save memory, map chromosome name to integer index and vice versa. */
{
struct hashEl *hel = hashLookup(chrIds, chrom);
int id = 0;
if (hel == NULL)
    {
    id = nextChrId++;
    hel = hashAddInt(chrIds, chrom, id);
    if (id >= maxChrId)
	{
	maxChrId *= 4;
	idChrs = needLargeMemResize(idChrs, maxChrId * sizeof(char *));
	}
    idChrs[id] = hel->name;
    }
else
    id = ptToInt(hel->val);
return id;
}

void storeMapping()
/* Keep track of coordinates to which each SNP is mapped, so we can 
 * detect multiple mappings after processing all rows of input.
 * Also, for snp132Ext, store exceptions so we can add the exceptions 
 * column in the second pass.
 * Note: this must be called for each SNP after all input columns 
 * have been processed, so all exceptions are stored here. */
{
struct coords *mapping;
AllocVar(mapping);
if (chrIds == NULL)
    {
    chrIds = hashNew(19);
    AllocArray(idChrs, maxChrId);
    AllocArray(mappings, MAX_SNPID);
    }
if (rsId >= MAX_SNPID)
    errAbort("Need to increase MAX_SNPID. (%d)", rsId);
if (rsId > lastRsId)
    lastRsId = rsId;
mapping->chrId = idForChr(chr);
mapping->start = chrStart;
mapping->end   = chrEnd;
mapping->exceptions = exceptionBits;
slAddHead(&mappings[rsId], mapping);
}

void stripHapSuffix(struct coords *map, char chromBase[], size_t size)
/* Translate map->chrId into a string, store in pre-allocated chromBase, strip any _hap suffix. */
{
safecpy(chromBase, size, idChrs[map->chrId]);
if (strstr(chromBase, "_hap"))
    {
    char *p = strchr(chromBase, '_');
    *p = '\0';
    }
}

boolean sameChromDiffHaplotype(struct coords *firstMap, struct coords *newMap)
/* Return TRUE if the two maps' chroms are different haplotype versions of the same
 * physical chromosome, e.g. chr6 and chr6_mcf_hap5, as opposed to mapping to
 * different chromosomes or the same chrom and same haplotype. */
{
if (firstMap->chrId == newMap->chrId)
    return FALSE;
char firstChromBase[256];
char newChromBase[256];
stripHapSuffix(firstMap, firstChromBase, sizeof(firstChromBase));
stripHapSuffix(newMap, newChromBase, sizeof(newChromBase));
return sameString(firstChromBase, newChromBase);
}

const char *parName(struct coords *map)
/* If map falls within a Pseudo-Autosomal Region (PAR), return the name (usu. "PAR1" or "PAR2"). */
{
char *chrom = idChrs[map->chrId];
struct bed4 *par;
for (par = parList;  par != NULL;  par = par->next)
    if (sameString(chrom, par->chrom) && map->start < par->chromEnd && map->end > par->chromStart)
	return par->name;
return NULL;
}

boolean diffChromSamePAR(struct coords *firstMap, struct coords *newMap)
/* Return TRUE if both maps are in the same PAR but not on the same chromosome. */
{
if (parList == NULL)
    // not applicable to this genome
    return FALSE;
if (firstMap->chrId == newMap->chrId)
    return FALSE;
const char *firstParName = parName(firstMap);
if (firstParName == NULL)
    return FALSE;
const char *newParName = parName(newMap);
return (newParName != NULL && sameString(firstParName, newParName));
}


void reportMultipleMappings()
/* Print exceptions for SNPs that have multiple mappings to the genome; single mappings to
 * different haplotype versions of the same chrom are OK, and single mappings to both
 * chrX's and chrY's PAR are OK. */
{
if (mappings == NULL)
    // don't SEGV on empty input
    return;
int chromCounts[nextChrId];
int id, i;
for (id = 0;  id <= lastRsId;  id++)
    if (mappings[id] && mappings[id]->next)
	{
	boolean gotMult = FALSE;
	for (i = 0;  i < nextChrId;  i++)
	    chromCounts[i] = 0;
	struct coords *firstMap = mappings[id], *map;
	chromCounts[firstMap->chrId]++;
	for (map = firstMap->next;  map != NULL;  map = map->next)
	    {
	    chromCounts[map->chrId]++;
	    if ((!sameChromDiffHaplotype(firstMap, map) && !diffChromSamePAR(firstMap, map)) ||
		chromCounts[map->chrId] > 1)
		{
		gotMult = TRUE;
		break;
		}
	    }
	if (gotMult)
	    {
	    for (map = firstMap;  map != NULL;  map = map->next)
		writeExceptionRetro(idChrs[map->chrId], map->start, map->end,
				    id, MultipleAlignments);
	    }
	}
}


struct coords *mustGetMapping(char *mChr, int mStart, int mEnd, int mRsId)
/* Look up mapping record by id and position or error out. */
{
int chromId = idForChr(mChr);
struct coords *map = mappings[mRsId];
while (map != NULL &&
       (chromId != map->chrId || mStart != map->start || mEnd != map->end))
    map = map->next;
if (map == NULL)
    errAbort("Can't find mapping for rs=%d at %s\t%d\t%d", mRsId, mChr, mStart, mEnd);
return map;
}

void setExceptionBit(char *eChr, int eStart, int eEnd, int eRsId, enum exceptionType exc)
/* Look up mapping record by id and position, and set bit exc in map->exceptions.
 * Naturally this must be called only after the mapping has been stored. */
{
struct coords *map = mustGetMapping(eChr, eStart, eEnd, eRsId);
map->exceptions |= (1 << exc);
}

char *makeExceptionColVal()
/* Translate exception codes for the current SNP into comma-sep'd string 
 * for snp132Ext's exceptions col.
 * Do not free result! */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(512);
dyStringClear(dy);
struct coords *map = mustGetMapping(chr, chrStart, chrEnd, rsId);
boolean needComma = FALSE;
enum exceptionType exc;
for (exc = 0;  exc < exceptionTypeCount;  exc++)
    if (map->exceptions & (1 << exc))
	{
	if (needComma)
	    dyStringAppendC(dy, ',');
	dyStringAppend(dy, exceptionNames[exc]);
	needComma = TRUE;
	}
return dy->string;
}

static float *parseFloatArray(struct lineFile *lf, char **row, int i, int count)
/* Return an array[count] of floats parsed from row[i]. */
{
float *retArray;
int size;
sqlFloatDynamicArray(row[i], &retArray, &size);
if (size != count)
    lineFileAbort(lf, "Expected %d floats in column %d but got %d.", count, i, size);
return retArray;
}

/* Several fields are numeric unless they have a placeholder value: */
#define missingOrInt(lf, row, i) (isMissing(row[i]) ? -1 : lineFileNeedFullNum(lf, row, i))

#define missingOrDouble(lf, row, i) (isMissing(row[i]) ? 0.0 : lineFileNeedDouble(lf, row, i))

#define missingOrFloatArray(lf, row, i, count) (isMissing(row[i]) ? NULL : \
						parseFloatArray(lf, row, i, count))

void snpNcbiToUcsc(char *rawFileName, char *twoBitFileName, char *outRoot)
/* snpNcbiToUcsc - Reformat NCBI SNP field values into UCSC, and flag exceptions.. */
{
struct lineFile *lf = lineFileOpen(rawFileName, TRUE);
struct twoBitFile *twoBit = twoBitOpen(twoBitFileName);
char *row[32];
int wordCount;
int weightHisto[MAX_WEIGHT+1];
char outFileName[2048];

safef(outFileName, sizeof(outFileName), "%s.bed", outRoot);
FILE *f = mustOpen(outFileName, "w");
char tmpFileName[512];
if (snp132Ext)
    {
    // Write to tmp file on first pass, then add exceptions column on second pass:
    carefulClose(&f);
    safef(tmpFileName, sizeof(tmpFileName), "firstPass.%ld.tab", (unsigned long)getpid());
    f = mustOpen(tmpFileName, "w");
    }

initErrException(outRoot);
initStringsUsed();
initPar();
memset(weightHisto, 0, sizeof(weightHisto));

int expectedCols = snp132Ext ? 26 : 16;
while ((wordCount = lineFileChopTab(lf, row)) > 0)
    {
    /* Read in raw data: same columns as SNP table, but in NCBI's encoding: */
    lineFileExpectWords(lf, expectedCols, wordCount);
    chr            = row[0];
    chrStart       = missingOrInt(lf, row, 1);
    chrEnd         = missingOrInt(lf, row, 2);
    rsId           = missingOrInt(lf, row, 3);
    int score      = 0;
    int strandNum  = missingOrInt(lf, row, 4);
    char *refNCBI  = row[5];
    char *refUCSC;
    char *observed = row[6];
    char *molType  = row[7];
    int classNum   = missingOrInt(lf, row, 8);
    int validNum   = missingOrInt(lf, row, 9);
    double avHet   = missingOrDouble(lf, row, 10);
    double avHetSE = missingOrDouble(lf, row, 11);
    char *funcCode = row[12];
    int locTypeNum = missingOrInt(lf, row, 13);
    int weight     = missingOrInt(lf, row, 14);
    /* Extra input column for comparing NCBI's chrom coords vs. ours: */
    int ncbiChrStart = missingOrInt(lf, row, 15);
    // Additional input columns for snp132Ext:
    int submitterCount = 0;
    char *submitters = NULL;
    int alleleFreqCount = 0;
    char *alleles = NULL;
    float *alleleNs = NULL, *alleleFreqs = NULL;
    int linkPropB2 = 0, freqProp = 0, phenoProp = 0, qualCheck = 0;
    if (snp132Ext)
	{
	submitterCount = missingOrInt(lf, row, 16);
	submitters = row[17];
	alleleFreqCount = missingOrInt(lf, row, 18);
	alleles = row[19];
	alleleNs = missingOrFloatArray(lf, row, 20, alleleFreqCount);
	alleleFreqs = missingOrFloatArray(lf, row, 21, alleleFreqCount);
	linkPropB2 = missingOrInt(lf, row, 22);
	freqProp = missingOrInt(lf, row, 23);
	phenoProp = missingOrInt(lf, row, 24);
	qualCheck = missingOrInt(lf, row, 25);
	}
    skipIt = FALSE;
    exceptionBits = 0;

#ifdef DEBUG
    carefulCheckHeap();
#endif

    /* Exclude SNPs with weight==0 or 10 -- don't even do other checks. */
    if (weight == -1)
	writeError("Missing weight for rs%d", rsId);
    else if (weight > 10)
	lineFileAbort(lf, "Got weight > 10!");
    else
	weightHisto[weight]++;
    if (weight <= 0 || weight == 10)
	continue;

    /* Translate (and check) NCBI encoding into UCSC: */
    char strand = processStrand(lf, strandNum);
    char class[128];
    char valid[128];
    char func[1024];
    char locType[128];
    char refAllele[MAX_SNPSIZE+1];
    processClass(lf, classNum, class, sizeof(class));
    processValid(lf, validNum, valid, sizeof(valid), rsId);
    processFunc(lf, funcCode, func, sizeof(func));
    processLocType(lf, locTypeNum, locType, sizeof(locType));
    refNCBI = processRefAllele(lf, refNCBI, locType);
    checkNcbiChrStart(ncbiChrStart);
    adjustCoords(lf, locTypeNum, locType, refNCBI);
    refUCSC = processUcscAllele(lf, refNCBI, strand, twoBit);

    safecpy(refAllele, sizeof(refAllele), refUCSC);
    if (strand == '-')
	reverseComplement(refAllele, strlen(refAllele));
    checkLocType(lf, locType);
    checkClass(lf, class, observed, refUCSC);
    boolean obsOk = checkObserved(lf, class, observed, refAllele);
    if (obsOk && sameString(class, "in-del"))
	adjustClass(lf, class, sizeof(class), observed, locType, refAllele);

    if (sameString("between", locType) && sameString("insertion", class) &&
	!skipIt)
	checkCluster(lf, strand, observed);
    char bitfieldsStr[256];
    if (snp132Ext)
	{
	checkSubmitters(lf, submitterCount, submitters);
	processAlleleFreqs(lf, &alleleFreqCount, alleles, alleleNs, alleleFreqs);
	// Note: the order of bytes here must match bitfieldsOffsets above:
	int bytes[] = { linkPropB2, freqProp, phenoProp, qualCheck };
	processBitfields(lf, bytes, bitfieldsStr, sizeof(bitfieldsStr));
	}
    if (! skipIt)
	{
	storeMapping();
	tallyMoltype(lf, molType);
	}

    if (! skipIt)
	{
	fprintf(f, "%s\t%d\t%d\trs%d\t", chr, chrStart, chrEnd, rsId);
	fprintf(f, "%d\t%c\t%s\t%s\t", score, strand, refNCBI, refUCSC);
	fprintf(f, "%s\t%s\t%s\t%s\t", observed, molType, class, valid);
	/* Save a few bytes by not printing out tons of 0.000000's: */
	if (avHet == 0.0)
	    fprintf(f, "0.0\t");
	else
	    fprintf(f, "%lf\t", avHet);
	if (avHetSE == 0.0)
	    fprintf(f, "0.0\t");
	else
	    fprintf(f, "%lf\t", avHetSE);
	fprintf(f, "%s\t%s\t%d", func, locType, weight);
	if (snp132Ext)
	    {
	    // On first pass, write out empty exceptions column that will
	    // be replaced on second pass:
	    fputc('\t', f);
	    fprintf(f , "\t%d\t%s", submitterCount, submitters);
	    fprintf(f, "\t%d\t%s\t", alleleFreqCount, alleles);
	    int i;
	    for (i=0;  i < alleleFreqCount;  i++)
		fprintf(f, "%f,", alleleNs[i]);
	    fputc('\t', f);
	    for (i=0;  i < alleleFreqCount;  i++)
		fprintf(f, "%f,", alleleFreqs[i]);
	    fprintf(f, "\t%s", bitfieldsStr);
	    }
	fputc('\n', f);
	}
    }
/* Flush checkers' internal state at end of input: */
checkCluster(lf, '\0', NULL);
reportMultipleMappings();

int i;
for (i = 0;  i <= MAX_WEIGHT;  i++)
    {
    if (i == 0 || i == 1 || i == 2 || i == 3 || i == 10)
	verbose(1, "count of snps with weight %2d = %d\n", i, weightHisto[i]);
    else if (weightHisto[i] != 0)
	verbose(0, "UNEXPECTED count of snps with weight %2d = %d\n",
		i, weightHisto[i]);
    }

if (skipCount > 0)
    verbose(1, "Skipped %d snp mappings due to errors -- see %sErrors.bed\n",
	    skipCount, outRoot);
else
    verbose(1, "Found no errors.\n");

fclose(f);
lineFileClose(&lf);
twoBitClose(&twoBit);
finishErrException(outRoot);

int parWeightTweakCount = 0;
if (snp132Ext)
    {
    // Second pass to fill in the empty exceptions column:
    int outputColumnCount = 25;
    lf = lineFileOpen(tmpFileName, TRUE);
    f = mustOpen(outFileName, "w");
    while ((wordCount = lineFileChopTab(lf, row)) > 0)
	{
	lineFileExpectWords(lf, outputColumnCount, wordCount);
	chr = row[0];
	chrStart = atoi(row[1]);
	chrEnd = atoi(row[2]);
	rsId = atoi(row[3]+2);
	row[17] = makeExceptionColVal();
	// also, if we find PAR SNPs that don't have MultipleAlignments but have
	// weight > 1, tweak their weight to 1 because mapping uniquely to both PARs
	// counts as a unique mapping.
	if (parList != NULL)
	    {
	    struct coords *map = mustGetMapping(chr, chrStart, chrEnd, rsId);
	    boolean hasMult = (map->exceptions & (1 << MultipleAlignments));
	    if (parName(map) != NULL &&	!hasMult && atoi(row[16]) > 1)
		{
		row[16] = "1";
		parWeightTweakCount++;
		}
	    }
	fputs(row[0], f);
	for (i = 1;  i < outputColumnCount;  i++)
	    fprintf(f, "\t%s", row[i]);
	fputc('\n', f);
	}
    fclose(f);
    lineFileClose(&lf);
    unlink(tmpFileName);
    }
if (parWeightTweakCount > 0)
    verbose(1, "%d uniquely mapped PAR SNPs with weight > 1 were tweaked to weight=1\n",
	    parWeightTweakCount);
writeSnpSql(outRoot);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
oneKGenomesRsIds = optionVal("1000GenomesRsIds", oneKGenomesRsIds);
snp132Ext = optionExists("snp132Ext");
parFile = optionVal("par", parFile);
if (argc != 4)
    usage();
#ifdef DEBUG
pushCarefulMemHandler(1024 * 1024 * 1024);
#endif
snpNcbiToUcsc(argv[1], argv[2], argv[3]);
return 0;
}
