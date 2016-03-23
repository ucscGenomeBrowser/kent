/* hgGtex - Load data from NIH Common Fund Gene Tissue Expression (GTEX) portal.
                In the style of hgGnfMicroarray */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "hgRelate.h"
#include "gtexInfo.h"
#include "gtexDonor.h"
#include "gtexSample.h"
#include "gtexTissue.h"
#include "gtexSampleData.h"
#include "gtexTissueData.h"
#include "gtexTissueMedian.h"

/* globals */
char *database = "hgFixed";
char *tabDir = ".";
boolean doLoad = FALSE;
boolean doData = FALSE;
boolean doRound = FALSE;
boolean median = FALSE;
boolean exon = FALSE;
boolean dropZeros = FALSE;
char *releaseDate = NULL;
int limit = 0;

int dataSampleCount = 0;
FILE *sampleDataFile, *tissueDataFile;
FILE *tissueMedianAllFile, *tissueMedianFemaleFile, *tissueMedianMaleFile;
struct hash *donorHash;

#define DATA_FILE_VERSION "#1.2"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtex - Load GTEX data and sample files\n"
  "usage:\n"
  "   hgGtex dataFile samplesFile outTissuesFile\n"
  "        or\n"
  "   hgGtex [options] tableRoot version dataFile samplesFile subjectsFile tissuesFile\n"
  "\n"
  "The first syntax generates a tissues file, with ids and candidate short names,\n"
  "intended for manual editing for clarity and conciseness.\n"
  "\n"
  "The second syntax creates tables in hgFixed:\n"
  "  1. All data (rootSampleData) : a row for each gene+sample, with RPKM expression level\n"
  "  2. Tissue data (rootTissueData): a row for each gene+tissue, with min/max/q1/q2/median\n"
  "  3. Median data (rootTissueMedian[All|Female|Male]): a row for each gene with a list of\n"
  "             median RPKM expression levels by tissue. There are 3 of these\n"
  "  4. Sample (rootSample): a row per sample, with metadata from GTEX\n"
  "  5. Donor (rootDonor): a row per subject, with metadata from GTEX\n"
  "  6. Info: Info: version, release date, and max median score (merge this into existing\n"
  "             file if any)\n"
  "\n"
  "options:\n"
  "    -database=XXX (default %s)\n"
  "    -tab=dir - Output tab-separated files to directory.\n"
  "    -noLoad  - Don't load database and don't clean up tab files\n"
  "    -noData  - Don't create data files/tables (just metadata)\n"
  "    -doRound - Round data values\n"
  "    -dropZeros - Ignore zero-valued data rows (not recommended)\n"
  "    -limit=N - Only do limit rows of data table, for testing\n"
  "    -exon -    Create exon tables instead of gene tables\n" 
  "                    1. All data (rootSampleExonData)\n"
  "                    2. Tissue data (rootTissueExonData)\n"
  "                    3. Median data (rootTissueExonMedian)\n"
  "    -releaseDate=YY-MM-DD - Set release date (o/w use 'now')\n"
  , database);
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"noData", OPTION_BOOLEAN},
   {"doRound", OPTION_BOOLEAN},
   {"dropZeros", OPTION_BOOLEAN},
   {"exon", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {NULL, 0},
};

/****************************/
/* Deal with donors */

#define SUBJECT_FIRST_FIELD_LABEL "SUBJID"

#define SUBJECT_NAME_FIELD 0
        // GTEX-XXXX
#define SUBJECT_GENDER_FIELD 1
        // 1=Male (hmmf), 2=Female
#define donorGetGender(x) (sqlUnsigned(x) == 1 ? "M" : "F")
#define donorIsFemale(x) (sameString(x, "F"))
#define SUBJECT_AGE_FIELD 2
        // e.g. 60-69 years
#define SUBJECT_DEATH_FIELD 3   
        // Hardy scale 0-4 or empty (unknown?).  See .as for scale definitions.
#define donorGetDeathClass(x) (isEmpty(x) ? -1 : sqlUnsigned(x))
#define SUBJECT_LAST_FIELD SUBJECT_DEATH_FIELD

int donorGetAge(char *age)
/* Change '60-69 yrs' to numeric 60 */
{
char *pos;
char *ageBuf = cloneString(age);
pos = stringIn("-", ageBuf);
if (pos == NULL)
    return 0;
*pos = '\0';
return sqlUnsigned(ageBuf);
}

char *donorFromSampleId(char *sampleId)
/* Parse donorId from sampleId */
/*  donor is first 2 components of sampleId: GTEX-XXXX */
{
char *donor = cloneString(sampleId);
*strchr(strchr(donor, '-')+1, '-') = 0;
return donor;
}

