/* VCF: Variant Call Format, version 4.0 / 4.1
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-40
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-41
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dnautil.h"
#include "errAbort.h"
#include <limits.h>
#include "localmem.h"
#include "net.h"
#include "regexHelper.h"
#include "htslib/tbx.h"
#include "vcf.h"

/* Reserved but optional INFO keys: */
const char *vcfInfoAncestralAllele = "AA";
const char *vcfInfoPerAlleleGtCount = "AC";	// allele count in genotypes, for each ALT allele,
						// in the same order as listed
const char *vcfInfoAlleleFrequency = "AF";	// allele frequency for each ALT allele in the same
						// order as listed: use this when estimated from
						// primary data, not called genotypes
const char *vcfInfoNumAlleles = "AN";		// total number of alleles in called genotypes
const char *vcfInfoBaseQuality = "BQ";		// RMS base quality at this position
const char *vcfInfoCigar = "CIGAR";		// cigar string describing how to align an
						// alternate allele to the reference allele
const char *vcfInfoIsDbSnp = "DB";		// dbSNP membership
const char *vcfInfoDepth = "DP";		// combined depth across samples, e.g. DP=154
const char *vcfInfoEnd = "END";			// end position of the variant described in this
						// record (esp. for CNVs)
const char *vcfInfoIsHapMap2 = "H2";		// membership in hapmap2
const char *vcfInfoIsHapMap3 = "H3";		// membership in hapmap3
const char *vcfInfoIs1000Genomes = "1000G";	// membership in 1000 Genomes
const char *vcfInfoMappingQuality = "MQ";	// RMS mapping quality, e.g. MQ=52
const char *vcfInfoMapQual0Count = "MQ0";	// number of MAPQ == 0 reads covering this record
const char *vcfInfoNumSamples = "NS";		// Number of samples with data
const char *vcfInfoStrandBias = "SB";		// strand bias at this position
const char *vcfInfoIsSomatic = "SOMATIC";	// indicates that the record is a somatic mutation,
						// for cancer genomics
const char *vcfInfoIsValidated = "VALIDATED";	// validated by follow-up experiment

/* Reserved but optional per-genotype keys: */
const char *vcfGtGenotype = "GT";	// Integer allele indices separated by "/" (unphased)
					// or "|" (phased). Allele values are 0 for
					// reference allele, 1 for the first allele in ALT,
					// 2 for the second allele in ALT and so on.
const char *vcfGtDepth = "DP";		// Read depth at this position for this sample
const char *vcfGtFilter = "FT";		// Analogous to variant's FILTER field
const char *vcfGtLikelihoods = "GL";	// Floating point log10-scaled likelihoods for genotypes.
					// If bi-allelic, order of genotypes is AA,AB,BB
					// where A=ref and B=alt.
					// If tri-allelic, AA,AB,BB,AC,BC,CC.
const char *vcfGtPhred = "PL";		// Phred-scaled genotype likelihoods rounded to closest int
					// Same ordering as GL
const char *vcfGtConditionalQual = "GQ";// Conditional genotype quality
					// i.e. phred quality -10log_10 P(genotype call is wrong,
					// conditioned on the site's being variant)
const char *vcfGtHaplotypeQualities = "HQ";	// two phred qualities comma separated
const char *vcfGtPhaseSet = "PS";	// Set of phased genotypes to which this genotype belongs
const char *vcfGtPhasingQuality = "PQ";	// Phred-scaled P(alleles ordered wrongly in heterozygote)
const char *vcfGtExpectedAltAlleleCount = "EC";	// Typically used in association analyses


// Make definitions of reserved INFO and genotype keys, in case people take them for
// granted and don't make ##INFO headers for them:
static struct vcfInfoDef *vcfSpecInfoDefs = NULL;
static struct vcfInfoDef *vcfSpecGtFormatDefs = NULL;

static void addInfoDef(struct vcfInfoDef **pList,
		       const char *key, int fieldCount, enum vcfInfoType type, char *description)
/* Allocate and initialize an info def and add it to pList. */
{
struct vcfInfoDef *def;
AllocVar(def);
def->key = (char *)key;
def->fieldCount = fieldCount;
def->type = type;
def->description = description;
slAddHead(pList, def);
}

static void initVcfSpecInfoDefs()
/* Make linked list of INFO defs reserved in the spec. */
{
if (vcfSpecInfoDefs != NULL)
    return;
addInfoDef(&vcfSpecInfoDefs, vcfInfoAncestralAllele, 1, vcfInfoString, "Ancestral allele");
addInfoDef(&vcfSpecInfoDefs, vcfInfoPerAlleleGtCount, 1, vcfInfoInteger,
	   "Allele count in genotypes, for each ALT allele, in the same order as listed");
addInfoDef(&vcfSpecInfoDefs, vcfInfoAlleleFrequency, -1, vcfInfoFloat,
	   "Allele frequency for each ALT allele in the same order as listed: "
	   "use this when estimated from primary data, not called genotypes");
addInfoDef(&vcfSpecInfoDefs, vcfInfoNumAlleles, 1, vcfInfoInteger,
	   "Total number of alleles in called genotypes");
addInfoDef(&vcfSpecInfoDefs, vcfInfoBaseQuality, 1, vcfInfoFloat,
	   "RMS base quality at this position");
addInfoDef(&vcfSpecInfoDefs, vcfInfoCigar, 1, vcfInfoString,
	   "CIGAR string describing how to align an alternate allele to the reference allele");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIsDbSnp, 0, vcfInfoFlag, "dbSNP membership");
addInfoDef(&vcfSpecInfoDefs, vcfInfoDepth, 1, vcfInfoString,
	   "Combined depth across samples, e.g. DP=154");
addInfoDef(&vcfSpecInfoDefs, vcfInfoEnd, 1, vcfInfoInteger,
	   "End position of the variant described in this record "
	   "(especially for structural variants)");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIsHapMap2, 1, vcfInfoFlag, "Membership in HapMap 2");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIsHapMap3, 1, vcfInfoFlag, "Membership in HapMap 3");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIs1000Genomes, 1, vcfInfoFlag, "Membership in 1000 Genomes");
addInfoDef(&vcfSpecInfoDefs, vcfInfoMappingQuality, 1, vcfInfoFloat,
	   "RMS mapping quality, e.g. MQ=52");
addInfoDef(&vcfSpecInfoDefs, vcfInfoMapQual0Count, 1, vcfInfoInteger,
	   "Number of MAPQ == 0 reads covering this record");
addInfoDef(&vcfSpecInfoDefs, vcfInfoNumSamples, 1, vcfInfoInteger, "Number of samples with data");
addInfoDef(&vcfSpecInfoDefs, vcfInfoStrandBias, 1, vcfInfoString, "Strand bias at this position");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIsSomatic, 1, vcfInfoFlag,
	   "Indicates that the record is a somatic mutation, for cancer genomics");
addInfoDef(&vcfSpecInfoDefs, vcfInfoIsValidated, 1, vcfInfoFlag,
	   "Validated by follow-up experiment");
}


static void initVcfSpecGtFormatDefs()
/* Make linked list of genotype info defs reserved in spec. */
{
if (vcfSpecGtFormatDefs != NULL)
    return;
addInfoDef(&vcfSpecGtFormatDefs, vcfGtGenotype, 1, vcfInfoString,
	   "Integer allele indices separated by \"/\" (unphased) "
	   "or \"|\" (phased). Allele values are 0 for "
	   "reference allele, 1 for the first allele in ALT, "
	   "2 for the second allele in ALT and so on, or \".\" for unknown");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtDepth, 1, vcfInfoInteger,
	   "Read depth at this position for this sample");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtFilter, 1, vcfInfoString,
	   "PASS to indicate that all filters have been passed, "
	   "a semi-colon separated list of codes for filters that fail, "
	   "or \".\" to indicate that filters have not been applied");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtLikelihoods, -1, vcfInfoFloat,
	   "Genotype likelihoods comprised of comma separated floating point "
	   "log10-scaled likelihoods for all possible genotypes given the set "
	   "of alleles defined in the REF and ALT fields. ");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtPhred, -1, vcfInfoInteger,
	   "Phred-scaled genotype likelihoods rounded to the closest integer "
	   "(and otherwise defined precisely as the genotype likelihoods (GL) field)");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtConditionalQual, -1, vcfInfoFloat,
	   "phred-scaled genotype posterior probabilities "
	   "(and otherwise defined precisely as the genotype likelihoods (GL) field)"
	   "; intended to store imputed genotype probabilities");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtHaplotypeQualities, 2, vcfInfoFloat,
	   "Two comma-separated phred-scaled haplotype qualities");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtPhaseSet, 1, vcfInfoFloat,
	   "A set of phased genotypes to which this genotype belongs");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtPhasingQuality, 1, vcfInfoFloat,
	   "Phasing quality, the phred-scaled probability that alleles are ordered "
	   "incorrectly in a heterozygote (against all other members in the phase set)");
