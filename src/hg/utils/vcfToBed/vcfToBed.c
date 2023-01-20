/* vcfToBed - Convert VCF to BED9+ with optional extra fields.
 * Extra VCF tags get placed into a separate tab file for later indexing.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vcf.h"
#include "dnautil.h"

// how many extra fields  can we put into the bigBed itself (for filtering, labels, etc):
#define MAX_BED_EXTRA 100
#define bed9Header "#chrom\tchromStart\tchromEnd\tname\tscore\tstrand\tthickStart\tthickEnd\titemRgb\tref\talt\tFILTER"

// hash up all the info tags and their vcfInfoDef structs for faster searching
struct hash *infoHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "vcfToBed - Convert VCF to BED9+ with optional extra fields.\n"
    "usage:\n"
    "    vcfToBed in.vcf outPrefix\n"
    "options:\n"
    "    -fixChromNames If present, prepend 'chr' to chromosome names\n"
    "    -fields=comma-sep list of tags to include in the bed file, other fields will be placed into\n"
    "           out.extraFields.tab\n"
    "\n"
    "NOTE: Extra VCF tags (that aren't listed in -fields)  get placed into a separate tab\n"
    "file for later indexing.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"fields", OPTION_STRING},
    {"fixChromNames", OPTION_BOOLEAN},
    {NULL, 0},
};

int trimMissingTagsFromFieldList(char **keepFields, char **tempKeepFields, int keepCount, struct vcfFile *vcff)
/* If there are requested fields in keepFields that don't exist in vcff, warn about them
 * and remove them from the keepFields array. Return number of valid entries */
{
int i = 0;
int keepIx = 0;
for (; i < keepCount; i++)
    {
    char *tag = tempKeepFields[i];
    struct vcfInfoDef *def = NULL;
    if ( (def = vcfInfoDefForKey(vcff, tag)) == NULL)
        verbose(2, "Warning: skipping desired tag '%s', does not exist in INFO fields.\n", tag);
    else
        {
        keepFields[keepIx++] = cloneString(tag);
        }
    }
return keepIx;
}

char *fixupChromName(char *chrom)
/* Prepend "chr" if missing */
{
if (startsWith("chr", chrom))
    return chrom;
else
    return catTwoStrings("chr", chrom);
}

#define VCF_MAX_INFO (4*1024)

void fixupLeftPadding(int *start, char *ref, char *alt)
/* If an indel is left padded fix up the start position */
{
char *altTmp = cloneString(alt);
char *alleles[VCF_MAX_INFO+1];
alleles[0] = cloneString(ref);
char *altAlleles[VCF_MAX_INFO];
int altCount = chopCommas(altTmp, altAlleles);
int alleleCount = 1 + altCount;
if (alleleCount > 1)
    {
    int i;
    for (i = 0; i < altCount; i++)
        alleles[1+i] = altAlleles[i];
    boolean hasPaddingBase = allelesHavePaddingBase(alleles, alleleCount);
    if (hasPaddingBase)
        (*start)++;
    }
}

void shortenRefAltString(struct dyString *shortenedRef, struct dyString *shortenedAlt, char *ref, char *alt)
/* Shorten up the Ref and Alt strings for bed labels */
{
int i;
if (strlen(ref) > 10)
    {
    for (i = 0; i < 10; i++)
        dyStringAppendC(shortenedRef, ref[i]);
    dyStringAppend(shortenedRef, "...");
    }
else
    shortenedRef->string = ref;
if (strlen(alt) > 10)
    {
    for (i = 0; i < 10; i++)
        dyStringAppendC(shortenedAlt, alt[i]);
    dyStringAppend(shortenedAlt, "...");
    }
else
    shortenedAlt->string = alt;
}

void printExtraFieldInfo(FILE *outBed, char *info, char **infoTags, int infoCount, char **keepFields, int extraFieldCount)
{
int i,j;
int numTags = chopByChar(info, ';', infoTags, infoCount);
for (i = 0; i < extraFieldCount; i++)
    {
    char *extraField = keepFields[i];
    for (j = 0; j < numTags && infoTags[j]; j++)
        {
        boolean isKeyVal = (strchr(infoTags[j], '=') != NULL);
        if (isKeyVal && startsWithWordByDelimiter(extraField, '=', infoTags[j]))
            {
            char *infoTagVal = infoTags[j];
            char *val = strchr(infoTagVal, '=');
            *(val++) = '\0';
            if (sameString(infoTagVal, extraField))
                {
                fprintf(outBed, "%s", val);
                break;
                }
            }
        else if (sameString(infoTags[j], extraField))
            {
            fprintf(outBed, "Yes");
            break;
            }
        }
    if (i < (extraFieldCount - 1))
        fprintf(outBed, "\t");
    }
}

void printHeaders(struct vcfFile *vcff, char **keepFields, int keepCount, FILE *outBed)
/* Print a comment describing all the fields into outBed */
{
int i;
fprintf(outBed, "%s", bed9Header);
for (i = 0; i < keepCount; i++)
    if (keepFields[i] != NULL)
        fprintf(outBed, "\t%s", keepFields[i]);
fprintf(outBed, "\n");
}