boolean sampleIsFemale(char *sampleId)
/* Return TRUE if sample is from a female donor */
{
char *donorId = donorFromSampleId(sampleId);
struct gtexDonor *donor = hashMustFindVal(donorHash, donorId);
return donorIsFemale(donor->gender);
}

struct gtexDonor *parseSubjectFile(struct lineFile *lf)
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith(SUBJECT_FIRST_FIELD_LABEL, line))
    errAbort("unrecognized format - expecting subject file header in %s first line", lf->fileName);

char *words[100];
int wordCount;
struct gtexDonor *donor=NULL, *donors = NULL;

while (lineFileNext(lf, &line, NULL))
    {
    /* Convert line to donor record */
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, SUBJECT_LAST_FIELD+1, wordCount);

    AllocVar(donor);
    char *subject = cloneString(words[SUBJECT_NAME_FIELD]);
    char *gender = cloneString(words[SUBJECT_GENDER_FIELD]);
    char *age = cloneString(words[SUBJECT_AGE_FIELD]);
    char *deathClass = cloneString(words[SUBJECT_DEATH_FIELD]);

    verbose(3, "subject: %s %s %s %s\n", subject, gender, age, deathClass);
    donor->name = subject;
    donor->gender = donorGetGender(gender);
    donor->age = donorGetAge(age);
    donor->deathClass = donorGetDeathClass(deathClass);
    slAddTail(&donors, donor);
    //slAddHead(&donors, donor);
    //slReverse(&donors);
    }
verbose(2, "Found %d donors\n", slCount(donors));
return(donors);
}

/****************************/
/* Process sample file */

#define SAMPLE_FIRST_FIELD_LABEL "SAMPID"
#define SAMPLE_TISSUE_FIELD_LABEL "SMTSD"

// NOTE: more robust to include map of GTEX field names (do this if we include RNA-seqC metrics)

#define SAMPLE_NAME_FIELD_INDEX 0
#define SAMPLE_AUTOLYSIS_FIELD_INDEX 1
#define SAMPLE_CENTERS_FIELD_INDEX 2
#define SAMPLE_PATHOLOGY_FIELD_INDEX 3
#define SAMPLE_RIN_FIELD_INDEX 4
#define SAMPLE_ORGAN_FIELD_INDEX 5
#define SAMPLE_TISSUE_FIELD_INDEX 6
#define SAMPLE_ISCHEMIC_FIELD_INDEX 7

#define V6
/* Sample file changed format between V4 and V6 */
#ifdef V6
   #define SAMPLE_BATCH_FIELD_INDEX 10
   #define SAMPLE_ISOLATION_FIELD_INDEX 11
   #define SAMPLE_DATE_FIELD_INDEX 12
#else
   #define SAMPLE_BATCH_FIELD_INDEX 8
   #define SAMPLE_ISOLATION_FIELD_INDEX 9
   #define SAMPLE_DATE_FIELD_INDEX 10
#endif


int parseSampleFileHeader(struct lineFile *lf)
/* Parse GTEX sample file header. Return number of columns */
/* TODO: return column headers in array */
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith(SAMPLE_FIRST_FIELD_LABEL, line))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
char *words[100];
int sampleCols = chopTabs(line, words);
if (sampleCols < SAMPLE_TISSUE_FIELD_INDEX+1 || 
        differentString(words[SAMPLE_TISSUE_FIELD_INDEX], SAMPLE_TISSUE_FIELD_LABEL))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
return sampleCols;
}

struct sampleTissue {
    struct sampleTissue *next;
    char *name;
    char *tissue;
    char *organ;
    };

struct hash *parseSampleTissues(struct lineFile *lf, int expectedCols)
/* Parse sample descriptions. Return hash of samples with tissue info */
{
char *line;
int wordCount;
char *words[100];
struct sampleTissue *sample;
struct hash *sampleHash = hashNew(0);

while (lineFileNext(lf, &line, NULL))
    {
    /* Convert line to sample record */
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, expectedCols, wordCount);

    AllocVar(sample);
    sample->name = cloneString(words[SAMPLE_NAME_FIELD_INDEX]);
    sample->organ = cloneString(words[SAMPLE_ORGAN_FIELD_INDEX]);
    // Handle missing tissue and organ
    if (!*sample->organ)
        sample->organ = "Unannotated";
    sample->tissue = cloneString(words[SAMPLE_TISSUE_FIELD_INDEX]);
    if (!*sample->tissue)
        sample->tissue = "Unannotated";
    hashAdd(sampleHash, sample->name, sample);
    }
