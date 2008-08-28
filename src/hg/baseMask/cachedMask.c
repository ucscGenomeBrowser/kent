/* cachedMask - cache and then process baseMasks - 'and' or 'or' two baseMasks together */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include "hdb.h"
#include "genomeRangeTree.h"
#include "options.h"

//#define MJP verbose(1,"%s[%3d]: ", __func__, __LINE__);

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {"and", OPTION_BOOLEAN},
    {"or", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {"saveMem", OPTION_BOOLEAN},
    {"orDirectToFile", OPTION_BOOLEAN},
    {"logTimes", OPTION_BOOLEAN},
    {NULL, 0}
};


void trackToBaseMask(char *db, char *track, char *obama, boolean quiet);
char *chromTable(struct sqlConnection *conn, char *table);
/* chromTable copied verbatim from hgTables.c, could be libraryised */


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "cachedMaskUsage.msg" 
    ;
errAbort("%s\n%s", msg, usageMsg);
}

void splitDbTable(char *chromDb, char *track, char **pDb, char **pTable)
/* split the track into db name and table name. 
 * If no db specified then this will be chromDb.
 * Cannabalizes track. */
{
char *words[2];
int n = chopByChar(track, '.', words, 2);
if (n==0)
    errAbort("error: null track name");
else if (n==1)
    {
    *pDb = chromDb;
    *pTable = words[0];
    }
else if (n==2)
    {
    *pDb = words[0];
    *pTable = words[1];
    }
else
    errAbort("invalid track name %s (must be 'table' or 'db.table')\n", track);
}

char *cacheTrack(char *cacheDir, char *chromDb, char *db, char *tableName, boolean quiet, boolean logUpdateTimes)
/* Return the file name of the cached baseMask for the specified table and db.
 * The chromInfo table is read from chromDb database.
 * The cached file will be saved in the file cacheDir/db/table.bama 
 * If logUpdateTimes is true then the table and cache update times will be
 * appended to cacheDir/db/table.bama.log
 * The bama file is written as a temp file then moved to the final name as 
 * an atomic operation. 
 * Return value must be freed with freeMem(). */
{
char *file, *logFile=NULL, *timeString = NULL;
int n, nDir, n0;
/* get either tableName or chr1_tableName */
struct sqlConnection *conn = hAllocOrConnect(chromDb);
char *chrTable = chromTable(conn, tableName); 
sqlDisconnect(&conn);
/* connect to database and get last update time for table */
conn = hAllocOrConnect(db);
if (sqlTableExists(conn, chrTable))
    timeString = sqlTableUpdate(conn, chrTable); /* format %4d-%2d-%2d %2d:%2d:%2d */
else
    errAbort("cant find table %s or %s in %s database (using chromosomes from %s)\n", tableName, chrTable, db, chromDb);
sqlDisconnect(&conn);
int timeInt = sqlDateToUnixTime(timeString);
/* generate bama file name and cache table if necessary */
nDir = strlen(cacheDir)+1+strlen(db)+1; /* directory */
n = nDir+strlen(tableName)+strlen(".bama")+1;
AllocArray(file, n); 
if (!fileExists(cacheDir)) /* if cache dir does not exist, abort */
    errAbort("cache directory [%s] does not exist (chromDb=%s,db=%s,table=%s)\n", cacheDir, chromDb, db, tableName);
n0 = safef(file, n, "%s/%s/",cacheDir,db);
if (nDir != n0)
    errAbort("error: safef did not write all bytes (wrote %d, expected %d) [%s]", n0, nDir, file);
if (!fileExists(file)) /* if cacheDir/db does not exist, make it */
    makeDirs(file);
safecat(file, n, tableName);
safecat(file, n, ".bama");
/* if file exists and is older than the table, no need to update it */
struct stat statBuf;
statBuf.st_mtime = -1;
if (fileExists(file))
    {
    if (stat(file,&statBuf)==-1)
	errnoAbort("could not stat file %s\n", file);
    if (statBuf.st_mtime > timeInt)
	{
	freeMem(timeString);
	return file;
	}
    }
/* otherwise create a new temp bama file and the rename it to the real one */
char tmpFile[512];
safef(tmpFile, sizeof(tmpFile), "%s/%s/%sXXXXXX", cacheDir, db, tableName);
int fd = mkstemp(tmpFile); 
if (fd==-1)
    errAbort("could not create temp file %s\n", tmpFile);
if (close(fd) != 0)
    errnoAbort("could not close temp file %s\n", tmpFile);
trackToBaseMask(db, tableName, tmpFile, TRUE);
if (rename(tmpFile, file))
    errnoAbort("could not rename temp file %s -> %s\n", tmpFile, file);
if (logUpdateTimes)
    {
    char buf[512], newTimeBuf[80], oldTimeBuf[80];
    time_t now = time(NULL);
    struct tm  *ts = localtime(&now);
    if (!strftime(newTimeBuf, sizeof(newTimeBuf), "%Y-%m-%d %H:%M:%S", ts))
	errAbort("error in strftime newTimeBuf\n");
    if (statBuf.st_mtime < 0)
	oldTimeBuf[0]=0;
    else
	{
	struct tm *ts = localtime(&statBuf.st_mtime);
	if (!strftime(oldTimeBuf, sizeof(oldTimeBuf), "%Y-%m-%d %H:%M:%S", ts))
	    errAbort("error in strftime oldTimeBuf\n");
	}
    safef(buf, sizeof(buf), "new_cache=[%s] tableUpdateTime=[%s] old_cache=[%s]\n", newTimeBuf, timeString, oldTimeBuf);
    AllocArray(logFile, n+4);
    safecat(logFile, n+4, file);
    safecat(logFile, n+4, ".log");
    FILE *f = mustOpen(logFile, "a");
    mustWrite(f, buf, strlen(buf));
    carefulClose(&f);
    freeMem(logFile);
    }
freeMem(timeString);
return file;
}