addInfoDef(&vcfSpecGtFormatDefs, vcfGtExpectedAltAlleleCount, 1, vcfInfoFloat,
	   "Expected alternate allele count (typically used in association analyses)");
}

static bool vcfFileStopDueToErrors(struct vcfFile *vcff)
/* determine if we should stop due to the number of errors */
{
return vcff->errCnt > vcff->maxErr;
}

static void vcfFileErr(struct vcfFile *vcff, char *format, ...)
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

static void vaVcfWarn(struct vcfFile *vcff, char *format, va_list args)
/* Add a little bit of info like file position to warning */
{
if (vcff->lf != NULL)
    {
    char formatPlus[1024];
    safef(formatPlus, sizeof(formatPlus), 
	"%s:%d: %s", vcff->lf->fileName, vcff->lf->lineIx, format);
    vaWarn(formatPlus, args);
    }
else
    vaWarn(format, args);
}

static void vcfFileWarn(struct vcfFile *vcff, char *format, ...)
/* Send error message to errabort stack's warn handler and abort */
{
va_list args;
va_start(args, format);
vaVcfWarn(vcff, format, args);
}

static void vcfFileErr(struct vcfFile *vcff, char *format, ...)
/* Send error message to errabort stack's warn handler and abort */
{
vcff->errCnt++;
if (vcff->maxErr == VCF_IGNORE_ERRS)
    return;
va_list args;
va_start(args, format);
vaVcfWarn(vcff, format, args);
if (vcfFileStopDueToErrors(vcff))
    errAbort("VCF: %d parser errors, quitting", vcff->errCnt);
}

static void *vcfFileAlloc(struct vcfFile *vcff, size_t size)
/* Use vcff's local mem to allocate memory. */
{
return lmAlloc( vcfFileLm(vcff), size);
}

INLINE char *vcfFileCloneStrZ(struct vcfFile *vcff, char *str, size_t size)
/* Use vcff's local mem to allocate memory for a string and copy it. */
{
return lmCloneStringZ( vcfFileLm(vcff), str, size);
}

INLINE char *vcfFileCloneStr(struct vcfFile *vcff, char *str)
/* Use vcff's local mem to allocate memory for a string and copy it. */
{
return vcfFileCloneStrZ(vcff, str, strlen(str));
}

INLINE char *vcfFileCloneSubstr(struct vcfFile *vcff, char *line, regmatch_t substr)
/* Allocate memory for and copy a substring of line. */
{
return vcfFileCloneStrZ(vcff, line+substr.rm_so, (substr.rm_eo - substr.rm_so));
}

#define vcfFileCloneVar(vcff, var) lmCloneMem( vcfFileLm(vcff), var, sizeof(var))

char *vcfFilePooledStr(struct vcfFile *vcff, char *str)
/* Allocate memory for a string from vcff's shared string pool. */
{
return hashStoreName(vcff->pool, str);  // Always stored in main pool, not reuse pool
}

static enum vcfInfoType vcfInfoTypeFromSubstr(struct vcfFile *vcff, char *line, regmatch_t substr)
/* Translate substring of line into vcfInfoType or complain. */
{
char typeWord[16];
int substrLen = substr.rm_eo - substr.rm_so;
if (substrLen > sizeof(typeWord) - 1)
    {
    vcfFileErr(vcff, "substring passed to vcfInfoTypeFromSubstr is too long.");
    return vcfInfoString;
    }
safencpy(typeWord, sizeof(typeWord), line + substr.rm_so, substrLen);
if (sameString("Integer", typeWord))
    return vcfInfoInteger;
if (sameString("Float", typeWord))
    return vcfInfoFloat;
if (sameString("Flag", typeWord))
    return vcfInfoFlag;
if (sameString("Character", typeWord))
    return vcfInfoCharacter;
if (sameString("String", typeWord))
    return vcfInfoString;
vcfFileErr(vcff, "Unrecognized type word \"%s\" in metadata line \"%s\"", typeWord, line);
return vcfInfoString;
}

// Regular expressions to check format and extract information from header lines:
static const char *fileformatRegex = "^##(file)?format=VCFv([0-9]+)(\\.([0-9]+))?$";
static const char *infoOrFormatRegex =
    "^##(INFO|FORMAT)="
    "<ID=([\\.+A-Za-z0-9_:-]+),"
    "Number=([\\.AGR]|[0-9-]+),"
    "Type=([A-Za-z]+),"
    "Description=\"?(.*)\"?>$";
static const char *filterOrAltRegex =
    "^##(FILTER|ALT)="
    "<ID=([^,]+),"
    "(Description|Type)=\"?(.*)\"?>$";
// VCF version 3.3 was different enough to warrant separate regexes:
static const char *infoOrFormatRegex3_3 =
    "^##(INFO|FORMAT)="
    "([A-Za-z0-9_:-]+),"
    "(\\.|A|G|[0-9-]+),"
    "([A-Za-z]+),"
    "\"?(.*)\"?$";
static const char *filterRegex3_3 =
    "^##(FILTER)="
    "([^,]+),"
    "()\"?(.*)\"?$";

INLINE void nonAsciiWorkaround(char *line)
// Workaround for annoying 3-byte quote marks included in some 1000 Genomes files:
{
(void)strSwapStrs(line, strlen(line)+1, "\342\200\234", "\"");
(void)strSwapStrs(line, strlen(line)+1, "\342\200\235", "\"");
}

static void parseMetadataLine(struct vcfFile *vcff, char *line)
/* Parse a VCF header line beginning with "##" that defines a metadata. */
{
char *ptr = line;
if (ptr == NULL && !startsWith(ptr, "##"))
    errAbort("Bad line passed to parseMetadataLine");
ptr += 2;
char *firstEq = strchr(ptr, '=');
if (firstEq == NULL)
    {
    if (vcff->majorVersion > 4 ||
	(vcff->majorVersion == 4 && vcff->minorVersion > 0))
	vcfFileErr(vcff, "Metadata line lacks '=': \"%s\"", line);
    return;
    }
regmatch_t substrs[8];
// Some of the metadata lines are crucial for parsing the rest of the file:
if (startsWith("##fileformat=", line) || startsWith("##format", line))
    {
    if (regexMatchSubstr(line, fileformatRegex, substrs, ArraySize(substrs)))
	{
	// substrs[2] is major version #, substrs[3] is set only if there is a minor version,
	// and substrs[4] is the minor version #.
	vcff->majorVersion = atoi(line + substrs[2].rm_so);
	if (substrs[3].rm_so != -1)
	    vcff->minorVersion = atoi(line + substrs[4].rm_so);
	}
    else
	vcfFileErr(vcff, "##fileformat line does not match expected pattern /%s/: \"%s\"",
		   fileformatRegex, line);
    }
else if (startsWith("##INFO=", line) || startsWith("##FORMAT=", line))
    {
    boolean isInfo = startsWith("##INFO=", line);
    nonAsciiWorkaround(line);
    if (regexMatchSubstr(line, infoOrFormatRegex, substrs, ArraySize(substrs)) ||
	regexMatchSubstr(line, infoOrFormatRegex3_3, substrs, ArraySize(substrs)))
	// substrs[2] is ID/key, substrs[3] is Number, [4] is Type and [5] is Description.
	{
	struct vcfInfoDef *def = vcfFileAlloc(vcff, sizeof(struct vcfInfoDef));
	def->key = vcfFileCloneSubstr(vcff, line, substrs[2]);
	char *number = vcfFileCloneSubstr(vcff, line, substrs[3]);
	if (sameString(number, ".") || sameString(number, "A") || sameString(number, "G"))
	    // A is #alts which varies line-to-line; "G" is #genotypes which we haven't
	    // yet seen.  Why is there a G here -- shouldn't such attributes go in the
	    // genotype columns?
	    def->fieldCount = -1;
	else
	    def->fieldCount = atoi(number);
	def->type = vcfInfoTypeFromSubstr(vcff, line, substrs[4]);
	// greedy regex pulls in end quote, trim if found:
	if (line[substrs[5].rm_eo-1] == '"')
	    line[substrs[5].rm_eo-1] = '\0';
	def->description = vcfFileCloneSubstr(vcff, line, substrs[5]);
        char *p = NULL;
        if ((p = strstr(def->description, "\",Source=\"")) ||
            (p = strstr(def->description, "\",Version=\"")))
            *p = '\0';
	slAddHead((isInfo ? &(vcff->infoDefs) : &(vcff->gtFormatDefs)), def);
	}
    else
	vcfFileErr(vcff, "##%s line does not match expected pattern /%s/ or /%s/: \"%s\"",
		   (isInfo ? "INFO" : "FORMAT"), infoOrFormatRegex, infoOrFormatRegex3_3, line);
    }
else if (startsWith("##FILTER=", line) || startsWith("##ALT=", line))
    {
    boolean isFilter = startsWith("##FILTER", line);
    if (regexMatchSubstr(line, filterOrAltRegex, substrs, ArraySize(substrs)) ||
	regexMatchSubstr(line, filterRegex3_3, substrs, ArraySize(substrs)))
	{
	// substrs[2] is ID/key, substrs[4] is Description.
	struct vcfInfoDef *def = vcfFileAlloc(vcff, sizeof(struct vcfInfoDef));
	def->key = vcfFileCloneSubstr(vcff, line, substrs[2]);
	// greedy regex pulls in end quote, trim if found:
	if (line[substrs[4].rm_eo-1] == '"')
	    line[substrs[4].rm_eo-1] = '\0';
	def->description = vcfFileCloneSubstr(vcff, line, substrs[4]);
	slAddHead((isFilter ? &(vcff->filterDefs) : &(vcff->altDefs)), def);
	}
    else
	{
	if (isFilter)
	    vcfFileErr(vcff, "##FILTER line does not match expected pattern /%s/ or /%s/: \"%s\"",
		       filterOrAltRegex, filterRegex3_3, line);
	else
	    vcfFileErr(vcff, "##ALT line does not match expected pattern /%s/: \"%s\"",
		       filterOrAltRegex, line);
	}
    }
}

