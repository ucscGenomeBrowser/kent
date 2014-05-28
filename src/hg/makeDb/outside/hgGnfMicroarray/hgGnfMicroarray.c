/* hgGnfMicroarray - Load data from (2003-style) GNF Affy Microarrays. */

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


char *chip = "HG-U95Av2";
char *database = "hgFixed";
char *tabDir = ".";
char *chopName = NULL;
boolean doLoad;
boolean noRound;
int limit = 0;
char *expUrl = "http://www.affymetrix.com/analysis/index.affx";
char *expRef = "http://expression.gnf.org";
char *expCredit = "http://www.gnf.org";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGnfMicroarray - Load data from (2003-style) GNF Affy Microarrays\n"
  "usage:\n"
  "   hgGnfMicroarray expTable spotTable atlasFile\n"
  "This will create two tables in hgFixed, an experiment table\n"
  "that contains a row for each mRNA sample run over the chip,\n"
  "and a spot table that contains the observed intensity of the\n"
  "spot. Note these will be absolute values, not ratios.  Use the\n"
  "hgRatioMicroarray program to create a ratio-based spot table.\n"
  "options:\n"
  "    -database=XXX (default %s)\n"
  "    -chip=XXXX (default %s)\n"
  "    -tab=dir - Output tab-separated files to directory.\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  "    -noRound - If true don't round data values\n"
  "    -limit=N - Only do limit rows of table, for testing\n"
  "    -chopName=XXX (chop off name at XXX)\n"
  "    -url=XXX, default %s\n"
  "    -ref=XXX, default %s\n"
  "    -credit=XXX, default %s\n"
  , database, chip, expUrl, expRef, expCredit);
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"chip", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"noRound", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {"chopName", OPTION_STRING},
   {"url", OPTION_STRING},
   {"ref", OPTION_STRING},
   {"credit", OPTION_STRING},
   {NULL, 0},
};

struct expCounter
/* Keep track when have multiple experiments on same tissue. */
    {
    char *name;	/* Not allocated here. */
    int count;	/* Count of times seen. */
    };

int lineToExp(char *line, FILE *f)
/* Convert line to an expression record file. 
 * Return number of expression records. */
{
struct hash *hash = newHash(10);	/* Integer valued hash */
char *word;
int wordCount = 0;
struct expCounter *ec;
char *spaced;
char name[128];

while ((word = nextTabWord(&line)) != NULL)
    {
    if ((ec = hashFindVal(hash, word)) == NULL)
        {
	AllocVar(ec);
	hashAddSaveName(hash, word, ec, &ec->name);
	}
    spaced = cloneString(word);
    subChar(spaced, '_', ' ');
    ec->count += 1;
    if (ec->count > 1)
        safef(name, sizeof(name), "%s %d", spaced, ec->count);
    else
        safef(name, sizeof(name), "%s", spaced);
    fprintf(f, "%d\t", wordCount);
    fprintf(f, "%s\t", name);
    fprintf(f, "%s\t", name);
    fprintf(f, "%s\t", expUrl);
    fprintf(f, "%s\t", expRef);
    fprintf(f, "%s\t", expCredit);
    fprintf(f, "3\t");
    fprintf(f, "%s,%s,%s,\n", chip, "n/a", spaced);
    ++wordCount;
    }
return wordCount;
}

int lineToExpTable(char *line, char *table)
/* Create expression format table from line. */
{
FILE *f = hgCreateTabFile(tabDir, table);
int count = lineToExp(line, f);
if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    expRecordCreateTable(conn, table);
    hgLoadTabFile(conn, tabDir, table, &f);
    hgRemoveTabFile(tabDir, table);
    sqlDisconnect(&conn);
    }
return count;
}

void shortDataOut(FILE *f, char *name, int count, float *scores)
/* Do short type output. */
{
int i;
fprintf(f, "%s\t%d\t", name, count);
/* if no rounding, then print as float, otherwise round */
for (i=0; i<count; ++i)
    {
    if (noRound)
        fprintf(f, "%0.3f,", scores[i]);
    else 
        fprintf(f, "%d,", round(scores[i]));
    } 
fprintf(f, "\n");
}

void hgGnfMicroarray(char *expTable, char *dataTable, char *atlasFile)
/** Main function that does all the work for new-style*/
{
struct lineFile *lf = lineFileOpen(atlasFile, TRUE);
char *line;
int i, wordCount, expCount;
char **row;
float *data;
char *affyId;
struct hash *hash = newHash(17);
FILE *f = NULL;
int dataCount = 0;

/* Open Atlas file and use first line to create experiment table. */
if (!lineFileNextReal(lf, &line))
    errAbort("%s is empty", lf->fileName);
if (startsWith("Affy", line))
    line += 4;
if (startsWith("Gene Name", line))
    line += 9;
if (line[0] != '\t')
    errAbort("%s doesn't seem to be a new format atlas file", lf->fileName);
expCount = lineToExpTable(line+1, expTable);
if (expCount <= 0)
    errAbort("No experiments in %s it seems", lf->fileName);
warn("%d experiments\n", expCount);

f = hgCreateTabFile(tabDir, dataTable);

AllocArray(row, expCount);
AllocArray(data, expCount);
while (lineFileNextReal(lf, &line))
    {
    affyId = nextWord(&line);
    wordCount = chopByWhite(line, row, expCount);
    if (wordCount != expCount)
        errAbort("Expecting %d data points, got %d line %d of %s", 
		expCount, wordCount, lf->lineIx, lf->fileName);
    if (chopName != NULL)
        {
	char *e = stringIn(chopName, affyId);
	if (e != NULL)
	    *e = 0;
	}
    if (hashLookup(hash, affyId))
	{
        warn("Duplicate %s, skipping all but first.", affyId);
	continue;
	}
    for (i=0; i<expCount; ++i)
        {
        data[i] = sqlFloat(row[i]);
        }
    shortDataOut(f, affyId, expCount, data);
    ++dataCount;
    if (limit != 0 && dataCount >= limit)
        break;
    }
lineFileClose(&lf);

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    expDataCreateTable(conn, dataTable);
    hgLoadTabFile(conn, tabDir, dataTable, &f);
    hgRemoveTabFile(tabDir, dataTable);
    sqlDisconnect(&conn);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
database = optionVal("database", database);
chip = optionVal("chip", chip);
doLoad = !optionExists("noLoad");
noRound = optionExists("noRound");
chopName = optionVal("chopName", chopName);
expUrl = optionVal("url", expUrl);
expRef = optionVal("ref", expRef);
expCredit = optionVal("credit", expCredit);
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
if (argc != 4)
    usage();
hgGnfMicroarray(argv[1], argv[2], argv[3]);
return 0;
}
