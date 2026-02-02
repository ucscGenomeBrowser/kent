/* findGenome search functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"
#include "asmAlias.h"
#include "assemblyList.h"
#include "liftOver.h"

/* will be initialized as this function begins */
static char *genarkTable = NULL;
static char *asmListTable = NULL;
static boolean statsOnly = FALSE;
/* these three are radio button states, only one of these three can be TRUE */
static boolean browserMustExist = TRUE;	/* default: browser must exist */
static boolean browserMayExist = FALSE;
static boolean browserNotExist = FALSE;
static unsigned specificYear = 0;	/* from year=1234 argument */
/* from category= reference or representative */
static char *refSeqCategory = NULL;
/* from status= one of: latest, replaced or suppressed */
static char *versionStatus = NULL;
/* from level= one of complete, chromosome, scaffold or contig */
static char *assemblyLevel = NULL;
static boolean liftable = FALSE;

/*
hgsql -e 'desc assemblyList;' hgcentraltest
+----------------+---------------------+------+-----+---------+-------+
| Field          | Type                | Null | Key | Default | Extra |
+----------------+---------------------+------+-----+---------+-------+
| name           | varchar(255)        | NO   | PRI | NULL    |       |
| priority       | int(10) unsigned    | YES  |     | NULL    |       |
| commonName     | varchar(511)        | YES  |     | NULL    |       |
| scientificName | varchar(511)        | YES  |     | NULL    |       |
| taxId          | int(10) unsigned    | YES  |     | NULL    |       |
| clade          | varchar(255)        | YES  |     | NULL    |       |
| description    | varchar(1023)       | YES  |     | NULL    |       |
| browserExists  | tinyint(3) unsigned | YES  |     | NULL    |       |
| hubUrl         | varchar(511)        | YES  |     | NULL    |       |
+----------------+---------------------+------+-----+---------+-------+
*/

static long long sqlJsonOut(struct jsonWrite *jw, struct sqlResult *sr)
/* given a sqlResult, walk through the rows and output the json */
{
long long itemCount = 0;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct assemblyList *el = assemblyListLoadWithNull(row);
    jsonWriteObjectStart(jw, el->name);
    if (el->priority)
        jsonWriteNumber(jw, "priority", (long long)*el->priority);
    else
        jsonWriteNumber(jw, "priority", (long long)0);
    jsonWriteString(jw, "commonName", el->commonName);
    jsonWriteString(jw, "scientificName", el->scientificName);
    if (el->taxId)
        jsonWriteNumber(jw, "taxId", (long long)*el->taxId);
    else
        jsonWriteNumber(jw, "taxId", (long long)0);
    jsonWriteString(jw, "clade", el->clade);
    jsonWriteString(jw, "description", el->description);
    if (1 == *el->browserExists)
        jsonWriteBoolean(jw, "browserExists", TRUE);
    else
        jsonWriteBoolean(jw, "browserExists", FALSE);
    if (isEmpty(el->hubUrl))
        jsonWriteString(jw, "hubUrl", NULL);
    else
        jsonWriteString(jw, "hubUrl", el->hubUrl);
    if (el->year)
        jsonWriteNumber(jw, "year", (long long)*el->year);
    else
        jsonWriteNumber(jw, "year", (long long)0);
    if (isEmpty(el->refSeqCategory))
        jsonWriteString(jw, "refSeqCategory", NULL);
    else
        jsonWriteString(jw, "refSeqCategory", el->refSeqCategory);
    if (isEmpty(el->versionStatus))
        jsonWriteString(jw, "versionStatus", NULL);
    else
        jsonWriteString(jw, "versionStatus", el->versionStatus);
    if (isEmpty(el->assemblyLevel))
        jsonWriteString(jw, "assemblyLevel", NULL);
    else
        jsonWriteString(jw, "assemblyLevel", el->assemblyLevel);
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
return (itemCount);
}

/* MySQL FULLTEXT indexing has indexed 'words' as broken up by
 *   word break characters, such as in the regular expression: '\W+'
 *   or, in this case, checking the string with isalnum() function,
 *   must all be isalnum()
 * Return: TRUE when there are word breaks
 *         FALSE - the string is all one 'word'
 */
