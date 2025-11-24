/* liftOver functions */

#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"
#include "genark.h"
#include "asmAlias.h"
#include "assemblyList.h"
#include "liftOver.h"
#include "liftOverChain.h"
#include "net.h"

/**** SHOULD BE IN LIBRARY - code from hgConvert.c ******/
static long chainTotalBlockSize(struct chain *chain)
/* Return sum of sizes of all blocks in chain */
{
struct cBlock *block;
long total = 0;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}
/**** SHOULD BE IN LIBRARY - code from hgConvert.c ******/

static void chainListOut(char *fromDb, char* toDb, int origSize, char *fromPos, struct chain *chainList)
/* given the list of chains, output the list in JSON */
{
struct chain *chain = NULL;
char position[4096];
char coverage[4096];
struct jsonWrite *jw = apiStartOutput();
jsonWriteListStart(jw, "from");
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, fromDb);
jsonWriteString(jw, NULL, fromPos);
jsonWriteListEnd(jw);
jsonWriteListEnd(jw);
jsonWriteListStart(jw, "to");
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    int blockSize;
    int qStart, qEnd;
    if (chain->qStrand == '-')
        {
        qStart = chain->qSize - chain->qEnd;
        qEnd = chain->qSize - chain->qStart;
        }
    else
        {
        qStart = chain->qStart;
        qEnd = chain->qEnd;
        }
    blockSize = chainTotalBlockSize(chain);
    safef(position, sizeof(position), "%s:%d-%d", chain->qName, qStart+1, qEnd);
    safef(coverage, sizeof(coverage), "%3.1f%% of bases, %3.1f%% of span",
        100.0 * blockSize / origSize,
        100.0 * (chain->tEnd - chain->tStart) / origSize);
    jsonWriteListStart(jw, NULL);
    jsonWriteString(jw, NULL, toDb);
    jsonWriteString(jw, NULL, position);
    jsonWriteString(jw, NULL, coverage);
    jsonWriteListEnd(jw);
    }
jsonWriteListEnd(jw);
apiFinishOutput(0, NULL, jw);
}

static void listExisting()
/* output the fromDb,toDb from liftOverChain.hgcentral SQL table */
{
char *filter = cgiOptionalString(argFilter);
char *fromDb = cgiOptionalString(argFromGenome);
char *toDb = cgiOptionalString(argToGenome);

struct sqlConnection *conn = hConnectCentral();
char *tableName = cloneString(liftOverChainTable());
struct dyString *query = newDyString(0);
sqlDyStringPrintf(query, "SELECT count(*) FROM %s", tableName);
long long totalRows = sqlQuickLongLong(conn, dyStringContents(query));
dyStringClear(query);

if (isNotEmpty(fromDb) || isNotEmpty(toDb))
    {
    sqlDyStringPrintf(query, "SELECT * FROM %s WHERE ", tableName);
    if (isNotEmpty(fromDb))
        sqlDyStringPrintf(query, "LOWER(fromDb) = LOWER('%s') %s ", fromDb, isNotEmpty(toDb) ? "AND" : "");
    if (isNotEmpty(toDb))
        sqlDyStringPrintf(query, "LOWER(toDb) = LOWER('%s') ", toDb);
    }
else if (isNotEmpty(filter))
    {
    sqlDyStringPrintf(query, "SELECT * FROM %s WHERE LOWER(fromDb) = LOWER('%s') OR LOWER(toDb) = LOWER('%s')", tableName, filter, filter);
    }
else
    {
    sqlDyStringPrintf(query, "SELECT * FROM %s", tableName);
    }
sqlDyStringPrintf(query, " LIMIT %d;", maxItemsOutput);

char *dataTime = sqlTableUpdate(conn, tableName);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);

jsonWriteListStart(jw, "existingLiftOvers");
struct liftOverChain *chain, *chainList = liftOverChainLoadByQuery(conn, dyStringCannibalize(&query));
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "fromDb", chain->fromDb);
    jsonWriteString(jw, "toDb", chain->toDb);
    jsonWriteString(jw, "path", chain->path);
    jsonWriteDouble(jw, "minMatch", chain->minMatch);
    jsonWriteNumber(jw, "minChainT", chain->minChainT);
    jsonWriteNumber(jw, "minSizeQ", chain->minSizeQ);
    jsonWriteString(jw, "multiple", chain->multiple);
    jsonWriteDouble(jw, "minBlocks", chain->minBlocks);
    jsonWriteString(jw, "fudgeThick", chain->fudgeThick);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
jsonWriteNumber(jw, "totalLiftOvers", totalRows);
jsonWriteNumber(jw, "itemsReturned", slCount(chainList));
liftOverChainFreeList(&chainList);

apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

/**** SHOULD BE IN LIBRARY - code from hgConvert.c ******/
static char *skipWord(char *fw)
/* skips over current word to start of next. 
 * Error for this not to exist. */
{
char *s;
s = skipToSpaces(fw);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
s = skipLeadingSpaces(s);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
return s;
}

