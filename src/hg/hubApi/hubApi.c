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
#include "vcf.h"
#include "bedTabix.h"
#include "bamFile.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

/*
+------------------+------------------+------+-----+---------+-------+
| Field            | Type             | Null | Key | Default | Extra |
+------------------+------------------+------+-----+---------+-------+
| hubUrl           | longblob         | NO   | PRI | NULL    |       |
| shortLabel       | varchar(255)     | NO   |     | NULL    |       |
| longLabel        | varchar(255)     | NO   |     | NULL    |       |
| registrationTime | varchar(255)     | NO   |     | NULL    |       |
| dbCount          | int(10) unsigned | NO   |     | NULL    |       |
| dbList           | blob             | YES  |     | NULL    |       |
| descriptionUrl   | longblob         | YES  |     | NULL    |       |
+------------------+------------------+------+-----+---------+-------+
*/

struct hubPublic
/* Table of public track data hub connections. */
    {
    struct hubPublic *next;  /* Next in singly linked list. */
    char *hubUrl;	/* URL to hub.ra file */
    char *shortLabel;	/* Hub short label. */
    char *longLabel;	/* Hub long label. */
    char *registrationTime;	/* Time first registered */
    unsigned dbCount;	/* Number of databases hub has data for. */
    char *dbList;	/* Comma separated list of databases. */
    char *descriptionUrl;	/* URL to description HTML */
    };

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
static struct hash *trackCounter = NULL;
static long totalTracks = 0;
static boolean measureTiming = FALSE;	/* set by CGI parameters */
static boolean allTrackSettings = FALSE;	/* checkbox setting */
static char **shortLabels = NULL;	/* public hub short labels in array */
static int publicHubCount = 0;
static struct hubPublic *publicHubList = NULL;
static char *defaultHub = "Plants";
static long enteredMainTime = 0;	/* will become = clock1000() on entry */
		/* to allow calculation of when to bail out, taking too long */
static long timeOutSeconds = 100;
static boolean timedOut = FALSE;
static boolean jsonOutput = FALSE;	/* turns on when pathInfo present */

/* ######################################################################### */

/* json output needs to encode special characters in strings:
  " - quotation mark
  / - forward slash
  \ - back slash
  \n - new line
  \r - carriage return
  \t - tab
*/

static char* jsonEscape(char *jsonString)
/* escape any of the special characters in the string for json output */
{
if (NULL == jsonString)
    return NULL;
/* going to alternate the result string between a and b so the returned
 * string from replaceChars() can be freemem'ed
 * returned result from here should also be freemem'ed
 */
static char *a = NULL;
static char *b = NULL;
/* replace back slash first since the other encodings will be adding
 * the back slash
 */
a = replaceChars(jsonString, "\\", "\\\\");	/* \ -> \\ */
b = replaceChars(a, "\"", "\\\"");	/* " -> \" */
freeMem(a);
a = replaceChars(b, "/", "\\/");	/* / -> \/ */
freeMem(b);
b = replaceChars(a, "\n", "\\\n");	/* \n -> \\n */
freeMem(a);
a = replaceChars(b, "\r", "\\\r");	/* \r -> \\r */
freeMem(b);
b = replaceChars(a, "\t", "\\\t");	/* \t -> \\t */
return b;
}

static void hubPublicJsonOutput(struct hubPublic *el, FILE *f) 
/* Print out hubPublic element in JSON format. */
{
fputc('{',f);
fputc('"',f);
fprintf(f,"hubUrl");
fputc('"',f);
fputc(':',f);
fputc('"',f);
char *a = jsonEscape(el->hubUrl);
fprintf(f, "%s", a);
freeMem(a);
fputc('"',f);
fputc(',',f);
fputc('"',f);
fprintf(f,"shortLabel");
fputc('"',f);
fputc(':',f);
fputc('"',f);
a = jsonEscape(el->shortLabel);
fprintf(f, "%s", a);
freeMem(a);
fputc('"',f);
fputc(',',f);
fputc('"',f);
fprintf(f,"longLabel");
fputc('"',f);
fputc(':',f);
fputc('"',f);
a = jsonEscape(el->longLabel);
fprintf(f, "%s", a);
freeMem(a);
fputc('"',f);
fputc(',',f);
fputc('"',f);
fprintf(f,"registrationTime");
fputc('"',f);
fputc(':',f);
fputc('"',f);
a = jsonEscape(el->registrationTime);
fprintf(f, "%s", a);
freeMem(a);
fputc('"',f);
fputc(',',f);
fputc('"',f);
fprintf(f,"dbCount");
fputc('"',f);
fputc(':',f);
fprintf(f, "%u", el->dbCount);
fputc(',',f);
fputc('"',f);
fprintf(f,"dbList");
fputc('"',f);
fputc(':',f);
fputc('"',f);
a = jsonEscape(el->dbList);
fprintf(f, "%s", a);
freeMem(a);
fputc('"',f);
fputc(',',f);
fputc('"',f);
fprintf(f,"descriptionUrl");
fputc('"',f);
fputc(':',f);
a = jsonEscape(el->descriptionUrl);
if (isEmpty(a))
    fprintf(f, "%s", "null");
else
    {
    fputc('"',f);
    fprintf(f, "%s", a);
    fputc('"',f);
}
freeMem(a);
fputc('}',f);
}

