/* chainNetDbLoad - This will load a database representation of
 * a net into a chainNet representation.  Also helps database
 * representation of chain into chain. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "chain.h"
#include "chainDb.h"
#include "chainLink.h"
#include "chainNet.h"
#include "netAlign.h"
#include "chainNetDbLoad.h"

static char const rcsid[] = "$Id: chainNetDbLoad.c,v 1.10 2008/09/03 19:19:20 markd Exp $";

struct cnFill *cnFillFromNetAlign(struct netAlign *na, struct hash *nameHash)
/* Convert netAlign to cnFill. Name hash is a place to store
 * the strings. */
{
struct cnFill *fill;
AllocVar(fill);
fill->tStart = na->tStart;
fill->tSize = na->tEnd - na->tStart;
fill->qName = hashStoreName(nameHash, na->qName);
fill->qStrand = na->strand[0];
fill->qStart = na->qStart;
fill->qSize = na->qEnd - na->qStart;
fill->chainId = na->chainId;
fill->score = na->score;
fill->ali = na->ali;
fill->qOver = na->qOver;
fill->qFar = na->qFar;
fill->qDup = na->qDup;
if (!sameString(na->type, "gap"))
    fill->type = hashStoreName(nameHash, na->type);
fill->tN = na->tN;
fill->qN = na->qN;
fill->tR = na->tR;
fill->qR = na->qR;
fill->tNewR = na->tNewR;
fill->qNewR = na->qNewR;
fill->tOldR = na->tOldR;
fill->qOldR = na->qOldR;
fill->tTrf = na->tTrf;
fill->qTrf = na->qTrf;
return fill;
}

struct cnlHelper
/* Structure to help us make tree from flattened database
 * representation. */
    {
    struct cnlHelper *next;	/* Next in list. */
    char *tName;		/* Name of target.  Not allocated here. */
    struct hash *nameHash;	/* Place to keep names. */
    struct cnFill **levels;	/* Array to sort leaves by depth. */
    int maxDepth;		/* Maximum depth allowed. */
    };

struct chainNet *helpToNet(struct cnlHelper **pHelp)
/* Make a chainNet from cnlHelper.  This will destroy
 * *pHelp in the process. */
{
struct chainNet *net;
struct cnlHelper *help = *pHelp;
struct cnFill *parentList, *childList, *parent, *child, *nextChild;
struct cnFill **levels = help->levels;
int depth, maxDepth = 0;

/* Note that level 0 is always empty. */

/* Sort everybody by target start. */
for (depth=1; ; ++depth)
    {
    if (levels[depth] == NULL && depth != 0)
        break;
    maxDepth = depth;
    slSort(&levels[depth], cnFillCmpTarget);
    }

/* Assign children to parents. */
for (depth=maxDepth; depth >= 2; --depth)
    {
    childList = levels[depth];
    parentList = levels[depth-1];
    child = childList;
    for (parent = parentList; parent != NULL; parent = parent->next)
        {
	while (child != NULL && 
		child->tStart + child->tSize <= parent->tStart + parent->tSize)
	    {
	    nextChild = child->next;
	    slAddHead(&parent->children, child);
	    child = nextChild;
	    }
	slReverse(&parent->children);
	}
    }

/* Make up net structure and fill it in. */
AllocVar(net);
net->name = cloneString(help->tName);
net->fillList = levels[1];
net->nameHash = help->nameHash;

/* Cleanup what's left of help and go home. */
freeMem(help->levels);
freez(pHelp);
return net;
}

struct chainNet *chainNetLoadResult(struct sqlResult *sr, int rowOffset)
/* Given a query result that returns a bunch netAligns, make up
 * a list of chainNets that has the equivalent information. 
 * Note the net->size field is not filled in. */
{
char **row;
struct netAlign na;
struct cnFill *fill;
struct cnlHelper *helpList = NULL, *help, *nextHelp;
struct chainNet *netList = NULL, *net;
struct hash *helpHash = hashNew(0);

/* Convert database rows to cnFills, and store
 * them in help->levels. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    netAlignStaticLoad(row + rowOffset, &na);
    help = hashFindVal(helpHash, na.tName);
    if (help == NULL)
        {
	AllocVar(help);
	hashAddSaveName(helpHash, na.tName, help, &help->tName);
	help->nameHash = hashNew(8);
	help->maxDepth = 40;
	slAddHead(&helpList, help);
	AllocArray(help->levels, help->maxDepth);
	}
    fill = cnFillFromNetAlign(&na, help->nameHash);
    if (na.level >= help->maxDepth)
        errAbort("can't handle level %d, only go up to %d", 
		na.level, help->maxDepth-1);
    slAddHead(&help->levels[na.level], fill);
    }

/* Convert helps to nets. */
for (help = helpList; help != NULL; help = nextHelp)
    {
    nextHelp = help->next;
    net = helpToNet(&help);
    slAddHead(&netList, net);
    }
helpList = NULL;

/* Clean up and go home. */
hashFree(&helpHash);
return netList;
}

