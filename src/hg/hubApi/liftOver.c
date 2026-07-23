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
#include "wikiLink.h"
#include "userdata.h"

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

if (isNotEmpty(fromDb) && isNotEmpty(toDb))
    {
    /* match a chain recorded in either direction */
    sqlDyStringPrintf(query, "SELECT * FROM %s WHERE "
        "(LOWER(fromDb) = LOWER('%s') AND LOWER(toDb) = LOWER('%s')) "
        "OR (LOWER(fromDb) = LOWER('%s') AND LOWER(toDb) = LOWER('%s'))",
        tableName, fromDb, toDb, toDb, fromDb);
    }
else if (isNotEmpty(fromDb) || isNotEmpty(toDb))
    {
    sqlDyStringPrintf(query, "SELECT * FROM %s WHERE ", tableName);
    if (isNotEmpty(fromDb))
        sqlDyStringPrintf(query, "LOWER(fromDb) = LOWER('%s') ", fromDb);
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
int chainListCount = slCount(chainList);
jsonWriteNumber(jw, "itemsReturned", chainListCount);
liftOverChainFreeList(&chainList);

/* if no chain rows for this pair, check ottoRequest for any existing
 * row (any status) so the user is told their pair has already been
 * submitted instead of being allowed to create a duplicate row */
if (chainListCount == 0 && isNotEmpty(fromDb) && isNotEmpty(toDb))
    {
    char *ottoTable = cfgOption("ottoTable");
    if (isNotEmpty(ottoTable))
        {
        struct sqlConnection *ottoConn = hConnectOtto();
        if (sqlTableExists(ottoConn, ottoTable))
            {
            struct dyString *pq = newDyString(0);
            sqlDyStringPrintf(pq,
                "SELECT id, status, requestTime FROM %s "
                "WHERE requestType='liftOver' AND "
                "((fromDb='%s' AND toDb='%s') OR (fromDb='%s' AND toDb='%s')) "
                "ORDER BY requestTime DESC LIMIT 1",
                ottoTable, fromDb, toDb, toDb, fromDb);
            char **row;
            struct sqlResult *sr = sqlGetResult(ottoConn, dyStringCannibalize(&pq));
            if ((row = sqlNextRow(sr)) != NULL)
                {
                jsonWriteBoolean(jw, "pending", TRUE);
                jsonWriteNumber(jw, "pendingStatus", sqlSigned(row[1]));
                jsonWriteString(jw, "pendingRequestTime", row[2]);
                }
            sqlFreeResult(&sr);
            }
        hDisconnectOtto(&ottoConn);
        }
    }

apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static char *thisHostName()
/* Return this machine's own hostname via gethostname().  Unlike hHttpHost(),
 * which reflects the client-supplied HTTP_HOST/Host: header, this can't be
 * spoofed by the request and doesn't change depending on which round-robin
 * name (e.g. genome.ucsc.edu) the client used to reach this box -- using
 * hHttpHost() here made onGenomeRRMachine() misclassify RR machines reached
 * via the genome.ucsc.edu name, sending them into an infinite self-relay
 * loop in fetchGbMembersFromCentral(). */
{
static char host[256];
static boolean init = FALSE;
if (!init)
    {
    if (gethostname(host, sizeof(host)) != 0)
        host[0] = '\0';
    init = TRUE;
    }
return host;
}

static boolean inUcscEduDomain()
/* Return TRUE if this host is anywhere under the ucsc.edu domain. */
{
char *hostName = thisHostName();
return (hostName[0] != '\0' && endsWith(hostName, ".ucsc.edu"));
}

static boolean onGenomeRRMachine()
/* Return TRUE if running on one of the genome.ucsc.edu round-robin
 * machines (hgw0, hgw1, hgw2, ...), which carry SQL grants on
 * hgcentral.gbMembers.  Only meaningful within inUcscEduDomain(). */
{
if (hIsPrivateHost())	// stay on localhost for hgwdev and hgwbeta
    return TRUE;
char *hostName = thisHostName();
if (hostName[0] == '\0' || !startsWith("hgw", hostName))
    return FALSE;
char afterHgw = hostName[3];
return (afterHgw >= '0' && afterHgw <= '9');
}

static boolean fetchGbMembersFromCentral(char *userName, char **retEmail, char **retRealName)
/* Relay to genome.ucsc.edu's own loginStatus endpoint to get email/realName
 * for userName, forwarding this request's Cookie header so genome.ucsc.edu
 * authenticates the same session.  Used on ucsc.edu hosts that lack SQL
 * grants on hgcentral.gbMembers.  Returns FALSE on any failure. */
{
char *cookieHeader = getenv("HTTP_COOKIE");
if (isEmpty(cookieHeader))
    return FALSE;

struct dyString *reqHeader = dyStringNew(0);
dyStringPrintf(reqHeader, "Cookie: %s\r\n", cookieHeader);
char *url = "https://genome.ucsc.edu/cgi-bin/hubApi/liftOver/loginStatus";
int sd = netOpenHttpExt(url, "GET", reqHeader->string);
dyStringFree(&reqHeader);
if (sd < 0)
    return FALSE;

char *redirectedUrl = NULL;
if (!netSkipHttpHeaderLinesWithRedirect(sd, url, &redirectedUrl))
    {
    close(sd);
    return FALSE;
    }

struct dyString *body = netSlurpFile(sd);
close(sd);
struct jsonElement *json = jsonParse(body->string);
dyStringFree(&body);
if (json == NULL)
    return FALSE;

char *email = jsonStringField(json, "email");
char *realName = jsonStringField(json, "realName");
*retEmail = cloneString(email ? email : "");
*retRealName = cloneString(realName ? realName : "");
return TRUE;
}

static void loginStatus()
/* output current user login status as JSON */
{
/* wikiLinkUserName() handles all cookie validation internally */
char *userName = wikiLinkUserName();
struct jsonWrite *jw = apiStartOutput();
char hgLoginLink[2048];
boolean privateHost = hIsPrivateHost();
/* can not use hgcentral hglogin from hgwdev/genome-test */
if (privateHost)
    safef(hgLoginLink, sizeof(hgLoginLink), "%shgLogin", hLocalHostCgiBinUrl());
else
    safef(hgLoginLink, sizeof(hgLoginLink), "%shgLogin", hLoginHostCgiBinUrl());

if (userName != NULL)
    {
    // Get both email and realName from gbMembers table
    char *email = NULL;
    char *realName = NULL;

    if (inUcscEduDomain() && !onGenomeRRMachine())
        {
        // dev sandboxes, hgwbeta, etc: no local grants on gbMembers,
        // relay to genome.ucsc.edu instead
        fetchGbMembersFromCentral(userName, &email, &realName);
        }
    else
        {
        // RR machines, and anything entirely outside ucsc.edu: unchanged
        struct sqlConnection *sc = NULL;
        if (privateHost)
	    sc = hConnectCentral();
        else
	    sc = hConnectOtto();
        struct dyString *query = sqlDyStringCreate("select email, realName from gbMembers where userName = '%s'", userName);
        struct sqlResult *sr = sqlGetResult(sc, dyStringCannibalize(&query));
        char **row = sqlNextRow(sr);

        if (row != NULL)
            {
            email = cloneString(row[0] ? row[0] : "");
            realName = cloneString(row[1] ? row[1] : "");
            }
        sqlFreeResult(&sr);
        if (privateHost)
	    hDisconnectCentral(&sc);
        else
	    hDisconnectOtto(&sc);
        }

    // Build logout URL with returnto parameter
    char *returnTo = cgiOptionalString("returnTo");
    struct dyString *logoutUrl = dyStringNew(0);
    dyStringPrintf(logoutUrl, "%s?hgLogin.do.displayLogout=1", hgLoginLink);
    if (isNotEmpty(returnTo))
        {
        char *encodedReturnUrl = cgiEncodeFull(returnTo);

        dyStringPrintf(logoutUrl, "&returnto=%s", encodedReturnUrl);
        freeMem(encodedReturnUrl);
        }

    jsonWriteString(jw, "userName", userName);
    jsonWriteString(jw, "email", email ? email : "");
    jsonWriteString(jw, "realName", realName ? realName : "");
    jsonWriteString(jw, "logoutUrl", dyStringCannibalize(&logoutUrl));

    if (email)
        freeMem(email);
    if (realName)
        freeMem(realName);
    }
else
    {
    jsonWriteString(jw, "userName", NULL);
    // Use returnTo parameter passed by calling JavaScript
    char *returnTo = cgiOptionalString("returnTo");
    struct dyString *loginUrl = dyStringNew(0);
    dyStringPrintf(loginUrl, "%s?hgLogin.do.displayLoginPage=1", hgLoginLink);
    struct dyString *signUpUrl = dyStringNew(0);
    dyStringPrintf(signUpUrl, "%s?hgLogin.do.displaySignupPage=1", hgLoginLink);
    if (isNotEmpty(returnTo))
        {
        char *encodedReturnUrl = cgiEncodeFull(returnTo);
        dyStringPrintf(loginUrl, "&returnto=%s", encodedReturnUrl);
        jsonWriteString(jw, "loginUrl", dyStringCannibalize(&loginUrl));
        freeMem(encodedReturnUrl);
        }
    else
        {
        // No returnTo provided, just give basic login URL
        jsonWriteString(jw, "loginUrl", dyStringCannibalize(&loginUrl));
        }
    jsonWriteString(jw, "signupUrl", dyStringCannibalize(&signUpUrl));
    }

apiFinishOutput(0, NULL, jw);
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
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /liftOver '%s'", extraArgs);

if (sameWordOk("listExisting", words[1]))
    {
    listExisting();
    return;
    }
else if (sameWordOk("loginStatus", words[1]))
    {
    loginStatus();
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
}	/*	void apiLiftOver(char *words[MAX_PATH_INFO])	*/

// char *argLiftRequest[] = {argFromGenome, argToGenome, argEmail, argComment, NULL};
void apiLiftRequest(char *words[MAX_PATH_INFO])
/* 'liftOver' function words[1] is the subCommand */
{
char *extraArgs = verifyLegalArgs(argLiftRequest);
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /liftRequest '%s'", extraArgs);

char *fromGenome = cgiOptionalString(argFromGenome);
char *toGenome = cgiOptionalString(argToGenome);
char *email = cgiOptionalString(argEmail);
char *comment = cgiOptionalString(argComment);

/* probably want a silent exit here */
if (isEmpty(fromGenome) || isEmpty(toGenome) || isEmpty(email) || isEmpty(comment))
    apiErrAbort(err400, err400Msg, "must have all arguments: %s, %s, %s, %s for endpoint '/liftRequest", argFromGenome, argToGenome, argEmail, argComment);

/* Require a session cookie.  Robots that have not
 *   passed the challenge will not have one. */
char *cookieName = hUserCookie();
char *userId = findCookieData(cookieName);
if (isEmpty(userId))
    apiErrAbort(err400, err400Msg, "can not find required inputs for endpoint '/liftRequest");

/* verify (again) that the requested assemblies actually exist */
struct dbDb *fromDb = hDbDb(fromGenome);
if (fromDb == NULL)
    {
    fromDb = genarkLiftOverDb(fromGenome);
    }
struct dbDb *toDb = hDbDb(toGenome);
if (toDb == NULL)
    {
    toDb = genarkLiftOverDb(toGenome);
    }
if ( (fromDb == NULL) || (fromDb == NULL) )
    {
    if ( (fromDb == NULL) && (toDb == NULL) )
	    apiErrAbort(err400, err400Msg, "can not find either 'fromGenome=%s' or 'toGenome=%s' for endpoint '/liftOver", fromGenome, toGenome);
        else
	    apiErrAbort(err400, err400Msg, "can not find 'fromoGenome=%s' for endpoint '/liftOver", fromGenome);
    if (toDb == NULL)
        apiErrAbort(err400, err400Msg, "can not find 'toGenome=%s' for endpoint '/liftOver", toGenome);
    }

/* duplicate-row guard: any existing row in ottoRequest for this pair
 * (either direction, any status) blocks resubmission.  The form's JS
 * already shows a "pending" panel for this case via the listExisting
 * endpoint; this is the backstop for clients that bypass the form. */
{
char *dupOttoTable = cfgOption("ottoTable");
if (isNotEmpty(dupOttoTable))
    {
    struct sqlConnection *conn = hConnectOtto();
    if (sqlTableExists(conn, dupOttoTable))
        {
        struct dyString *dq = newDyString(0);
        sqlDyStringPrintf(dq,
            "SELECT COUNT(*) FROM %s WHERE requestType='liftOver' AND "
            "((fromDb='%s' AND toDb='%s') OR (fromDb='%s' AND toDb='%s'))",
            dupOttoTable, fromGenome, toGenome, toGenome, fromGenome);
        int dupCount = sqlQuickNum(conn, dyStringCannibalize(&dq));
        hDisconnectOtto(&conn);
        if (dupCount > 0)
            apiErrAbort(err409, err409Msg,
                "A request for %s <-> %s has already been submitted "
                "and is on record.  Duplicates are not accepted.",
                fromGenome, toGenome);
        }
    else
        hDisconnectOtto(&conn);
    }
}

/* per-email daily rate limit, per requestType, calendar-day server time */
char *limitStr = cfgOption("liftDailyLimit");
int dailyLimit = isNotEmpty(limitStr) ? atoi(limitStr) : 0;
if (dailyLimit > 0)
    {
    char *limitOttoTable = cfgOption("ottoTable");
    if (isNotEmpty(limitOttoTable))
        {
        struct sqlConnection *conn = hConnectOtto();
        if (sqlTableExists(conn, limitOttoTable))
            {
            struct dyString *q = newDyString(0);
            sqlDyStringPrintf(q,
                "SELECT COUNT(*) FROM %s "
                "WHERE requestType='liftOver' AND email='%s' "
                "AND DATE(requestTime) = CURDATE()",
                limitOttoTable, email);
            int todayCount = sqlQuickNum(conn, dyStringCannibalize(&q));
            hDisconnectOtto(&conn);
            if (todayCount >= dailyLimit)
                apiErrAbort(err429, err429Msg,
                    "Daily limit reached: %d liftOver requests per day. "
                    " Please try again tomorrow.",
                    dailyLimit);
            }
        else
            hDisconnectOtto(&conn);
        }
    }

char *toAddr = cfgOption("chainFileRequestEmail");
char *fromAddr = cfgOption("apiFromEmail");

if (isNotEmpty(toAddr) && isNotEmpty(fromAddr))
    {
    char nowTime[256];
    time_t seconds = clock1();
    struct tm *timeNow = localtime(&seconds);
    strftime(nowTime, sizeof nowTime, "%Y-%m-%d %H:%M:%S", timeNow);

    struct dyString *msg = newDyString(0);
    /* may need to encode these inputs to make them safe */
    dyStringPrintf(msg, "%s\nLift over request\nfrom: %s\nto: %s\nemail '%s'\ncomment: '%s'", nowTime, fromGenome, toGenome, email, comment);

    /* some kind of response here back to the request page */
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "msg", dyStringCannibalize(&msg));
    apiFinishOutput(0,NULL,jw);
    char *ottoTable = cfgOption("ottoTable");	/* probably ottoRequest */
    if (isNotEmpty(ottoTable))
        {
        struct sqlConnection *conn = hConnectOtto();
        if (sqlTableExists(conn, ottoTable))
	    {
            /* Atomic insert with duplicate check - prevents race condition */
            struct dyString *update = newDyString(0);
            sqlDyStringPrintf(update,
                "INSERT INTO %s (requestType, fromDb, toDb, email, comment, requestTime, status, buildDir) "
                "SELECT 'liftOver', '%s', '%s', '%s', '%s', now(), 0, '' "
                "WHERE NOT EXISTS ("
                "  SELECT 1 FROM %s WHERE requestType='liftOver' AND "
                "  ((fromDb='%s' AND toDb='%s') OR (fromDb='%s' AND toDb='%s'))"
                ")",
                ottoTable, fromGenome, toGenome, email, comment,
                ottoTable, fromGenome, toGenome, toGenome, fromGenome);
            int rowsAffected = sqlUpdateRows(conn, dyStringCannibalize(&update), NULL);
            if (rowsAffected == 0)
                {
                hDisconnectOtto(&conn);
                apiErrAbort(err409, err409Msg,
                    "A request for %s <-> %s has already been submitted "
                    "and is on record.  Duplicates are not accepted.",
                    fromGenome, toGenome);
                }
	    }
        hDisconnectOtto(&conn);
        }
    }
}	/*	void apiLiftRequest(char *words[MAX_PATH_INFO])	*/
