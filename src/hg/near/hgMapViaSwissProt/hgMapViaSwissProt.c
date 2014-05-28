/* hgMapViaSwissProt - Make table that maps to external database via 
 * SwissProt. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "spDb.h"
#include "hdb.h"
#include "hgRelate.h"


char *uniProt = UNIPROT_DB_NAME;

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
  "   spIdField is the field in inTable that is a UniProt accession or ID\n"
  "   spExtDb is the UniProt external database name (like Pfam)\n"
  "   outTable is the output name/value table\n"
  "example:\n"
  "   hgMapViaSwissProt hg16 knownGene name proteinID Pfam knownToPfam\n"
  "options:\n"
  "   uniProt=sp070202 - Set uniProt database to something specific\n"
  );
}

static struct optionSpec options[] = {
   {"uniProt", OPTION_STRING},
   {NULL, 0},
};

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
sqlDyStringPrintf(dy, 
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
struct sqlConnection *spConn = sqlConnect(uniProt);
char geneQuery[256], query[256];
struct sqlResult *geneSr, *sr;
char **geneRow, **row;
char *spExtDbId;
char *tempDir = ".";
char *chp;

FILE *f = hgCreateTabFile(tempDir, outTable);
struct hash *uniqHash = newHash(18);

/* Look up id for swissprot external database. */
sqlSafef(query, sizeof(query), "select id from extDb where val = '%s'", spExtDb);
spExtDbId = sqlQuickString(spConn, query);
if (spExtDbId == NULL)
    errAbort("Couldn't find %s in UniProt.extDb", spExtDb);

/* Stream through input table. */
printf("Looking up %s.%s in UniProt database\n", inTable, spIdField);
sqlSafef(geneQuery, sizeof(geneQuery), 
	"select %s,%s from %s", nameField, spIdField, inTable);
geneSr = sqlGetResult(dbConn, geneQuery);
while ((geneRow = sqlNextRow(geneSr)) != NULL)
    {
    char *name = geneRow[0];
    char *spId = geneRow[1];
    char *acc = spFindAcc(spConn, spId);
    if (acc != NULL)
        {
	/* chop off the tail, if the protein is a variant splice isoform. */
	chp = strstr(acc, "-");
	if (chp != NULL) *chp = '\0';
	sqlSafef(query, sizeof(query), 
		"select extAcc1 from extDbRef where acc='%s' and extDb=%s",
		acc, spExtDbId);
	sr = sqlGetResult(spConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char uniqId[256];
	    safef(uniqId, sizeof(uniqId), "%s %s", name, row[0]);
	    if (!hashLookup(uniqHash, uniqId))
		{
		hashAdd(uniqHash, uniqId, NULL);
		fprintf(f, "%s\t%s\n", name, row[0]);
		}
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
uniProt = optionVal("uniProt", uniProt);
hgMapViaSwissProt(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
return 0;
}