verbose(2, "Found %d samples in sample file\n", hashNumEntries(sampleHash));
return sampleHash;
}

struct hash *parseSamples(struct lineFile *lf, struct slName *sampleIds, int expectedCols, 
                                struct hash *tissueNameHash)
/* Parse sample descriptions and populate sample objects using tissue name from hash. 
   Limit to samples present in data file (in sampleId list).  Return hash keyed on sample name */
{
char *line;
int wordCount;
char *words[100];
struct hash *hash = hashNew(0);
struct gtexSample *sample = NULL;

struct hash *sampleNameHash = hashNew(0);
struct slName *sampleId;
for (sampleId = sampleIds; sampleId != NULL; sampleId = sampleId->next)
    {
    hashAdd(sampleNameHash, sampleId->name, NULL);
    }
int i = 0;
while (lineFileNext(lf, &line, NULL))
    {
    /* Convert line to sample record */
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, expectedCols, wordCount);
    i++;

    char *sampleId = cloneString(words[SAMPLE_NAME_FIELD_INDEX]);
    if (!hashLookup(sampleNameHash, sampleId))
        continue;

    AllocVar(sample);
    sample->sampleId = sampleId;

    sample->donor = donorFromSampleId(sampleId);

    verbose(4, "parseSamples: lookup %s in tissueNameHash\n", words[SAMPLE_TISSUE_FIELD_INDEX]);
    sample->tissue = hashMustFindVal(tissueNameHash, words[SAMPLE_TISSUE_FIELD_INDEX]);

    verbose(4, "autolysis=%s, ischemic=%s, rin=%s, pathNotes=%s, sites=%s, batch=%s, isolation=%s, date=%s\n", 
        words[SAMPLE_AUTOLYSIS_FIELD_INDEX],
        words[SAMPLE_ISCHEMIC_FIELD_INDEX],
        words[SAMPLE_RIN_FIELD_INDEX],
        words[SAMPLE_PATHOLOGY_FIELD_INDEX],
        words[SAMPLE_CENTERS_FIELD_INDEX],
        words[SAMPLE_BATCH_FIELD_INDEX],
        words[SAMPLE_ISOLATION_FIELD_INDEX],
        words[SAMPLE_DATE_FIELD_INDEX]);

    char *word = words[SAMPLE_AUTOLYSIS_FIELD_INDEX];
    sample->autolysisScore = (isNotEmpty(word) ? sqlSigned(word) : -1);

    //word = words[SAMPLE_ISCHEMIC_FIELD_INDEX];
    //sample->ischemicTime = (word ? cloneString(word) : "unknown");
#ifdef V6
    // Feb 2016: this field is not in posted V6 sample file.  Broad has been notified.
    sample->ischemicTime = "n/a";
#else
    sample->ischemicTime = cloneString(words[SAMPLE_ISCHEMIC_FIELD_INDEX]);
#endif
    word = words[SAMPLE_RIN_FIELD_INDEX];
    sample->rin = (isNotEmpty(word) ? sqlFloat(word) : 0);

    //word = words[SAMPLE_PATHOLOGY_FIELD_INDEX];
    sample->pathNotes = cloneString(words[SAMPLE_PATHOLOGY_FIELD_INDEX]);

    // Sites may be comma-sep list with embedded spaces.  Strip the spaces
    word = cloneString(words[SAMPLE_CENTERS_FIELD_INDEX]);
    stripChar(word, ' ');
    sample->collectionSites = word;

    // These are always populated
    sample->batchId = cloneString(words[SAMPLE_BATCH_FIELD_INDEX]);

    // Another field with embedded spaces -- strip them out
    word = cloneString(words[SAMPLE_ISOLATION_FIELD_INDEX]);
    subChar(word, ' ', '_');
    sample->isolationType = word;

    sample->isolationDate = cloneString(words[SAMPLE_DATE_FIELD_INDEX]);
    verbose(4, "Adding sample: \'%s'\n", sampleId);
    hashAdd(hash, sampleId, sample);
    }
verbose(2, "Found %d data samples out of %d in sample file\n", hashNumEntries(hash), i);
return hash;
}


struct sampleOffset {
        struct sampleOffset *next;
        char *sample;
        unsigned int offset;
        };

struct hash *groupSamplesByTissue(struct hash *sampleHash, struct slName *sampleIds, 
                                int sampleCount)
