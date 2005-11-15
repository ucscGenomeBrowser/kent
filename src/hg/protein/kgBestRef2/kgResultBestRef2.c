/* kgResultBestRef2 - program to select best mRNA for each protein */

#include <sys/param.h>
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

char proteinName[20], mrnaName[20];
char mrnaNames[500][20];
int  mrnaScore[500];
char proteinNameOld[20] = {""};

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
char *kgTempDb;
char *genomeReadOnly;
char *genomeDBname;
char proteinsDB[100];
char spDB[100];
char gbTempDB[100];

/* Explain usage and exit. */
void usage()
    {
    errAbort(
  	"usage:\tkgResultBestRef YYMMDD db ro_db> BestResult.out\n"
  	"\tYYMMDD is the release date of SWISS-PROT data, eg: 031117\n"
  	"\tdb is the genome under construction, eg: kgDB\n"
  	"\tro_db is the actual target genome, e.g.: mm7\n"
  	"\tprotRefTableName is the table name for protMrna alignment table, e.g.: protMrnaBlast\n"
	);
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
FILE *inf, *inf2, *outf;

int  newRef;
int  ii= -1;
char line[2000];
char *mrnaDate;
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

char *protAcc, *mrnaAcc, *matchStr;
char *protSizeStr, *mrnaSizeStr;
int  protSize, mrnaSize, match;
char *protRefTableName;

char *chp0, *chp;

int totalCount, matchCount;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd, *exonCount, *exonStarts, *exonEnds;

int  i;
char *mrnaID;

FILE *aaOut, *mrnaOut;
char *aaSeq, *mrnaSeq;
char condStr[255];
int score;

if (argc != 5) usage();
    
proteinDataDate = argv[1];
kgTempDb = argv[2];
genomeReadOnly = argv[3];
protRefTableName = argv[4];

// make sure db connection goes to correct genome
// hRnaSeqAndIdx() needs this.
hSetDb(genomeReadOnly);

sprintf(spDB, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);
sprintf(gbTempDB, "%sTemp", kgTempDb);
  
inf = fopen("protRef.lis", "r"); 
if ((FILE *) NULL == inf)
    errAbort("ERROR: Can not open input file: protRef.lis");
o3  = fopen("kgBestRef.out",   "w");
if ((FILE *) NULL == o3)
    errAbort("ERROR: Can not open output file: kgBestRef.out");

conn = hAllocConn();
conn2= hAllocConn();
conn3= hAllocConn();
   
proteinCount = 0; 
snprintf(dirName, (size_t) sizeof(dirName), "%s", "./clusterRun" );

sprintf(query,"select qName, tName, matches, qSize, tSize from %s.%s", kgTempDb, protRefTableName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
while (row != NULL)
    {
    protAcc 	= row[0];
    mrnaAcc 	= row[1];
    matchStr    = row[2];
    protSizeStr = row[3];
    mrnaSizeStr = row[4];

    sscanf(matchStr, "%d", &match);
    sscanf(protSizeStr, "%d", &protSize);
    sscanf(mrnaSizeStr, "%d", &mrnaSize);
    sscanf(matchStr, "%d", &match);
  
    if ((float)match/(float)protSize > 0.3)
    	{
        sprintf(condStr, "acc='%s'", mrnaAcc);
        mrnaDate = sqlGetField(conn3, genomeReadOnly, "gbCdnaInfo", "moddate", condStr);
	if (mrnaDate != NULL)
	{
        months = cal_months(mrnaDate);
		
        score  = mrnaSize + months*2 - (protSize - match) *50;
    
        printf("%s\t%s\t%d\n", protAcc, mrnaAcc, score);fflush(stdout);
        }
	}
    row = sqlNextRow(sr);
    }    

hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(o3);
return(0);
}