static void expectColumnName2(struct vcfFile *vcff, char *exp1, char *exp2, char *words[], int ix)
/* Every file must include a header naming the columns, though most column names are
 * fixed; make sure the names of fixed columns are as expected. */
{
if (! sameString(exp1, words[ix]))
    {
    if (exp2 == NULL)
	vcfFileErr(vcff, "Expected column %d's name in header to be \"%s\" but got \"%s\"",
		   ix+1, exp1, words[ix]);
    else if (! sameString(exp2, words[ix]))
	vcfFileErr(vcff, "Expected column %d's name in header to be \"%s\"  or \"%s\" "
		   "but got \"%s\"", ix+1, exp1, exp2, words[ix]);
    }
}

#define expectColumnName(vcff, exp, words, ix) expectColumnName2(vcff, exp, NULL, words, ix)

// There might be a whole lot of genotype columns...
#define VCF_MAX_COLUMNS 16 * 1024

char *vcfDefaultHeader = "#CHROM POS ID REF ALT QUAL FILTER INFO";
/* Default header if we have none. */

static void parseColumnHeaderRow(struct vcfFile *vcff, char *line)
/* Make sure column names are as we expect, and store genotype sample IDs if any are given. */
{
char *words[VCF_MAX_COLUMNS];
int wordCount = chopLine(line+1, words);
if (wordCount >= VCF_MAX_COLUMNS)
    vcfFileErr(vcff, "header contains at least %d columns; "
	       "VCF_MAX_COLUMNS may need to be increased in vcf.c!", VCF_MAX_COLUMNS);
expectColumnName(vcff, "CHROM", words, 0);
expectColumnName(vcff, "POS", words, 1);
expectColumnName(vcff, "ID", words, 2);
expectColumnName(vcff, "REF", words, 3);
expectColumnName(vcff, "ALT", words, 4);
expectColumnName2(vcff, "QUAL", "PROB", words, 5);
expectColumnName(vcff, "FILTER", words, 6);
expectColumnName(vcff, "INFO", words, 7);
if (wordCount > 8)
    {
    expectColumnName(vcff, "FORMAT", words, 8);
    if (wordCount < 10)
	vcfFileErr(vcff, "FORMAT column is given, but no sample IDs for genotype columns...?");
    vcff->genotypeCount = (wordCount - 9);
    vcff->genotypeIds = vcfFileAlloc(vcff, vcff->genotypeCount * sizeof(char *));
    int i;
    for (i = 9;  i < wordCount;  i++)
	vcff->genotypeIds[i-9] = vcfFileCloneStr(vcff, words[i]);
    }
}

struct vcfFile *vcfFileNew()
/* Return a new, empty vcfFile object. */
{
struct vcfFile *vcff = NULL;
AllocVar(vcff);
vcff->pool = hashNew(0);
vcff->reusePool = NULL;  // Must explicitly request a separate record pool
return vcff;
}

void vcfFileMakeReusePool(struct vcfFile *vcff, int initialSize)
// Creates a separate memory pool for records.  Establishing this pool allows
// using vcfFileFlushRecords to abandon previously read records and free
// the associated memory. Very useful when reading an entire file in batches.
{
assert(vcff->reusePool == NULL); // don't duplicate this
vcff->reusePool = lmInit(initialSize);
}

void vcfFileFlushRecords(struct vcfFile *vcff)
// Abandons all previously read vcff->records and flushes the reuse pool (if it exists).
// USE WITH CAUTION.  All previously allocated record pointers are now invalid.
{
if (vcff->reusePool != NULL)
    {
    size_t poolSize = lmSize(vcff->reusePool);
    //if (poolSize > (48 * 1024 * 1024))
    //    printf("\nReuse pool %ld of %ld unused\n",lmAvailable(vcff->reusePool),poolSize);
    lmCleanup(&vcff->reusePool);
    vcff->reusePool = lmInit(poolSize);
    }
vcff->records = NULL;
}

static struct vcfFile *vcfFileHeaderFromLineFile(struct lineFile *lf, int maxErr)
/* Parse a VCF file into a vcfFile object.  If maxErr not zero, then
 * continue to parse until this number of error have been reached.  A maxErr
 * less than zero does not stop and reports all errors.
 * Set maxErr to VCF_IGNORE_ERRS for silence */
{
initVcfSpecInfoDefs();
initVcfSpecGtFormatDefs();
if (lf == NULL)
    return NULL;
struct vcfFile *vcff = vcfFileNew();
vcff->lf = lf;
vcff->fileOrUrl = vcfFileCloneStr(vcff, lf->fileName);
vcff->maxErr = (maxErr < 0) ? INT_MAX : maxErr;

struct dyString *dyHeader = dyStringNew(1024);
char *line = NULL;
// First, metadata lines beginning with "##":
while (lineFileNext(lf, &line, NULL) && startsWith("##", line))
    {
    dyStringAppend(dyHeader, line);
    dyStringAppendC(dyHeader, '\n');
    parseMetadataLine(vcff, line);
    }
slReverse(&(vcff->infoDefs));
slReverse(&(vcff->filterDefs));
slReverse(&(vcff->gtFormatDefs));
// Did we get the bare minimum VCF header with supported version?
if (vcff->majorVersion == 0)
    {
    vcfFileWarn(vcff, "missing ##fileformat= header line?  Assuming 4.1.");
    vcff->majorVersion = 4;
    vcff->minorVersion = 1;
    }
if ((vcff->majorVersion != 4 || vcff->minorVersion < 0 || vcff->minorVersion > 3) &&
    (vcff->majorVersion != 3))
    vcfFileErr(vcff, "VCFv%d.%d not supported -- only v3.*, v4.0 - v4.3",
	       vcff->majorVersion, vcff->minorVersion);
// Next, one header line beginning with single "#" that names the columns:
if (line == NULL)
    // EOF after metadata
    return vcff;
char headerLineBuf[256];
if (line[0] != '#')
    {
    lineFileReuse(lf);
    vcfFileWarn(vcff, "Expected to find # followed by column names (\"#CHROM POS ...\"), "
	       "assuming default VCF 4.1 columns");
    safef(headerLineBuf, sizeof(headerLineBuf), "%s", vcfDefaultHeader);
    line = headerLineBuf;
    }
dyStringAppend(dyHeader, line);
dyStringAppendC(dyHeader, '\n');
parseColumnHeaderRow(vcff, line);
vcff->headerString = dyStringCannibalize(&dyHeader);
return vcff;
}

