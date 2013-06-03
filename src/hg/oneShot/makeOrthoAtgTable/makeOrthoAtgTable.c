/* makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.\n"
  "usage:\n"
  "   makeOrthoAtgTable totalOrfs species1.tab species2.tab ...\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void analyzeOne(int totalOrfs, char *fileName)
/* Do analysis on one tab-separated file. */
{
int nativeAtgCount = 0, nativeKozakCount = 0, nativeStopCount = 0;
int atgCount = 0, kozakCount = 0, stopCount=0, count = 0;
int atgAli = 0, kozakAli = 0, stopAli = 0;
double totalOrfSizeRatio = 0.0;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[18];
while (lineFileRow(lf, row))
    {
    boolean xenoAtg = (row[10][0] == '1');
    boolean xenoKozak = (row[11][0] == '1');
    boolean xenoStop = (row[12][0] == '1');
    boolean nativeAtg = (row[13][0] == '1');
    boolean nativeKozak = (row[14][0] == '1');
    boolean nativeStop = (row[15][0] == '1');
    int atgDotCount = lineFileNeedNum(lf, row, 4);
    int kozakDotCount = lineFileNeedNum(lf, row, 6);
    int stopDotCount = lineFileNeedNum(lf, row, 8);
    int possibleOrfSize = lineFileNeedNum(lf, row, 16);
    int orfSize = lineFileNeedNum(lf, row, 17);
    double orfSizeRatio = (double)orfSize/possibleOrfSize;
    totalOrfSizeRatio += orfSizeRatio;
    if (nativeAtg)
       {
       ++nativeAtgCount;
       if (atgDotCount == 0)
	   {
	   ++atgAli;
	   if (xenoAtg)
	       ++atgCount;
	   }
       }
    if (nativeKozak)
       {
       ++nativeKozakCount;
       if (kozakDotCount == 0 && atgDotCount == 0)
	   {
	   ++kozakAli;
	   if (xenoKozak)
	       ++kozakCount;
	   }
       }
    if (nativeStop)
       {
       ++nativeStopCount;
       if (stopDotCount == 0)
	   {
	   ++stopAli;
	   if (xenoStop)
	       ++stopCount;
	   }
       }
    ++count;
    }
char dbName[256];
splitPath(fileName, NULL, dbName, NULL);

struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof(query), "select organism from dbDb where name='%s'", dbName);
char *organism = sqlQuickString(conn, query);
sqlSafef(query, sizeof(query), "select scientificName from dbDb where name='%s'", dbName);
char *scientificName = sqlQuickString(conn, query);
hDisconnectCentral(&conn);

printf("%s\t%s\t%s\t", organism, scientificName, dbName);
printf("%4.2f%%\t", 100.0 * count/totalOrfs);
printf("%4.2f%%\t", 100.0 * atgAli/nativeAtgCount);
printf("%4.2f%%\t", 100.0 * atgCount/atgAli);
// printf("%4.2f%%\t", 100.0 * kozakAli/nativeKozakCount);
// printf("%4.2f%%\t", 100.0 * kozakCount/kozakAli);
printf("%4.2f%%\t", 100.0 * stopAli/nativeStopCount);
printf("%4.2f%%\t", 100.0 * stopCount/stopAli);
printf("%4.2f%%\n", 100.0 * totalOrfSizeRatio/count);
}

void makeOrthoAtgTable(int totalOrfs, int fileCount, char *files[])
/* makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.. */
{
int i;
printf("species\tbinomial name\tdb\t%%genes\t%%startAli\t%%startId\t%%stopAli\t%%stopId\t%%orfCover\n");
for (i=0; i<fileCount; ++i)
    analyzeOne(totalOrfs, files[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
makeOrthoAtgTable(atoi(argv[1]), argc-2, argv+2);
return 0;
}