static int publicHubCmpCase(const void *va, const void *vb)
/* Compare two slNames, ignore case. */
{
const struct hubPublic *a = *((struct hubPublic **)va);
const struct hubPublic *b = *((struct hubPublic **)vb);
return strcasecmp(a->shortLabel, b->shortLabel);
}

static void publicHubSortCase(struct hubPublic **pList)
/* Sort slName list, ignore case. */
{
slSort(pList, publicHubCmpCase);
}

static struct hubPublic *hubPublicLoad(char **row)
/* Load a hubPublic from row fetched with select * from hubPublic
 * from database.  Dispose of this with hubPublicFree(). */
{
struct hubPublic *ret;

AllocVar(ret);
ret->hubUrl = cloneString(row[0]);
ret->shortLabel = cloneString(row[1]);
ret->longLabel = cloneString(row[2]);
ret->registrationTime = cloneString(row[3]);
ret->dbCount = sqlUnsigned(row[4]);
ret->dbList = cloneString(row[5]);
// if (row[6])
    ret->descriptionUrl = cloneString(row[6]);
// else
//     ret->descriptionUrl = cloneString("");
return ret;
}

static struct hubPublic *hubPublicLoadAll()
{
struct hubPublic *list = NULL;
struct sqlConnection *conn = hConnectCentral();
// Build a query to find all public hub URL's
struct dyString *query = sqlDyStringCreate("select * from %s",
                                           hubPublicTableName());
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hubPublic *el = hubPublicLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
publicHubSortCase(&list);
int listSize = slCount(list);
AllocArray(shortLabels, listSize);
struct hubPublic *el = list;
int i = 0;
for ( ; el != NULL; el = el->next )
    {
    shortLabels[i++] = el->shortLabel;
    ++publicHubCount;
    }
return list;
}

#ifdef NOT
static void startHtml(char *title)
{
printf ("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n<html>\n<head><title>%s</title></head><body>\n", title);
}

static void endHtml()
{
printf ("</body></html>\n");
}
#endif

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

static int bbiBriefMeasure(char *type, char *bigDataUrl, char *bigDataIndex, long *chromCount, long *itemCount, struct dyString *errors)
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
    else if (startsWithWord("vcfTabix", type))
        {
        struct vcfFile *vcf = vcfTabixFileAndIndexMayOpen(bigDataUrl, bigDataIndex, NULL, 0, 0, 1, 1);
        if (vcf == NULL)
	    {
	    dyStringPrintf(errors, "Could not open %s and/or its tabix index (.tbi) file.  See http://genome.ucsc.edu/goldenPath/help/vcf.html", bigDataUrl);
            retVal = 1;
	    }
        else
	    vcfFileFree(&vcf);
        }
    else if (startsWithWord("bam", type))
        {
	bamFileAndIndexMustExist(bigDataUrl, bigDataIndex);
        }
    else if (startsWithWord("longTabix", type))
        {
	struct bedTabixFile *btf = bedTabixFileMayOpen(bigDataUrl, NULL, 0, 0);
	if (btf == NULL)
	    {
	    dyStringPrintf(errors, "Couldn't open %s and/or its tabix index (.tbi) file.", bigDataUrl);
            retVal = 1;
	    }
	else
	    bedTabixFileClose(&btf);
        }
