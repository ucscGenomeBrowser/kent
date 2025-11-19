/* hgBlat - CGI-script to manage fast human genome sequence searching. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include <pthread.h>
#include "common.h"
#include "errAbort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "web.h"
#include "hash.h"
#include "botDelay.h"
#include "trashDir.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "errCatch.h"
#include "portable.h"
#include "portable.h"
#include "dystring.h"
#include "chromInfo.h"
#include "net.h"
#include "fuzzyFind.h"
#include "chromAlias.h"
#include "subText.h"
#include "jsHelper.h"

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;
boolean orgChange = FALSE;
boolean dbChange = FALSE;
boolean allGenomes = FALSE;
boolean allResults = FALSE;
boolean autoRearr = FALSE;
static long enteredMainTime = 0;


boolean autoBigPsl = FALSE;  // DEFAULT VALUE change to TRUE in future

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   0.5    /* standard penalty is 1.0 for most CGIs */
                                /* this one is 0.5 */
static boolean issueBotWarning = FALSE;

struct gfResult
/* Detailed gfServer results, this is a span of several nearby tiles, minimum 2 for dna. */
    {
    struct gfResult *next;
    /* have to multiply translated coordinates by 3 */
    int qStart;    /* Query Start Coordinate */  
    int qEnd;      /* Query End Coordinate */
    char *chrom;   /* Target Chrom Name */
    int tStart;    /* Target Start Coordinate */  
    int tEnd;      /* Target End Coordinate */
    int numHits;   /* number of tile hits, minimum  2 for dna */ 
    char tStrand;  /* + or - Target Strand used with prot, rnax, dnax */ 
    int tFrame;    /* Target Frame 0,1,2 (mostly ignorable?) used with prot, rnax, dnax */ 
    int qFrame;    /* Query  Frame 0,1,2 (mostly ignorable?) used with rnax, dnax*/ 
   
    char qStrand;  /* + or - Query Strand used with prot, rnax, dnax, 
		      given by caller rather than returned by gfServer. */ 
    };

struct genomeHits
/* Information about hits on a genome assembly */
    {
    struct genomeHits *next;
    char *host;		/* Host. */
    char *port;	        /* Port. */
    char *db;		/* Database name. */
    char *genome;	/* Genome name. */
    int seqNumber;      /* Submission order */
    char *faName;       /* fasta name */
    char *dna;          /* query dna */
    int dnaSize;        /* query dna size */
    char *type;         /* query type = query, protQuery, transQuery */
    char *xType;        /* query type = dna, prot, rnax, dnax */
    boolean queryRC;    /* is the query reverse-complemented */
    boolean complex;    /* is the query complex */
    boolean isProt;     /* is the protein query */
    boolean isDynamic;  /* is a dynamic server */
    char *genomeDataDir; /* dynamic server root-relative directory */
   
    int maxGeneHits;    /* Highest gene hit-count */
    char *maxGeneChrom; /* Target Chrom for gene with max gene hits */
    int maxGeneChromSize; /* Target Chrom Size for only prot, rnax, dnax */
    int maxGeneTStart;  /* Target Start Coordinate for gene with max hits */  
    int maxGeneTEnd;    /* Target End Coordinate for gene with max hits*/
    int maxGeneExons;   /* Number of Exons in gene with max hits */
    char maxGeneStrand[3]; /* + or - or ++ +- -+ -- Strand for gene with max hits */ 
    char maxGeneTStrand;/* + or - TStrand for gene with max hits */ 
    boolean done;       /* Did the job get to finish */
    boolean error;      /* Some error happened */
    char *networkErrMsg;      /* Network layer error message */
    struct dyString *dbg;     /* Output debugging info */
    struct gfResult *gfList;  /* List of gfResult records */
    boolean hide;      /* To not show both strands, suppress the weaker-scoring one */
    };

boolean debuggingGfResults = FALSE; //TRUE;

int gfResultsCmp(const void *va, const void *vb)
/* Compare two gfResults. */
{
const struct gfResult *a = *((struct gfResult **)va);
const struct gfResult *b = *((struct gfResult **)vb);
int result = a->tStrand - b->tStrand;
if (result == 0)
    {
    result = strcmp(a->chrom, b->chrom);
    if (result == 0)
	{
	return (a->tStart - b->tStart);
	}
    else
	return result;
    }
else
    return result;
}

int rcPairsCmp(const void *va, const void *vb)
/* Recombine Reverse-complimented Pairs (to hide the weaker one). */
{
const struct genomeHits *a = *((struct genomeHits **)va);
const struct genomeHits *b = *((struct genomeHits **)vb);
// int result = strcmp(a->faName, b->faName); order by faName alphabetical
int result = a->seqNumber - b->seqNumber;
if (result == 0)
    {
    result = strcmp(a->db, b->db);
    if (result == 0)
	{
	return (a->queryRC - b->queryRC); 
	}
    else
	return result;
    }
else
    return result;
}


int genomeHitsCmp(const void *va, const void *vb)
/* Compare two genomeHits. */
{
const struct genomeHits *a = *((struct genomeHits **)va);
const struct genomeHits *b = *((struct genomeHits **)vb);
// int result = strcmp(a->faName, b->faName); order by faName alphabetical
int result = a->seqNumber - b->seqNumber;
if (result == 0)
    {
    if (a->error && b->error)
	return 0;
    else if (b->error && !a->error)
	return -1;
    else if (!b->error && a->error)
	return 1;
    else
	{
	return (b->maxGeneHits - a->maxGeneHits);
	}
    }
else
    return result;
}

// === parallel code ====

void queryServerFinish(struct genomeHits *gH);   // Forward declaration

static pthread_mutex_t pfdMutex = PTHREAD_MUTEX_INITIALIZER;
static struct genomeHits *pfdList = NULL, *pfdRunning = NULL, *pfdDone = NULL, *pfdNeverStarted = NULL;

static void *remoteParallelLoad(void *x)
/* Each thread loads tracks in parallel until all work is done. */
{
struct genomeHits *pfd = NULL;
boolean allDone = FALSE;
while(1)
    {
    pthread_mutex_lock( &pfdMutex );
    if (!pfdList)
	{
	allDone = TRUE;
	}
    else
	{  // move it from the waiting queue to the running queue
	pfd = slPopHead(&pfdList);
	slAddHead(&pfdRunning, pfd);
        }
    pthread_mutex_unlock( &pfdMutex );
    if (allDone)
	return NULL;

    if (!pfd->networkErrMsg)  // we may have already had a connect error.
	{
	/* protect against errAbort */
	struct errCatch *errCatch = errCatchNew();
	if (errCatchStart(errCatch))
	    {
	    pfd->done = FALSE;

	    // actually do the parallel work
	    queryServerFinish(pfd);

	    pfd->done = TRUE;
	    }
	errCatchEnd(errCatch);
	if (errCatch->gotError)
	    {
	    pfd->networkErrMsg = cloneString(errCatch->message->string);
	    pfd->error = TRUE;
	    pfd->done = TRUE;
	    }
	errCatchFree(&errCatch);
	}

    pthread_mutex_lock( &pfdMutex );
    slRemoveEl(&pfdRunning, pfd);  // this list will not be huge
    slAddHead(&pfdDone, pfd);
    pthread_mutex_unlock( &pfdMutex );

    }
}

static int remoteParallelLoadWait(int maxTimeInSeconds)
/* Wait, checking to see if finished (completed or errAborted).
 * If timed-out or never-ran, record error status.
 * Return error count. */
{
int maxTimeInMilliseconds = 1000 * maxTimeInSeconds;
struct genomeHits *pfd;
int errCount = 0;
int waitTime = 0;
while(1)
    {
    sleep1000(50); // milliseconds, give the threads some time to work
    waitTime += 50;
    // check if they are done
    boolean done = TRUE;
    pthread_mutex_lock( &pfdMutex );
    if (pfdList || pfdRunning)  // lists not empty, stuff to do still
	done = FALSE;
    pthread_mutex_unlock( &pfdMutex );
    if (done)
        break;
    // check if max allowed time has been exceeded
    if (waitTime >= maxTimeInMilliseconds)
        break;
    }
pthread_mutex_lock( &pfdMutex );
pfdNeverStarted = pfdList;
pfdList = NULL;  // stop the workers from starting any more waiting track loads
for (pfd = pfdNeverStarted; pfd; pfd = pfd->next)
    {
    // query was never even started
    char temp[1024];
    safef(temp, sizeof temp, "Ran out of time (%d milliseconds) unable to process %s %s", maxTimeInMilliseconds, pfd->genome, pfd->db);
    pfd->networkErrMsg = cloneString(temp);
    pfd->error = TRUE;
    ++errCount;
    }
for (pfd = pfdRunning; pfd; pfd = pfd->next)
    {
    // unfinished query
    char temp[1024];
    safef(temp, sizeof temp, "Timeout %d milliseconds exceeded processing %s %s", maxTimeInMilliseconds, pfd->genome, pfd->db);
    pfd->networkErrMsg = cloneString(temp);
    pfd->error = TRUE;
    ++errCount;
    }
for (pfd = pfdDone; pfd; pfd = pfd->next)
    {
    // some done queries may have errors
    if (pfd->error)
        ++errCount;
    }
slCat(pfdDone, pfdRunning);
pfdRunning = NULL;
slCat(pfdDone, pfdNeverStarted);
pfdNeverStarted = NULL;
pthread_mutex_unlock( &pfdMutex );
return errCount;
}

// ==================

int nonHubDynamicBlatServerCount = 0;
int hubDynamicBlatServerCount   = 0;

struct serverTable
/* Information on a server. */
    {
    char *db;		/* Database name. */
    char *genome;	/* Genome name. */
    boolean isTrans;	/* Is tranlated to protein? */
    char *host;		/* Name of machine hosting server. */
    char *port;		/* Port that hosts server. */
    char *nibDir;	/* Directory of sequence files. */
    int tileSize;       /* gfServer -tileSize */
    int stepSize;       /* gfServer -stepSize */
    int minMatch;       /* gfServer -minMatch */
    boolean isDynamic;  /* is a dynamic server */
    char* genomeDataDir; /* genome name for dynamic gfServer  */
    };

char *typeList[] = {"BLAT's guess", "DNA", "protein", "translated RNA", "translated DNA"};
char *outputList[] = {"hyperlink", "psl", "psl no header", "JSON"};

int minMatchShown = 0;

static struct serverTable *databaseServerTable(char *db, boolean isTrans)
/* Load blat table for a database */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
struct sqlResult *sr;
char **row;
char dbActualName[32];

/* If necessary convert database description to name. */
sqlSafef(query, sizeof(query), "select name from dbDb where name = '%s'", db);
if (!sqlExists(conn, query))
    {
    sqlSafef(query, sizeof(query), "select name from dbDb where description = '%s'", db);
    if (sqlQuickQuery(conn, query, dbActualName, sizeof(dbActualName)) != NULL)
        db = dbActualName;
    }

struct serverTable *st;
AllocVar(st);

/* Do a little join to get data to fit into the serverTable and grab
 * dbDb.nibPath too.  Check for newer dynamic flag and allow with or without
 * it.
 * For debugging, one set the variable blatServersTbl to some db.table to
 * pick up settings from somewhere other than dbDb.blatServers.
 */
char *blatServersTbl = cfgOptionDefault("blatServersTbl", "blatServers");
boolean haveDynamic = sqlColumnExists(conn, blatServersTbl, "dynamic");
sqlSafef(query, sizeof(query), "select dbDb.name, dbDb.description, blatServers.isTrans,"
         "blatServers.host, blatServers.port, dbDb.nibPath, %s "
         "from dbDb, %s blatServers where blatServers.isTrans = %d and "
         "dbDb.name = '%s' and dbDb.name = blatServers.db", 
         (haveDynamic ? "blatServers.dynamic" : "0"), blatServersTbl, isTrans, db);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    errAbort("Can't find a server for %s database %s.  Click "
	     "<A HREF=\"/cgi-bin/hgBlat?%s&command=start&db=%s\">here</A> "
	     "to reset to default database.",
	     (isTrans ? "translated" : "DNA"), db,
	     cartSidUrlString(cart), hDefaultDb());
    }
st->db = cloneString(row[0]);
st->genome = cloneString(row[1]);
st->isTrans = atoi(row[2]);
st->host = cloneString(row[3]);
st->port = cloneString(row[4]);
st->nibDir = hReplaceGbdbSeqDir(row[5], st->db);
if (atoi(row[6]))
    {
    st->isDynamic = TRUE;
    st->genomeDataDir = cloneString(st->db);  // directories by database name for database genomes
    ++nonHubDynamicBlatServerCount;
    }

sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return st;
}

static struct serverTable *trackHubServerTable(char *db, boolean isTrans)
/* Load blat table for a hub */
{
char *host, *port;
char *genomeDataDir;

if (!trackHubGetBlatParams(db, isTrans, &host, &port, &genomeDataDir))
    return NULL;

struct serverTable *st;
AllocVar(st);

st->db = cloneString(db);
st->genome = cloneString(hGenome(db));
st->isTrans = isTrans;
st->host = host; 
st->port = port;
struct trackHubGenome *genome = trackHubGetGenome(db);
st->nibDir = cloneString(genome->twoBitPath);
char *ptr = strrchr(st->nibDir, '/');
// we only want the directory name
if (ptr != NULL)
    *ptr = 0;
if (genomeDataDir != NULL)
    {
    st->isDynamic = TRUE;
    st->genomeDataDir = cloneString(genomeDataDir);
    ++hubDynamicBlatServerCount;
    }
return st;
}

struct serverTable *findServer(char *db, boolean isTrans)
/* Return server for given database.  Db can either be
 * database name or description. */
{
if (trackHubDatabase(db))
    return trackHubServerTable(db, isTrans);
else
    return databaseServerTable(db, isTrans);
}

void findClosestServer(char **pDb, char **pOrg)
/* If db doesn't have a blat server, look for the closest db (or org) that has one,
 * as hgPcr does. */
{
char *db = *pDb, *org = *pOrg;

if (trackHubDatabase(db) && (trackHubServerTable(db, FALSE) != NULL))
    {
    *pDb = db;
    *pOrg = hGenome(db);
    return;
    }

struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof(query), "select db from blatServers where db = '%s'", db);
if (!sqlExists(conn, query))
    {
    sqlSafef(query, sizeof(query), "select blatServers.db from blatServers,dbDb "
	  "where blatServers.db = dbDb.name and dbDb.genome = '%s'", org);
    char *db = sqlQuickString(conn, query);
    if (db == NULL)
	{
	sqlSafef(query, sizeof(query), "select blatServers.db from blatServers,dbDb "
	      "where blatServers.db = dbDb.name order by dbDb.orderKey,dbDb.name desc");
	char *db = sqlQuickString(conn, query);
	if (db == NULL)
	    errAbort("central database tables blatServers and dbDb are disjoint/empty");
	else
	    {
	    *pDb = db;
	    *pOrg = hGenome(db);
	    }
	}
    else
	{
	*pDb = db;
	*pOrg = hGenome(db);
	}
    }
hDisconnectCentral(&conn);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBlat - CGI-script to manage fast human genome sequence searching\n");
}

int countSameNonDigit(char *a, char *b)
/* Return count of characters in a,b that are the same
 * up until first digit in either one. */
{
char cA, cB;
int same = 0;
for (;;)
   {
   cA = *a++;
   cB = *b++;
   if (cA != cB)
       break;
   if (cA == 0 || cB == 0)
       break;
   if (isdigit(cA) || isdigit(cB))
       break;
   ++same;
   }
return same;
}

boolean allDigits(char *s)
/* Return TRUE if s is all digits */
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}


void printLuckyRedirect(char *browserUrl, struct psl *psl, char *database, char *pslName, 
			char *faName, char *uiState, char *unhideTrack)
/* Print out a very short page that redirects us. */
{
char url[1024];
/* htmlStart("Redirecting"); */
/* Odd it appears that we've already printed the Content-Typ:text/html line
   but I can't figure out where... */
if (autoBigPsl)
    {
    // skip ss variable
    safef(url, sizeof(url), "%s?position=%s:%d-%d&db=%s&%s%s",
      browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
      uiState, unhideTrack);
    jsInlineF("luckyLocation = '%s';\n", url);
   }
else
    {
    safef(url, sizeof(url), "%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s%s",
      browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
      pslName, faName, uiState, unhideTrack);
    htmStart(stdout, "Redirecting"); 
    jsInlineF("location.replace('%s');\n", url);
    printf("<noscript>No javascript support:<br>Click <a href='%s'>here</a> for browser.</noscript>\n", url);
    htmlEnd();
    }
}


/* forward declaration to reduce churn */
static void getCustomName(char *database, struct cart *cart, struct psl *psl, char **pName, char **pDescription);

void showAliPlaces(char *pslName, char *faName, char *customText, char *database, 
           enum gfType qType, enum gfType tType, 
           char *organism, boolean feelingLucky)
