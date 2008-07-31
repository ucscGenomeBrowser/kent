/* hgFindSpecCustom - custom (not autoSQL generated) code for working
 * with hgFindSpec.  This code is concerned with making the hgFindSpec
 * MySQL table out of the trackDb.ra files. */

#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "hgFindSpec.h"
#include "hdb.h"
#include "hui.h"
#include "ra.h"
#include "hash.h"
#include "obscure.h"
#include <regex.h>
#include "trackDb.h"

static char const rcsid[] = "$Id: hgFindSpecCustom.c,v 1.14.68.1 2008/07/31 02:24:30 markd Exp $";

/* ----------- End of AutoSQL generated code --------------------- */

#define REGEX_OPTIONS (REG_NOSUB | REG_EXTENDED | REG_ICASE)
#define REGEX_SUBSTR_OPTIONS (REG_EXTENDED | REG_ICASE)

static regex_t *compileRegex(char *exp, char *what, int compileFlags)
/* Compile exp (or die with an informative-as-possible error message). 
 * Cache pre-compiled regex's internally (so don't free result after use). */
{
static struct hash *reHash = NULL;
struct hashEl *hel = NULL;

if (reHash == NULL)
    reHash = newHash(10);
hel = hashLookup(reHash, exp);
if (hel != NULL)
    return((regex_t *)hel->val);
else
    {
    regex_t *compiledExp = NULL;
    int errNum = 0;
    AllocVar(compiledExp);
    errNum = regcomp(compiledExp, exp, compileFlags);
    if (errNum != 0)
	{
	char errBuf[512];
	regerror(errNum, compiledExp, errBuf, sizeof(errBuf));
	errAbort("%s \"%s\" got regular expression compilation error %d:\n%s\n",
		 what, exp, errNum, errBuf);
	}
    hashAdd(reHash, exp, compiledExp);
    return(compiledExp);
    }
}

boolean matchRegex(char *name, char *exp)
/* Return TRUE if name matches the regular expression pattern
 * (case insensitive). */
{
regex_t *compiledExp = compileRegex(exp, "Regular expression", REGEX_OPTIONS);
return(regexec(compiledExp, name, 0, NULL, 0) == 0);
}

boolean matchRegexSubstr(char *name, char *exp, regmatch_t substrArr[],
			 size_t substrArrSize)
/* Return TRUE if name matches exp (case insensitive); regexec fills in 
 * substrArr with substring offsets. */
{
regex_t *compiledExp = compileRegex(exp, "Regular expression w/substrings",
				    REGEX_SUBSTR_OPTIONS);
return(regexec(compiledExp, name, substrArrSize, substrArr, 0) == 0);
}


static void anchorTermRegex(struct hgFindSpec *hfs)
/* termRegex must match the whole term.  If it doesn't already start with 
 * ^ and end in $, add those (no need to make the trackDb.ra file even 
 * harder to read with those extra magic chars :). */
{
if (isNotEmpty(hfs->termRegex))
    {
    char *orig = hfs->termRegex;
    char first = orig[0];
    char last  = orig[strlen(orig)-1];
    char buf[512];
    safef(buf, sizeof(buf), "%s%s%s",
	  (first == '^') ? "" : "^",
	  orig,
	  (last  == '$') ? "" : "$");
    freeMem(hfs->termRegex);
    hfs->termRegex = cloneString(buf);
    }
else if (hfs->termRegex == NULL)
    hfs->termRegex = "";
}


static void checkTermRegex(struct hgFindSpec *hfs)
/* Make sure termRegex compiles OK. */
{
if (isNotEmpty(hfs->termRegex))
    {
    regex_t *compiledExp = NULL;
    char buf[256];
    safef(buf, sizeof(buf), "hfsPolish: search %s: termRegex", hfs->searchName);
    compiledExp = compileRegex(hfs->termRegex, buf, REGEX_OPTIONS);
    }
}

static void escapeTermRegex(struct hgFindSpec *hfs)
/* Escape any '\' characters in termRegex for sql storage. */
{
if (isNotEmpty(hfs->termRegex))
    {
    char *orig = hfs->termRegex;
    hfs->termRegex = makeEscapedString(orig, '\\');
    freeMem(orig);
    }
}


