/* hgMapMicroarray - Make mapped version of microarray data, merging psl(s) in.. */
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

static char const rcsid[] = "$Id: hgMapMicroarray.c,v 1.1 2004/03/31 17:51:08 kent Exp $";

char *pslPrefix = NULL;
char *pslSuffix = NULL;
char *tempDir = ".";

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
  );
}

static struct optionSpec options[] = {
   {"pslSuffix", OPTION_STRING},
   {"pslPrefix", OPTION_STRING},
   {NULL, 0},
};

struct hash *expDataHash(char *tableName)
/* Load up expData table into a hash. */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
struct hash *hash = hashNew(16);
struct sqlResult *sr;
char **row, query[256];
int count = 0;

safef(query, sizeof(query), "select * from %s", tableName);
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

void writeMappedExp(FILE *f, struct expData *expData, struct psl *psl)
/* Write out mapped expression data to file. */
{
struct bed *bed = bedFromPsl(psl);
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
bedFree(&bed);
}

void hgMapMicroarray(char *bedName, char *fixedTable, 
	int pslCount, char *psls[])
/* hgMapMicroarray - Make mapped version of microarray data, merging psl in. */
{
int i;
struct hash *expHash = expDataHash(fixedTable);
struct hash *uniqHash = hashNew(16);
FILE *f = mustOpen(bedName, "w");
struct expData *expData;
int mapCount = 0, dupeMapCount = 0, missCount = 0, unmappedCount = 0;
int prefixSize = 0;
if (pslPrefix != NULL)
    prefixSize += strlen(pslPrefix);
for (i=0; i<pslCount; ++i)
    {
    char *pslName = psls[i];
    struct lineFile *lf = pslFileOpen(pslName);
    struct psl *psl;
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
	    if (hashLookup(uniqHash, name))
		++dupeMapCount;
	    else
		{
		++mapCount;
		hashAdd(uniqHash, name, NULL);
		}
	    writeMappedExp(f, expData, psl);
	    }
	else
	    {
	    ++missCount;
	    verbose(2, "miss %s\n", name);
	    }
	pslFree(&psl);
	}
    lineFileClose(&lf);
    }
/* Look at ones that we don't have mapping data on. */
    {
    struct hashEl *expList, *expEl;
    expList = hashElListHash(expHash);
    for (expEl = expList; expEl != NULL; expEl = expEl->next)
        {
	expData = expEl->val;
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
if (argc < 4)
    usage();
hgMapMicroarray(argv[1], argv[2], argc-3, argv+3);
return 0;
}
