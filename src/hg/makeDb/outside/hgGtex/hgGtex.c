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
#include "gtexData.h"
#include "gtexSample.h"

char *database = "hgFixed";
char *tabDir = ".";
boolean doLoad = FALSE;
boolean doRound = FALSE;
boolean median = FALSE;
int limit = 0;

#define DATA_FILE_VERSION "#1.2"

struct gtexSample *sample = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtex - Load GTEX data and sample files\n"
  "usage:\n"
  "   hgGtex tableRoot dataFile samplesFile\n"
  "This will create two tables in hgFixed, a data table with a row for\n"
  "each gene containing a list of RPKM expression levels for each sample\n"
  "and an experiment table with sample information.\n"
  "options:\n"
  "    -database=XXX (default %s)\n"
  "    -tab=dir - Output tab-separated files to directory.\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  "    -doRound - If true round data values\n"
  "    -limit=N - Only do limit rows of table, for testing\n"
  "    -median - Create tables grouped by tissue, using median expression value\n"
  , database);
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"doRound", OPTION_BOOLEAN},
   {"median", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {NULL, 0},
};

struct hash *parseSamples(struct lineFile *lf, int expectedCols, int *sampleCount)
/* Parse sample descriptions. Return hash keyed on sample name */
{
char *line;
int wordCount;
char *words[100];
char *sampleId, *tissue, *donor;
struct hash *hash = hashNew(0);
int count = 0;

while (lineFileNext(lf, &line, NULL))
    {
    /* Convert line to sample record */
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, expectedCols, wordCount);
    sampleId = cloneString(words[0]);
    tissue = cloneString(words[6]);

    /*  Donor is GTEX-XXXX-* */
    chopByChar(cloneString(sampleId), '-', words, ArraySize(words));
    donor = cloneString(words[1]);

    AllocVar(sample);
    sample->tissue = tissue;
    sample->donor = donor;
    sample->name = sampleId;
    hashAdd(hash, sampleId, sample);
    count++;
    }
if (sampleCount)
    *sampleCount = count;
return hash;
}

struct hashEl *groupSamples(struct hash *sampleHash, char **sampleIds, int sampleCount)
/* Group samples by tissue for median option */
{
struct hash *tissueHash = hashNew(0);
struct slUnsigned *offset;
struct hashEl *el;
struct gtexSample *sample;
int i;

for (i = 0; i < sampleCount; i++)
    {
    sample = hashMustFindVal(sampleHash, sampleIds[i]);
    offset = slUnsignedNew(i);
    el = hashLookup(tissueHash, sample->tissue);
    if (el)
        slAddHead((struct slUnsigned *)el->val, offset);
    else
        hashAdd(tissueHash, sample->tissue, offset);
    }

struct hashEl *tissueOffsets = hashElListHash(tissueHash);
slSort(&tissueOffsets, hashElCmp);

//#define DEBUG 1
#ifdef DEBUG
uglyf("tissue count: %d\n", slCount(tissueOffsets));
for (el = tissueOffsets; el->next; el = el->next)
    {
    uglyf("%s\t", el->name);
    for (offset = (struct slUnsigned *)el->val; offset->next; offset = offset->next)
        uglyf("%d,", offset->val);
    uglyf("\n");
    }
#endif

return tissueOffsets;
}

void sampleRowOut(FILE *f, struct gtexSample *sample, int id)
/* Output description of one sample */
{
fprintf(f, "%d\t", id);
fprintf(f, "%s\t", sample->tissue);
fprintf(f, "%s\t", sample->donor);
fprintf(f, "%s\n", sample->name);
}

void dataRowOut(FILE *f, int count, char **row)
/* Output expression levels per sample for one gene */
{
int i;
/* Print geneId and sample count */
fprintf(f, "%s\t%d\t", row[0], count);

/* If no rounding, then print as float, otherwise round */
for (i=0; i<count; ++i)
    {
    float datum = sqlFloat(row[i+2]);
    if (doRound)
        fprintf(f, "%d,", round(datum));
    else 
        fprintf(f, "%0.3f,", datum);
    } 
fprintf(f, "\n");
}

void dataMedianRowOut(FILE *f, int count, char **row, struct hashEl *tissueOffsets, int tissueCount)
/* Output expression levels per tissue for one gene */
{
int i = 0;
struct hashEl *el;
struct slUnsigned *offset, *offsets;
double *sampleVals;
int sampleCount = 0;
float medianVal;

/* Print geneId and tissue count */
fprintf(f, "%s\t%d\t", row[0], tissueCount);
for (el = tissueOffsets; el->next; el = el->next)
    {
    /* Get median value for this tissue */
    offsets = (struct slUnsigned *)el->val;
    sampleCount = slCount(offsets);
    AllocArray(sampleVals, sampleCount);
    for (i=0, offset = offsets; offset->next; offset = offset->next)
        {
        sampleVals[i] = sqlDouble(row[(offset->val)+2]);
        i++;
        }
    medianVal = (float)doubleMedian(sampleCount, sampleVals);

    /* If no rounding, then print as float, otherwise round */
    if (doRound)
        fprintf(f, "%d,", round(medianVal));
    else 
        fprintf(f, "%0.3f,", medianVal);
    freez(&sampleVals);
    }
fprintf(f, "\n");
}