/* Group samples by tissue for median option */
{
struct hash *tissueOffsetHash = hashNew(0);
struct sampleOffset *offset;
struct hashEl *el;
struct gtexSample *sample;
int i;

struct slName *sampleId;
for (i=0, sampleId = sampleIds; sampleId != NULL; sampleId = sampleId->next, i++)
    {
    verbose(4, "groupSamplesByTissue: lookup %s in sampleHash\n", sampleId->name);
    sample = hashMustFindVal(sampleHash, sampleId->name);
    AllocVar(offset);
    offset->offset = i;
    offset->sample = cloneString(sampleId->name);
    el = hashLookup(tissueOffsetHash, sample->tissue);
    if (el)
        slAddHead((struct sampleOffset *)el->val, offset);
    else
        hashAdd(tissueOffsetHash, sample->tissue, offset);
    }

//#define DEBUG 1
#ifdef DEBUG
uglyf("tissue count: %d\n", slCount(tissueOffsets));
for (el = tissueOffsets; el != NULL; el = el->next)
    {
    uglyf("%s\t", el->name);
    for (offset = (struct slUnsigned *)el->val; offset->next; offset = offset->next)
        uglyf("%d,", offset->val);
    uglyf("\n");
    }
#endif

return tissueOffsetHash;
}

/****************************/
/* Process data file */

#define DATA_GENE_COUNT_FIELD_INDEX 0
#define DATA_SAMPLE_COUNT_FIELD_INDEX 1

struct slName *parseDataFileHeader(struct lineFile *lf, int sampleCount, int *dataSampleCountRet)
/* Parse version, info, and header lines. Return array of sample Ids in order from header */
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith(DATA_FILE_VERSION, line))
    errAbort("unrecognized format - expecting %s in %s first line", 
                DATA_FILE_VERSION, lf->fileName);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);

/* Parse #genes #samples */
char *words[100];
int wordCount = chopLine(line, words);
if (wordCount != 2)
    errAbort("%s is truncated: expecting <#genes> <#samples>", lf->fileName);

int geneCount = sqlUnsigned(words[DATA_GENE_COUNT_FIELD_INDEX]);
int headerSampleCount = sqlUnsigned(words[DATA_SAMPLE_COUNT_FIELD_INDEX]);
if (headerSampleCount > sampleCount)
    errAbort("data file has more samples than sample file");
verbose(2, "GTEX data file: %d genes, %d samples\n", geneCount, headerSampleCount);

/* Parse header line containing sample names */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);
if (!startsWith("Name\tDescription", line))
    errAbort("%s unrecognized format", lf->fileName);
char *sampleIds[sampleCount+3];
dataSampleCount = chopTabs(line, sampleIds) - 2;
if (headerSampleCount != dataSampleCount)
    warn("Sample count mismatch in data file: header=%d, columns=%d\n",
                headerSampleCount, dataSampleCount);
verbose(3, "dataSampleCount=%d\n", dataSampleCount);
if (dataSampleCountRet)
    *dataSampleCountRet = dataSampleCount;
char **samples = &sampleIds[2];
struct slName *idList = slNameListFromStringArray(samples, sampleCount+3);
return idList;
}

struct slName *parseExonDataFileHeader(struct lineFile *lf, int sampleCount, 
                                        int *dataSampleCountRet)
/* Parse header line. Return array of sample Ids in order from header */
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);
if (!startsWith("Id", line))
    errAbort("%s unrecognized format", lf->fileName);
char *sampleIds[sampleCount+2];
int dataSampleCount = chopTabs(line, sampleIds) - 1;
verbose(3, "dataSampleCount=%d\n", dataSampleCount);
if (dataSampleCountRet)
    *dataSampleCountRet = dataSampleCount;
char **samples = &sampleIds[1];  // skip over Id column
struct slName *idList = slNameListFromStringArray(samples, dataSampleCount+2);
return idList;
}

void dataRowsOut(char **row, int tissueCount,  char *tissueOrder[], struct hash *tissueOffsets, 
                        double *maxScoreRet, double *maxMedianRet)
