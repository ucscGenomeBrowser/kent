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

static void startGenomes(struct jsonWrite *jw)
/* begin the list output */
{
if (jsonOutputArrays)
    jsonWriteListStart(jw, "genomes");
else
    jsonWriteObjectStart(jw, "genomes");
}

static void endGenomes(struct jsonWrite *jw)
/* end the list output */
{
if (jsonOutputArrays)
    jsonWriteListEnd(jw);
else
    jsonWriteObjectEnd(jw);
}

static void finiGenArk(struct jsonWrite *jw, struct genark *el)
/* output of a genark item has started, finish it off */
{
jsonWriteBoolean(jw, "isGenArkBrowser", TRUE);
jsonWriteString(jw, "hubUrl", el->hubUrl);
jsonWriteString(jw, "asmName", el->asmName);
jsonWriteString(jw, "scientificName", el->scientificName);
jsonWriteString(jw, "commonName", el->commonName);
}

static void finiSummary(struct jsonWrite *jw, struct asmSummary *el)
/* output of a summary item has started, finish it off */
{
jsonWriteString(jw, "bioProject", el->bioproject);
jsonWriteString(jw, "bioSample", el->biosample);
jsonWriteString(jw, "wgsMaster", el->wgsMaster);
jsonWriteString(jw, "refseqCategory", el->refseqCategory);
jsonWriteNumber(jw, "taxId", (long long)el->taxId);
jsonWriteNumber(jw, "speciesTaxid", (long long)el->speciesTaxid);
jsonWriteString(jw, "organismName", el->organismName);
jsonWriteString(jw, "infraspecificName", el->infraspecificName);
jsonWriteString(jw, "isolate", el->isolate);
jsonWriteString(jw, "versionStatus", el->versionStatus);
jsonWriteString(jw, "assemblyLevel", el->assemblyLevel);
jsonWriteString(jw, "releaseType", el->releaseType);
jsonWriteString(jw, "genomeRep", el->genomeRep);
jsonWriteString(jw, "seqRelDate", el->seqRelDate);
jsonWriteString(jw, "asmName", el->asmName);
jsonWriteString(jw, "asmSubmitter", el->asmSubmitter);
jsonWriteString(jw, "gbrsPairedAsm", el->gbrsPairedAsm);
jsonWriteString(jw, "pairedAsmComp", el->pairedAsmComp);
jsonWriteString(jw, "ftpPath", el->ftpPath);
jsonWriteString(jw, "excludedFromRefseq", el->excludedFromRefseq);
jsonWriteString(jw, "relationToTypeMaterial", el->relationToTypeMaterial);
jsonWriteString(jw, "assemblyType", el->assemblyType);
jsonWriteString(jw, "phyloGroup", el->phyloGroup);
jsonWriteNumber(jw, "genomeSize", (long long)el->genomeSize);
jsonWriteNumber(jw, "genomeSizeUngapped", (long long)el->genomeSizeUngapped);
jsonWriteDouble(jw, "gcPercent", (double)el->gcPercent);
jsonWriteNumber(jw, "repliconCount", (long long)el->repliconCount);
jsonWriteNumber(jw, "scaffoldCount", (long long)el->scaffoldCount);
jsonWriteNumber(jw, "contigCount", (long long)el->contigCount);
jsonWriteString(jw, "annotationProvider", el->annotationProvider);
jsonWriteString(jw, "annotationName", el->annotationName);
jsonWriteString(jw, "annotationDate", el->annotationDate);
jsonWriteNumber(jw, "totalGeneCount", (long long)el->totalGeneCount);
jsonWriteNumber(jw, "proteinCodingGeneCount", (long long)el->proteinCodingGeneCount);
jsonWriteNumber(jw, "nonCodingGeneCount", (long long)el->nonCodingGeneCount);
jsonWriteString(jw, "pubmedId", el->pubmedId);
}

