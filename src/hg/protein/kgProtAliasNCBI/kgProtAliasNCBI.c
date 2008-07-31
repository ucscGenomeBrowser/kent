/* kgProtAliasNCBI - generate alias list table for NCBI protein AC */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgProtAliasNCBI - create gene alias (mRNA part) .tab files "
  "usage:\n"
  "   kgProtAliasNCBI <DB> <RO_DB>\n"
  "            <DB> is knownGene DB under construction\n"
  "            <RO_DB> is read only target genome database\n"
  "example: kgProtAliasNCBI kgDB hg16\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;

char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
 
char *chp;
char *kgID;
FILE *o2;
char cond_str[256];
char *database;
char *ro_db;

char *proteinID;
char *proteinAC;

if (argc != 3) usage();
database  = cloneString(argv[1]);
ro_db  = cloneString(argv[2]);

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

    // get RefSeq protein AC numbers (NP_xxxxx) if they exist
    sprintf(cond_str, "kgID='%s'", kgID);
    proteinAC = sqlGetField(database, "kgXref", "protAcc", cond_str);
    if (proteinAC != NULL)
	{
	if (strlen(proteinAC) > 0)
	    {
	    fprintf(o2, "%s\t%s\t%s\n", kgID, proteinID, proteinAC);
	    }
	}

    // get Genbank protein accession numbers
    if (strstr(kgID, "NM_") != NULL)
	{
	sprintf(query,"select protAcc from %s.refLink where mrnaAcc = '%s';", ro_db, kgID);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	while (row != NULL)
    	    {
    	    proteinAC = row[0];
	    fprintf(o2, "%s\t%s\t%s\n", kgID, proteinID, proteinAC);
	    row = sqlNextRow(sr);
	    }
    	sqlFreeResult(&sr);
	}
    else
	{
	sprintf(query,"select proteinAC from %sTemp.locus2Acc0 where gbAC like '%s%c';", database, kgID, '%');
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	while (row != NULL)
    	    {
    	    proteinAC = row[0];

	    chp = strstr(proteinAC, ".");
	    if (chp != NULL)
		{
		*chp = '\0';
		}
	    if (proteinAC[0] != '-')
		{
		fprintf(o2, "%s\t%s\t%s\n", kgID, proteinID, proteinAC);
		}
	    row = sqlNextRow(sr);
	    }
    	sqlFreeResult(&sr);
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
fclose(o2);
hFreeConn(&conn);
hFreeConn(&conn2);

system("cat jj.dat|sort|uniq  >kgProtAliasNCBI.tab");
system("rm jj.dat");
    
return(0);
}