struct vcfFile *vcfFileFromHeader(char *name, char *headerString, int maxErr)
/* Parse the VCF header string into a vcfFile object with no rows.
 * name is for error reporting.
 * If maxErr is non-negative then continue to parse until maxErr+1 errors have been found.
 * A maxErr less than zero does not stop and reports all errors.
 * Set maxErr to VCF_IGNORE_ERRS for silence. */
{
struct lineFile *lf = lineFileOnString(name, TRUE, cloneString(headerString));
return vcfFileHeaderFromLineFile(lf, maxErr);
}

#define VCF_MAX_INFO (4*1024)

static void parseRefAndAlt(struct vcfFile *vcff, struct vcfRecord *record, char *ref, char *alt)
/* Make an array of alleles, ref first, from the REF and comma-sep'd ALT columns.
 * Use the length of the reference sequence to set record->chromEnd.
 * Note: this trashes the alt argument, since this is expected to be its last use. */
{
char *altAlleles[VCF_MAX_INFO];
int altCount = chopCommas(alt, altAlleles);
record->alleleCount = 1 + altCount;
record->alleles = vcfFileAlloc(vcff, record->alleleCount * sizeof(record->alleles[0]));
record->alleles[0] = vcfFilePooledStr(vcff, ref);
int i;
for (i = 0;  i < altCount;  i++)
    record->alleles[1+i] = vcfFilePooledStr(vcff, altAlleles[i]);
int refLen = strlen(ref);
if (refLen == dnaFilteredSize(ref))
    record->chromEnd = record->chromStart + refLen;
}

static void parseFilterColumn(struct vcfFile *vcff, struct vcfRecord *record, char *filterStr)
/* Transform ;-separated filter codes into count + string array. */
{
// We don't want to modify something allocated with vcfFilePooledStr because that uses
// hash element names for storage!  So don't make a vcfFilePooledStr copy of filterStr and
// chop that; instead, chop a temp string and pool the words separately.
static struct dyString *tmp = NULL;
if (tmp == NULL)
    tmp = dyStringNew(0);
dyStringClear(tmp);
dyStringAppend(tmp, filterStr);
record->filterCount = countChars(filterStr, ';') + 1;
record->filters = vcfFileAlloc(vcff, record->filterCount * sizeof(char **));
(void)chopByChar(tmp->string, ';', record->filters, record->filterCount);
int i;
for (i = 0;  i < record->filterCount;  i++)
    record->filters[i] = vcfFilePooledStr(vcff, record->filters[i]);
}

struct vcfInfoDef *vcfInfoDefForKey(struct vcfFile *vcff, const char *key)
/* Return infoDef for key, or NULL if it wasn't specified in the header or VCF spec. */
{
struct vcfInfoDef *def;
// I expect there to be fairly few definitions (less than a dozen) so
// I'm just doing a linear search not hash:
for (def = vcff->infoDefs;  def != NULL;  def = def->next)
    {
    if (sameString(key, def->key))
	return def;
    }
for (def = vcfSpecInfoDefs;  def != NULL;  def = def->next)
    {
    if (sameString(key, def->key))
	return def;
    }
return NULL;
}

static enum vcfInfoType typeForInfoKey(struct vcfFile *vcff, const char *key)
/* Look up the type of INFO component key, in the definitions from the header,
 * and failing that, from the keys reserved in the spec. */
{
struct vcfInfoDef *def = vcfInfoDefForKey(vcff, key);
return def ? def->type : vcfInfoString;
}

static int parseInfoValue(struct vcfRecord *record, char *infoKey, enum vcfInfoType type,
			  char *valStr, union vcfDatum **pData, bool **pMissingData)
/* Parse a comma-separated list of values into array of union vcfInfoDatum and return count. */
{
char *valWords[VCF_MAX_INFO];
int count = chopCommas(valStr, valWords);
struct vcfFile *vcff = record->file;
union vcfDatum *data = vcfFileAlloc(vcff, count * sizeof(union vcfDatum));
bool *missingData = vcfFileAlloc(vcff, count * sizeof(*missingData));
int j;
for (j = 0;  j < count;  j++)
    {
    if (type != vcfInfoString && type != vcfInfoCharacter && sameString(valWords[j], "."))
	missingData[j] = TRUE;
    switch (type)
	{
	case vcfInfoInteger:
	    data[j].datInt = atoi(valWords[j]);
	    break;
	case vcfInfoFloat:
	    data[j].datFloat = atof(valWords[j]);
	    break;
	case vcfInfoFlag:
	    // Flag key might have a value in older VCFs e.g. 3.2's DB=0, DB=1
	    data[j].datString = vcfFilePooledStr(vcff, valWords[j]);
	    break;
	case vcfInfoCharacter:
	    data[j].datChar = valWords[j][0];
	    break;
	case vcfInfoString:
	    data[j].datString = vcfFilePooledStr(vcff, valWords[j]);
	    break;
	default:
	    errAbort("invalid vcfInfoType (uninitialized?) %d", type);
	    break;
	}
    }
// If END is given, use it as chromEnd:
if (sameString(infoKey, vcfInfoEnd))
    record->chromEnd = data[0].datInt;
*pData = data;
*pMissingData = missingData;
return count;
}

static void parseInfoColumn(struct vcfFile *vcff, struct vcfRecord *record, char *string)
/* Translate string into array of vcfInfoElement. */
{
if (sameString(string, "."))
    {
    record->infoCount = 0;
    return;
    }
char *elWords[VCF_MAX_INFO];
record->infoCount = chopByChar(string, ';', elWords, ArraySize(elWords));
if (record->infoCount >= VCF_MAX_INFO)
    vcfFileErr(vcff, "INFO column contains at least %d elements; "
	       "VCF_MAX_INFO may need to be increased in vcf.c!", VCF_MAX_INFO);
record->infoElements = vcfFileAlloc(vcff, record->infoCount * sizeof(struct vcfInfoElement));
char *emptyString = vcfFilePooledStr(vcff, "");
int i;
for (i = 0;  i < record->infoCount;  i++)
    {
    char *elStr = elWords[i];
    char *eq = strchr(elStr, '=');
    struct vcfInfoElement *el = &(record->infoElements[i]);
    if (eq == NULL)
	{
	el->key = vcfFilePooledStr(vcff, elStr);
	enum vcfInfoType type = typeForInfoKey(vcff, el->key);
	if (type != vcfInfoFlag)
	    {
	    struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
	    // Complain only if we are expecting a particular number of values for this keyword:
	    if (def != NULL && def->fieldCount >= 0)
		vcfFileErr(vcff, "Missing = after key in INFO element: \"%s\" (type=%d)",
			   elStr, type);
	    if (type == vcfInfoString)
		{
		el->values = vcfFileAlloc(vcff, sizeof(union vcfDatum));
		el->values[0].datString = emptyString;
		}
	    }
	continue;
	}
    *eq = '\0';
    el->key = vcfFilePooledStr(vcff, elStr);
    enum vcfInfoType type = typeForInfoKey(vcff, el->key);
    char *valStr = eq+1;
    el->count = parseInfoValue(record, el->key, type, valStr, &(el->values), &(el->missingData));
    if (el->count >= VCF_MAX_INFO)
	vcfFileErr(vcff, "A single element of the INFO column has at least %d values; "
	       "VCF_MAX_INFO may need to be increased in vcf.c!", VCF_MAX_INFO);
    }
}

struct vcfRecord *vcfRecordFromRow(struct vcfFile *vcff, char **words)
/* Parse words from a VCF data line into a VCF record structure. */
{
struct vcfRecord *record = vcfFileAlloc(vcff, sizeof(struct vcfRecord));
record->file = vcff;
record->chrom = vcfFilePooledStr(vcff, words[0]);
record->chromStart = lineFileNeedNum(vcff->lf, words, 1) - 1;
// chromEnd may be overwritten by parseRefAndAlt and parseInfoColumn.
record->chromEnd = record->chromStart+1;
record->name = vcfFilePooledStr(vcff, words[2]);
parseRefAndAlt(vcff, record, words[3], words[4]);
record->qual = vcfFilePooledStr(vcff, words[5]);
parseFilterColumn(vcff, record, words[6]);
parseInfoColumn(vcff, record, words[7]);
if (vcff->genotypeCount > 0)
    {
    record->format = vcfFilePooledStr(vcff, words[8]);
    record->genotypeUnparsedStrings = vcfFileAlloc(vcff,
						   vcff->genotypeCount * sizeof(char *));
    int i;
    // Don't bother actually parsing all these until & unless we need the info:
    for (i = 0;  i < vcff->genotypeCount;  i++)
	record->genotypeUnparsedStrings[i] = vcfFileCloneStr(vcff, words[9+i]);
    }
return record;
}