/* Output expression levels per sample and tissue for one gene. Return max score, median computed */
{
int i=0, j=0;
struct sampleOffset *sampleOffset, *sampleOffsets;
double *sampleVals, *femaleSampleVals, *maleSampleVals;
double maxMedian = 0;
double maxScore = 0;

/* Print geneId and tissue count to median table files */
char *gene = row[0];
fprintf(tissueMedianAllFile, "%s\t%d\t", gene, tissueCount);
fprintf(tissueMedianFemaleFile, "%s\t%d\t", gene, tissueCount);
fprintf(tissueMedianMaleFile, "%s\t%d\t", gene, tissueCount);
verbose(3, "%s\n", gene);

for (i=0; i<tissueCount; i++)
    {
    char *tissue = tissueOrder[i];
    /* Get values for all samples for each tissue */
    sampleOffsets = (struct sampleOffset *)hashMustFindVal(tissueOffsets, tissue);
    int tissueSampleCount = slCount(sampleOffsets);
    verbose(3, "%s\t%s\t%d samples\t", gene, tissue, tissueSampleCount);
    verbose(3, "\n");
    AllocArray(sampleVals, tissueSampleCount);
    AllocArray(femaleSampleVals, tissueSampleCount);
    AllocArray(maleSampleVals, tissueSampleCount);
    int mj =0, fj = 0;
    for (j = 0, sampleOffset = sampleOffsets; j < tissueSampleCount;
                sampleOffset = sampleOffset->next, j++)
        {
        // skip over Name and Description fields to find first score for this gene
        // WARNING: row parsing should be handled in parse routines
        int skip = (exon ? 1 : 2);
        double val = sqlDouble(row[(sampleOffset->offset) + skip]);
        if (dropZeros && val == 0.0)
            continue;

        // Output to sample data file
        //     TODO: use gtexSampleDataOut
        verbose(3, "    %s\t%s\t%s\t%0.3f\n", gene, sampleOffset->sample, tissue, val);
        fprintf(sampleDataFile, "%s\t%s\t%s\t", gene, sampleOffset->sample, tissue);
        if (doRound)
            fprintf(sampleDataFile, "%d", round(val));
        else 
            fprintf(sampleDataFile, "%0.3f", val);
        fprintf(sampleDataFile, "\n");
        sampleVals[j] = val;
        maxScore = max(val, maxScore);

        // create gender subsets
        if (sampleIsFemale(sampleOffset->sample))
            femaleSampleVals[fj++] = val;
        else
            maleSampleVals[mj++] = val;
        }
    /* Compute stats for all samples */
    double min, q1, median, q3, max;
    doubleBoxWhiskerCalc(j, sampleVals, &min, &q1, &median, &q3, &max);
    //medianVal = (float)doubleMedian(tissueSampleCount, sampleVals);
    maxMedian = max(median, maxMedian);
    verbose(3, "median %s %0.3f\n", tissue, median);
    /* If no rounding, then print as float, otherwise round */
    if (doRound)
        fprintf(tissueMedianAllFile, "%d,", round(median));
    else 
        fprintf(tissueMedianAllFile, "%0.3f,", median);

    /* Compute stats for gender subsets */
    median = 0.0;
    if (fj)
        doubleBoxWhiskerCalc(fj, femaleSampleVals, &min, &q1, &median, &q3, &max);
    if (doRound)
        fprintf(tissueMedianFemaleFile, "%d,", round(median));
    else 
        fprintf(tissueMedianFemaleFile, "%0.3f,", median);
        
    median = 0.0;
    if (mj)
        doubleBoxWhiskerCalc(mj, maleSampleVals, &min, &q1, &median, &q3, &max);
    if (doRound)
        fprintf(tissueMedianMaleFile, "%d,", round(median));
    else 
        fprintf(tissueMedianMaleFile, "%0.3f,", median);

    // calculate other stats
    // print row in tissue data file
    fprintf(tissueDataFile, "%s\t%s\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t%0.3f\n", 
                                    gene, tissue, min, q1, median, q3, max);
    freez(&sampleVals);
    }
fprintf(tissueMedianAllFile, "\n");
fprintf(tissueMedianFemaleFile, "\n");
fprintf(tissueMedianMaleFile, "\n");
verbose(3, "max median: %0.3f\n", maxMedian);
if (maxScoreRet)
    *maxScoreRet = maxScore;
if (maxMedianRet)
    *maxMedianRet = maxMedian;
}

/****************************/
/* Deal with tissues */

char *makeTissueName(char *description)
/* Create a single word camel-case name from a tissue description */
{
char *words[10];
int count = chopByWhite(cloneString(description), words, sizeof(words));
struct dyString *ds = newDyString(0);
int i;
for (i=0; i<count; i++)
    {
    char *word = words[i];
    if (!isalpha(word[0]) || !isalpha(word[strlen(word)-1]))
        continue;
    dyStringAppend(ds, word);
    }
char *newName = dyStringCannibalize(&ds);
newName[0] = tolower(newName[0]);
return newName;
}

