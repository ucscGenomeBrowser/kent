/* chainDbToFile - translate a chain's db representation back to file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "localmem.h"
#include "chain.h"
#include "chainDb.h"

static char const rcsid[] = "$Id: chainDbToFile.c,v 1.3 2008/09/03 19:20:34 markd Exp $";

static struct optionSpec options[] = {
/*   {"option", OPTION_STRING}, */
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainDbToFile - translate a chain's db representation back to file\n"
  "usage:\n"
  "   chainDbToFile database chainTable out.chain\n"
/*
  "options:\n"
  "   -option=whatever - blah blah\n"
*/
  );
}


struct hash *hashLinks(char *database, char *chainTblName)
/* Read in all chain links, translate to block lists, and hash by 
 * tName.chainId.
 * chainDb.c has chainAddBlocks -- but that's one SQL query per chain,
 * way too slow for bulk. 
 * Note: blockLists will be in reverse order rel. to database. 
 * Since we don't necessarily know when we're done with a chainId, 
 * leave the reversing to the consumer -- chainDbToFile(). */
{
struct hash *h = newHash(20);
struct hashEl *hel = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];

safef(query, sizeof(query),
      "select tName,chainId,tStart,tEnd,qStart from %sLink", chainTblName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cBlock *b = NULL;
    char key[128];
    safef(key, sizeof(key), "%s.%s", row[0], row[1]);
    /* Avoid hashLookup/Add if this row is for same chain as last row. */
    if (hel == NULL || !sameString(hel->name, key))
	{
	hel = hashLookup(h, key);
	if (hel == NULL)
	    hel = hashAdd(h, key, NULL);
	}
    AllocVar(b);
    b->tStart = sqlUnsigned(row[2]);
    b->tEnd = sqlUnsigned(row[3]);
    b->qStart = sqlUnsigned(row[4]);
    b->qEnd = b->qStart + (b->tEnd - b->tStart);
    slAddHead(&(hel->val), b);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
return(h);
}

int sortBoxInTStart(const void *pa, const void *pb)
/* Sort cBlock elements by tStart (ascending). */
{
return((*(struct cBlock **)pa)->tStart - (*(struct cBlock **)pb)->tStart);
}

void chainDbToFile(char *database, char *chainTable, char *outName)
/* chainDbToFile - translate a chain's db representation back to file. */
{
struct slName *chainTables = hSplitTableNames(database, chainTable);
struct slName *chainTbl = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
FILE *f = mustOpen(outName, "w");

for (chainTbl = chainTables;  chainTbl != NULL;  chainTbl = chainTbl->next)
    {
    boolean hasBin = hOffsetPastBin(database, NULL, chainTbl->name);
    struct hash *h = hashLinks(database, chainTbl->name);
    safef(query, sizeof(query), "select * from %s", chainTbl->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct chain *chain = chainHeadLoad(row + hasBin);
	struct hashEl *hel = NULL;
	char key[128];
	safef(key, sizeof(key), "%s.%d", chain->tName, chain->id);
	hel = hashLookup(h, key);
	if (hel == NULL)
	    errAbort("chain %d (on target %s) not found in link table %sLink",
		     chain->id, chain->tName, chainTbl->name);
	else if (hel->val == NULL)
	    errAbort("chain %d (on target %s) duplicated in %s",
		     chain->id, chain->tName, chainTbl->name);
	slReverse(&(hel->val));
	/* Just to be safe, sort by tStart. */
	slSort(&(hel->val), sortBoxInTStart);
	chain->blockList = (struct cBlock *)hel->val;
	chainWrite(chain, f);
	chainFree(&chain);
	/* chainFree frees the blockList, so NULL it out in the hash. */
	hel->val = NULL;
	}
    hashFree(&h);
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

chainDbToFile(argv[1], argv[2], argv[3]);

return 0;
}