static boolean hasWordBreaks(char *s)
/* Return TRUE if there is any word break in string.
 * allowing - and + characters as first since those are
 *   special characters to the MySQL FULLTEXT search
 * after that, allowing _ * + - as those are special characters
 *  to the MySQL FULLTEXT search
 *  The string has already been checked for the special prefix
 *   characters of: " - +
 *   or the special end character of: *
 */
{
char c;
if (startsWith("-", s) || startsWith("+", s))
    s++;
while ((c = *s++) != 0)
    {
    if (c == '_')	/* TBD: maybe dot . and apostrophe ' */
	continue;
    if (! isalnum(c))
        return TRUE;
    }
return FALSE;
}

static char *quoteWords(char *s)
/* given a string with word break characters, break it up into
 *  a quoted string with the word break characters turned to single space
 */
{
char c;
struct dyString *quoteString = dyStringNew(128);
/* start with the special MySQL characters if present at the beginning */
if (startsWith("-", s) || startsWith("+", s))
    {
    c = *s++;
    dyStringPrintf(quoteString, "%c", c);
    }
/* then continue with the " to start the quoted string */
dyStringPrintf(quoteString, "\"");
int spaceCount = 0;
while ((c = *s++) != 0)
    if (isalnum(c) || c == '_')	/* TBD: maybe dot . and apostrophe ' */
	{
	dyStringPrintf(quoteString, "%c", c);
	spaceCount = 0;
	}
    else
	{
	if (spaceCount)
	    continue;
	dyStringPrintf(quoteString, " ");
	++spaceCount;
	}
dyStringPrintf(quoteString, "\"");
return dyStringCannibalize(&quoteString);
}

static void addBrowserExists(struct dyString *query)
/* add the AND clauses for browserExist depending upon option */
{
if (browserMustExist)
    sqlDyStringPrintf(query, " AND browserExists=1");
else if (browserNotExist)
    sqlDyStringPrintf(query, " AND browserExists=0");
}

static void addCategory(struct dyString *query)
/* refSeqCategory = reference or representative */
{
if (isNotEmpty(refSeqCategory))
    sqlDyStringPrintf(query, " AND refSeqCategory='%s'", refSeqCategory);
}

static void addStatus(struct dyString *query)
/* versionStatus = latest, replaced or suppressed */
{
if (isNotEmpty(versionStatus))
    sqlDyStringPrintf(query, " AND versionStatus='%s'", versionStatus);
}

static void addLevel(struct dyString *query)
/* assemblyLevel = complete, chromosome, scaffold or contig */
{
if (isNotEmpty(assemblyLevel))
    sqlDyStringPrintf(query, " AND assemblyLevel='%s'", assemblyLevel);
}

static void addLiftover(struct dyString *query)
/* liftable = are there liftover chains for this assembly */
{
if (liftable)
    sqlDyStringPrintf(query, " AND exists (select 1 from %s where %s.name = %s.fromDb) ", liftOverChainTable(), asmListTable, liftOverChainTable());
}

static void addConditions(struct dyString *query)
/* add any of the optional conditions */
{
addBrowserExists(query);
addCategory(query);
addStatus(query);
addLevel(query);
addLiftover(query);
if (specificYear > 0)
    sqlDyStringPrintf(query, " AND year='%u'", specificYear);
}

static long long multipleWordSearch(struct sqlConnection *conn, char **words, int wordCount, struct jsonWrite *jw, long long *totalMatchCount)
/* perform search on multiple words, prepare json and return number of matches */
{
long long itemCount = 0;
*totalMatchCount = 0;
if (wordCount < 0)
    return itemCount;

/* get the words[] into a single string */
struct dyString *queryDy = dyStringNew(128);
dyStringPrintf(queryDy, "%s", words[0]);
for (int i = 1; i < wordCount; ++i)
    dyStringPrintf(queryDy, " %s", words[i]);

/* initial SELECT allows any browser exist status, existing or not */
struct dyString *query = dyStringNew(64);
sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s ", asmListTable);
sqlDyStringPrintf(query, "WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE)", queryDy->string);
addConditions(query);	/* add optional SELECT options */

long long matchCount = sqlQuickLongLong(conn, query->string);
if (matchCount > 0)
    {
    *totalMatchCount = matchCount;
    if (statsOnly)	// only counting, nothing returned
	{	// the LIMIT would limit results to maxItemsOutput
	itemCount = min(maxItemsOutput, matchCount);
	}	// when less than totalMatchCount
    else
	{
	dyStringFree(&query);
	query = dyStringNew(64);
	sqlDyStringPrintf(query, "SELECT * FROM %s ", asmListTable);
        sqlDyStringPrintf(query, "WHERE MATCH(name, commonName, scientificName, clade, description, refSeqCategory, versionStatus, assemblyLevel) AGAINST ('%s' IN BOOLEAN MODE)", queryDy->string);
	addConditions(query);	/* add optional SELECT options */
	sqlDyStringPrintf(query, " ORDER BY priority LIMIT %d;", maxItemsOutput);
	struct sqlResult *sr = sqlGetResult(conn, query->string);
	itemCount = sqlJsonOut(jw, sr);
	sqlFreeResult(&sr);
	dyStringFree(&query);
	}
    }
return itemCount;
}

