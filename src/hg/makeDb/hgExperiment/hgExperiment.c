/* hgExperiment - Load experimental data into two tables -- 
   one containing the data (comma-separated experiment values), and
   the other describing the experiments.  This uses expression experiment
   table types, so suitable for display with "expRatio" track type */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "hgRelate.h"
#include "expRecord.h"
#include "expData.h"
#include "bed.h"


char *tabDir = ".";
char *chopName = NULL;
char *expUrl = "n/a";
char *expRef = "n/a";
char *expCredit = "n/a";
boolean doLoad;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "\nhgExperiment - Load data from a BED of region positions,\n"
  "             an experiment file containing <name> [<description>]\n"
  "             and a data file containing <region-name> <experiment-name> <score>\n"
  "usage:\n"
  "       hgExperiment db table expFile posFile dataFile \n\n"
  "This will create an experiment table in the selected database\n"
  " and a BED15 file of experiment data, suitable for loading with hgLoadBed\n"
  "Input files should be tab-separated.\n\n"
  "options:\n"
  "    -tab=dir - Output tab-separated files to directory.\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  "    -chopName=XXX (chop off name at XXX)\n"
  "    -url=XXX\n"
  "    -ref=XXX\n"
  "    -credit=XXX\n"
        );
}

static struct optionSpec options[] = {
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"chopName", OPTION_STRING},
   {"url", OPTION_STRING},
   {"ref", OPTION_STRING},
   {"credit", OPTION_STRING},
   {NULL, 0},
};

struct hash *makeExpsTable(char *database, char *expTable, char *expFile,
                                 int *expCount)
/* Open experiment file and use it to create experiment table.
   Use optional fields if present, otherwise defaults.
   Return a hash of expId's, keyed by name */
{
struct lineFile *lf = lineFileOpen(expFile, TRUE);
FILE *f = hgCreateTabFile(tabDir, expTable);
int expId = 0;
char *words[6];
int wordCt;
struct hash *expHash = newHash(0);

while ((wordCt = lineFileChopNext(lf, words, ArraySize(words))))
    {
    char *name = words[0];
    hashAddInt(expHash, name, expId);
    fprintf(f, "%d\t%s\t", expId++, name);
    fprintf(f, "%s\t", wordCt > 1 ? words[1] : name);
    fprintf(f, "%s\t", wordCt > 2 ? words[2] : expUrl);
    fprintf(f, "%s\t", wordCt > 3 ? words[3] : expRef);
    fprintf(f, "%s\t", wordCt > 4 ? words[4] : expCredit);
    fprintf(f, "0\n");          /* extras */
    }
if (expId <= 0)
    errAbort("No experiments in %s", lf->fileName);
verbose(2, "%d experiments\n", expId);

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    expRecordCreateTable(conn, expTable);
    hgLoadTabFile(conn, tabDir, expTable, &f);
    sqlDisconnect(&conn);
    }
lineFileClose(&lf);
if (expCount)
    *expCount = expId;
return expHash;
}

void calculateAverage(struct bed *bed)
/* TODO: librarify (share with affyPslAndAtlasToBed) */
/** divide the sum of the intensities stored in the score by
    number of experiments */
{
    int i;
    float sum = 0;
    if (!bed->expCount)
        errAbort("No experiments in bed %s", bed->name);
    for (i = 0; i < bed->expCount; i++)
        sum += bed->expScores[i];
    bed->score = sum / bed->expCount;
    /* NOTE: *10 this value (affyToBed does this) ? 
     *  Also, might want to use missing data value */
}

void hgExperiment(char *database, char *table, 
                        char *expFile, char *posFile, char *dataFile)
/* Main function */
{
struct lineFile *lf;
int *data = NULL;
int *scores;
FILE *f = NULL;
char expTable[32];
char *words[3];
int wordCt;
struct bed *bedList, *bed;
int expCount;
struct hash *expHash, *dataHash;
struct hashEl *hel;

/* Open experiment file and use it to create experiment table.
   Use optional fields if present, otherwise defaults */
safef(expTable, ArraySize(expTable), "%sExps", table);
expHash = makeExpsTable(database, expTable, expFile, &expCount);

/* Read in positions file */
bedList = bedLoadAll(posFile);
slSort(&bedList, bedCmp);

/* Read data file into a hash of arrays of data values, keyed by name */
dataHash = newHash(0);
lf = lineFileOpen(dataFile, TRUE);
while ((wordCt = lineFileChopNext(lf, words, ArraySize(words))))
    {
    /* format: <region-name> <experiment-name> <data-value> */
    char *name, *exp;
    int expId;
    int value;
    if (wordCt != 3)
        errAbort("Expecting 3 words in data file, got %d line %d of %s", 
		wordCt, lf->lineIx, lf->fileName);
    name = words[0];
    hel = hashLookup(dataHash, name);
    if (!hel)
        {
        AllocArray(data, expCount);
        hel = hashAdd(dataHash, name, data);
        }
    data = (int *)hel->val;
    exp = words[1];
    expId = hashIntVal(expHash, exp);
    if (expId < 0 || expId > expCount-1)
        errAbort("Invalid experiment ID %d for %s, line %d of %s",
                 expId, exp, lf->lineIx, lf->fileName);
    //value = atoi(words[2]);
    value = round(atof(words[2]));
    if (data[expId] != 0)
        errAbort("Extra experiment data value %d for %s %s, line %d of %s",
                         value, name, exp, lf->lineIx, lf->fileName);
    data[expId] = value;
    }
lineFileClose(&lf);

/* Fill in BED15 fields - add experiment values, and setup block (only 1)*/
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int i;
    bed->thickStart = bed->chromStart;
    bed->thickEnd = bed->chromEnd;
    bed->blockCount = 1;
    AllocArray(bed->blockSizes, 1);
    bed->blockSizes[0] = bed->chromEnd - bed->chromStart;
    AllocArray(bed->chromStarts, 1);
    bed->chromStarts[0] = 0;
    bed->expCount = expCount;
    AllocArray(bed->expIds, expCount);
    for (i = 0; i < expCount; i++)
        bed->expIds[i] = i;
    AllocArray(bed->expScores, expCount);
    scores = hashMustFindVal(dataHash, bed->name);
    for (i = 0; i < expCount; i++)
        bed->expScores[i] = scores[i];
    /* set score for bed to the average of the scores in all experiments */
    calculateAverage(bed);
    }

/* from affyPslAndAtlsoToBed ?
   convertIntensitiesToRatios(bedList);
   */

/* Write BED data file */
f = hgCreateTabFile(tabDir, table);
for (bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 15, f);

/* Cleanup */
carefulClose(&f);
freeHash(&expHash);
freeHash(&dataHash);
bedFreeList(&bedList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
doLoad = !optionExists("noLoad");
chopName = optionVal("chopName", chopName);
expUrl = optionVal("url", expUrl);
expRef = optionVal("ref", expRef);
expCredit = optionVal("credit", expCredit);
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
if (argc != 6)
    usage();
hgExperiment(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
