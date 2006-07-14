/* spToProteins- Create tab delimited data file for proteinsxxxx database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spToProteins- Create tab delimited data files from spxxxx database for proteinsxxxx database.\n"
  "usage:\n"
  "   spToProteins xxxx\n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "Example: spToProteins 100503\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2;
char cond_str[255];
char proteinDatabaseName[255];
char proteinsDB[255];
char *isCurated;
char *desc;
char *hugoSymbol, *hugoDesc;
char empty_str[1] = {""};
FILE *o1, *o2, *o3;
char *proteinDataDate;
int  bioDatabase, bioentryId;
char *displayId;
char *accession;
char *extAC;
char *extDb;
int taxon;
struct slName *taxonList, *name;

if (argc != 2) usage();

proteinDataDate = argv[1];
sprintf(proteinDatabaseName, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);

o1 = mustOpen("temp_spXref2.dat", "w");
o2 = mustOpen("spXref3.tab", "w");
o3 = mustOpen("temp_spOrganism.dat", "w");

conn  = hAllocConn();
conn2 = hAllocConn();
conn3 = sqlConnect(proteinDatabaseName);

bioentryId = 0;

sprintf(query2,"select acc, isCurated from %s.info;", proteinDatabaseName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    bioentryId++;
    accession		= row2[0];   
    isCurated           = row2[1];
  
    if (*isCurated == '1')
	{
	bioDatabase = 1;
	}
    else
	{
	if (strlen(accession) > 7) 
	    {
	    bioDatabase = 3;
	    }
	else
	    {
	    bioDatabase = 2;
	    }
	}
    sprintf(cond_str, "acc='%s'", accession);
    displayId = sqlGetField(conn, proteinDatabaseName, "displayId", "val", cond_str);

    // !!! the divsion field probably should be eliminated later
    // use the simple 1 taxon returned value function for the time being, 
    // could expand into multiple by calling spBinomialNames later 
    taxon = spTaxon(conn3, accession);

    taxonList = spTaxons(conn3, accession);
    for (name = taxonList; name != NULL; name = name->next)
	{
	fprintf(o3, "%s\t%s\n", displayId, name->name);
	}

    sprintf(cond_str, "acc='%s'", accession);
    desc = sqlGetField(conn, proteinDatabaseName, "description", "val", cond_str);
    sprintf(cond_str, "swissprot='%s'", accession);
    hugoSymbol = sqlGetField(conn, proteinsDB, "hugo", "symbol", cond_str);
    hugoDesc = sqlGetField(conn, proteinsDB, "hugo", "name", cond_str);
    if (hugoSymbol==NULL) hugoSymbol = empty_str;
    if (hugoDesc==NULL)   hugoDesc   = empty_str;

    fprintf(o2, "%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\n", accession, displayId, 
	   taxon, bioentryId, bioDatabase, desc, hugoSymbol, hugoDesc);
    
    sprintf(query, 
	    "select extAcc1, extDb.val from sp%s.extDb, sp%s.extDbRef where extDbRef.acc='%s' %s",
	    proteinDataDate, proteinDataDate, accession, "and extDb.id = extDbRef.extDb;"); 
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
    	extAC = row[0];
	extDb = row[1];

        fprintf(o1, "%s\t%s\t%d\t%s\t%s\t%d\t%d\n", accession, displayId, taxon, 
		extDb, extAC, bioentryId, bioDatabase);
  
    	sqlFreeResult(&sr3);
	row = sqlNextRow(sr);
	}
   sqlFreeResult(&sr);
   row2 = sqlNextRow(sr2);
   }

fclose(o1);
fclose(o2);
fclose(o3);

sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);
sqlDisconnect(&conn3);

system("cat temp_spXref2.dat | sort |uniq > spXref2.tab");
system("rm temp_spXref2.dat");
system("cat temp_spOrganism.dat | sort |uniq > spOrganism.tab");
system("rm temp_spOrganism.dat");
return(0);
}