#ifdef USE_HAL
    else if (startsWithWord("halSnake", type))
        {
	char *errString;
	int handle = halOpenLOD(bigDataUrl, &errString);
	if (handle < 0)
	    {
	    dyStringPrintf(errors, "HAL open error: %s", errString);
            retVal = 1;
	    }
	if (halClose(handle, &errString) < 0)
	    {
	    dyStringPrintf(errors, "HAL close error: %s", errString);
	    retVal = 1;
	    }
        }
#endif
    else
        {
	dyStringPrintf(errors, "unrecognized type %s", type);
        retVal = 1;
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

static void trackList(struct trackDb *tdb, struct trackHubGenome *genome)
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
	    char *bigDataIndex = NULL;
	    char *relIdxUrl = trackDbSetting(tdb, "bigDataIndex");
	    if (relIdxUrl != NULL)
		bigDataIndex = trackHubRelativeUrl(genome->trackDbFile, relIdxUrl);

	    long chromCount = 0;
	    long itemCount = 0;
	    struct dyString *errors = newDyString(1024);
	    int retVal = bbiBriefMeasure(track->type, bigDataUrl, bigDataIndex, &chromCount, &itemCount, errors);
            if (retVal)
		{
		    hPrintf("    <li>%s : %s : <font color='red'>ERROR: %s</font></li>\n", track->track, track->type, errors->string);
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

static void assemblySettings(struct trackHubGenome *genome)
/* display all the assembly 'settingsHash' */
{
hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(genome->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	{
        struct trackDb *tdb = trackHubTracksForGenome(genome->trackHub, genome);
	trackList(tdb, genome);
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
long totalAssemblyCount = 0;
struct trackHubGenome *genome = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub</h4>\n");
hPrintf("<ul>\n");
long lastTime = clock1000();
for ( ; genome; genome = genome->next )
    {
    ++totalAssemblyCount;
    if (genome->organism)
	{
	hPrintf("<li>%s - %s - %s</li>\n", genome->organism, genome->name, genome->description);
	}
    else
	{	/* can there be a description when organism is empty ? */
	hPrintf("<li>%s</li>\n", genome->name);
	}
    if (genome->settingsHash)
	assemblySettings(genome);
    if (measureTiming)
	{
	long thisTime = clock1000();
	hPrintf("<em>processing time %s: %ld millis</em><br>\n", genome->name, thisTime - lastTime);
	}
    if (timeOutReached())
	break;
    }
if (trackCounter->elCount)
    {
    hPrintf("    <li>total assembly count: %ld</li>\n", totalAssemblyCount);
    hPrintf("    <li>%ld total tracks counted, %d different track types:</li>\n", totalTracks, trackCounter->elCount);
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

void jsonPublicHubs()
{
struct hubPublic *el = publicHubList;
hPrintf("{\"publicHubs\":[");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonOutput(el, stdout);
    if (el->next)
       hPrintf(",");
    }
hPrintf("]}\n");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
// struct hubPublic *hubList = hubPublicLoadAll();
publicHubList = hubPublicLoadAll();
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

if ((NULL == pathInfo) || strlen(pathInfo) < 1)
    {
    pathInfo = cloneString("noPathInfo");
    }
else
    {
    jsonOutput = TRUE;
    }

if (jsonOutput)
    {
//    startHtml("json output example");
//    hPrintf("<br><a href='http://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi'>return to hubApi</a><br><br>\n");
    jsonPublicHubs();
//    endHtml();
    return;
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

hPrintf("<h2>Example URLs to return json data structures:</h2>\n");
hPrintf("<ul>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/list/publicHubs'>list public hubs</a> <em>/cgi-bin/hubApi/list/publicHubs</em></li>\n");
hPrintf("</ul>\n");
long lastTime = clock1000();
struct trackHub *hub = trackHubOpen(urlInput, "");
if (measureTiming)
    {
    long thisTime = clock1000();
    hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

hPrintf("<h4>cart dump</h4>");
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

if (timedOut)
    hPrintf("<h1>Reached time out %ld seconds</h1>", timeOutSeconds);
if (measureTiming)
    hPrintf("<em>Overall total time: %ld millis</em><br>\n", clock1000() - enteredMainTime);

cartWebEnd();
}	/*	void doMiddle(struct cart *theCart)	*/

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
return 0;
}
