/* hgGeneBands: print genes with band info */
#include "common.h"
#include "cytoBand.h"
#include "hCommon.h"
#include "hdb.h"
#include "jksql.h"

static char const rcsid[] = "$Id: hgGeneBands.c,v 1.4.142.1 2008/07/31 02:24:04 markd Exp $";

char *database = NULL;
char *chromName = NULL;

char geneTable[] = "refGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
    "hgGeneBands - print genes with band info \n"
    "usage:\n"
    "    hgGeneBands database chrom \n");
}

char *getAltName(char *name)
/* query refLink */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
char *altName = needMem(32);

safef(query, sizeof(query), "select name from refLink where mrnaAcc = '%s' ", name);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) 
    strcpy(altName, "n/a");
else
    strcpy(altName, row[0]);

sqlFreeResult(&sr);
hFreeConn(&conn);
return altName;
}

struct genePred *readGenes(char *chrom)
/* Slurp in the genes for one chrom */
{
struct sqlConnection *conn = hAllocConn(database);
struct genePred *list=NULL, *el;
char query[512];
struct sqlResult *sr;
char **row;
int count = 0;

safef(query, sizeof(query), "select * from %s where chrom='%s' ", geneTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genePredLoad(row);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
hFreeConn(&conn);
verbose(1, "Count of genes found = %d\n\n", count);
return list;
}


void hgGeneBands()
/* hgGeneBands - print genes with band info */
{
struct genePred *genes = NULL, *gene = NULL;
int start = 0, end = 0;
int geneCount = 0;
char query1[256], query2[256], name1[64], name2[64];
struct sqlResult *sr = NULL;
char **row = NULL;
struct sqlConnection *conn = NULL;
char *altName = NULL;

genes = readGenes(chromName);

conn = hAllocConn();

for (gene = genes; gene != NULL; gene = gene->next)
    {
    verbose(4, "gene %d = %s\n----------\n", geneCount, gene->name);
    geneCount++;

    start = gene->txStart;
    end = gene->txEnd;

    safef(query1, sizeof(query1), 
        "select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd >= %d", chromName, start, start);
    sr = sqlGetResult(conn, query1);
    // check for zero results
    // should actually just be one row
    while ((row = sqlNextRow(sr)) != NULL)
        {
        sprintf(name1, "%s", row[0]);
	}

    safef(query2, sizeof(query2), 
        "select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd >= %d", chromName, end, end);
    sr = sqlGetResult(conn, query2);
    // check for zero results
    // should actually just be one row
    while ((row = sqlNextRow(sr)) != NULL)
        {
        sprintf(name2, "%s", row[0]);
	}

    altName = getAltName(gene->name);
    if (sameString(name1, name2))
        printf("%s %s %d %d %s%s\n", gene->name, altName, start, end, skipChr(chromName), name1);
    else
        printf("%s %s %d %d %s%s %s%s\n", gene->name, altName, start, end, skipChr(chromName), name1, skipChr(chromName), name2);
    }

genePredFreeList(&genes);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Check args and call hgGeneBands. */
{
if (argc != 3)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
if(!hTableExistsDb(database, "cytoBand"))
    errAbort("no cytoBand table in %s\n", database);
chromName = argv[2];
if(hgOfficialChromName(chromName) == NULL)
    errAbort("no such chromosome %s in %s\n", chromName, database);
hgGeneBands();
return 0;
}
