/* hgLoadBlastTab - Load blast table into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "options.h"
#include "hgRelate.h"
#include "blastTab.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBlastTab - Load blast table into database\n"
  "usage:\n"
  "   hgLoadBlastTab database table file(s).tab\n"
  "File.tab is something generated via ncbi blast\n"
  "using the -m 8 flag\n"
  "options:\n"
  "   -createOnly - just create the table, don't load it\n"
  "   -maxPer=N - maximum to load for any query sequence\n"
  "   -bestOnly - keep only the first hit for {query,target} (blast output\n"
  "               is sorted by score so first hit is best-scoring hit)\n"
  );
}

int maxPer = BIGNUM;

static struct optionSpec options[] = {
   {"createOnly", OPTION_BOOLEAN},
   {"maxPer", OPTION_INT},
   {"bestOnly", OPTION_BOOLEAN},
   {NULL, 0},
};

void blastTabTableCreate(struct sqlConnection *conn, char *tableName)
/* Create a scored-ref table with the given name. */
{
static char *createString = "CREATE TABLE %s (\n"
"    query varchar(255) not null,	# Name of query sequence\n"
"    target varchar(255) not null,	# Name of target sequence\n"
"    identity float not null,	# Percent identity\n"
"    aliLength int unsigned not null,	# Length of alignment\n"
"    mismatch int unsigned not null,	# Number of mismatches\n"
"    gapOpen int unsigned not null,	# Number of gap openings\n"
"    qStart int unsigned not null,	# Start in query (0 based)\n"
"    qEnd int unsigned not null,	# End in query (non-inclusive)\n"
"    tStart int unsigned not null,	# Start in target (0 based)\n"
"    tEnd int unsigned not null,	# Start in query (non-inclusive)\n"
"    eValue double not null,	# Expectation value\n"
"    bitScore double not null,	# Bit score\n"
"              #Indices\n"
"    INDEX(query)\n"
")\n";
struct dyString *dy = newDyString(1024);
sqlDyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

#define THASH_BITS 12

void hgLoadBlastTab(char *database, char *table, int inCount, char *inNames[])
/* hgLoadBlastTab - Load blast table into database. */
{
struct sqlConnection *conn = sqlConnect(database);
blastTabTableCreate(conn, table);
sqlDisconnect(&conn);

if (!optionExists("createOnly"))
    {
    FILE *f = hgCreateTabFile(".", table);
    struct hash *tHash;
    boolean bestOnly = optionExists("bestOnly");
    int i;
    int count = 0;
    int qHitCount = 0;
    char lastQ[512];
    lastQ[0] = 0;
    verbose(1, "Scanning through %d files\n", inCount);

    if (bestOnly)
	tHash = hashNew(THASH_BITS);
    for (i=0; i<inCount; ++i)
	{
	struct lineFile *lf = lineFileOpen(inNames[i], TRUE);
	struct blastTab bt;
	char *row[BLASTTAB_NUM_COLS];
	while (lineFileRow(lf, row))
	    {
	    blastTabStaticLoad(row, &bt);
	    bt.qStart -= 1;
	    bt.tStart -= 1;
	    if (!sameString(lastQ, bt.query))
	        {
		safef(lastQ, sizeof(lastQ), "%s", bt.query);
		qHitCount = 0;
		if (bestOnly)
		    {
		    hashFree(&tHash);
		    tHash = hashNew(THASH_BITS);
		    }
		}
	    /* Keep only the best alignment of query and target: */
	    if (bestOnly)
		{
		if (hashLookup(tHash, bt.target) != NULL)
		    continue;
		hashAddInt(tHash, bt.target, 1);
		}
	    ++qHitCount;
	    if (qHitCount <= maxPer)
		{
		blastTabTabOut(&bt, f);
		++count;
		}
	    }
	lineFileClose(&lf);
	}
    if (bestOnly)
	hashFree(&tHash);
    verbose(1, "Loading database with %d rows\n", count);
    conn = sqlConnect(database);
    hgLoadTabFile(conn, ".", table, &f);
    hgRemoveTabFile(".", table);

    /* add a comment to the history table and finish up connection */
    hgHistoryComment(conn, "Add %d blast alignments to %s table", count, table);
    sqlDisconnect(&conn);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
maxPer = optionInt("maxPer", maxPer);
hgLoadBlastTab(argv[1], argv[2], argc-3, argv+3);
return 0;
}