/* Show all the places that align. */
{
boolean useBigPsl = cfgOptionBooleanDefault("useBlatBigPsl", TRUE);
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl;
char *browserUrl = hgTracksName();
char *hgcUrl = hgcName();
char uiState[64];
char *vis;
char unhideTrack[64];
char *sort = cartUsualString(cart, "sort", pslSortList[0]);
char *output = cartUsualString(cart, "output", outputList[0]);
boolean pslOut = startsWith("psl", output);
boolean pslRawOut = sameWord("pslRaw", output);
boolean jsonOut = sameWord(output, "json");

sprintf(uiState, "%s=%s", cartSessionVarName(), cartSessionId(cart));

/* If user has hidden BLAT track, add a setting that will unhide the 
   track if user clicks on a browser link. */
vis = cartOptionalString(cart, "hgUserPsl");
if (vis != NULL && sameString(vis, "hide"))
    snprintf(unhideTrack, sizeof(unhideTrack), "&hgUserPsl=dense");
else
    unhideTrack[0] = 0;

while ((psl = pslNext(lf)) != NULL)
    {
    if (psl->match >= minMatchShown)
	slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
if (pslList == NULL && !jsonOut)
    {
    printf("<table><tr><td><hr>Sorry, no matches found");
    if (!allResults)
	printf(" (with a score of at least %d)", minMatchShown);
    printf("<hr><td></tr></table>\n");
    return;
    }

pslSortListByVar(&pslList, sort);

if (feelingLucky && autoBigPsl)
    {  // ignore and pass-through to hyperlink.
    pslOut = FALSE;
    jsonOut = FALSE;    
    }


if(feelingLucky && ! autoBigPsl)  // IDEA test pslOut and jsonOut
    {
    /* If we found something jump browser to there. */
    if(slCount(pslList) > 0)
	printLuckyRedirect(browserUrl, pslList, database, pslName, faName, uiState, unhideTrack);
    /* Otherwise call ourselves again not feeling lucky to print empty 
       results. */
    else 
	{
	cartWebStart(cart, trackHubSkipHubName(database), "%s (%s) BLAT Results", trackHubSkipHubName(organism), trackHubSkipHubName(database));
	showAliPlaces(pslName, faName, customText, database, qType, tType, organism, FALSE);
	cartWebEnd();
	}
    }
else if (pslOut)
    {
    if (!pslRawOut)
        printf("<TT><PRE>");
    if (!sameString(output, "psl no header"))
	pslxWriteHead(stdout, qType, tType);

    for (psl = pslList; psl != NULL; psl = psl->next)
	pslTabOut(psl, stdout);

    if (pslRawOut)
        exit(0);
    printf("<TT><PRE>");
    printf("</PRE></TT>");
    }
else if (jsonOut)  
    {
    webStartText();
    pslWriteAllJson(pslList, stdout, database, TRUE);
    exit(0);
    }
else  // hyperlink
    {
    printf("<H2>BLAT Search Results</H2>");
    char* posStr = cartOptionalString(cart, "position");
    if (posStr != NULL)
        printf("<P>Go back to <A HREF=\"%s\">%s</A> on the Genome Browser.</P>\n", browserUrl, posStr);

    if (autoBigPsl)
	{
        char *trackName = NULL;
        char *trackDescription = NULL;
        getCustomName(database, cart, pslList, &trackName, &trackDescription);

        psl = pslList;
        char item[1024];
        safef(item, sizeof item, "%s %s %s", pslName,faName,psl->qName);

        struct dyString *url = dyStringNew(256);
        dyStringPrintf(url, "http%s://%s", sameOk(getenv("HTTPS"), "on") ? "s" : "", getenv("HTTP_HOST"));
        dyStringPrintf(url, "%s",    hgcUrl+2);
        dyStringPrintf(url, "?o=%d", psl->tStart);
        dyStringPrintf(url, "&t=%d", psl->tEnd);
        dyStringPrintf(url, "&g=%s", "buildBigPsl");
        dyStringPrintf(url, "&i=%s", cgiEncode(item));
        dyStringPrintf(url, "&c=%s", cgiEncode(psl->tName));
        dyStringPrintf(url, "&l=%d", psl->tStart);
        dyStringPrintf(url, "&r=%d", psl->tEnd);
        dyStringPrintf(url, "&%s=%s",  cartSessionVarName(), cartSessionId(cart));
        dyStringPrintf(url, "&Submit=%s", cgiEncode("Create a stable custom track with these results"));
        if (pslIsProtein(psl))
            dyStringPrintf(url, "&isProt=on");

        jsInlineF(
	    "var luckyLocation = '';\n"
        );
	if(feelingLucky)
	    {
	    /* If we found something jump browser to there. */
	    if(slCount(pslList) > 0)
	    printLuckyRedirect(browserUrl, pslList, database, pslName, faName, uiState, unhideTrack);
            }

	//  RENAME BLAT CT FORM
	// new re-submit code with new trackname and decription
        if (!feelingLucky)
	    {
	    printf("<div id=renameFormItem style='display: none'>\n");
	    printf("<FORM ACTION=>\n");
	    printf("<INPUT TYPE=SUBMIT NAME=Submit id='showRenameForm' VALUE=\"Rename Custom Track\">\n");
	    printf("</FORM>\n");
	    printf("</div>\n");

	    printf("<div id=renameForm style='display: none'>\n");
	    char *hgcUrl = hgcName();
	    printf( "<DIV STYLE=\"display:block;\"><FORM ACTION=\"%s\"  METHOD=\"%s\" NAME=\"customTrackForm\">\n", hgcUrl,cartUsualString(cart, "formMethod", "POST"));

	    printf("<TABLE><TR><TD>Custom track name: ");
	    cgiMakeTextVar( "trackName", "FAKETRACKNAME", 30); 
	    printf("</TD></TR>");

	    printf("<TR><TD> Custom track description: ");
	    cgiMakeTextVar( "trackDescription", "FAKETRACKDESCRIPTION",50);  // track description or longLabel
	    printf("</TD></TR>");

	    // remove the current blat ct so we can create a new one.
	    cgiMakeHiddenVar(CT_DO_REMOVE_VAR, "Remove Custom track");
	    cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, "FAKETRACKNAME");

	    printf("<TR><TD><INPUT TYPE=SUBMIT NAME=Submit id=submitTrackNameDescr VALUE=\"Submit\">\n");
	    puts("</TD></TR>");
	    printf("</TABLE></FORM></DIV>");

	    printf("</div>\n");
	    }

        if (!feelingLucky)
	    {
	    // REMOVE CT BUTTON FORM.
	    printf("<div id=deleteCtForm style='display: none'>\n");
	    printf("<FORM ACTION=\"%s?hgsid=%s&db=%s\" NAME=\"MAIN_FORM\" METHOD=%s>\n\n",
		hgTracksName(), cartSessionId(cart), database, cartUsualString(cart, "formMethod", "POST"));
	    cartSaveSession(cart);
	    cgiMakeButton(CT_DO_REMOVE_VAR, "Delete Custom Track");
	    cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, "FAKETRACKNAME");
	    printf("</FORM>\n");
	    printf("</div>\n");
	    }

        jsInlineF(
	    
	    "var ct_blat = '';\n"
	    "\n"
	    "function buildBigPslCtSuccess (content, status)\n"
	    "{ // Finishes the succesful creation of blat ct bigPsl.  Called by ajax return.\n"
	    "  // saves the ct name so it can be used later for rename or delete.\n"
	    "\n"
	    "var matchWord = '&table=';\n"
	    "var ct_blatPos = content.indexOf(matchWord) + matchWord.length;\n"
	    "\n"
	    "if (ct_blatPos >= 0)\n"
	    "    {\n"
	    "    var ct_blatPosEnd = content.indexOf('\"', ct_blatPos);\n"
	    "    ct_blat = content.slice(ct_blatPos, ct_blatPosEnd);\n"
	    "    if (luckyLocation == '')\n"
	    "        {\n"
	    "        $('input[name=\""CT_SELECTED_TABLE_VAR"\"]')[0].value = ct_blat;\n"
	    "        $('input[name=\""CT_SELECTED_TABLE_VAR"\"]')[1].value = ct_blat;\n"
	    "        }\n"
	    "    }\n"
	    "}\n"
	    "\n"
	    "function buildBigPslCt (url, trackName, trackDescription)\n"
	    "{ // call hgc to buildBigPsl from blat result.\n"
	    "\n"
	    "var cgiVars = 'trackName='+encodeURIComponent(trackName)+'&trackDescription='+encodeURIComponent(trackDescription);\n"
	    "if (!ct_blat !== '')\n"
	    "    {\n"  	
	    "    cgiVars += '&"CT_DO_REMOVE_VAR"='+encodeURIComponent('Remove Custom Track');\n"
	    "    cgiVars += '&"CT_SELECTED_TABLE_VAR"='+encodeURIComponent(ct_blat);\n"
	    "    }\n"  	
	    "\n"
	    "$.ajax({\n"
	    "    type: 'GET',\n"
	    "    url: url,\n"
	    "    data: cgiVars,\n"
	    "    dataType: 'html',\n"
	    "    trueSuccess: buildBigPslCtSuccess,\n"
	    "    success: catchErrorOrDispatch,\n"
	    "    error: errorHandler,\n"
	    "    cache: false,\n"
	    "    async: false\n"
	    "    });\n"
	    "}\n"
	    "\n"
	    "var url='%s';\n"
	    "var trackName='%s';\n"
	    "var trackDescription='%s';\n"
            "$(document).ready(function() {\n"
	    "\n"
	    "buildBigPslCt(url, trackName, trackDescription);\n"
	    "if (luckyLocation !== '')\n"
	    "    {\n"
            "    location.replace(luckyLocation);\n"
	    "    }\n"
	    "else\n"
	    "    {\n"
	    "    $('#renameFormItem')[0].style.display = 'block';\n"   
	    "    $('#deleteCtForm')[0].style.display = 'block';\n"   
	    "    }\n"
            "});\n", url->string, trackName, trackDescription);
 

            // RENAME CT JS CODE
	    if (!feelingLucky)
		jsInline("$('#showRenameForm').click(function(){\n"
		    "  $('#renameForm')[0].style.display = 'block';\n"
		    "  $('#renameFormItem')[0].style.display = 'none';\n"
		    "  $('#showRenameForm')[0].style.display = 'none';\n"
		    "  $('input[name=\"trackName\"]')[0].value = trackName;\n"
		    "  $('input[name=\"trackDescription\"]')[0].value = trackDescription;\n"
		    "return false;\n"
		    "});\n");

            // RENAME CT JS CODE
	    if (!feelingLucky)
		jsInline("$('#submitTrackNameDescr').click(function(){\n"
		    "  $('#renameForm')[0].style.display = 'none';\n"
		    "  $('#renameFormItem')[0].style.display = 'block';\n"
		    "  $('#showRenameForm')[0].style.display = 'block';\n"
		    "  trackName = $('input[name=\"trackName\"]')[0].value;\n"
		    "  trackDescription = $('input[name=\"trackDescription\"]')[0].value;\n"
		    "buildBigPslCt(url, trackName, trackDescription);\n"
		    "return false;\n"
		    "});\n");

        dyStringFree(&url);

       
        }
    else if (useBigPsl)
        {
        char *trackName = NULL;
        char *trackDescription = NULL;
        getCustomName(database, cart, pslList, &trackName, &trackDescription);
        psl = pslList;
        printf( "<DIV STYLE=\"display:block;\"><FORM ACTION=\"%s\"  METHOD=\"%s\" NAME=\"customTrackForm\">\n", hgcUrl,cartUsualString(cart, "formMethod", "POST"));
        printf("<INPUT TYPE=\"hidden\" name=\"o\" value=\"%d\" />\n",psl->tStart);
        printf("<INPUT TYPE=\"hidden\" name=\"t\" value=\"%d\" />\n",psl->tEnd);
        printf("<INPUT TYPE=\"hidden\" name=\"g\" value=\"%s\" />\n","buildBigPsl");
        printf("<INPUT TYPE=\"hidden\" name=\"i\" value=\"%s %s %s\" />\n",pslName,faName,psl->qName);
        printf("<INPUT TYPE=\"hidden\" name=\"c\" value=\"%s\" />\n",psl->tName);
        printf("<INPUT TYPE=\"hidden\" name=\"l\" value=\"%d\" />\n",psl->tStart);
        printf("<INPUT TYPE=\"hidden\" name=\"r\" value=\"%d\" />\n",psl->tEnd);
        printf("<INPUT TYPE=\"hidden\" name=\"%s\" value=\"%s\" />\n",  cartSessionVarName(), cartSessionId(cart));
        if (pslIsProtein(psl))
            printf("<INPUT TYPE=\"hidden\" name=\"isProt\" value=\"on\" />\n");

        printf("<TABLE><TR><TD>Custom track name: ");
        cgiMakeTextVar( "trackName", trackName, 30);
        printf("</TD></TR>");

        printf("<TR><TD> Custom track description: ");
        cgiMakeTextVar( "trackDescription", trackDescription,50);
        printf("</TD></TR>");
        printf("<TR><TD><INPUT TYPE=SUBMIT NAME=Submit VALUE=\"Create a stable custom track with these results\">\n");
        printInfoIcon("The BLAT results below are temporary and will be replaced by your next BLAT search. "
                "However, when saved as a custom track with the button on the left, BLAT results are stored on our "
                "servers and can be saved as stable session (View &gt; My Sessions) links that can be shared via email or in manuscripts. "
                "\n<p>We have never cleaned up the data under stable session links so far. "
                "To reduce track clutter in your own sessions, you can delete BLAT custom tracks from the main Genome Browser "
                "view using the little trash icon next to each custom track.</p>");
        puts("</TD></TR>");
        printf("</TABLE></FORM></DIV>");
        }

        boolean hasDb = sqlDatabaseExists(database);
        struct sqlConnection *locusConn = NULL;
        struct subText *subList = NULL;
        if (hasDb)
            {
	    struct sqlConnection *conn = hAllocConn(database);
            if (cfgOptionBooleanDefault("blatShowLocus", FALSE) && sqlTableExists(conn, "locusName") )
                {
                locusConn = hAllocConn(database);
                slSafeAddHead(&subList, subTextNew("ig:", "intergenic "));
                slSafeAddHead(&subList, subTextNew("ex:", "exon "));
                slSafeAddHead(&subList, subTextNew("in:", "intron "));
                slSafeAddHead(&subList, subTextNew("|", "-"));
                }
            hFreeConn(&conn);
            }

    printf("<DIV STYLE=\"display:block;\"><PRE>");

    // find maximum query name size for padding calculations and
    // find maximum target chrom name size for padding calculations
    int maxQChromNameSize = 0;
    int maxTChromNameSize = 0;
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	int qLen = strlen(psl->qName);
	maxQChromNameSize = max(maxQChromNameSize,qLen);
	int tLen = strlen(psl->tName);
	maxTChromNameSize = max(maxTChromNameSize,tLen);
	}
    maxQChromNameSize = max(maxQChromNameSize,5);
    maxTChromNameSize = max(maxTChromNameSize,5);

    printf("   ACTIONS ");
    if (locusConn)
        // 25 characters wide
        printf("               LOCUS ");

    printf("                 QUERY ");
    
    spaceOut(stdout, maxQChromNameSize - 5);

    printf("SCORE START   END QSIZE IDENTITY  CHROM ");
    spaceOut(stdout, maxTChromNameSize - 5);

    printf(" STRAND  START       END   SPAN\n");

    printf("----------------------------------------------------------------------------------------------------------");
    if (locusConn)
        repeatCharOut(stdout, '-', 25);
    repeatCharOut(stdout, '-', maxQChromNameSize - 5);
    repeatCharOut(stdout, '-', maxTChromNameSize - 5);

    printf("\n");

    for (psl = pslList; psl != NULL; psl = psl->next)
	{
        char *browserHelp = "Open a Genome Browser showing this match";
        char *helpText = "Open a Genome Browser with the BLAT results, but in a new internet browser tab";
        // XX putting SVG into C code like this is ugly. define somewhere? maybe have globals for these?
        char *icon = "<svg style='height:10px; padding-left:2px' xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'><!--!Font Awesome Free 6.5.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license/free Copyright 2024 Fonticons, Inc.--><path d='M320 0c-17.7 0-32 14.3-32 32s14.3 32 32 32h82.7L201.4 265.4c-12.5 12.5-12.5 32.8 0 45.3s32.8 12.5 45.3 0L448 109.3V192c0 17.7 14.3 32 32 32s32-14.3 32-32V32c0-17.7-14.3-32-32-32H320zM80 32C35.8 32 0 67.8 0 112V432c0 44.2 35.8 80 80 80H400c44.2 0 80-35.8 80-80V320c0-17.7-14.3-32-32-32s-32 14.3-32 32V432c0 8.8-7.2 16-16 16H80c-8.8 0-16-7.2-16-16V112c0-8.8 7.2-16 16-16H192c17.7 0 32-14.3 32-32s-14.3-32-32-32H80z'/></svg>";


	if (customText)
	    {
	    printf("<A TITLE='%s' HREF=\"%s?position=%s:%d-%d&db=%s&hgt.customText=%s&%s%s\">browser</A>&nbsp;",
		browserHelp, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		customText, uiState, unhideTrack);
	    printf("<A TITLE='%s' TARGET=_BLANK HREF=\"%s?position=%s:%d-%d&db=%s&hgt.customText=%s&%s\">new tab%s</A>&nbsp;",
		helpText, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		customText, unhideTrack, icon);
	    } 
	else 
	    {
	    if (autoBigPsl)
		{
		// skip ss variable
		printf("<A TITLE='%s' HREF=\"%s?position=%s:%d-%d&db=%s&%s%s\">browser</A>&nbsp;",
		    browserHelp, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		    uiState, unhideTrack);
		printf("<A TITLE='%s' TARGET=_BLANK HREF=\"%s?position=%s:%d-%d&db=%s&%s\">new tab%s</A>&nbsp;",
		    helpText, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		    unhideTrack, icon);
		}
	    else 
		{
		printf("<A TITLE='%s' HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s%s\">browser</A>&nbsp;",
		    browserHelp, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		    pslName, faName, uiState, unhideTrack);
		printf("<A TITLE='%s' TARGET=_BLANK HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s\">new tab%s</A>&nbsp;",
		    helpText, browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
		    pslName, faName, unhideTrack, icon);
		}
	    }
	printf("<A title='Show query sequence, genome hit and sequence alignment' "
                "HREF=\"%s?o=%d&g=htcUserAli&i=%s+%s+%s&c=%s&l=%d&r=%d&db=%s&%s\">", 
	    hgcUrl, psl->tStart, pslName, cgiEncode(faName), psl->qName,  psl->tName,
	    psl->tStart, psl->tEnd, database, uiState);
	printf("details</A> ");

        // print name of this locus
        if (locusConn)
            {
            struct sqlResult *sr = hRangeQuery(locusConn, "locusName", psl->tName, psl->tStart, psl->tEnd, NULL, 0);
            char **row;
            row = sqlNextRow(sr);
            if (row != NULL)
                {
                char *desc = row[4];
                char *descLong = subTextString(subList, desc);
                printf("%-25s", descLong);
                freeMem(descLong);
                }
            sqlFreeResult(&sr);
            }

	printf("%s",psl->qName);
	spaceOut(stdout, maxQChromNameSize - strlen(psl->qName));
	printf(" %5d %5d %5d %5d   %5.1f%%  ",
	    pslScore(psl), psl->qStart+1, psl->qEnd, psl->qSize,
	    100.0 - pslCalcMilliBad(psl, TRUE) * 0.1);
        char *displayChromName = chromAliasGetDisplayChrom(database, cart, psl->tName);
	printf("%s",displayChromName);
	spaceOut(stdout, maxTChromNameSize - strlen(displayChromName));
	printf("  %-2s  %9d %9d %6d",
	    psl->strand, psl->tStart+1, psl->tEnd,
	    psl->tEnd - psl->tStart);

        // if you modify this, also modify hgPcr.c:doQuery, which implements a similar feature
        char *seq = psl->tName;
        if (endsWith(seq, "_fix"))
            printf("   <A target=_blank HREF=\"../FAQ/FAQdownloads.html#downloadFix\">What is chrom_fix?</A>");
        else if (endsWith(seq, "_alt"))
            printf("   <A target=_blank HREF=\"../FAQ/FAQdownloads.html#downloadAlt\">What is chrom_alt?</A>");
        else if (endsWith(seq, "_random"))
            printf("   <A target=_blank HREF=\"../FAQ/FAQdownloads.html#download10\">What is chrom_random?</A>");
        else if (startsWith(seq, "chrUn"))
            printf("   <A target=_blank HREF=\"../FAQ/FAQdownloads.html#download11\">What is a chrUn sequence?</A>");
        printf("\n");
	}
    printf("</PRE>\n");
    webNewSection("Help");
    puts("<P style=\"text-align:left\"><A target=_blank HREF=\"../FAQ/FAQblat.html#blat1b\">Missing a match?</A><br>");
    puts("<A target=_blank HREF=\"../FAQ/FAQblat.html#blat1c\">What is chr_alt & chr_fix?</A></P>\n");
    puts("</DIV>\n");
    }