static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw, long long *totalMatchCount, boolean *prefixSearch)
/* perform search on a single word, prepare json and return number of matches
 *   and number of potential matches totalMatchCount
 */
{
long long itemCount = 0;
*totalMatchCount = 0;

struct dyString *query = sqlDyStringCreate("SELECT COUNT(*) FROM %s ", asmListTable);
sqlDyStringPrintf(query, "WHERE MATCH(name, commonName, scientificName, clade, description, refSeqCategory, versionStatus, assemblyLevel) AGAINST ('%s' IN BOOLEAN MODE)", searchWord);
addConditions(query);	/* add optional SELECT options */

long long matchCount = sqlQuickLongLong(conn, query->string);
*prefixSearch = FALSE;	/* assume not */
if (matchCount < 1)	/* no match, add the * wild card match to make a prefix match */
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s ", asmListTable);
    sqlDyStringPrintf(query, "WHERE MATCH(name, commonName, scientificName, clade, description, refSeqCategory, versionStatus, assemblyLevel) AGAINST ('%s*' IN BOOLEAN MODE)", searchWord);
    addConditions(query);	/* add optional SELECT options */
    matchCount = sqlQuickLongLong(conn, query->string);
    if (matchCount > 0)
	*prefixSearch = TRUE;
    }
if (matchCount < 1)	// nothing found, returning zero
    return itemCount;
*totalMatchCount = matchCount;

if (statsOnly)	// only counting, nothing returned
    {	// the LIMIT would limit results to maxItemsOutput
    itemCount = min(maxItemsOutput, matchCount);
    }	// when less than totalMatchCount
else
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "SELECT * FROM %s ", asmListTable);
    sqlDyStringPrintf(query, "WHERE MATCH(name, commonName, scientificName, clade, description, refSeqCategory, versionStatus, assemblyLevel) AGAINST ('%s%s' IN BOOLEAN MODE)", searchWord, *prefixSearch ? "*" : "");
    addConditions(query);	/* add optional SELECT options */
    sqlDyStringPrintf(query, " ORDER BY priority LIMIT %d;", maxItemsOutput);
    struct sqlResult *sr = sqlGetResult(conn, query->string);
    itemCount = sqlJsonOut(jw, sr);
    sqlFreeResult(&sr);
    dyStringFree(&query);
    }

return itemCount;
}	/*	static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw, boolean *prefixSearch) */

#ifdef NOT
// disabled 2025-10-22
static long elapsedTime(struct jsonWrite *jw)
{
long nowTime = clock1000();
long elapsedTimeMs = nowTime - enteredMainTime;
jsonWriteNumber(jw, "elapsedTimeMs", elapsedTimeMs);
return elapsedTimeMs;
}
#endif

void apiFindGenome(char *pathString[MAX_PATH_INFO])
/* 'findGenome' function */
{
char *searchString = cgiOptionalString(argQ);
char *inputSearchString = cloneString(searchString);
char *endResultSearchString = inputSearchString;
boolean prefixSearch = FALSE;
char *extraArgs = verifyLegalArgs(argFindGenome);
genarkTable = genarkTableName();
asmListTable = assemblyListTableName();
struct sqlConnection *conn = hConnectCentral();

if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /findGenome'%s'", extraArgs);

boolean asmListExists = sqlTableExists(conn, asmListTable);
if (!asmListExists)
    apiErrAbort(err400, err400Msg, "table central.assemblyList does not exist for /findGenome");

boolean genArkExists = sqlTableExists(conn, genarkTable);
if (!genArkExists)
    apiErrAbort(err400, err400Msg, "table central.%s does not exist for /findGenome", genarkTable);

char *yearString = cgiOptionalString(argYear);
char *categoryString = cgiOptionalString(argCategory);
char *statusString = cgiOptionalString(argStatus);
char *levelString = cgiOptionalString(argLevel);
char *liftableStr = cgiOptionalString(argLiftable);
/* protect sqlUnsigned from errors */
if (isNotEmpty(yearString))
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
        specificYear = sqlUnsigned(yearString);
        if ((specificYear < 1800) || (specificYear > 2100))
	    apiErrAbort(err400, err400Msg, "year specified '%s' must be >= 1800 and <= 2100", yearString);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	apiErrAbort(err400, err400Msg, "can not recognize year '%s' as a number", yearString);
    }
