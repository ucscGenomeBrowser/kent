/* makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"

static char const rcsid[] = "$Id: makeOrthoAtgTable.c,v 1.1 2007/03/22 06:52:23 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.\n"
  "usage:\n"
  "   makeOrthoAtgTable totalOrfs XXX\n"
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
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[14];
while (lineFileRow(lf, row))
    {
    boolean xenoAtg = (row[8][0] == '1');
    boolean xenoKozak = (row[9][0] == '1');
    boolean xenoStop = (row[10][0] == '1');
    boolean nativeAtg = (row[11][0] == '1');
    boolean nativeKozak = (row[12][0] == '1');
    boolean nativeStop = (row[13][0] == '1');
    if (nativeAtg)
       {
       ++nativeAtgCount;
       if (xenoAtg)
           ++atgCount;
       }
    if (nativeKozak)
       {
       ++nativeKozakCount;
       if (xenoKozak)
           ++kozakCount;
       }
    if (nativeStop)
       {
       ++nativeStopCount;
       if (xenoStop)
           ++stopCount;
       }
    ++count;
    }
char *dbName = cloneString(fileName);
chopSuffix(dbName);


struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select organism from dbDb where name='%s'", dbName);
char *organism = sqlQuickString(conn, query);
safef(query, sizeof(query), "select scientificName from dbDb where name='%s'", dbName);
char *scientificName = sqlQuickString(conn, query);
hDisconnectCentral(&conn);

printf("%s\t%s\t%s\t", organism, scientificName, dbName);
printf("%4.2f%%\t", 100.0 * count/totalOrfs);
printf("%4.2f%%\t", 100.0 * atgCount/nativeAtgCount);
printf("%4.2f%%\t", 100.0 * kozakCount/nativeKozakCount);
printf("%4.2f%%\n", 100.0 * stopCount/nativeStopCount);
}

void makeOrthoAtgTable(int totalOrfs, int fileCount, char *files[])
/* makeOrthoAtgTable - Create a table that lists how often atg, kozak, stop is conserved based on output from txCdsOrtho.. */
{
int i;
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
