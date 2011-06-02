/* doOmimDisorders - This is a one shot program used in the OMIM related subtracks build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doOmimDisorders - This program is part of the OMIM related subtracks build pipeline.\n"
  "             It concats disorders1, disorders2, disorders3 together then parses out individual disorders."
  "usage:\n"
  "   doOmimDisorders db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: doOmimDisorders hg19 omimDisorderPhenotype.tab\n");
}

char *omimId;
FILE   *outf;

int main(int argc, char *argv[])
{
char *database;
char *outFn;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

int iCnt;

char *chp1, *chp2, *chp9;
char *geneSymbol, *location;
boolean questionable;

if (argc != 3) usage();

database = argv[1];
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

sprintf(query2,
"select omimId, concat(disorders1,' ',disorders2, ' ',disorders3), geneSymbol, location from omimGeneMap where disorders1 <>''");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    omimId = row2[0];
    iCnt = 0;
    chp1 = row2[1];
    
    geneSymbol = row2[2];
    location = row2[3];

    chp9 = strstr(chp1, ";");
    while (chp9 != NULL)
    	{
	questionable = FALSE;
	*chp9 = '\0';
	while (*chp1 == ' ') chp1++;
	
	if (*chp1 == '?') 
	    {
	    questionable = TRUE;
	    chp1++;
	    }
	chp2 = chp9;
	chp2--;
	while (*chp2 == ' ')
	    {
	    *chp2 = '\0';
	    chp2--;
	    }

	fprintf(outf, "%s\t%s\t%s\t%s\t%d\n", chp1, geneSymbol, omimId, location,questionable); 
	chp9++;
	chp1 = chp9;
        chp9 = strstr(chp1, ";");
	}
	
    while (*chp1 == ' ') chp1++;
    chp2 = chp1 + strlen(chp1);
    chp2--;
    while (*chp2 == ' ')
	{
	*chp2 = '\0';
	chp2--;
	}
    questionable = FALSE;
    if (*chp1 == '?') 
    	{
	questionable = TRUE;
	chp1++;
	}

    fprintf(outf, "%s\t%s\t%s\t%s\t%d\n", chp1, geneSymbol, omimId, location, questionable); 
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}