struct chainNet *chainNetLoadRange(char *database, char *track,
	char *chrom, int start, int end, char *extraWhere)
/* Load parts of a net track that intersect range. */
{
int rowOffset;
struct sqlConnection *conn;
struct sqlResult *sr;
struct chainNet *net;

conn = sqlConnect(database);
sr = hRangeQuery(conn, track, chrom, start, end, extraWhere, &rowOffset);
net = chainNetLoadResult(sr, rowOffset);
sqlFreeResult(&sr);
if (net != NULL)
    net->size = hChromSize(database, chrom);
sqlDisconnect(&conn);
return net;
}

struct chainNet *chainNetLoadChrom(char *database, char *track,
	char *chrom, char *extraWhere)
/* Load net on whole chromosome. */
{
int rowOffset;
struct sqlConnection *conn;
struct sqlResult *sr;
struct chainNet *net;

conn = sqlConnect(database);
sr = hChromQuery(conn, track, chrom, extraWhere, &rowOffset);
net = chainNetLoadResult(sr, rowOffset);
sqlFreeResult(&sr);
net->size = hChromSize(database, chrom);
sqlDisconnect(&conn);
return net;
}

static void chainLinkAddResult(struct sqlResult *sr, int rowOffset, struct chain *chain)
/* Add links that we get from sr to chain. */
{
char **row;
struct cBlock *list = NULL, *b;
struct chainLink link;
while ((row = sqlNextRow(sr)) != NULL)
    {
    chainLinkStaticLoad(row+rowOffset, &link);
    AllocVar(b);
    b->tStart = link.tStart;
    b->tEnd = link.tEnd;
    b->qStart = link.qStart;
    b->qEnd = link.qStart + (link.tEnd - link.tStart);
    slAddHead(&list, b);
    }
slSort(&list, cBlockCmpTarget);
chain->blockList = list;
}

static struct chain *chainLoadIdSome(char *database, char *track, char *chrom, 
	int start, int end, int id, boolean loadAll)
/* Load some or all of chain. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char table[64];
boolean hasBin;
struct chain *chain;
char query[256];
struct dyString *dy = newDyString(128);

/* Load chain header. */
if (!hFindSplitTable(database, chrom, track, table, &hasBin))
   errAbort("%s table is not in %s", track, database);
conn = sqlConnect(database);
snprintf(query, sizeof(query),
	"select * from %s where id = %d", table, id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("chain %d is not in %s", id, table);
chain = chainHeadLoad(row+hasBin);
sqlFreeResult(&sr);

/* Load links. */
if (loadAll)
    {
    dyStringPrintf(dy, 
	 "select * from %sLink where chainId = %d", table, id);
    }
else
    {
    dyStringPrintf(dy, 
	 "select * from %sLink where ",table );
    hAddBinToQuery(start, end, dy);
    dyStringPrintf(dy," chainId = %d and tStart < %d and tEnd > %d", id, end, start);
    }
sr = sqlGetResult(conn, dy->string);
chainLinkAddResult(sr, hasBin, chain);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
freeDyString(&dy);
return chain;
}

struct chain *chainLoadIdRange(char *database, char *track, char *chrom, 
	int start, int end, int id)
/* Load parts of chain of given ID from database.  Note the chain header
 * including score, tStart, tEnd, will still reflect the whole chain,
 * not just the part in range.  However only the blocks of the chain
 * overlapping the range will be loaded. */
{
return chainLoadIdSome(database, track, chrom, start, end, id, FALSE);
}

struct chain *chainLoadId(char *database, char *track, char *chrom, int id)
/* Load chain of given ID from database. */
{
return chainLoadIdSome(database, track, chrom, 0, 0, id, TRUE);
}

