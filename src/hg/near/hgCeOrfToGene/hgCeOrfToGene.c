/* hgCeOrfToGene - Make orfToGene table for C.elegans from GENE_DUMPS/gene_names.txt. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCeOrfToGene - Make orfToGene table for C.elegans from GENE_DUMPS/gene_names.txt\n"
  "usage:\n"
  "   hgCeOrfToGene database gene_names.txt sangerGene orfToGene\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean unique = FALSE;

static struct optionSpec options[] = {
   {NULL, 0},
};

void createTable(struct sqlConnection *conn, char *tableName, boolean unique)
/* Create our name/value table, dropping if it already exists. */
{
char *indexType =  (unique ? "UNIQUE" : "INDEX");
struct dyString *dy = dyStringNew(512);
sqlDyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    %s(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName, indexType);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}


void hgCeOrfToGene(char *database, char *geneNames, 
	char *geneTable, char *table)
/* hgCeOrfToGene - Make orfToGene table for C.elegans from 
 * GENE_DUMPS/gene_names.txt. */
{
struct lineFile *lf = lineFileOpen(geneNames, TRUE);
struct sqlConnection *conn;
struct sqlResult *sr;
char query[256];
char **row;
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, table);
char *words[4];
struct hash *orfHash = newHash(17);

/* Make hash to look up gene names. */
while (lineFileNextRowTab(lf, words, ArraySize(words)))
    {
    char *gene = words[0];
    char *orfs = words[3];
    char *type = words[2];
    char *orf[128];
    int i, orfCount;

    if (sameString(type, "Gene"))
	{
	orfCount = chopString(orfs, ",", orf, ArraySize(orf));
	if (orfCount >= ArraySize(orf))
	     errAbort("Too many ORFs line %d of %s", lf->lineIx, lf->fileName);
	for (i=0; i<orfCount; ++i)
	    hashAdd(orfHash, orf[i], cloneString(gene));
	}
    }
lineFileClose(&lf);

/* For each orf in gene table write out gene name if possible,
 * otherwise orf name. */
conn = sqlConnect(database);
sqlSafef(query, sizeof(query), "select name from %s", geneTable);
sr = sqlGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *orf = row[0];
    char *gene = hashFindVal(orfHash, orf);
    if (gene == NULL)
        gene = orf;
    fprintf(f, "%s\t%s\n", orf, gene);
    }
sqlFreeResult(&sr);

createTable(conn, table, unique);
hgLoadTabFile(conn, tempDir, table, &f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hgCeOrfToGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
