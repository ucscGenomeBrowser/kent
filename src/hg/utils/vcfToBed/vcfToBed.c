/* vcfToBed - Convert VCF to BED9+ with optional extra fields.
 * Extra VCF tags get placed into a separate tab file for later indexing.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vcf.h"

#define MAX_BED_EXTRA 50 // how many extra fields  can we put into the bigBed itself (for filtering, labels, etc)
#define bed9Header "#chrom\tchromStart\tchromEnd\tname\tscore\tstrand\tthickStart\tthickEnd\titemRgb"

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
    {NULL, 0},
};

void hashInfoTags(struct vcfFile *vcff)
{
if (!infoHash)
    infoHash = hashNew(0);
struct vcfInfoDef *def = NULL;
for (def = vcff->infoDefs; def != NULL; def = def->next)
    {
    hashAdd(infoHash, def->key, def);
    }
}

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
if (!startsWith(chrom, "chr"))
    return catTwoStrings("chr", chrom);
else
    return chrom;
}

char *fixupVcfRecordName(struct vcfRecord *rec, int fixedChromStart)
/* Fill in for often missing vcf record name (subs out '.' for something useful) */
{
static struct dyString *ds;
if (!ds)
    ds = dyStringNew(0);
char *name = NULL;
if (isNotEmpty(rec->name) && !sameString(rec->name,"."))
    name = cloneString(rec->name);
else
    {
    dyStringPrintf(ds, "%s_%d:", fixupChromName(rec->chrom), fixedChromStart);
    if (rec->alleleCount < 2)
        dyStringPrintf(ds, "?");
    else
        dyStringPrintf(ds, "%s/%s", rec->alleles[0], rec->alleles[1]);
    name = cloneString(ds->string);
    }
dyStringClear(ds);
return name;
}

void printVcfInfoElem(struct vcfFile *vcff, struct vcfRecord *rec, char *tag, FILE *out, boolean printTabular)
/* Maybe print a VCF INFO tag element to out */
{
const struct vcfInfoElement *elem = vcfRecordFindInfo(rec, tag);
if (elem)
    {
    const struct vcfInfoDef *def = hashMustFindVal(infoHash, elem->key);
    //const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, elem->key);
    enum vcfInfoType type = def ? def->type : vcfInfoString;
    if (elem != NULL)
        {
        if (type == vcfInfoFlag && elem->count == 0)
            fprintf(out, "Yes");
        if (looksTabular(def, elem) && !printTabular)
            errAbort("Error: Tables not supported in bigBed fields, tag '%s'", elem->key);
        else
            {
            int j;
            for (j = 0; j < elem->count; j++)
                {
                if (j > 0)
                    fprintf(out, ", ");
                if (elem->missingData[j])
                    fprintf(out, ".");
                else
                    vcfPrintDatum(out, elem->values[j], type);
                }
            }
        }
    }
}

void printHeaders(struct vcfFile *vcff, char **keepFields, int keepCount, FILE *outBed, FILE *outExtra)
/* Print the required header into extraFields.tab, and put a technically optional
 * comment into outBed */
{
int i;
fprintf(outBed, "%s", bed9Header);
for (i = 0; i < keepCount; i++)
    if (keepFields[i] != NULL)
        fprintf(outBed, "\t%s", keepFields[i]);
fprintf(outBed, "\n");

if (outExtra)
    {
    struct vcfInfoDef *def;
    boolean first = TRUE;
    for (def = vcff->infoDefs; def != NULL; def = def->next)
        {
        if (stringArrayIx(def->key, keepFields, keepCount) == -1)
            {
            if (!first)
                fprintf(outExtra, "\t");
            fprintf(outExtra, "%s", def->key);
            first = FALSE;
            }
        }
    fprintf(outExtra, "\n");
    }
}

void vcfLinesToBed(struct vcfFile *vcff, char **keepFields, int extraFieldCount,
                    FILE *outBed, FILE *outExtra)
/* Turn a VCF line into a bed9+ line */
{
struct vcfRecord *rec;
int i;
while ( (rec = vcfNextRecord(vcff)) != NULL)
    {
    int start = vcfRecordTrimIndelLeftBase(rec);
    char *name = fixupVcfRecordName(rec, start);
    fprintf(outBed, "%s\t%d\t%d\t%s", fixupChromName(rec->chrom), start, rec->chromEnd, name);
    fprintf(outBed, "0\t.\t%d\t%d\t0,0,0", start, rec->chromEnd);
    for (i = 0; i < extraFieldCount; i++)
        {
        fprintf(outBed, "\t");
        printVcfInfoElem(vcff, rec, keepFields[i], outBed, FALSE);
        }
    fprintf(outBed, "\n");

    // write out extra tags for this line, if any:
    if (outExtra)
        {
        fprintf(outExtra, "%s", name);
        struct vcfInfoDef *def;
        for (def = vcff->infoDefs; def != NULL; def = def->next)
            {
            if (stringArrayIx(def->key, keepFields, extraFieldCount) == -1)
                {
                fprintf(outExtra, "\t");
                printVcfInfoElem(vcff, rec, def->key, outExtra, TRUE);
                }
            }
        fprintf(outExtra, "\n");
        }
    fflush(outBed);
    fflush(outExtra);
    }
}

void vcfToBed(char *vcfFileName, char *outPrefix, char *tagsToKeep)
/* vcfToBed - Convert VCF to BED9+ with optional extra fields.
 * Extra VCF tags get placed into a separate tab file for later indexing. */
{
FILE *outBed, *outExtra;
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
    hashInfoTags(vcff);
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
char *outBedName = NULL, *outExtraName = NULL;
if (endsWith(outPrefix, ".bed"))
    {
    outBedName = cloneString(outPrefix);
    if (keepCount <= vcfTagCount)
        {
        outExtraName = replaceChars(outPrefix, ".bed", ".extraFields.tab");
        outExtra = mustOpen(outExtraName, "w");
        }
    }
else
    {
    outBedName = catTwoStrings(outPrefix, ".bed");
    if (keepCount <= vcfTagCount)
        {
        outExtraName =  catTwoStrings(outPrefix, ".extraFields.tab");
        outExtra = mustOpen(outExtraName, "w");
        }
    }
outBed = mustOpen(outBedName, "w");

verbose(2, "input vcf tag count = %d\n", vcfTagCount);
verbose(2, "number of valid requested tags = %d\n", keepCount);
for (i = 0; i < keepCount; i++)
    if (keepFields[i] != NULL)
        verbose(2, "found tag: '%s'\n", keepFields[i]);

printHeaders(vcff, keepFields, keepCount, outBed, outExtra);
vcfLinesToBed(vcff, keepFields, keepCount, outBed, outExtra);
vcfFileFree(&vcff);
carefulClose(&outBed);
carefulClose(&outExtra);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fields = optionVal("fields", NULL);
vcfToBed(argv[1],argv[2], fields);
return 0;
}
