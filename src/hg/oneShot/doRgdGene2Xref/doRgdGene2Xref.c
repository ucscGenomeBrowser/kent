/* doRgdGene2Xref - This is a one shot program used in the RGD Genes build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doRgdGene2Xref - This program is part of the RGD Genes build pipeline.\n"
  "             It parses the rgdId col of the gene records of the raw RGD gene data\n"
  "             and put the results into two xref output files\n"
  "usage:\n"
  "   doRgdGene2Xref db fnXref1 fnXref2\n"
  "      db is the database name\n"
  "      fnXref1 is the filename of the 1st xref output\n"
  "      fnXref2 is the filename of the 2nd xref output\n"
  "example: doRgdGene2Xref rn4 xref1.out xref2.out\n");
}

char *rgdId;
FILE   *o2, *o3;

/* process the Dbxref item */
void processDbxref(char *itemStr)
{
char *chp1;
char *chp;
char *chp3;

char buf[2048];

strcpy(buf, itemStr);

chp1 = strstr(buf,"=");
chp1 ++;
chp = chp1;
chp = strstr(chp, ",");

while (chp != NULL)
    {
    *chp = '\0';
    chp3 = chp1;
    while (*chp3 != '\0')
    	{
	if (*chp3 == ':') *chp3 = '\t';
	chp3++;
	}

    // skip printing the RGD id field
    if (strstr(chp1, "RGD\t") == NULL)
    	{
	fprintf(o3, "%s\t%s\n", rgdId, chp1); fflush(stdout);
    	}
    chp++;
    chp1= chp;
    chp = strstr(chp, ",");
    }

chp3 = chp1;
while (*chp3 != '\0')
    {
    if (*chp3 == ':') *chp3 = '\t';
    chp3++;
    }

// skip printing the RGD id field
if (strstr(chp1, "RGD\t") == NULL)
    {
    fprintf(o3, "%s\t%s\n", rgdId, chp1); fflush(stdout);
    }
}

/* process an item */
void processItem(char *itemStr)
{
char *chp1, *chp2;
char chSav;

/* the RGD ID is in the Dbxref field */
chp1 = strstr(itemStr, "Dbxref=");
if (chp1 != NULL)
    {
    chp2 = strstr(chp1, "RGD:");
    if (chp2 != NULL)
    	{
	chp1 = strstr(chp2, ",");
	if (chp1 != NULL) 
	    {
	    chSav = *chp1;
	    *chp1 = '\0';
	    }
	rgdId = strdup(chp2);
	if (chp1 != NULL)
	    {
	    *chp1 = chSav;
	    }
	}

    /* other IDs are also in the Dbxref field, get them with processDbxref() */	
    processDbxref(itemStr);
    }
}

int main(int argc, char *argv[])
{
char *database;
char *fnXref1, *fnXref2;

struct sqlConnection *conn2, *conn3;
char query2[256];
struct sqlResult *sr2;
char **row2;

int iCnt;
int j;
char *item[100];

char *chp0;
char *chp1, *chp2, *chp3;

/* some records from RGD raw data have huge size */
char inLine1[20000];
char inLine2[20000];

if (argc != 4) usage();

database = argv[1];
conn2= hAllocConn(database);
conn3= hAllocConn(database);

fnXref1 = argv[2];
fnXref2 = argv[3];

o2 = fopen(fnXref1, "w");
o3 = fopen(fnXref2, "w");

sqlSafef(query2, sizeof query2, "select rgdId from rgdGeneRaw0 where feature='gene'");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    rgdId = NULL;
    strcpy(inLine1, row2[0]);
    strcpy(inLine2, row2[0]);
    iCnt = 0;
    item[0] = inLine2;

    /* parse out individual items first */
    chp1 = inLine1;
    chp2 = inLine2;
    while (*chp1 != '\0')
    	{
	if (*chp1 == ';') 
	    {
	    iCnt++;
	    *chp1 = '\t';
	    *chp2 = '\0';
	    chp0 = chp2;
	    chp0++;
	    item[iCnt] = chp0;
	    }
	chp1++;
	chp2++;
	}

    for (j=0; j<iCnt; j++)
    	{
	processItem(item[j]);	
	}

    if (rgdId != NULL)
    	{
	for (j=0; j<iCnt; j++)
    	    {
	    chp3 = item[j];
	    while (*chp3 != '\0')
    		{
    		if (*chp3 == '=') *chp3 = '\t';
    		chp3++;
    		}

	    // skip printing of the Dbxref item. 		
	    if (strstr(item[j], "Dbxref\t") == NULL)
		{
		fprintf(o2, "%s\t%s\n", rgdId, item[j]);fflush(stdout);
	    	}
	    }
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(o2);
fclose(o3);
hFreeConn(&conn2);
hFreeConn(&conn3);
    
return(0);
}

