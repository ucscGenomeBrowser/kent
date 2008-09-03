/*
 *	kgPrepBestRef2 - prepare input files for blat run to
 *	select best mRNA for each protein
 */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

extern int hRnaSeqAndIdx(char *acc, struct dnaSeq **retSeq, HGID *retId, char *gbdate, struct sqlConnection *conn);

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
char *db;
char proteinsDB[100];
char spDB[100];
char gbTempDB[100];
char *clusterDir = "./clusterRun";

/* Explain usage and exit. */
void usage()
    {
    errAbort(
  	"usage:\tkgPrepBestRef YYMMDD tempDb db\n"
  	"\tYYMMDD is the release date of SWISS-PROT data, eg: 031117\n"
	"\ttempDb is the temporary db for kg construction, eg: kgMm4ATemp\n"
  	"\tdb is the genome under construction, eg: mm4\n"
  	"kgPrepBestRef - read a list of proteins from protRef.lis and get\n"
	"\tcorresponding mRNAs from dbTemp.refRef to prepare input files\n"
	"\tfor blat run.  Creates files ./clusterRun/protN/[mp]NNNN.fa\n"
	"\tand result directories: ./out/protN/  (2000 results per directory)\n"
	"\tCreates files rawJobList and protRef.lis.");
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
char proteinID[20];

FILE *IN, *OUT;

char line[2000];
char mrnaDate[20];
int  imrna;
char dirName[1000];
char protName[1000];
char mrnaFile[1000];
char outName[1000];
char outDir[1000];
char blatCmd[1000];

HGID id;
bioSeq *mSeq;

struct sqlConnection *conn2, *conn3;
char query2[256];
struct sqlResult *sr2;
char **row2;

char *mrnaID;

FILE *aaOut, *mrnaOut;
char *aaSeq;
char cond_str[200];

if (argc != 4) usage();
    
proteinDataDate = argv[1];
kgTempDb = argv[2];
db = argv[3];


sprintf(spDB, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);
sprintf(gbTempDB, "%s", kgTempDb);
  
IN  = fopen("protRef.lis", "r"); 
OUT = fopen("rawJobList", "w");
    
conn2= hAllocConn(db);
conn3= hAllocConn(db);
   
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
    printf(">%s\n", proteinID);

    sprintf(cond_str, "acc='%s'", proteinID);
    aaSeq = sqlGetField(spDB, "protein","val", cond_str);

    if (aaSeq == NULL)
    	{
	aaSeq = sqlGetField(spDB, "varProtein","val", cond_str);
	}
			    
    if (aaSeq == NULL)
	{
	printf("no seq found for %s\n", proteinID);
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

    sprintf(query2,"select mrnaID from %s.spRef where spID='%s';",kgTempDb, proteinID);
	
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    imrna = 0;
    snprintf(mrnaFile, (size_t) sizeof(mrnaFile), \
	"%s/m%05d.fa", dirName, proteinCount );
    mrnaOut = NULL;
    while (row2 != NULL)
	{
 	mrnaID 	= row2[0];
	strcpy(mrnaNames[imrna], mrnaID);

	printf("%s\t%s\n", proteinID, mrnaID);fflush(stdout);
	sprintf(cond_str, "name='%s'", mrnaID);

        if (hRnaSeqAndIdx(mrnaID, &mSeq, &id, mrnaDate, conn3)== -1)
	        {
		printf("%s could not be found!!!\n", mrnaID);fflush(stdout);
		}
	else
	    {
    	    if (mrnaOut == NULL)mrnaOut = fopen(mrnaFile, "w");
	    fprintf(mrnaOut, ">%s\n%s\n", mrnaID, mSeq->dna);
	    }
	row2 = sqlNextRow(sr2);
	imrna++;
	}
    mrnaCount = imrna;
    if (mrnaOut != NULL)fclose(mrnaOut);
    sqlFreeResult(&sr2);
    
    snprintf(outName, (size_t) sizeof(outName),"./%s/b%05d.out",
	outDir, proteinCount);
    snprintf(blatCmd, (size_t) sizeof(blatCmd),
    "blat {check in exists %s/%s} {check in exists %s/%s} {check out line %s} -noHead -t=dnax -q=prot", \
	clusterDir, mrnaFile, clusterDir, protName, outName);
    if (mrnaOut != NULL) 
    	{
	fprintf(OUT, "%s\n", blatCmd);
    
    	proteinCount++;
	}
    }    

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(IN);
fclose(OUT);
return(0);
}

