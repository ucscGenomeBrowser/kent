/* findGenome search functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"
#include "asmAlias.h"
#include "asmSummary.h"

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

static int outputFullText(struct jsonWrite *jw, struct asmSummary *summaryList)
{
int itemCount = 0;
/* XXX TBD need to get common names for these items */
struct asmSummary *el = NULL;
startGenomes(jw);
for (el=summaryList; el != NULL; el=el->next)
    {
    if (jsonOutputArrays)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "accession", el->assemblyAccession);
        jsonWriteString(jw, "asmName", el->asmName);
        jsonWriteString(jw, "asmSubmitter", el->asmSubmitter);
        jsonWriteString(jw, "annotationProvider", el->annotationProvider);
        jsonWriteString(jw, "assemblyType", el->assemblyType);
        jsonWriteString(jw, "bioProject", el->bioproject);
        jsonWriteString(jw, "bioSample", el->biosample);
        jsonWriteString(jw, "organismName", el->organismName);
        jsonWriteString(jw, "isolate", el->isolate);
        jsonWriteNumber(jw, "taxId", (long long)el->taxId);
        jsonWriteObjectEnd(jw);
        }
    else
        {
        jsonWriteObjectStart(jw, el->assemblyAccession);
        jsonWriteString(jw, "asmName", el->asmName);
        jsonWriteString(jw, "asmSubmitter", el->asmSubmitter);
        jsonWriteString(jw, "annotationProvider", el->annotationProvider);
        jsonWriteString(jw, "assemblyType", el->assemblyType);
        jsonWriteString(jw, "bioProject", el->bioproject);
        jsonWriteString(jw, "bioSample", el->biosample);
        jsonWriteString(jw, "organismName", el->organismName);
        jsonWriteString(jw, "isolate", el->isolate);
        jsonWriteNumber(jw, "taxId", (long long)el->taxId);
        jsonWriteObjectEnd(jw);
        }
    ++itemCount;
    }
endGenomes(jw);
return (itemCount);
}	/*	static int outputFullText(struct jsonWrite *jw, struct asmSummary *summaryList, ) */

static int singleWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw)
/* conn is a connection to hgcentral, searching for one word,
 *   might be a database or a GenArk hub.
 */
{
/* the input word might be a database alias
 * asmAliasFind returns the searchWord if no alias found
 */
char *perhapsAlias = asmAliasFind(searchWord);
char *genarkTable = genarkTableName();
int itemCount = 0;
if (startsWith("GC", perhapsAlias))
    {
    char query[4096];
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE gcAccession LIKE \"%s%%\"", genarkTable, perhapsAlias);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    /* XXXX - TBD need to decide on a common set of output items */
//    startGenomes(jw);
    struct genark *genArkList = NULL;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct genark *genome = genarkLoad(row);
        slAddHead(&genArkList, genome);
        ++itemCount;
if (1 == 0) {
        if (jsonOutputArrays)
            {
            jsonWriteObjectStart(jw, NULL);
            jsonWriteString(jw, "gcAccession", genome->gcAccession);
            jsonWriteString(jw, "hubUrl", genome->hubUrl);
            jsonWriteString(jw, "asmName", genome->asmName);
            jsonWriteString(jw, "scientificName", genome->scientificName);
            jsonWriteString(jw, "commonName", genome->commonName);
            jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
            jsonWriteObjectEnd(jw);
            }
        else
            {
            jsonWriteObjectStart(jw, genome->gcAccession);
            jsonWriteString(jw, "hubUrl", genome->hubUrl);
            jsonWriteString(jw, "asmName", genome->asmName);
            jsonWriteString(jw, "scientificName", genome->scientificName);
            jsonWriteString(jw, "commonName", genome->commonName);
            jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
            jsonWriteObjectEnd(jw);
            }
}
        }