static void setBrowserUrl(struct jsonWrite *jw, char *browserId)
{
char* host = getenv("HTTP_HOST");
char browserUrl[1024];
if (startsWith("GC", browserId))
    {
    if (host)
        safef(browserUrl, sizeof(browserUrl), "https://%s/h/%s", host, browserId);
    else
        safef(browserUrl, sizeof(browserUrl), "https://genome.ucsc.edu/h/%s", browserId);
    }
else
    {
    if (host)
        safef(browserUrl, sizeof(browserUrl), "https://%s/cgi-bin/hgTracks?db=%s", host, browserId);
    else
        safef(browserUrl, sizeof(browserUrl), "https://genome.ucsc.edu/cgi-bin/hgTracks?db=%s", browserId);
    }
jsonWriteString(jw, "browserUrl", browserUrl);
}

static int outputCombo(struct jsonWrite *jw, struct combinedSummary *summaryList)
/* may be information from any of the three tables */
{
int itemCount = 0;
struct combinedSummary *el = NULL;
startGenomes(jw);
for (el=summaryList; el != NULL; el=el->next)
    {
    if (el->summary)
	{
	if (jsonOutputArrays)
	    {
	    jsonWriteObjectStart(jw, NULL);
	    jsonWriteString(jw, "accession", el->summary->assemblyAccession);
	    }
	else
	    jsonWriteObjectStart(jw, el->summary->assemblyAccession);

        finiSummary(jw, el->summary);
	boolean sciNameDone = FALSE;
	boolean comNameDone = FALSE;
        if (el->genArk)
	    {
//            jsonWriteString(jw, "gcAccession", el->genArk->gcAccession);
	    /* some of these are going to be dups of the asmSummary */
            setBrowserUrl(jw, el->genArk->gcAccession);
	    finiGenArk(jw, el->genArk);
	    sciNameDone = TRUE;
	    comNameDone = TRUE;
//            jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
	    }
	if (el->dbDb)
	    {
            setBrowserUrl(jw, el->dbDb->name);
            jsonWriteString(jw, "database", el->dbDb->name);
            jsonWriteString(jw, "description", el->dbDb->description);
            jsonWriteString(jw, "sourceName", el->dbDb->sourceName);
	    if (!sciNameDone)
		jsonWriteString(jw, "scientificName", el->dbDb->scientificName);
	    if (!comNameDone)
		jsonWriteString(jw, "commonName", el->dbDb->organism);
//            jsonWriteNumber(jw, "taxId", (long long)dbDbEntry->taxId);
	    }
	}
    else if (el->genArk)	/* ONLY genArk ?? - should have asmSummary TBD */
	{
	if (jsonOutputArrays)
	    {
	    jsonWriteObjectStart(jw, NULL);
	    jsonWriteString(jw, "gcAccession", el->genArk->gcAccession);
	    }
	else
	    jsonWriteObjectStart(jw, el->genArk->gcAccession);
        setBrowserUrl(jw, el->genArk->gcAccession);
	finiGenArk(jw, el->genArk);
	if (el->dbDb)
	    {
	    jsonWriteBoolean(jw, "isUcscDatabase", TRUE);
            jsonWriteString(jw, "database", el->dbDb->name);
            jsonWriteString(jw, "description", el->dbDb->description);
            jsonWriteString(jw, "sourceName", el->dbDb->sourceName);
//	    if (!sciNameDone)
//		jsonWriteString(jw, "scientificName", el->dbDb->scientificName);
//	    if (!comNameDone)
//		jsonWriteString(jw, "commonName", el->dbDb->organism);
//            jsonWriteNumber(jw, "taxId", (long long)dbDbEntry->taxId);
	    }
	}
    else if (el->dbDb)	/* ONLY dbDb ?  - should have the other two TBD */
	{
	if (jsonOutputArrays)
	    {
	    jsonWriteObjectStart(jw, NULL);
	    jsonWriteString(jw, "database", el->dbDb->name);
	    }
	else
	    jsonWriteObjectStart(jw, el->dbDb->name);

        setBrowserUrl(jw, el->dbDb->name);
	jsonWriteBoolean(jw, "isUcscDatabase", TRUE);
	jsonWriteString(jw, "description", el->dbDb->description);
	jsonWriteString(jw, "sourceName", el->dbDb->sourceName);
	jsonWriteString(jw, "scientificName", el->dbDb->scientificName);
	jsonWriteString(jw, "commonName", el->dbDb->organism);
	jsonWriteNumber(jw, "taxId", (long long)el->dbDb->taxId);
	}

    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
endGenomes(jw);
return (itemCount);
}	/*	static int outputCombo(struct jsonWrite *jw, struct combinedSummary *summaryList) */

static struct combinedSummary *checkForGenArk(struct sqlConnection *conn, struct asmSummary *list)
/* given an asmSummary list, see if any match to genArk genomes */
/* TBD - should also check here for UCSC database matching */
{
struct combinedSummary *comboSumary = NULL;
if (list)
    {
    struct asmSummary *el = NULL;
    for (el=list; el != NULL; el=el->next)
        {
        struct combinedSummary *cs = NULL;
        AllocVar(cs);
        cs->summary = el;
        cs->genArk = NULL;
        cs->dbDb = NULL;
        char query[4096];
        sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE gcAccession = \"%s\"", genarkTable, el->assemblyAccession);
        struct genark *gA = genarkLoadByQuery(conn, query);
        if (gA)
            cs->genArk = gA;
        slAddHead(&comboSumary, cs);
        }
    }
if (comboSumary)
  slReverse(&comboSumary);
return comboSumary;
}