static char *genePredDefaultFormat =
	"select chrom,txStart,txEnd,name from %s where name like ";
static char *bedDefaultFormat =
	"select chrom,chromStart,chromEnd,name from %s where name like ";
static char *pslDefaultFormat =
	"select tName,tStart,tEnd,qName from %s where qName like ";
static char *exactTermFormat = "'%s'";
static char *prefixTermFormat = "'%s%%'";
static char *fuzzyTermFormat = "'%%%s%%'";

static char *getQueryFormat(struct hgFindSpec *hfs)
/* Fill in query format from searchType if necessary. */
{
char *queryFormat = hfs->query;
char buf[256];

if (isEmpty(queryFormat))
    {
    char *baseFmt = "";
    char *termFmt = "";
    buf[0] = 0;
    if (sameString(hfs->searchType, "genePred"))
	baseFmt = genePredDefaultFormat;
    else if (sameString(hfs->searchType, "bed"))
	baseFmt = bedDefaultFormat;
    else if (sameString(hfs->searchType, "psl"))
	baseFmt = pslDefaultFormat;
    if (isNotEmpty(baseFmt))
	{
	if (isNotEmpty(hfs->xrefQuery))
	    termFmt = exactTermFormat;
	else if (sameString(hfs->searchMethod, "fuzzy"))
	    termFmt = fuzzyTermFormat;
	else if (sameString(hfs->searchMethod, "prefix"))
	    termFmt = prefixTermFormat;
	else
	    termFmt = exactTermFormat;
	}
    safef(buf, sizeof(buf), "%s%s", baseFmt, termFmt);
    queryFormat = cloneString(buf);
    }
return(queryFormat);
}

static char *queryFormatRegex =
    "^select [[:alnum:]]+, ?[[:alnum:]]+, ?[[:alnum:]]+, ?[[:alnum:]]+ "
    "from %s where [[:alnum:]]+ (r?like|=) ['\"]?.*%s.*['\"]?$";
static char *exactTermFormatRegex = "['\"]?.*%s.*['\"]?$";
static char *prefixTermFormatRegex = "['\"]?%s.*%%['\"]?$";

static void checkQueryFormat(struct hgFindSpec *hfs)
/* Make sure query looks right and jives with searchMethod. */
{
if (isNotEmpty(hfs->query) && !hgFindSpecSetting(hfs, "dontCheckQueryFormat"))
    {
    if (! matchRegex(hfs->query, queryFormatRegex))
	errAbort("hfsPolish: search %s: query needs to be of the format "
		 "\"select field1,field2,field3,field4 from %%s "
		 "where field4 like '%%s'\" "
		 "(for prefix, '%%s%%%%'; for fuzzy, '%%%%%%s%%%%'), "
		 "but instead is this:\n%s",
		 hfs->searchName, hfs->query);
    if (isNotEmpty(hfs->xrefQuery))
	{
	if (!matchRegex(hfs->query, exactTermFormatRegex))
	    errAbort("hfsPolish: search %s: there is an xrefQuery so query "
		     "needs to end with %s (exact match to xref results).",
		     hfs->searchName, exactTermFormat);
	}
    else
	{
	if (sameString(hfs->searchMethod, "fuzzy") &&
	    !endsWith(hfs->query, fuzzyTermFormat))
	    errAbort("hfsPolish: search %s: searchMethod is fuzzy so query "
		     "needs to end with %s.",
		     hfs->searchName, fuzzyTermFormat);
	else if (sameString(hfs->searchMethod, "prefix") &&
		 !matchRegex(hfs->query, prefixTermFormatRegex))
	    errAbort("hfsPolish: search %s: searchMethod is prefix so query "
		     "needs to end with %s.",
		     hfs->searchName, prefixTermFormat);
	
	else if (sameString(hfs->searchMethod, "exact") &&
		 !matchRegex(hfs->query, exactTermFormatRegex))
	    errAbort("hfsPolish: search %s: searchMethod is exact so query "
		     "needs to end with %s.",
		     hfs->searchName, exactTermFormat);
	}
    }
}

