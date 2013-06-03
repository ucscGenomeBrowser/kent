/* weedKnownBlastTab - Remove bad elements from knownBlastTab. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "blastTab.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "weedKnownBlastTab - Remove bad elements from knownBlastTab\n"
  "usage:\n"
  "   weedKnownBlastTab database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void weedKnownBlastTab(char *database)
/* weedKnownBlastTab - Remove bad elements from knownBlastTab. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[512];
struct hash *goodHash = newHash(16);
FILE *f = mustOpen("knownBlastTab.tab", "w");
int i, knownCount = 0, allCount = 0, goodCount = 0;

/* Build up list of good ones. */
sqlSafef(query, sizeof(query), "select name from knownGene");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!hashLookup(goodHash, row[0]))
	{
	hashAdd(goodHash, row[0], NULL);
	++knownCount;
	}
    }
sqlFreeResult(&sr);
printf("%d known genes\n", knownCount);

sqlSafef(query, sizeof(query), "select * from knownBlastTab");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++allCount;
    if (hashLookup(goodHash,row[0]) && hashLookup(goodHash,row[1]))
        {
	++goodCount;
	fprintf(f, "%s", row[0]);
	for (i=1; i<BLASTTAB_NUM_COLS; ++i)
	    fprintf(f, "\t%s", row[i]);
	fprintf(f, "\n");
	}
    }
printf("%d of %d written\n", goodCount, allCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
weedKnownBlastTab(argv[1]);
return 0;
}