pslFreeList(&pslList);

}

void trimUniq(bioSeq *seqList)
/* Check that all seq's in list have a unique name.  Try and
 * abbreviate longer sequence names. */
{
struct hash *hash = newHash(0);
bioSeq *seq;

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    char *saferString = needMem(strlen(seq->name)+1);
    char *c, *s;

    /*	Some chars are safe to allow through, other chars cause
     *	problems.  It isn't necessarily a URL safe string that is
     *	being calculated here.  The original problem was a user had
     *	the fasta header line of:
     *	chr8|59823648:59825047|+
     *	The plus sign was being taken as the query name and this
     *	created problems as that name was passed on to hgc via
     *	the ss cart variable.  The + sign became part of a URL
     *	eventually.  This loop allows only isalnum and =_/.:;_|
     *	to get through as part of the header name.  These characters
     *	all proved to be safe as single character names, or all
     *	together.
     */
    s = saferString;
    for (c = seq->name; *c != '\0'; ++c)
	{
	if (c && (*c != '\0'))
	    {
	    if ( isalnum(*c) || (*c == '=') || (*c == '-') || (*c == '/') ||
		(*c == '.') || (*c == ':') || (*c == ';') || (*c == '_') ||
		    (*c == '|') )
		*s++ = *c;
	    }
	}
    *s = '\0';
    freeMem(seq->name);
    if (*saferString == '\0')
	{
	freeMem(saferString);
	saferString = cloneString("YourSeq");
	}
    seq->name = saferString;

    if (strlen(seq->name) > 14)	/* Try and get rid of long NCBI .fa cruft. */
        {
	char *nameClone = NULL;
	char *abbrv = NULL;
	char *words[32];
	int wordCount;
	boolean isEns = (stringIn("ENSEMBL:", seq->name) != NULL);

	nameClone = cloneString(seq->name);
	wordCount = chopString(nameClone, "|", words, ArraySize(words));
	if (wordCount > 1)	/* Looks like it's an Ensembl/NCBI 
		                 * long name alright. */
	    {
	    if (isEns)
		{
	        abbrv = words[0];
		if (abbrv[0] == 0) abbrv = words[1];
		}
	    else if (sameString(words[1], "dbSNP"))
	        {
		if (wordCount > 2)
		    abbrv = words[2];
		else
		    abbrv = nameClone;
		}
	    else
		{
		abbrv = words[wordCount-1];
		if (abbrv[0] == 0) abbrv = words[wordCount-2];
		}
	    if (hashLookup(hash, abbrv) == NULL)
	        {
		freeMem(seq->name);
		seq->name = cloneString(abbrv);
		}
	    freez(&nameClone);
	    }
	}
    if (hashLookup(hash, seq->name) != NULL)
        errAbort("The sequence identifier '%s' is duplicated in the input. "
                "FASTA sequence identifiers should be unique and they cannot contain spaces. "
                "You can make them unique by adding a suffix such as _1, _2, ... to the duplicated names, e.g. '%s_1'.", 
                seq->name, seq->name);
    hashAdd(hash, seq->name, hash);
    }
freeHash(&hash);
}

int realSeqSize(bioSeq *seq, boolean isDna)
/* Return size of sequence without N's or (for proteins)
 * X's. */
{
char unknown = (isDna ? 'n' : 'X');
int i, size = seq->size, count = 0;
char *s = seq->dna;
for (i=0; i<size; ++i)
    if (s[i] != unknown) ++count;
return count;
}

void uToT(struct dnaSeq *seqList)
/* Convert any u's in sequence to t's. */
{
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    subChar(seq->dna, 'u', 't');
}

static struct slName *namesInPsl(struct psl *psl)
/* Find all the unique names in a list of psls. */
{
struct hash *hash = newHash(3);
struct slName *nameList = NULL;
struct slName *name;
for(; psl; psl = psl->next)
    {
    if (hashLookup(hash, psl->qName) == NULL)
        {
        name = slNameNew(psl->qName);
        slAddHead(&nameList, name);
        hashStore(hash, psl->qName);
        }
    }
slReverse(&nameList);
return nameList;
}

static char *makeNameUnique(char *name, char *database, struct cart *cart)
/* Make sure track name will create a unique custom track. */
{
struct slName *browserLines = NULL;
struct customTrack *ctList = customTracksParseCart(database, cart, &browserLines, NULL);
struct customTrack *ct;
int count = 0;
char buffer[4096];
safef(buffer, sizeof buffer, "%s", name);

for(;;count++)
    {
    char *customName = customTrackTableFromLabel(buffer);
    for (ct=ctList;
         ct != NULL;
         ct=ct->next) 
        {
        if (startsWith(customName, ct->tdb->track))
            // Found a track with this name.
            break;
        }

    if (ct == NULL)
        break;

    safef(buffer, sizeof buffer, "%s (%d)", name, count + 1);
    }

return cloneString(buffer);
}

static void getCustomName(char *database, struct cart *cart, struct psl *psl, char **pName, char **pDescription)
// Find a track name that isn't currently a custom track. Also fill in description.
{
struct slName *names = namesInPsl(psl);
char shortName[4096];
char description[4096];

unsigned count = slCount(names);
if (count == 1)
    {
    safef(shortName, sizeof shortName, "blat %s", names->name);
    safef(description, sizeof description, "blat on %s",  names->name);
    }
else if (count == 2)
    {
    safef(shortName, sizeof shortName, "blat %s+%d", names->name, count - 1);
    safef(description, sizeof description, "blat on %d queries (%s, %s)", count, names->name, names->next->name);
    }
else
    {
    safef(shortName, sizeof shortName, "blat %s+%d", names->name, count - 1);
    safef(description, sizeof description, "blat on %d queries (%s, %s, ...)", count, names->name, names->next->name);
    }

*pName = makeNameUnique(shortName, database, cart);
*pDescription = cloneString(description);
}

void queryServer(char *host, char *port, char *db, struct dnaSeq *seq, char *type, char *xType,
    boolean complex, boolean isProt, boolean queryRC, int seqNumber, char *genomeDataDir)
/* Send simple query to server and report results. (no, it doesn't do this)
 * queryRC is true when the query has been reverse-complemented */
{

struct genomeHits *gH;
AllocVar(gH);

gH->host=cloneString(host);
gH->port=cloneString(port);
gH->db = cloneString(db);
gH->genome = cloneString(hGenome(db));
gH->seqNumber = seqNumber;
gH->faName = cloneString(seq->name);
gH->dna = cloneString(seq->dna);
gH->dnaSize = seq->size;
gH->type = cloneString(type);
gH->xType = cloneString(xType);
gH->queryRC = queryRC;
gH->complex = complex;
gH->isProt = isProt;
gH->isDynamic = (genomeDataDir != NULL);
gH->genomeDataDir = genomeDataDir;
gH->dbg = dyStringNew(256);

/* SKIP DYNAMIC SERVERS
 * xinetd throttles by refusing more connections, which causes queries to fail
 * when the configured limit is reached.  Rather than trying to throttle in the
 * client, dynamic servers are excluded. See issue #26658.
 */
if (gH->isDynamic)
    {
    gH->error = TRUE;
    gH->networkErrMsg = cloneString("Skipped Dynamic Server");
    slAddHead(&pfdDone, gH);
    }
else
    {
    slAddHead(&pfdList, gH);
    }
}

void findBestGene(struct genomeHits *gH, int queryFrame)
/* Find best gene-like object with multiple linked-features.
 * Remember chrom start end of best gene found and total hits in the gene. 
 * Should sort the gfResults by tStrand, chrom, tStart.
 * Filters on queryFrame */
{
char *bestChrom = NULL;
int bestHits   = 0;
int bestTStart = 0;
int bestTEnd   = 0;
int bestExons  = 0;
char bestTStrand = ' ';
char bestQStrand = ' ';

char *thisChrom = NULL;
int thisHits   = 0;
int thisTStart = 0;
int thisTEnd   = 0;
int thisExons  = 0;
char thisTStrand = ' ';
char thisQStrand = ' ';

int geneGap = ffIntronMaxDefault;  // about a million bases is a cutoff for genes.

int qSlop = 5; // forgive 5 bases, about dna stepSize = 5.
if (gH->complex)
    qSlop = 4; // reduce for translated with stepSize = 4.

struct gfResult *gfR = NULL, *gfLast = NULL;
for(gfR=gH->gfList; gfR; gfR=gfR->next)
    {
    // filter on given queryFrame
    if (gfR->qFrame != queryFrame)
	continue;

    if (gfLast && (gfR->tStrand == gfLast->tStrand) && sameString(gfR->chrom,gfLast->chrom) && 

	(gfR->qStart >= (gfLast->qEnd - qSlop)) && (gfR->tStart >= gfLast->tEnd) && ((gfR->tStart - gfLast->tEnd) < geneGap))
	{	
	thisHits += gfR->numHits;
	thisTEnd  = gfR->tEnd;
	thisExons += 1;
	}
    else
	{
	thisHits   = gfR->numHits;
	thisChrom  = gfR->chrom;
	thisTStart = gfR->tStart;
	thisTEnd   = gfR->tEnd;
	thisExons  = 1;
	thisTStrand = gfR->tStrand;
	thisQStrand = gfR->qStrand;
	}
    if (thisHits > bestHits)
	{
	bestHits   = thisHits;
	bestExons  = thisExons;
	bestChrom  = thisChrom;
	bestTStart = thisTStart;
	bestTEnd   = thisTEnd;
	bestTStrand = thisTStrand;
	bestQStrand = thisQStrand;
	}
    gfLast = gfR;
    }

// save best gene with max hits
gH->maxGeneHits   = bestHits;
gH->maxGeneChrom  = cloneString(bestChrom);
gH->maxGeneTStart = bestTStart;
gH->maxGeneTEnd   = bestTEnd;
gH->maxGeneExons  = bestExons;
gH->maxGeneTStrand = bestTStrand;  // tells if we need to change to get pos strand coords.
if (gH->complex)
    {
    safef(gH->maxGeneStrand, sizeof gH->maxGeneStrand, "%c%c", bestQStrand, bestTStrand);
    }
else
    {
    safef(gH->maxGeneStrand, sizeof gH->maxGeneStrand, "%c", bestQStrand);
    }
}

void unTranslateCoordinates(struct genomeHits *gH)
/* convert translated coordinates back to dna for all but prot query */
{
if (!gH->complex)
    return;
int qFactor = 3;
int tFactor = 3;
if (gH->isProt)
    qFactor = 1;
struct gfResult *gfR = NULL;
for(gfR=gH->gfList; gfR; gfR=gfR->next)
    {
    gfR->qStart = gfR->qStart * qFactor + gfR->qFrame;
    gfR->qEnd   = gfR->qEnd   * qFactor + gfR->qFrame;
    gfR->tStart = gfR->tStart * tFactor + gfR->tFrame;
    gfR->tEnd   = gfR->tEnd   * tFactor + gfR->tFrame;
    }
}