void genomeRangeTreeStats(char *fileName, int *numChroms, int *nodes, int *size)
{
struct genomeRangeTreeFile *tf = genomeRangeTreeFileReadHeader(fileName);
struct hashEl *c;
int n;
*size = 0;
*nodes = 0;
*numChroms = tf->numChroms;
for (c = tf->chromList ; c ; c = c->next)
    {
    struct rangeStartSize *r;
    n = hashIntVal(tf->nodes, c->name);
    *nodes += n;
    AllocArray(r, n);
    rangeReadArray(tf->file, r, n, tf->isSwapped);
    *size += rangeArraySize(r, n);
    freez(&r);
    }
genomeRangeTreeFileFree(&tf);
}

/* entry */
int main(int argc, char** argv)
{
char *cacheDir, *chromDb, *track1, *track2, *db1, *table1, *db2, *table2, *baseMask1, *baseMask2, *obama;
struct genomeRangeTreeFile *tf1, *tf2;
struct genomeRangeTree *t1, *t2;
unsigned size = 0;
int nodes, numChroms;
optionInit(&argc, argv, optionSpecs);
boolean and = optionExists("and");
boolean or = optionExists("or");
boolean quiet = optionExists("quiet");
boolean saveMem = optionExists("saveMem");
boolean logTimes = optionExists("logTimes");
boolean orDirectToFile = optionExists("orDirectToFile");
--argc;
++argv;
if (argc == 0)
    usage("");
if (argc < 3 || argc > 5)
    usage("wrong # args\n");
if (argc == 3 && (and || or))
    usage("specify second file for options: -and or -or\n");
if (argc >= 4 && ((and && or) || (!and && !or)))
    usage("specify only one of the options: -and or -or\n");

cacheDir = argv[0];
chromDb = argv[1];
track1 = argv[2];
track2 = (argc > 3 ? argv[3] : NULL);
obama = (argc > 4 ? argv[4] : NULL);

if (!strlen(cacheDir))
    errAbort("specify cache directory\n");
if (argc == 3)
    {
    /* cache the track if it is not there already */
    splitDbTable(chromDb, track1, &db1, &table1);
    baseMask1 = cacheTrack(cacheDir, chromDb, db1, table1, TRUE, logTimes);
    if (!quiet)
	{
	genomeRangeTreeStats(baseMask1, &numChroms, &nodes, &size);
	fprintf(stderr, "%d bases in %d ranges in %d chroms in baseMask\n", size, nodes, numChroms);
	}
    }
else
    {
    /* cache the tracks if they are not there already */
    splitDbTable(chromDb, track1, &db1, &table1);
    splitDbTable(chromDb, track2, &db2, &table2);
    baseMask1 = cacheTrack(cacheDir, chromDb, db1, table1, TRUE, logTimes);
    baseMask2 = cacheTrack(cacheDir, chromDb, db2, table2, TRUE, logTimes);
    tf1 = genomeRangeTreeFileReadHeader(baseMask1);
    tf2 = genomeRangeTreeFileReadHeader(baseMask2);
    if (and)
	{
	genomeRangeTreeFileIntersectionDetailed(tf1, tf2, obama, &numChroms, &nodes, 
	    (quiet ? NULL : &size), saveMem);
	if (!quiet)
	    fprintf(stderr, "%d bases in %d ranges in %d chroms in intersection\n", size, nodes, numChroms);
	}
    else if (or)
	{
	genomeRangeTreeFileUnionDetailed(tf1, tf2, obama, &numChroms, &nodes, 
	    (quiet ? NULL : &size), saveMem, orDirectToFile);
	if (!quiet)
	    fprintf(stderr, "%d bases in %d ranges in %d chroms in union\n", size, nodes, numChroms);
	}
    t1 = genomeRangeTreeFileFree(&tf1);
    genomeRangeTreeFree(&t1);
    t2 = genomeRangeTreeFileFree(&tf2);
    genomeRangeTreeFree(&t2);
    }
return 0;
}

