/* hgMapViaSwissProt - Make table that maps to external database via 
 * SwissProt. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "spDb.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgMapViaSwissProt.c,v 1.1 2003/11/15 20:04:32 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMapViaSwissProt - Make table that maps to external database via SwissProt\n"
  "usage:\n"
  "   hgMapViaSwissProt database inTable nameField spIdField spExtDb outTable\n"
  "where\n"
  "   database is a genome database like hg16\n"
  "   inTable is a table in genome database like knownGene\n"
  "   keyField is the field in inTable to put in name field of outTable\n"
  "   spIdField is the field in inTable that is a swissProt accession or ID\n"
  "   spExtDb is the swissProt external database name (like Pfam)\n"
  "   outTable is the output name/value table\n"
  "example:\n"
  "   hgMapViaSwissProt hg16 knownGene name proteinID Pfam knownToPfam\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    INDEX(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void hgMapViaSwissProt(char *database, char *inTable, 
	char *nameField, char *spIdField, 
	char *spExtDb, char *outTable)
/* hgMapViaSwissProt - Make table that maps to external database 
 * via SwissProt. */
{
struct sqlConnection *dbConn = sqlConnect(database);
struct sqlConnection *spConn = sqlConnect("swissProt");
char geneQuery[256], query[256];
struct sqlResult *geneSr, *sr;
char **geneRow, **row;
char *spExtDbId;
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, outTable);

/* Look up id for swissprot external database. */
safef(query, sizeof(query), "select id from extDb where val = '%s'", spExtDb);
spExtDbId = sqlQuickString(spConn, query);
if (spExtDbId == NULL)
    errAbort("Couldn't find %s in swissProt.extDb", spExtDb);

/* Stream through input table. */
printf("Looking up %s.%s in swissProt database\n", inTable, spIdField);
safef(geneQuery, sizeof(geneQuery), 
	"select %s,%s from %s", nameField, spIdField, inTable);
geneSr = sqlGetResult(dbConn, geneQuery);
while ((geneRow = sqlNextRow(geneSr)) != NULL)
    {
    char *name = geneRow[0];
    char *spId = geneRow[1];
    char *acc = spFindAcc(spConn, spId);
    if (acc != NULL)
        {
	safef(query, sizeof(query), 
		"select extAcc1 from extDbRef where acc='%s' and extDb=%s",
		acc, spExtDbId);
	sr = sqlGetResult(spConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    fprintf(f, "%s\t%s\n", name, row[0]);
	    }
	sqlFreeResult(&sr);
	freez(&acc);
	}
    }
printf("Loading %s.%s\n", database, outTable);
createTable(dbConn, outTable);
hgLoadTabFile(dbConn, tempDir, outTable, &f);
hgRemoveTabFile(tempDir, outTable);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
hgMapViaSwissProt(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
return 0;
}