static struct asmSummary *checkAsmSummary(struct sqlConnection *conn, char *oneAccession, boolean exactMatch, long long limit)
/* check the asmSummary table for oneAccession, mayby exact, maybe a limit */
{
char query[4096];
if (limit > 0)
    {
    if (exactMatch)
        sqlSafef(query, sizeof(query), "SELECT * FROM asmSummary WHERE assemblyAccession = '%s' LIMIT %lld", oneAccession, limit);
    else
        sqlSafef(query, sizeof(query), "SELECT * FROM asmSummary WHERE assemblyAccession LIKE '%s%%' LIMIT %lld", oneAccession, limit);
    }
else
    {
    if (exactMatch)
        sqlSafef(query, sizeof(query), "SELECT * FROM asmSummary WHERE assemblyAccession = '%s'", oneAccession);
    else
        sqlSafef(query, sizeof(query), "SELECT * FROM asmSummary WHERE assemblyAccession LIKE '%s%%'", oneAccession);
    }

struct asmSummary *list = asmSummaryLoadByQuery(conn, query);
return list;
}

// Field   Type    Null    Key     Default Extra
// source  varchar(255)    NO      MUL     NULL
// destination     varchar(255)    NO      MUL     NULL
// sourceAuthority enum('ensembl','ucsc','genbank','refseq') NO  NULL
// destinationAuthority    enum('ensembl','ucsc','genbank','refseq')   NO  NULL
// matchCount      bigint(20)      NO              NULL
// sourceCount     bigint(20)      NO              NULL
// destinationCount        bigint(20)      NO              NULL

