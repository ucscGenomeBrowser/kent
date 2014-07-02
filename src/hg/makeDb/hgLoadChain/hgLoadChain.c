/* hgLoadChain - Load a chain file into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"
#include "chainBlock.h"
#include "options.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
        {"noBin", OPTION_BOOLEAN},
	{"noSort", OPTION_BOOLEAN},
        {"tIndex", OPTION_BOOLEAN},
        {"oldTable", OPTION_BOOLEAN},
        {"normScore", OPTION_BOOLEAN},
        {"sqlTable", OPTION_STRING},
        {"qPrefix", OPTION_STRING},
        {"test", OPTION_BOOLEAN},
        {NULL, 0}
};

/* Command line switches. */
static boolean noBin = FALSE;		/* Suppress bin field. */
static boolean noSort = FALSE;		/* Suppress sorting of input. */
static boolean tIndex = FALSE;		/* Include tName in index. */
static boolean oldTable = FALSE;	/* Don't redo table. */
static boolean normScore = FALSE;	/* Add normScore column */
static char *sqlTable = NULL;	/* Read table from this .sql if non-NULL. */
static char *qPrefix = NULL;	/* prefix to prepend (with -) to query name */
static boolean test = FALSE;		/* suppress loading to table */

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadChain - Load a generic Chain file into database\n"
  "usage:\n"
  "   hgLoadChain database chrN_track chrN.chain\n"
  "options:\n"
  "   -tIndex  Include tName in indexes (for non-split chain tables)\n"
  "   -noBin   suppress bin field, default: bin field is added\n"
  "   -noSort  Don't sort by target (memory-intensive) -- input *must* be\n"
  "            sorted by target already if this option is used.\n"
  "   -oldTable add to existing table, default: create new table\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -normScore add normalized score column to table, default: not added\n"
  "   -qPrefix=xxx   prepend \"xxx\" to query name\n"
  "   -test    suppress loading to database\n"
  );
}

static void writeLink(struct cBlock *b, char *tName, int chainId, FILE *f)
/* Write out a single link in tab separated format. */
{
if (!noBin)
    fprintf(f, "%u\t", hFindBin(b->tStart, b->tEnd));
fprintf(f,"%s\t%d\t%d\t%d\t%d\n",
	tName, b->tStart, b->tEnd, b->qStart, chainId);
}

static void writeChain(struct chain *a, FILE *f, int basesMatched)
/* Write out chain (just the header parts) in tab separated format. */
{
char qName[64];

if (!noBin)
    fprintf(f,"%u\t", hFindBin(a->tStart, a->tEnd));
qName[0] = 0;
if (qPrefix != NULL)
    strcat(qName, qPrefix);
strcat(qName, a->qName);
if (normScore)
    fprintf(f, "%f\t%s\t%d\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\t%.1f\n",
	    a->score, a->tName, a->tSize, a->tStart, a->tEnd, 
	    qName, a->qSize, a->qStrand, a->qStart, a->qEnd, a->id,
	    a->score/basesMatched);
else
    fprintf(f, "%f\t%s\t%d\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\n",
	    a->score, a->tName, a->tSize, a->tStart, a->tEnd, 
	    qName, a->qSize, a->qStrand, a->qStart, a->qEnd, a->id);
}


