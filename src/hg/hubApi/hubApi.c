/* hubApi - access mechanism to hub data resources. */
#include "dataApi.h"
#include "botDelay.h"

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

/* Global Variables for all modules */

int maxItemsOutput = 1000;	/* can be set in URL maxItemsOutput=N */
static int maxItemLimit = 1000000;   /* maximum of 1,000,000 items returned */
/* for debugging purpose, current bot delay value */
int botDelay = 0;
boolean debug = FALSE;	/* can be set in URL debug=1, to turn off: debug=0 */
#define delayFraction	0.03

/* Global only to this one source file */
static struct cart *cart;             /* CGI and other variables */
static struct hash *oldVars = NULL;
static struct hash *trackCounter = NULL;
static long totalTracks = 0;
static boolean measureTiming = FALSE;	/* set by CGI parameters */
static boolean allTrackSettings = FALSE;	/* checkbox setting */
static char **shortLabels = NULL;	/* public hub short labels in array */
// struct hubPublic *publicHubList = NULL;
static int publicHubCount = 0;
static char *defaultHub = "Plants";
static char *defaultDb = "ce11";
static long enteredMainTime = 0;	/* will become = clock1000() on entry */
		/* to allow calculation of when to bail out, taking too long */
static long timeOutSeconds = 100;
static boolean timedOut = FALSE;
static char *urlPrefix = "";	/* initalized to support self references */
static struct slName *supportedTypes = NULL;
	/* will be initialized to a known supported set */

static void initSupportedTypes()
/* initalize the list of supported track types */
{
struct slName *el = newSlName("bed");
slAddHead(&supportedTypes, el);
el = newSlName("wig");
slAddHead(&supportedTypes, el);
el = newSlName("broadPeak");
slAddHead(&supportedTypes, el);
el = newSlName("narrowPeak");
slAddHead(&supportedTypes, el);
el = newSlName("bigBed");
slAddHead(&supportedTypes, el);
el = newSlName("bigWig");
slAddHead(&supportedTypes, el);
el = newSlName("bigNarrowPeak");
slAddHead(&supportedTypes, el);
el = newSlName("bigGenePred");
slAddHead(&supportedTypes, el);
// el = newSlName("bigPsl");
// slAddHead(&supportedTypes, el);
// el = newSlName("bigBarChart");
// slAddHead(&supportedTypes, el);
// el = newSlName("bigInteract");
// slAddHead(&supportedTypes, el);
// el = newSlName("bigMaf");
// slAddHead(&supportedTypes, el);
// el = newSlName("bigChain");
// slAddHead(&supportedTypes, el);
slNameSort(&supportedTypes);
}

static boolean isSupportedType(char *type)
/* is given type in the supportedTypes list ? */
{
boolean ret = FALSE;
if (startsWith("wigMaf", type))	/* not wigMaf at this time */
    return ret;
struct slName *el;
for (el = supportedTypes; el; el = el->next)
    {
    if (startsWith(el->name, type))
	{
	ret = TRUE;
	break;
	}
    }
return ret;
}

static int publicHubCmpCase(const void *va, const void *vb)
/* Compare two shortLabels, ignore case. */
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

