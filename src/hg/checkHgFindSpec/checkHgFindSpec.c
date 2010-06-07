/* checkHgFindSpec - test & describe search specs in hgFindSpec table. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "portable.h"
#include "hdb.h"
#include "hui.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hgFind.h"
#include "hgFindSpec.h"

static char const rcsid[] = "$Id: checkHgFindSpec.c,v 1.12 2008/09/03 19:18:21 markd Exp $";

char *database = NULL;
/* Need to get a cart in order to use hgFind. */
struct cart *cart = NULL;

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"showSearches",    OPTION_BOOLEAN},
    {"checkTermRegex",  OPTION_BOOLEAN},
    {"exampleFor",      OPTION_STRING},
    {"checkIndexes",    OPTION_BOOLEAN},
    {"makeExamples",    OPTION_BOOLEAN},
    {NULL, 0}
};


void usage()
{
errAbort(
"checkHgFindSpec - test and describe search specs in hgFindSpec tables.\n"
"usage:\n"
"  checkHgFindSpec database [options | termToSearch]\n"
"If given a termToSearch, displays the list of tables that will be searched\n"
"and how long it took to figure that out; then performs the search and the\n"
"time it took.\n"
"options:\n"
"  -showSearches       Show the order in which tables will be searched in\n"
"                      general.  [This will be done anyway if no\n"
"                      termToSearch or options are specified.]\n"
"  -checkTermRegex     For each search spec that includes a regular\n"
"                      expression for terms, make sure that all values of\n"
"                      the table field to be searched match the regex.  (If\n"
"                      not, some of them could be excluded from searches.)\n"
"  -checkIndexes       Make sure that an index is defined on each field to\n"
"                      be searched.\n"
/**#*** IMPLEMENT ME!
"  -exampleFor=search  Randomly choose a term for the specified search (from\n"
"                      the target table for the search).  Search for it.\n"
"  -makeExamples       Print out an HTML table of example positions\n"
"                      (suitable for a gateway description.html).\n"
*/
	 );
}


boolean reportSearch(char *termToSearch)
/* Show the list of tables that will be searched, and how long it took to 
 * figure that out.  Then do the search; show results and time required. */
//#*** this doesn't handle ; in termToSearch (until the actual search)
{
struct hgFindSpec *shortList = NULL, *longList = NULL;
struct hgFindSpec *hfs = NULL;
struct hgPositions *hgp = NULL;
int startMs = 0, endMs = 0;
boolean gotError = FALSE;
char *chrom = NULL;
int chromStart = 0, chromEnd = 0;

hgFindSpecGetAllSpecs(database, &shortList, &longList);
puts("\n");
startMs = clock1000();
for (hfs = shortList;  hfs != NULL;  hfs = hfs->next)
    {
    boolean matches = TRUE;
    boolean tablesExist = hTableOrSplitExists(database, hfs->searchTable);
    if (isNotEmpty(termToSearch) && isNotEmpty(hfs->termRegex))
	matches = matchRegex(termToSearch, hfs->termRegex);
    if (isNotEmpty(hfs->xrefTable))
	tablesExist |= hTableExists(database, hfs->xrefTable);
    if (matches && tablesExist)
	{
	verbose(1, "SHORT-CIRCUIT %s\n", hfs->searchName);
	}
    else if (matches)
	{
	verbose(1, "no table %s: %s%s%s\n", hfs->searchName, hfs->searchTable,
		isNotEmpty(hfs->xrefTable) ? " and/or " : "",
		isNotEmpty(hfs->xrefTable) ? hfs->xrefTable : "");
	}
    else
	{
	verbose(1, "no match %s: %s\n", hfs->searchName, hfs->termRegex);
	}
    }
endMs = clock1000();
printf("\nTook %dms to determine short-circuit searches.\n\n",
       endMs - startMs);

startMs = clock1000();
for (hfs = longList;  hfs != NULL;  hfs = hfs->next)
    {
    boolean matches = TRUE;
    boolean tablesExist = hTableOrSplitExists(database, hfs->searchTable);
    if (isNotEmpty(termToSearch) && isNotEmpty(hfs->termRegex))
	matches = matchRegex(termToSearch, hfs->termRegex);
    if (isNotEmpty(hfs->xrefTable))
	tablesExist |= hTableExists(database, hfs->xrefTable);
    if (matches && tablesExist)
	{
	verbose(1, "ADDITIVE %s\n", hfs->searchName);
	}
    else if (matches)
	{
	verbose(1, "no table %s: %s%s%s\n", hfs->searchName, hfs->searchTable,
		isNotEmpty(hfs->xrefTable) ? " and/or " : "",
		isNotEmpty(hfs->xrefTable) ? hfs->xrefTable : "");
	}
    else
	{
	verbose(1, "no match %s: %s\n", hfs->searchName, hfs->termRegex);
	}
    }
endMs = clock1000();
printf("\nTook %dms to determine multiple/additive searches.\n"
       "(These won't happen if it short-circuits.)\n\n",
       endMs - startMs);

if (isNotEmpty(termToSearch))
    {
    startMs = clock1000();
    hgp = findGenomePos(database, termToSearch, &chrom, &chromStart, &chromEnd, cart);
    endMs = clock1000();
    if (hgp != NULL && hgp->singlePos != NULL)
	{
	struct hgPos *pos = hgp->singlePos;
	char *table = "[No reported table!]";
	char *name = pos->name ? pos->name : "";
	char *browserName = pos->browserName ? pos->browserName : "";
	char *description = pos->description ? pos->description : "";
	if (hgp->tableList != NULL)
	    table = hgp->tableList->name;
	printf("\nSingle result for %s from %s: %s:%d-%d   [%s | %s | %s]\n",
	       termToSearch, table, chrom, chromStart+1, chromEnd,
	       name, browserName, description);
	}
    printf("\nTook %dms to search for %s.\n\n",
	   endMs - startMs, termToSearch);
    }

hgFindSpecFreeList(&shortList);
hgFindSpecFreeList(&longList);
return(gotError);
}

