/* kgResultBestRef2 - program to select best mRNA for each protein */

#include <sys/param.h>
#include <limits.h>
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
FILE *inf;

char *mrnaDate;
int  months;
char dirName[PATH_MAX];

struct sqlConnection *conn, *conn3;
char query[256];
struct sqlResult *sr;
char **row;

char *protAcc, *mrnaAcc, *matchStr;
char *protSizeStr, *mrnaSizeStr;
int  protSize, mrnaSize, match;
char *protRefTableName;

char condStr[255];
int score;

if (argc != 5) usage();
    
proteinDataDate = argv[1];
kgTempDb = argv[2];
genomeReadOnly = argv[3];
protRefTableName = argv[4];

sprintf(spDB, "sp%s", proteinDataDate);
sprintf(proteinsDB, "proteins%s", proteinDataDate);
sprintf(gbTempDB, "%sTemp", kgTempDb);
  
inf = fopen("protRef.lis", "r"); 
if ((FILE *) NULL == inf)
    errAbort("ERROR: Can not open input file: protRef.lis");
o3  = fopen("kgBestRef.out",   "w");
if ((FILE *) NULL == o3)
    errAbort("ERROR: Can not open output file: kgBestRef.out");

conn = hAllocConn(genomeReadOnly);
conn3= hAllocConn(genomeReadOnly);
   
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
        mrnaDate = sqlGetField(genomeReadOnly, "gbCdnaInfo", "moddate", condStr);
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
hFreeConn(&conn3);
fclose(o3);
return(0);
}

