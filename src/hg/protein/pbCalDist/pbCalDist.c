/* pbCalDist - Create tab delimited data files to be used by Proteome Browser stamps */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalDist- Create tab delimited data files to be used by Proteome Browser stamps.\n"
  "usage:\n"
  "   pbCalDist spDb protsDb taxn gnDb\n"
  "      spDb is the name of SWISS-PROT database\n"
  "      protsDb is the name of proteinsXXXXXX database\n"
  "      taxn is the taxnomy number of the Taxonomy ID\n"
  "      gnDb is the genome database name\n"
  "Example: pbCalDist sp031112 proteins031112 9606 hg16\n");
}

int calDist(double *measure, int nInput, int nDist, double xMin, double xDelta, char *oFileName)
/* calculate histogram distribution of a double array of nInput elements */ 
{
int distCnt[1000];
double xDist[1000];
FILE *o3;
int i,j;
int highestCnt, totalCnt;

assert(nDist < ArraySize(distCnt));

o3 = mustOpen(oFileName, "w");
for (j=0; j<=nDist; j++)
    {
    distCnt[j] = 0;
    xDist[j] = xMin + xDelta * (double)j;
    }

for (i=0; i<nInput; i++)
    {
    if (measure[i] <= xDist[0])
	{
	distCnt[0]++;
	}
    for (j=1; j<nDist; j++)
	{
	if ((measure[i] > xDist[j-1]) && (measure[i] <= xDist[j]))
 	    {
	    distCnt[j]++;
	    }
	}
    if (measure[i] > xDist[nDist-1])
	{
	distCnt[nDist]++;
	}
    }

highestCnt = 0;
totalCnt   = 0;
for (j=0; j<=nDist; j++)
    {
    if (distCnt[j] > highestCnt) highestCnt = distCnt[j];
    totalCnt = totalCnt + distCnt[j];
    }
    
if (totalCnt != nInput)
    errAbort("nInput %d is not equal totalCnt %d, aborting ...\n", nInput, totalCnt);
   
// do not print out count of the last inteval, which is everything beyond xMax 
for (j=0; j<nDist; j++)
    {
    fprintf(o3, "%f\t%d\n", xDist[j], distCnt[j]);
    }
carefulClose(&o3);
   
return(highestCnt);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2, **row3;
char *r1, *r2, *r3, *r4;
char cond_str[255];
char *proteinDatabaseName;	// example: sp031112
char *protDbName;		// example: proteins031112
char emptyStr[1] = {""};
FILE *o1, *o2, *o3, *o4, *o5;
char *accession;
char *aaSeq;
char *chp;
int i, j, len;
int cCnt;
char *answer, *answer2;
double hydroSum;
char *protDisplayId;
char *kgId;
int aaResCnt[30];
double aaResCntDouble[30];
char aaAlphabet[30];
int aaResFound;
int totalResCnt;
int molWtCnt;
double molWt[100000];
int pIcnt;
double pI[100000];

double aa_hydro[256];
int icnt, jExon, pcnt, ipcnt;
double aaLenDouble[100000];
double avgHydro[100000];
double cCountDouble[100000];
double exonCountDouble[100000];
double pfamCountDouble[100000];
double interProCountDouble[100000];
char *taxon;
char *database;
char *exonCnt;
int pfamCount;
int interProCount;
struct slName *taxonList, *name;

if (argc != 5) usage();

strcpy(aaAlphabet, "WCMHYNFIDQKRTVPGEASLXZB");

//Ala:  1.800  Arg: -4.500  Asn: -3.500  Asp: -3.500  Cys:  2.500  Gln: -3.500
aa_hydro['A'] =  1.800;
aa_hydro['R'] = -4.500;
aa_hydro['N'] = -3.500;
aa_hydro['D'] = -3.500;
aa_hydro['C'] =  2.500;
aa_hydro['Q'] = -3.500;

//Glu: -3.500  Gly: -0.400  His: -3.200  Ile:  4.500  Leu:  3.800  Lys: -3.900
aa_hydro['E'] = -3.500;
aa_hydro['G'] = -0.400;
aa_hydro['H'] = -3.200;
aa_hydro['I'] =  4.500;
aa_hydro['L'] =  3.800;
aa_hydro['K'] = -3.900;

//Met:  1.900  Phe:  2.800  Pro: -1.600  Ser: -0.800  Thr: -0.700  Trp: -0.900
aa_hydro['M'] =  1.900;
aa_hydro['F'] =  2.800;
aa_hydro['P'] = -1.600;
aa_hydro['S'] = -0.800;
aa_hydro['T'] = -0.700;
aa_hydro['W'] = -0.900;

//Tyr: -1.300  Val:  4.200  Asx: -3.500  Glx: -3.500  Xaa: -0.490
aa_hydro['Y'] = -1.300;
aa_hydro['V'] =  4.200;

proteinDatabaseName = argv[1];
protDbName 	    = argv[2];
taxon 	 	    = argv[3];
database 	    = argv[4];

//o1 = mustOpen("pepProp.tab",    "w");
o2 = mustOpen("pepResDist.tab", "w");
//o4 = mustOpen("pfamCount.tab",  "w");
//o5 = mustOpen("interProCount.tab",  "w");

conn  = hAllocConn();
conn2 = hAllocConn();

for (j=0; j<23; j++)
    {
    aaResCnt[j] = 0;
    }

icnt = jExon = pcnt = 0;
//interProCount = 0;
pIcnt = 0;
molWtCnt = 0;
//pfamCount = 0;

sprintf(query2,"select acc from %s.accToTaxon where taxon=%s;", proteinDatabaseName, taxon);
sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

while (row2 != NULL)
    {
    accession = row2[0];   

    sprintf(cond_str, "acc='%s'", accession);
    protDisplayId = sqlGetField(conn, proteinDatabaseName, "displayId", "val", cond_str);
    
    sprintf(cond_str, "proteinID='%s'", protDisplayId);
    answer = sqlGetField(conn, database, "knownGene", "name", cond_str);

    // count InterPro domains
    if (answer != NULL)
	{
    	sprintf(cond_str, "accession='%s'", accession);
    	answer2 = sqlGetField(conn, protDbName, "swInterPro", "count(*)", cond_str);
	if (answer2 != NULL)
	    {
	    interProCount = interProCount + atoi(answer2);
	    interProCountDouble[ipcnt] = (double)(atoi(answer2));
	    //fprintf(o5, "%s\t%s\n", accession, answer2);
	    ipcnt++;
	    }
	else
	    {
	    printf("%s is not in  InterPro DB.\n", accession);fflush(stdout);
	    }
	}
    
    // count Pfam domains
    // This part is replaced by InterPro count
    /*
    if (answer != NULL)
	{
	kgId = answer;;
    	sprintf(cond_str, "name='%s'", kgId);
    	answer2 = sqlGetField(conn, database, "knownToPfam", "count(*)", cond_str);
	if (answer2 != NULL)
	    {
	    pfamCount = pfamCount + atoi(answer2);
	    pfamCountDouble[pcnt] = (double)(atoi(answer2));
	    fprintf(o4, "%s\t%s\n", accession, answer2);
	    pcnt++;
	    }
	}
    */
    
    // count exons, using coding exons from kgProtMap table
    sprintf(cond_str, "qName='%s'", accession);
    answer2 = sqlGetField(conn, database, "kgProtMap", "blockCount", cond_str);

    if (answer2 != NULL)
	{
	exonCnt = strdup(answer2);
	if (atoi(exonCnt) == 0)
	    {
	    errAbort("%s %s %s has 0 block count\n", accession, protDisplayId, kgId);
	    }
	exonCountDouble[jExon] = (double)(atoi(exonCnt));
	jExon++;
	}
    else
	{
	exonCnt = emptyStr;
	}
    
    // process Mol Wt
    sprintf(cond_str, "accession='%s'", accession);
    answer2 = sqlGetField(conn, database, "pepMwAa", "molWeight", cond_str);
    if (answer2 != NULL)
	{
	molWt[molWtCnt] = (double)(atof(answer2));
	molWtCnt++;
	}
    
    // process pI
    sprintf(cond_str, "accession='%s'", accession);
    answer2 = sqlGetField(conn, database, "pepPi", "pI", cond_str);
    if (answer2 != NULL)
	{
	pI[pIcnt] = (double)(atof(answer2));
	pIcnt++;
	}
     
    sprintf(cond_str, "acc='%s'", accession);
    aaSeq = sqlGetField(conn, proteinDatabaseName, "protein", "val", cond_str);
    if (aaSeq == NULL)
	{
	errAbort("%s does not have protein sequence data in %s, aborting ...\n", accession, 
		 proteinDatabaseName);
	}

    len  = strlen(aaSeq);

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
    
    // calculate hydrophobicity
    chp  = aaSeq;
    cCnt = 0;
    hydroSum = 0;
    for (i=0; i<len; i++)
	{
        hydroSum = hydroSum + aa_hydro[(int)(*chp)];

	// count Cysteines
	if ((*chp == 'C') || (*chp == 'c'))
	    {
	    cCnt ++;
	    }
	chp++;
	}

    aaLenDouble[icnt]  = len;
    cCountDouble[icnt] = (double)cCnt;
    avgHydro[icnt] = hydroSum/(double)len; 
    //fprintf(o1, "%s\t%s\t%d\t%s\t%d\t%f\n", 
    // 	    accession, protDisplayId, len, exonCnt, cCnt, avgHydro[icnt]);fflush(stdout);
    icnt++;
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }

totalResCnt = 0;
for (i=0; i<23; i++)
    {
    totalResCnt = totalResCnt + aaResCnt[i];
    }

// write out residue count distribution
for (i=0; i<20; i++)
    {
    aaResCntDouble[i] = ((double)aaResCnt[i])/((double)totalResCnt);
    fprintf(o2, "%d\t%f\n", i+1, (float)aaResCntDouble[i]);
    }
fprintf(o2, "%d\t%f\n", i+1, 0.0);
carefulClose(&o2);

// calculate and write out various distributions
calDist(molWt,  	 molWtCnt, 20, 10000.0, 10000.0,"pepMolWtDist.tab");
calDist(pI,  	         pIcnt,    61,     3.0, 0.2, 	"pepPiDist.tab");
calDist(avgHydro,     	  icnt,    41,    -2.0, 0.1, 	"pepHydroDist.tab");
calDist(cCountDouble, 	  icnt,    51,     0.0, 1.0, 	"pepCCntDist.tab");
calDist(exonCountDouble, jExon,    31,     0.0, 1.0, 	"pepExonCntDist.tab");
calDist(interProCountDouble,  ipcnt,    16,     0.0, 1.0, 	"pepIPCntDist.tab");
//calDist(pfamCountDouble,  pcnt,    16,     0.0, 1.0, 	"pfamCntDist.tab");

//carefulClose(&o1);
//carefulClose(&o4);

sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);
return(0);
}
