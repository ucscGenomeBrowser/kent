/* hgProtIdToGenePred - Add proteinID column to genePrediction. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgProtIdToGenePred - Add proteinID column to genePrediction\n"
  "usage:\n"
  "   hgProtIdToGenePred database genePred linkTable geneField protField\n"
  "This will add a proteinID field to the genePred table in database,\n"
  "filling it in with values it gets from looking up things in linkTable\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *makeProtHash(struct sqlConnection *conn, 
	char *linkTable, char *geneField, char *protField)
/* Make hash that goes from gene to prot. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(18);
printf("looking up proteins\n");
sqlSafef(query, sizeof(query), 
   "select %s,%s from %s", geneField, protField, linkTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
return hash;
}

void hgProtIdToGenePred(char *database, char *geneTable, 
	char *linkTable, char *geneField, char *protField)
/* hgProtIdToGenePred - Add proteinID column to genePrediction. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[256];
struct hash *protHash = makeProtHash(conn, linkTable, geneField, protField);
struct slName *gene, *geneList = NULL;

/* Create new column not filled with anything. */
printf("Adding column\n");
sqlSafef(query, sizeof(query), 
   "alter table %s add column (proteinID varchar(40) not null)",
   geneTable);
sqlUpdate(conn, query);

/* Get list of genes. */
printf("scanning genes\n");
sqlSafef(query, sizeof(query),
   "select name from %s", geneTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gene = slNameNew(row[0]);
    slAddHead(&geneList, gene);
    }
sqlFreeResult(&sr);
slReverse(&geneList);

/* Update each gene. */
printf("updating proteinID values\n");
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    char *prot = hashFindVal(protHash, gene->name);
    if (prot == NULL)
        prot = "n/a";
    sqlSafef(query, sizeof(query), 
    	"update %s set proteinID = '%s' where name = '%s'",
	geneTable, prot, gene->name);
    sqlUpdate(conn, query);
    }

/* Add new index. */
printf("indexing\n");
sqlSafef(query, sizeof(query),
    "create index proteinID on %s (proteinID(10))", geneTable);
sqlUpdate(conn, query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
hgProtIdToGenePred(argv[1],argv[2],argv[3],argv[4],argv[5]);
return 0;
}
