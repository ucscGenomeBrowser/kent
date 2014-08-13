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
#include "gtexSample.h"
#include "gtexTissue.h"
#include "gtexSampleData.h"
#include "gtexTissueData.h"

char *database = "hgFixed";
char *tabDir = ".";
boolean doLoad = FALSE;
boolean doRound = FALSE;
boolean median = FALSE;
int limit = 0;

#define DATA_FILE_VERSION "#1.2"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtex - Load GTEX data and sample files\n"
  "usage:\n"
  "   hgGtex dataFile samplesFile outTissuesFile\n"
  "        or\n"
  "   hgGtex [options] tableRoot dataFile samplesFile subjectsFile tissuesFile\n"
  "\n"
  "The first syntax generates a tissues file, with ids and candidate short names,\n"
  "intended for manual editing for clarity and conciseness.\n"
  "\n"
  "The second syntax creates four tables in hgFixed:\n"
  "  1. All data: a row for each gene+sample, with RPKM expression level\n"
  "  2. Median data: a row for each gene with a list of median RPKM expression levels by tissue\n"
  "  3. Samples: a row per sample, with full metadata from GTEX\n"
  "  4. Tissues: a row per tissue, with id key for median data table\n"
  "\n"
  "options:\n"
  "    -database=XXX (default %s)\n"
  "    -tab=dir - Output tab-separated files to directory.\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  "    -doRound - If true round data values\n"
  "    -limit=N - Only do limit rows of data table, for testing\n"
  , database);
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"doRound", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {NULL, 0},
};

/****************************/
/* Process sample file */

int parseSampleFileHeader(struct lineFile *lf)
/* Parse GTEX sample file header. Return number of columns */
/* TODO: return column headers in array */
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith("SAMPID", line))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
char *words[100];
int sampleCols = chopTabs(line, words);
if (sampleCols < 7 || differentString(words[6], "SMTSD"))
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
    sample->name = cloneString(words[0]);
    sample->organ = cloneString(words[5]);
    // Handle missing tissue and organ
    if (!*sample->organ)
        sample->organ = "Unannotated";
    sample->tissue = cloneString(words[6]);
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

    char *sampleId = cloneString(words[0]);
    if (!hashLookup(sampleNameHash, sampleId))
        continue;

    /*  Donor is GTEX-XXXX-* */
    chopByChar(cloneString(sampleId), '-', words, ArraySize(words));

    AllocVar(sample);
    sample->name = sampleId;
    sample->donor = cloneString(words[1]);
    verbose(4, "parseSamples: lookup %s in tissueNameHash\n", words[6]);
    sample->tissue = hashMustFindVal(tissueNameHash, words[6]);
    verbose(3, "Adding sample: \'%s'\n", sampleId);
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

struct hashEl *groupSamples(struct hash *sampleHash, struct slName *sampleIds, 
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
    verbose(4, "groupSamples: lookup %s in sampleHash\n", sampleId->name);
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

struct hashEl *tissueOffsets = hashElListHash(tissueOffsetHash);
slSort(&tissueOffsets, hashElCmp);

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

return tissueOffsets;
}

/****************************/
/* Process data file */

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

int geneCount = sqlUnsigned(words[0]);
int headerSampleCount = sqlUnsigned(words[1]);
if (headerSampleCount > sampleCount)
    errAbort("data file has more samples than sample file");
verbose(2, "GTEX data file: %d genes, %d samples\n", geneCount, headerSampleCount);

/* Parse header line containing sample names */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);
if (!startsWith("Name\tDescription", line))
    errAbort("%s unrecognized format", lf->fileName);
char *sampleIds[sampleCount+3];
int dataSampleCount = chopTabs(line, sampleIds) - 2;
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

void dataRowsOut(char **row, 
                FILE *allFile, int sampleCount,
                FILE *medianFile, int tissueCount, 
                struct hashEl *tissueOffsets)