static void loadDatabaseLink(char *database, char *tab, char *track)
/* Load database from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    int minLength = hGetMinIndexLength(database);
    /* Create definition statement. */
    sqlDyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  tName varchar(255) not null,\n");
    dyStringAppend(dy, "  tStart int unsigned not null,\n");
    dyStringAppend(dy, "  tEnd int unsigned not null,\n");
    dyStringAppend(dy, "  qStart int unsigned not null,\n");
    dyStringAppend(dy, "  chainId int unsigned not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (tIndex)
        {
	if (!noBin)
           dyStringPrintf(dy, "  INDEX(tName(%d),bin),\n", minLength);
	}
    else
	{
	if (!noBin)
	   dyStringAppend(dy, "  INDEX(bin),\n");
	}
    dyStringAppend(dy, "  INDEX(chainId)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

dyStringClear(dy);
sqlDyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

static void loadDatabaseChain(char *database, char *tab, char *track, int count)
/* Load database from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    int minLength = hGetMinIndexLength(database);
    /* Create definition statement. */
    sqlDyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  score double not null,\n");
    dyStringAppend(dy, "  tName varchar(255) not null,\n");
    dyStringAppend(dy, "  tSize int unsigned not null, \n");
    dyStringAppend(dy, "  tStart int unsigned not null,\n");
    dyStringAppend(dy, "  tEnd int unsigned not null,\n");
    dyStringAppend(dy, "  qName varchar(255) not null,\n");
    dyStringAppend(dy, "  qSize int unsigned not null,\n");
    dyStringAppend(dy, "  qStrand char(1) not null,\n");
    dyStringAppend(dy, "  qStart int unsigned not null,\n");
    dyStringAppend(dy, "  qEnd int unsigned not null,\n");
    dyStringAppend(dy, "  id int unsigned not null,\n");
    if (normScore)
	dyStringAppend(dy, "  normScore double not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (tIndex)
        {
	if (noBin)
	    {
            dyStringPrintf(dy, "  INDEX(tName(%d),tStart),\n", minLength);
            dyStringPrintf(dy, "  INDEX(tName(%d),tEnd),\n", minLength);
	    }
	else
           dyStringPrintf(dy, "  INDEX(tName(%d),bin),\n", minLength);
	}
    else
	{
	if (noBin)
	    {
	    dyStringAppend(dy, "  INDEX(tStart),\n");
	    dyStringAppend(dy, "  INDEX(tEnd),\n");
	    }
	else
	   dyStringAppend(dy, "  INDEX(bin),\n");
	}
    dyStringAppend(dy, "  INDEX(id)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }
dyStringClear(dy);
sqlDyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
/* add a comment to the history table and finish up connection */
hgHistoryComment(conn, "Loaded %d chains into %s chain table", count, track);
sqlDisconnect(&conn);
}

static void oneChain(struct chain *chain, FILE *linkFile, FILE *chainFile)
/* Put one chain into tab delimited chain and link files. */
{
int basesMatched = 0;
struct cBlock *b;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    basesMatched += b->qEnd - b->qStart;
    writeLink(b, chain->tName, chain->id, linkFile);
    }
writeChain(chain, chainFile, basesMatched);
}
	


static void hgLoadChain(char *database, char *track, char *fileName)
/* hgLoadChain - Load a Chain file into database. */
{
int count = 0;
struct lineFile *lf ;
struct chain *chain, *chainList = NULL;
char chainFileName[] ="chain.tab";
char linkFileName[] ="link.tab";
FILE *chainFile = mustOpen(chainFileName,"w");
FILE *linkFile = mustOpen(linkFileName,"w");
char linkTrack[128];

sprintf(linkTrack, "%sLink",track);
lf = lineFileOpen(fileName, TRUE);
while ((chain = chainRead(lf)) != NULL)
    {
    if (noSort)
	{
	oneChain(chain, linkFile, chainFile);
	/* Freeing slows us down a bit but the purpose of the noSort option */
	/* is to prevent us from running out of memory: */
	chainFree(&chain);
	}
    else
	{
	slAddHead(&chainList, chain);
	}
    ++count;
    }
if (!noSort)
    {
    slSort(&chainList, chainCmpTarget);
    for (chain = chainList; chain != NULL; chain = chain->next)
	oneChain(chain, linkFile, chainFile);
/* chainFreeList(&chainList); */ /* This slows things way down, especially on
                                  * chromosome 19.  I'm not sure if it's just
				  * the usual slow Linux free on 100 million
				  * items or so, or something else. It's a
				  * good thing hgTracks uses lmAlloc on the
				  * chain links it looks like! */
    }
fclose(chainFile);
fclose(linkFile);
if (!test)
    {
    verbose(1, "Loading %d chains into %s.%s\n", count, database, track);
    loadDatabaseChain(database, chainFileName, track, count);
    loadDatabaseLink(database, linkFileName, linkTrack);
    remove(chainFileName);
    remove(linkFileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
noBin = optionExists("noBin");
noSort = optionExists("noSort");
oldTable = optionExists("oldTable");
normScore = optionExists("normScore");
sqlTable = optionVal("sqlTable", NULL);
qPrefix = optionVal("qPrefix", NULL);
test = optionExists("test");
tIndex = optionExists("tIndex");
hgLoadChain(argv[1], argv[2], argv[3]);
return 0;
}
