/* bigGene - Find biggest gene in genome.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"


char *db = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigGene - Find biggest gene in genome.\n"
  "usage:\n"
  "   bigGene input\n"
  "Where input is a file in genePred format, or a database table if using the -db option.\n"
  "options:\n"
  "   -db=database\n"
  "   -intron - calculate biggest intron instead of biggest gene\n"
  "   -verbose=N - set verbosity level.  1 is normal, 0 silent, 2 or more loud.\n"
  );
}

static struct optionSpec options[] = {
   {"db", OPTION_STRING},
   {"intron", OPTION_BOOLEAN},
   {NULL, 0},
};

struct genePred *readAllGenesFromDb(char *database, char *table)
/* Read all genes from given database and table, return as a list. */
{
struct genePred *list = NULL;
struct sqlConnection *conn = sqlConnect(database);
char query[256];

/* Make something like "select * from knownGene" */
sqlSafef(query, sizeof(query), "select * from %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    verbose(2, "loading %s\n", row[1]);
    struct genePred *gp = genePredLoad(row);
    slAddHead(&list, gp);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

int biggestIntron(struct genePred *gp)
/* Figure out biggest intron in gene. */
{
int biggestSize = 0;
int i;
for (i=1; i<gp->exonCount; ++i)
    {
    int intronSize = gp->exonStarts[i] - gp->exonEnds[i-1];
    if (intronSize > biggestSize)
        biggestSize = intronSize;
    }
return biggestSize;
}

void bigGene(char *input)
/* bigGene - Find biggest gene in genome.. */
{
struct genePred *list = NULL;
if (db)
    list = readAllGenesFromDb(db, input);
else
    list = genePredLoadAll(input);
struct genePred *biggest = NULL, *gp;
int biggestSize = 0;
boolean doIntron = optionExists("intron");

for (gp = list; gp != NULL; gp = gp->next)
    {
    int size;
    if (doIntron)
        size = biggestIntron(gp);
    else
    	size = gp->txEnd - gp->txStart;
    if (size > biggestSize)
        {
	biggestSize = size;
	biggest = gp;
	}
    }
printf("biggest %s is %s, size %d\n", 
	(doIntron ? "intron" : "gene"),
	biggest->name, biggestSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
db = optionVal("db", db);
if (argc != 2)
    usage();
bigGene(argv[1]);
return 0;
}