static struct chain *chainLoadIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region. */
{
struct lineFile *lf = netLineFileOpen(fileName);
char *line;
int chromNameSize = strlen(chrom);
struct chain *chainList = NULL, *chain;
#ifdef SOON	/* Put in if we index. */
boolean gotChrom = FALSE;
#endif  /* SOON */
int chainCount = 0;

while (lineFileNextReal(lf, &line))
    {
    if (startsWith("chain", line) && isspace(line[5]))
        {
	++chainCount;
	line = skipWord(line);	/* Skip over 'chain' */
	line = skipWord(line);	/* Skip over chain score */
	if (startsWith(chrom, line) && isspace(line[chromNameSize]))
	    {
#ifdef SOON	/* Put in if we index. */
	    gotChrom = TRUE;
#endif  /* SOON */
	    lineFileReuse(lf);
	    chain = chainReadChainLine(lf);
	    if (rangeIntersection(chain->tStart, chain->tEnd, start, end) > 0)
		{
		chainReadBlocks(lf, chain);
		slAddHead(&chainList, chain);
		}
	    else
	        chainFree(&chain);
	    }
#ifdef SOON	/* Put in if we index. */
	else if (gotChrom)
	    break;	/* We assume file is sorted by chromosome, so we're done. */
#endif /* SOON */
	}
    }
lineFileClose(&lf);
slReverse(&chainList);
return chainList;
}

static struct chain *chainLoadAndTrimIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region, and trim them
 * to fit region. */
{
struct chain *rawList, *chainList = NULL, *chain, *next;
rawList = chainLoadIntersecting(fileName, chrom, start, end);
for (chain = rawList; chain != NULL; chain = next)
    {
    struct chain *subChain, *chainToFree;
    next = chain->next;
    chainSubsetOnT(chain, start, end, &subChain, &chainToFree);
    if (subChain != NULL)
	slAddHead(&chainList, subChain);
    if (chainToFree != NULL)
        chainFree(&chain);
    }
slSort(&chainList, chainCmpScore);
return chainList;
}
/**** SHOULD BE IN LIBRARY - code from hgConvert.c ******/

void apiLiftOver(char *words[MAX_PATH_INFO])
/* 'liftOver' function words[1] is the subCommand */
{
char *extraArgs = verifyLegalArgs(argLiftOver);
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /liftOver'%s'", extraArgs);

if (sameWordOk("listExisting", words[1]))
    {
    listExisting();
    return;
    }

char *fromGenome = cgiOptionalString(argFromGenome);
char *toGenome = cgiOptionalString(argToGenome);
char *chrom = cgiOptionalString(argChrom);
char *start = cgiOptionalString(argStart);
char *end = cgiOptionalString(argEnd);

if (isEmpty(fromGenome) || isEmpty(toGenome) || isEmpty(chrom) || isEmpty(start) || isEmpty(end))
    apiErrAbort(err400, err400Msg, "must have all arguments: %s, %s, %s, %s, %s  for endpoint '/liftOver", argFromGenome, argToGenome, argChrom, argStart, argEnd);

unsigned uStart = 0;
unsigned uEnd = 0;
uStart = sqlUnsigned(start);
uEnd = sqlUnsigned(end);
if (uEnd < uStart)
    apiErrAbort(err400, err400Msg, "given start coordinate %u is greater than given end coordinate", uStart, uEnd);

struct dbDb *fromDb = hDbDb(fromGenome);
if (fromDb == NULL)
    {
    fromDb = genarkLiftOverDb(fromGenome);
    }
if (fromDb == NULL)
    apiErrAbort(err400, err400Msg, "can not find 'fromGenome=%s' for endpoint '/liftOver", fromGenome);
struct dbDb *toDb = hDbDb(toGenome);
if (toDb == NULL)
    {
    toDb = genarkLiftOverDb(toGenome);
    }
if (toDb == NULL)
    apiErrAbort(err400, err400Msg, "can not find 'toGenome=%s' for endpoint '/liftOver", toGenome);

char *fileName = liftOverChainFile(fromDb->name, toDb->name);
if (isEmpty(fileName))
    apiErrAbort(err400, err400Msg, "Unable to find a chain file from %s to %s - please contact support", fromGenome, toGenome);
fileName = hReplaceGbdbMustDownload(fileName);
char fromPos[4096];
safef(fromPos, sizeof(fromPos), "%s:%u-%u", chrom, uStart, uEnd);
char *nChrom;
int nStart, nEnd;
if (!hgParseChromRange(NULL, fromPos, &nChrom, &nStart, &nEnd))
    apiErrAbort(err400, err400Msg, "position %s is not in chrom:start-end format", fromPos);
int origSize = nEnd - nStart;
struct chain *chainList = chainLoadAndTrimIntersecting(fileName, nChrom, nStart, nEnd);
if (chainList == NULL)
    apiErrAbort(err400, err400Msg, "Sorry, this position %s is not found in the %s assembly", fromPos, toGenome);
chainListOut(fromGenome, toGenome, origSize, fromPos, chainList);
}