void tissuesOut(char *outFile, struct hash *tissueSampleHash)
/* Write tissues file */
{
struct hashEl *helList = hashElListHash(tissueSampleHash);
slSort(&helList, hashElCmp);

FILE *f = mustOpen(outFile, "w");
struct hashEl *el;
struct gtexTissue *tissue;
int i;
for (i=0, el = helList; el != NULL; el = el->next, i++)
    {
    struct sampleTissue *sample = (struct sampleTissue *)el->val;
    AllocVar(tissue);
    tissue->id = i;
    tissue->description = sample->tissue;
    tissue->organ = sample->organ;
    tissue->name = makeTissueName(tissue->description);
    tissue->color = 0;
    // TODO: get GTEX colors from file
    gtexTissueOutput(tissue, f, '\t', '\n');
    }
carefulClose(&f);
}


/****************************/
/* Main functions */

void hgGtexTissues(char *dataFile, char *sampleFile, char *outFile)
/* Create a tissues file with aliases for use in other tables.  Limit to tissues
 * in the data file, since these will be indexes into comma-sep data values.
 * This is a separate step to support manual editing of aliases for clarity & conciseness */
{
struct lineFile *lf;
int i;

/* Parse tissue info from samples file */
lf = lineFileOpen(sampleFile, TRUE);
int sampleCols = parseSampleFileHeader(lf);
struct hash *sampleHash = parseSampleTissues(lf, sampleCols);
verbose(2, "%d samples in samples file\n", hashNumEntries(sampleHash));
lineFileClose(&lf);

/* Get sample IDs from header in data file */
lf = lineFileOpen(dataFile, TRUE);
int dataSampleCount = 0;
struct slName *sampleIds = NULL;
sampleIds = parseDataFileHeader(lf, hashNumEntries(sampleHash), &dataSampleCount);
verbose(2, "%d samples in data file\n", dataSampleCount);
lineFileClose(&lf);

/* Gather tissues from samples */
struct hash *tissueSampleHash = hashNew(0);
struct sampleTissue *sample;
struct slName *sampleId;
for (i=0, sampleId = sampleIds; sampleId != NULL; sampleId = sampleId->next, i++)
    {
    verbose(4, "hgGtexTissues: lookup %s in sampleHash\n", sampleId->name);
    sample = hashMustFindVal(sampleHash, sampleId->name);
    if (!hashLookup(tissueSampleHash, sample->tissue))
        hashAdd(tissueSampleHash, sample->tissue, sample);
    }
verbose(3, "%d tissues\n", hashNumEntries(tissueSampleHash));
/* Write tissues file */
tissuesOut(outFile, tissueSampleHash);
}

void hgGtex(char *tableRoot, char *version, 
                char *dataFile, char *sampleFile, char *subjectFile, char *tissuesFile)
