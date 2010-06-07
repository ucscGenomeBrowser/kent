/* spToProteinsVar- Create tab delimited data file, spXref3Var.tab, for variant splice proteins in proteinsYYMMDD database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spToProteinsVar- Create tab delimited data file, spXrefVar.tab,\n"
  "from spYYMMDD database for proteinsYYMMDD database.\n"
  "usage:\n"
  "   spToProteinsVar YYMMDD\n"
  "      YYMMDD is the release date of SWISS-PROT data\n"
  "Example: spToProteinsVar 050415\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
char cond_str[255];
char proteinDatabaseName[255];
char proteinsDB[255];
char *bioDatabase;
char *desc;
char *hugoSymbol, *hugoDesc;
FILE *o2;
char *proteinDataDate;
int  bioentryId;
char *displayId;
char *acc;
char *parAcc;
char *division;

if (argc != 2) usage();

proteinDataDate = argv[1];
sprintf(proteinDatabaseName, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);

o2 = mustOpen("spXref3Var.tab", "w");

conn  = hAllocConn(hDefaultDb());
conn2 = hAllocConn(hDefaultDb());

bioentryId = 9000000;	/* to differentiate with regular proteins */

sprintf(query2,"select * from %s.varProtTemp;", proteinDatabaseName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    bioentryId++;
    acc		= row2[0];   
    bioDatabase = row2[1];
    parAcc      = row2[2];
    desc        = row2[3];
    
    /* set display ID the same as accession for variant splice proteins */
    displayId = acc;
    
    /* duplicate the following info from its parent protein */
    sprintf(cond_str, "accession='%s'", parAcc);
    division   = sqlGetField(proteinsDB, "spXref3", "division", cond_str);
    hugoSymbol = sqlGetField(proteinsDB, "spXref3", "hugoSymbol", cond_str);
    hugoDesc   = sqlGetField(proteinsDB, "spXref3", "hugoDesc", cond_str);
    
    fprintf(o2, "%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\n", acc, displayId, 
	    division, bioentryId, bioDatabase, desc, hugoSymbol, hugoDesc);
    
   row2 = sqlNextRow(sr2);
   }

fclose(o2);

sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);

return(0);
}