void queryServerFinish(struct genomeHits *gH)
/* Report results from gfServer. */
{
char buf[256];
int matchCount = 0;

struct gfConnection *conn = gfMayConnect(gH->host, gH->port, trackHubDatabaseToGenome(gH->db), gH->genomeDataDir);
if (conn == NULL)
    {
    gH->error = TRUE;
    gH->networkErrMsg = "Connection to gfServer failed.";
    return;
    }

dyStringPrintf(gH->dbg,"query strand %s qsize %d<br>\n", gH->queryRC ? "-" : "+", gH->dnaSize);

/* Put together query command. */
if (gH->isDynamic)
    safef(buf, sizeof buf, "%s%s %s %s %d", gfSignature(), gH->type,
          conn->genome, conn->genomeDataDir, gH->dnaSize);
else
    safef(buf, sizeof buf, "%s%s %d", gfSignature(), gH->type, gH->dnaSize);
gfBeginRequest(conn);
mustWriteFd(conn->fd, buf, strlen(buf));

if (read(conn->fd, buf, 1) < 0)
    errAbort("queryServerFinish: read failed: %s", strerror(errno));
if (buf[0] != 'Y')
    errAbort("Expecting 'Y' from server, got %c", buf[0]);
mustWriteFd(conn->fd, gH->dna, gH->dnaSize);  // Cannot shifted earlier for speed. must wait for Y confirmation.

if (gH->complex)
    {
    char *s = netRecieveString(conn->fd, buf);
    if (!s)
	errAbort("expected response from gfServer with tileSize");
    dyStringPrintf(gH->dbg,"%s<br>\n", s);  // from server: tileSize 4
    }

for (;;)
    {
    if (netGetString(conn->fd, buf) == NULL)
        break;
    if (sameString(buf, "end"))
        {
        dyStringPrintf(gH->dbg,"%d matches<br>\n", matchCount);
        break;
        }
    else if (startsWith("Error:", buf))
       {
       errAbort("%s", buf);
       break;
       }
    else
        {
        dyStringPrintf(gH->dbg,"%s<br>\n", buf);
	// chop the line into words
	int numHits = 0;
	char *line = buf;
	struct gfResult *gfR = NULL;
	AllocVar(gfR);
	gfR->qStrand = (gH->queryRC ? '-' : '+');

	char *word[10];
	int fields = chopLine(line,word);
	int expectedFields = 6;
	if (gH->complex)
	    {
	    if (gH->isProt)
		expectedFields = 8;
	    else
		expectedFields = 9;
	    }
	if (fields != expectedFields)
	    errAbort("expected %d fields, got %d fields", expectedFields, fields);

	gfR->qStart = sqlUnsigned(word[0]); // e.g. 3139
	gfR->qEnd   = sqlUnsigned(word[1]); // e.g. 3220

	// e.g. hg38.2bit:chr1
	char *colon = strchr(word[2], ':');
	if (colon)
	    {
	    gfR->chrom = cloneString(colon+1);
	    }
	else
	    { // e.g. chr10.nib
	    char *dot = strchr(word[2], '.');
	    if (dot)
		{
		*dot = 0;
		gfR->chrom = cloneString(word[2]);
		}
	    else
		errAbort("Expecting colon or period in the 3rd field of gfServer response");
	    }

	gfR->tStart = sqlUnsigned(word[3]); // e.g. 173515
	gfR->tEnd   = sqlUnsigned(word[4]); // e.g. 173586

	numHits     = sqlUnsigned(word[5]); // e.g. 14

	// Frustrated with weird little results with vastly exaggerated hit-score,
	// I have flattened that out so it maxes out at #tiles that could fit 
	// It seems to still work just fine. I am going to leave it for now.
	// One minor note, by suppressing extra scores for short exons,
	// it will not prefer alignments with the same number of hits, but more exons.
	if (!gH->complex) // dna xType
	    {
	    // maximum tiles that could fit in qSpan
	    int limit = ((gfR->qEnd - gfR->qStart) - 6)/5;  // stepSize=5
	    ++limit;  // for a little extra.
	    if (numHits > limit)
		numHits = limit;
	    }

	gfR->numHits = numHits;

	if (gH->complex)
	    {
	    gfR->tStrand = word[6][0];          // e.g. + or -
	    gfR->tFrame = sqlUnsigned(word[7]); // e.g. 0,1,2
	    if (!gH->isProt)
		{
		gfR->qFrame = sqlUnsigned(word[8]); // e.g. 0,1,2
		}
	    }
	else
	    {
	    gfR->tStrand = '+';  // dna search only on + target strand
	    }
	
        if (gH->complex)
            {
            char *s = netGetLongString(conn->fd);
            if (s == NULL)
                break;
            dyStringPrintf(gH->dbg,"%s<br>\n", s); //dumps out qstart1 tstart1 qstart2 tstart2 ...  
            freeMem(s);
            }

	slAddHead(&gH->gfList, gfR);
        }
    ++matchCount;
    }
slReverse(&gH->gfList);

unTranslateCoordinates(gH);  // convert back to untranslated coordinates
slSort(&gH->gfList, gfResultsCmp);  // sort by tStrand, chrom, tStart



struct qFrameResults
/* Information about hits on a genome assembly */
    {
    int maxGeneHits;    /* Highest gene hit-count */
    char *maxGeneChrom; /* Target Chrom for gene with max gene hits */
    int maxGeneTStart;  /* Target Start Coordinate for gene with max hits */  
    int maxGeneTEnd;    /* Target End Coordinate for gene with max hits*/
    int maxGeneExons;   /* Number of Exons in gene with max hits */
    char maxGeneTStrand;/* + or - TStrand for gene with max hits */ 
    };

int qFrame = 0;
int qFrameStart = 0;
int qFrameEnd = 0;

if (gH->complex && !gH->isProt)  // rnax, dnax
    {
    qFrameEnd = 2;
    }

struct qFrameResults r[3]; 

for(qFrame = qFrameStart; qFrame <= qFrameEnd; ++qFrame)
    {	    
    findBestGene(gH, qFrame);  // find best gene-like thing with most hits
    
    r[qFrame].maxGeneHits = gH->maxGeneHits;
    r[qFrame].maxGeneChrom = cloneString(gH->maxGeneChrom);
    r[qFrame].maxGeneTStart = gH->maxGeneTStart;
    r[qFrame].maxGeneTEnd = gH->maxGeneTEnd;
    r[qFrame].maxGeneExons = gH->maxGeneExons;
    r[qFrame].maxGeneTStrand = gH->maxGeneTStrand;

    }

// combine the results from all 3 qFrames

if (gH->complex && !gH->isProt)  // rnax, dnax
    {
    int biggestHit = -1;
    int bigQFrame = -1;

    // find the qFrame with the biggest hits
    for(qFrame = qFrameStart; qFrame <= qFrameEnd; ++qFrame)
	{	    
	if (r[qFrame].maxGeneHits > biggestHit)
	    {
	    bigQFrame = qFrame;
            biggestHit = r[qFrame].maxGeneHits;
	    }
	}

    // save combined final answers back in gH
    gH->maxGeneHits = 0;
    gH->maxGeneTStrand = r[bigQFrame].maxGeneTStrand;
    gH->maxGeneChrom   = cloneString(r[bigQFrame].maxGeneChrom);
    gH->maxGeneExons   = r[bigQFrame].maxGeneExons;
    gH->maxGeneTStart  = r[bigQFrame].maxGeneTStart;
    gH->maxGeneTEnd    = r[bigQFrame].maxGeneTEnd;

    for(qFrame = qFrameStart; qFrame <= qFrameEnd; ++qFrame)
	{

	// ignore frames that do not have same tStrand, chrom, and overlap with the best
	if (!(
	    (r[qFrame].maxGeneTStrand == r[bigQFrame].maxGeneTStrand) && 
	    sameOk(r[qFrame].maxGeneChrom, r[bigQFrame].maxGeneChrom) && 
	    // overlap start,end between this and bigQFrame
	    (
	       (r[qFrame].maxGeneTEnd > r[bigQFrame].maxGeneTStart) &&
	    (r[bigQFrame].maxGeneTEnd >    r[qFrame].maxGeneTStart)
	    )
	    ))
	    {
	    continue;  // incompatible with best result, skip it
	    }

	// Add total hits
	gH->maxGeneHits += r[qFrame].maxGeneHits;  // should be pretty accurate
	// Find biggest exon count
	if (gH->maxGeneExons < r[qFrame].maxGeneExons)   // often underestimates actual exon count.
	    {
	    gH->maxGeneExons = r[qFrame].maxGeneExons; 
	    }
	// Adjust tStart
	if (gH->maxGeneTStart > r[qFrame].maxGeneTStart)
	    {
	    gH->maxGeneTStart = r[qFrame].maxGeneTStart; 
	    }
	// Adjust tEnd
	if (gH->maxGeneTEnd < r[qFrame].maxGeneTEnd)
	    {
	    gH->maxGeneTEnd = r[qFrame].maxGeneTEnd; 
	    }
		
	}

    gH->maxGeneHits /= 3;  // average over 3 frames.

    char qStrand = (gH->queryRC ? '-' : '+');
    safef(gH->maxGeneStrand, sizeof gH->maxGeneStrand, "%c%c", qStrand, gH->maxGeneTStrand);

    }

gfDisconnect(&conn);
}

int findMinMatch(long genomeSize, boolean isProt)
// Return default minMatch for genomeSize,
// the expected number of occurrences of string length k 
// in random genome of size N = N/(4^k)
{
int alphaBetSize;
if (isProt)
    {
    alphaBetSize = 20;
    genomeSize = genomeSize / 3;
    }
else
    {
    alphaBetSize = 4;
    }
int k = 1;
double expected = genomeSize;
for (k=1; k<36; k++)
    {
    expected /= alphaBetSize;
    // Set this to .19 to allow 18bp searches on hg18 (no effect on hg38?).
    if (expected < .004)
	break;
    }
return k;
}

long findGenomeSize(char *database)
// get genomeSize from database.
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
sqlSafef(query, sizeof query, "select sum(size) from chromInfo");
long genomeSize = sqlQuickLongLong(conn, query);
hFreeConn(&conn);
if (genomeSize == 0)
    {
    warn("Genome Size not found for %s", database);
    genomeSize = 3272116950;  // substitute human genome size
    }
return genomeSize;
}

long findGenomeSizeFromHub(char *database)
// fetch genome size by adding up chroms from hub 2bit.
{
struct trackHubGenome *genome = trackHubGetGenome(database);

genome->tbf = twoBitOpen(genome->twoBitPath);

long genomeSize = 0;
struct twoBitIndex *index;
for (index = genome->tbf->indexList; index != NULL; index = index->next)
    {
    genomeSize += twoBitSeqSize(genome->tbf, index->name);
    }

twoBitClose(&genome->tbf);

return genomeSize;
}


int findGenomeParams(struct gfConnection *conn, struct serverTable *serve)
/* Send status message to server arnd report result.
 * Get tileSize stepSize and minMatch.
 */
{
char buf[256];
int ret = 0;

/* Put together command. */
gfBeginRequest(conn);
if (serve->isDynamic)
    sprintf(buf, "%s%s %s %s", gfSignature(), (serve->isTrans ? "transInfo" : "untransInfo"),
            conn->genome, conn->genomeDataDir);
else
    sprintf(buf, "%sstatus", gfSignature());
mustWriteFd(conn->fd, buf, strlen(buf));

for (;;)
    {
    if (netGetString(conn->fd, buf) == NULL)
	{
	long et = clock1000() - enteredMainTime;
	if (serve->isDynamic)
	    {
	    if (et > NET_TIMEOUT_MS)
		warn("the dynamic blat service is taking too long to respond, probably overloaded at this time, try again later.  Error reading status information from %s:%s",
		serve->host, serve->port);
	    else if (et < NET_QUICKEXIT_MS)
		warn("the dynamic blat service is returning an error immediately. it is probably overloaded at this time, try again later.  Error reading status information from %s:%s",
		serve->host, serve->port);
	    else
		warn("the dynamic blat service is returning an error at this time, try again later.  Error reading status information from %s:%s",
		serve->host, serve->port);
	    }
	else
	    {
	    warn("Error reading status information from %s:%s; gfServer maybe down or misconfigured, see system logs for details)",
             serve->host, serve->port);
	    }
	ret = -1;
        break;
	}
    if (sameString(buf, "end"))
        break;
    else
        {
	if (startsWith("tileSize ", buf))
	    {
            serve->tileSize = atoi(buf+strlen("tileSize "));
	    }
	if (startsWith("stepSize ", buf))
	    {
            serve->stepSize = atoi(buf+strlen("stepSize "));
	    }
	if (startsWith("minMatch ", buf))
	    {
            serve->minMatch = atoi(buf+strlen("minMatch "));
	    }
        }
    }
gfEndRequest(conn);
return(ret); 
}

