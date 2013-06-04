/*
 *	kgPrepBestMrna - prepare input files for blat run to
 *	select best mRNA for each protein
 */
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

char *proteinDataDate;
char *kgTempDb;
char *genomeDBname;
char proteinsDB[100];
char spDB[100];
char gbTempDB[100];
char *clusterDir = "./clusterRun";

/* Explain usage and exit. */
void usage()
    {
    errAbort(
  	"usage:\tkgPrepBestMrna YYMMDD db 2> jobList\n"
  	"\tYYMMDD is the release date of SWISS-PROT data, eg: 031117\n"
  	"\tdb is the genome under construction, eg: mm4\n"
  	"kgPrepBestMrna - read a list of proteins from protein.lis and get\n"
	"\tcorresponding mRNAs from dbTemp.refMrna to prepare input files\n"
	"\tfor blat run.  Creates files ./clusterRun/protN/[mp]NNNN.fa\n"
	"\tand result directories: ./out/protN/  (2000 results per directory)\n"
	"\tOutputs the jobList for 'para create jobList' to stderr");
    }

int cal_months(char *date)
    {
    int year, month, day;
    int months;
	
    sscanf(date, "%d-%d-%d", &year, &month, &day);
    months = (year - 1970)*12 + month - 1;
    return(months);
    }

int main(int argc, char *argv[])
{
int  ixm, maxixm;       // index for mRNA
int  maxScore;
char proteinID[20], mrnaName[20];

int  monthss[500];
int  mrnalens[500];
FILE *IN, *OUT;

int  newMrna;
int  ii= -1;
char line[2000];
char mrnaDate[20];
int  months;
int  diff;
int  mrnalen;
int  imrna;
char dirName[1000];
char protName[1000];
char mrnaFile[1000];
char outName[1000];
char outDir[1000];
char blatCmd[1000];

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
  
char *genomeDb;
char *chp0, *chp;

int totalCount, matchCount;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd, *exonCount, *exonStarts, *exonEnds;

int  i;
char *mrnaID;

FILE *aaOut, *mrnaOut;
char *aaSeq, *mrnaSeq;
char cond_str[200];

if (argc != 3) usage();
    
proteinDataDate = argv[1];
//genomeDb = argv[2];
kgTempDb = argv[2];

// make sure db connection goes to correct genome
// hRnaSeqAndIdx() needs this.
hSetDb(kgTempDb);

sprintf(spDB, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);
sprintf(gbTempDB, "%s", kgTempDb);
  
IN  = fopen("protein.lis", "r"); 
OUT = fopen("rawJobList", "w");
    
conn2= hAllocConn();
conn3= hAllocConn();
   
proteinCount = 0; 
snprintf(dirName, (size_t) sizeof(dirName), "./out" );
mkdir(dirName, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
	S_IROTH | S_IWOTH | S_IXOTH );
snprintf(dirName, (size_t) sizeof(dirName), "%s", clusterDir );
mkdir(dirName, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
	S_IROTH | S_IWOTH | S_IXOTH );
if (chdir(dirName))
    errAbort("ERROR: Can not change dir to %s\n", dirName);

while (fgets(line, 1000, IN) != NULL)
    {
    sscanf(line, "%s", proteinID);
    printf(">>%s\n", proteinID);

    sprintf(cond_str, "acc='%s'", proteinID);
    aaSeq = sqlGetField(conn3, spDB, "protein","val", cond_str);
    
    if (aaSeq == NULL)
	{
    	aaSeq = sqlGetField(conn3, spDB, "varProtein","val", cond_str);
        }
	
    if (aaSeq == NULL)
	{
	printf("no seq found for %s in %s.\n", proteinID, spDB);
	fflush(stdout);
	exit(1);
	}	
	
    /*	2000 results per output directory.  4000 input files for each
     *	./protN directory
     */
    if ( 0 == (proteinCount % 2000) )
	{
	snprintf(dirName, (size_t)sizeof(dirName), "../out/prot%05d", proteinCount);
	if (mkdir(dirName, S_IRUSR | S_IWUSR | S_IXUSR |
	    S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH ))
	    {
	    if (errno != EEXIST)
		errAbort("ERROR: Can not create dir %s\n", dirName);
	    }
	snprintf(dirName, (size_t) sizeof(dirName), "prot%05d", proteinCount );
	if (mkdir(dirName, S_IRUSR | S_IWUSR | S_IXUSR |
	    S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH ))
	    {
	    if (errno != EEXIST)
		errAbort("ERROR: Can not create dir %s\n", dirName);
	    }
	snprintf(outDir, (size_t) sizeof(outDir), "out/prot%05d", proteinCount);
	}
    snprintf(protName, (size_t) sizeof(protName), \
	"%s/p%05d.fa", dirName, proteinCount );
    aaOut = fopen(protName, "w");
    fprintf(aaOut, ">%s\n%s\n", proteinID, aaSeq);
    fclose(aaOut);

    sqlSafef(query2, sizeof query2, "select mrnaID from %s.spMrna where spID='%s';",kgTempDb, proteinID);
	
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    imrna = 0;
    snprintf(mrnaFile, (size_t) sizeof(mrnaFile), \
	"%s/m%05d.fa", dirName, proteinCount );
    mrnaOut = fopen(mrnaFile, "w");
    while (row2 != NULL)
	{
 	mrnaID 	= row2[0];
	strcpy(mrnaNames[imrna], mrnaID);

	printf("%s\t%s\n", proteinID, mrnaID);fflush(stdout);
	sprintf(cond_str, "name='%s'", mrnaID);
    	mrnaSeq = sqlGetField(conn3,gbTempDB,"mrnaSeq","seq", cond_str);
	fprintf(mrnaOut, ">%s\n%s\n", mrnaID, mrnaSeq);
	row2 = sqlNextRow(sr2);
	imrna++;
	}
    mrnaCount = imrna;
    fclose(mrnaOut);
    sqlFreeResult(&sr2);
    
    snprintf(outName, (size_t) sizeof(outName),"./%s/b%05d.out",
	outDir, proteinCount);
    snprintf(blatCmd, (size_t) sizeof(blatCmd),
    "blat {check in exists %s/%s} {check in exists %s/%s} {check out line %s} -noHead -t=dnax -q=prot", \
	clusterDir, mrnaFile, clusterDir, protName, outName);
    fprintf(OUT, "%s\n", blatCmd);
    
    proteinCount++;
    }    

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(IN);
fclose(OUT);
return(0);
}