/* Output expression levels per sample and tissue for one gene */
{
int i = 0;
struct hashEl *el;
struct sampleOffset *sampleOffset, *sampleOffsets;
double *sampleVals;
float medianVal;

/* Print geneId and tissue count to median table file */
char *gene = row[0];
fprintf(medianFile, "%s\t%d\t", gene, tissueCount);
//verbose(2, "%s\n", gene);
for (el = tissueOffsets; el != NULL; el = el->next)
    {
    /* Get values for all samples for each tissue */
    char *tissue = el->name;
    sampleOffsets = (struct sampleOffset *)el->val;
    int tissueSampleCount = slCount(sampleOffsets);
    verbose(3, "%s\t%s\t%d samples\t", gene, tissue, tissueSampleCount);
    verbose(3, "\n");
    AllocArray(sampleVals, tissueSampleCount);
    for (i = 0, sampleOffset = sampleOffsets; i<tissueSampleCount;
                sampleOffset = sampleOffset->next, i++)
        {
        double val = sqlDouble(row[(sampleOffset->offset)+2]);
        verbose(3, "    %s\t%s\t%s\t%0.3f\n", gene, tissue, sampleOffset->sample, val);
        fprintf(allFile, "%s\t%s\t%s\t", gene, tissue, sampleOffset->sample);
        if (doRound)
            fprintf(allFile, "%d", round(val));
        else 
            fprintf(allFile, "%0.3f", val);
        fprintf(allFile, "\n");
        sampleVals[i] = val;
        }
    /* Compute median */
    medianVal = (float)doubleMedian(tissueSampleCount, sampleVals);
    //verbose(2, "MEDIAN: %0.3f\n", medianVal);

    /* If no rounding, then print as float, otherwise round */
    if (doRound)
        fprintf(medianFile, "%d,", round(medianVal));
    else 
        fprintf(medianFile, "%0.3f,", medianVal);
    freez(&sampleVals);
    }
fprintf(medianFile, "\n");
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
struct slName *sampleIds = parseDataFileHeader(lf, hashNumEntries(sampleHash), &dataSampleCount);
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

void hgGtex(char *tableRoot, char *dataFile, char *sampleFile, char *subjectFile, char *tissuesFile)
/* Main function to load tables*/
{
char *line;
int wordCount;
FILE *f = NULL;
FILE *tissueDataFile = NULL, *sampleDataFile = NULL;
int dataCount = 0;
struct lineFile *lf;
int dataSampleCount = 0;
struct hash  *sampleHash;

/* Load tissue file as we will use short tissue names, not long descriptions as in sample file */
struct gtexTissue *tissue, *tissues;
tissues = gtexTissueLoadAllByChar(tissuesFile, '\t');
struct hash *tissueNameHash = hashNew(0);
for (tissue = tissues; tissue != NULL; tissue = tissue->next)
    {
    verbose(4, "Adding to tissueNameHash: key=%s, val=%s, group=%s\n", tissue->description, tissue->name, tissue->organ);
    hashAdd(tissueNameHash, tissue->description, tissue->name);
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
struct slName *sampleIds = parseDataFileHeader(lf, sampleCount, &dataSampleCount);
verbose(3, "%d samples in data file\n", dataSampleCount);
lineFileClose(&lf);

/* Parse sample file, creating sample objects for all samples in data file */
lf = lineFileOpen(sampleFile, TRUE);
parseSampleFileHeader(lf);
sampleHash = parseSamples(lf, sampleIds, sampleCols, tissueNameHash);
lineFileClose(&lf);

/* Get offsets in data file for samples by tissue */
struct hashEl *tissueOffsets = groupSamples(sampleHash, sampleIds, dataSampleCount);
int tissueCount = slCount(tissueOffsets);
verbose(2, "tissue count: %d\n", tissueCount);

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

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);

    /* Load sample table */
    gtexSampleCreateTable(conn, sampleTable);
    hgLoadTabFile(conn, tabDir, sampleTable, &f);
    hgRemoveTabFile(tabDir, sampleTable);

    /* Load tissue table */
    char tissueTable[64];
    safef(tissueTable, sizeof(tissueTable), "%sTissue", tableRoot);
    gtexTissueCreateTable(conn, tissueTable);
    hgLoadNamedTabFile(conn, NULL, tissueTable, tissuesFile, NULL);

    sqlDisconnect(&conn);
    }
else
    {
    carefulClose(&f);
    }

/* Ready to process data items */

/* Create sample data and tissue (median) data table files */
char sampleDataTable[64];
safef(sampleDataTable, sizeof(sampleDataTable), "%sSampleData", tableRoot);
sampleDataFile = hgCreateTabFile(tabDir,sampleDataTable);

char tissueDataTable[64];
safef(tissueDataTable, sizeof(tissueDataTable), "%sTissueData", tableRoot);
tissueDataFile = hgCreateTabFile(tabDir,tissueDataTable);

/* Open GTEX expression data file, and read header lines.  Return list of sample IDs */
lf = lineFileOpen(dataFile, TRUE);
parseDataFileHeader(lf, sampleCount, NULL);

/* Parse expression values in data file. Each row is a gene */
char **row;
AllocArray(row, dataSampleCount+2);
while (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopByChar(line, '\t', row, dataSampleCount+2);
    if (wordCount-2 != dataSampleCount)
        errAbort("Expecting %d data points, got %d line %d of %s", 
		dataSampleCount, wordCount-2, lf->lineIx, lf->fileName);
    dataRowsOut(row, sampleDataFile, dataSampleCount, 
                        tissueDataFile, tissueCount, tissueOffsets);
    dataCount++;
    if (limit != 0 && dataCount >= limit)
        break;
    }
lineFileClose(&lf);

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);

    // Load sample data table 
    gtexSampleDataCreateTable(conn, sampleDataTable);
    hgLoadTabFile(conn, tabDir, sampleDataTable, &sampleDataFile);
    hgRemoveTabFile(tabDir, sampleDataTable);

    // Load tissue data (medians) table
    gtexTissueDataCreateTable(conn, tissueDataTable);
    hgLoadTabFile(conn, tabDir, tissueDataTable, &tissueDataFile);
    hgRemoveTabFile(tabDir, tissueDataTable);

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
doRound = optionExists("doRound");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
if (argc == 4)
    hgGtexTissues(argv[1], argv[2], argv[3]);
else if (argc == 6)
    hgGtex(argv[1], argv[2], argv[3], argv[4], argv[5]);
else
    usage();
return 0;
}