void blatSeq(char *userSeq, char *organism, char *database, int dbCount)
/* Blat sequence user pasted in. */
{
FILE *f;
struct dnaSeq *seqList = NULL, *seq;
struct tempName pslTn, faTn;
int maxSingleSize, maxTotalSize, maxSeqCount;
char *genome, *db;
char *type = cgiString("type");
char *seqLetters = cloneString(userSeq);
struct serverTable *serve;
struct gfConnection *conn = NULL;
int oneSize, totalSize = 0, seqCount = 0;
boolean isTx = FALSE;
boolean isTxTx = FALSE;
boolean txTxBoth = FALSE;
struct gfOutput *gvo;
boolean qIsProt = FALSE;
enum gfType qType, tType;
struct hash *tFileCache = gfFileCacheNew();
// allGenomes ignores I'm Feeling Lucky for simplicity
boolean feelingLucky = cgiBoolean("Lucky") && !allGenomes;

char *xType = NULL; 

if (allGenomes)
    {
    db = database;
    genome = organism;
    }
else
    getDbAndGenome(cart, &db, &genome, oldVars);

char *output = cgiOptionalString("output");
boolean isJson= sameWordOk(output, "json");
boolean isPslRaw= sameWordOk(output, "pslRaw");


if ((!feelingLucky && !allGenomes && !isJson && !isPslRaw) || (autoBigPsl && feelingLucky))
    cartWebStart(cart, db, "%s (%s) BLAT Results",  trackHubSkipHubName(organism), trackHubSkipHubName(db));

seqList = faSeqListFromMemTextRaw(seqLetters);

/* Load user sequence and figure out if it is DNA or protein. */
if (sameWord(type, "DNA"))
    {
    isTx = FALSE;
    xType = "dna"; 
    }
else if (sameWord(type, "translated RNA"))
    {
    isTx = TRUE;
    isTxTx = TRUE;
    xType = "rnax"; 
    }
else if (sameWord(type, "translated DNA"))
    {
    isTx = TRUE;
    isTxTx = TRUE;
    txTxBoth = TRUE;
    xType = "dnax"; 
    }
else if (sameWord(type, "protein"))
    {
    isTx = TRUE;
    qIsProt = TRUE;
    xType = "prot"; 
    }
else  // BLAT's Guess
    {
    if (seqList)
	{
	isTx = !seqIsDna(seqList); // only tests first element, assumes the rest are the same type.
	if (isTx)
	    {
	    qIsProt = TRUE;
	    xType = "prot"; 
	    }
	else
	    {
	    xType = "dna"; 
	    }
	}
    }
if (isTx && !isTxTx)
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	seq->size = aaFilteredSize(seq->dna);
	aaFilter(seq->dna, seq->dna);
	toUpperN(seq->dna, seq->size);
	}
    }
else
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	seq->size = dnaFilteredSize(seq->dna);
	dnaFilter(seq->dna, seq->dna);
	toLowerN(seq->dna, seq->size);
	subChar(seq->dna, 'u', 't');
	}
    }

if (seqList != NULL && seqList->name[0] == 0)
    {
    freeMem(seqList->name);
    seqList->name = cloneString("YourSeq");
    }
trimUniq(seqList);


/* If feeling lucky only do the first one. */
if(feelingLucky && seqList != NULL)
    {
    seqList->next = NULL;
    }

/* Figure out size allowed. */
maxSingleSize = (isTx ? 10000 : 75000);
maxTotalSize = maxSingleSize * 2.5;
#ifdef LOWELAB
maxSeqCount = 200;
#else
maxSeqCount = 25;
#endif
char *optionMaxSeqCount = cfgOptionDefault("hgBlat.maxSequenceCount", NULL);

if (isNotEmpty(optionMaxSeqCount))
   maxSeqCount = sqlSigned(optionMaxSeqCount);

/* Create temporary file to store sequence. */
trashDirFile(&faTn, "hgSs", "hgSs", ".fa");
faWriteAll(faTn.forCgi, seqList);

/* Create a temporary .psl file with the alignments against genome. */
trashDirFile(&pslTn, "hgSs", "hgSs", ".pslx");
f = mustOpen(pslTn.forCgi, "w");
gvo = gfOutputPsl(0, qIsProt, FALSE, f, FALSE, TRUE);


/* Strategy for calculating minMatchShown.
 Calculate the minimum size for filtering based on genome size.
 The expected numberof occurrences of q = n/(4^k)
 = N * pow(4,-k);  where N is genome size, and k is minimum size required.
 For protein, use 22 instead of 4.
 For human, a cutoff of 20, expected number < .003 or 0.3% 
 so the probability of getting it just by chance is low.
*/

int xlat;

serve = findServer(db, isTx);
/* Write header for extended (possibly protein) psl file. */
if (isTx)
    {
    if (isTxTx)
        {
	qType = gftDnaX;
	tType = gftDnaX;
        xlat = 3;
	}
    else
        {
	qType = gftProt;
	tType = gftDnaX;
        xlat = 1;
	}
    serve->tileSize = 4;
    serve->stepSize = 4;
    serve->minMatch = 3;
    }
else
    {
    qType = gftDna;
    tType = gftDna;
    serve->tileSize = 11;
    serve->stepSize = 5;
    serve->minMatch = 2;
    xlat = 1;
    }
pslxWriteHead(f, qType, tType);


long genomeSize = 3272116950;  // substitute human genome size

// Use with message about recommended length when using short query
// This is the minimum that can find anything at all.
int minSuggested = 0;

if (allGenomes)
    {
    minMatchShown = 0;
    }
else
    {
    #ifdef LOWELAB
    minMatchShown = 14;
    #else
    // read genome size
    if (trackHubDatabase(database))
	{
	genomeSize = findGenomeSizeFromHub(database);
	}
    else
	{
	genomeSize = findGenomeSize(database);
	}
    minMatchShown = findMinMatch(genomeSize, qType == gftProt);
    #endif
    if (allResults)
	minMatchShown = 0;

    conn = gfConnect(serve->host, serve->port, trackHubDatabaseToGenome(serve->db), serve->genomeDataDir);

    // read tileSize stepSize minMatch from server status
    findGenomeParams(conn, serve);

    int minLucky = (serve->minMatch * serve->stepSize + (serve->tileSize - serve->stepSize)) * xlat;

    minSuggested = max(minMatchShown,minLucky);
    }

int seqNumber = 0;
/* Loop through each sequence. */
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    printf(" "); fflush(stdout);  /* prevent apache cgi timeout by outputting something */
    oneSize = realSeqSize(seq, !isTx);
    // Impose half the usual bot delay per sequence
    
    if (dbCount == 0 && issueBotWarning)
        {
        char *ip = getenv("REMOTE_ADDR");
        botDelayMessage(ip, botDelayMillis);
        }

    if (++seqCount > maxSeqCount)
        {
	warn("More than %d input sequences, stopping at %s<br>(see also: cgi-bin/hg.conf hgBlat.maxSequenceCount setting).",
	    maxSeqCount, seq->name);
	break;
	}
    if (oneSize > maxSingleSize)
	{
	warn("Sequence %s is %d letters long (max is %d), skipping",
	    seq->name, oneSize, maxSingleSize);
	continue;
	}
    if (oneSize < minSuggested)
        {
	warn("Warning: Sequence %s is only %d letters long (%d is the recommended minimum).<br><br>"
                "To search for short sequences in the browser window, use the <a href='hgTrackUi?%s=%s&g=oligoMatch&oligoMatch=pack'>Short Sequence Match</a> track. "
                "You can also use our commandline tool <tt>findMotifs</tt> "
                "(see the <a target=_blank href='https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads'>utilities download page</a>) to "
                "search for sequences on the entire genome.<br><br>"
                "For primers, you can use the <a href='hgPcr?%s=%s'>In-silico PCR</a> tool. In-silico PCR can search the entire genome or a set of "
                "transcripts. In the latter case, it can find matches that straddle exon/intron boundaries.<br><br>"
                "<a href='../contacts.html'>Contact us</a> for additional help using BLAT or its related tools.",
		seq->name, oneSize, minSuggested, cartSessionVarName(), cartSessionId(cart), cartSessionVarName(), cartSessionId(cart));
	// we could use "continue;" here to actually enforce skipping, 
	// but let's give the short sequence a chance, it might work.
	// minimum possible length = tileSize+stepSize, so mpl=16 for dna stepSize=5, mpl=10 for protein.
	if (qIsProt && oneSize < 1) // protein does not tolerate oneSize==0
	    continue;
	}
    totalSize += oneSize;
    if (totalSize > maxTotalSize)
        {
	warn("Sequence %s would take us over the %d letter limit, stopping here.",
	     seq->name, maxTotalSize);
	break;
	}

    if (isTx)
	{
	gvo->reportTargetStrand = TRUE;
	if (isTxTx)
	    {
	    if (allGenomes)
		queryServer(serve->host, serve->port, db, seq, "transQuery", xType, TRUE, FALSE, FALSE, seqNumber,
                            serve->genomeDataDir);
	    else
		{
		gfAlignTransTrans(conn, serve->nibDir, seq, FALSE, 5, tFileCache, gvo, !txTxBoth);
		}
	    if (txTxBoth)
		{
		reverseComplement(seq->dna, seq->size);
		if (allGenomes)
		    queryServer(serve->host, serve->port, db, seq, "transQuery", xType, TRUE, FALSE, TRUE, seqNumber,
                                serve->genomeDataDir);
		else
		    {
		    gfAlignTransTrans(conn, serve->nibDir, seq, TRUE, 5, tFileCache, gvo, FALSE);
		    }
		}
	    }
	else
	    {
	    if (allGenomes)
		queryServer(serve->host, serve->port, db, seq, "protQuery", xType, TRUE, TRUE, FALSE, seqNumber,
                            serve->genomeDataDir);
	    else
		{
		gfAlignTrans(conn, serve->nibDir, seq, 5, tFileCache, gvo);
		}
	    }
	}
    else
	{
	if (allGenomes)
	    queryServer(serve->host, serve->port, db, seq, "query", xType, FALSE, FALSE, FALSE, seqNumber,
                        serve->genomeDataDir);
	else
	    {
	    gfAlignStrand(conn, serve->nibDir, seq, FALSE, minMatchShown, tFileCache, gvo);
	    }
	reverseComplement(seq->dna, seq->size);
	if (allGenomes)
	    queryServer(serve->host, serve->port, db, seq, "query", xType, FALSE, FALSE, TRUE, seqNumber,
                        serve->genomeDataDir);
	else
	    {
	    gfAlignStrand(conn, serve->nibDir, seq, TRUE, minMatchShown, tFileCache, gvo);
	    }
	}
    gfOutputQuery(gvo, f);
    ++seqNumber;
    }
carefulClose(&f);

if (!allGenomes)
    {
    showAliPlaces(pslTn.forCgi, faTn.forCgi, NULL, serve->db, qType, tType, 
              organism, feelingLucky);
    }

if ((!feelingLucky && !allGenomes) || (autoBigPsl && feelingLucky))
    cartWebEnd();

gfFileCacheFree(&tFileCache);
}