static int checkWordCount(struct vcfFile *vcff, char **words, int wordCount)
// Compensate for error in 1000 Genomes Phase 1 file
// ALL.chr21.integrated_phase1_v3.20101123.snps_indels_svs.genotypes.vcf.gz
// which has some lines that have an extra "\t" at the end of line,
// causing the wordCount to be too high by 1:
{
int expected = 8;
if (vcff->genotypeCount > 0)
    expected = 9 + vcff->genotypeCount;
if (wordCount == expected+1 && words[expected][0] == '\0')
    wordCount--;
lineFileExpectWords(vcff->lf, expected, wordCount);
return wordCount;
}

struct vcfRecord *vcfNextRecord(struct vcfFile *vcff)
/* Parse the words in the next line from vcff into a vcfRecord. Return NULL at end of file.
 * Note: this does not store record in vcff->records! */
{
char *words[VCF_MAX_COLUMNS];
int wordCount;
if ((wordCount = lineFileChopTab(vcff->lf, words)) <= 0)
    return NULL;
wordCount = checkWordCount(vcff, words, wordCount);
return vcfRecordFromRow(vcff, words);
}

static boolean noAltAllele(char **alleles, int alleleCount)
/* Return true if there is no alternate allele (missing value ".") or the given alternate allele
 * is the same as the reference allele. */
{
return (alleleCount == 2 &&
        (sameString(alleles[0], alleles[1]) || sameString(".", alleles[1])));
}

static boolean allelesHavePaddingBase(char **alleles, int alleleCount)
/* Examine alleles to see if they either a) all start with the same base or
 * b) include a symbolic or 0-length allele.  In either of those cases, there
 * must be an initial padding base that we'll need to trim from non-symbolic
 * alleles. */
{
if (sameString(alleles[0], "-"))
    return FALSE;
else if (noAltAllele(alleles, alleleCount))
    // Don't trim assertion of no change (ref == alt)
    return FALSE;
boolean hasPaddingBase = TRUE;
char firstBase = '\0';
if (isAllNt(alleles[0], strlen(alleles[0])))
    firstBase = alleles[0][0];
int i;
for (i = 1;  i < alleleCount;  i++)
    {
    if (sameString(alleles[i], "-"))
        {
        hasPaddingBase = FALSE;
        break;
        }
    else if (isAllNt(alleles[i], strlen(alleles[i])))
	{
	if (firstBase == '\0')
	    firstBase = alleles[i][0];
	if (alleles[i][0] != firstBase)
	    // Different first base implies unpadded alleles.
	    hasPaddingBase = FALSE;
	}
    else if (sameString(alleles[i], "<X>") || sameString(alleles[i], "<*>"))
        {
        // Special case for samtools mpileup "<X>" or gVCF "<*>" (no alternate allele observed) --
        // being symbolic doesn't make this an indel and ref base is not necessarily padded.
        hasPaddingBase = FALSE;
        }
    else
	{
	// Symbolic ALT allele: Indel, so REF must have the padding base.
	hasPaddingBase = TRUE;
	break;
	}
    }
return hasPaddingBase;
}

unsigned int vcfRecordTrimIndelLeftBase(struct vcfRecord *rec)
/* For indels, VCF includes the left neighboring base; for example, if the alleles are
 * AA/- following a G base, then the VCF record will start one base to the left and have
 * "GAA" and "G" as the alleles.  Also, if any alt allele is symbolic (e.g. <DEL>) then
 * the ref allele must have a padding base.
 * That is not nice for display for two reasons:
 * 1. Indels appear one base wider than their dbSNP entries.
 * 2. In pgSnp display mode, the two alleles are always the same color.
 * However, for hgTracks' mapBox we need the correct chromStart for identifying the
 * record in hgc -- so return the original chromStart. */
{
unsigned int chromStartOrig = rec->chromStart;
struct vcfFile *vcff = rec->file;
if (rec->alleleCount > 1)
    {
    boolean hasPaddingBase = allelesHavePaddingBase(rec->alleles, rec->alleleCount);
    if (hasPaddingBase)
	{
	rec->chromStart++;
	int i;
	for (i = 0;  i < rec->alleleCount;  i++)
	    {
	    if (rec->alleles[i][1] == '\0')
		rec->alleles[i] = vcfFilePooledStr(vcff, "-");
	    else if (isAllNt(rec->alleles[i], strlen(rec->alleles[i])))
		rec->alleles[i] = vcfFilePooledStr(vcff, rec->alleles[i]+1);
	    else // don't trim first character of symbolic allele
		rec->alleles[i] = vcfFilePooledStr(vcff, rec->alleles[i]);
	    }
	}
    }
return chromStartOrig;
}

static boolean allEndsGEStartsAndIdentical(char **starts, char **ends, int count)
/* Given two arrays with <count> elements, return true if all strings in ends[] are
 * greater than or equal to the corresponding strings in starts[], and all ends[]
 * have the same char. */
{
int i;
char refEnd = ends[0][0];
for (i = 0;  i < count;  i++)
    {
    if (ends[i] < starts[i] || ends[i][0] != refEnd)
	return FALSE;
    }
return TRUE;
}

static int countIdenticalBasesRight(char **alleles, int alCount)
/* Return the number of bases that are identical at the end of each allele (usually 0). */
{
if (noAltAllele(alleles, alCount))
    // Don't trim assertion of no change (ref == alt)
    return 0;
char *alleleEnds[alCount];
int i;
for (i = 0;  i < alCount;  i++)
    {
    int alLen = strlen(alleles[i]);
    // If any allele is symbolic, don't try to trim.
    if (sameString(alleles[i], "-") ||
        !isAllNt(alleles[i], alLen))
	return 0;
    alleleEnds[i] = alleles[i] + alLen-1;
    }
int trimmedBases = 0;
while (allEndsGEStartsAndIdentical(alleles, alleleEnds, alCount))
    {
    trimmedBases++;
    // Trim identical last base of alleles and move alleleEnds[] items back.
    for (i = 0;  i < alCount;  i++)
	alleleEnds[i]--;
    }
return trimmedBases;
}

unsigned int vcfRecordTrimAllelesRight(struct vcfRecord *rec)
/* Some tools output indels with extra base to the right, for example ref=ACC, alt=ACCC
 * which should be ref=A, alt=AC.  When the extra bases make the variant extend from an
 * intron (or gap) into an exon, it can cause a false appearance of a frameshift.
 * To avoid this, when all alleles have identical base(s) at the end, trim all of them,
 * and update rec->chromEnd.
 * For hgTracks' mapBox we need the correct chromStart for identifying the record in hgc,
 * so return the original chromEnd. */
{
unsigned int chromEndOrig = rec->chromEnd;
int alCount = rec->alleleCount;
char **alleles = rec->alleles;
int trimmedBases = countIdenticalBasesRight(alleles, alCount);
if (trimmedBases > 0)
    {
    struct vcfFile *vcff = rec->file;
    // Allocate new pooled strings for the trimmed alleles.
    int i;
    for (i = 0;  i < alCount;  i++)
	{
	int alLen = strlen(alleles[i]); // alLen >= trimmedBases > 0
	char trimmed[alLen+1];
	safencpy(trimmed, sizeof(trimmed), alleles[i], alLen - trimmedBases);
	if (isEmpty(trimmed))
	    safencpy(trimmed, sizeof(trimmed), "-", 1);
	alleles[i] = vcfFilePooledStr(vcff, trimmed);
	}
    rec->chromEnd -= trimmedBases;
    }
return chromEndOrig;
}

static boolean chromsMatch(char *chromA, char *chromB)
// Return TRUE if chromA and chromB are non-NULL and identical, possibly ignoring
// "chr" at the beginning of one but not the other.
{
if (chromA == NULL || chromB == NULL)
    return FALSE;
char *chromAPlus = startsWith("chr", chromA) ? chromA+3 : chromA;
char *chromBPlus = startsWith("chr", chromB) ? chromB+3 : chromB;
return sameString(chromAPlus, chromBPlus);
}

static struct vcfRecord *vcfParseData(struct vcfFile *vcff, char *chrom, int start, int end,
				      int maxRecords)