struct hubPublic *hubPublicDbLoadAll()
/* read entire hubPublic table in hgcentral and return resulting list */
{
char query[1024];
struct hubPublic *list = NULL;
struct sqlConnection *conn = hConnectCentral();
sqlSafef(query, sizeof(query), "select * from %s", hubPublicTableName());
struct sqlResult *sr = sqlGetResult(conn, query);
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

static boolean timeOutReached()
/* see if the timeout has been reached to determine if an exit
 *   is appropriate at this time
 */
{
long nowTime = clock1000();
timedOut = FALSE;
if ((nowTime - enteredMainTime) > (1000 * timeOutSeconds))
    timedOut= TRUE;
return timedOut;
}

#ifdef NOT
static void showCounts(struct hash *countTracks)
{
if (countTracks->elCount)
    {
    hPrintf("        <li><ul>\n");
    struct hashEl *hel;
    struct hashCookie hc = hashFirst(countTracks);
    while ((hel = hashNext(&hc)) != NULL)
        hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
    hPrintf("        </ul></li>\n");
    }
}
#endif

static void hashCountTrack(struct trackDb *tdb, struct hash *countTracks)
/* this is counting up track types into the hash countTracks */
{
char *stripType = cloneString(tdb->type);
if (startsWith("chain ", tdb->type))
    stripType = cloneString("chain");
else if (startsWith("netAlign ", tdb->type))
    stripType = cloneString("netAlign");
else if (startsWith("genePred ", tdb->type))
    stripType = cloneString("genePred");
else if (startsWith("bigWig ", tdb->type))
    stripType = cloneString("bigWig");
else if (startsWith("wigMaf ", tdb->type))
    stripType = cloneString("wigMaf");
else if (startsWith("wig ", tdb->type))
    stripType = cloneString("wig");
else
    stripType = cloneString(tdb->type);
// char *compositeTrack = trackDbLocalSetting(tdb, "compositeTrack");
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
// char *superTrack = trackDbLocalSetting(tdb, "superTrack");
boolean superChild = tdbIsSuperTrackChild(tdb);
if (compositeContainer)
    hashIncInt(countTracks, "composite container");
else if (compositeView)
    hashIncInt(countTracks, "composite view");
else if (superChild)
    {
    hashIncInt(countTracks, "superTrack child");
    hashIncInt(countTracks, stripType);
    hashIncInt(countTracks, "track count");
    }
else if (isEmpty(tdb->type))
    hashIncInt(countTracks, "no type specified");
else
    {
    hashIncInt(countTracks, stripType);
    hashIncInt(countTracks, "track count");
    }
freeMem(stripType);
// showCounts(countTracks);
}

static void sampleUrl(struct trackHub *hub, char *db, struct trackDb *tdb, char *chrom, unsigned chromSize, char *errorString)
/* print out a sample getData URL */
{
char errorPrint[2048];
errorPrint[0] = 0;

if (isNotEmpty(errorString))
    {
    safef(errorPrint, sizeof(errorPrint), " <font color='red'>ERROR: %s</font>", errorString);
    }

unsigned start = chromSize / 4;
unsigned end = start + 10000;
if (end > chromSize)
    end = chromSize;
char *genome = NULL;
if (hub)
    genome = hub->genomeList->name;

if (db)
    {
    if (hub)
	{
	char urlReference[2048];
	safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?hubUrl=%s&amp;genome=%s&amp;track=%s&amp;chrom=%s&amp;start=%u&amp;end=%u' target=_blank>(sample getData)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, chrom, start, end, errorPrint);

	if (tdb->parent)
	    hPrintf("<li>%s : %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
	else
	    hPrintf("<li>%s : %s%s</li>\n", tdb->track, tdb->type, urlReference);
	}
    else
	{
	char urlReference[2048];
	safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?db=%s&amp;chrom=%s&amp;track=%s&amp;start=%u&amp;end=%u' target=_blank>(sample getData)%s</a>\n", urlPrefix, db, chrom, tdb->track, start, end, errorPrint);

	if (tdb->parent)
	    hPrintf("<li>%s : %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
	else
	    hPrintf("<li>%s : %s%s</li>\n", tdb->track, tdb->type, urlReference );
	}
    }
else if (hub)
    {
    char urlReference[2048];
    safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?hubUrl=%s&amp;genome=%s&amp;track=%s&amp;chrom=%s&amp;start=%u&amp;end=%u' target=_blank>(sample getData)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, chrom, start, end, errorPrint);

    if (tdb->parent)
	hPrintf("<li>%s : %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
    else
	hPrintf("<li>%s : %s%s</li>\n", tdb->track, tdb->type, urlReference);
    }
else
    hPrintf("<li>%s : %s not db hub track ?</li>\n", tdb->track, tdb->type);
}

static void hubSampleUrl(struct trackHub *hub, struct trackDb *tdb,
    long chromCount, long itemCount, char *chromName, unsigned chromSize,
      char *genome, char *errorString)
{
unsigned start = chromSize / 4;
unsigned end = start + 10000;
if (end > chromSize)
    end = chromSize;

char errorPrint[2048];
errorPrint[0] = 0;

if (isNotEmpty(errorString))
    {
    safef(errorPrint, sizeof(errorPrint), " : <font color='red'>ERROR: %s</font>", errorString);
    }

char countsMessage[512];
countsMessage[0] = 0;
if (chromCount > 0 || itemCount > 0)
    {
    if (startsWithWord("bigBed", tdb->type))
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld item count ", chromCount, itemCount);
    else if (startsWithWord("bigWig", tdb->type))
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld bases covered ", chromCount, itemCount);
    else
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld count ", chromCount, itemCount);
    }

if (isSupportedType(tdb->type))
    {
	char urlReference[2048];
	safef(urlReference, sizeof(urlReference), "<a href='%s/getData/track?hubUrl=%s&amp;genome=%s&amp;track=%s&amp;chrom=%s&amp;start=%u&amp;end=%u' target=_blank>(sample getData)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, chromName, start, end, errorPrint);

        if (startsWithWord("bigBed", tdb->type))
            hPrintf("    <li>%s : %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
        else if (startsWithWord("bigWig", tdb->type))
            hPrintf("    <li>%s : %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
        else
            hPrintf("    <li>%s : %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
    }
else
    {
        if (startsWithWord("bigBed", tdb->type))
            hPrintf("    <li>%s : %s%s</li>\n", tdb->track, tdb->type, countsMessage);
        else if (startsWithWord("bigWig", tdb->type))
            hPrintf("    <li>%s : %s%s</li>\n", tdb->track, tdb->type, countsMessage);
        else
            hPrintf("    <li>%s : %s%s</li>\n", tdb->track, tdb->type, countsMessage);
    }
}	/* static void hubSampleUrl(struct trackHub *hub, struct trackDb *tdb,
	 * long chromCount, long itemCount, char *chromName, unsigned chromSize,
	 *   char *genome)
	 */

static void bbiLargestChrom(struct bbiChromInfo *chromList, char **chromName,
    unsigned *chromSize)
/* find largest chromosome name and size in the chromList */
{
if (chromName && chromSize)
    {
    *chromSize = 0;
    char *returnName = NULL;
    struct bbiChromInfo *el;
    for (el = chromList; el; el = el->next)
	{
	if (el->size > *chromSize)
	    { 
	    *chromSize = el->size;
	    returnName = el->name;
	    } 
	}
    if (chromSize > 0)
	*chromName = cloneString(returnName);
    }
}

static int bbiBriefMeasure(char *type, char *bigDataUrl, char *bigDataIndex, long *chromCount, long *itemCount, struct dyString *errors, char **chromName, unsigned *chromSize)
/* check a bigDataUrl to find chrom count and item count, return
 *   name of largest chrom and its size
 */
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
        bbiLargestChrom(chromList, chromName, chromSize);
        bbiChromInfoFreeList(&chromList);
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("bigWig", type))
        {
        struct bbiFile *bwf = bigWigFileOpen(bigDataUrl);
        struct bbiChromInfo *chromList = bbiChromList(bwf);
        struct bbiSummaryElement sum = bbiTotalSummary(bwf);
        *chromCount = slCount(chromList);
        *itemCount = sum.validCount;
        bbiLargestChrom(chromList, chromName, chromSize);
        bbiChromInfoFreeList(&chromList);
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

static void hubSubTracks(struct trackHub *hub, char *db, struct trackDb *tdb,
    struct hash *countTracks,  long chromCount, long itemCount,
    char *chromName, unsigned chromSize, char *genome, char *errorString)
/* tdb has subtracks, show only subTracks, no details */
{
hPrintf("    <li><ul>\n");
if (debug)
    {
    hPrintf("    <li>subtracks for '%s' db: '%s'</li>\n", tdb->track, db);
    hPrintf("    <li>chrom: '%s' size: %u</li>\n", chromName, chromSize);
    }
if (tdb->subtracks)
    {
    struct trackDb *tdbEl = NULL;
    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
	boolean compositeContainer = tdbIsComposite(tdbEl);
	boolean compositeView = tdbIsCompositeView(tdbEl);
	if (! (compositeContainer || compositeView) )
	    {
	    if (chromSize < 1)
		{
		char *bigDataIndex = NULL;
		char *relIdxUrl = trackDbSetting(tdbEl, "bigDataIndex");
		if (relIdxUrl != NULL)
		    bigDataIndex = trackHubRelativeUrl(hub->genomeList->trackDbFile, relIdxUrl);
		char *bigDataUrl = trackDbSetting(tdbEl, "bigDataUrl");
		char *longName = NULL;
		unsigned longSize = 0;
		struct dyString *errors = newDyString(1024);
		(void) bbiBriefMeasure(tdbEl->type, bigDataUrl, bigDataIndex, &chromCount, &itemCount, errors, &longName, &longSize);
		chromSize = longSize;
		chromName = longName;
		}
	    }
        if (tdbIsCompositeView(tdbEl))
	    hPrintf("<li>%s : %s : composite view of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	else
	    {
	    if (isSupportedType(tdbEl->type))
		hubSampleUrl(hub, tdbEl, chromCount, itemCount, chromName, chromSize, genome, errorString);
	    else
		hPrintf("<li>%s : %s : subtrack of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	    }
	hashCountTrack(tdbEl, countTracks);
        if (tdbEl->subtracks)
	    hubSubTracks(hub, db, tdbEl, countTracks, chromCount, itemCount, chromName, chromSize, genome, errorString);
	}
    }
hPrintf("    </ul></li>\n");
}	/* hubSubTracks() */

static void showSubTracks(struct trackHub *hub, char *db, struct trackDb *tdb, struct hash *countTracks,
    char *chromName, unsigned chromSize, char *errorString)
/* tdb has subtracks, show only subTracks, no details */
{
hPrintf("    <li><ul>\n");
if (debug)
    hPrintf("    <li>subtracks for '%s' db: '%s'</li>\n", tdb->track, db);
if (tdb->subtracks)
    {
    struct trackDb *tdbEl = NULL;
    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
        if (tdbIsCompositeView(tdbEl))
	    hPrintf("<li>%s : %s : composite view of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	else
	    {
	    if (isSupportedType(tdbEl->type))
		sampleUrl(hub, db, tdbEl, chromName, chromSize, errorString);
	    else
		hPrintf("<li>%s : %s : subtrack of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	    }
	hashCountTrack(tdbEl, countTracks);
        if (tdbEl->subtracks)
	    showSubTracks(hub, db, tdbEl, countTracks, chromName, chromSize, errorString);
	}
    }
hPrintf("    </ul></li>\n");
}

static void trackSettings(struct trackDb *tdb, struct hash *countTracks)
/* process the settingsHash for a trackDb, recursive when subtracks */
{
hPrintf("    <li><ul>\n");
// if (tdb->children)  haven't yet seen a track with children ?
//   hPrintf("    <li>%s: has children</li>\n", tdb->track);
// else
//   hPrintf("    <li>%s: NO children</li>\n", tdb->track);
struct hashEl *hel;
struct hashCookie hc = hashFirst(tdb->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    if (sameWord("track", hel->name))
	continue;	// already output in header
    if (isEmpty((char *)hel->val))
	hPrintf("    <li>%s : &lt;empty&gt;</li>\n", hel->name);
    else
	hPrintf("    <li>%s : '%s'</li>\n", hel->name, (char *)hel->val);
    }
if (tdb->subtracks)
    {
    struct trackDb *tdbEl = NULL;
    if (debug)
	hPrintf("   <li>has %d subtrack(s)</li>\n", slCount(tdb->subtracks));
    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
        hPrintf("<li>subtrack: %s of parent: %s : type: '%s'</li>\n", tdbEl->track, tdbEl->parent->track, tdbEl->type);
	hashCountTrack(tdbEl, countTracks);
	trackSettings(tdbEl, countTracks);
	}
    }
hPrintf("    </ul></li>\n");
}

static void hubCountOneTdb(struct trackHub *hub, char *db, struct trackDb *tdb,
    char *bigDataIndex, struct hash *countTracks, char *chromName,
    unsigned chromSize, char *genome)
{
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
// char *compositeTrack = trackDbSetting(tdb, "compositeTrack");
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
// char *superTrack = trackDbSetting(tdb, "superTrack");
boolean superChild = tdbIsSuperTrackChild(tdb);
boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hashCountTrack(tdb, countTracks);

long chromCount = 0;
long itemCount = 0;

struct dyString *errors = newDyString(1024);

/* if given a chromSize, it belongs to a UCSC db and this is *not* an
 *   assembly hub, otherwise, look up a chrom and size in the bbi file
 */
if (! (compositeContainer || compositeView) )
    {
    if (chromSize < 1 || depthSearch)
	{
	char *longName = NULL;
	unsigned longSize = 0;
        (void) bbiBriefMeasure(tdb->type, bigDataUrl, bigDataIndex, &chromCount, &itemCount, errors, &longName, &longSize);
	chromSize = longSize;
	chromName = longName;
	}
    }

if (depthSearch && bigDataUrl)
    {
    if (isSupportedType(tdb->type))
	    hubSampleUrl(hub, tdb, chromCount, itemCount, chromName, chromSize, genome, errors->string);
    }
else
    {
    if (compositeContainer)
        hPrintf("    <li>%s : %s : composite track container has %d subtracks</li>\n", tdb->track, tdb->type, slCount(tdb->subtracks));
    else if (compositeView)
        hPrintf("    <li>%s : %s : composite view of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
    else if (superChild)
        hPrintf("    <li>%s : %s : superTrack child of parent: %s (sample getData)</li>\n", tdb->track, tdb->type, tdb->parent->track);
    else if (! depthSearch && bigDataUrl)
	{
        if (isSupportedType(tdb->type))
	    {
	    hubSampleUrl(hub, tdb, chromCount, itemCount, chromName, chromSize, genome, errors->string);
	    }
	}
    else
	{
        if (isSupportedType(tdb->type))
	    {
	    hubSampleUrl(hub, tdb, chromCount, itemCount, chromName, chromSize, genome, errors->string);
	    }
	else
	    hPrintf("    <li>%s : %s (what is this)</li>\n", tdb->track, tdb->type);
        }
    }
if (allTrackSettings)
    {
    hPrintf("    <li><ul>\n");
    trackSettings(tdb, countTracks); /* show all settings */
    hPrintf("    </ul></li>\n");
    }
else if (tdb->subtracks)
    {
    hubSubTracks(hub, db, tdb, countTracks, chromCount, itemCount, chromName, chromSize, genome, errors->string);
    }
return;
}	/*	static void hubCountOneTdb(char *db, struct trackDb *tdb,
	 *	char *bigDataIndex, struct hash *countTracks,
	 *	char *chromName, unsigned chromSize)
	 */


static void countOneTdb(char *db, struct trackDb *tdb,
    struct hash *countTracks, char *chromName, unsigned chromSize,
      char *errorString)
/* for this tdb in this db, count it up and provide a sample */
{
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
// char *compositeTrack = trackDbSetting(tdb, "compositeTrack");
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
// char *superTrack = trackDbSetting(tdb, "superTrack");
boolean superChild = tdbIsSuperTrackChild(tdb);
boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hashCountTrack(tdb, countTracks);

if (compositeContainer)
    hPrintf("    <li>%s : %s : composite track container has %d subtracks</li>\n", tdb->track, tdb->type, slCount(tdb->subtracks));
else if (compositeView)
    hPrintf("    <li>%s : %s : composite view of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
else if (superChild)
    hPrintf("    <li>%s : %s : superTrack child of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
else if (! depthSearch && bigDataUrl)
    hPrintf("    <li>%s : %s : %s</li>\n", tdb->track, tdb->type, bigDataUrl);
else
    {
    if (isSupportedType(tdb->type))
        sampleUrl(NULL, db, tdb, chromName, chromSize, errorString);
    else
        hPrintf("    <li>%s : %s</li>\n", tdb->track, tdb->type);
    }

if (allTrackSettings)
    {
    hPrintf("    <li><ul>\n");
    trackSettings(tdb, countTracks); /* show all settings */
    hPrintf("    </ul></li>\n");
    }
else if (tdb->subtracks)
    {
    showSubTracks(NULL, db, tdb, countTracks, chromName, chromSize, NULL);
    }
return;
}	/*	static void countOneTdb(char *db, struct trackDb *tdb,
	 *	struct hash *countTracks, char *chromName,
	 *	unsigned chromSize)
	 */

static unsigned largestChrom(char *db, char **nameReturn)
/* return the length and get the chrom name for the largest chrom
 * from chromInfo table.  For use is sample getData URLs
 */
{
char query[1024];
struct sqlConnection *conn = hAllocConn(db);
sqlSafef(query, sizeof(query), "select chrom,size from chromInfo order by size desc limit 1");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
unsigned length = 0;
if (row)
   {
   *nameReturn = cloneString(row[0]);
   length = sqlLongLong(row[1]);
   }
sqlFreeResult(&sr);
hFreeConn(&conn);
return length;
}

static void hubTrackList(struct trackHub *hub, struct trackDb *topTrackDb, struct trackHubGenome *genome)
/* process the track list in a hub to show all tracks */
{
hPrintf("    <li><ul>\n");
if (topTrackDb)
    {
    struct hash *countTracks = hashNew(0);
    struct trackDb *tdb = NULL;
    for ( tdb = topTrackDb; tdb; tdb = tdb->next )
	{
	char *bigDataIndex = NULL;
	char *relIdxUrl = trackDbSetting(topTrackDb, "bigDataIndex");
	if (relIdxUrl != NULL)
	    bigDataIndex = trackHubRelativeUrl(genome->trackDbFile, relIdxUrl);
        char *defaultGenome = NULL;
        if (isNotEmpty(genome->name))
	    defaultGenome = genome->name;
        char *chromName = NULL;
        unsigned chromSize = 0;
        if (isEmpty(genome->twoBitPath))
            chromSize = largestChrom(defaultGenome, &chromName);
	else
	    hPrintf("    <li>twoBitPath %s genome %s</li>\n", genome->twoBitPath, defaultGenome);
	hubCountOneTdb(hub, defaultGenome, tdb, bigDataIndex, countTracks, chromName, chromSize, defaultGenome);
	if (timeOutReached())
	    break;
	}	/*	for ( tdb = topTrackDb; tdb; tdb = tdb->next )	*/
    hPrintf("    <li>%d different track types</li>\n",countTracks->elCount - 1);
    /* add this single genome count to the overall multi-genome counts */
    if (countTracks->elCount)
	{
        hPrintf("        <li><ol>\n");
	struct hashEl *hel, *helList = hashElListHash(countTracks);
	slSort(&helList, hashElCmpIntValDesc);
	for (hel = helList; hel; hel = hel->next)
	    {
	    if (sameOk("track count", hel->name))
		continue;
            int prevCount = ptToInt(hashFindVal(trackCounter, hel->name));
	    if (differentStringNullOk("track count", hel->name))
		totalTracks += ptToInt(hel->val);
	    hashReplace(trackCounter, hel->name, intToPt(prevCount + ptToInt(hel->val)));
	    if (isSupportedType(hel->name))
		hPrintf("        <li>%d - %s - supported</li>\n", ptToInt(hel->val), hel->name);
	    else
		hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
	    }
        hPrintf("        </ol></li>\n");
	}
    }
else
    hPrintf("    <li>no trackTopDb ?</li>\n");

hPrintf("    </ul><li>\n");
}	/*	static struct trackDb *hubTrackList()	*/

static void hubAssemblySettings(struct trackHub *hub, struct trackHubGenome *genome)
/* display all the assembly 'settingsHash' */
{
struct trackDb *tdb = obtainTdb(genome, NULL);

hPrintf("    <li><ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(genome->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	{
	hubTrackList(hub, tdb, genome);
        }
    if (timeOutReached())
	break;
    }
hPrintf("    </ul></li>\n");
}

static unsigned largestChromInfo(struct chromInfo *ci, char **chromName)
/* find largest chrom in this chromInfo, return name and size */
{
unsigned size = 0;
char *name = NULL;
struct chromInfo *el;
for (el = ci; el; el = el->next)
    {
    if (el->size > size)
	{
	size = el->size;
	name = el->chrom;
	}
    }
if (chromName)
    *chromName = name;
return size;
}

static void genomeList(struct trackHub *hubTop)
/* follow the pointers from the trackHub to trackHubGenome and around
 * in a circle from one to the other to find all hub resources
 */
{
long totalAssemblyCount = 0;
struct trackHubGenome *genome = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub</h4>\n");

if (NULL == genome)
    {
    hPrintf("<h4>odd error, can not find a gnomeList ? at url: '%s'</h4>\n", hubTop->url);
    return;
    }

hPrintf("<ul>\n");
long lastTime = clock1000();
for ( ; genome; genome = genome->next )
    {
    ++totalAssemblyCount;
    if (isNotEmpty(genome->twoBitPath))
	{
	hPrintf("<li>assembly hub %s twoBitPath: %s</li>\n", genome->name, genome->twoBitPath);
	char *chromName = NULL;
	struct chromInfo *ci = trackHubAllChromInfo(genome->name);
        unsigned chromSize = largestChromInfo(ci, &chromName);
	char sizeString[64];
	sprintLongWithCommas(sizeString, chromSize);
	hPrintf("<li>%d chromosomes, largest %s at %s bases</li>\n", slCount(ci), chromName, sizeString);
	}
    if (genome->organism)
	{
	hPrintf("<li>%s - %s - %s</li>\n", genome->organism, genome->name, genome->description);
	}
    else
	{	/* can there be a description when organism is empty ? */
	hPrintf("<li>name: %s</li>\n", genome->name);
	}
    hubAssemblySettings(hubTop, genome);
    if (measureTiming || debug)
	{
	long thisTime = clock1000();
	hPrintf("<li><em>processing time %s: %ld millis</em></li>\n", genome->name, thisTime - lastTime);
	}
    if (timeOutReached())
	break;
    }
if (trackCounter->elCount)
    {
    hPrintf("    <li>total genome assembly count: %ld</li>\n", totalAssemblyCount);
    hPrintf("    <li>%ld total tracks counted, %d different track types:</li>\n", totalTracks, trackCounter->elCount);
    hPrintf("    <li><ol>\n");
    struct hashEl *hel, *helList = hashElListHash(trackCounter);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	hPrintf("    <li>%d - %s - total</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("    </ol></li>\n");
    }
hPrintf("</ul>\n");
}	/*	static void genomeList (hubTop)	*/

static char *urlFromShortLabel(char *shortLabel)
/* this is not a fair way to get the URL since shortLabel's are not
 * necessarily unique.  This is temporary.  TBD: need to always use URL
 * and then get the shortLabel
 */
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

static int dbDbCmpName(const void *va, const void *vb)
/* Compare two dbDb elements: name, ignore case. */
{
const struct dbDb *a = *((struct dbDb **)va);
const struct dbDb *b = *((struct dbDb **)vb);
return strcasecmp(a->name, b->name);
}

struct dbDb *ucscDbDb()
/* return the dbDb table as an slList */
{
char query[1024];
struct sqlConnection *conn = hConnectCentral();
sqlSafef(query, sizeof(query), "select * from dbDb");
struct dbDb *dbList = NULL, *el = NULL;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = dbDbLoad(row);
    slAddHead(&dbList, el);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slSort(&dbList, dbDbCmpName);
return dbList;
}

static struct hash *apiFunctionHash = NULL;

static void setupFunctionHash()
/* initialize the apiFunctionHash */
{
if (apiFunctionHash)
    return;	/* already done */

apiFunctionHash = hashNew(0);
hashAdd(apiFunctionHash, "list", &apiList);
hashAdd(apiFunctionHash, "getData", &apiGetData);
}

static struct hashEl *parsePathInfo(char *pathInfo, char *words[MAX_PATH_INFO])
/* given a pathInfo string: /command/subCommand/etc...
 *  parse that and return a function pointer and the parsed words
 * Returns NULL if not recognized
 */
{
char *tmp = cloneString(pathInfo);
/* skip the first leading slash to simplify chopByChar parsing */
tmp += 1;
int wordCount = chopByChar(tmp, '/', words, MAX_PATH_INFO);
if (wordCount < 1)
    return NULL;

struct hashEl *hel = hashLookup(apiFunctionHash, words[0]);
return hel;
}

static void tracksForUcscDb(char *db)
/* scan the specified database for all tracks */
{
struct hash *countTracks = hashNew(0);
char *chromName = NULL;
unsigned chromSize = largestChrom(db, &chromName);
hPrintf("<h4>Tracks in UCSC genome: '%s', longest chrom: %s:%u</h4>\n", db, chromName, chromSize);
struct trackDb *tdbList = obtainTdb(NULL, db);
struct trackDb *tdb;
hPrintf("<ul>\n");
hPrintf("<li>%s:%u</li>\n", chromName, chromSize);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next )
    {
    countOneTdb(db, tdb, countTracks, chromName, chromSize, NULL);
    if (timeOutReached())
	break;
    }
int trackCount = ptToInt(hashFindVal(countTracks, "track count"));
/* elCount - 1 since the 'track count' element isn't a track */
hPrintf("    <li>%d total tracks counted, %d different track types</li>\n", trackCount, countTracks->elCount - 1);
if (countTracks->elCount)
    {
    hPrintf("        <ol>\n");
    struct hashEl *hel, *helList = hashElListHash(countTracks);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	if (sameOk("track count", hel->name))
	    continue;
	if (isSupportedType(hel->name))
	    hPrintf("        <li>%d - %s - supported</li>\n", ptToInt(hel->val), hel->name);
	else
	    hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("        </ol>\n");
    }
hPrintf("</ul>\n");
hPrintf("</p>\n");
}	// static void tracksForUcscDb(char * db)

static void initUrlPrefix()
/* set up urlPrefix for self referenes */
{
char *httpHost = getenv("HTTP_HOST");

if (isEmpty(httpHost))
    urlPrefix = "";
else
    {
    if (! startsWith("hgwdev-api", httpHost))
	{
	if (startsWith("hgwdev",httpHost) || startsWith("genome-test", httpHost))
	    {
	    urlPrefix = "../cgi-bin/hubApi";
	    }
	}
    }
}

#ifdef NOT
static void showExamples(char *url, struct trackHubGenome *hubGenome, char *ucscDb)
{
hPrintf("<h2>Example URLs to return json data structures:</h2>\n");

hPrintf("<h3>listing functions</h3>\n");
hPrintf("<ol>\n");
hPrintf("<li><a href='%s/list/publicHubs' target=_blank>list public hubs</a> <em>%s/list/publicHubs</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/list/ucscGenomes' target=_blank>list database genomes</a> <em>%s/list/ucscGenomes</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/list/hubGenomes?hubUrl=%s' target=_blank>list genomes from specified hub</a> <em>%s/list/hubGenomes?hubUrl=%s</em></li>\n", urlPrefix, url, urlPrefix, url);
hPrintf("<li><a href='%s/list/tracks?hubUrl=%s&amp;genome=%s' target=_blank>list tracks from specified hub and genome</a> <em>%s/list/tracks?hubUrl=%s&amp;genome=%s</em></li>\n", urlPrefix, url, hubGenome->name, urlPrefix, url, hubGenome->name);
hPrintf("<li><a href='%s/list/tracks?db=%s' target=_blank>list tracks from specified UCSC database</a> <em>%s/list/tracks?db=%s</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/list/chromosomes?db=%s' target=_blank>list chromosomes from specified UCSC database</a> <em>%s/list/chromosomes?db=%s</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/list/chromosomes?db=%s&amp;track=gold' target=_blank>list chromosomes from specified track from UCSC databaset</a> <em>%s/list/chromosomes?db=%s&amp;track=gold</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("</ol>\n");

hPrintf("<h3>getData functions</h3>\n");
hPrintf("<ol>\n");
hPrintf("<li><a href='%s/getData/sequence?db=%s&amp;chrom=chrM' target=_blank>get sequence from specified database and chromosome</a> <em>%s/getData/sequence?db=%s&amp;chrom=chrM</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/getData/sequence?db=%s&amp;chrom=chrM&amp;start=0&amp;end=128' target=_blank>get sequence from specified database, chromosome with start,end coordinates</a> <em>%s/getData/sequence?db=%s&amp;chrom=chrM&amp;start=0&amp;end=128</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;track=gold' target=_blank>get entire track data from specified database and track name (gold == Assembly)</a> <em>%s/getData/track?db=%s&amp;track=gold</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chrM&amp;track=gold' target=_blank>get track data from specified database, chromosome and track name (gold == Assembly)</a> <em>%s/getData/track?db=%s&amp;chrom=chrM&amp;track=gold</em></li>\n", urlPrefix, ucscDb, urlPrefix, ucscDb);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chrI&amp;track=gold&amp;start=107680&amp;end=186148' target=_blank>get track data from specified database, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?db=%s&amp;chrom=chrI&amp;track=gold&amp;start=107680&amp;end=186148</em></li>\n", urlPrefix, defaultDb, urlPrefix, defaultDb);
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt&amp;genome=hg19&amp;track=ultraConserved' target=_blank>get entire track data from specified hub and track name</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt&amp;genome=hg19&amp;track=ultraConserved</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrCp&amp;track=assembly_' target=_blank>get track data from specified hub, chromosome and track name (full chromosome)</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrCp&amp;track=assembly_</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chr1&amp;track=assembly_&amp;start=0&amp;end=14309681' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chr1&amp;track=assembly_&amp;start=0&amp;end=14309681</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;track=gc5Base_' target=_blank>get all track data from specified hub and track name</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;track=gc5Base</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrMt&amp;track=gc5Base_&amp;start=143600&amp;end=143685' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrMt&amp;track=gc5Base&amp;start=143600&amp;end=143685</em></li>\n", urlPrefix, urlPrefix);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chrI&amp;track=gc5BaseBw&amp;start=107680&amp;end=186148' target=_blank>get bigWig track data from specified database, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?db=%s&amp;chrom=chrI&amp;track=gc5BaseBw&amp;start=107680&amp;end=186148</em></li>\n", urlPrefix, defaultDb, urlPrefix, defaultDb);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chrII&amp;track=ncbiRefSeqOther&amp;start=14334626&amp;end=14979625' target=_blank>get bigBed track data from specified database, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?db=%s&amp;chrom=chrII&amp;track=ncbiRefSeqOther&amp;start=14334626&amp;end=14979625</em></li>\n", urlPrefix, defaultDb, urlPrefix, defaultDb);
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chr1&amp;track=wgEncodeAwgDnaseDuke8988tUniPk&amp;start=14334626&amp;end=14979625' target=_blank>get narrowPeak track data from specified database, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?db=%s&amp;chrom=chr1&amp;track=wgEncodeAwgDnaseDuke8988tUniPk&amp;start=14334626&amp;end=14979625</em></li>\n", urlPrefix, "hg19", urlPrefix, "hg19");
hPrintf("<li><a href='%s/getData/track?db=%s&amp;chrom=chr1&amp;track=wgEncodeBroadHistoneOsteoP300kat3bPk&amp;start=14334626&amp;end=14979625' target=_blank>get broadPeak track data from specified database, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?db=%s&amp;chrom=chr1&amp;track=wgEncodeBroadHistoneOsteoP300kat3bPk&amp;start=14334626&amp;end=14979625</em></li>\n", urlPrefix, "hg19", urlPrefix, "hg19");

hPrintf("</ol>\n");

hPrintf("<h2>Example URLs to generate errors:</h2>\n");
hPrintf("<ol>\n");
hPrintf("<li><a href='%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrI&amp;track=assembly_&amp;start=0&amp;end=14309681' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>%s/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&amp;genome=_araTha1&amp;chrom=chrI&amp;track=assembly_&amp;start=0&amp;end=14309681</em></li>\n", urlPrefix, urlPrefix);
hPrintf("</ol>\n");
}	/*	static void showExamples()	*/

#endif

static void showCartDump()
/* for information purposes only during development, will become obsolete */
{
hPrintf("<h4>cart dump</h4>");
hPrintf("<pre>\n");
cartDump(cart);
hPrintf("</pre>\n");
}

static void sendJsonHogMessage(char *hogHost)
{
apiErrAbort("Your host, %s, has been sending too many requests lately and is "
       "unfairly loading our site, impacting performance for other users. "
       "Please contact genome@soe.ucsc.edu to ask that your site "
       "be reenabled.  Also, please consider downloading sequence and/or "
       "annotations in bulk -- see http://genome.ucsc.edu/downloads.html.",
       hogHost);
}

static void sendHogMessage(char *hogHost)
{
hPrintf("Your host, %s, has been sending too many requests lately and is "
       "unfairly loading our site, impacting performance for other users. "
       "Please contact genome@soe.ucsc.edu to ask that your site "
       "be reenabled.  Also, please consider downloading sequence and/or "
       "annotations in bulk -- see http://genome.ucsc.edu/downloads.html.",
       hogHost);
exit(0);
}

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
measureTiming = hPrintStatus() && isNotEmpty(cartOptionalString(cart, "measureTiming"));
measureTiming = TRUE;
char *database = NULL;
char *genome = NULL;

cgiVarSet("ignoreCookie", "1");

getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);
initSupportedTypes();
initUrlPrefix();

/* global variable for all workers to honor this limit */
maxItemsOutput = cartUsualInt(cart, "maxItemsOutput", maxItemsOutput);
if (maxItemsOutput > maxItemLimit)	/* safety check */
    maxItemsOutput = maxItemLimit;
if (maxItemsOutput < 1)	/* safety check */
    maxItemsOutput = 1;

debug = cartUsualBoolean(cart, "debug", debug);
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *pathInfo = getenv("PATH_INFO");
/* nothing on incoming path, then display the WEB page instead */
if (sameOk("/",pathInfo))
    pathInfo = NULL;

boolean commandError = FALSE;
/*expect no more than MAX_PATH_INFO number of words*/
char *words[MAX_PATH_INFO];

if (isNotEmpty(pathInfo))
    {
    setupFunctionHash();
    struct hashEl *hel = parsePathInfo(pathInfo, words);
    /* verify valid API command */

    if (hel)	/* have valid command */
	{
        hPrintDisable();
	puts("Content-Type:application/json");
	puts("\n");
	/* similar delay system as in DAS server */
	botDelay = hgBotDelayTimeFrac(delayFraction);
	if (botDelay > 0)
	    {
	    if (botDelay > 2000)
		{
		char *hogHost = getenv("REMOTE_ADDR");
		sendJsonHogMessage(hogHost);
		return;
		}
	sleep1000(botDelay);
	}
        void (*apiFunction)(char **) = hel->val;
        (*apiFunction)(words);
	return;
	}
     else
	commandError = TRUE;
    }

puts("Content-Type:text/html");
puts("\n");

/* similar delay system as in DAS server */
botDelay = hgBotDelayTimeFrac(delayFraction);
if (botDelay > 0)
    {
    if (botDelay > 2000)
	{
	char *hogHost = getenv("REMOTE_ADDR");
	sendHogMessage(hogHost);
	}
    sleep1000(botDelay);
    }

(void) hubPublicDbLoadAll();

struct dbDb *dbList = ucscDbDb();
char **ucscDbList = NULL;
int listSize = slCount(dbList);
AllocArray(ucscDbList, listSize);
struct dbDb *el = dbList;
int ucscDataBaseCount = 0;
int maxDbNameWidth = 0;
for ( ; el != NULL; el = el->next )
    {
    ucscDbList[ucscDataBaseCount++] = el->name;
    if (strlen(el->name) > maxDbNameWidth)
	maxDbNameWidth = strlen(el->name);
    }
maxDbNameWidth += 1;

cartWebStart(cart, database, "UCSC JSON API interface");

if (debug)
    {
    hPrintf("<ul>\n");
    hPrintf("<li>hgBotDelay: %d</li>\n", botDelay);
    char *envVar = getenv("BROWSER_HOST");
    hPrintf("<li>BROWSER_HOST:%s</li>\n", envVar);
    envVar = getenv("CONTEXT_DOCUMENT_ROOT");
    hPrintf("<li>CONTEXT_DOCUMENT_ROOT:%s</li>\n", envVar);
    envVar = getenv("CONTEXT_PREFIX");
    hPrintf("<li>CONTEXT_PREFIX:%s</li>\n", envVar);
    envVar = getenv("DOCUMENT_ROOT");
    hPrintf("<li>DOCUMENT_ROOT:%s</li>\n", envVar);
    envVar = getenv("HTTP_HOST");
    hPrintf("<li>HTTP_HOST:%s</li>\n", envVar);
    envVar = getenv("REQUEST_URI");
    hPrintf("<li>REQUEST_URI:%s</li>\n", envVar);
    envVar = getenv("SCRIPT_FILENAME");
    hPrintf("<li>SCRIPT_FILENAME:%s</li>\n", envVar);
    envVar = getenv("SCRIPT_NAME");
    hPrintf("<li>SCRIPT_NAME:%s</li>\n", envVar);
    envVar = getenv("SCRIPT_URI");
    hPrintf("<li>SCRIPT_URI:%s</li>\n", envVar);
    envVar = getenv("SCRIPT_URL");
    hPrintf("<li>SCRIPT_URL:%s</li>\n", envVar);
    envVar = getenv("SERVER_NAME");
    hPrintf("<li>SERVER_NAME:%s</li>\n", envVar);
    envVar = getenv("PATH_INFO");
    if (isNotEmpty(envVar))
       hPrintf("<li>PATH_INFO:'%s'</li>\n", envVar);
    else
       hPrintf("<li>PATH_INFO:&lt;empty&gt;</li>\n");
    hPrintf("</ul>\n");
    }

char *goOtherHub = cartUsualString(cart, "goOtherHub", defaultHub);
char *goUcscDb = cartUsualString(cart, "goUcscDb", "");
char *otherHubUrl = cartUsualString(cart, "urlHub", defaultHub);
char *goPublicHub = cartUsualString(cart, "goPublicHub", defaultHub);
char *hubDropDown = cartUsualString(cart, "publicHubs", defaultHub);
char *urlDropDown = urlFromShortLabel(hubDropDown);
char *ucscDb = cartUsualString(cart, "ucscGenomes", defaultDb);
char *urlInput = urlDropDown;	/* assume public hub */
if (sameWord("go", goOtherHub))	/* requested other hub URL */
    urlInput = otherHubUrl;
else if (isEmpty(otherHubUrl))
    otherHubUrl = urlInput;

if (commandError)
  {
  hPrintf("<h3>ERROR: no such command: '%s/%s' for endpoint '%s'</h3>", words[0], words[1], pathInfo);
  }

long lastTime = clock1000();
struct trackHub *hub = errCatchTrackHubOpen(urlInput);
if (measureTiming || debug)
    {
    long thisTime = clock1000();
    if (debug)
       hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

webIncludeFile("inc/dataApi.html");

// struct trackHubGenome *hubGenome = hub->genomeList;
// showExamples(urlInput, hubGenome, ucscDb);

if (debug)
    showCartDump();

hPrintf("<h2>Explore hub or database assemblies and tracks</h2>\n");

hPrintf("<form action='%s' name='hubApiUrl' id='hubApiUrl' method='GET'>\n\n", urlPrefix);

hPrintf("<b>Select public hub:&nbsp;</b>");
#define JBUFSIZE 2048
#define SMALLBUF 256
char javascript[JBUFSIZE];
struct slPair *events = NULL;
safef(javascript, sizeof(javascript), "this.lastIndex=this.selectedIndex;");
slPairAdd(&events, "focus", cloneString(javascript));

cgiMakeDropListClassWithIdStyleAndJavascript("publicHubs", "publicHubs",
    shortLabels, publicHubCount, hubDropDown, NULL, "width: 400px", events);

hWrites("&nbsp;");
hButton("goPublicHub", "go");

hPrintf("<br>Or, enter a hub URL:&nbsp;");
hPrintf("<input type='text' name='urlHub' id='urlHub' size='60' value='%s'>\n", otherHubUrl);
hWrites("&nbsp;");
hButton("goOtherHub", "go");

hPrintf("<br>Or, select a UCSC database name:&nbsp;");
maxDbNameWidth *= 9;  // 9 should be font width here
char widthPx[SMALLBUF];
safef(widthPx, sizeof(widthPx), "width: %dpx", maxDbNameWidth);
cgiMakeDropListClassWithIdStyleAndJavascript("ucscGenomes", "ucscGenomes",
    ucscDbList, ucscDataBaseCount, ucscDb, NULL, widthPx, events);
hWrites("&nbsp;");
hButton("goUcscDb", "go");

boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hPrintf("<br>\n&nbsp;&nbsp;");
hCheckBox("depthSearch", cartUsualBoolean(cart, "depthSearch", FALSE));
hPrintf("&nbsp;perform full bbi file measurement : %s (will time out if taking longer than %ld seconds)<br>\n", depthSearch ? "TRUE" : "FALSE", timeOutSeconds);
hPrintf("\n&nbsp;&nbsp;");
allTrackSettings = cartUsualBoolean(cart, "allTrackSettings", FALSE);
hCheckBox("allTrackSettings", allTrackSettings);
hPrintf("&nbsp;display all track settings for each track : %s<br>\n", allTrackSettings ? "TRUE" : "FALSE");

hPrintf("<br>\n</form>\n");

hPrintf("<p>\n");
if (sameWord("go", goUcscDb))	/* requested UCSC db track list */
    {
    tracksForUcscDb(ucscDb);
    }
else
    {
    hPrintf("<h4>Examine %s at: %s</h4>\n", sameWord("go",goPublicHub) ? "public hub" : "other hub", urlInput);
    hPrintf("<ul>\n");
    hPrintf("<li>%s</li>\n", hub->shortLabel);
    hPrintf("<li>%s</li>\n", hub->longLabel);
    if (isNotEmpty(hub->defaultDb))
        hPrintf("<li>%s - default database</li>\n", hub->defaultDb);
    hPrintf("</ul>\n");

    genomeList(hub);
    }

if (timedOut)
    hPrintf("<h1>Reached time out %ld seconds</h1>", timeOutSeconds);
if (measureTiming || debug)
    hPrintf("<em>Overall total time: %ld millis</em><br>\n", clock1000() - enteredMainTime);

hPrintf("</p>\n");

cartWebEnd();
}	/*	void doMiddle(struct cart *theCart)	*/

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
static char *excludeVars[] = {"Submit", "submit", "goOtherHub", "goPublicHub", "goUcscDb", "ucscGenomes", "publicHubs", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
measureTiming = TRUE;
verboseTimeInit();
trackCounter = hashNew(0);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