void askForSeq(char *organism, char *db)
/* Put up a little form that asks for sequence.
 * Call self.... */
{
/* ignore struct serverTable* return, but can error out if not found */
findServer(db, FALSE);

char *userSeq = NULL;
char *type = NULL;

printf( 
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n"
"<H2>BLAT Search Genome</H2>\n");
cartSaveSession(cart);
puts("\n");
puts("<INPUT TYPE=HIDDEN NAME=changeInfo VALUE=\"\">\n");
puts("<TABLE class='hgBlatTable' BORDER=0 WIDTH=80>\n");
printf("<TR>\n");
printf("<TD ALIGN=CENTER style='overflow:hidden;white-space:nowrap;'>Genome:");
printf(" <INPUT TYPE=CHECKBOX id=allGenomes NAME=allGenomes VALUE=\"\">");
printf(" <span id=searchAllText> Search all genomes</span>");
printf("</TD>");
// clicking on the Search ALL text clicks the checkbox.
jsOnEventById("click", "searchAllText", 
    "document.mainForm.allGenomes.click();"
    "return false;"   // cancel the default
    );

printf("<TD ALIGN=CENTER>Assembly:</TD>");
printf("<TD ALIGN=CENTER>Query type:</TD>");
printf("<TD ALIGN=CENTER>Sort output:</TD>");
printf("<TD ALIGN=CENTER>Output type:</TD>");
printf("<TD ALIGN=CENTER>&nbsp;</TD>");
printf("</TR>\n");

printf("<TR>\n");
printf("<TD class='searchCell' ALIGN=CENTER>\n");
// hgBlat requires this <input> be created to go along with form submission, we
// will change it when a genome is selected in the search bar
printf("<input name='db' value='%s' type='hidden'></input>\n", db);
jsIncludeAutoCompleteLibs();
char *searchPlaceholder = "Search any species, genome or assembly name";
char *searchBarId = "genomeSearch";
printGenomeSearchBar(searchBarId, searchPlaceholder, NULL, TRUE, NULL, NULL);
jsInlineF(
    "function blatSelect(selectEle, item) {\n"
    "   selectEle.innerHTML = item.label;\n"
    "   document.mainForm.db.value = item.genome;\n"
    "}\n\n"
    "document.addEventListener(\"DOMContentLoaded\", () => {\n"
    "    // bind the actual <select> to the function blatSelect, that way\n"
    "    // initSpeciesAutoCompleteDropdown can call the function\n"
    "    let selectEle = document.getElementById(\"genomeLabel\");\n"
    "    let boundSelect = blatSelect.bind(null, selectEle);\n"
    "    initSpeciesAutoCompleteDropdown('%s', boundSelect);\n"
    "    // make the search button trigger the autocomplete manually\n"
    "    let btn = document.getElementById(\"%sButton\");\n"
    "    btn.addEventListener(\"click\", () => {\n"
    "        let val = document.getElementById(\"%s\").value;\n"
    "        $(\"[id=\'%s\']\").autocompleteCat(\"search\", val);\n"
    "    });\n"
    "});\n"
    , searchBarId, searchBarId, searchBarId, searchBarId
);
printf("</TD>\n");
printf("<TD id='genomeLabel' class='searchCell' ALIGN=CENTER>\n");
char *dbLabel = getCurrentGenomeLabel(db);
printf("%s\n", dbLabel);
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
if (orgChange)
    type = cartOptionalString(cart, "type");
cgiMakeDropList("type", typeList, ArraySize(typeList), type);
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
cgiMakeDropList("sort", pslSortList, ArraySize(pslSortList), cartOptionalString(cart, "sort"));
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
cgiMakeDropList("output", outputList, ArraySize(outputList), cartOptionalString(cart, "output"));
printf("</TD>\n");
printf("</TR>\n<TR>\n");
userSeq = cartUsualString(cart, "userSeq", "");
printf("<TD COLSPAN=5 ALIGN=CENTER>\n");
htmlPrintf("<TEXTAREA NAME=userSeq ROWS=14 COLS=140>%s</TEXTAREA>\n", userSeq);
printf("</TD>\n");
printf("</TR>\n");

printf("<TR>\n");
printf("<TD COLSPAN=1 ALIGN=CENTER style='overflow:hidden;white-space:nowrap;font-size:0.9em'>\n");
cgiMakeCheckBoxWithId("allResults", allResults, "allResults");
printf("<span id=allResultsText>All Results (no minimum matches)</span>");
// clicking on the All Results text clicks the checkbox.
jsOnEventById("click", "allResultsText", 
    "document.mainForm.allResults.click();"
    "return false;"   // cancel the default
    );
printf("</TD>\n");

printf("<TD COLSPAN=1 ALIGN=CENTER style='overflow:hidden;white-space:nowrap;font-size:0.9em'>\n");
printf("<label for='autoRearr'>");
cgiMakeCheckBoxWithId("autoRearr", autoRearr, "autoRearr");
printf("<span> Optimize for Rearrangements </span>");
printf("</label>");
printInfoIcon("Rearrangement display (aka 'snakes' tracks) can show "
"duplications of the query sequence using multiple lines and lines between "
"fragments, see our "
"<a href='/goldenPath/help/chain.html#rearrangement'>"
"snakes documentation page"
"</a>"
"for more "
"details. You can also switch this on or off on the BLAT track configuration "
"page by checking the 'Rearrangement display' box.");
printf("</TD>\n");

printf("<TD COLSPAN=4 style='text-align:right'>\n");
printf("<INPUT style=' font-size:1.0em; width:100px' TYPE=SUBMIT NAME=Submit VALUE=Submit>\n");
printf("<INPUT style='font-size:1.0em' TYPE=SUBMIT NAME=Lucky VALUE=\"I'm feeling lucky\">\n");
printf("<INPUT style='font-size:1.0em' TYPE=SUBMIT NAME=Clear VALUE=Clear>\n");
printf("</TD>\n");
printf("</TR>\n");

printf("<TR>\n"); 
puts("<TD COLSPAN=5 WIDTH=\"100%\">\n" 
    "Paste in a query sequence to find its location in the\n"
    "the genome. Multiple sequences may be searched \n"
    "if separated by lines starting with '>' followed by the sequence name.\n"
    "</TD>\n"
    "</TR>\n"
);

puts("<TR><TD COLSPAN=5 WIDTH=\"100%\">\n"); 
puts("<BR><B>File Upload:</B> ");
puts("Rather than pasting a sequence, you can choose to upload a text file containing "
	 "the sequence.<BR>");
puts("Upload sequence: <INPUT TYPE=FILE NAME=\"seqFile\">");
puts(" <INPUT TYPE=SUBMIT Name=Submit VALUE=\"Submit file\"><P>\n");
printf("%s", 
"<P>Only DNA sequences of 25,000 or fewer bases and protein or translated \n"
"sequence of 10000 or fewer letters will be processed.  Up to 25 sequences\n"
"can be submitted at the same time. The total limit for multiple sequence\n"
"submissions is 50,000 bases or 25,000 letters.<br> A valid example "
"is <tt>GTCCTCGGAACCAGGACCTCGGCGTGGCCTAGCG</tt> (human SOD1).\n</P>\n");

printf("%s", 
"<P>The <b>Search all</b> checkbox allows you to search all genomes at the same time. "
"Search all is only available for default assemblies and attached hubs with dedicated BLAT servers."
"The new dynamic BLAT servers are not supported, and they are noted as skipped in the output. "
"<b>See our <a href='/FAQ/FAQblat.html#blat9'>BLAT All FAQ</a> for more information.</b>\n"
);

printf("<P>The <b>All Results</b> checkbox disables minimum matches filtering so all results are seen." 
" For example, with a human dna search, 20 is minimum matches required, based on the genome size, to filter out lower-quality results.\n"
"This checkbox can be useful with short queries and with the tiny genomes of microorganisms. \n"
);

printf("<P>If you are interested in programmatic BLAT use,"
" see our <a href=\"/FAQ/FAQblat.html#blat14\">BLAT FAQ</a>.</P>\n"
);

if (hgPcrOk(db))
    printf("<P>For locating PCR primers, use <A HREF=\"../cgi-bin/hgPcr?db=%s\">In-Silico PCR</A>"
           " for best results instead of BLAT. " 
           "To search for short sequences &lt; 20bp only in the sequence shown on the Genome Browser, "
           "use our <a href='hgTrackUi?%s=%s&g=oligoMatch&oligoMatch=pack'>Short Sequence Match</a> track, "
           "If you are using the command line and want to search the entire genome, our command line tool <tt>findMotifs</tt>, from the "
           "<a target=_blank href='https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads'>utilities download page</a>.</p>",
           db, cartSessionVarName(), cartSessionId(cart));
puts("</TD></TR></TABLE>\n");



printf("</FORM>\n");

webNewSection("About BLAT");
printf( 
"<P>BLAT on DNA is designed to\n"
"quickly find sequences of 95%% and greater similarity of length 25 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.  It will find\n"
"perfect sequence matches of 20 bases.\n"
"BLAT on proteins finds sequences of 80%% and greater similarity of length 20 amino\n"
"acids or more.  In practice DNA BLAT works well on primates, and protein\n"
"BLAT on land vertebrates."
);


printf("%s",
"\n</P><P>BLAT is not BLAST.  DNA BLAT works by keeping an index of the entire genome\n"
"in memory.  The index consists of all overlapping 11-mers stepping by 5 except for\n"
"those heavily involved in repeats.  The index takes up about\n"
"2 gigabytes of RAM.  RAM can be further reduced to less than 1 GB by increasing step size to 11.\n"
"The genome itself is not kept in memory, allowing\n"
"BLAT to deliver high performance on a reasonably priced Linux box.\n"
"The index is used to find areas of probable homology, which are then\n"
"loaded into memory for a detailed alignment. Protein BLAT works in a similar\n"
"manner, except with 4-mers rather than 11-mers.  The protein index takes a little\n"
"more than 2 gigabytes.</P>\n"
"<P>BLAT was written by <A HREF=\"mailto:kent@soe.ucsc.edu\">Jim Kent</A>.\n"
"Like most of Jim's software, interactive use on this web server is free to all.\n"
"Sources and executables to run batch jobs on your own server are available free\n"
"for academic, personal, and non-profit purposes.  Non-exclusive commercial\n"
"licenses are also available. See the \n"
"<A HREF=\"http://www.kentinformatics.com\" TARGET=_blank>Kent Informatics</A>\n"
"website for details.</P>\n"
"\n"
"<P>For more information on the graphical version of BLAT, click the Help \n"
"button on the top menu bar");
printf(" or see the Genome Browser <A HREF=\"../FAQ/FAQblat.html\">FAQ</A>. </P> \n"
"<P>Kent WJ. <a href=\"https://genome.cshlp.org/content/12/4/656.abstract\" \n"
"target=_blank>BLAT - the BLAST-like alignment tool</a>. \n" 
"Genome Res. 2002 Apr;12(4):656-64. PMID: 11932250</p>");

}

