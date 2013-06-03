/* kgBestMrna - driver program to call blat to select best mRNA for each protein */

#include	<sys/param.h>
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

char proteinName[20], mrnaName[20];
char mrnaNames[500][20];
int  mrnaScore[500];
char proteinNameOld[20] = {""};

struct dnaSeq *seq;
HGID id;

bioSeq *mSeq, qSeq, *pSeq;
extern int answer_for_kg;

struct dnaSeq *untransList;

char line[2000];
char line2[2000];

int mrnaCount;
int proteinCount;

char mrnaNames[500][20];
char mrnaDates[500][20];
int  mrnaScore[500];
int  diffIdent[500];

FILE *o3, *o7;

char *proteinDataDate;
char *genomeRelease;
char *genomeReadOnly;
char *genomeDBname;
char proteinsDB[100];
char spDB[100];
char gbTempDB[100];

/* Explain usage and exit. */
void usage()
    {
    errAbort(
  	"usage:\tkgResultBestMrna YYMMDD db ro_db> BestResult.out\n"
  	"\tYYMMDD is the release date of SWISS-PROT data, eg: 031117\n"
  	"\tdb is the genome under construction, eg: kgDB\n"
  	"\tro_db is the actual target genome, e.g.: mm4\n"
  	"kgResultBestMrna - after the cluster run is done, this reads the\n"
	"\tresults and produces a best.lis listing.\n"
	"\tExpects to find ./clusterRun and ./out directories in the\n"
	"\tcurrent working directory.");
    }

int cal_months(char *date)
    {
    int year, month, day;
    int months;
	
    sscanf(date, "%d-%d-%d", &year, &month, &day);
    months = (year - 1970)*12 + month - 1;
    return(months);
    }

void calScores(char *proteinID, int mrnaCount)
{
int  ixm, maxixm;	// index for mRNA
int  maxScore;
int  i, ii;

char proteinName[20], mrnaName[20];
int  diffs[500];
int  monthss[500];
int  mrnalens[500];
char line[2000];
char mrnaDate[20];
int  months;
int  diff;
int  mrnalen;

char *temp_str;

struct dnaSeq *seq;
struct sqlConnection *connR;
    
connR = hAllocConn();

ixm = 0;
maxScore = 0;

strcpy(proteinName, proteinID);
maxixm = -1;
for (ii=0; ii<mrnaCount; ii++)
    {
    strcpy(mrnaName, mrnaNames[ii]);
    if (hRnaSeqAndIdx(mrnaNames[ii], &mSeq, &id, mrnaDate, connR)== -1) 
	{
	printf("%s could not be found!!!\n", mrnaNames[ii]);fflush(stdout);
	exit(1);
	}
    strcpy(mrnaDates[ii], mrnaDate);
    mrnalen = strlen(mSeq->dna);
    months = cal_months(mrnaDate);
		
    diffs[ii] 	   = diffIdent[ii];
    mrnalens[ii]   = mrnalen;
    monthss[ii]    = months;
    mrnaScore[ii]  = mrnalen + months*2 - diffs[ii]*50;

    if (mrnaScore[ii] > maxScore)
	{
	maxScore = mrnaScore[ii];
	maxixm 	 = ii;
	}	
    }

for (i=0; i<mrnaCount; i++)
    {
    if (diffIdent[i] != -1)
	{
	fprintf(o3, "%10s %10s %8s %5d %5d %5d %5d",
        	proteinName, mrnaNames[i], mrnaDates[i], mrnaScore[i],
                mrnalens[i], monthss[i], diffs[i]);
        if (i == maxixm)
            {
	    fprintf(o3, " *\n");
	    fprintf(o7, "%s\t%s\n", proteinName, mrnaNames[i]); fflush(o7);
	    }
        else
	    {
            fprintf(o3, "\n");
	    }
	fflush(o3);
	}
    }
fprintf(o3, "\n");
fflush(o3);

hFreeConn(&connR);
}

