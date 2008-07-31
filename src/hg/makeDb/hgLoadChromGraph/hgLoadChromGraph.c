/* hgLoadChromGraph - Load up chromosome graph. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "chromGraph.h"

static char const rcsid[] = "$Id: hgLoadChromGraph.c,v 1.7.50.1 2008/07/31 02:24:36 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadChromGraph - Load up chromosome graph.\n"
  "usage:\n"
  "   hgLoadChromGraph database track input.tab\n"
  "options:\n"
  "   -noLoad - don't load database\n"
  "   -idTable - Use a table on the same database to fetch coordinates\n"
  "              e.g. \"snp127\".\n" 
  "   -minusLog10 - log10 the data then minus it before loading.\n"
  "   -pathPrefix - Specify a location to use in metaChromGraph \n"
  "                 instead of /gbdb/<database>/chromGraph for the\n"
  "                 .cgb file.\n\n"
  "*NOTE: remember to put .cgb file in a good place for the web server\n"
  "       and make a link to the file in /gbdb/<database>/chromGraph.\n"
  );
}

static struct optionSpec options[] = {
   {"noLoad", OPTION_BOOLEAN},
   {"idTable", OPTION_STRING},
   {"minusLog10", OPTION_BOOLEAN},
   {"pathPrefix", OPTION_STRING},
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

void checkTableForFields(struct sqlConnection *conn, char *tableName)
/* Do basic checks on the table to make sure it's kosher. i.e. */
/* chrom, chromStart, name all exist along with the table itself. */
{
struct slName *fieldList;
if (!sqlTableExists(conn, tableName))
    errAbort("table %s not found.", tableName);
fieldList = sqlListFields(conn, tableName);
if (!slNameInList(fieldList, "chrom"))
    errAbort("table %s doesn't have a chrom field. It must have chrom, chromStart, and name at the minimum.", tableName);
if (!slNameInList(fieldList, "chromStart"))
    errAbort("table %s doesn't have a chrom field. It must have chrom, chromStart, and name at the minimum.", tableName);
if (!slNameInList(fieldList, "name"))
    errAbort("table %s doesn't have a chrom field. It must have chrom, chromStart, and name at the minimum.", tableName);
slNameFreeList(&fieldList);
}

struct hash *posHashFromTable(struct sqlConnection *conn, char *table)
/* Store name, chrom, chromStart in a hash of slPairs. (cheap) */
{
struct hash *posHash = newHash(24);
struct sqlResult *sr = NULL;
char query[256];
char **row;
safef(query, sizeof(query), "select name,chrom,chromStart from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int chromStart = (int)sqlUnsigned(row[2]);
    struct slPair *pair;
    AllocVar(pair);
    pair->name = cloneString(row[1]);
    pair->val = intToPt(chromStart);
    hashAdd(posHash, row[0], pair);
    }
sqlFreeResult(&sr);
return posHash;
}

struct chromGraph *chromGraphListWithTable(char *fileName, char *db, char *table)
/* Read the chromGraph file and convert to the  chr,chromStart format. */
{
struct sqlConnection *conn = sqlConnect(db);
struct hash *posHash = NULL;
struct lineFile *lf;
struct chromGraph *list = NULL;
char *words[2];
checkTableForFields(conn, table);
posHash = posHashFromTable(conn, table);
sqlDisconnect(&conn);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, words))
    {
    struct chromGraph *cg;
    /* Look up ID in hash. */
    struct slPair *infoFromHash = hashFindVal(posHash, words[0]);
    if (!infoFromHash)
	warn("%s line %d: %s not found in %s table", fileName, lf->lineIx, words[0], table);
    else
	{
	AllocVar(cg);
	cg->chrom = cloneString(infoFromHash->name);
	cg->chromStart = ptToInt(infoFromHash->val);
	cg->val = sqlDouble(words[1]);
	slAddHead(&list, cg);
	}   
    }
slReverse(&list);
/* Free stuff up. */
lineFileClose(&lf);
hashFreeWithVals(&posHash, slPairFree);
return list;
}

void hgLoadChromGraph(boolean doLoad, char *db, char *track, char *fileName)
/* hgLoadChromGraph - Load up chromosome graph. */
{
double minVal,maxVal;
struct chromGraph *el, *list;
FILE *f;
char *tempDir = ".";
char path[PATH_LEN], gbdbPath[PATH_LEN];
char *idTable = optionVal("idTable", NULL);
char *pathPrefix = NULL;

if (idTable == NULL)
    list = chromGraphLoadAll(fileName);
else 
    list = chromGraphListWithTable(fileName, db, idTable);
if (list == NULL)
    errAbort("%s is empty", fileName);

/* Figure out min/max values */
minVal = maxVal = list->val;
for (el = list->next; el != NULL; el = el->next)
    {
    if (optionExists("minusLog10"))
	{
	if (el->val == 1)
	    el->val = 0;
	else if (el->val > 0)
	    el->val = -1 * log(el->val)/log(10);
	}
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
    conn = hAllocConn(db);
    dyStringPrintf(dy, createString, track, hGetMinIndexLength(db));
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

    /* Make chrom graph file */
    safef(path, sizeof(path), "%s.cgb", track);
    chromGraphToBin(list, path);
    safef(path, sizeof(path), "/gbdb/%s/chromGraph", db);
    pathPrefix = optionVal("pathPrefix", path);
    safef(gbdbPath, sizeof(gbdbPath), "%s/%s.cgb", pathPrefix, track);

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
