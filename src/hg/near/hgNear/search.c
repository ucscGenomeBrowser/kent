#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgNear.h"

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

void doSearch(struct sqlConnection *conn, struct column *colList, char *search)
/* Search.  If result is unambiguous call displayData, otherwise
 * put up a page of choices. */
{
search = trimSpaces(search);
if (search != NULL && sameString(search, ""))
    search = NULL;
if (search == NULL)
    displayData(conn, colList, NULL);
else 
    {
    struct genePos *accList = findKnownAccessions(conn, search);
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
}