void fakeAskForSeqForm(char *organism, char *db)
/* Put up a hidden form that asks for sequence.
 * Call self.... */
{
char *userSeq = NULL;
char *type = NULL;
char *sort = NULL;
char *output = NULL;

printf("<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);
printf("<INPUT TYPE=HIDDEN NAME=org VALUE=\"%s\">\n", organism);
printf("<INPUT TYPE=HIDDEN NAME=db VALUE=\"%s\">\n", db);
type = cartUsualString(cart, "type", "");
printf("<INPUT TYPE=HIDDEN NAME=type VALUE=\"%s\">\n", type);
sort = cartUsualString(cart, "sort", "");
printf("<INPUT TYPE=HIDDEN NAME=sort VALUE=\"%s\">\n", sort);
output = cartUsualString(cart, "output", "");
printf("<INPUT TYPE=HIDDEN NAME=output VALUE=\"%s\">\n", output);
userSeq = cartUsualString(cart, "userSeq", "");
printf("<INPUT TYPE=HIDDEN NAME=userSeq VALUE=\"%s\">\n", userSeq);
printf("<INPUT TYPE=HIDDEN NAME=Submit VALUE=submit>\n"); 
printf("</FORM>\n");
}

void hideWeakerOfQueryRcPairs(struct genomeHits* gH1)
/* hide the weaker of the pair of rc'd query results
 * so users sees only one strand with the best gene hit. 
 * Input must be sorted already into the pairs. */
{
struct genomeHits* gH2 = NULL;
for (;gH1; gH1 = gH2->next)
    {
    gH2 = gH1->next;
    if (!gH2)
	errAbort("Hiding weaker of pairs found one without sibling.");
    if (!((gH1->seqNumber == gH2->seqNumber) && sameString(gH1->db, gH2->db) && (gH1->queryRC != gH2->queryRC)))
	errAbort("Error matching pairs, sibling does not match seqNumber and db.");
    // check if one or the other had an error
    if (gH1->error && gH2->error)
	gH2->hide = TRUE;  // arbitrarily
    else if (gH1->error && !gH2->error)
	gH1->hide = TRUE;
    else if (!gH1->error && gH2->error)
	gH2->hide = TRUE;
    else  // keep the best scoring of the pair, hide the other
	{
	if (gH2->maxGeneHits > gH1->maxGeneHits)
	    gH1->hide = TRUE;
	else
	    gH2->hide = TRUE;
	}
    }
}

void changeMaxGenePositionToPositiveStrandCoords(struct genomeHits *gH)
/* convert negative strand coordinates to positive strand coordinates if TStrand=='-' */
{
for (;gH; gH = gH->next)
    {
    if (gH->hide)
	continue;
    if (gH->error)
	continue;
    if (gH->maxGeneTStrand == '-')  // convert to pos strand coordinates
	{
	int chromSize = 0;
	char temp[1024];
	safef(temp, sizeof temp, "%s", "");
	if (trackHubDatabase(gH->db))  // if not hub db, make sure it is the default assembly.
	    {

	    struct chromInfo *ci = trackHubMaybeChromInfo(gH->db, gH->maxGeneChrom);
	    if (ci)
		chromSize = ci->size;
	    else
		{
		warn("chromosome %s missing from %s .2bit (%s).", gH->maxGeneChrom, gH->db, gH->genome);
		safef(temp, sizeof temp, "chromosome %s missing from %s .2bit.", gH->maxGeneChrom, gH->db);
		}
	    }
	else
	    {
	    struct sqlConnection *conn = hAllocConn(gH->db);
	    char query[256];
	    sqlSafef(query, sizeof query, "select size from chromInfo where chrom='%s'", gH->maxGeneChrom);
	    chromSize = sqlQuickNum(conn, query);
	    hFreeConn(&conn);
	    if (chromSize == 0)
		{
		warn("chromosome %s missing from %s.chromInfo (%s)", gH->maxGeneChrom, gH->db, gH->genome);
		safef(temp, sizeof temp, "chromosome %s missing from %s.chromInfo", gH->maxGeneChrom, gH->db);
		}
	    }
	if (chromSize == 0)
	    {
	    gH->error = TRUE;
	    gH->networkErrMsg = cloneString(temp);
	    }
	else
	    {
	    gH->maxGeneChromSize = chromSize;
	    int tempTStart = gH->maxGeneTStart;
	    gH->maxGeneTStart = chromSize - gH->maxGeneTEnd;
	    gH->maxGeneTEnd   = chromSize - tempTStart;
	    }
	}
    }
}

void printDebugging()
/* print detailed gfServer response info  */
{
struct genomeHits *gH;
printf("<PRE>\n");
for (gH = pfdDone; gH; gH = gH->next)
    {
    if (gH->hide) // hide weaker of pairs for dna and dnax with reverse-complimented queries.
	    continue;
    if (gH->error)
	printf("%s %s %s %s %d %s %s\n", 
	    gH->faName, gH->genome, gH->db, gH->networkErrMsg, gH->queryRC, gH->type, gH->xType);
    else
	{
	int tStart = gH->maxGeneTStart;
	int tEnd = gH->maxGeneTEnd;
	// convert back to neg strand coordinates for debug display
	if (gH->complex && (gH->maxGeneTStrand == '-'))
	    {
	    int tempTStart = tStart;
	    tStart = gH->maxGeneChromSize - tEnd;
	    tEnd   = gH->maxGeneChromSize - tempTStart;
	    }
	printf("%s %s %s %s %d %d %s %s %d %d\n", 
	    gH->faName, gH->genome, 
	    gH->db, 
	    gH->maxGeneChrom, gH->maxGeneHits, gH->queryRC, gH->type, gH->xType, tStart, tEnd);
	printf("%s", gH->dbg->string);

	struct gfResult *gfR = NULL;
	for(gfR=gH->gfList; gfR; gfR=gfR->next)
	    {
	    printf("(%3d)\t%4d\t%4d\t%s\t(%3d)\t%9d\t%9d\t%3d\t",
		(gfR->qEnd-gfR->qStart), 
	    gfR->qStart, gfR->qEnd, gfR->chrom, 
		(gfR->tEnd-gfR->tStart), 
	    gfR->tStart, gfR->tEnd, gfR->numHits);

	    if (gH->complex)
		{
		printf("%c\t%d\t", gfR->tStrand, gfR->tFrame);
		if (!gH->isProt)
		    {
		    printf("%d\t", gfR->qFrame);
		    }
		}
	    printf("\n");
	    }
	}
    printf("\n");

    }
printf("</PRE>\n");
}


void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
char *userSeq;
char *db, *organism;

boolean clearUserSeq = cgiBoolean("Clear");
allGenomes = cgiVarExists("allGenomes");

cart = theCart;
dnaUtilOpen();

orgChange = sameOk(cgiOptionalString("changeInfo"),"orgChange");
if (orgChange)
    cgiVarSet("db", hDefaultDbForGenome(cgiOptionalString("org"))); 
getDbAndGenome(cart, &db, &organism, oldVars);
chromAliasSetup(db);
char *oldDb = cloneString(db);

// n.b. this changes to default db if db doesn't have BLAT
findClosestServer(&db, &organism);

allResults = cartUsualBoolean(cart, "allResults", allResults);
autoRearr  = cartUsualBoolean(cart, "autoRearr", autoRearr);

/* Get sequence - from userSeq variable, or if 
 * that is empty from a file. */
if (clearUserSeq)
    {
    cartSetString(cart, "userSeq", "");
    cartSetString(cart, "seqFile", "");
    }
userSeq = cartUsualString(cart, "userSeq", "");
if (isEmpty(userSeq))
    {
    userSeq = cartOptionalString(cart, "seqFile");
    }
if (isEmpty(userSeq) || orgChange)
    {
    cartWebStart(theCart, db, "%s BLAT Search", trackHubSkipHubName(organism));
    if (differentString(oldDb, db))
	printf("<HR><P><EM><B>Note:</B> BLAT search is not available for %s %s; "
	       "defaulting to %s %s</EM></P><HR>\n",
	       hGenome(oldDb), hFreezeDate(oldDb), organism, hFreezeDate(db));
    askForSeq(organism,db);
    cartWebEnd();
    }
else 
    {
    if (allGenomes)
	{
	cartWebStart(cart, db, "All Genomes BLAT Results");

	struct dbDb *dbList = hGetBlatIndexedDatabases();

	struct dbDb *this = NULL;
	char *saveDb = db;
	char *saveOrg = organism;
	struct sqlConnection *conn = hConnectCentral();
	int dbCount = 0;
	nonHubDynamicBlatServerCount = 0;
	hubDynamicBlatServerCount   = 0;

	for(this = dbList; this; this = this->next)
	    {
	    db = this->name;
	    organism = hGenome(db);

	    if (!trackHubDatabase(db))  // if not hub db, make sure it is the default assembly.
		{	    
		char query[256];
		sqlSafef(query, sizeof query, "select name from defaultDb where genome='%s'", organism);
		char *defaultDb = sqlQuickString(conn, query);
		if (!sameOk(defaultDb, db))
		    continue;  // skip non-default dbs
		}

	    blatSeq(skipLeadingSpaces(userSeq), organism, db, dbCount);

	    ++dbCount;
	    }
	dbDbFreeList(&dbList);
	db = saveDb;
	organism = saveOrg;
	hDisconnectCentral(&conn);

	// Loop over each org's default assembly

	/* pre-load remote tracks in parallel */
	int ptMax = atoi(cfgOptionDefault("parallelFetch.threads", "20"));  // default number of threads for parallel fetch.
	int pfdListCount = 0;
	pthread_t *threads = NULL;

	pfdListCount = slCount(pfdList);
        if (pfdListCount > 0)
	    {

	    /* launch parallel threads */
	    ptMax = min(ptMax, pfdListCount);
	    if (ptMax > 0)
		{
		AllocArray(threads, ptMax);
		/* Create threads */
		int pt;
		for (pt = 0; pt < ptMax; ++pt)
		    {
		    int rc = pthread_create(&threads[pt], NULL, remoteParallelLoad, NULL);
		    if (rc)
			{
			errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
			}
		    pthread_detach(threads[pt]);  // this thread will never join back with it's progenitor
			// Canceled threads that might leave locks behind,
			// so the threads are detached and will be neither joined nor canceled.
		    }
		}

	    if (ptMax > 0)
		{
		/* wait for remote parallel load to finish */
		remoteParallelLoadWait(atoi(cfgOptionDefault("parallelFetch.timeout", "90")));  // wait up to default 90 seconds.
		}

	    // Should continue with pfdDone since threads could still be running that might access pdfList ?

	    // Hide weaker of RC'd query pairs, if not debugging.
	    // Combine pairs with a query and its RC.
	    if (!(debuggingGfResults) &&
	       (sameString(pfdDone->xType,"dna") || sameString(pfdDone->xType,"dnax")))
		{
		slSort(&pfdDone, rcPairsCmp);  
		hideWeakerOfQueryRcPairs(pfdDone);
		}

	    // requires db for chromSize, do database after multi-threading done.
	    changeMaxGenePositionToPositiveStrandCoords(pfdDone);

	    // sort by maximum hits
	    slSort(&pfdDone, genomeHitsCmp);

	    // Print instructions
	    printf("The single best alignment found for each assembly is shown below.\n"
		    "The approximate results below are sorted by number of matching 'tiles', "
		    "perfectly matching sub-sequences of length 11 (DNA) "
		    "or 4 (protein). Using only tile hits, this speedy method can not see mismatches.");
	    printf("Click the 'assembly' link to trigger a full BLAT alignment for that genome. \n");
	    printf("The entire alignment, including mismatches and gaps, must score 20 or higher in order to appear in the Blat output.\n");
	    printf("For more details see the <a href='/FAQ/FAQblat.html#blat9'>BLAT FAQ</a>.<br>\n");

	    // Print report  // move to final report at the end of ALL Assemblies
	    int lastSeqNumber = -1;
	    int idCount = 0;
	    char id[256];
	    struct genomeHits *gH = NULL;
	    for (gH = pfdDone; gH; gH = gH->next)
		{
		if (lastSeqNumber != gH->seqNumber)
		    {
		    if (lastSeqNumber != -1) // end previous table
			{
			printf("</TABLE><br><br>\n");
			}
		    lastSeqNumber = gH->seqNumber;
		    // print next sequence table header
		    printf("<TABLE cellspacing='5'>\n");
		    printf("<TR>\n");
		    printf(
			"<th style='text-align:left'>Name</th>"
			"<th style='text-align:left'>Genome</th>"
			"<th style='text-align:left'>Assembly</th>"
			);
		    
		    printf(
			"<th style='text-align:right'>Tiles</th>"
			"<th style='text-align:left'>Chrom</th>"
			    );
		    if (debuggingGfResults)
			{
			printf(
			"<th style='text-align:left'>Pos</th>"
			"<th style='text-align:left'>Strand</th>"
			"<th style='text-align:left'>Exons</th>"
			"<th style='text-align:left'>Query RC'd</th>"
			"<th style='text-align:left'>Type</th>"
			    );
			}
		    printf("\n");
		    printf("</TR>\n");
		    }

		if (gH->hide) // hide weaker of pairs for dna and dnax with reverse-complimented queries.
			continue;
		printf("<TR>\n");
		if (gH->error)
		    {
		    printf("<td>%s</td><td>%s</td><td>%s</td><td></td><td>%s</td><td></td>",
			gH->faName, trackHubSkipHubName(gH->genome), trackHubSkipHubName(gH->db), gH->networkErrMsg); 
		    if (debuggingGfResults)
			printf("<td>%d</td><td>%s</td><td></td>", 
			gH->queryRC, gH->type);
		    printf("\n");
		    }
		else
		    {
		    char pos[256];
		    safef(pos, sizeof pos, "%s:%d-%d", gH->maxGeneChrom, gH->maxGeneTStart+1, gH->maxGeneTEnd); // 1-based closed coord
		    if (!gH->maxGeneChrom) // null
			pos[0] = 0;  // empty string
		    safef(id, sizeof id, "res%d", idCount);
		    printf("<td>%s</td><td>%s</td>"
			"<td><a id=%s href=''>%s</a></td>"
			, gH->faName, trackHubSkipHubName(gH->genome), id, trackHubSkipHubName(gH->db));

		    printf("<td style='text-align:right'>%d</td><td>%s</td>", gH->maxGeneHits, 
			gH->maxGeneChrom ? gH->maxGeneChrom : "");

		    if (debuggingGfResults)
			{
			printf("<td>%s</td><td>%s</td>", pos, gH->maxGeneStrand);
			printf( "<td>%d</td><td>%d</td><td>%s</td>", gH->maxGeneExons, gH->queryRC, gH->xType);
			}

		    printf("\n");
		    jsOnEventByIdF("click", id, 
			"document.mainForm.org.value=\"%s\";"  // some have single-quotes in their value.
			"document.mainForm.db.value='%s';"
			"document.mainForm.submit();"
			"return false;"   // cancel the default link url
			, gH->genome, gH->db
			);
		    idCount++;
		    }
	    
		printf("</TR>\n");
		}
	    printf("</TABLE><br>\n");

	    if (debuggingGfResults)
		printDebugging();

	    if (hubDynamicBlatServerCount > 0 || nonHubDynamicBlatServerCount > 0) 
		{
		printf("The BLAT All Genomes feature does not currently support dynamic BLAT servers. "
		"<b>See our <a href='/FAQ/FAQblat.html#blat9'>BLAT All FAQ</a> for more information.</b><br>\n");
		}
	    printf( "<br>\n");

	    fakeAskForSeqForm(organism, db);
	    }
	else
	    {
	    printf("No input sequences provided.<br><br>\n");
	    }

	cartWebEnd();
	}
    else
	blatSeq(skipLeadingSpaces(userSeq), organism, db, 0);
    }
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "Clear", "Lucky", "type", "userSeq", "seqFile", "showPage", "changeInfo", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cfgInitCgi();

enteredMainTime = clock1000();
/* 0, 0, == use default 10 second for warning, 20 second for immediate exit */
issueBotWarning = earlyBotCheck(enteredMainTime, "hgBlat", delayFraction, 0, 0, "html");
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

autoBigPsl = cfgOptionBooleanDefault("autoBlatBigPsl", autoBigPsl); 

/* org has precedence over db when changeInfo='orgChange' */

char *cookieName = hUserCookie();
cartEmptyShell(doMiddle, cookieName, excludeVars, oldVars);
cgiExitTime("hgBlat", enteredMainTime);
return 0;
}