static char *getFieldFromQuery(char *query, char *searchName)
/* Get the value of the field that's being searched in query. */
{
char *ptr = strstr(query, " where ");
char *field = NULL;
if (ptr == NULL)
    errAbort("Can't find \" where \" in query \"%s\" for search %s",
	     query, searchName);
field = cloneString(ptr + strlen(" where "));
ptr = strchr(field, '=');
if (ptr == NULL)
    ptr = strstr(field, " like ");
if (ptr == NULL)
    ptr = strstr(field, " rlike ");
if (ptr == NULL)
    errAbort("Can't find \"=\" or \" like \" after \" where %s\" in query "
	     "\"%s\" for search %s",
	     field, query, searchName);
*ptr = 0;
return(trimSpaces(field));
}

static boolean checkRegexOnTableField(char *exp, char *altExp, char *table,
				      char *field, char *searchName)
/* Return TRUE and complain if any values of table.field do not match exp. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int errCount = 0;
char buf[512];
safef(buf, sizeof(buf), "select %s from %s", field, table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (isEmpty(row[0]))
	continue;
    if (! matchRegex(row[0], exp))
	{
	if (isNotEmpty(altExp) && matchRegex(row[0], altExp))
	    continue;
	if (errCount < 1 ||
	    (errCount < 10 && verboseLevel() > 1))
	    {
	    printf("Error: %s.%s.%s value \"%s\" doesn't match termRegex \"%s\"",
		   database, table, field, row[0], exp);
	    if (isNotEmpty(altExp))
		printf(" or dontCheck \"%s\"", altExp);
	    printf(" for search %s\n", searchName);
	    }
	errCount++;
	}
    }
if (errCount > 0)
    verbose(2, "Search %s: %d values of %s.%s overlooked.\n",
	    searchName, errCount, table, field);
sqlFreeResult(&sr);
hFreeConn(&conn);
return(errCount > 0);
}

boolean doCheckTermRegex()
/* For each search that includes a regex, make sure that all values of the 
 * target table field match the regex -- otherwise those values would be 
 * invisible to a search. */
{
struct hgFindSpec *shortList = NULL, *longList = NULL, *wholeList = NULL;
struct hgFindSpec *hfs = NULL;
boolean gotError = FALSE;

hgFindSpecGetAllSpecs(database, &shortList, &longList);
wholeList = slCat(shortList, longList);

puts("\n");
for (hfs = wholeList;  hfs != NULL;  hfs = hfs->next)
    {
    if (isNotEmpty(hfs->termRegex))
	{
	char *table = NULL, *query = NULL;
	if (isNotEmpty(hfs->xrefTable))
	    {
	    table = hfs->xrefTable;
	    query = hfs->xrefQuery;
	    }
	else
	    {
	    table = hfs->searchTable;
	    query = hfs->query;
	    }
	if (isNotEmpty(query))
	    {
	    struct slName *tableList = hSplitTableNames(database, table);
	    struct slName *tPtr = NULL;
	    char *termPrefix = hgFindSpecSetting(hfs, "termPrefix");
	    char *field = getFieldFromQuery(query, hfs->searchName);
	    char *termRegex = hfs->termRegex;
	    char *altRegex = hgFindSpecSetting(hfs, "dontCheck");
	    if (termPrefix != NULL && startsWith(termPrefix, termRegex+1))
		termRegex += strlen(termPrefix)+1;
	    verbose(2, "Checking termRegex \"%s\" for table %s (search %s).\n",
		    termRegex, table, hfs->searchName);
	    for (tPtr = tableList;  tPtr != NULL;  tPtr = tPtr->next)
		{
		gotError |= checkRegexOnTableField(termRegex, altRegex,
				       tPtr->name, field, hfs->searchName);
		}
	    }
	}
    }

hgFindSpecFreeList(&wholeList);
return(gotError);
}

