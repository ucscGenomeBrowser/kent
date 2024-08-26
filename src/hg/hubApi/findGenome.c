/* findGenome search functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"
#include "asmAlias.h"
#include "asmSummary.h"
#include "assemblyList.h"

struct combinedSummary
/* may have information from any of:  asmSummary, genark or dbDb */
    {
    struct combinedSummary *next;	/* Next in singly linked list */
    struct asmSummary *summary;		/* from asmSummary table */
    struct genark *genArk;		/* from genark table */
    struct dbDb *dbDb;			/* from dbDb table */
    };

/* will be initialized as this function begins */
static char *genarkTable = NULL;
static char *asmListTable = NULL;
static boolean statsOnly = FALSE;
/* these three are radio button states, only one of these three can be TRUE */
static boolean browserMustExist = TRUE;	/* default: browser must exist */
static boolean browserMayExist = FALSE;
static boolean browserNotExist = FALSE;

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
    jsonWriteNumber(jw, "priority", (long long)el->priority);
    jsonWriteString(jw, "commonName", el->commonName);
    jsonWriteString(jw, "scientificName", el->scientificName);
    jsonWriteNumber(jw, "taxId", (long long)el->taxId);
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
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
return (itemCount);
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
sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE)", asmListTable, queryDy->string);
/* add specific browserExists depending upon options */
if (browserMustExist)
    sqlDyStringPrintf(query, " AND browserExists=1");
else if (browserNotExist)
    sqlDyStringPrintf(query, " AND browserExists=0");
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
	sqlDyStringPrintf(query, "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE)", asmListTable, queryDy->string);
	/* add specific browserExists depending upon options */
	if (browserMustExist)
	    sqlDyStringPrintf(query, " AND browserExists=1");
	else if (browserNotExist)
	    sqlDyStringPrintf(query, " AND browserExists=0");
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

struct dyString *query = dyStringNew(64);
sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE)", asmListTable, searchWord);
if (browserMustExist)
    sqlDyStringPrintf(query, " AND browserExists=1");
else if (browserNotExist)
    sqlDyStringPrintf(query, " AND browserExists=0");

long long matchCount = sqlQuickLongLong(conn, query->string);
*prefixSearch = FALSE;	/* assume not */
if (matchCount < 1)	/* no match, add the * wild card match to make a prefix match */
    {
    dyStringFree(&query);
    query = dyStringNew(64);
    sqlDyStringPrintf(query, "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s*' IN BOOLEAN MODE)", asmListTable, searchWord);
    /* add specific browserExists depending upon options */
    if (browserMustExist)
	sqlDyStringPrintf(query, " AND browserExists=1");
    else if (browserNotExist)
	sqlDyStringPrintf(query, " AND browserExists=0");
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
    dyStringFree(&query);
    query = dyStringNew(64);

    sqlDyStringPrintf(query, "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s%s' IN BOOLEAN MODE)", asmListTable, searchWord, *prefixSearch ? "*" : "");
    /* add specific browserExists depending upon options */
    if (browserMustExist)
	sqlDyStringPrintf(query, " AND browserExists=1");
    else if (browserNotExist)
	sqlDyStringPrintf(query, " AND browserExists=0");
    sqlDyStringPrintf(query, " ORDER BY priority LIMIT %d;", maxItemsOutput);
    struct sqlResult *sr = sqlGetResult(conn, query->string);
    itemCount = sqlJsonOut(jw, sr);
    sqlFreeResult(&sr);
    dyStringFree(&query);
    }

return itemCount;
}	/*	static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw, boolean *prefixSearch) */

static long elapsedTime(struct jsonWrite *jw)
{
long nowTime = clock1000();
long elapsedTimeMs = nowTime - enteredMainTime;
jsonWriteNumber(jw, "elapsedTimeMs", elapsedTimeMs);
return elapsedTimeMs;
}

void apiFindGenome(char *pathString[MAX_PATH_INFO])
/* 'findGenome' function */
{
char *searchString = cgiOptionalString(argQ);
char *inputSearchString = cloneString(searchString);
char *endResultSearchString = NULL;
boolean prefixSearch = FALSE;
char *extraArgs = verifyLegalArgs(argFindGenome);
genarkTable = genarkTableName();
asmListTable = assemblyListTableName();

if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /findGenome'%s'", extraArgs);

boolean asmListExists = hTableExists("hgcentraltest", asmListTable);
if (!asmListExists)
    apiErrAbort(err400, err400Msg, "table hgcentraltest.assemblyList does not exist for /findGenome");

boolean asmSummaryExists = hTableExists("hgcentraltest", "asmSummary");
if (!asmSummaryExists)
    apiErrAbort(err400, err400Msg, "table hgcentraltest.asmSummary does not exist for /findGenome");

boolean genArkExists = hTableExists("hgcentraltest", genarkTable);
if (!genArkExists)
    apiErrAbort(err400, err400Msg, "table hgcentraltest.%s does not exist for /findGenome", genarkTable);

char *browserExistString = cgiOptionalString(argBrowser);
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

struct sqlConnection *conn = hConnectCentral();

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

jsonWriteString(jw, argBrowser, browserExistString);

long long itemCount = 0;
long long totalMatchCount = 0;
char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
if (1 == wordCount)
    itemCount = oneWordSearch(conn, words[0], jw, &totalMatchCount, &prefixSearch);
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
    endResultSearchString = inputSearchString;
    jsonWriteString(jw, argQ, inputSearchString);
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

long elapTimeMs = elapsedTime(jw);
/* apache error_log recording */
fprintf(stderr, "findGenome: '%s' found %lld returned %lld in %ld ms\n", endResultSearchString, totalMatchCount, itemCount, elapTimeMs);
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