//    endGenomes(jw);
    sqlFreeResult(&sr);
    slReverse(&genArkList);
    struct asmSummary *summaryList = NULL;
    struct genark *el = NULL;
    for (el = genArkList;  el != NULL; el = el->next)
	{
	char asmSumQuery[4096];
	sqlSafef(asmSumQuery, sizeof(asmSumQuery), "SELECT * FROM asmSummary WHERE assemblyAccession = \"%s\"", el->gcAccession);
	struct asmSummary *el = asmSummaryLoadByQuery(conn, asmSumQuery);
	slAddHead(&summaryList, el);
	}
    if (summaryList)
	{
	itemCount = outputFullText(jw, summaryList);
	}
    } else {	/* not a GC genArk name, perhaps a UCSC database */
	if (hDbIsActive(perhapsAlias))
	    {
            struct dbDb *dbDbEntry = hDbDb(perhapsAlias);
            startGenomes(jw);
	    if (jsonOutputArrays)
		{
		jsonWriteObjectStart(jw, NULL);
		jsonWriteString(jw, "database", dbDbEntry->name);
		jsonWriteString(jw, "description", dbDbEntry->description);
		jsonWriteString(jw, "sourceName", dbDbEntry->sourceName);
		jsonWriteString(jw, "scientificName", dbDbEntry->scientificName);
		jsonWriteString(jw, "commonName", dbDbEntry->organism);
		jsonWriteNumber(jw, "taxId", (long long)dbDbEntry->taxId);
		jsonWriteObjectEnd(jw);
		}
	    else
		{
		jsonWriteObjectStart(jw, dbDbEntry->name);
		jsonWriteString(jw, "description", dbDbEntry->description);
		jsonWriteString(jw, "sourceName", dbDbEntry->sourceName);
		jsonWriteString(jw, "scientificName", dbDbEntry->scientificName);
		jsonWriteString(jw, "commonName", dbDbEntry->organism);
		jsonWriteNumber(jw, "taxId", (long long)dbDbEntry->taxId);
		jsonWriteObjectEnd(jw);
		}
	    endGenomes(jw);
            ++itemCount;
	    }
	else	/* not genArk and not UCSC, check the asmSummary data */
	    {
	    long long totalMatch = 0;
            /* no more than 100 items result please */
            struct asmSummary *summaryList = asmSummaryFullText(conn, perhapsAlias, (long long) 100, &totalMatch);
            if (summaryList)
		{
                itemCount = outputFullText(jw, summaryList);
		}
	    }
    }
return itemCount;
}	/*	static void singleWordSearch(struct sqlConnection *conn, char *searchWord) */

void apiFindGenome(char *pathString[MAX_PATH_INFO])
/* 'findGenome' function */
{
char *searchString = cgiOptionalString(argGenomeSearchTerm);
char *inputSearchString = cloneString(searchString);
char *extraArgs = verifyLegalArgs(argFindGenome);

if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /findGenome'%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *genarkTable = genarkTableName();
if (!sqlTableExists(conn, genarkTable))
    apiErrAbort(err500, err500Msg, "missing central.genark table in function /findGenome'%s'", extraArgs);

/* verify number of words in search string is legal */
int wordCount = chopByWhite(searchString, NULL, 0);

if (wordCount < 1)
    apiErrAbort(err400, err400Msg, "search term '%s' does not contain a word ? for function /findGenome", argGenomeSearchTerm);
if (wordCount > 5)
    apiErrAbort(err400, err400Msg, "search term '%s=%s' should not have more than 5 words for function /findGenome", argGenomeSearchTerm, searchString);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, argGenomeSearchTerm, searchString);

int itemCount = 0;
/* save the search string before it is chopped */
char *pristineSearchString = cloneString(searchString);

char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
if (1 == wordCount)
    {
    itemCount = singleWordSearch(conn, words[0], jw);
    if (itemCount)
	jsonWriteNumber(jw, "itemCount", itemCount);
    else
	verbose(0, "# DBG need to search this word %s somewhere else\n", words[0]);
    }
else
    {
    long long totalMatch = 0;
    /* no more than 100 items result please */
    struct asmSummary *summaryList = asmSummaryFullText(conn, pristineSearchString, (long long) 100, &totalMatch);
    if (summaryList)
	{
	itemCount = outputFullText(jw, summaryList);
	if (itemCount)
	    jsonWriteNumber(jw, "itemCount", itemCount);
	else
	    verbose(0, "# DBG need to search this word %s somewhere else\n", words[0]);
	}
#ifdef NOT
    for (int w = 0; w < wordCount; w++)
        {
        if (startsWith("GC", words[w]))
            {
            char query[4096];
            sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE gcAccession LIKE '%s%%'", genarkTable, words[w]);
            struct sqlResult *sr = sqlGetResult(conn, query);
            char **row;
            while ((row = sqlNextRow(sr)) != NULL)
                {
                struct genark *genome = genarkLoad(row);
                ++itemCount;
                if (jsonOutputArrays)
                    {
                    jsonWriteListStart(jw, NULL);
                    jsonWriteString(jw, "gcAccession", genome->gcAccession);
                    jsonWriteString(jw, "hubUrl", genome->hubUrl);
                    jsonWriteString(jw, "asmName", genome->asmName);
                    jsonWriteString(jw, "scientificName", genome->scientificName);
                    jsonWriteString(jw, "commonName", genome->commonName);
                    jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
                    jsonWriteListEnd(jw);
                    }
                else
                    {
                    jsonWriteObjectStart(jw, NULL);
                    jsonWriteString(jw, "gcAccession", genome->gcAccession);
                    jsonWriteString(jw, "hubUrl", genome->hubUrl);
                    jsonWriteString(jw, "asmName", genome->asmName);
                    jsonWriteString(jw, "scientificName", genome->scientificName);
                    jsonWriteString(jw, "commonName", genome->commonName);
                    jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
                    jsonWriteObjectEnd(jw);
                    }
                }
            }
        }
#endif
    }

if (itemCount)
    apiFinishOutput(0, NULL, jw);
else
    apiErrAbort(err400, err400Msg, "no genomes found matching search term %s='%s' for endpoint: /findGenome", argGenomeSearchTerm, inputSearchString);

hDisconnectCentral(&conn);

}