struct asmSummary *dbDbAsmEquivalent(struct sqlConnection *centConn, char *dbDbName)
{
struct asmSummary *itemReturn = NULL;
struct sqlConnection *conn = hAllocConn("hgFixed");
char query[4096];
sqlSafef(query, sizeof(query), "SELECT destination from asmEquivalent WHERE source='%s' AND sourceAuthority='ucsc' AND destinationAuthority='refseq' LIMIT 1", dbDbName);
char *asmIdFound = NULL;
char *genBank = NULL;
char *refSeq = sqlQuickString(conn, query);
if (refSeq)
    asmIdFound = refSeq;
else
    {
    sqlSafef(query, sizeof(query), "SELECT destination from asmEquivalent WHERE source='%s' AND sourceAuthority='ucsc' AND destinationAuthority='genbank' LIMIT 1", dbDbName);
    genBank = sqlQuickString(conn, query);
    if (genBank)
	asmIdFound = genBank;
    }
if (asmIdFound)
    {
    char *words[3];
    int wordCount = chopString(asmIdFound, "_", words, ArraySize(words));
    if (wordCount > 2)
	{
	safef(query, sizeof(query), "%s_%s", words[0], words[1]);
        itemReturn = checkAsmSummary(centConn, query, TRUE, 1);
	}
    }
return itemReturn;
}

static int sqlJsonOut(struct jsonWrite *jw, struct sqlResult *sr)
/* given a sqlResult, walk through the rows and output the json */
{
int itemCount = 0;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct assemblyList *el = assemblyListLoadWithNull(row);
    jsonWriteObjectStart(jw, el->name);
//    jsonWriteString(jw, "name", el->name);
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

char query[4096];
sqlSafef(query, sizeof(query), "SELECT count(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE);", asmListTable, queryDy->string);
long long matchCount = sqlQuickLongLong(conn, query);
if (matchCount > 0)
    {
    verbose(1, "DBG: matchCount: %lld from search '%s'\n", matchCount, query);
    *totalMatchCount = matchCount;
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE) ORDER BY priority LIMIT %d;", asmListTable, queryDy->string, maxItemsOutput);
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
int itemCount = 0;
*totalMatchCount = 0;

sqlSafef(query, sizeof(query), "SELECT count(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s' IN BOOLEAN MODE);", asmListTable, searchWord);
long long matchCount = sqlQuickLongLong(conn, query);
boolean prefixSearch = FALSE;
if (matchCount < 1)	/* no match, add the * wild card match to make a prefix match */
    {
    sqlSafef(query, sizeof(query), "SELECT count(*) FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s*' IN BOOLEAN MODE);", asmListTable, searchWord);
    matchCount = sqlQuickLongLong(conn, query);
    if (matchCount > 0)
	prefixSearch = TRUE;
    }
if (matchCount < 1)
    return itemCount;
*totalMatchCount = matchCount;

sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE MATCH(name, commonName, scientificName, clade, description) AGAINST ('%s%s' IN BOOLEAN MODE) ORDER BY priority LIMIT %d;", asmListTable, searchWord, prefixSearch ? "*" : "", maxItemsOutput);
struct sqlResult *sr = sqlGetResult(conn, query);
itemCount = sqlJsonOut(jw, sr);
sqlFreeResult(&sr);

return itemCount;
}	/*	static long long oneWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw) */

static void asmSummaryGroup(struct sqlConnection *conn, struct jsonWrite *jw, char *field)
/* show a grouping count for a field in asmSummary table */
{
char query[4096];
jsonWriteObjectStart(jw, field);
sqlSafef(query, sizeof(query), "SELECT %s, COUNT(*) FROM asmSummary GROUP by %s", field, field);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (strlen(row[0]) < 1)
	jsonWriteNumber(jw, "na", sqlLongLong(row[1]));
    else
	jsonWriteNumber(jw, row[0], sqlLongLong(row[1]));
    }
sqlFreeResult(&sr);
jsonWriteObjectEnd(jw);
}

