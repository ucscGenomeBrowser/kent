/* pbCalDistGlobal - Create tab delimited data files to be used by Proteome Browser stamps */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

#define MAX_PROTEIN_CNT 4000000

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalDistGlobal- Create tab delimited data files to be used by Proteome Browser stamps.\n"
  "usage:\n"
  "   pbCalDistGlobal spDb protsDb\n"
  "      spDb is the name of SWISS-PROT database\n"
  "      protsDb is the name of proteinsXXXXXX database\n"
  "Example: pbCalDistGlobal sp040915 proteins040915\n");
}

int calDist(double *measure, int nInput, int nDist, double xMin, double xDelta, char *oFileName)
/* calculate histogram distribution of a double array of nInput elements */ 
{
int distCnt[1000];
double xDist[1000];
FILE *o3;
int i,j;
int highestCnt, totalCnt;
int lowCnt, hiCnt;

printf("processing %s\n", oFileName);fflush(stdout);
assert(nDist < ArraySize(distCnt));

o3 = mustOpen(oFileName, "w");
for (j=0; j<=(nDist+1); j++)
    {
    distCnt[j] = 0;
    xDist[j] = xMin + xDelta * (double)j;
    }

lowCnt = 0;
hiCnt  = 0;
for (i=0; i<nInput; i++)
    {
    /* count values below xmin */
    if (measure[i] < xDist[0])
	{
	lowCnt++;
	}
    
    for (j=0; j<nDist; j++)
	{
	if ((measure[i] >= xDist[j]) && (measure[i] < xDist[j+1]))
 	    {
	    distCnt[j]++;
	    }
	}

    /* count values above xmax */
    if (measure[i] >= xDist[nDist])
	{
	hiCnt++;
	}
    }

highestCnt = 0;
totalCnt   = 0;
for (j=0; j<nDist; j++)
    {
    if (distCnt[j] > highestCnt) highestCnt = distCnt[j];
    totalCnt = totalCnt + distCnt[j];
    }

printf("\tdisplayedCnt=%d lowCnt=%d hiCnt=%d total=%d\n", totalCnt, lowCnt, hiCnt, 
totalCnt + hiCnt + lowCnt);fflush(stdout);
totalCnt = totalCnt + hiCnt + lowCnt;
if (totalCnt != nInput)
    errAbort("nInput %d is not equal totalCnt %d, aborting ...\n", nInput, totalCnt);
  
for (j=0; j<nDist; j++)
    {
    fprintf(o3, "%f\t%d\n", xDist[j], distCnt[j]);
    }
carefulClose(&o3);
return(highestCnt);
}

double molWt[MAX_PROTEIN_CNT];
double pI[MAX_PROTEIN_CNT];
double aaLenDouble[MAX_PROTEIN_CNT];
double avgHydro[MAX_PROTEIN_CNT];
double cCountDouble[MAX_PROTEIN_CNT];
double exonCountDouble[MAX_PROTEIN_CNT];
double pfamCountDouble[MAX_PROTEIN_CNT];
double interProCountDouble[MAX_PROTEIN_CNT];
int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
char query2[256];
struct sqlResult *sr2;
char **row2;
char cond_str[255];
char *proteinDatabaseName;	/* example: sp031112 */
char *protDbName;		/* example: proteins031112 */
FILE *o2;
char *accession;
char *aaSeq;
char *chp;
int i, j, len;
int cCnt;
char *answer3;
double hydroSum;
int aaResCnt[30];
double aaResCntDouble[30];
char aaAlphabet[30];
int aaResFound;
int totalResCnt;
int molWtCnt;
int pIcnt;
char *database;
int aaSize;

double aa_hydro[256];
int icnt, jExon, pcnt;
int ipcnt={0};
int interProCount;
if (argc != 3) usage();

strcpy(aaAlphabet, "WCMHYNFIDQKRTVPGEASLXZB");

/* Ala:  1.800  Arg: -4.500  Asn: -3.500  Asp: -3.500  Cys:  2.500  Gln: -3.500 */
aa_hydro['A'] =  1.800;
aa_hydro['R'] = -4.500;
aa_hydro['N'] = -3.500;
aa_hydro['D'] = -3.500;
aa_hydro['C'] =  2.500;
aa_hydro['Q'] = -3.500;

/* Glu: -3.500  Gly: -0.400  His: -3.200  Ile:  4.500  Leu:  3.800  Lys: -3.900 */
aa_hydro['E'] = -3.500;
aa_hydro['G'] = -0.400;
aa_hydro['H'] = -3.200;
aa_hydro['I'] =  4.500;
aa_hydro['L'] =  3.800;
aa_hydro['K'] = -3.900;