boolean doCheckIndexes()
/* For each search, make sure there's an index on the right table field(s). */
{
struct hgFindSpec *shortList = NULL, *longList = NULL, *wholeList = NULL;
struct hgFindSpec *hfs = NULL;
struct slName *allChroms = hAllChromNames(database);
boolean gotError = FALSE;

hgFindSpecGetAllSpecs(database, &shortList, &longList);
wholeList = slCat(shortList, longList);

puts("\n");
for (hfs = wholeList;  hfs != NULL;  hfs = hfs->next)
    {
    if (isNotEmpty(hfs->query) && hTableOrSplitExists(database, hfs->searchTable))
	{
	char *field = getFieldFromQuery(hfs->query, hfs->searchName);
	struct slName *tableList = hSplitTableNames(database, hfs->searchTable);
	struct slName *tPtr = NULL;
	for (tPtr = tableList;  tPtr != NULL;  tPtr = tPtr->next)
	    {
	    if (! hFieldHasIndex(database, tPtr->name, field))
		{
		gotError = TRUE;
		printf("Error: No SQL index defined for %s.%s.%s (search %s)\n",
		       database, tPtr->name, field, hfs->searchName);
		}
	    else
		verbose(2, "Index exists for %s.%s (search %s)\n",
			tPtr->name, field, hfs->searchName);
	    }
	}
    if (isNotEmpty(hfs->xrefQuery) && hTableOrSplitExists(database, hfs->xrefTable))
	{
	char *field = getFieldFromQuery(hfs->xrefQuery, hfs->searchName);
	if (! hFieldHasIndex(database, hfs->xrefTable, field))
	    {
	    gotError = TRUE;
	    printf("Error: No SQL index defined for %s.%s.%s (search %s)\n",
		   database, hfs->xrefTable, field, hfs->searchName);
	    }
	else
	    verbose(2, "Index exists for %s.%s.%s (search %s)\n",
		    database, hfs->xrefTable, field, hfs->searchName);
	}
    }

slFreeList(&allChroms);
hgFindSpecFreeList(&wholeList);
return(gotError);
}

char *getExampleFor(char *searchName)
/* Randomly choose a search field value -- if it comes from the table,
 * we should be able to search for it and find our way back to the table. */
{
char *example = NULL;

errAbort("Sorry, -exampleFor=search not implemented yet.");

return(example);
}

boolean doMakeExamples()
/* Print out an HTML table of position examples for description.html. */
{
boolean gotError = FALSE;

errAbort("Sorry, -makeExamples not implemented yet.");

return(gotError);
}


int checkHgFindSpec(char *db, char *termToSearch, boolean showSearches,
		    boolean checkTermRegex, char *exampleFor,
		    boolean checkIndexes, boolean makeExamples)
/* Perform searches/checks as specified, summarize errors, 
 * return nonzero if there are errors. */
{
boolean gotError = FALSE;

database = db;

if (isNotEmpty(termToSearch))
    gotError |= reportSearch(termToSearch);
if (showSearches)
    gotError |= reportSearch(NULL);
if (checkTermRegex)
    gotError |= doCheckTermRegex();
if (isNotEmpty(exampleFor))
    {
    termToSearch = getExampleFor(exampleFor);
    gotError |= reportSearch(termToSearch);
    }
if (checkIndexes)
    gotError |= doCheckIndexes();
if (makeExamples)
    gotError |= doMakeExamples();

return gotError;
}


/* Just a placeholder -- we don't do anything with the cart. */
char *excludeVars[] = { NULL };


int main(int argc, char *argv[])
{
char   *termToSearch   = NULL;
boolean showSearches   = FALSE;
boolean checkTermRegex = FALSE;
char   *exampleFor     = NULL;
boolean checkIndexes   = FALSE;
boolean makeExamples   = FALSE;

optionInit(&argc, argv, optionSpecs);
/* Allow "checkHgFindSpec db" or "checkHgFindSpec db termToSearch" usage: */
if (termToSearch == NULL && argc == 3)
    {
    termToSearch = argv[2];
    argc--;
    }
if (argc != 2)
    usage();

showSearches = optionExists("showSearches");
checkTermRegex = optionExists("checkTermRegex");
exampleFor = optionVal("exampleFor", exampleFor);
checkIndexes = optionExists("checkIndexes");
makeExamples = optionExists("makeExamples");

/* If no termToSearch or options are specified, do showSearches. */
if (termToSearch == NULL && exampleFor == NULL &&
    !showSearches && !checkTermRegex && !checkIndexes && !makeExamples)
    showSearches = TRUE;

cgiSpoof(&argc, argv);
cart = cartAndCookie(hUserCookie(), excludeVars, NULL);
return checkHgFindSpec(argv[1], termToSearch, showSearches, checkTermRegex,
		       exampleFor, checkIndexes, makeExamples);
}