int main(int argc, char *argv[])
{
int  ixm, maxixm;       // index for mRNA
int  maxScore;
char proteinID[20], mrnaName[20];

int  monthss[500];
int  mrnalens[500];
FILE *inf, *inf2, *outf;

int  newMrna;
int  ii= -1;
char line[2000];
char mrnaDate[20];
int  months;
int  diff;
int  mrnalen;
int  imrna;
char dirName[PATH_MAX];
char mrnaFile[PATH_MAX];
char outName[PATH_MAX];
char outDir[PATH_MAX];
char blatCmd[PATH_MAX];
char cwd[PATH_MAX];

struct dnaSeq *seq;
HGID id;
bioSeq *mSeq, qSeq, *pSeq;

struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2, **row3;

char *accession;
char *displayID;
char *division;
char *extDB;
char *extAC;
  
char *chp0, *chp;

int totalCount, matchCount;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd, *exonCount, *exonStarts, *exonEnds;

int  i;
char *mrnaID;

FILE *aaOut, *mrnaOut;
char *aaSeq, *mrnaSeq;
char cond_str[200];

if (argc != 4) usage();
    
proteinDataDate = argv[1];
genomeRelease = argv[2];
genomeReadOnly = argv[3];

// make sure db connection goes to correct genome
// hRnaSeqAndIdx() needs this.
hSetDb(genomeReadOnly);

sprintf(spDB, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);
sprintf(gbTempDB, "%sTemp", genomeRelease);
  
inf = fopen("protein.lis", "r"); 
if ((FILE *) NULL == inf)
    errAbort("ERROR: Can not open input file: protein.lis");
o3  = fopen("kgBestMrna.out",   "w");
if ((FILE *) NULL == o3)
    errAbort("ERROR: Can not open output file: kgBestMrna.out");
o7  = fopen("best.lis",    "w");
if ((FILE *) NULL == o7)
    errAbort("ERROR: Can not open output file: best.lis");

conn2= hAllocConn();
conn3= hAllocConn();
   
proteinCount = 0; 
snprintf(dirName, (size_t) sizeof(dirName), "%s", "./clusterRun" );

while (fgets(line, 1000, inf) != NULL)
    {
    sscanf(line, "%s", proteinID);
    printf(">%s\n", proteinID);
    fflush(stdout);

    sprintf(cond_str, "val='%s'", proteinID);
    accession = sqlGetField(conn3, spDB, "displayId","acc", cond_str);
    sprintf(cond_str, "acc='%s'", accession);
    aaSeq = sqlGetField(conn3, spDB, "protein","val", cond_str);
	
    if (aaSeq == NULL)
	{
	printf("no seq found for %s\n", proteinID);
	fflush(stdout);
	exit(1);
	}	
	
    if ( 0 == (proteinCount % 2000) )
	{
	snprintf(dirName, (size_t) sizeof(dirName), "prot%05d", proteinCount );
	snprintf(outDir, (size_t) sizeof(outDir), "prot%05d", proteinCount );
	}

    sqlSafef(query2, sizeof query2, "select mrnaID from %sTemp.spMrna where spID='%s';",genomeRelease, proteinID);
	
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    imrna = 0;
    while (row2 != NULL)
	{
 	mrnaID 	= row2[0];
	strcpy(mrnaNames[imrna], mrnaID);

	sprintf(cond_str, "name='%s'", mrnaID);
    	mrnaSeq = sqlGetField(conn3,gbTempDB,"refMrna","seq", cond_str);
	row2 = sqlNextRow(sr2);
	imrna++;
	}
    mrnaCount = imrna;
    sqlFreeResult(&sr2);
    
    if ( ((char *) NULL) == getcwd(cwd, (size_t) sizeof(cwd)) )
	errAbort("ERROR: Can not get current working directory");
    snprintf(outName, (size_t) sizeof(outName),"%s/out/%s/b%05d.out",
	cwd, outDir, proteinCount);
    
    inf2 = fopen(outName, "r");
    if ((FILE *) NULL == inf2)
	errAbort("ERROR: Can not open result file: %s, errno: %d", outName, errno);
    for (i=0; i<mrnaCount; i++) diffIdent[i] = -1;
    imrna = 0;
    newMrna = 0;
    while (fgets(line2, 1000, inf2) != NULL)
    	{
	if (line2[0] == '>') 
	    {
	    newMrna = 1;
	    line2[strlen(line2)-1] = '\0';
	    chp = line2 + 1;
	    ii=-1;
	    for (i=0; i<mrnaCount; i++)
		{
		if (strcmp(chp, mrnaNames[i]) == 0)
		    {
		    ii = i;
		    }
		}
	    if (ii == -1)
		{
		errAbort("ERROR: no mrna registered found", outName, errno);
		}
	    }
	chp0 = strstr(line2, "Identities = ");
	if (chp0 == NULL) continue;

	if (!newMrna) continue;
	else newMrna = 0;

	chp0 = chp0 + strlen("Identities = ");

	chp = strstr(chp0, "/");
	*chp = '\0';
	sscanf(chp0, "%d", &matchCount);
	
	chp++;
	chp0  = strstr(chp, " ");
	*chp0 = '\0';
	sscanf(chp, "%d", &totalCount);

	// some time mulitiple alignments for a single mRNA-protein pair
	// the first one usually is the best alignment with highest score
	if (diffIdent[ii] == -1) 
	    {
	    diffIdent[ii] = totalCount - matchCount;
 	    imrna++;
	    }
	}
    fclose(inf2);
   
    if (imrna != mrnaCount)
	{
	fprintf(stderr, "total mrna count %d %d is different for %s\n", imrna, mrnaCount, proteinID);
	}
    calScores(proteinID, mrnaCount);
    proteinCount++;
    }    

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(o3);
fclose(o7);
return(0);
}

