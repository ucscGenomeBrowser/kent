/* findGenome search functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"
#include "asmAlias.h"
#include "asmSummary.h"

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
	    finiGenArk(jw, el->genArk);
	    sciNameDone = TRUE;
	    comNameDone = TRUE;
//            jsonWriteNumber(jw, "taxId", (long long)genome->taxId);
	    }
	if (el->dbDb)
	    {
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

static int singleWordSearch(struct sqlConnection *conn, char *searchWord, struct jsonWrite *jw)
/* conn is a connection to hgcentral, searching for one word,
 *   might be a UCSC database or a GenArk hub accession, or just some word.
 */
{
/* the input word might be a database alias
 * asmAliasFind returns the searchWord if no alias found
 */
char *perhapsAlias = asmAliasFind(searchWord);

int itemCount = 0;

if (startsWith("GC", perhapsAlias))
    {
    char query[4096];
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE gcAccession LIKE \"%s%%\"", genarkTable, perhapsAlias);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    struct genark *genArkList = NULL;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct genark *genome = genarkLoad(row);
        slAddHead(&genArkList, genome);
        }
    sqlFreeResult(&sr);
    slReverse(&genArkList);
    struct combinedSummary *comboOutput = NULL;
    struct genark *el = NULL;
    for (el = genArkList;  el != NULL; el = el->next)
	{
        struct combinedSummary *cs = NULL;
        AllocVar(cs);
        cs->summary = NULL;
        cs->genArk = el;
        cs->dbDb = NULL;
	char asmSumQuery[4096];
	sqlSafef(asmSumQuery, sizeof(asmSumQuery), "SELECT * FROM asmSummary WHERE assemblyAccession = \"%s\"", el->gcAccession);
	struct asmSummary *as = asmSummaryLoadByQuery(conn, asmSumQuery);
        if (as)
            cs->summary = as;
	slAddHead(&comboOutput, cs);
	}
    if (comboOutput)
	{
	slReverse(&comboOutput);
	itemCount = outputCombo(jw, comboOutput);
	}
    }	/*	if (startsWith("GC", perhapsAlias))	*/
else
    {	/* not a GC genArk name, perhaps a UCSC database */
    if (hDbIsActive(perhapsAlias))
        {
	struct combinedSummary *comboOutput = NULL;
        AllocVar(comboOutput);
        comboOutput->summary = NULL;
        comboOutput->genArk = NULL;
	/* TBD need to get an equivalent GC accession name for UCSC database */
        struct dbDb *dbDbEntry = hDbDb(perhapsAlias);
        comboOutput->dbDb = dbDbEntry;
	itemCount = outputCombo(jw, comboOutput);
        }
    }	/*	checked genArk and UCSC database */
if (0 == itemCount)	/* not found in genark or ucsc db, check asmSummary */
    {
    long long totalMatch = 0;
    /* no more than 100 items result please */
    struct asmSummary *summaryList = asmSummaryFullText(conn, perhapsAlias, (long long) 100, &totalMatch);
    /* now check the genark table to see if there are any of those */
    struct combinedSummary *comboOutput = checkForGenArk(conn, summaryList);
    if (comboOutput)
	itemCount = outputCombo(jw, comboOutput);
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
genarkTable = genarkTableName();

if (!sqlTableExists(conn, genarkTable))
    apiErrAbort(err500, err500Msg, "missing central.genark table in function /findGenome'%s'", extraArgs);

/* verify number of words in search string is legal */
int wordCount = chopByWhite(searchString, NULL, 0);

if (wordCount < 1)
    apiErrAbort(err400, err400Msg, "search term '%s' does not contain a word ? for function /findGenome", argGenomeSearchTerm);
if (wordCount > 5)
    apiErrAbort(err400, err400Msg, "search term '%s=%s' should not have more than 5 words for function /findGenome", argGenomeSearchTerm, searchString);

genarkTable = genarkTableName();

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
else	/* multiple word search */
    {
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
    }

if (itemCount)
    apiFinishOutput(0, NULL, jw);
else
    apiErrAbort(err400, err400Msg, "no genomes found matching search term %s='%s' for endpoint: /findGenome", argGenomeSearchTerm, inputSearchString);

hDisconnectCentral(&conn);

}