/* Met:  1.900  Phe:  2.800  Pro: -1.600  Ser: -0.800  Thr: -0.700  Trp: -0.900 */ 
aa_hydro['M'] =  1.900;
aa_hydro['F'] =  2.800;
aa_hydro['P'] = -1.600;
aa_hydro['S'] = -0.800;
aa_hydro['T'] = -0.700;
aa_hydro['W'] = -0.900;

/* Tyr: -1.300  Val:  4.200  Asx: -3.500  Glx: -3.500  Xaa: -0.490 */
aa_hydro['Y'] = -1.300;
aa_hydro['V'] =  4.200;

proteinDatabaseName = argv[1];
protDbName 	    = argv[2];
database 	    = argv[2];

o2 = mustOpen("pepResDist.tab", "w");

conn2 = sqlConnect(database);
conn3 = sqlConnect(protDbName);

for (j=0; j<23; j++)
    {
    aaResCnt[j] = 0;
    }

icnt = jExon = pcnt = 0;
pIcnt = 0;
molWtCnt = 0;

safef(query2, sizeof(query2), 
"select info.acc, molWeight, aaSize, protein.val, Pi from %s.info, %s.protein, %s.pepPi where info.acc=protein.acc and pepPi.accession=protein.acc", 
      proteinDatabaseName, proteinDatabaseName, database);

sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

while (row2 != NULL)
    {
    accession = row2[0];   
    molWt[molWtCnt] = (double)(atof(row2[1]));
    molWtCnt++;
    aaSize = atoi(row2[2]);
    aaSeq  = row2[3];
     
    pI[pIcnt] = (double)(atof(row2[4]));
    pIcnt++;
    
    /* count InterPro domains */
    safef(cond_str, sizeof(cond_str), "accession='%s'", accession);
    answer3 = sqlGetField(conn3, protDbName, "swInterPro", "count(*)", cond_str);
    if (answer3 != NULL)
	{
	interProCount = interProCount + atoi(answer3);
	interProCountDouble[ipcnt] = (double)(atoi(answer3));
	ipcnt++;
	}
    else
	{
	printf("%s is not in  InterPro DB.\n", accession);fflush(stdout);
	}
    
    len  = aaSize;

    chp = aaSeq;
    for (i=0; i<len; i++)
	{
	aaResFound = 0;
	for (j=0; j<23; j++)
	    {
	    if (*chp == aaAlphabet[j])
		{
		aaResFound = 1;
		aaResCnt[j] ++;
		}
	    }
	if (!aaResFound)
	    {
	    errAbort("%c %d not a valid AA residue in %s:\n%s\n", *chp, *chp, accession, aaSeq);
	    }
	chp++;
	}
    
    /* calculate hydrophobicity */
    chp  = aaSeq;
    cCnt = 0;
    hydroSum = 0;
    for (i=0; i<len; i++)
	{
        hydroSum = hydroSum + aa_hydro[(int)(*chp)];

    /* count Cysteines */
	if ((*chp == 'C') || (*chp == 'c'))
	    {
	    cCnt ++;
	    }
	chp++;
	}

    aaLenDouble[icnt]  = len;
    cCountDouble[icnt] = (double)cCnt;
    avgHydro[icnt] = hydroSum/(double)len;

    icnt++;
    if ((icnt % 10000) == 0)
        {
	printf("%d done.\n", icnt);fflush(stdout);
	system("date");
	}
    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
sqlDisconnect(&conn2);
sqlDisconnect(&conn3);

totalResCnt = 0;
for (i=0; i<23; i++)
    {
    totalResCnt = totalResCnt + aaResCnt[i];
    }

/* write out residue count distribution */
for (i=0; i<20; i++)
    {
    aaResCntDouble[i] = ((double)aaResCnt[i])/((double)totalResCnt);
    fprintf(o2, "%d\t%f\n", i+1, (float)aaResCntDouble[i]);
    }
fprintf(o2, "%d\t%f\n", i+1, 0.0);
carefulClose(&o2);

/* calculate and write out various distributions */
calDist(molWt,  	 molWtCnt, 21, 	   0.0, 10000.0,"pepMolWtDist.tab");
calDist(pI,  	         pIcnt,    61,     3.0, 0.2, 	"pepPiDist.tab");
calDist(avgHydro,     	  icnt,    41,    -2.0, 0.1, 	"pepHydroDist.tab");
calDist(cCountDouble, 	  icnt,    51,     0.0, 1.0, 	"pepCCntDist.tab");
calDist(interProCountDouble,ipcnt, 16,     0.0, 1.0, 	"pepIPCntDist.tab");

return(0);
}
