/* findGenome search functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"


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

int itemsFound = 0;

char **words;
AllocArray(words, wordCount);
(void) chopByWhite(searchString, words, wordCount);
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
	    ++itemsFound;
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

if (itemsFound)
    apiFinishOutput(0, NULL, jw);
else
    apiErrAbort(err400, err400Msg, "no genomes found matching search term %s='%s' for endpoint: /findGenome", argGenomeSearchTerm, inputSearchString);

hDisconnectCentral(&conn);

}
