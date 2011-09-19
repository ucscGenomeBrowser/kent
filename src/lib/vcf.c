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
va_list args;
va_start(args, format);
char formatPlus[1024];
if (vcff->lf != NULL)
    sprintf(formatPlus, "%s:%d: %s", vcff->lf->fileName, vcff->lf->lineIx, format);
else
    strcpy(formatPlus, format);
vaWarn(formatPlus, args);
va_end(args);
vcff->errCnt++;
if (vcfFileStopDueToErrors(vcff))
    errAbort("VCF: %d parser errors, quitting", vcff->errCnt);
}

static void *vcfFileAlloc(struct vcfFile *vcff, size_t size)
/* allocate memory from the memory pool */
{
return lmAlloc(vcff->pool->lm, size);
}

static char *vcfFileCloneStrZ(struct vcfFile *vcff, char *str, size_t size)
/* allocate memory for a string and copy it */
{
return lmCloneStringZ(vcff->pool->lm, str, size);
}

static char *vcfFileCloneStr(struct vcfFile *vcff, char *str)
/* allocate memory for a string and copy it */
{
return vcfFileCloneStrZ(vcff, str, strlen(str));
}

static char *vcfFileCloneSubstr(struct vcfFile *vcff, char *line, regmatch_t substr)
/* Allocate memory for and copy a substring of line. */
{
return vcfFileCloneStrZ(vcff, line+substr.rm_so, (substr.rm_eo - substr.rm_so));
}

#define vcfFileCloneVar(var) lmCloneMem(vcff->pool->lm, var, sizeof(var));

static char *vcfFilePooledStr(struct vcfFile *vcff, char *str)
/* allocate memory for a string from the shared string pool */
{
return hashStoreName(vcff->pool, str);
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
    "Number=(\\.|[0-9-]+),"
    "Type=([A-Za-z]+),"
    "Description=\"?([^\"]+)\"?>$";
static const char *filterOrAltRegex =
    "^##(FILTER|ALT)="
    "<ID=([^,]+),"
    "(Description|Type)=\"([^\"]+)\">$";

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
    if (regexMatchSubstr(line, infoOrFormatRegex, substrs, ArraySize(substrs)))
	// substrs[2] is ID/key, substrs[3] is Number, [4] is Type and [5] is Description.
	{
	struct vcfInfoDef *def = vcfFileAlloc(vcff, sizeof(struct vcfInfoDef));
	def->key = vcfFileCloneSubstr(vcff, line, substrs[2]);
	if (sameString(def->key, "."))
	    def->fieldCount = -1;
	else
	    def->fieldCount = atoi(line + substrs[3].rm_so);
	def->type = vcfInfoTypeFromSubstr(vcff, line, substrs[4]);
	def->description = vcfFileCloneSubstr(vcff, line, substrs[5]);
	slAddHead((isInfo ? &(vcff->infoDefs) : &(vcff->gtFormatDefs)), def);
	}
    else
	vcfFileErr(vcff, "##%s line does not match expected pattern /%s/: \"%s\"",
		   (isInfo ? "INFO" : "FORMAT"), infoOrFormatRegex, line);
    }
else if (startsWith("##FILTER=", line) || startsWith("##ALT=", line))
    {
    boolean isFilter = startsWith("##FILTER", line);
    if (regexMatchSubstr(line, filterOrAltRegex, substrs, ArraySize(substrs)))
	{
	// substrs[2] is ID/key, substrs[3] is Description.
	struct vcfInfoDef *def = vcfFileAlloc(vcff, sizeof(struct vcfInfoDef));
	def->key = vcfFileCloneSubstr(vcff, line, substrs[2]);
	def->description = vcfFileCloneSubstr(vcff, line, substrs[3]);
	slAddHead((isFilter ? &(vcff->filterDefs) : &(vcff->altDefs)), def);
	}
    else
	vcfFileErr(vcff, "##%s line does not match expected pattern /%s/: \"%s\"",
		   (isFilter ? "FILTER" : "ALT"), filterOrAltRegex, line);
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
return vcff;
}

static struct vcfFile *vcfFileHeaderFromLineFile(struct lineFile *lf, int maxErr)
/* Parse a VCF file into a vcfFile object.  If maxErr not zero, then
 * continue to parse until this number of error have been reached.  A maxErr
 * less than zero does not stop and reports all errors. */
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
			  char *valStr, union vcfDatum **pData)
