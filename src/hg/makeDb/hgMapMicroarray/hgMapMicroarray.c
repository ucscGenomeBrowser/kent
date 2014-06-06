/* hgMapMicroarray - Make mapped version of microarray data, merging psl(s) in.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "hdb.h"
#include "expData.h"
#include "hgRelate.h"
#include "bed.h"


char *pslPrefix = NULL;
char *pslSuffix = NULL;
char *tempDir = ".";
boolean bedIn = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMapMicroarray - Make mapped version of microarray data, merging psl in.\n"
  "usage:\n"
  "   hgMapMicroarray mapped.bed hgFixed.table pslTable(s)\n"
  "where:\n"
  "   mapped.bed is where to put the mapped data in\n"
  "   hgFixed.table is the unmapped microarray data (usually in hgFixed)\n"
  "   pslTable(s) contain the mapping information\n"
  "options:\n"
  "   -pslPrefix=XXX - Skip over given prefix in psl file qName field.\n"
  "   -pslSuffix=YYY - Skip over given suffix in psl file qName.\n"
  "   -tempDir=ZZZ - Place to put temp files.\n"
  "   -bedIn - pslTable(s) are actually bed 12 not psl.\n"
  );
}

static struct optionSpec options[] = {
   {"pslSuffix", OPTION_STRING},
   {"pslPrefix", OPTION_STRING},
   {"tempDir", OPTION_STRING},
   {"bedIn", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *expDataHash(char *tableName)
/* Load up expData table into a hash. */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
struct hash *hash = hashNew(20);
struct sqlResult *sr;
char **row, query[256];
int count = 0;

sqlSafef(query, sizeof(query), "select * from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct expData *expData = expDataLoad(row);
    hashAdd(hash, expData->name, expData);
    ++count;
    }
verbose(1, "Loaded %d rows of expression data from %s\n", count, tableName);
sqlDisconnect(&conn);
return hash;
}

void writeMappedExp(FILE *f, struct expData *expData, struct bed *bed)
/* Write out mapped expression data to file. */
{
int i;
bed->expCount = expData->expCount;
AllocArray(bed->expIds, bed->expCount);
AllocArray(bed->expScores, bed->expCount);
for (i=0; i<bed->expCount; ++i)
    {
    bed->expIds[i] = i;
    bed->expScores[i] = expData->expScores[i];
    }
bedTabOutN(bed, 15, f);
}

void mapPsl(char *pslName, FILE *f,
	    struct hash *expHash, struct hash *uniqHash,
	    int *retDupeMapCount, int *retMapCount, int *retMissCount)
/* Map PSL to expression data. */
{
struct lineFile *lf = pslFileOpen(pslName);
struct psl *psl;
struct expData *expData;
int prefixSize = 0;
if (pslPrefix != NULL)
    prefixSize += strlen(pslPrefix);
while ((psl = pslNext(lf)) != NULL)
    {
    /* Chop off prefix and suffix. */
    char *name = psl->qName;
    if (pslPrefix != NULL && startsWith(pslPrefix, name))
	name += prefixSize;
    if (pslSuffix != NULL)
	{
	char *e = stringIn(pslSuffix, name);
	if (e != NULL)
	    *e = 0;
	}
    
    /* Find in expression data hash and process. */
    if ((expData = hashFindVal(expHash, name)) != NULL)
	{
	struct bed *bed = bedFromPsl(psl);
	if (hashLookup(uniqHash, name))
	    ++(*retDupeMapCount);
	else
	    {
	    ++(*retMapCount);
	    hashAdd(uniqHash, name, NULL);
	    }
	writeMappedExp(f, expData, bed);
	bedFree(&bed);
	}
    else
	{
	++(*retMissCount);
	verbose(2, "miss %s\n", name);
	}
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void mapBed(char *bedName, FILE *f,
	    struct hash *expHash, struct hash *uniqHash,
	    int *retDupeMapCount, int *retMapCount, int *retMissCount)
/* Map BED 12 to expression data. */
{
struct lineFile *lf = lineFileOpen(bedName, TRUE);
char *words[13];
while (lineFileNextRow(lf, words, 12))
    {
    struct bed *bed = bedLoad12(words);
    struct expData *expData;
    /* Find in expression data hash and process. */
    if ((expData = hashFindVal(expHash, bed->name)) != NULL)
	{
	if (hashLookup(uniqHash, bed->name))
	    ++(*retDupeMapCount);
	else
	    {
	    ++(*retMapCount);
	    hashAdd(uniqHash, bed->name, NULL);
	    }
	writeMappedExp(f, expData, bed);
	}
    else
	{
	++(*retMissCount);
	verbose(2, "miss %s\n", bed->name);
	}
    bedFree(&bed);
    }
}

void hgMapMicroarray(char *bedName, char *fixedTable, 
	int pslCount, char *psls[])
/* hgMapMicroarray - Make mapped version of microarray data, merging psl in. */
{
int i;
struct hash *expHash = expDataHash(fixedTable);
struct hash *uniqHash = hashNew(20);
FILE *f = mustOpen(bedName, "w");
int mapCount = 0, dupeMapCount = 0, missCount = 0, unmappedCount = 0;
for (i=0; i<pslCount; ++i)
    {
    char *pslName = psls[i];
    if (bedIn)
	mapBed(pslName, f, expHash, uniqHash, &dupeMapCount, &mapCount, &missCount);
    else
	mapPsl(pslName, f, expHash, uniqHash, &dupeMapCount, &mapCount, &missCount);
    }
/* Look at ones that we don't have mapping data on. */
    {
    struct hashEl *expList, *expEl;
    expList = hashElListHash(expHash);
    for (expEl = expList; expEl != NULL; expEl = expEl->next)
        {
	struct expData *expData = expEl->val;
	if (!hashLookup(uniqHash, expData->name))
	    {
	    ++unmappedCount;
	    verbose(2, "unmapped %s\n", expData->name);
	    }
	}
    slFreeList(&expList);
    }
verbose(1, "Mapped %d,  multiply-mapped %d, missed %d, unmapped %d\n",
	mapCount, dupeMapCount, missCount, unmappedCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
pslPrefix = optionVal("pslPrefix", pslPrefix);
pslSuffix = optionVal("pslSuffix", pslSuffix);
tempDir = optionVal("tempDir", tempDir);
bedIn = optionExists("bedIn");
if (argc < 4)
    usage();
hgMapMicroarray(argv[1], argv[2], argc-3, argv+3);
return 0;
}
