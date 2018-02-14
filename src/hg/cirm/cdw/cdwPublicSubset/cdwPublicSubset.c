/* cdwPublicSubset - Create a subset of cdw database only including publicly accessible files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwPublicSubset - Create a subset of cdw database only including publicly accessible files.\n"
  "usage:\n"
  "   cdwPublicSubset outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void tabWriteRow(FILE *f, char **row, int colCount)
/* Write out a single line to a tab-separated file */
{
int i;
for (i=0; i<colCount; ++i)
    {
    if (i != 0)
       fputc('\t', f);
    char *cell = row[i];
    if (cell != NULL)
	{
	if (strchr(cell, '\t'))
	    {
	    int len = strlen(cell);
	    int expLen = len*2+1;
	    char escaped[expLen];
	    sqlEscapeTabFileString2(escaped, cell);
	    fputs(escaped, f);
	    }
	else
	    {
	    fputs(cell, f);
	    }
	}
    }
fputc('\n', f);
}

void tabOutAll(struct sqlResult *sr, char *fileName)
/* Write out sql result to fileName */
{
FILE *f = mustOpen(fileName, "w");
int colCount = sqlCountColumns(sr);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    tabWriteRow(f, row, colCount);
    }
carefulClose(&f);
}

void outputSubset(struct sqlResult *sr, char *fileName, char *keyField, struct hash *keyHash)
/* Write out bits of result where keyField is in keyHash */
{
FILE *f = mustOpen(fileName, "w");
int colCount = sqlCountColumns(sr);
int keyIx = sqlFieldColumn(sr, keyField);
if (keyIx < 0)
    errAbort("Field %s not found for %s", keyField, fileName); 
int sqlFieldColumn(struct sqlResult *sr, char *colName);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hashLookup(keyHash, row[keyIx]) != NULL)
        tabWriteRow(f, row, colCount);
    }
carefulClose(&f);
}

void cdwPublicSubset(char *outDir)
/* cdwPublicSubset - Create a subset of cdw database only including publicly accessible files. */
{
makeDir(outDir);
struct sqlConnection *conn = cdwConnect();
struct slName *tableList = sqlListTablesLike(conn, "cdw%");
uglyf("Got %d tables\n", slCount(tableList));

char query[512];
sqlSafef(query, sizeof(query), "select id,metaTagsId from cdwFile where allAccess != 0");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct hash *fileIdHash = hashNew(16);
struct hash *metaTagsHash = hashNew(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(fileIdHash, row[0], NULL);
    hashStore(metaTagsHash, row[1]);
    }
sqlFreeResult(&sr);
uglyf("Got %d public files\n", fileIdHash->elCount);

struct slName *table;
for (table = tableList; table != NULL; table = table->next)
    {
    char *tableName = table->name;

    /* Tables we just skip here - maybe just cdwJob */
    if (sameString(tableName, "cdwJob") ||
        sameString(tableName, "cdwBiosample") ||
        sameString(tableName, "cdwExperiment") ||
	sameString(tableName, "cdwFileTags") ||
	sameString(tableName, "cdwScriptRegistry") ||
	sameString(tableName, "cdwSubscriber") )
        continue;

    char outTab[PATH_LEN];
    safef(outTab, sizeof(outTab), "%s/%s.txt", outDir, tableName);
    sqlSafef(query, sizeof(query), "select * from %s", tableName);
    struct sqlResult *sr = sqlGetResult(conn, query);
    /* Tables we copy whole */
    if (   sameString(tableName, "cdwAssembly")
        || sameString(tableName, "cdwDataset")
	|| sameString(tableName, "cdwGroup")
	|| sameString(tableName, "cdwGroupUser")
	|| sameString(tableName, "cdwHost")
	|| sameString(tableName, "cdwJointDataset")
	|| sameString(tableName, "cdwLab")
	|| sameString(tableName, "cdwQaContamTarget")
	|| sameString(tableName, "cdwSettings")
	|| sameString(tableName, "cdwStepDef")
	|| sameString(tableName, "cdwStepRun")
	|| sameString(tableName, "cdwSubmit")
	|| sameString(tableName, "cdwSubmitDir")
	|| sameString(tableName, "cdwUser"))
	{
	tabOutAll(sr, outTab);
	}

    /* Some special cases */
    else if (sameString(tableName, "cdwFile"))
        {
	outputSubset(sr, outTab, "id", fileIdHash);
	}
    else if (   sameString(tableName, "cdwQaPairCorrelation") 
	     || sameString(tableName, "cdwQaPairSampleOverlap") )
        {
	outputSubset(sr, outTab, "elderFileId", fileIdHash);
	}
    else if (sameString(tableName, "cdwQaPairedEndFastq"))
        {
	outputSubset(sr, outTab, "fileId1", fileIdHash);
	}
    else if (sameString(tableName, "cdwQaWigSpot"))
        {
	outputSubset(sr, outTab, "wigId", fileIdHash);
	}
    else if (sameString(tableName, "cdwMetaTags"))
        {
	outputSubset(sr, outTab, "id", metaTagsHash);
	}

    /* The main case - all fields where fileId matches */
    else
        {
	outputSubset(sr, outTab, "fileId", fileIdHash);
	}
    sqlFreeResult(&sr);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwPublicSubset(argv[1]);
return 0;
}