/* Parse a comma-separated list of values into array of union vcfInfoDatum and return count. */
{
char *valWords[VCF_MAX_INFO];
int count = chopCommas(valStr, valWords);
struct vcfFile *vcff = record->file;
union vcfDatum *data = vcfFileAlloc(vcff, count * sizeof(union vcfDatum));
int j;
for (j = 0;  j < count;  j++)
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
// If END is given, use it as chromEnd:
if (sameString(infoKey, vcfInfoEnd))
    record->chromEnd = data[0].datInt;
*pData = data;
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
    el->count = parseInfoValue(record, el->key, type, valStr, &(el->values));
    if (el->count >= VCF_MAX_INFO)
	vcfFileErr(vcff, "A single element of the INFO column has at least %d values; "
	       "VCF_MAX_INFO may need to be increased in vcf.c!", VCF_MAX_INFO);
    }
}

static void vcfParseData(struct vcfFile *vcff)
/* Given a vcfFile into which the header has been parsed, and whose lineFile is positioned
 * at the beginning of a data row, parse and store all data rows from lineFile. */
{
if (vcff == NULL)
    return;
int expected = 8;
if (vcff->genotypeCount > 0)
    expected = 9 + vcff->genotypeCount;
char *words[VCF_MAX_COLUMNS];
int wordCount;
while ((wordCount = lineFileChop(vcff->lf, words)) > 0)
    {
    lineFileExpectWords(vcff->lf, expected, wordCount);
    struct vcfRecord *record;
    AllocVar(record);
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
    slAddHead(&(vcff->records), record);
    }
slReverse(&(vcff->records));
lineFileClose(&(vcff->lf));
}

struct vcfFile *vcfFileMayOpen(char *fileOrUrl, int maxErr)
/* Parse a VCF file into a vcfFile object.  If maxErr not zero, then
 * continue to parse until this number of error have been reached.  A maxErr
 * less than zero does not stop and reports all errors. */
{
struct lineFile *lf = NULL;
if (startsWith("http://", fileOrUrl) || startsWith("ftp://", fileOrUrl) ||
    startsWith("https://", fileOrUrl))
    lf = netLineFileOpen(fileOrUrl);
else
    lf = lineFileMayOpen(fileOrUrl, TRUE);
struct vcfFile *vcff = vcfFileHeaderFromLineFile(lf, maxErr);
vcfParseData(vcff);
return vcff;
}

struct vcfFile *vcfTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end,
				    int maxErr)
/* Parse header and rows within the given position range from a VCF file that has been
 * compressed and indexed by tabix into a vcfFile object; return NULL if or if file has
 * no items in range.
 * If maxErr not zero, then continue to parse until this number of error have been reached.
 * A maxErr less than zero does not stop and reports all errors. */
{
struct lineFile *lf = lineFileTabixMayOpen(fileOrUrl, TRUE);
struct vcfFile *vcff = vcfFileHeaderFromLineFile(lf, maxErr);
if (vcff == NULL)
    return NULL;
if (isNotEmpty(chrom) && start != end)
    {
    if (lineFileSetTabixRegion(lf, chrom, start, end))
	vcfParseData(vcff);
    }
return vcff;
}

void vcfFileFree(struct vcfFile **pVcff)
/* Free a vcfFile object. */
{
if (pVcff == NULL || *pVcff == NULL)
    return;
struct vcfFile *vcff = *pVcff;
freez(&(vcff->headerString));
hashFree(&(vcff->pool));
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
	if (sameString (rec->name, variantId))
	    {
	    // Make shallow copy of rec so we can alter ->next:
	    struct vcfRecord *newRec = vcfFileCloneVar(rec);
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
	    struct vcfRecord *newRec = vcfFileCloneVar(rec);
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
	    gt->hapIxA = atoi(genotype);
	    if (sep == NULL)
		gt->isHaploid = TRUE;
	    else
		gt->hapIxB = atoi(sep+1);
	    }
	struct vcfInfoElement *el = &(gt->infoElements[j]);
	el->key = formatWords[j];
	el->count = parseInfoValue(record, formatWords[j], formatTypes[j], gtWords[j],
				   &(el->values));
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

