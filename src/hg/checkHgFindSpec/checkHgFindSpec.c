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

static char const rcsid[] = "$Id: checkHgFindSpec.c,v 1.1 2004/04/03 02:36:36 angie Exp $";

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
/**#*** IMPLEMENT ME!
"options:\n"
"  -showSearches       Show the order in which tables will be searched in\n"
"                      general.  [This will be done anyway if no\n"
"                      termToSearch or options are specified.]\n"
"  -checkTermRegex     For each search spec that includes a regular\n"
"                      expression for terms, make sure that all values of\n"
"                      the table field to be searched match the regex.  (If\n"
"                      not, some of them could be excluded from searches.)\n"
"  -exampleFor=search  Randomly choose a term for the specified search (from\n"
"                      the target table for the search).  Search for it.\n"
"  -checkIndexes       Make sure that an index is defined on each field to\n"
"                      be searched.\n"
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
struct hgFindSpec *shortList = hgFindSpecGetShortCircuits();
struct hgFindSpec *longList  = hgFindSpecGetAdditives();
struct hgFindSpec *hfs = NULL;
struct hgPositions *hgp = NULL;
char *db = hGetDb();
int startMs = 0, endMs = 0;
boolean gotError = FALSE;
char *chrom = NULL;
int chromStart = 0, chromEnd = 0;

puts("\n");
startMs = clock1000();
for (hfs = shortList;  hfs != NULL;  hfs = hfs->next)
    {
    boolean matches = TRUE;
    boolean tablesExist = hTableOrSplitExistsDb(db, hfs->searchTable);
    if (isNotEmpty(hfs->termRegex))
	matches = matchRegex(termToSearch, hfs->termRegex);
    if (isNotEmpty(hfs->xrefTable))
	tablesExist |= hTableExists(hfs->xrefTable);
    if (matches && tablesExist)
	{
	printf("SHORT-CIRCUIT %s\n", hfs->searchName);
	}
    else if (matches)
	{
	printf("no table %s: %s%s%s\n", hfs->searchName, hfs->searchTable,
	       isNotEmpty(hfs->xrefTable) ? " and/or " : "",
	       isNotEmpty(hfs->xrefTable) ? hfs->xrefTable : "");
	}
    else
	{
	printf("no match %s: %s\n", hfs->searchName, hfs->termRegex);
	}
    }
endMs = clock1000();
printf("\nTook %dms to determine short-circuit searches.\n\n",
       endMs - startMs);

startMs = clock1000();
for (hfs = longList;  hfs != NULL;  hfs = hfs->next)
    {
    boolean matches = TRUE;
    boolean tablesExist = hTableOrSplitExistsDb(db, hfs->searchTable);
    if (isNotEmpty(hfs->termRegex))
	matches = matchRegex(termToSearch, hfs->termRegex);
    if (isNotEmpty(hfs->xrefTable))
	tablesExist |= hTableExists(hfs->xrefTable);
    if (matches && tablesExist)
	{
	printf("ADDITIVE %s\n", hfs->searchName);
	}
    else if (matches)
	{
	printf("no table %s: %s%s%s\n", hfs->searchName, hfs->searchTable,
	       isNotEmpty(hfs->xrefTable) ? " and/or " : "",
	       isNotEmpty(hfs->xrefTable) ? hfs->xrefTable : "");
	}
    else
	{
	printf("no match %s: %s\n", hfs->searchName, hfs->termRegex);
	}
    }
endMs = clock1000();
printf("\nTook %dms to determine multiple/additive searches.\n"
       "(These won't happen if it short-circuits.)\n\n",
       endMs - startMs);

startMs = clock1000();
hgp = findGenomePos(termToSearch, &chrom, &chromStart, &chromEnd, cart);
endMs = clock1000();
if (hgp != NULL && hgp->singlePos != NULL)
    {
    char *table = "[No reported table!]";
    if (hgp->tableList != NULL)
	table = hgp->tableList->name;
    printf("\nSingle result for %s from %s: %s:%d-%d\n", termToSearch,
	   table, chrom, chromStart, chromEnd);
    }
printf("\nTook %dms to search for %s.\n\n",
       endMs - startMs, termToSearch);

hgFindSpecFreeList(&shortList);
hgFindSpecFreeList(&longList);
return(gotError);
}

boolean doShowSearches()
/* Show what searches will be performed in general (for a term that does not 
 * short-circuit to a search, or get excluded by regexps). */
{
struct hgFindSpec *shortList = hgFindSpecGetShortCircuits();
struct hgFindSpec *longList  = hgFindSpecGetAdditives();
struct hgFindSpec *hfs = NULL;
boolean gotError = FALSE;

errAbort("Sorry, not implemented yet.");

for (hfs = shortList;  hfs != NULL;  hfs = hfs->next)
    {
    }

hgFindSpecFreeList(&shortList);
hgFindSpecFreeList(&longList);
return(gotError);
}

boolean doCheckTermRegex()
/* For each search that includes a regex, make sure that all values of the 
 * target table field match the regex -- otherwise those values would be 
 * invisible to a search. */
{
boolean gotError = FALSE;

errAbort("Sorry, not implemented yet.");

return(gotError);
}

boolean doCheckIndexes()
/* For each search, make sure there's an index on the right table field(s). */
{
boolean gotError = FALSE;

errAbort("Sorry, not implemented yet.");

return(gotError);
}

char *getExampleFor(char *searchName)
/* Randomly choose a search field value -- if it comes from the table,
 * we should be able to search for it and find our way back to the table. */
{
char *example = NULL;

return(example);
}

boolean doMakeExamples()
/* Print out an HTML table of position examples for description.html. */
{
boolean gotError = FALSE;

errAbort("Sorry, not implemented yet.");

return(gotError);
}


int checkHgFindSpec(char *db, char *termToSearch, boolean showSearches,
		    boolean checkTermRegex, char *exampleFor,
		    boolean checkIndexes, boolean makeExamples)
/* Perform searches/checks as specified, summarize errors, 
 * return nonzero if there are errors. */
{
boolean gotError = FALSE;

hSetDb(db);

if (isNotEmpty(termToSearch))
    gotError |= reportSearch(termToSearch);
if (showSearches)
    gotError |= doShowSearches();
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
