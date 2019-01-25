/* hubApi - access mechanism to hub data resources. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "hPrint.h"
#include "bigWig.h"
#include "hubConnect.h"
#include "obscure.h"
#include "errCatch.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
static struct hash *trackCounter = NULL;
static long totalTracks = 0;
static boolean measureTiming = FALSE;	/* set by CGI parameters */
static boolean allTrackSettings = FALSE;	/* checkbox setting */
static struct slName *publicHubShortLabels = NULL;
static char **shortLabels = NULL;	/* public hub short labels in array */
static int publicHubCount = 0;
static char *defaultHub = "Plants";
static long enteredMainTime = 0;	/* will become = clock1000() on entry */
		/* to allow calculation of when to bail out, taking too long */
static long timeOutSeconds = 100;
static boolean timedOut = FALSE;

static boolean timeOutReached()
{
long nowTime = clock1000();
timedOut = FALSE;
if ((nowTime - enteredMainTime) > (1000 * timeOutSeconds))
    timedOut= TRUE;
return timedOut;
}

static void trackSettings(struct trackDb *tdb)
/* process the settingsHash for a track */
{
hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(tdb->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    if (isEmpty((char *)hel->val))
	hPrintf("    <li>%s : &lt;empty&gt;</li>\n", hel->name);
    else
	hPrintf("    <li>%s : '%s'</li>\n", hel->name, (char *)hel->val);
    }
hPrintf("    </ul>\n");
}

