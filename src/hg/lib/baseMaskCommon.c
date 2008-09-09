#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "genomeRangeTree.h"
#include "genomeRangeTreeFile.h"
#include "baseMaskCommon.h"

//#define MJP verbose(1,"%s[%3d]: ", __func__, __LINE__);

static struct chromInfo *createChromInfoList(char *name, char *database)
/* Load up all chromosome information. 
 * Similar to featureBits.c - maybe could be moved to library ? */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int loaded=0;
struct chromInfo *ret = NULL;
unsigned totalSize = 0;
/* do the query */
if (sameWord(name, "all"))
    sr = sqlGetResult(conn, "select * from chromInfo");
else
    {
    char select[256];
    safef(select, ArraySize(select), "select * from chromInfo where chrom='%s'",
        name);
    sr = sqlGetResult(conn, select);
    }
/* read the rows and build the chromInfo list */
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *el;
    struct chromInfo *ci;
    AllocVar(ci);
    el = chromInfoLoad(row);
    ci->chrom = cloneString(el->chrom);
    ci->size = el->size;
    totalSize += el->size;
    slAddHead(&ret, ci);
    ++loaded;
    }
if (loaded < 1)
    errAbort("ERROR: can not find chrom name: '%s'\n", name);
slReverse(&ret);
if (sameWord(name, "all"))
    verbose(2, "#\tloaded size info for %d chroms, total size: %u\n",
        loaded, totalSize);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

static char *chromTable(char *db, char *table, char *chromDb)
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */
{
char *realTable;
struct sqlConnection *sc = hAllocConn(chromDb);
char *chrom = hDefaultChrom(chromDb);
hFreeConn(&sc);
sc = hAllocConn(db);
if (sqlTableExists(sc, table))
    {
    realTable = cloneString(table);
    }
else
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s_%s", chrom, table);
    realTable = cloneString(buf);
    }
hFreeConn(&sc);
return realTable;
}

void splitDbTable(char *chromDb, char *track, char **pDb, char **pTable)
/* split the track into db name and table name. 
 * If no db specified then this will be chromDb.
 * Cannabalizes track. 
 * *pDb points a table in *track. *pTable points at database in *track or *chromDb. */
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

static time_t fileUpdateTime(char *file)
/* Return the file modification time or -1 if file does not exist */
{
struct stat statBuf;
statBuf.st_mtime = -1;
if (fileExists(file))
    if (stat(file,&statBuf)==-1)
        errnoAbort("could not stat file %s\n", file);
return statBuf.st_mtime;
}

static time_t chromTableUpdateTime(char *db, char *table, char *chromDb)
/* Find the last update time for table in db.
 * If the table doesnt exist, find a chromosome or scaffold) name from chromDb and 
 * check if the table is split by chromosome.
 * Errors out if time is not a positive integer.
 * Returns the table update time. */
{
/* get either table or chr1_table (or scaffold123_table)*/
char *realTable = chromTable(db, table, chromDb);
time_t time = 0;
/* connect to database and get last update time for table if it exists */
struct sqlConnection *sc = hAllocConn(db);
if (sqlTableExists(sc, realTable))
    time = sqlTableUpdateTime(sc, realTable);
else
    errAbort("cant find table %s or %s in %s database (using chromosomes from %s)\n", table, realTable, db, chromDb);
if (time <= 0)
    errAbort("invalid table update time (%d)\n", (int)time);
freeMem(realTable);
hFreeConn(&sc);
return time;
}

static char *cacheFile(char *cacheDir, char *db, char *table, char *suffix)
/* Create cache file name as {cacheDir}/{db}/{table}{suffix}
 * Errors out if directory cacheDir does not exist.
 * Creates directory cacheDir/db/ if this does not exist.
 * Return cache file name. Return value needs to be freeMem'd */
{
char *file;
/* generate bama file name and cache directories if necessary */
int n = strlen(cacheDir)+1+strlen(db)+1+strlen(table)+strlen(suffix)+1;
if (!fileExists(cacheDir)) /* if cache dir does not exist, abort */
    errAbort("cache directory [%s] does not exist \n", cacheDir);
AllocArray(file, n);
safef(file, n, "%s/%s/",cacheDir,db);
if (!fileExists(file)) /* if cacheDir/db/ does not exist, make it */
    makeDirs(file);
safecat(file, n, table);
safecat(file, n, suffix);
return file;
}

static void makeTempFile(char *file)
/* Make a temp file using the template file name.
 * File name should contain XXXXXX as the suffix.
 * An empty temporary file is created and the input file name
 * is modified to contain the name of the temp file created.
 * The temp file is closed and safe to be overwritten.
 * Errors out if the temp file could not be created. */
{
int fd = mkstemp(file);
if (fd==-1)
    errAbort("could not create temp file %s\n", file);
if (close(fd) != 0)
    errnoAbort("could not close temp file %s\n", file);
}