void hgGtex(char *tableRoot, char *dataFile, char *sampleFile)
/* Main function */
{
struct lineFile *lf;
char *line;
int i, wordCount;
char *words[100];
FILE *f = NULL;
int dataCount = 0;
int sampleCount = 0;
struct hash *sampleHash;

/* Parse GTEX sample file */
lf = lineFileOpen(sampleFile, TRUE);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith("SAMPID", line))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
int sampleCols = chopTabs(line, words);
if (sampleCols < 7 || differentString(words[6], "SMTSD"))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
sampleHash = parseSamples(lf, sampleCols, &sampleCount);
lineFileClose(&lf);

/* Open GTEX expression data file, and read header lines */
lf = lineFileOpen(dataFile, TRUE);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith(DATA_FILE_VERSION, line))
    errAbort("unrecognized format - expecting %s in %s first line", 
                DATA_FILE_VERSION, lf->fileName);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);

/* Parse #genes #samples */
wordCount = chopLine(line, words);
if (wordCount != 2)
    errAbort("%s is truncated: expecting <#genes> <#samples>", lf->fileName);
int geneCount = sqlUnsigned(words[0]);
if (sampleCount < sqlUnsigned(words[1]))
    errAbort("data file has more samples than sample file");
sampleCount = sqlUnsigned(words[1]);
verbose(2, "GTEX data file: %d genes, %d samples\n", geneCount, sampleCount);

/* Parse header line containing sample names */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is truncated", lf->fileName);
if (!startsWith("Name\tDescription", line))
    errAbort("%s unrecognized format", lf->fileName);
char *sampleWords[sampleCount+3];
int dataSampleCount = chopTabs(line, sampleWords) - 2;
char **samples = &sampleWords[2];

/* Create sample table with samples ordered as in data file, 
        or tissues alpha-sorted (median option) */
char sampleTable[64];
struct hashEl *tissueEl, *tissueOffsets = NULL;
int tissueCount = 0;
struct gtexSample *sample;
char donorCount[10];
if (median)
    {
    tissueOffsets = groupSamples(sampleHash, samples, sampleCount);
    tissueCount = slCount(tissueOffsets);
    verbose(2, "tissue count: %d\n", tissueCount);

    safef(sampleTable, sizeof(sampleTable), "%sTissues", tableRoot);
    f = hgCreateTabFile(tabDir, sampleTable);

    AllocVar(sample);
    sample->name = "n/a";

    i = 0;
    for (tissueEl = tissueOffsets; tissueEl->next;  tissueEl = tissueEl->next)
        {
        sample->tissue = tissueEl->name;
        safef(donorCount, sizeof(donorCount), "%i", slCount(tissueEl->val));
        sample->donor = donorCount;
        sampleRowOut(f, sample, i++);
        }
    }
else
    {
    safef(sampleTable, sizeof(sampleTable), "%sSamples", tableRoot);
    f = hgCreateTabFile(tabDir, sampleTable);
    for (i=0; i<dataSampleCount; i++)
        {
        sample = hashMustFindVal(sampleHash, samples[i]);
        sampleRowOut(f, sample, i);
        }
    }

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    gtexSampleCreateTable(conn, sampleTable);
    hgLoadTabFile(conn, tabDir, sampleTable, &f);
    hgRemoveTabFile(tabDir, sampleTable);
    sqlDisconnect(&conn);
    }
else
    {
    carefulClose(&f);
    }

/* Create data table file */
char *dataTable = tableRoot;
f = hgCreateTabFile(tabDir,dataTable);

/* Parse expression values in data file. Each row is a gene */
char **row;
AllocArray(row, dataSampleCount+2);
while (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopByChar(line, '\t', row, dataSampleCount+2);
    if (wordCount-2 != dataSampleCount)
        errAbort("Expecting %d data points, got %d line %d of %s", 
		dataSampleCount, wordCount-2, lf->lineIx, lf->fileName);
    if (median)
        {
        dataMedianRowOut(f, dataSampleCount, row, tissueOffsets, tissueCount);
        }
    else
        {
        dataRowOut(f, dataSampleCount, row);
        }
    dataCount++;
    if (limit != 0 && dataCount >= limit)
        break;
    }
lineFileClose(&lf);

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    gtexDataCreateTable(conn, dataTable);
    hgLoadTabFile(conn, tabDir, dataTable, &f);
    hgRemoveTabFile(tabDir, dataTable);
    sqlDisconnect(&conn);
    }
else
    {
    carefulClose(&f);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
database = optionVal("database", database);
doLoad = !optionExists("noLoad");
doRound = optionExists("doRound");
median = optionExists("median");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
if (argc != 4)
    usage();
hgGtex(argv[1], argv[2], argv[3]);
return 0;
}