static char *xrefQueryFormatRegex =
    "select [[:alnum:]]+, ?[[:alnum:]]+(\\([^)]+\\))? from %s where [[:alnum:]]+ (like|=) ['\"]?[%s]+['\"]?";

static void checkXrefQueryFormat(struct hgFindSpec *hfs)
/* Make sure xrefQuery looks right and jives with searchMethod. */
{
if (isNotEmpty(hfs->xrefQuery) &&
    !hgFindSpecSetting(hfs, "dontCheckXrefQueryFormat"))
    {
    if (! matchRegex(hfs->xrefQuery, xrefQueryFormatRegex))
	errAbort("hfsPolish: search %s: xrefQuery needs to be of the format "
		 "\"select field1,field2 from %%s where field2 like '%%s'\" "
		 "(for prefix, '%%s%%%%'; for exact, '%%%%%%s%%%%'), "
		 "but instead is this:\n%s",
		 hfs->searchName, hfs->xrefQuery);
    if (sameString(hfs->searchMethod, "fuzzy") &&
	!endsWith(hfs->xrefQuery, fuzzyTermFormat))
	errAbort("hfsPolish: search %s: searchMethod is fuzzy so xrefQuery "
		 "needs to end with %s.",
		 hfs->searchName, fuzzyTermFormat);
    else if (sameString(hfs->searchMethod, "prefix") &&
	     !matchRegex(hfs->xrefQuery, prefixTermFormatRegex))
	errAbort("hfsPolish: search %s: searchMethod is prefix so xrefQuery "
		 "needs to end with %s.",
		 hfs->searchName, prefixTermFormat);
	
    else if (sameString(hfs->searchMethod, "exact") &&
	     !matchRegex(hfs->xrefQuery, exactTermFormatRegex))
	errAbort("hfsPolish: search %s: searchMethod is exact so xrefQuery "
		 " needs to end with %s.",
		 hfs->searchName, exactTermFormat);
    }
}


static void hgFindSpecPolish(char *db, struct hgFindSpec *hfs)
/* Fill in missing values with defaults, check for consistency. */
{
/* At least one of {searchName, searchTable} must be defined. */
if ((hfs->searchName == NULL) && (hfs->searchTable == NULL))
    errAbort("hfsPolish: searchName or searchTable must be defined.\n");
if (hfs->searchName == NULL)
    hfs->searchName = cloneString(hfs->searchTable);
if (hfs->searchTable == NULL)
    hfs->searchTable = cloneString(hfs->searchName);
/* If searchType is not defined, query must be defined. */
if (hfs->searchType == NULL && hfs->query == NULL)
    errAbort("hfsPolish: search %s: if searchType is not defined, "
	     "then query must be defined.\n",
	     hfs->searchName);
/* If one of {xrefTable,xrefQuery} is defined, both must be. */
if ((hfs->xrefTable == NULL) ^ (hfs->xrefQuery == NULL))
    errAbort("hfsPolish: search %s: can't define xrefTable without xrefQuery "
	     "or vice versa.\n",
	     hfs->searchName);
if (hfs->searchMethod == NULL)
    hfs->searchMethod = cloneString("exact");
if (hfs->searchType == NULL)
    hfs->searchType = "";
anchorTermRegex(hfs);
checkTermRegex(hfs);
escapeTermRegex(hfs);
if (hfs->query == NULL)
    hfs->query = getQueryFormat(hfs);
checkQueryFormat(hfs);
checkXrefQueryFormat(hfs);
if (hfs->xrefTable == NULL)
    hfs->xrefTable = "";
if (hfs->xrefQuery == NULL)
    hfs->xrefQuery = "";
if (hfs->searchPriority == 0.0)
    hfs->searchPriority = 1000.0;
if (hfs->searchDescription == NULL)
    {
    char buf[512];
    struct sqlConnection *conn = hAllocConn(db);
    struct trackDb *tdb = hMaybeTrackInfo(conn, hfs->searchTable);
    hFreeConn(&conn);
    if (tdb != NULL)
	safef(buf, sizeof(buf), "%s (%s)", tdb->longLabel, hfs->searchTable);
    else
	safef(buf, sizeof(buf), "%s", hfs->searchTable);
    hfs->searchDescription = cloneString(buf);
    }
if (hfs->searchSettings == NULL)
    hfs->searchSettings = cloneString("");
}