static int formatTime(char *buf, int bufSize, char *prefix, time_t time, char *suffix)
/* Format time as YYYY-MM-DD HH:MM:SS into buffer 'buf' 
 * of size 'bufSize'.
 * Prepends 'prefix' and appends 'suffix' to the formatted time. 
 * Returns the number of bytes written to buf. */
{
    struct tm  *ts = localtime(&time);
    char timeBuf[80];
    if (!strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", ts))
        errAbort("error formatting time in strftime \n");
    return safef(buf, bufSize, "%s%s%s", prefix, timeBuf, suffix);
}

static void writeCacheLog(char *file, char *db, char *table, time_t newCacheTime, time_t tableTime, time_t oldCacheTime)
/* Append db.table and the new cache time, table update time, and
 * old cache time (if any) to the log file.
 * Closes the file. */
{
char buf[1024], timeBuf[1024];
safef(buf, sizeof(buf), "%s.%s: ", db, table);
formatTime(timeBuf, sizeof(timeBuf), "new_cache=[", newCacheTime, "] ");
safecat(buf, sizeof(buf), timeBuf);
formatTime(timeBuf, sizeof(timeBuf), "tableUpdateTime=[", tableTime, "] ");
safecat(buf, sizeof(buf), timeBuf);
if (oldCacheTime<=0)
    safef(timeBuf, sizeof(timeBuf), "old_cache=[]\n");
else
    formatTime(timeBuf, sizeof(timeBuf), "old_cache=[", oldCacheTime, "]\n");
safecat(buf, sizeof(buf), timeBuf);
FILE *f = mustOpen(file, "a");
mustWrite(f, buf, strlen(buf));
carefulClose(&f);
}

void trackToBaseMask(char *db, char *track, char *chromDb, char *obama, boolean quiet)
/* Create a baseMask file obama representing the 'track' in database 'db'.
 * If quiet = false, outputs number of based on overlap of chromosome ranges. */
{
struct bed *b, *bList;
struct genomeRangeTree *tree = genomeRangeTreeNew();
//struct slName *chrom, *chromList = hAllChromNamesDb(db);
struct chromInfo *ci, *ciList = createChromInfoList("all", chromDb);
struct sqlConnection *conn = hAllocConn(db);
struct rbTree *rt;
double totalSize = 0, treeSize = 0, nodes = 0;
/* loop over all chromosomes adding to range tree */
for (ci = ciList ; ci ; ci = ci->next)
    {
    totalSize += ci->size;
    rt = NULL;
    if ( (bList = hGetBedRange(db, track, ci->chrom, 0, 0, NULL)) )
	{
	rt = genomeRangeTreeFindOrAddRangeTree(tree, ci->chrom);
        for ( b = bList ; b ; b = b->next )
	    {
	    bedIntoRangeTree(b, rt);
	    }
	nodes += rt->n;
        bedFreeList(&bList);
	}
    if (!quiet && rt)
	treeSize += rangeTreeOverlapTotalSize(rt);
    }
if (!quiet)
    fprintf(stderr,"%1.0f bases in %1.0f ranges in %d chromosomes of %1.0f (%4.3f%%) bases in genome\n",
	treeSize, nodes, tree->hash->elCount, totalSize, 100.0*treeSize/totalSize);
if (obama)
    genomeRangeTreeWrite(tree, obama); /* write the tree out */
genomeRangeTreeFree(&tree);
slFreeList(&ciList);
hFreeConn(&conn);
}

char *baseMaskCacheTrack(char *cacheDir, char *chromDb, char *db, char *table, boolean quiet, boolean logUpdateTimes)
/* Return the file name of the cached baseMask for the specified table and db.
 * The chromInfo table is read from chromDb database.
 * The cached file will be saved in the file cacheDir/db/table.bama 
 * If logUpdateTimes is true then the table and cache update times will be
 * appended to cacheDir/db/table.bama.log
 * The bama file is written as a temp file then moved to the final name as 
 * an atomic operation. 
 * Return value must be freed with freeMem(). */
{
char *file = cacheFile(cacheDir, db, table, ".bama");
time_t cacheTime = fileUpdateTime(file);
time_t tableTime = chromTableUpdateTime(db, table, chromDb);
if (cacheTime > tableTime)
    return file; /* cache is up to date */
/* otherwise create a new temp bama file and the rename it to the real one */
char *tmpFile = cacheFile(cacheDir, db, table, ".XXXXXX");
makeTempFile(tmpFile);
trackToBaseMask(db, table, chromDb, tmpFile, TRUE);
if (rename(tmpFile, file))
    errnoAbort("could not rename temp file %s -> %s\n", tmpFile, file);
freeMem(tmpFile);
if (logUpdateTimes)
    {
    char *logFile=cacheFile(cacheDir, db, table, ".bama.log");
    time_t now = time(NULL);
    writeCacheLog(logFile, db, table, now, tableTime, cacheTime);
    freeMem(logFile);
    }
return file;
}

