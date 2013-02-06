/* VCF: Variant Call Format, version 4.0 / 4.1
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-40
 * http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-41
 */

#include "common.h"
#include "dnautil.h"
#include "errabort.h"
#include <limits.h>
#include "localmem.h"
#include "net.h"
#include "regexHelper.h"
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
const char *vcfGtLikelihoods = "GL";	// Three floating point log10-scaled likelihoods for
					// AA,AB,BB genotypes where A=ref and B=alt;
					// not applicable if site is not biallelic.
const char *vcfGtPhred = "PL";		// Phred-scaled genotype likelihoods rounded to closest int
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

static void vcfFileErr(struct vcfFile *vcff, char *format, ...)
/* Send error message to errabort stack's warn handler and abort */
{
vcff->errCnt++;
if (vcff->maxErr == VCF_IGNORE_ERRS)
    return;
va_list args;
va_start(args, format);
char formatPlus[1024];
if (vcff->lf != NULL)
    sprintf(formatPlus, "%s:%d: %s", vcff->lf->fileName, vcff->lf->lineIx, format);
else
    strcpy(formatPlus, format);
vaWarn(formatPlus, args);
va_end(args);
if (vcfFileStopDueToErrors(vcff))
    errAbort("VCF: %d parser errors, quitting", vcff->errCnt);
}

static void *vcfFileAlloc(struct vcfFile *vcff, size_t size)
/* Use vcff's local mem to allocate memory. */
{
return lmAlloc( vcfFileLm(vcff), size);
}

inline char *vcfFileCloneStrZ(struct vcfFile *vcff, char *str, size_t size)
/* Use vcff's local mem to allocate memory for a string and copy it. */
{
return lmCloneStringZ( vcfFileLm(vcff), str, size);
}

inline char *vcfFileCloneStr(struct vcfFile *vcff, char *str)
/* Use vcff's local mem to allocate memory for a string and copy it. */
{
return vcfFileCloneStrZ(vcff, str, strlen(str));
}

inline char *vcfFileCloneSubstr(struct vcfFile *vcff, char *line, regmatch_t substr)
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
    return vcfInfoNoType;
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
return vcfInfoNoType;
}

// Regular expressions to check format and extract information from header lines:
static const char *fileformatRegex = "^##(file)?format=VCFv([0-9]+)(\\.([0-9]+))?$";
static const char *infoOrFormatRegex =
    "^##(INFO|FORMAT)="
    "<ID=([A-Za-z0-9_:-]+),"
    "Number=(\\.|A|G|[0-9-]+),"
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

static void parseColumnHeaderRow(struct vcfFile *vcff, char *line)
/* Make sure column names are as we expect, and store genotype sample IDs if any are given. */
{
if (line[0] != '#')
    {
    vcfFileErr(vcff, "Expected to find # followed by column names (\"#CHROM POS ...\"), "
	       "not \"%s\"", line);
    lineFileReuse(vcff->lf);
    return;
    }
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

static struct vcfFile *vcfFileNew()
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
    vcfFileErr(vcff, "missing ##fileformat= header line?  Assuming 4.1.");
if ((vcff->majorVersion != 4 || (vcff->minorVersion != 0 && vcff->minorVersion != 1)) &&
    (vcff->majorVersion != 3))
    vcfFileErr(vcff, "VCFv%d.%d not supported -- only v3.*, v4.0 or v4.1",
	       vcff->majorVersion, vcff->minorVersion);
// Next, one header line beginning with single "#" that names the columns:
if (line == NULL)
    // EOF after metadata
    return vcff;
dyStringAppend(dyHeader, line);
dyStringAppendC(dyHeader, '\n');
parseColumnHeaderRow(vcff, line);
vcff->headerString = dyStringCannibalize(&dyHeader);
return vcff;
}


#define VCF_MAX_INFO 512

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
if (def == NULL)
    {
    vcfFileErr(vcff, "There is no INFO header defining \"%s\"", key);
    // default to string so we can display value as-is:
    return vcfInfoString;
    }
return def->type;
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

struct vcfRecord *vcfNextRecord(struct vcfFile *vcff)
/* Parse the words in the next line from vcff into a vcfRecord. Return NULL at end of file.
 * Note: this does not store record in vcff->records! */
{
char *words[VCF_MAX_COLUMNS];
int wordCount;
if ((wordCount = lineFileChop(vcff->lf, words)) <= 0)
    return NULL;
int expected = 8;
if (vcff->genotypeCount > 0)
    expected = 9 + vcff->genotypeCount;
lineFileExpectWords(vcff->lf, expected, wordCount);
return vcfRecordFromRow(vcff, words);
}