/* probably be better to place this arg checking business into a function
 * operating from a list
 */
if (isNotEmpty(categoryString))
    {
    refSeqCategory = cloneString(categoryString);
    toLowerN(refSeqCategory, strlen(refSeqCategory));
    if (differentWord(refSeqCategory, "reference"))
	{
        if (differentWord(refSeqCategory, "representative"))
	    apiErrAbort(err400, err400Msg, "values for argument %s=%s must be 'reference' or 'representative'", argCategory, categoryString);
	}
    }
if (isNotEmpty(statusString))
    {
    versionStatus = cloneString(statusString);
    toLowerN(versionStatus, strlen(versionStatus));
    if (differentWord(versionStatus, "latest"))
	{
        if (differentWord(versionStatus, "replaced"))
	    if (differentWord(versionStatus, "suppressed"))
		apiErrAbort(err400, err400Msg, "values for argument %s=%s must be one of: 'latest', 'replaced' or 'suppressed'", argStatus, statusString);
	}
    }
if (isNotEmpty(levelString))
    {
    assemblyLevel = cloneString(levelString);
    toLowerN(assemblyLevel, strlen(assemblyLevel));
    if (differentWord(assemblyLevel, "complete"))
	{
	if (differentWord(assemblyLevel, "chromosome"))
	    if (differentWord(assemblyLevel, "scaffold"))
		if (differentWord(assemblyLevel, "contig"))
		    apiErrAbort(err400, err400Msg, "values for argument %s=%s must be one of: 'complete', 'chromosome', 'scaffold' or 'contig'", argLevel, levelString);
	}
    }

if (isNotEmpty(liftableStr))
    {
    char *lower = cloneString(liftableStr);
    tolowers(lower);
    if (sameWord(lower, "liftable") || sameWord(lower, "true") || sameWord(lower, "yes") ||sameWord(lower, "on"))
        liftable = TRUE;
    else if (sameWord(lower, "false") || sameWord(lower, "no") || sameWord(lower, "off"))
        liftable = FALSE;
    else
        apiErrAbort(err400, err400Msg, "unrecognized '%s=%s' argument, must be either 'liftable' or 'true', or completely missing", argLiftable, liftableStr);
    }

char *browserExistString = cgiOptionalString(argBrowser);
if (NULL == browserExistString)	/* set default if none given */
    browserExistString = cloneString("mustExist");

if (isNotEmpty(browserExistString))
    {	/* from radio buttons, only one can be on */
    if (sameWord(browserExistString, "mustExist"))
	{
	browserMustExist = TRUE;	/* default: browser must exist */
        browserMayExist = FALSE;
        browserNotExist = FALSE;
	}
    else if (sameWord(browserExistString, "mayExist"))
	{
	browserMustExist = FALSE;
        browserMayExist = TRUE;
        browserNotExist = FALSE;
	}
    else if (sameWord(browserExistString, "notExist"))
	{
	browserMustExist = FALSE;
        browserMayExist = FALSE;
        browserNotExist = TRUE;
	}
     else
	apiErrAbort(err400, err400Msg, "unrecognized '%s=%s' argument, must be one of: mustExist, mayExist or notExist", argBrowser, browserExistString);
    }

char *statsOnlyString = cgiOptionalString(argStatsOnly);
if (isNotEmpty(statsOnlyString))
    {
    if (SETTING_IS_ON(statsOnlyString))
	statsOnly = TRUE;
    else if (SETTING_IS_OFF(statsOnlyString))
	statsOnly = FALSE;
    else
	apiErrAbort(err400, err400Msg, "unrecognized '%s=%s' argument, can only be =1 or =0", argStatsOnly, statsOnlyString);
    }


