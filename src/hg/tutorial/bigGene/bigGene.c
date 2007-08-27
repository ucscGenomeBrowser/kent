/* bigGene - Find biggest gene in genome.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"

static char const rcsid[] = "$Id: bigGene.c,v 1.1 2007/08/27 21:34:00 kent Exp $";

char *db = "hg18";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigGene - Find biggest gene in genome.\n"
  "usage:\n"
  "   bigGene inputTable\n"
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

struct genePred *readAllGenes(char *database, char *table)
/* Read all genes from given database and table, return as a list. */
{
struct genePred *list = NULL;
struct sqlConnection *conn = sqlConnect(database);
char query[256];

/* Make something like "select * from knownGene" */
safef(query, sizeof(query), "select * from %s", table);
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
struct genePred *biggest = NULL, *gp, *list = readAllGenes(db, input);
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
	(doIntron ? "gene" : "intron"),
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
