/* cachedMask - cache and then process baseMasks - 'and' or 'or' two baseMasks together */

#include "common.h"
#include "hdb.h"
#include "genomeRangeTree.h"
#include "options.h"

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
errAbort("%s:  %s", msg, usageMsg);
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

char *cacheTrack(char *cacheDir, char *chromDb, char *db, char *tableName, boolean quiet)
/* Return the file name of the cached baseMask for the specified table and db.
 * The chromInfo table is read from chromDb database.
 * The cached file will be saved in the directory cacheDir/db.
 * The file name will be table.{TableUpdateTime}.bama where {TableUpdateTime} 
 * is the time the table was last updated as reported by mysql in the format
 * "YYYY-MM-DD_HH-MM-SS".
 * Old cached tables are not deleted.
 * The bama file is written as a temp file then moved to the final name as 
 * an atomic operation. 
 * Return value must be freed with freez(). */
{
char *file, *t = NULL;
int n, nDir, n0;
/* get either tableName or chr1_tableName */
struct sqlConnection *conn = hAllocOrConnect(chromDb);
char *table = chromTable(conn, tableName); 
sqlDisconnect(&conn);
/* connect to database and get last update time for table */
conn = hAllocOrConnect(db);
if (sqlTableExists(conn, table))
    t = sqlTableUpdate(conn, table); /* format %4d-%2d-%2d %2d:%2d:%2d */
else
    errAbort("cant find table %s in %s database (using chromosomes from %s)\n", table, db, chromDb);
sqlDisconnect(&conn);
/* generate bama file name and cache table if necessary */
subChar(t, ' ', '_'); /* remove ugly chars from time as it will form file name */
subChar(t, ':', '-');
nDir = strlen(cacheDir)+1+strlen(db)+1; /* directory */
n = nDir+strlen(table)+1+strlen(t)+strlen(".bama")+1;
AllocArray(file, n);
if (!fileExists(cacheDir)) /* if cache dir does not exist, abort */
    errAbort("cache directory [%s] does not exist\n", cacheDir);
n0 = safef(file, n, "%s/%s/",cacheDir,db);
if (nDir != n0)
    errAbort("error: safef did not write all bytes (wrote %d, expected %d) [%s]", n0, nDir, file);
if (!fileExists(file)) /* if cacheDir/db does not exist, make it */
    {
    makeDirs(file);
    }
safecat(file, n, table);
safecat(file, n, ".");
safecat(file, n, t);
safecat(file, n, ".bama");
/* if file does not exist, cache it */
if (!fileExists(file))
    {
    trackToBaseMask(db, table, file, quiet);
    }
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
boolean orDirectToFile = optionExists("orDirectToFile");
--argc;
++argv;
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
    baseMask1 = cacheTrack(cacheDir, chromDb, db1, table1, TRUE);
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
    baseMask1 = cacheTrack(cacheDir, chromDb, db1, table1, TRUE);
    baseMask2 = cacheTrack(cacheDir, chromDb, db2, table2, TRUE);
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