if (!sqlTableExists(conn, asmListTable))
    apiErrAbort(err500, err500Msg, "missing central.assemblyList table in function /findGenome'%s'", extraArgs);

int wordCount = 0;

/* verify number of words in search string is legal */
wordCount = chopByWhite(searchString, NULL, 0);

if (wordCount < 1)
apiErrAbort(err400, err400Msg, "search term '%s' does not contain a word ? for function /findGenome", argQ);
if (wordCount > 5)
apiErrAbort(err400, err400Msg, "search term '%s=%s' should not have more than 5 words for function /findGenome", argQ, searchString);

struct jsonWrite *jw = apiStartOutput();

/* show options in effect in JSON return */

jsonWriteString(jw, argBrowser, browserExistString);
if (specificYear > 0)
    jsonWriteNumber(jw, argYear, specificYear);
if (isNotEmpty(refSeqCategory))
    jsonWriteString(jw, argCategory, refSeqCategory);
if (isNotEmpty(versionStatus))
    jsonWriteString(jw, argStatus, versionStatus);
if (isNotEmpty(assemblyLevel))
    jsonWriteString(jw, argLevel, assemblyLevel);
jsonWriteString(jw, argLiftable, liftableStr);

long long itemCount = 0;
long long totalMatchCount = 0;
char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
if (1 == wordCount)
    {
    boolean doQuote = TRUE;
    /* already quoted, let it go as-is */
    if (startsWith("\"", words[0]) && endsWith(words[0],"\""))
	doQuote = FALSE;
    /* already wildcard, let it go as-is */
    if (endsWith(words[0],"*"))
	doQuote = FALSE;
    if (doQuote && hasWordBreaks(words[0]))
	{
	char *quotedWords = quoteWords(words[0]);
	endResultSearchString = quotedWords;
	itemCount = oneWordSearch(conn, quotedWords, jw, &totalMatchCount, &prefixSearch);
	} else {
	itemCount = oneWordSearch(conn, words[0], jw, &totalMatchCount, &prefixSearch);
	}
    }
else	/* multiple word search */
    itemCount = multipleWordSearch(conn, words, wordCount, jw, &totalMatchCount);

if (prefixSearch)
    {
    struct dyString *addedStar = dyStringNew(64);
    dyStringPrintf(addedStar, "%s*", inputSearchString);
    endResultSearchString = dyStringCannibalize(&addedStar);
    jsonWriteString(jw, argQ, endResultSearchString);
    }
else
    {
    jsonWriteString(jw, argQ, endResultSearchString);
    }

/* rules about what can be in the search string:
 *  + sign before a word indicates the word must be in the result
 *  - sign before a word indicates it must not be in the result
 *  * at end of word makes the word be a prefix search
 *  "double quotes" to group words together as a phrase to match exactly
 *  < or > adjust the words contribution to the relevance value
 *          >moreImportant  <lessImportant
 *  ~ negates the word's contribution to the relevance value without
 *    excluding it from the results
 *  (parens clauses) to groups words together for more complex queries
 *  | OR operator 'thisWord | otherWord'
 */

struct dyString *query = dyStringNew(64);
sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s", asmListTable);
long long universeCount = sqlQuickLongLong(conn, query->string);
dyStringFree(&query);

// disabled 2025-10-22 long elapTimeMs = elapsedTime(jw);
/* apache error_log recording */
// disabled 2025-10-22 fprintf(stderr, "findGenome: '%s' found %lld returned %lld in %ld ms\n", endResultSearchString, totalMatchCount, itemCount, elapTimeMs);
if (statsOnly)
    jsonWriteBoolean(jw, "statsOnly", TRUE);
if (itemCount)
    {
    jsonWriteNumber(jw, "itemCount", itemCount);
    jsonWriteNumber(jw, "totalMatchCount", totalMatchCount);
    jsonWriteNumber(jw, "availableAssemblies", universeCount);
    if (totalMatchCount > itemCount)
	jsonWriteBoolean(jw, "maxItemsLimit", TRUE);
    apiFinishOutput(0, NULL, jw);
    }
else
    apiErrAbort(err400, err400Msg, "no genomes found matching search term %s='%s' for endpoint: /findGenome", argQ, inputSearchString);

hDisconnectCentral(&conn);

}