// Given a vcfFile into which the header has been parsed, and whose
// lineFile is positioned at the beginning of a data row, parse and
// return all data rows (in region, if chrom is non-NULL) from lineFile,
// up to maxRecords.
{
if (vcff == NULL)
    return NULL;
int recCount = 0;
struct vcfRecord *records = NULL;
struct vcfRecord *record;
while ((record = vcfNextRecord(vcff)) != NULL)
    {
    if (maxRecords >= 0 && recCount >= maxRecords)
        break;
    if (chrom == NULL)
	{
	slAddHead(&records, record);
	recCount++;
	}
    else if (chromsMatch(chrom, record->chrom))
	{
	if (end > 0 && record->chromStart >= end)
	    break;
	else if (record->chromEnd > start)
	    {
	    slAddHead(&records, record);
	    recCount++;
	    }
	}
    }
slReverse(&records);
return records;
}

struct vcfFile *vcfFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
			       int maxErr, int maxRecords, boolean parseAll)
/* Open fileOrUrl and parse VCF header; return NULL if unable.
 * If chrom is non-NULL, scan past any variants that precede {chrom, chromStart}.
 * Note: this is very inefficient -- it's better to use vcfTabix if possible!
 * If parseAll, then read in all lines in region, parse and store in
 * vcff->records; if maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence */
{
struct lineFile *lf = NULL;
if (startsWith("http://", fileOrUrl) || startsWith("ftp://", fileOrUrl) ||
    startsWith("https://", fileOrUrl))
    lf = netLineFileOpen(fileOrUrl);
else
    lf = lineFileMayOpen(fileOrUrl, TRUE);
struct vcfFile *vcff = vcfFileHeaderFromLineFile(lf, maxErr);
if (vcff && chrom != NULL)
    {
    char *line = NULL;
    while (lineFileNextReal(vcff->lf, &line))
	{
	char lineCopy[strlen(line)+1];
	safecpy(lineCopy, sizeof(lineCopy), line);
	char *words[VCF_MAX_COLUMNS];
	int wordCount = chopTabs(lineCopy, words);
	wordCount = checkWordCount(vcff, words, wordCount);
	struct vcfRecord *record = vcfRecordFromRow(vcff, words);
	if (chromsMatch(chrom, record->chrom))
	    {
	    if (record->chromEnd < start)
		continue;
	    else
		{
		lineFileReuse(vcff->lf);
		break;
		}
	    }
	}
    }
if (vcff && parseAll)
    {
    vcff->records = vcfParseData(vcff, chrom, start, end, maxRecords);
    lineFileClose(&(vcff->lf)); // Not sure why it is closed.  Angie?
    }
return vcff;
}

struct vcfFile *vcfTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
				    int maxErr, int maxRecords)
/* Open a VCF file that has been compressed and indexed by tabix and
 * parse VCF header, or return NULL if unable.  If chrom is non-NULL,
 * seek to the position range and parse all lines in range into
 * vcff->records.  If maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence */
{
return vcfTabixFileAndIndexMayOpen(fileOrUrl, NULL, chrom, start, end, maxErr, maxRecords);
}

struct vcfFile *vcfTabixFileAndIndexMayOpen(char *fileOrUrl, char *tbiFileOrUrl, char *chrom, int start, int end,
				    int maxErr, int maxRecords)
/* Open a VCF file that has been compressed and indexed by tabix and
 * parse VCF header, or return NULL if unable. tbiFileOrUrl can be NULL.
 * If chrom is non-NULL, seek to the position range and parse all lines in
 * range into vcff->records.  If maxErr >= zero, then continue to parse until
 * there are maxErr+1 errors.  A maxErr less than zero does not stop
 * and reports all errors. Set maxErr to VCF_IGNORE_ERRS for silence */
{
struct lineFile *lf = lineFileTabixAndIndexMayOpen(fileOrUrl, tbiFileOrUrl, TRUE);
if (lf == NULL)
    return NULL;
struct vcfFile *vcff = vcfFileHeaderFromLineFile(lf, maxErr);
if (vcff == NULL)
    return NULL;
if (isNotEmpty(chrom) && start != end)
    {
    if (lineFileSetTabixRegion(lf, chrom, start, end))
        vcff->records = vcfParseData(vcff, NULL, 0, 0, maxRecords);
    lineFileClose(&(vcff->lf)); // file is all read in so we close it
    }

return vcff;
}

