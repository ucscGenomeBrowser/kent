/* hgCeOrfToGene - Make orfToGene table for C.elegans from GENE_DUMPS/gene_names.txt. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgCeOrfToGene.c,v 1.1 2003/09/24 06:47:23 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCeOrfToGene - Make orfToGene table for C.elegans from GENE_DUMPS/gene_names.txt\n"
  "usage:\n"
  "   hgCeOrfToGene database gene_names.txt orfToGene\n"
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
dyStringPrintf(dy, 
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


void hgCeOrfToGene(char *database, char *geneNames, char *table)
/* hgCeOrfToGene - Make orfToGene table for C.elegans from 
 * GENE_DUMPS/gene_names.txt. */
{
struct lineFile *lf = lineFileOpen(geneNames, TRUE);
struct sqlConnection *conn;
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, table);
char *row[4];

while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    char *gene = row[0];
    char *orfs = row[3];
    char *type = row[2];
    char *orf[128];
    int i, orfCount;

    if (sameString(type, "Gene"))
	{
	orfCount = chopString(orfs, ",", orf, ArraySize(orf));
	if (orfCount >= ArraySize(orf))
	     errAbort("Too many ORFs line %d of %s", lf->lineIx, lf->fileName);
	for (i=0; i<orfCount; ++i)
	    {
	    fprintf(f, "%s\t%s\n", orf[i], gene);
	    }
	}
    }
conn = sqlConnect(database);
createTable(conn, table, unique);
hgLoadTabFile(conn, tempDir, table, &f);
hgRemoveTabFile(tempDir, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgCeOrfToGene(argv[1], argv[2], argv[3]);
return 0;
}
