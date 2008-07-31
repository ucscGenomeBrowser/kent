/* kgProtAlias - generate protein alias list table */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgProtAlias - create protein alias .tab files "
  "usage:\n"
  "   kgProtAlias xxxx yyyy\n"
  "            xxxx is genome  database name\n"
  "            yyyy is protein database date \n"
  "example: kgProtAlias hg16 040315\n");
}

int main(int argc, char *argv[])
    {
    struct sqlConnection *conn, *conn2;
    char query[256], query2[256];
    struct sqlResult *sr, *sr2;
    char **row, **row2;

    char *kgID;
    FILE *o1;

    char cond_str[256];
    char *database;
    char spDB[256];
    char proteinDB[256];

    char *chp;

    char *alignID;
    char *displayID, *secondaryID, *pdbID;
    char *proteinAC;
    char *ncbiProtAc;

    if (argc != 3) usage();
    database  = cloneString(argv[1]);
    
    sprintf(spDB, "sp%s", argv[2]);
    sprintf(proteinDB, "proteins%s", argv[2]);

    conn = hAllocConn(database);
    conn2= hAllocConn(database);

    o1 = fopen("j.dat", "w");

    sprintf(query2,"select name, proteinID, alignID from %s.knownGene;", database);
    
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
	kgID		= row2[0];
	displayID	= row2[1];
	alignID		= row2[2];

	fprintf(o1, "%s\t%s\t%s\n", kgID, displayID, displayID);
       
        sprintf(cond_str, "displayID = '%s'", displayID);
        proteinAC = sqlGetField(proteinDB, "spXref3", "accession", cond_str);
        if (proteinAC != NULL)
		{
		fprintf(o1, "%s\t%s\t%s\n", kgID, displayID, proteinAC);
        
		sprintf(cond_str, "acc = '%s' and extDb=1", proteinAC);
        	ncbiProtAc = sqlGetField(spDB, "extDbRef", "extAcc2", cond_str);
		if (ncbiProtAc != NULL)
		    {
		    chp = strstr(ncbiProtAc, ".");
		    if (chp != NULL) *chp = '\0';
		    fprintf(o1, "%s\t%s\t%s\n", kgID, displayID, ncbiProtAc);
		    }
		}
	else
		{
		fprintf(stderr, "%s\t%s\t%s does not have protein accession number!\n", 
			kgID, displayID, alignID);
		fflush(stderr);
		break;
		}
 
	sprintf(query,"select accession2 from %s.spSecondaryID where displayID='%s';", 
		proteinDB, displayID);
    	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);
    	while (row != NULL)
		{
		secondaryID = row[0];
		fprintf(o1, "%s\t%s\t%s\n", kgID, displayID, secondaryID);
		row = sqlNextRow(sr);
		}
    	sqlFreeResult(&sr);

        sprintf(query,"select pdb from %s.pdbSP where sp='%s';", proteinDB, displayID);
    	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);
    	while (row != NULL)
		{
		pdbID = row[0];
		fprintf(o1, "%s\t%s\t%s\n", kgID, displayID, pdbID);
		row = sqlNextRow(sr);
		}
    	sqlFreeResult(&sr);
	fflush(o1);
   
	row2 = sqlNextRow(sr2);
	}
    sqlFreeResult(&sr2);

    fclose(o1);
    hFreeConn(&conn);
    hFreeConn(&conn2);

    system("cat j.dat|sort|uniq  >kgProtAlias.tab");
    system("rm j.dat");
    
    return(0);
    }