unsigned int vcfRecordTrimIndelLeftBase(struct vcfRecord *rec)
/* For indels, VCF includes the left neighboring base; for example, if the alleles are
 * AA/- following a G base, then the VCF record will start one base to the left and have
 * "GAA" and "G" as the alleles.  That is not nice for display for two reasons:
 * 1. Indels appear one base wider than their dbSNP entries.
 * 2. In pgSnp display mode, the two alleles are always the same color.
 * However, for hgTracks' mapBox we need the correct chromStart for identifying the
 * record in hgc -- so return the original chromStart. */
{
unsigned int chromStartOrig = rec->chromStart;
struct vcfFile *vcff = rec->file;
if (rec->alleleCount > 1)
    {
    boolean allSameFirstBase = TRUE;
    char firstBase = rec->alleles[0][0];
    int i;
    for (i = 1;  i < rec->alleleCount;  i++)
	if (rec->alleles[i][0] != firstBase)
	    {
	    allSameFirstBase = FALSE;
	    break;
	    }
    if (allSameFirstBase)
	{
	rec->chromStart++;
	for (i = 0;  i < rec->alleleCount;  i++)
	    {
	    if (rec->alleles[i][1] == '\0')
		rec->alleles[i] = vcfFilePooledStr(vcff, "-");
	    else
		rec->alleles[i] = vcfFilePooledStr(vcff, rec->alleles[i]+1);
	    }
	}
    }
return chromStartOrig;
}

static struct vcfRecord *vcfParseData(struct vcfFile *vcff, int maxRecords)
// Given a vcfFile into which the header has been parsed, and whose
// lineFile is positioned at the beginning of a data row, parse and
// return all data rows from lineFile.
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
    slAddHead(&records, record);
    recCount++;
    }
slReverse(&records);

return records;
}

struct vcfFile *vcfFileMayOpen(char *fileOrUrl, int maxErr, int maxRecords, boolean parseAll)
/* Open fileOrUrl and parse VCF header; return NULL if unable.
 * If parseAll, then read in all lines, parse and store in
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
if (parseAll)
    {
    vcff->records = vcfParseData(vcff, maxRecords);
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
struct lineFile *lf = lineFileTabixMayOpen(fileOrUrl, TRUE);
struct vcfFile *vcff = vcfFileHeaderFromLineFile(lf, maxErr);
if (vcff == NULL)
    return NULL;
if (isNotEmpty(chrom) && start != end)
    {
    if (lineFileSetTabixRegion(lf, chrom, start, end))
        {
        vcff->records = vcfParseData(vcff, maxRecords);
        lineFileClose(&(vcff->lf)); // Not sure why it is closed.  Angie?
        }
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
    struct vcfRecord *records = vcfParseData(vcff, maxRecords);
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
if (def == NULL)
    {
    vcfFileErr(vcff, "There is no FORMAT header defining \"%s\"", key);
    // default to string so we can display value as-is:
    return vcfInfoString;
    }
return def->type;
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
if (differentString(formatWords[0], vcfGtGenotype))
    vcfFileErr(vcff, "FORMAT column should begin with \"%s\" but begins with \"%s\"",
	       vcfGtGenotype, formatWords[0]);
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
    int j;
    for (j = 0;  j < gtWordCount;  j++)
	{
	// Special parsing of genotype:
	if (sameString(formatWords[j], vcfGtGenotype))
	    {
	    char *genotype = gtWords[j];
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

// - - - - - - Support for bit map based analysis of variants - - - - - -

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
    { // spin through all the subjects to see if any are non-haploid
    for (ix = 0; ix < vcff->genotypeCount && record->genotypes[ix].isHaploid;ix++)
        ;
    if (ix == vcff->genotypeCount)
        vBits->haplotypeSlots = 1;
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

int vcfVariantBitsDropSparse(struct variantBits **vBitsList, int haploGenomeMin)
// Drops vBits found in less than a minimum number of haplotype genomes.
// Returns count of vBits structure that were dropped.
{
struct variantBits *vBitsKeep = NULL;
struct variantBits *vBits = NULL;
int dropped = 0;
while ((vBits = slPopHead(vBitsList)) != NULL)
    {
    if (vBits->bitsOn >= haploGenomeMin)
        slAddHead(&vBitsKeep,vBits);
    else  // drop memory, since this is on lm
        dropped++;
    }
slReverse(&vBitsKeep);
*vBitsList = vBitsKeep;
return dropped;
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
        if (!ignoreReference || hBits->bitsOn > 0) // gonna keep it so flesh it out
            {
            hBits->genomeIx  = genoIx;
            hBits->haploidIx = haploIx;
            struct vcfRecord *record = vBitsList->record; // any will do
            struct vcfGenotype *gt = &(record->genotypes[genoIx]);
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
        //else
        //    haploBitsFree(&hBits); // lm so just abandon
        }
    }
slReverse(&hBitsList);

return hBitsList;
}

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

int vcfHaploMemCmp(const void *va, const void *vb)
// Compare to sort haploBits based upon how many bits are on
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
slSort(&hBitsToTest, vcfHaploMemCmp);

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
