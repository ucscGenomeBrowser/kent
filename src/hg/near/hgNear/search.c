/* search - manage simple searches - ones based on just a single name. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: search.c,v 1.7 2003/09/08 09:04:21 kent Exp $";

int searchResultCmpShortLabel(const void *va, const void *vb)
/* Compare to sort based on short label. */
{
const struct searchResult *a = *((struct searchResult **)va);
const struct searchResult *b = *((struct searchResult **)vb);
return strcmp(a->shortLabel, b->shortLabel);
}

static struct genePos *findGenePredPos(struct sqlConnection *conn, char *table, char *search)
/* Find out gene ID from search term. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct genePos *gpList = NULL, *gp;

if (search != NULL)
    {
    safef(query, sizeof(query), 
    	"select name,chrom,txStart,txEnd from %s where name = '%s'", 
		table, search);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	AllocVar(gp);
	gp->name = cloneString(row[0]);
	gp->chrom = cloneString(row[1]);
	gp->start = sqlUnsigned(row[2]);
	gp->end = sqlUnsigned(row[3]);
	slAddHead(&gpList, gp);
	}
    sqlFreeResult(&sr);
    }
slReverse(&gpList);
return gpList;
}

struct columnSearchResults
/* Search results for one column. */
    {
    struct columnSearchResults *next;	/* next in list. */
    char *label;  	  /* Description of column - not allocated here. */
    struct searchResult *results; /* List of results. */
    };

static void transformToCannonical(struct searchResult *list,
	struct sqlConnection *conn)
/* Transform search results to cannonical versions.  */
{
char buf[64];
char query[512];
struct sqlResult *sr;
char **row;
struct searchResult *el;
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query),
    	"select knownCannonical.transcript,knownCannonical.chrom,"
	          "knownCannonical.chromStart,knownCannonical.chromEnd,"
		  "knownCannonical.protein "
	"from knownIsoforms,knownCannonical "
	"where knownIsoforms.transcript = '%s' "
	"and knownIsoforms.clusterId = knownCannonical.clusterId"
	, el->gp.name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	genePosFillFrom5(&el->gp, row);
    sqlFreeResult(&sr);
    }
}

static struct searchResult *removeDupes(struct searchResult *list)
/* Remove duplicates from list.  Return weeded list. */
{
struct searchResult *el, *next, *newList = NULL;
struct hash *dupHash = newHash(0);
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (!hashLookup(dupHash, el->gp.name))
        {
	hashAdd(dupHash, el->gp.name, NULL);
	slAddHead(&newList, el);
	}
    }
hashFree(&dupHash);
slReverse(&newList);
return newList;
}

static void searchAllColumns(struct sqlConnection *conn, 
	struct column *colList, char *search)
/* Call search on each column. */
{
struct column *col;
struct searchResult *srList, *sr;
struct columnSearchResults *csrList = NULL, *csr;
int totalCount = 0;
struct searchResult *srOne = NULL;

for (col = colList; col != NULL; col = col->next)
    {
    if (col->simpleSearch)
	 {
         srList = col->simpleSearch(col, conn, search);
	 if (showOnlyCannonical() && srList != NULL)
	     {
	     transformToCannonical(srList, conn);
	     srList = removeDupes(srList);
	     }
	 if (srList != NULL)
	     {
	     srOne = srList;
	     AllocVar(csr);
	     csr->label = columnSetting(col, "searchLabel", col->longLabel);
	     csr->results = srList;
	     slAddTail(&csrList, csr);
	     totalCount += slCount(srList);
	     }
	 }
    }
if (totalCount == 0)
    errAbort("Sorry, couldn't find '%s'", search);
else if (totalCount == 1)
    displayData(conn, colList, &srOne->gp);
else
    {
    hPrintf("<H1>Simple Search Results</H1>\n");
    for (csr = csrList; csr != NULL; csr = csr->next)
        {
	hPrintf("<H2>%s</H2>\n", csr->label);
	slSort(&csr->results, searchResultCmpShortLabel);
	for (sr = csr->results; sr != NULL; sr = sr->next)
	    {
	    selfAnchorSearch(&sr->gp);
	    hPrintf("%s</A> - %s<BR>\n", sr->shortLabel, sr->longLabel);
	    }
	}
    }
}

static struct genePos *findKnownAccessions(struct sqlConnection *conn, 
	char *search)
/* Return list of known accessions. */
{
return findGenePredPos(conn, "knownGene", search);
}

void doSearch(struct sqlConnection *conn, struct column *colList)
/* Search.  If result is unambiguous call displayData, otherwise
 * put up a page of choices. */
{
char *search = cartString(cart, searchVarName);
struct genePos *accList;
search = trimSpaces(search);
accList = findKnownAccessions(conn, search);
if (accList != NULL)
    {
    if (slCount(accList) == 1 || !sameString(groupOn, "position"))
	displayData(conn, colList, accList);
    else
	{
	struct genePos *acc;
	hPrintf("%s maps to %d positions, please pick one below:<BR>\n", 
	    search, slCount(accList));
	for (acc = accList; acc != NULL; acc = acc->next)
	    {
	    hPrintf("&nbsp;");
	    selfAnchorSearch(acc);
	    hPrintf("%s:%d-%d</A><BR>\n", acc->chrom, acc->start+1, acc->end);
	    }
	}
	
    }
else
    {
    searchAllColumns(conn, colList, search);
    }
}