int hgFindSpecCmp(const void *va, const void *vb)
/* Compare to sort based on searchPriority. */
{
const struct hgFindSpec *a = *((struct hgFindSpec **)va);
const struct hgFindSpec *b = *((struct hgFindSpec **)vb);
float dif = a->searchPriority - b->searchPriority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

static void hgFindSpecAddInfo(struct hgFindSpec *hfs, char *var, char *value)
/* Add info from a variable/value pair to hgFindSpec. */
{
if (sameString(var, "searchName"))
    hfs->searchName = cloneString(value);
else if (sameString(var, "searchTable"))
    hfs->searchTable = cloneString(value);
else if (sameString(var, "searchMethod"))
    hfs->searchMethod = cloneString(value);
else if (sameString(var, "searchType"))
    hfs->searchType = cloneString(value);
else if (sameWord(var, "shortCircuit"))
    hfs->shortCircuit = TRUE;
else if (sameString(var, "termRegex"))
    hfs->termRegex = cloneString(value);
else if (sameString(var, "query"))
    hfs->query = cloneString(value);
else if (sameString(var, "xrefTable"))
    hfs->xrefTable = cloneString(value);
else if (sameString(var, "xrefQuery"))
    hfs->xrefQuery = cloneString(value);
else if (sameString(var, "searchPriority"))
    hfs->searchPriority = atof(value);
else if (sameString(var, "searchDescription"))
    hfs->searchDescription = cloneString(value);
else	/* Add to settings. */
    {
    if (hfs->settingsHash == NULL)
	hfs->settingsHash = hashNew(7);
    hashAdd(hfs->settingsHash, var, cloneString(value));
    if (sameWord(var, "semiShortCircuit"))
	hfs->shortCircuit = TRUE;
    }
}

struct hgFindSpec *hgFindSpecFromRa(char *db, char *raFile)
/* Load track info from ra file into list. */
{
static boolean reEntered = FALSE;
struct lineFile *lf = lineFileOpen(raFile, TRUE);
char *line, *word;
struct hgFindSpec *hfsList = NULL, *hfs;
boolean done = FALSE;
char *incFile;

for (;;)
    {
    /* Seek to next line that starts with 'searchName' or 'searchTable' */
    for (;;)
	{
	if (!lineFileNext(lf, &line, NULL))
	   {
	   done = TRUE;
	   break;
	   }
	if (startsWith("searchName", line) || startsWith("searchTable", line))
	   {
	   lineFileReuse(lf);
	   break;
	   }
        else if ((incFile = trackDbInclude(raFile, line)) != NULL)
            {
	    /* Set reEntered=TRUE whenever we recurse, so we don't polish
	     * multiple times and get too many backslash-escapes. */
	    boolean reBak = reEntered;
	    reEntered = TRUE;
            struct hgFindSpec *incHfs = hgFindSpecFromRa(db, incFile);
	    reEntered = reBak;
            hfsList = slCat(hfsList, incHfs);
            }
	}
    if (done)
        break;

    /* Allocate track structure and fill it in until next blank line. */
    AllocVar(hfs);
    slAddHead(&hfsList, hfs);
    for (;;)
        {
	/* Break at blank line or EOF. */
	if (!lineFileNext(lf, &line, NULL))
	    break;
	line = skipLeadingSpaces(line);
	if (line == NULL || line[0] == 0)
	    break;

	/* Skip comments. */
	if (line[0] == '#')
	    continue;

	/* Parse out first word and decide what to do. */
	word = nextWord(&line);
	if (line == NULL)
	    errAbort("No value for %s line %d of %s",
		     word, lf->lineIx, lf->fileName);
	line = trimSpaces(line);
	hgFindSpecAddInfo(hfs, word, line);
	}
    }
lineFileClose(&lf);
if (! reEntered)
    {
    for (hfs = hfsList; hfs != NULL; hfs = hfs->next)
	{
	hgFindSpecPolish(db, hfs);
	}
    }
slReverse(&hfsList);
return hfsList;
}


char *hgFindSpecSetting(struct hgFindSpec *hfs, char *name)
/* Return setting string or NULL if none exists. */
{
if (hfs == NULL)
    errAbort("Program error: null hfs passed to hgFindSpecSetting.");
if (hfs->settingsHash == NULL)
    hfs->settingsHash = raFromString(hfs->searchSettings);
return hashFindVal(hfs->settingsHash, name);
}

char *hgFindSpecRequiredSetting(struct hgFindSpec *hfs, char *name)
/* Return setting string or squawk and die. */
{
char *ret = hgFindSpecSetting(hfs, name);
if (ret == NULL)
   errAbort("Missing required %s setting in %s (%s) search spec",
	    name, hfs->searchTable, hfs->searchName);
return ret;
}

char *hgFindSpecSettingOrDefault(struct hgFindSpec *hfs, char *name,
				 char *defaultVal)
/* Return setting string, or defaultVal if none exists */
{
    char *val = hgFindSpecSetting(hfs, name);
    return (val == NULL ? defaultVal : val);
}

static struct slName *hgFindSpecNameList(char *db)
/* Return the hgFindSpec table name(s) to use (based on trackDb name). */
{
struct slName *trackDbList = hTrackDbList();
struct slName *specNameList = NULL;
struct slName *tdbName;
for (tdbName = trackDbList; tdbName != NULL; tdbName = tdbName->next)
    {
    char *subbed = replaceChars(tdbName->name, "trackDb", "hgFindSpec");
    if (hTableExists(db, subbed))
	slNameAddHead(&specNameList, subbed);
    freez(&subbed);
    }
if (!specNameList)
    specNameList = slNameNew("hgFindSpec");
else
    slReverse(&specNameList);
return specNameList;
}

static boolean haveSpecAlready(struct hgFindSpec *list, struct hgFindSpec *spec)
/* Simply check to see if we have this search in our list already. */
{
struct hgFindSpec *cur = list;
while ((cur != NULL) && (!sameString(cur->searchName, spec->searchName)))
    cur = cur->next;
return (cur) ? TRUE : FALSE;
}

struct hgFindSpec *hgFindSpecGetSpecs(char *db, boolean shortCircuit)
/* Load all short-circuit (or not) search specs from the current db, sorted by 
 * searchPriority. */
{
struct hgFindSpec *hfsList = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct slName *hgFindSpecList = hgFindSpecNameList(db);
struct slName *oneSpec;
for (oneSpec = hgFindSpecList; oneSpec != NULL; oneSpec = oneSpec->next)
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    /* Descending order, then slAddHead --> correct order out. */
    safef(query, sizeof(query),
	  "select * from %s where shortCircuit = %d order by searchPriority desc",
	  oneSpec->name, shortCircuit);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct hgFindSpec *hfs = hgFindSpecLoad(row);
	if (!haveSpecAlready(hfsList, hfs))
	    slAddHead(&hfsList, hfs);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return(hfsList);
}

void hgFindSpecGetAllSpecs(char *db, 
                           struct hgFindSpec **retShortCircuitList,
			   struct hgFindSpec **retAdditiveList)
/* Load all search specs from the current db, separated according to 
 * shortCircuit and sorted by searchPriority. */
{
struct hgFindSpec *shortList = NULL, *longList = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct slName *hgFindSpecList = hgFindSpecNameList(db);
struct slName *oneSpec;
for (oneSpec = hgFindSpecList; oneSpec != NULL; oneSpec = oneSpec->next)
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    /* Descending order, then slAddHead --> correct order out. */
    safef(query, sizeof(query),
	  "select * from %s order by searchPriority desc",
	  oneSpec->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct hgFindSpec *hfs = hgFindSpecLoad(row);	
	if ((hfs->shortCircuit)	&& (!haveSpecAlready(shortList, hfs)))
	    slAddHead(&shortList, hfs);
	else if (!haveSpecAlready(longList, hfs))
	    slAddHead(&longList, hfs);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
if (retShortCircuitList != NULL)
    *retShortCircuitList = shortList;
else
    hgFindSpecFreeList(&shortList);
if (retAdditiveList != NULL)
    *retAdditiveList = longList;
else
    hgFindSpecFreeList(&longList);
}