static void doStatsOnly(struct sqlConnection *conn, struct jsonWrite *jw)
/* only count the items in each database */
{
jsonWriteString(jw, "description", "counting items in tables: asmSummary, genark and dbDb for number of genomes available");

char query[4096];
sqlSafef(query, sizeof(query), "SELECT count(*) FROM %s", genarkTable);
long long genArkCount = sqlQuickLongLong(conn, query);
sqlSafef(query, sizeof(query), "SELECT count(*) FROM asmSummary");
long long asmSummaryTotal = sqlQuickLongLong(conn, query);
sqlSafef(query, sizeof(query), "SELECT count(*) FROM dbDb where active=1");
long long dbDbCount = sqlQuickLongLong(conn, query);
long long totalCount = genArkCount + asmSummaryTotal + dbDbCount;
jsonWriteNumber(jw, "totalCount", totalCount);
jsonWriteObjectStart(jw, "genark");
jsonWriteString(jw, "TBD", "the genark table can count GCF vs. GCA and will have a clade category to couint\n");
jsonWriteNumber(jw, "itemCount", genArkCount);
jsonWriteObjectEnd(jw);
jsonWriteObjectStart(jw, "dbDb");
jsonWriteString(jw, "description", "the dbDb table is a count of UCSC 'database' browser instances");
jsonWriteNumber(jw, "itemCount", dbDbCount);
jsonWriteObjectEnd(jw);
jsonWriteObjectStart(jw, "asmSummary");
jsonWriteString(jw, "description", "the asmSummary is the contents of the NCBI  genbank/refseq assembly_summary.txt information");
jsonWriteString(jw, "seeAlso", "https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/README_assembly_summary.txt");
jsonWriteNumber(jw, "asmSummaryTotal", asmSummaryTotal);
asmSummaryGroup(conn, jw, "refseqCategory");
asmSummaryGroup(conn, jw, "versionStatus");
asmSummaryGroup(conn, jw, "assemblyLevel");
asmSummaryGroup(conn, jw, "releaseType");
asmSummaryGroup(conn, jw, "genomeRep");
asmSummaryGroup(conn, jw, "pairedAsmComp");
asmSummaryGroup(conn, jw, "assemblyType");
asmSummaryGroup(conn, jw, "phyloGroup");
jsonWriteObjectEnd(jw);
}

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

if (! statsOnly)
    {
    /* verify number of words in search string is legal */
    wordCount = chopByWhite(searchString, NULL, 0);

    if (wordCount < 1)
    apiErrAbort(err400, err400Msg, "search term '%s' does not contain a word ? for function /findGenome", argGenomeSearchTerm);
    if (wordCount > 5)
    apiErrAbort(err400, err400Msg, "search term '%s=%s' should not have more than 5 words for function /findGenome", argGenomeSearchTerm, searchString);
    }

struct jsonWrite *jw = apiStartOutput();
if (statsOnly)
    {
    doStatsOnly(conn, jw);
    elapsedTime(jw);
    apiFinishOutput(0, NULL, jw);
    hDisconnectCentral(&conn);
    return;
    }

jsonWriteString(jw, argGenomeSearchTerm, searchString);
if (allowAll)
	jsonWriteBoolean(jw, argAllowAll, allowAll);

long long itemCount = 0;
long long totalMatchCount = 0;
/* save the search string before it is chopped */
char *pristineSearchString = cloneString(searchString);

char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
if (1 == wordCount)
    itemCount = oneWordSearch(conn, words[0], jw, &totalMatchCount);
else	/* multiple word search */
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
    {
    itemCount = multipleWordSearch(conn, words, wordCount, jw, &totalMatchCount);
    if ( 1 == 0 ) {
    long long totalMatch = 0;
    /* no more than 100 items result please */
    struct asmSummary *summaryList = asmSummaryFullText(conn, pristineSearchString, (long long) 100, &totalMatch);
    if (summaryList)
	{
	struct combinedSummary *comboOutput = checkForGenArk(conn, summaryList);
	if (comboOutput)
	    itemCount = outputCombo(jw, comboOutput);
	if (itemCount)
	    jsonWriteNumber(jw, "itemCount", itemCount);
	else
	    verbose(0, "# DBG need to search this string '%s' somewhere else\n", pristineSearchString);
	}
    else
	verbose(0, "# DBG need to search this string '%s' somewhere else\n", pristineSearchString);
    }	/*	if ( 1 == 0 )	*/
    }

elapsedTime(jw);
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