void vcfLinesToBed(struct vcfFile *vcff, char **keepFields, int extraFieldCount,
                    boolean fixChromNames, FILE *outBed) //, FILE *outExtra)
/* Turn a VCF line into a bed9+ line */
{
struct dyString *name = dyStringNew(0);
struct dyString *shortenedRef = dyStringNew(0);
struct dyString *shortenedAlt = dyStringNew(0);
char *line = NULL;
char *chopped[9]; // 9 fields in case VCF has genotype fields, keep them separate from info fields
int infoCount = slCount(vcff->infoDefs);
char *infoTags[infoCount];
while (lineFileNext(vcff->lf, &line, NULL))
    {
    int fieldCount = chopTabs(line, chopped);
    if (fieldCount < 8)
        errAbort("ERROR: malformed VCF, missing fields at line: '%d'", vcff->lf->lineIx);
    char *chrom = chopped[0];
    if (fixChromNames)
        chrom = fixupChromName(chrom);
    int start = atoi(chopped[1]) - 1;
    int end = start;
    char *ref = cloneString(chopped[3]);
    char *alt = cloneString(chopped[4]);
    if (strlen(ref) == dnaFilteredSize(ref))
        end = start + strlen(ref);
    fixupLeftPadding(&start, ref, alt);
    if (!sameString(chopped[2], "."))
        dyStringPrintf(name, "%s", chopped[2]);
    else
        {
        shortenRefAltString(shortenedRef, shortenedAlt, ref, alt);
        dyStringPrintf(name, "%s_%d:%s/%s", chrom,start,shortenedRef->string,shortenedAlt->string);
        }
    fprintf(outBed, "%s\t%d\t%d\t%s\t0\t.\t%d\t%d\t0,0,0", chrom,start,end,name->string,start,end);
    fprintf(outBed, "\t%s\t%s\t%s", ref, alt, chopped[6]);
    if (extraFieldCount)
        {
        fprintf(outBed, "\t");
        printExtraFieldInfo(outBed, chopped[7], infoTags, infoCount, keepFields, extraFieldCount);
        }
    fprintf(outBed, "\n");
    dyStringClear(name);
    dyStringClear(shortenedRef);
    dyStringClear(shortenedAlt);
    }
}

void vcfToBed(char *vcfFileName, char *outPrefix, char *tagsToKeep, boolean fixChromNames)
/* vcfToBed - Convert VCF to BED9+ with optional extra fields.
 * Extra VCF tags get placed into a separate tab file for later indexing. */
{
FILE *outBed;
struct vcfFile *vcff = NULL;
char tbiFile[4096];
int i;
int vcfTagCount = 0; // no need for extra file if there's only a few tags
int keepCount = 0; // count of comma-sep tagsToKeep
char *keepFields[MAX_BED_EXTRA]; // Max 50 extra fields to put into bigBed, also needed for
                                 // comment string header
char *tempKeepFields[MAX_BED_EXTRA]; // allow requesting fields that don't exist, just don't output
memset(keepFields, 0, MAX_BED_EXTRA);
memset(tempKeepFields, 0, MAX_BED_EXTRA);

// open up VCF for reading
safef(tbiFile, sizeof(tbiFile), "%s.tbi", vcfFileName);
if (fileExists(tbiFile))
    {
    vcff = vcfTabixFileAndIndexMayOpen(vcfFileName, tbiFile, NULL, 0,0,VCF_IGNORE_ERRS,-1);
    if (vcff == NULL)
        errAbort("error opening %s\n", vcfFileName);
    vcfTagCount = slCount(vcff->infoDefs);
    // do a batch read and use the reuse pool
    vcfFileMakeReusePool(vcff, 64*1024*1024);
    }
else
    errAbort("no tabix index for %s\n", vcfFileName);

if (tagsToKeep)
    {
    keepCount = chopCommas(tagsToKeep, tempKeepFields);
    if (keepCount > vcfTagCount)
        verbose(2, "Warning: fewer fields in VCF than -fields specification.");
    keepCount = trimMissingTagsFromFieldList(keepFields, tempKeepFields, keepCount, vcff);
    }

// open output for writing
char *outBedName = NULL;
if (endsWith(outPrefix, ".bed"))
    outBedName = cloneString(outPrefix);
else
    outBedName = catTwoStrings(outPrefix, ".bed");
outBed = mustOpen(outBedName, "w");

verbose(2, "input vcf tag count = %d\n", vcfTagCount);
verbose(2, "number of valid requested tags = %d\n", keepCount);
for (i = 0; i < keepCount; i++)
    if (keepFields[i] != NULL)
        verbose(2, "found tag: '%s'\n", keepFields[i]);

printHeaders(vcff, keepFields, keepCount, outBed);
vcfLinesToBed(vcff, keepFields, keepCount, fixChromNames, outBed);
vcfFileFree(&vcff);
carefulClose(&outBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fields = optionVal("fields", NULL);
boolean fixChromNames = optionExists("fixChromNames");
vcfToBed(argv[1],argv[2], fields, fixChromNames);
return 0;
}
