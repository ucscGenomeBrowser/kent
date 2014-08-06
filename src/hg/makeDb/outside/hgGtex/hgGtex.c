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
boolean doLoad;
boolean doRound;
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

void sampleRowOut(FILE *f, struct gtexSample *sample, int id)
{
fprintf(f, "%d\t", id);
fprintf(f, "%s\t", sample->tissue);
fprintf(f, "%s\t", sample->donor);
fprintf(f, "%s\n", sample->name);
}

void dataRowOut(FILE *f, int count, char **row)
/* Output data */
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

void hgGtex(char *tableRoot, char *dataFile, char *sampleFile)
/* Main function */
{
struct lineFile *lf;
char *line;
int i, wordCount;
char *words[100];
FILE *f = NULL;
struct hash *hash = newHash(17);
int dataCount = 0;
int sampleCount = 0;

/* Parse GTEX sample file */
lf = lineFileOpen(sampleFile, TRUE);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", lf->fileName);
if (!startsWith("SAMPID", line))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);
int sampleCols = chopTabs(line, words);
if (sampleCols < 7 || differentString(words[6], "SMTSD"))
    errAbort("unrecognized format - expecting sample file header in %s first line", lf->fileName);

while (lineFileNext(lf, &line, NULL))
    {
    /* Convert line to sample record */
    char *sampleId, *tissue, *donor;
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, sampleCols, wordCount);
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
    sampleCount++;
    }
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

/* Create sample table with samples ordered as in data file */
char sampleTable[64];
safef(sampleTable, sizeof(sampleTable), "%sSamples", tableRoot);
f = hgCreateTabFile(tabDir, sampleTable);
struct gtexSample *sample;
for (i=0; i<dataSampleCount; i++)
    {
    sample = hashMustFindVal(hash, samples[i]);
    sampleRowOut(f, sample, i);
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

/* Parse expression values in data file */
char **row;
AllocArray(row, dataSampleCount+2);
while (lineFileNext(lf, &line, NULL))
    {
    //geneId = nextWord(&line);
    //nextWord(&line);
    //wordCount = chopTabs(line, row);
    wordCount = chopByChar(line, '\t', row, dataSampleCount+2);
    if (wordCount-2 != dataSampleCount)
        errAbort("Expecting %d data points, got %d line %d of %s", 
		dataSampleCount, wordCount-2, lf->lineIx, lf->fileName);
    dataRowOut(f, dataSampleCount, row);
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