int vcfRecordCmp(const void *va, const void *vb)
/* Compare to sort based on position. */
{
const struct vcfRecord *a = *((struct vcfRecord **)va);
const struct vcfRecord *b = *((struct vcfRecord **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd; // shortest first
if (dif == 0)
    dif = strcmp(a->name, b->name);  // finally by name
return dif;
}

int vcfTabixBatchRead(struct vcfFile *vcff, char *chrom, int start, int end,
                      int maxErr, int maxRecords)
// Reads a batch of records from an opened and indexed VCF file, adding them to
// vcff->records and returning the count of new records added in this batch.
// Note: vcff->records will continue to be sorted, even if batches are loaded
// out of order.  Additionally, resulting vcff->records will contain no duplicates
// so returned count refects only the new records added, as opposed to all records
// in range.  If maxErr >= zero, then continue to parse until there are maxErr+1
// errors.  A maxErr less than zero does not stop and reports all errors.  Set
// maxErr to VCF_IGNORE_ERRS for silence.
{
int oldCount = slCount(vcff->records);

if (lineFileSetTabixRegion(vcff->lf, chrom, start, end))
    {
    struct vcfRecord *records = vcfParseData(vcff, NULL, 0, 0, maxRecords);
    if (records)
        {
        struct vcfRecord *lastRec = vcff->records;
        if (lastRec == NULL)
            vcff->records = records;
        else
            {
            // Considered just asserting the batches were in order, but a problem may
            // result when non-overlapping location windows pick up the same long variant.
            slSortMergeUniq(&(vcff->records), records, vcfRecordCmp, NULL);
            }
        }
    }

return slCount(vcff->records) - oldCount;
}

long long vcfTabixItemCount(char *fileOrUrl, char *tbiFileOrUrl)
/* Return the total number of items across all sequences in fileOrUrl, using index file.
 * If tbiFileOrUrl is NULL, the index file is assumed to be fileOrUrl.tbi.
 * NOTE: not all tabix index files include mapped item counts, so this may return 0 even for
 * large files. */
{
long long itemCount = 0;
hts_idx_t *idx = NULL;
if (isNotEmpty(tbiFileOrUrl))
    idx = hts_idx_load2(fileOrUrl, tbiFileOrUrl);
else
    {
    idx = hts_idx_load(fileOrUrl, HTS_FMT_TBI);
    }
if (idx == NULL)
    warn("vcfTabixItemCount: hts_idx_load(%s) failed.", tbiFileOrUrl ? tbiFileOrUrl : fileOrUrl);
else
    {
    int tCount;
    const char **seqNames = tbx_seqnames((tbx_t *)idx, &tCount);
    int tid;
    for (tid = 0;  tid < tCount;  tid++)
        {
        uint64_t mapped, unmapped;
        int ret = hts_idx_get_stat(idx, tid, &mapped, &unmapped);
        if (ret == 0)
            itemCount += mapped;
        // ret is -1 if counts are unavailable.
        }
    freeMem(seqNames);
    hts_idx_destroy(idx);
    idx = NULL;
    }
return itemCount;
}

void vcfFileFree(struct vcfFile **pVcff)
/* Free a vcfFile object. */
{
if (pVcff == NULL || *pVcff == NULL)
    return;
struct vcfFile *vcff = *pVcff;
if (vcff->maxErr == VCF_IGNORE_ERRS && vcff->errCnt > 0)
    {
    vcff->maxErr++; // Never completely silent!  Go ahead and report how many errs were detected
    vcfFileErr(vcff,"Closing with %d errors.",vcff->errCnt);
    }
freez(&(vcff->headerString));
hashFree(&(vcff->pool));
if (vcff->reusePool)
    lmCleanup(&vcff->reusePool);
hashFree(&(vcff->byName));
lineFileClose(&(vcff->lf));
freez(pVcff);
}

const struct vcfRecord *vcfFileFindVariant(struct vcfFile *vcff, char *variantId)
/* Return all records with name=variantId, or NULL if not found. */
{
struct vcfRecord *varList = NULL;
if (vcff->byName == NULL)
    {
    vcff->byName = hashNew(0);
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
	{
	if (sameString(rec->name, variantId))
	    {
	    // Make shallow copy of rec so we can alter ->next:
	    struct vcfRecord *newRec = vcfFileCloneVar(vcff,rec);
	    slAddHead(&varList, newRec);
	    }
	hashAdd(vcff->byName, rec->name, rec);
	}
    slReverse(&varList);
    }
else
    {
    struct hashEl *hel = hashLookup(vcff->byName, variantId);
    while (hel != NULL)
	{
	if (sameString(hel->name, variantId))
	    {
	    struct vcfRecord *rec = hel->val;
	    struct vcfRecord *newRec = vcfFileCloneVar(vcff,rec);
	    slAddHead(&varList, newRec);
	    }
	hel = hel->next;
	}
    // Don't reverse varList -- hash element list was already reversed
    }
return varList;
}

const struct vcfInfoElement *vcfRecordFindInfo(const struct vcfRecord *record, char *key)
/* Find an INFO element, or NULL. */
{
int i;
for (i = 0;  i < record->infoCount;  i++)
    {
    if (sameString(key, record->infoElements[i].key))
	return &(record->infoElements[i]);
    }
return NULL;
}

struct vcfInfoDef *vcfInfoDefForGtKey(struct vcfFile *vcff, const char *key)
/* Look up the type of genotype FORMAT component key, in the definitions from the header,
 * and failing that, from the keys reserved in the spec. */
{
struct vcfInfoDef *def;
// I expect there to be fairly few definitions (less than a dozen) so
// I'm just doing a linear search not hash:
for (def = vcff->gtFormatDefs;  def != NULL;  def = def->next)
    {
    if (sameString(key, def->key))
	return def;
    }
for (def = vcfSpecGtFormatDefs;  def != NULL;  def = def->next)
    {
    if (sameString(key, def->key))
	return def;
    }
return NULL;
}

static enum vcfInfoType typeForGtFormat(struct vcfFile *vcff, const char *key)
/* Look up the type of FORMAT component key, in the definitions from the header,
 * and failing that, from the keys reserved in the spec. */
{
struct vcfInfoDef *def = vcfInfoDefForGtKey(vcff, key);
return def ? def->type : vcfInfoString;
}

static void parseGt(char *genotype, struct vcfGenotype *gt)
/* Parse genotype, which should be something like "0/0", "0/1", "1|0" or "0/." into gt fields. */
{
char *sep = strchr(genotype, '|');
if (sep != NULL)
    gt->isPhased = TRUE;
else
    sep = strchr(genotype, '/');
if (genotype[0] == '.')
    gt->hapIxA = -1;
else
    gt->hapIxA = atoi(genotype);
if (sep == NULL)
    gt->isHaploid = TRUE;
else if (sep[1] == '.')
    gt->hapIxB = -1;
else
    gt->hapIxB = atoi(sep+1);
}

static void parseSgtAsGt(struct vcfRecord *rec, struct vcfGenotype *gt)
/* Parse SGT to normal and tumor genotypes */
{
const struct vcfInfoElement *sgtEl = vcfRecordFindInfo(rec, "SGT");
if (sgtEl)
    {
    char *val = sgtEl->values[0].datString;
    // set hapIxA and hapIxB where 0 = ref, 1 = alt
    if (startsWith("ref->", val))
        {
        //indel, use 0/0 for normal and 0/1 for tumor
        if (sameString(gt->id, "NORMAL"))
            gt->hapIxA = gt->hapIxB = 0;
        else if (sameString(gt->id, "TUMOR"))
            {
            gt->hapIxA = 0;
            gt->hapIxB = 1;
            }
        }
    else
        {
        if (sameString(gt->id, "NORMAL"))
            {
            char str[2];
            safef(str, sizeof(str), "%c", val[0]);
            if (sameString(rec->alleles[0], str))
                gt->hapIxA = 0;
            else if (sameString(rec->alleles[1], str))
                gt->hapIxA = 1;
            else
                gt->hapIxA = -1;
            safef(str, sizeof(str), "%c", val[1]);
            if (sameString(rec->alleles[1], str))
                gt->hapIxB = 1;
            else if (sameString(rec->alleles[0], str))
                gt->hapIxB = 0;
            else
                gt->hapIxB = -1;
            }
        else if (sameString(gt->id, "TUMOR"))
            {
            char str[2];
            safef(str, sizeof(str), "%c", val[4]);
            if (sameString(rec->alleles[0], str))
                gt->hapIxA = 0;
            else if (sameString(rec->alleles[1], str))
                gt->hapIxA = 1;
            else
                gt->hapIxA = -1;
            safef(str, sizeof(str), "%c", val[5]);
            if (sameString(rec->alleles[1], str))
                gt->hapIxB = 1;
            else if (sameString(rec->alleles[0], str))
                gt->hapIxB = 0;
            else
                gt->hapIxB = -1;
            }
        }
    }
}

static void parsePlAsGt(char *pl, struct vcfGenotype *gt)
/* Parse PL value, which is usually a list of 3 numbers, one of which is 0 (the selected haplotype),
 * into gt fields. */
{
char plCopy[strlen(pl)+1];
safecpy(plCopy, sizeof(plCopy), pl);
char *words[32];
int wordCount = chopCommas(plCopy, words);
if (wordCount > 0 && sameString(words[0], "0"))
    {
    // genotype is AA (ref/ref)
    gt->hapIxA = 0;
    gt->hapIxB = 0;
    }
else if (wordCount > 1 && sameString(words[1], "0"))
    {
    // genotype is AB (ref/alt)
    gt->hapIxA = 0;
    gt->hapIxB = 1;
    }
else if (wordCount > 2 && sameString(words[2], "0"))
    {
    // genotype is BB (alt/alt)
    gt->hapIxA = 1;
    gt->hapIxB = 1;
    }
else if (wordCount > 3 && sameString(words[3], "0"))
    {
    // genotype is AC (ref/alt1)
    gt->hapIxA = 0;
    gt->hapIxB = 2;
    }
else if (wordCount > 4 && sameString(words[4], "0"))
    {
    // genotype is BC (alt/alt1)
    gt->hapIxA = 1;
    gt->hapIxB = 2;
    }
else if (wordCount > 5 && sameString(words[5], "0"))
    {
    // genotype is CC (alt1/alt1)
    gt->hapIxA = 2;
    gt->hapIxB = 2;
    }
else
    {
    // too fancy, treat as "./."
    gt->hapIxA = -1;
    gt->hapIxB = -1;
    }
}

#define VCF_MAX_FORMAT VCF_MAX_INFO
#define VCF_MAX_FORMAT_LEN (VCF_MAX_FORMAT * 4)

void vcfParseGenotypes(struct vcfRecord *record)
/* Translate record->genotypesUnparsedStrings[] into proper struct vcfGenotype[].
 * This destroys genotypesUnparsedStrings. */
{
if (record->genotypeUnparsedStrings == NULL)
    return;
struct vcfFile *vcff = record->file;
record->genotypes = vcfFileAlloc(vcff, vcff->genotypeCount * sizeof(struct vcfGenotype));
char format[VCF_MAX_FORMAT_LEN];
safecpy(format, sizeof(format), record->format);
char *formatWords[VCF_MAX_FORMAT];
int formatWordCount = chopByChar(format, ':', formatWords, ArraySize(formatWords));
if (formatWordCount >= VCF_MAX_FORMAT)
    {
    vcfFileErr(vcff, "The FORMAT column has at least %d words; "
	       "VCF_MAX_FORMAT may need to be increased in vcf.c!", VCF_MAX_FORMAT);
    formatWordCount = VCF_MAX_FORMAT;
    }
int i;
// Store the pooled format word pointers and associated types for use in inner loop below.
enum vcfInfoType formatTypes[VCF_MAX_FORMAT];
for (i = 0;  i < formatWordCount;  i++)
    {
    formatTypes[i] = typeForGtFormat(vcff, formatWords[i]);
    formatWords[i] = vcfFilePooledStr(vcff, formatWords[i]);
    }
for (i = 0;  i < vcff->genotypeCount;  i++)
    {
    char *string = record->genotypeUnparsedStrings[i];
    struct vcfGenotype *gt = &(record->genotypes[i]);
    // Each genotype can have multiple :-separated info elements:
    char *gtWords[VCF_MAX_FORMAT];
    int gtWordCount = chopByChar(string, ':', gtWords, ArraySize(gtWords));
    if (gtWordCount != formatWordCount)
	vcfFileErr(vcff, "The FORMAT column has %d words but the genotype column for %s "
		   "has %d words", formatWordCount, vcff->genotypeIds[i], gtWordCount);
    if (gtWordCount > formatWordCount)
	gtWordCount = formatWordCount;
    gt->id = vcff->genotypeIds[i];
    gt->infoCount = gtWordCount;
    gt->infoElements = vcfFileAlloc(vcff, gtWordCount * sizeof(struct vcfInfoElement));
    gt->hapIxA = gt->hapIxB = -1;
    boolean foundGT = FALSE;
    int j;
    for (j = 0;  j < gtWordCount;  j++)
	{
	// Special parsing of genotype:
	if (sameString(formatWords[j], vcfGtGenotype))
	    {
            parseGt(gtWords[j], gt);
            foundGT = TRUE;
	    }
        else if (!foundGT && sameString(formatWords[j], vcfGtPhred))
            {
            parsePlAsGt(gtWords[j], gt);
            foundGT = TRUE;
            }
	struct vcfInfoElement *el = &(gt->infoElements[j]);
	el->key = formatWords[j];
	el->count = parseInfoValue(record, formatWords[j], formatTypes[j], gtWords[j],
				   &(el->values), &(el->missingData));
	if (el->count >= VCF_MAX_INFO)
	    vcfFileErr(vcff, "A single element of the genotype column for \"%s\" "
		       "has at least %d values; "
		       "VCF_MAX_INFO may need to be increased in vcf.c!",
		       gt->id, VCF_MAX_INFO);
	}
    if (!foundGT)   //try SGT
        {
        parseSgtAsGt(record, gt);
        if (gt->hapIxA != -1)
            foundGT = TRUE;
        }
    if (i == 0 && !foundGT)
        vcfFileErr(vcff,
                   "Genotype FORMAT column includes neither GT nor PL; unable to parse genotypes.");
    }
record->genotypeUnparsedStrings = NULL;
}

const struct vcfGenotype *vcfRecordFindGenotype(struct vcfRecord *record, char *sampleId)
/* Find the genotype and associated info for the individual, or return NULL.
 * This calls vcfParseGenotypes if it has not already been called. */
{
struct vcfFile *vcff = record->file;
if (sampleId == NULL || vcff->genotypeCount == 0)
    return NULL;
vcfParseGenotypes(record);
int ix = stringArrayIx(sampleId, vcff->genotypeIds, vcff->genotypeCount);
if (ix >= 0)
    return &(record->genotypes[ix]);
return NULL;
}

const struct vcfInfoElement *vcfGenotypeFindInfo(const struct vcfGenotype *gt, char *key)
/* Find the genotype infoElement for key, or return NULL. */
{
int i;
for (i = 0;  i < gt->infoCount;  i++)
    if (sameString(key, gt->infoElements[i].key))
        return &gt->infoElements[i];
return NULL;
}

static char *vcfDataLineAutoSqlString =
        "table vcfDataLine"
        "\"The fields of a Variant Call Format data line\""
        "    ("
        "    string chrom;      \"An identifier from the reference genome\""
        "    uint pos;          \"The reference position, with the 1st base having position 1\""
        "    string id;         \"Semi-colon separated list of unique identifiers where available\""
        "    string ref;                \"Reference base(s)\""
        "    string alt;                \"Comma separated list of alternate non-reference alleles "
                                         "called on at least one of the samples\""
        "    string qual;       \"Phred-scaled quality score for the assertion made in ALT. i.e. "
                                 "give -10log_10 prob(call in ALT is wrong)\""
        "    string filter;     \"PASS if this position has passed all filters. Otherwise, a "
                                  "semicolon-separated list of codes for filters that fail\""
        "    string info;       \"Additional information encoded as a semicolon-separated series "
                                 "of short keys with optional comma-separated values\""
        "    string format;     \"If genotype columns are specified in header, a "
                                 "semicolon-separated list of of short keys starting with GT\""
        "    string genotypes;  \"If genotype columns are specified in header, a tab-separated "
                                 "set of genotype column values; each value is a colon-separated "
                                 "list of values corresponding to keys in the format column\""
        "    )";

struct asObject *vcfAsObj()
// Return asObject describing fields of VCF
{
return asParseText(vcfDataLineAutoSqlString);
}

char *vcfGetSlashSepAllelesFromWords(char **words, struct dyString *dy)
/* Overwrite dy with a /-separated allele string from VCF words,
 * skipping the extra initial base that VCF requires for indel alleles if necessary,
 * and trimming identical bases at the ends of all alleles if there are any.
 * Return dy->string for convenience. */
{
// VCF reference allele gets its own column:
char *refAllele = words[3];
char *altAlleles = words[4];
// Make a vcfRecord-like allele array (ref in [0], alts after) so we can check for padding base:
int alCount = 1 + countChars(altAlleles, ',') + 1;
char *alleles[alCount];
alleles[0] = refAllele;
char altAlCopy[strlen(altAlleles)+1];
safecpy(altAlCopy, sizeof(altAlCopy), altAlleles);
chopByChar(altAlCopy, ',', &(alleles[1]), alCount-1);
int i;
if (allelesHavePaddingBase(alleles, alCount))
    {
    // Skip padding base (unless we have a symbolic allele):
    for (i = 0;  i < alCount;  i++)
	if (isAllNt(alleles[i], strlen(alleles[i])))
	    alleles[i]++;
    }
// Having dealt with left padding base, now look for identical bases on the right:
int trimmedBases = countIdenticalBasesRight(alleles, alCount);
// Build a /-separated allele string, trimming bases on the right if necessary:
dyStringClear(dy);
if (noAltAllele(alleles, alCount))
    alCount = 1;
for (i = 0;  i < alCount;  i++)
    {
    char *allele = alleles[i];
    if (!sameString(allele, "."))
        {
        if (i != 0)
            {
            if (sameString(alleles[0], allele))
                continue;
            dyStringAppendC(dy, '/');
            }
        if (allele[trimmedBases] == '\0')
            dyStringAppendC(dy, '-');
        else
            dyStringAppendN(dy, allele, strlen(allele)-trimmedBases);
        }
    }
return dy->string;
}

static void vcfWriteWordArrayWithSep(FILE *f, int count, char **words, char sep)
/* Write words joined by sep to f (or, if count is zero, ".").  */
{
if (count < 1)
    fputc('.', f);
else
    {
    fputs(words[0], f);
    int i;
    for (i = 1;  i < count;  i++)
        {
        fputc(sep, f);
        fputs(words[i], f);
        }
    }
}

static void vcfWriteInfo(FILE *f, struct vcfRecord *rec)
/* Write rec->infoElements to f. */
{
if (rec->infoCount < 1)
    fputc('.', f);
else
    {
    int i, j;
    for (i = 0;  i < rec->infoCount;  i++)
        {
        struct vcfInfoElement *info = &(rec->infoElements[i]);
        enum vcfInfoType type = typeForInfoKey(rec->file, info->key);
        if (i > 0)
            fputc(';', f);
        fputs(info->key, f);
        for (j = 0;  j < info->count;  j++)
            {
            union vcfDatum datum = info->values[j];
            switch (type)
                {
                case vcfInfoInteger:
                    fprintf(f, "=%d", datum.datInt);
                    break;
                case vcfInfoFloat:
                    fprintf(f, "=%lf", datum.datFloat);
                    break;
                case vcfInfoFlag:
                    // Flag key might have a value in older VCFs e.g. 3.2's DB=0, DB=1
                    if (isNotEmpty(datum.datString))
                        fprintf(f, "=%s", datum.datString);
                    break;
                case vcfInfoCharacter:
                    fprintf(f, "%c", datum.datChar);
                    break;
                case vcfInfoString:
                    fputc('=', f);
                    if (isNotEmpty(datum.datString))
                        fputs(datum.datString, f);
                    break;
                default:
                    vcfFileErr(rec->file, "invalid vcfInfoType (uninitialized?) %d", type);
                    break;
                }
            }
        }
    }
}

void vcfRecordWriteNoGt(FILE *f, struct vcfRecord *rec)
/* Write the first 8 columns of VCF rec to f.  Genotype data will be ignored if present. */
{
fprintf(f, "%s\t%d\t%s\t%s\t", rec->chrom, rec->chromStart+1, rec->name, rec->alleles[0]);
// Alternate alleles start at [1]
vcfWriteWordArrayWithSep(f, rec->alleleCount-1, &(rec->alleles[1]), ',');
fputc('\t', f);
fputs(rec->qual, f);
fputc('\t', f);
vcfWriteWordArrayWithSep(f, rec->filterCount, rec->filters, ';');
fputc('\t', f);
vcfWriteInfo(f, rec);
fputc('\n', f);
}
