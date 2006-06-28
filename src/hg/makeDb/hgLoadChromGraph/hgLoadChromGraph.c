/* hgLoadChromGraph - Load up chromosome graph. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "chromGraph.h"

static char const rcsid[] = "$Id: hgLoadChromGraph.c,v 1.4 2006/06/28 19:35:01 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadChromGraph - Load up chromosome graph.\n"
  "usage:\n"
  "   hgLoadChromGraph database track input.tab\n"
  "options:\n"
  "   -noLoad - don't load database\n"
  );
}

static struct optionSpec options[] = {
   {"noLoad", OPTION_BOOLEAN},
   {NULL, 0},
};

char *createString = 
"CREATE TABLE %s (\n"
"    chrom varchar(255) not null,       # Chromosome\n"
"    chromStart int not null,   # Start coordinate\n"
"    val double not null,        # Value at coordinate\n"
"              #Indices\n"
"    PRIMARY KEY(chrom(%d),chromStart)\n"
");\n";

char *metaCreateString = 
"CREATE TABLE metaChromGraph (\n"
"    name varchar(255) not null,        # Corresponds to chrom graph table name\n"
"    minVal double not null,    # Minimum value observed\n"
"    maxVal double not null,    # Maximum value observed\n"
"    binaryFile varchar(255) not null,  # Location of binary data point file if any\n"
"              #Indices\n"
"    PRIMARY KEY(name(32))\n"
");\n";



void hgLoadChromGraph(boolean doLoad, char *db, char *track, char *fileName)
/* hgLoadChromGraph - Load up chromosome graph. */
{
double minVal,maxVal;
struct chromGraph *el, *list = chromGraphLoadAll(fileName);
FILE *f;
char *tempDir = ".";
char path[PATH_LEN], gbdbPath[PATH_LEN];

if (list == NULL)
    errAbort("%s is empty", fileName);

/* Figure out min/max values */
minVal = maxVal = list->val;
for (el = list->next; el != NULL; el = el->next)
    {
    if (el->val < minVal)
        minVal = el->val;
    if (el->val > maxVal)
        maxVal = el->val;
    }


/* Sort and write out temp file. */
slSort(&list, chromGraphCmp);
f = hgCreateTabFile(tempDir, track);
for (el = list; el != NULL; el = el->next)
    chromGraphTabOut(el, f);

if (doLoad)
    {
    struct dyString *dy = dyStringNew(0);
    struct sqlConnection *conn;

    /* Set up connection to database and create main table. */
    hSetDb(db);
    conn = hAllocConn();
    dyStringPrintf(dy, createString, track, hGetMinIndexLength());
    sqlRemakeTable(conn, track, dy->string);

    /* Load main table and clean up file handle. */
    hgLoadTabFile(conn, tempDir, track, &f);
    hgRemoveTabFile(tempDir, track);

    /* If need be create meta table.  If need be delete old row. */
    if (!sqlTableExists(conn, "metaChromGraph"))
	sqlUpdate(conn, metaCreateString);
    else
        {
	dyStringClear(dy);
	dyStringPrintf(dy, "delete from metaChromGraph where name = '%s'", 
		track);
	sqlUpdate(conn, dy->string);
	}

    /* Make directory for track. */
    safef(path, sizeof(path), "/cluster/data/%s/bed/%s", db, track);
    makeDir(path);

    /* Make chrom graph file */
    safef(path, sizeof(path), "/cluster/data/%s/bed/%s/%s.cgb", 
    	db, track, track);
    chromGraphToBin(list, path);

    /* Link to gbdb */
    safef(path, sizeof(path), "/gbdb/%s", db);
    makeDir(path);
    safef(path, sizeof(path), "/gbdb/%s/chromGraph", db);
    makeDir(path);
    safef(gbdbPath, sizeof(gbdbPath), "%s/%s.cgb", path, track);
    remove(gbdbPath);
    dyStringClear(dy);
    dyStringPrintf(dy, "ln -s /cluster/data/%s/bed/%s/%s.cgb ",
    	db, track, track);
    dyStringPrintf(dy, "%s", gbdbPath);
    if (system(dy->string) != 0)
        errnoAbort("%s failed", dy->string);

    /* Create new line in meta table */
    dyStringClear(dy);
    dyStringPrintf(dy, "insert into metaChromGraph values('%s',%f,%f,'%s');",
    	track, minVal, maxVal, gbdbPath);
    sqlUpdate(conn, dy->string);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgLoadChromGraph(!optionExists("noLoad"), argv[1], argv[2], argv[3]);
return 0;
}
