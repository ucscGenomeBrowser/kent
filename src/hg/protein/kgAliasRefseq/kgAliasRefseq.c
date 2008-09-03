/* kgAliasRefseq - generate Known Genes alias list table for RefSeq accession numbers */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgAliasRefseq - create gene alias .tab file "
  "usage:\n"
  "   kgAliasRefseq xxxx\n"
  "            xxxx is genome database name\n"
  "example: kgAliasRefseq hg15\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;

char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
    
char *kgID;
FILE *o2;
char *database;
char *refseq;

char *proteinID;

if (argc != 2) usage();
database  = cloneString(argv[1]);

conn = hAllocConn(database);
conn2= hAllocConn(database);
o2 = fopen("jj.dat", "w");

sprintf(query2,"select name, proteinID from %s.knownGene;", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID = row2[0];
    proteinID = row2[1];
    sprintf(query,"select refseq from %s.kgXref where kgID = '%s';", database, kgID);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
   	{
	refseq = row[0];
	if (strlen(refseq) >0)
	     {
	     fprintf(o2, "%s\t%s\n", kgID, refseq);
	     }
	row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
fclose(o2);
hFreeConn(&conn);
hFreeConn(&conn2);

system("cat jj.dat|sort|uniq  >kgAliasRefseq.tab");
system("rm jj.dat");
    
return(0);
}