static int bbiBriefMeasure(char *type, char *bigDataUrl, long *chromCount, long *itemCount, struct dyString *errors)
/* check a bigDataUrl to find chrom count and item count */
{
int retVal = 0;
*chromCount = 0;
*itemCount = 0;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (startsWithWord("bigNarrowPeak", type)
            || startsWithWord("bigBed", type)
            || startsWithWord("bigGenePred", type)
            || startsWithWord("bigPsl", type)
            || startsWithWord("bigChain", type)
            || startsWithWord("bigMaf", type)
            || startsWithWord("bigBarChart", type)
            || startsWithWord("bigInteract", type))
        {
        struct bbiFile *bbi = NULL;
        bbi = bigBedFileOpen(bigDataUrl);
        struct bbiChromInfo *chromList = bbiChromList(bbi);
        *chromCount = slCount(chromList);
        *itemCount = bigBedItemCount(bbi);
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("bigWig", type))
        {
        struct bbiFile *bwf = bigWigFileOpen(bigDataUrl);
        struct bbiChromInfo *chromList = bbiChromList(bwf);
        struct bbiSummaryElement sum = bbiTotalSummary(bwf);
        *chromCount = slCount(chromList);
        *itemCount = sum.validCount;
        bbiFileClose(&bwf);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

return retVal;
}	/* static int bbiBriefMeasure() */

static void trackList(struct trackDb *tdb)
/* process the track list to show all tracks */
{
if (tdb)
    {
    struct hash *countTracks = hashNew(0);
    hPrintf("    <ul>\n");
    struct trackDb *track = tdb;
    for ( ; track; track = track->next )
	{
        char *bigDataUrl = hashFindVal(track->settingsHash, "bigDataUrl");
      char *compositeTrack = hashFindVal(track->settingsHash, "compositeTrack");
	char *superTrack = hashFindVal(track->settingsHash, "superTrack");
        boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
        if (compositeTrack)
           hashIncInt(countTracks, "composite container");
        else if (superTrack)
           hashIncInt(countTracks, "superTrack container");
	else if (isEmpty(track->type))
           hashIncInt(countTracks, "no type specified");
	else
           hashIncInt(countTracks, track->type);
        if (depthSearch && bigDataUrl)
	    {
	    long chromCount = 0;
	    long itemCount = 0;
	    struct dyString *errors = newDyString(1024);
	    int retVal = bbiBriefMeasure(track->type, bigDataUrl, &chromCount, &itemCount, errors);
            if (retVal)
		{
		    hPrintf("    <li>%s : %s : ERROR: %s</li>\n", track->track, track->type, errors->string);
		}
	    else
		{
		if (startsWithWord("bigBed", track->type))
		    hPrintf("    <li>%s : %s : %ld chroms : %ld item count</li>\n", track->track, track->type, chromCount, itemCount);
		else if (startsWithWord("bigWig", track->type))
		    hPrintf("    <li>%s : %s : %ld chroms : %ld bases covered</li>\n", track->track, track->type, chromCount, itemCount);
		else
		    hPrintf("    <li>%s : %s : %ld chroms : %ld count</li>\n", track->track, track->type, chromCount, itemCount);
		}
	    }
        else
	    {
	    if (compositeTrack)
		hPrintf("    <li>%s : %s : composite track container</li>\n", track->track, track->type);
	    else if (superTrack)
		hPrintf("    <li>%s : %s : superTrack container</li>\n", track->track, track->type);
	    else if (! depthSearch)
		hPrintf("    <li>%s : %s : %s</li>\n", track->track, track->type, bigDataUrl);
            else
		hPrintf("    <li>%s : %s</li>\n", track->track, track->type);
	    }
        if (allTrackSettings)
            trackSettings(track); /* show all settings */
	if (timeOutReached())
	    break;
	}	/*	for ( ; track; track = track->next )	*/
    hPrintf("    <li>%d different track types</li>\n", countTracks->elCount);
    if (countTracks->elCount)
	{
        hPrintf("        <ul>\n");
        struct hashEl *hel;
        struct hashCookie hc = hashFirst(countTracks);
        while ((hel = hashNext(&hc)) != NULL)
	    {
            int prevCount = ptToInt(hashFindVal(trackCounter, hel->name));
	    totalTracks += ptToInt(hel->val);
	    hashReplace(trackCounter, hel->name, intToPt(prevCount + ptToInt(hel->val)));
	    hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
	    }
        hPrintf("        </ul>\n");
	}
    hPrintf("    </ul>\n");
    }
}	/*	static void trackList(struct trackDb *tdb)	*/

static void assemblySettings(struct trackHubGenome *thg)
/* display all the assembly 'settingsHash' */
{
hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(thg->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	{
        struct trackDb *tdb = trackHubTracksForGenome(thg->trackHub, thg);
	trackList(tdb);
        }
    if (timeOutReached())
	break;
    }
hPrintf("    </ul>\n");
}

static void genomeList (struct trackHub *hubTop)
/* follow the pointers from the trackHub to trackHubGenome and around
 * in a circle from one to the other to find all hub resources
 */
{
struct trackHubGenome *thg = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub</h4>\n");
hPrintf("<ul>\n");
long lastTime = clock1000();
for ( ; thg; thg = thg->next )
    {
    if (thg->organism)
	{
	hPrintf("<li>%s - %s - %s</li>\n", thg->organism, thg->name, thg->description);
	}
    else
	{	/* can there be a description when organism is empty ? */
	hPrintf("<li>%s</li>\n", thg->name);
	}
    if (thg->settingsHash)
	assemblySettings(thg);
    if (measureTiming)
	{
	long thisTime = clock1000();
	hPrintf("<em>processing time %s: %ld millis</em><br>\n", thg->name, thisTime - lastTime);
	}
    if (timeOutReached())
	break;
    }
if (trackCounter->elCount)
    {
    hPrintf("    <li>%ld total tracks counted, %d different types:</li>\n", totalTracks, trackCounter->elCount);
    hPrintf("    <ul>\n");
    struct hashEl *hel;
    struct hashCookie hc = hashFirst(trackCounter);
    while ((hel = hashNext(&hc)) != NULL)
	{
	hPrintf("    <li>%d - %s - total</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("    </ul>\n");
    }
hPrintf("</ul>\n");
}

static void getPublicHubList()
/* obtain shortLabel from all public hubs */
{
struct sqlConnection *conn = hConnectCentral();
// Build a query to find all public hub URL's
struct dyString *query = sqlDyStringCreate("select shortLabel from %s",
                                           hubPublicTableName());
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameStore(&publicHubShortLabels, row[0]);
    }
sqlFreeResult(&sr);
slNameSort(&publicHubShortLabels);
int listSize = slCount(publicHubShortLabels);
AllocArray(shortLabels, listSize);
struct slName *el = publicHubShortLabels;
int i = 0;
for ( ; el != NULL; el = el->next )
    {
    shortLabels[i++] = el->name;
    ++publicHubCount;
    }
}	/*	static void getPublicHubList()	*/

static void displayPublicHubs()
/* show URLs to all public hubs */
{
return;
hPrintf("<h3>public hub URLs from hgwdev-gi</h3>\n");
hPrintf("<ul>\n");
struct sqlConnection *conn = hConnectCentral();
// Build a query to find all public hub URL's
struct dyString *query = sqlDyStringCreate("select hubUrl,shortLabel from %s",
                                           hubPublicTableName());
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<li>%s - %s</li>\n", row[1], row[0]);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);

hPrintf("</ul>\n");
}

static char *urlFromShortLabel(char *shortLabel)
{
char hubUrl[1024];
char query[1024];
struct sqlConnection *conn = hConnectCentral();
// Build a query to select the hubUrl for the given shortLabel
sqlSafef(query, sizeof(query), "select hubUrl from %s where shortLabel='%s'",
      hubPublicTableName(), shortLabel);
if (! sqlQuickQuery(conn, query, hubUrl, sizeof(hubUrl)))
    hubUrl[0] = 0;
hDisconnectCentral(&conn);
return cloneString(hubUrl);
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
getPublicHubList();	/* populates slName publicHubShortLabels	*/
cart = theCart;
measureTiming = hPrintStatus() && isNotEmpty(cartOptionalString(cart, "measureTiming"));
measureTiming = TRUE;
char *database = NULL;
char *genome = NULL;


getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

char *docRoot = cfgOptionDefault("browser.documentRoot", DOCUMENT_ROOT);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *pathInfo = getenv("PATH_INFO");

if ((NULL == pathInfo) || strlen(pathInfo) < 1) {
   pathInfo = cloneString("noPathInfo");
}

cartWebStart(cart, database, "access mechanism to hub data resources");

char *goOtherHub = cartUsualString(cart, "goOtherHub", defaultHub);
char *otherHubUrl = cartUsualString(cart, "urlHub", defaultHub);
char *goPublicHub = cartUsualString(cart, "goPublicHub", defaultHub);
char *hubDropDown = cartUsualString(cart, "publicHubs", defaultHub);
char *urlDropDown = urlFromShortLabel(hubDropDown);
char *urlInput = urlDropDown;	/* assume public hub */
if (sameWord("go", goOtherHub))	/* requested other hub URL */
    urlInput = otherHubUrl;

long lastTime = clock1000();
struct trackHub *hub = trackHubOpen(urlInput, "");
if (measureTiming)
    {
    long thisTime = clock1000();
    hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

hPrintf("<h2>cart dump</h2>");
hPrintf("<pre>\n");
cartDump(cart);
hPrintf("</pre>\n");

hPrintf("<form action='%s' name='hubApiUrl' id='hubApiUrl' method='GET'>\n\n", "../cgi-bin/hubApi");

hPrintf("<b>Select public hub:&nbsp;</b>");
#define JBUFSIZE 2048
char javascript[JBUFSIZE];
struct slPair *events = NULL;
safef(javascript, sizeof(javascript), "this.lastIndex=this.selectedIndex;");
slPairAdd(&events, "focus", cloneString(javascript));
#define SMALLBUF 256
// char class[SMALLBUF];
// safef(class, sizeof(class), "viewDD normalText %s", "class");

cgiMakeDropListClassWithIdStyleAndJavascript("publicHubs", "publicHubs",
    shortLabels, publicHubCount, hubDropDown, NULL, "width: 400px", events);

hWrites("&nbsp;");
hButton("goPublicHub", "go");

hPrintf("<br>Or, enter a hub URL:&nbsp;");
hPrintf("<input type='text' name='urlHub' id='urlHub' size='60' value='%s'>\n", urlInput);
hWrites("&nbsp;");
hButton("goOtherHub", "go");

boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hPrintf("<br>\n&nbsp;&nbsp;");
hCheckBox("depthSearch", cartUsualBoolean(cart, "depthSearch", FALSE));
hPrintf("&nbsp;perform full bbi file measurement : %s (will time out if taking longer than %ld seconds)<br>\n", depthSearch ? "TRUE" : "FALSE", timeOutSeconds);
hPrintf("\n&nbsp;&nbsp;");
allTrackSettings = cartUsualBoolean(cart, "allTrackSettings", FALSE);
hCheckBox("allTrackSettings", allTrackSettings);
hPrintf("&nbsp;display all track settings for each track : %s<br>\n", allTrackSettings ? "TRUE" : "FALSE");


hPrintf("<br>\n</form>\n");

hPrintf("<p>URL: %s - %s<br>\n", urlInput, sameWord("go",goPublicHub) ? "public hub" : "other hub");
hPrintf("name: %s<br>\n", hub->shortLabel);
hPrintf("description: %s<br>\n", hub->longLabel);
hPrintf("default db: '%s'<br>\n", isEmpty(hub->defaultDb) ? "(none available)" : hub->defaultDb);
printf("pathInfo:'%s'<br>\ndocRoot:'%s'<br>\n", pathInfo, docRoot);

if (hub->genomeList)
    genomeList(hub);

hPrintf("</p>\n");

displayPublicHubs();

cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
measureTiming = TRUE;
verboseTimeInit();
trackCounter = hashNew(0);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
if (timedOut)
    hPrintf("<h1>Reached time out %ld seconds</h1>", timeOutSeconds);
if (measureTiming)
    hPrintf("<em>Overall total time: %ld millis</em><br>\n", clock1000() - enteredMainTime);
return 0;
}