/* Main function to load tables*/
{
char *line;
int wordCount;
FILE *f = NULL;
FILE *infoFile = NULL;
int dataCount = 0;
struct lineFile *lf;
int dataSampleCount = 0;
struct hash  *sampleHash;

/* Load tissue file as we will use short tissue names, not long descriptions as in sample file */
struct gtexTissue *tissue, *tissues;
tissues = gtexTissueLoadAllByChar(tissuesFile, '\t');
struct hash *tissueNameHash = hashNew(0);
donorHash = hashNew(0);
char **tissueOrder = NULL;
AllocArray(tissueOrder, slCount(tissues));
for (tissue = tissues; tissue != NULL; tissue = tissue->next)
    {
    verbose(4, "Adding to tissueNameHash: id=%d, key=%s, val=%s, group=%s\n", tissue->id, tissue->description, tissue->name, tissue->organ);
    hashAdd(tissueNameHash, tissue->description, tissue->name);
    tissueOrder[tissue->id] = tissue->name;
    }
verbose(3, "tissues in hash: %d\n", hashNumEntries(tissueNameHash));

/* Count samples in sample file */
lf = lineFileOpen(sampleFile, TRUE);
int sampleCols = parseSampleFileHeader(lf);
sampleHash = parseSampleTissues(lf, sampleCols);
int sampleCount = hashNumEntries(sampleHash);
verbose(2, "%d samples in samples file\n", sampleCount);
lineFileClose(&lf);

/* Open GTEX expression data file, and read header lines.  Return list of sample IDs */
lf = lineFileOpen(dataFile, TRUE);
struct slName *sampleIds;
if (exon)
    sampleIds = parseExonDataFileHeader(lf, hashNumEntries(sampleHash), &dataSampleCount);
else
    sampleIds = parseDataFileHeader(lf, hashNumEntries(sampleHash), &dataSampleCount);
verbose(3, "%d samples in data file\n", dataSampleCount);
lineFileClose(&lf);

/* Parse sample file, creating sample objects for all samples in data file */
lf = lineFileOpen(sampleFile, TRUE);
parseSampleFileHeader(lf);
sampleHash = parseSamples(lf, sampleIds, sampleCols, tissueNameHash);
lineFileClose(&lf);

/* Get offsets in data file for samples by tissue */
struct hash *tissueOffsets = groupSamplesByTissue(sampleHash, sampleIds, dataSampleCount);
int tissueCount = hashNumEntries(tissueOffsets);
verbose(2, "tissue count: %d\n", tissueCount);

if (!exon)
    {
    /* Create sample table with samples ordered as in data file */
    char sampleTable[64];
    safef(sampleTable, sizeof(sampleTable), "%sSample", tableRoot);
    f = hgCreateTabFile(tabDir, sampleTable);
    struct gtexSample *sample;
    struct slName *sampleId;
    for (sampleId = sampleIds; sampleId != NULL; sampleId = sampleId->next)
        {
        verbose(4, "hgGtex: lookup %s in sampleHash\n", sampleId->name);
        sample = hashMustFindVal(sampleHash, sampleId->name);
        gtexSampleOutput(sample, f, '\t', '\n');
        }
    /* Load subjects (donors) file and write to table file */
    struct gtexDonor *donor, *donors;
    lf = lineFileOpen(subjectFile, TRUE);
    donors = parseSubjectFile(lf);
    verbose(2, "%d donors in subjects file\n", slCount(donors));
    lineFileClose(&lf);
    char donorTable[64];
    safef(donorTable, sizeof(donorTable), "%sDonor", tableRoot);
    FILE *donorFile = hgCreateTabFile(tabDir, donorTable);
    for (donor = donors; donor != NULL; donor = donor->next)
        {
        gtexDonorOutput(donor, donorFile, '\t', '\n');
        hashAdd(donorHash, donor->name, donor);
        }

    if (doLoad)
        {
        struct sqlConnection *conn = sqlConnect(database);

        /* Load sample table */
        verbose(2, "Creating sample table\n");
        gtexSampleCreateTable(conn, sampleTable);
        hgLoadTabFile(conn, tabDir, sampleTable, &f);
        hgRemoveTabFile(tabDir, sampleTable);

        /* Load tissue table */
        char tissueTable[64];
        verbose(2, "Creating tissue table\n");
        safef(tissueTable, sizeof(tissueTable), "%sTissue", tableRoot);
        gtexTissueCreateTable(conn, tissueTable);
        char dir[128];
        char fileName[64];
        splitPath(tissuesFile, dir, fileName, NULL);
        hgLoadNamedTabFile(conn, dir, tissueTable, fileName, NULL);

        /* Load donor table */
        verbose(2, "Creating donor table\n");
        gtexDonorCreateTable(conn, donorTable);
        hgLoadTabFile(conn, tabDir, donorTable, &donorFile);
        hgRemoveTabFile(tabDir, donorTable);

        sqlDisconnect(&conn);
        }
    else
        {
        carefulClose(&f);
        carefulClose(&donorFile);
        }
    }
if (!doData)
    return;

/* Ready to process data items */

/* Create sample data file */
char sampleDataTable[64];
safef(sampleDataTable, sizeof(sampleDataTable), "%s%sSampleData", tableRoot, exon ? "Exon": "");
sampleDataFile = hgCreateTabFile(tabDir,sampleDataTable);


/* Create tissue median files */
char tissueMedianAllTable[64], tissueMedianFemaleTable[64], tissueMedianMaleTable[64];
safef(tissueMedianAllTable, sizeof(tissueMedianAllTable), "%s%sTissueMedianAll", 
                tableRoot, exon ? "Exon": "");
tissueMedianAllFile = hgCreateTabFile(tabDir,tissueMedianAllTable);
safef(tissueMedianFemaleTable, sizeof(tissueMedianFemaleTable), "%s%sTissueMedianFemale", 
                tableRoot, exon ? "Exon": "");
tissueMedianFemaleFile= hgCreateTabFile(tabDir,tissueMedianFemaleTable);
safef(tissueMedianMaleTable, sizeof(tissueMedianMaleTable), "%s%sTissueMedianMale", 
                tableRoot, exon ? "Exon": "");
tissueMedianMaleFile = hgCreateTabFile(tabDir,tissueMedianMaleTable);

/* Create tissue summary data table */
char tissueDataTable[64];
safef(tissueDataTable, sizeof(tissueDataTable), "%s%sTissueData", tableRoot, exon ? "Exon": "");
tissueDataFile = hgCreateTabFile(tabDir,tissueDataTable);


/* Open GTEX expression data file, and read header lines.  Return list of sample IDs */
lf = lineFileOpen(dataFile, TRUE);
if (exon)
    parseExonDataFileHeader(lf, sampleCount, NULL);
else
    parseDataFileHeader(lf, sampleCount, NULL);

/* Parse expression values in data file. Each row is a gene (or exon)*/
char **row;
AllocArray(row, dataSampleCount+2);
double maxMedian = 0, maxScore = 0;
double rowMaxMedian, rowMaxScore;
while (lineFileNext(lf, &line, NULL))
    {
    // WARNING: header parsing should be managed in one place
    wordCount = chopByChar(line, '\t', row, dataSampleCount+2);
    int expected = wordCount - (exon ? 1 : 2);
    if (expected != dataSampleCount)
        errAbort("Expecting %d data points, got %d line %d of %s", 
		dataSampleCount, wordCount-2, lf->lineIx, lf->fileName);
    dataRowsOut(row, tissueCount, tissueOrder, tissueOffsets,
                &rowMaxMedian, &rowMaxScore);
    maxMedian = max(rowMaxMedian, maxMedian);
    maxScore = max(rowMaxScore, maxScore);
    dataCount++;
    if (limit != 0 && dataCount >= limit)
        break;
    }
lineFileClose(&lf);

/* Create info file */
char infoTable[64];
safef(infoTable, sizeof(infoTable), "%s%sInfo", tableRoot, exon ? "Exon": "");
infoFile = hgCreateTabFile(tabDir,infoTable);
struct gtexInfo *info;
AllocVar(info);
info->version = version;
info->releaseDate = releaseDate;
info->maxMedianScore = maxMedian;
gtexInfoOutput(info, infoFile, '\t', '\n');

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);

    // Load info table 
    verbose(2, "Creating info table\n");
    gtexInfoCreateTable(conn, infoTable);
    hgLoadTabFile(conn, tabDir, infoTable, &infoFile);
    hgRemoveTabFile(tabDir, infoTable);

    // Load tissue median tables
    verbose(2, "Creating all tissue medians table\n");
    gtexTissueMedianCreateTable(conn, tissueMedianAllTable);
    hgLoadTabFile(conn, tabDir, tissueMedianAllTable, &tissueMedianAllFile);
    hgRemoveTabFile(tabDir, tissueMedianAllTable);

    verbose(2, "Creating female tissue medians table\n");
    gtexTissueMedianCreateTable(conn, tissueMedianFemaleTable);
    hgLoadTabFile(conn, tabDir, tissueMedianFemaleTable, &tissueMedianFemaleFile);
    hgRemoveTabFile(tabDir, tissueMedianFemaleTable);
    // Load tissue median tables
    verbose(2, "Creating male tissue medians table\n");
    gtexTissueMedianCreateTable(conn, tissueMedianMaleTable);
    hgLoadTabFile(conn, tabDir, tissueMedianMaleTable, &tissueMedianMaleFile);
    hgRemoveTabFile(tabDir, tissueMedianMaleTable);

    // Load tissue data table
#ifdef FAST_STATS
    // Finish implementation of this if we want to add mean+whiskers to hgTracks
    verbose(2, "Creating tissue data table\n");
    gtexTissueDataCreateTable(conn, tissueDataTable);
    hgLoadTabFile(conn, tabDir, tissueDataTable, &tissueDataFile);
#endif

    // Load sample data table 
    verbose(2, "Creating sample data table\n");
    gtexSampleDataCreateTable(conn, sampleDataTable);
    hgLoadTabFile(conn, tabDir, sampleDataTable, &sampleDataFile);
    hgRemoveTabFile(tabDir, sampleDataTable);

    sqlDisconnect(&conn);
    }
else
    {
    carefulClose(&sampleDataFile);
    carefulClose(&tissueDataFile);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
database = optionVal("database", database);
doLoad = !optionExists("noLoad");
doData = !optionExists("noData");
doRound = optionExists("doRound");
dropZeros = optionExists("dropZeros");
releaseDate = optionVal("releaseDate", "0");
exon = optionExists("exon");
if (exon)
    verbose(2, "Parsing exon data file\n");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
if (argc == 4)
    hgGtexTissues(argv[1], argv[2], argv[3]);
else if (argc == 7)
    hgGtex(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
else
    usage();
return 0;
}
