/* pbCalResStd- Calculate the avg frequency and standard deviation of each AA residue for the proteins in a specific genome*/

#define MAXN 100000
#define MAXRES 23

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"
#include "math.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalResStd calculates the avg frequency and standard deviation of every AA residues of the proteins in a specific genome\n"
  "usage:\n"
  "   pbCalResStd sss xxx hhh\n"
  "      sss is the SWISS-PROT database name\n"
  "      xxx is the taxnomy number of the genome\n"
  "      sss is the genome database name\n"
  "Example: pbCalResStd sp031112 9606 hg16\n"
  );
}

double freq[MAXN][MAXRES];
double avg[MAXRES];
double sumJ[MAXRES];
double sigma[MAXRES];
double sumJ[MAXRES];

int recordCnt;
double recordCntDouble;
double lenDouble;
double sum;

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char *r1, *r2;
char cond_str[255];
char *proteinDatabaseName;
char emptyStr[1] = {""};
FILE *o1, *o2;
char *accession;
char *aaSeq;
char *chp;
int i, j, len;
char *answer;
char *protDisplayId;
int aaResCnt[30];
double aaResCntDouble[30];
char aaAlphabet[30];
int aaResFound;
int totalResCnt;

int icnt, jcnt;
double aaLenDouble[100000];
char *taxon;
char *database;

if (argc != 4) usage();

strcpy(aaAlphabet, "WCMHYNFIDQKRTVPGEASLXZB");

proteinDatabaseName = argv[1];
taxon = argv[2];
database = argv[3];

o1 = mustOpen("resData.tab", "w");
o2 = mustOpen("resAvgStd.tab", "w");

conn  = hAllocConn();
conn2 = hAllocConn();

sprintf(query2,"select acc from %s.accToTaxon where taxon=%s;", proteinDatabaseName, taxon);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
icnt = 0;
jcnt = 0;

for (j=0; j<MAXRES; j++)
    {
    sumJ[j] = 0;
    }

while (row2 != NULL)
    {
    accession = row2[0];   
  
    sprintf(cond_str, "acc='%s'", accession);
    protDisplayId = sqlGetField(conn, proteinDatabaseName, "displayId", "val", cond_str);

    sprintf(cond_str, "acc='%s'", accession);
    aaSeq = sqlGetField(conn, proteinDatabaseName, "protein", "val", cond_str);

    len  = strlen(aaSeq);
    lenDouble = (double)len;

    for (j=0; j<MAXRES; j++)
    	{
    	aaResCnt[j] = 0;
	}

    chp = aaSeq;
    for (i=0; i<len; i++)
	{
	aaResFound = 0;
	for (j=0; j<MAXRES; j++)
	    {
	    if (*chp == aaAlphabet[j])
		{
		aaResFound = 1;
		aaResCnt[j] ++;
		}
	    }
	if (!aaResFound)
	    {
	    fprintf(stderr, "%c %d not a valid AA residue.\n", *chp, *chp);
	    }
	chp++;
	}

    for (j=0; j<MAXRES; j++)
	{	
	freq[icnt][j] = (double)aaResCnt[j]/lenDouble;
	sumJ[j] = sumJ[j] + freq[icnt][j];
	}

    fprintf(o1, "%s\t%s\t%d", accession, protDisplayId, len);
    for (j=0; j<MAXRES; j++)
	{
	fprintf(o1, "\t%7.5f", freq[icnt][j]);
	}
    fprintf(o1, "\n");
    icnt++;
    row2 = sqlNextRow(sr2);
    }

recordCnt = icnt;
recordCntDouble = (double)recordCnt;

carefulClose(&o1);
sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);

for (j=0; j<MAXRES; j++)
    {
    avg[j] = sumJ[j]/recordCntDouble;
    }

for (j=0; j<20; j++)
    {
    sum = 0.0;
    for (i=0; i<recordCnt; i++)
     	{
	sum = sum + (freq[i][j] - avg[j]) * (freq[i][j] - avg[j]);
	}
    sigma[j] = sqrt(sum/(double)(recordCnt-1));
    fprintf(o2, "%c\t%f\t%f\n", aaAlphabet[j], avg[j], sigma[j]);
    }

carefulClose(&o2);
return(0);
}

