/* search - manage simple searches - ones based on just a single name. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "obscure.h"
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
    sqlSafef(query, sizeof(query), 
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

static void transformToCanonical(struct searchResult *list,
	struct sqlConnection *conn)
/* Transform search results to canonical versions.  */
{
struct dyString *dy = newDyString(1024);
char *cannon = genomeSetting("canonicalTable");
char *isoform = genomeSetting("isoformTable");
struct sqlResult *sr;
char **row;
struct searchResult *el;
for (el = list; el != NULL; el = el->next)
    {
    dyStringClear(dy);
    sqlDyStringPrintf(dy, 
    	"select %s.transcript,%s.chrom,%s.chromStart,%s.chromEnd,%s.protein ",
	cannon, cannon, cannon, cannon, cannon);
    sqlDyStringPrintf(dy,
	"from %s,%s ",
	isoform, cannon);
    sqlDyStringPrintf(dy,
        "where %s.transcript = '%s' ",
	isoform, el->gp.name);
    sqlDyStringPrintf(dy,
        "and %s.clusterId = %s.clusterId",
	isoform, cannon);
    sr = sqlGetResult(conn, dy->string);
    if ((row = sqlNextRow(sr)) != NULL)
	genePosFillFrom5(&el->gp, row);
    sqlFreeResult(&sr);
    }
dyStringFree(&dy);
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

static void fillInMissingLabels(struct sqlConnection *conn,
	struct column *colList, struct searchResult *srList)
/* Fill in missing columns by looking up name and description
 * if possible. */
{
struct column *nameColumn = findNamedColumn("name");
struct column *desColumn = findNamedColumn("description");
struct searchResult *sr;

for (sr = srList; sr != NULL; sr = sr->next)
    {
    if (sr->shortLabel == NULL)
        {
	if (nameColumn != NULL)
	    sr->shortLabel = nameColumn->cellVal(nameColumn, &sr->gp, conn);
	if (sr->shortLabel == NULL)
	    sr->shortLabel = cloneString(sr->gp.name);
	}
    if (sr->longLabel == NULL)
        {
	if (desColumn != NULL)
	    sr->longLabel = desColumn->cellVal(desColumn, &sr->gp, conn);
	else
	    sr->longLabel = cloneString("");
	}
    }
}

static boolean allSame(struct columnSearchResults *csrList)
/* Return TRUE if all search results point to the same place. */
{
struct hash *dupeHash = newHash(0);
char hashName[512];
int sepCount = 0;
struct columnSearchResults *csr;
struct searchResult *sr;

for (csr = csrList; csr != NULL && sepCount < 2; csr = csr->next)
    {
    for (sr = csr->results; sr != NULL && sepCount < 2; sr = sr->next)
        {
	char *chrom = sr->gp.chrom;
	if (chrom == NULL) chrom = "n/a";
	safef(hashName, sizeof(hashName), "%s %s:%d-%d", 
		sr->gp.name, chrom, sr->gp.start, sr->gp.end);
	if (!hashLookup(dupeHash, hashName))
	    {
	    hashAdd(dupeHash, hashName, NULL);
	    ++sepCount;
	    }
	}
    }
hashFree(&dupeHash);
return sepCount <= 1;
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
	 if (showOnlyCanonical() && srList != NULL)
	     {
	     transformToCanonical(srList, conn);
	     srList = removeDupes(srList);
	     }
	 if (srList != NULL)
	     {
	     srOne = srList;
	     fillInMissingLabels(conn, colList, srList);
	     AllocVar(csr);
	     csr->label = columnSetting(col, "searchLabel", col->longLabel);
	     csr->results = srList;
	     slAddTail(&csrList, csr);
	     totalCount += slCount(srList);
	     }
	 }
    }
if (totalCount == 0)
    {
    displayData(conn, colList, NULL);
    if (anyWild(search))
        warn("Sorry, the search box doesn't take wildcards, "
	     "though in most cases it will find things without "
	     "them.  Try entering your search without * or ? "
	     "characters.");
    else
	warn("Sorry, couldn't find '%s'", search);
    }
else if (totalCount == 1 || allSame(csrList))
// else if (totalCount == 1)
    displayData(conn, colList, &srOne->gp);
else
    {
    makeTitle("Simple Search Results", NULL);
    for (csr = csrList; csr != NULL; csr = csr->next)
        {
	hPrintf("<H2>%s</H2>\n", csr->label);
	slSort(&csr->results, searchResultCmpShortLabel);
	for (sr = csr->results; sr != NULL; sr = sr->next)
	    {
	    selfAnchorSearch(&sr->gp);
	    if (sr->matchingId != NULL)
		hPrintf("%s (%s)", sr->matchingId, sr->shortLabel);
	    else
		hPrintf("%s", sr->shortLabel);
	    hPrintf("</A> - %s<BR>\n", sr->longLabel);
	    }
	}
    }
}

static char *transcriptToCanonical(struct sqlConnection *conn, char *transcript)
/* Translate transcript to canonical ID if possible, otherwise just return 
 * a copy of transcript. */
{
struct dyString *dy = newDyString(1024);
char *cannon = genomeSetting("canonicalTable");
char *isoform = genomeSetting("isoformTable");
char buf[128];
char *result = NULL;
sqlDyStringPrintf(dy, "select %s.transcript from %s,%s where %s.transcript = '%s'",
	       cannon, isoform, cannon, isoform, transcript);
sqlDyStringPrintf(dy, " and %s.clusterId = %s.clusterId", isoform, cannon);
result = sqlQuickQuery(conn, dy->string, buf, sizeof(buf));
if (result != NULL)
    return(cloneString(result));
else
    return(cloneString(transcript));
}

static struct genePos *findKnownAccessions(struct sqlConnection *conn, 
	char *search)
/* Return list of known accessions. */
{
search = transcriptToCanonical(conn, search);
return findGenePredPos(conn, genomeSetting("geneTable"), search);
}

void doSearch(struct sqlConnection *conn, struct column *colList)
/* Search.  If result is unambiguous call displayData, otherwise
 * put up a page of choices. */
{
char *search = cartString(cart, searchVarName);
char *escSearch = makeEscapedString(trimSpaces(search), '\'');
struct genePos *accList;
accList = findKnownAccessions(conn, escSearch);
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
    searchAllColumns(conn, colList, escSearch);
    }
freez(&escSearch);
}

