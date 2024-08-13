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
static boolean allowAll = FALSE;	/* default only show existing browsers*/

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
int itemCount = 0;
char **row;
if (statsOnly)	// only counting, nothing returned
    {
    while ((row = sqlNextRow(sr)) != NULL)
        ++itemCount;
    }
else
    {
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

char query[4096];
if (allowAll)
    sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE);", asmListTable, queryDy->string);
else
    sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE) AND browserExists=1;", asmListTable, queryDy->string);
long long matchCount = sqlQuickLongLong(conn, query);
if (matchCount > 0)
    {
    verbose(1, "DBG: matchCount: %lld from search '%s'\n", matchCount, query);
    *totalMatchCount = matchCount;
    if (allowAll)
	sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE) ORDER BY priority LIMIT %d;", asmListTable, queryDy->string, maxItemsOutput);
    else
	sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE) AND browserExists=1 ORDER BY priority LIMIT %d;", asmListTable, queryDy->string, maxItemsOutput);
    struct sqlResult *sr = sqlGetResult(conn, query);
    itemCount = sqlJsonOut(jw, sr);
    verbose(1, "DBG: itemCount: %lld from search '%s'\n", itemCount, query);
    sqlFreeResult(&sr);
    }
return itemCount;
}

static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw, long long *totalMatchCount)
/* perform search on a single word, prepare json and return number of matches
 *   and number of potential matches totalMatchCount
 */
{
char query[4096];
long long itemCount = 0;
*totalMatchCount = 0;

if (allowAll)
    sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE);", asmListTable, searchWord);
else
    sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE) AND browserExists=1;", asmListTable, searchWord);

verbose(1, "DBG '%s'\n", query);
long long matchCount = sqlQuickLongLong(conn, query);
boolean prefixSearch = FALSE;
if (matchCount < 1)	/* no match, add the * wild card match to make a prefix match */
    {
    if (allowAll)
        sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s*' IN BOOLEAN MODE);", asmListTable, searchWord);
    else
        sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s*' IN BOOLEAN MODE) AND browserExists=1;", asmListTable, searchWord);
    matchCount = sqlQuickLongLong(conn, query);
    if (matchCount > 0)
	prefixSearch = TRUE;
    }
if (matchCount < 1)
    return itemCount;
*totalMatchCount = matchCount;

if (allowAll)
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s%s' IN BOOLEAN MODE) ORDER BY priority LIMIT %d;", asmListTable, searchWord, prefixSearch ? "*" : "", maxItemsOutput);
else
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s%s' IN BOOLEAN MODE) AND browserExists=1 ORDER BY priority LIMIT %d;", asmListTable, searchWord, prefixSearch ? "*" : "", maxItemsOutput);
struct sqlResult *sr = sqlGetResult(conn, query);
itemCount = sqlJsonOut(jw, sr);
sqlFreeResult(&sr);

return itemCount;
}	/*	static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw) */

static void elapsedTime(struct jsonWrite *jw)
{
long nowTime = clock1000();
long elapsedTimeMs = nowTime - enteredMainTime;
jsonWriteNumber(jw, "elapsedTimeMs", elapsedTimeMs);
}

void apiFindGenome(char *pathString[MAX_PATH_INFO])
/* 'findGenome' function */
{
char *searchString = cgiOptionalString(argGenomeSearchTerm);
char *inputSearchString = cloneString(searchString);
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

char *allowAllString = cgiOptionalString(argAllowAll);
if (isNotEmpty(allowAllString))
    {
    if (sameString("1", allowAllString))
	allowAll = TRUE;
    else if (sameString("0", allowAllString))
	allowAll = FALSE;
    else
	apiErrAbort(err400, err400Msg, "unrecognized '%s=%s' argument, can only be =1 or =0", argAllowAll, allowAllString);
    }

char *statsOnlyString = cgiOptionalString(argStatsOnly);
if (isNotEmpty(statsOnlyString))
    {
    if (sameString("1", statsOnlyString))
	statsOnly = TRUE;
    else if (sameString("0", statsOnlyString))
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
apiErrAbort(err400, err400Msg, "search term '%s' does not contain a word ? for function /findGenome", argGenomeSearchTerm);
if (wordCount > 5)
apiErrAbort(err400, err400Msg, "search term '%s=%s' should not have more than 5 words for function /findGenome", argGenomeSearchTerm, searchString);

struct jsonWrite *jw = apiStartOutput();

jsonWriteString(jw, argGenomeSearchTerm, searchString);
if (allowAll)
	jsonWriteBoolean(jw, argAllowAll, allowAll);

long long itemCount = 0;
long long totalMatchCount = 0;
char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
if (1 == wordCount)
    itemCount = oneWordSearch(conn, words[0], jw, &totalMatchCount);
else	/* multiple word search */
    itemCount = multipleWordSearch(conn, words, wordCount, jw, &totalMatchCount);

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

elapsedTime(jw);
if (statsOnly)
    jsonWriteBoolean(jw, "statsOnly", TRUE);
if (itemCount)
    {
    jsonWriteNumber(jw, "itemCount", itemCount);
    jsonWriteNumber(jw, "totalMatchCount", totalMatchCount);
    if (totalMatchCount > itemCount)
	jsonWriteBoolean(jw, "maxItemsLimit", TRUE);
    apiFinishOutput(0, NULL, jw);
    }
else
    apiErrAbort(err400, err400Msg, "no genomes found matching search term %s='%s' for endpoint: /findGenome", argGenomeSearchTerm, inputSearchString);

hDisconnectCentral(&conn);

}
