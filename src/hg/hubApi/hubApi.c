/* hubApi - access mechanism to hub data resources. */
#include "dataApi.h"
#include "botDelay.h"
#include "jsHelper.h"
#include "srcVersion.h"
/* can not include bamFile.h with the liftOver business, there
 * is a conflict in a definition of the enum 'bed'
 */
#include "bamFile.h"

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

static int maxItemLimit = 1000000;   /* maximum of 1,000,000 items returned */
int maxItemsOutput = 1000000;   /* can be set in URL maxItemsOutput=N */
boolean reachedMaxItems = FALSE;	/* during getData, signal to return */
long long itemsReturned = 0;	/* for getData functions, number of items returned */
/* for debugging purpose, current bot delay value */
int botDelay = 0;
boolean debug = FALSE;	/* can be set in URL debug=1, to turn off: debug=0 */
#define delayFraction	0.03

/* default is to list all trackDb entries, composite containers too.
 * This option will limit to only the actual track entries with data
 */
boolean trackLeavesOnly = FALSE;  /* set by CGI parameter 'trackLeavesOnly' */
/* this selects output type 'arrays', where the default type is: objects */
boolean jsonOutputArrays = FALSE; /* set by CGI parameter 'jsonOutputArrays' */

boolean measureTiming = FALSE;	/* set by CGI parameters */

/* downloadUrl for use in error exits when reachedMaxItems */
struct dyString *downloadUrl = NULL;

/* valid argument listings to verify extraneous arguments */
char *argListPublicHubs[] = { NULL };
char *argListUcscGenomes[] = { NULL };
char *argListGenarkGenomes[] = { argMaxItemsOutput, argGenome, NULL };
char *argListHubGenomes[] = { argHubUrl, NULL };
char *argListTracks[] = { argGenome, argHubUrl, argTrackLeavesOnly, NULL };
char *argListChromosomes[] = { argGenome, argHubUrl, argTrack, NULL };
char *argListSchema[] = { argGenome, argHubUrl, argTrack, NULL };
char *argListFiles[] = { argGenome, argMaxItemsOutput, argFormat, argSkipContext, argFileType, NULL };
char *argGetDataTrack[] = { argGenome, argHubUrl, argTrack, argChrom, argStart, argEnd, argMaxItemsOutput, argJsonOutputArrays, NULL };
char *argGetDataSequence[] = { argGenome, argHubUrl, argTrack, argChrom, argStart, argEnd, argRevComp, NULL };
char *argSearch[] = {argSearchTerm, argGenome, argHubUrl, argCategories, NULL};
char *argFindGenome[] = {argQ, argMaxItemsOutput, argJsonOutputArrays, argStatsOnly, argBrowser, argYear, argCategory, argStatus, argLevel, argLiftable, NULL};
char *argLiftOver[] = {argFromGenome, argToGenome, argChrom, argStart, argEnd, argFilter, argMaxItemsOutput, NULL};

/* Global only to this one source file */
static struct cart *cart;             /* CGI and other variables */
static struct hash *oldVars = NULL;
static struct hash *trackCounter = NULL;
static long totalTracks = 0;
static boolean allTrackSettings = FALSE;	/* checkbox setting */
static char **shortLabels = NULL;	/* public hub short labels in array */
static int publicHubCount = 0;
static char *defaultHub = "Synonymous Constraint";
static char *defaultDb = "ce11";
long enteredMainTime = 0;	/* will become = clock1000() on entry */
		/* to allow calculation of when to bail out, taking too long */
static long timeOutSeconds = 100;
static boolean timedOut = FALSE;
static char *urlPrefix = "";	/* initalized to support self references */

/* supportedTypes will be initialized to a known supported set */
struct slName *supportedTypes = NULL;

static void initSupportedTypes()
/* initalize the list of supported track types */
{
struct slName *el = newSlName("bed");
slAddHead(&supportedTypes, el);
el = newSlName("wig");
slAddHead(&supportedTypes, el);
el = newSlName("wigMaf");
slAddHead(&supportedTypes, el);
el = newSlName("broadPeak");
slAddHead(&supportedTypes, el);
el = newSlName("narrowPeak");
slAddHead(&supportedTypes, el);
el = newSlName("bigBed");
slAddHead(&supportedTypes, el);
el = newSlName("bigWig");
slAddHead(&supportedTypes, el);
el = newSlName("bigLolly");
slAddHead(&supportedTypes, el);
el = newSlName("bigNarrowPeak");
slAddHead(&supportedTypes, el);
el = newSlName("bigGenePred");
slAddHead(&supportedTypes, el);
el = newSlName("genePred");
slAddHead(&supportedTypes, el);
el = newSlName("psl");
slAddHead(&supportedTypes, el);
el = newSlName("rmsk");
slAddHead(&supportedTypes, el);
el = newSlName("bigRmsk");
slAddHead(&supportedTypes, el);
el = newSlName("bigPsl");
slAddHead(&supportedTypes, el);
el = newSlName("altGraphX");
slAddHead(&supportedTypes, el);
el = newSlName("barChart");
slAddHead(&supportedTypes, el);
el = newSlName("chain");
slAddHead(&supportedTypes, el);
el = newSlName("ctgPos");
slAddHead(&supportedTypes, el);
el = newSlName("expRatio");
slAddHead(&supportedTypes, el);
el = newSlName("factorSource");
slAddHead(&supportedTypes, el);
el = newSlName("gvf");
slAddHead(&supportedTypes, el);
el = newSlName("interact");
slAddHead(&supportedTypes, el);
el = newSlName("netAlign");
slAddHead(&supportedTypes, el);
el = newSlName("peptideMapping");
slAddHead(&supportedTypes, el);
el = newSlName("pgSnp");
slAddHead(&supportedTypes, el);
el = newSlName("bigBarChart");
slAddHead(&supportedTypes, el);
el = newSlName("bigInteract");
slAddHead(&supportedTypes, el);
el = newSlName("clonePos");
slAddHead(&supportedTypes, el);
el = newSlName("bigDbSnp");
slAddHead(&supportedTypes, el);
el = newSlName("bigMaf");
slAddHead(&supportedTypes, el);
el = newSlName("bigChain");
slAddHead(&supportedTypes, el);
slNameSort(&supportedTypes);
}	/*	static void initSupportedTypes()	*/

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
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
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
}

static void sampleUrl(struct trackHub *hub, char *db, struct trackDb *tdb, char *errorString)
/* print out a sample getData URL */
{
char errorPrint[2048];
errorPrint[0] = 0;

if (isNotEmpty(errorString))
    {
    safef(errorPrint, sizeof(errorPrint), " <font color='red'>ERROR: %s</font>", errorString);
    }

boolean superChild = tdbIsSuperTrackChild(tdb);
char *genome = NULL;
if (hub)
    genome = hub->genomeList->name;

struct dyString *extraDyFlags = dyStringNew(128);
if (debug)
    dyStringAppend(extraDyFlags, ";debug=1");
if (jsonOutputArrays)
    dyStringAppend(extraDyFlags, ";jsonOutputArrays=1");
char *extraFlags = dyStringCannibalize(&extraDyFlags);

if (protectedTrack(db, tdb, tdb->track))
    hPrintf("<li>%s : %s &lt;protected data&gt;</li>\n", tdb->track, tdb->type);
else if (db)
    {
    if (hub)
	{
	char urlReference[2048];
	safef(urlReference,	sizeof(urlReference), " <a href='%s/getData/track?hubUrl=%s;genome=%s;track=%s;maxItemsOutput=5%s' target=_blank>(sample data)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, extraFlags, errorPrint);

	if (tdb->parent)
	    hPrintf("<li><b>%s</b>: %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
	else
	    hPrintf("<li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, urlReference);
	}
    else
	{
	char urlReference[2048];
	safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?genome=%s;track=%s;maxItemsOutput=5%s' target=_blank>(sample data)%s</a>\n", urlPrefix, db, tdb->track, extraFlags, errorPrint);

	if (superChild)
	    hPrintf("<li><b>%s</b>: %s superTrack child of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
	else if (tdb->parent)
	    hPrintf("<li><b>%s</b>: %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
	else
	    hPrintf("<li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, urlReference );
	}
    }
else if (hub)
    {
    char urlReference[2048];
    safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?hubUrl=%s;genome=%s;track=%s;maxItemsOutput=5%s' target=_blank>(sample data)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, extraFlags, errorPrint);

    if (tdb->parent)
	hPrintf("<li><b>%s</b>: %s subtrack of parent: %s%s</li>\n", tdb->track, tdb->type, tdb->parent->track, urlReference);
    else
	hPrintf("<li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, urlReference);
    }
else
    hPrintf("<li>%s : %s not db hub track ?</li>\n", tdb->track, tdb->type);
}

static void hubSampleUrl(struct trackHub *hub, char *db, struct trackDb *tdb,
    long chromCount, long itemCount, char *genome, char *errorString)
{
struct dyString *extraDyFlags = dyStringNew(128);
if (debug)
    dyStringAppend(extraDyFlags, ";debug=1");
if (jsonOutputArrays)
    dyStringAppend(extraDyFlags, ";jsonOutputArrays=1");
char *extraFlags = dyStringCannibalize(&extraDyFlags);

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
    if (allowedBigBedType(tdb->type))
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld item count ", chromCount, itemCount);
    else if (startsWithWord("bigWig", tdb->type))
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld bases covered ", chromCount, itemCount);
    else
        safef(countsMessage, sizeof(countsMessage), " : %ld chroms : %ld count ", chromCount, itemCount);
    }

if (protectedTrack(db, tdb, tdb->track))
    hPrintf("    <li><b>%s</b>: %s protected data</li>\n", tdb->track, tdb->type);
else if (isSupportedType(tdb->type))
    {
	char urlReference[2048];
	safef(urlReference, sizeof(urlReference), "<a href='%s/getData/track?hubUrl=%s;genome=%s;track=%s;maxItemsOutput=5%s' target=_blank>(sample data)%s</a>\n", urlPrefix, hub->url, genome, tdb->track, extraFlags, errorPrint);

	if (allowedBigBedType(tdb->type))
            hPrintf("    <li><b>%s</b>: %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
        else if (startsWithWord("bigWig", tdb->type))
            hPrintf("    <li><b>%s</b>: %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
        else
            hPrintf("    <li><b>%s</b>: %s%s%s</li>\n", tdb->track, tdb->type, countsMessage, urlReference);
    }
else
    {
        if (allowedBigBedType(tdb->type))
            hPrintf("    <li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, countsMessage);
        else if (startsWithWord("bigWig", tdb->type))
            hPrintf("    <li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, countsMessage);
        else
            hPrintf("    <li><b>%s</b>: %s%s</li>\n", tdb->track, tdb->type, countsMessage);
    }
}	/* static void hubSampleUrl(struct trackHub *hub, struct trackDb *tdb,
	 * long chromCount, long itemCount, char *genome)
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
            || startsWithWord("bigDbSnp", type)
            || startsWithWord("bigMaf", type)
            || startsWithWord("bigChain", type)
            || startsWithWord("bigRmsk", type)
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
/* tdb has subtracks, show only subTracks, no details, this is RECURSIVE */
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
            char *bigDataIndex = NULL;
            char *relIdxUrl = trackDbSetting(tdbEl, "bigDataIndex");
            if (relIdxUrl != NULL)
                bigDataIndex = trackHubRelativeUrl(hub->genomeList->trackDbFile, relIdxUrl);
            char *bigDataUrl = trackDbSetting(tdbEl, "bigDataUrl");
            char *longName = NULL;
            unsigned longSize = 0;
            struct dyString *errors = dyStringNew(1024);
            (void) bbiBriefMeasure(tdbEl->type, bigDataUrl, bigDataIndex, &chromCount, &itemCount, errors, &longName, &longSize);
            chromSize = longSize;
            chromName = longName;
	    }
        if (tdbIsCompositeView(tdbEl))
	    hPrintf("<li><b>%s</b>: %s : composite view of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	else
	    {
	    if (isSupportedType(tdbEl->type))
		hubSampleUrl(hub, db, tdbEl, chromCount, itemCount, genome, errorString);
	    else
		hPrintf("<li><b>%s</b>: %s : subtrack of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
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
    struct dyString *extraDyFlags = dyStringNew(128);
    if (debug)
	dyStringAppend(extraDyFlags, ";debug=1");
    if (jsonOutputArrays)
	dyStringAppend(extraDyFlags, ";jsonOutputArrays=1");
    char *extraFlags = dyStringCannibalize(&extraDyFlags);
    struct trackDb *tdbEl = NULL;
    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
        if (tdbIsCompositeView(tdbEl))
	    hPrintf("<li><b>%s</b>: %s : composite view of parent: %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track);
	else
	    {
	    if (isSupportedType(tdbEl->type))
		sampleUrl(hub, db, tdbEl, errorString);
	    else
                if (hub && hub->url)
		    hPrintf("<li><b>%s</b>: %s : subtrack of parent: %s, genome %s, url %s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track, db, hub->url);
                else
		    {
		    char urlReference[2048];
		    safef(urlReference, sizeof(urlReference), " <a href='%s/getData/track?genome=%s;track=%s;maxItemsOutput=5%s' target=_blank>(sample data)</a>\n", urlPrefix, db, tdbEl->track, extraFlags);
		    hPrintf("<li><b>%s</b>: %s : subtrack of parent: %s%s</li>\n", tdbEl->track, tdbEl->type, tdbEl->parent->track, urlReference);
		    }
	    }
	hashCountTrack(tdbEl, countTracks);
        if (tdbEl->subtracks)
	    showSubTracks(hub, db, tdbEl, countTracks, chromName, chromSize, errorString);
	}
    }
hPrintf("    </ul></li>\n");
}

static void trackSettings(char *db, struct trackDb *tdb, struct hash *countTracks)
/* process the settingsHash for a trackDb, recursive when subtracks */
{
hPrintf("    <li><ul>\n");
boolean protectedData = protectedTrack(db, tdb, tdb->track);
struct hashEl *hel;
struct hashCookie hc = hashFirst(tdb->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    if (sameWord("track", hel->name))
	continue;	// already output in header
    if (sameWord("tableBrowser", hel->name)
		&& startsWithWord("off", (char*)hel->val))
	hPrintf("    <li><b>protectedData</b>: 'true'</li>\n");
    else if (protectedData && sameWord("bigDataUrl", hel->name))
	hPrintf("    <li><b>bigDataUrl</b>: &lt;protected data&gt;</li>\n");
    else if (isEmpty((char *)hel->val))
	hPrintf("    <li><b>%s</b>: &lt;empty&gt;</li>\n", hel->name);
    else
	hPrintf("    <li><b>%s</b>: '%s'</li>\n", hel->name, (char *)hel->val);
    }
if (tdb->subtracks)
    {
    struct trackDb *tdbEl = NULL;
    if (debug)
	hPrintf("   <li>has %d subtrack(s)</li>\n", slCount(tdb->subtracks));

    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
        hPrintf("<li>subtrack: %s of parent: %s : type: '%s' (TBD: sample data)</li>\n", tdbEl->track, tdbEl->parent->track, tdbEl->type);
	hashCountTrack(tdbEl, countTracks);
	trackSettings(db, tdbEl, countTracks);
	}
    }
hPrintf("    </ul></li>\n");
}

static void hubCountOneTdb(struct trackHub *hub, char *db, struct trackDb *tdb,
    char *bigDataIndex, struct hash *countTracks, char *chromName,
    unsigned chromSize, char *genome)
{
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
boolean superChild = tdbIsSuperTrackChild(tdb);
boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hashCountTrack(tdb, countTracks);

long chromCount = 0;
long itemCount = 0;

struct dyString *errors = dyStringNew(1024);

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
	    hubSampleUrl(hub, db, tdb, chromCount, itemCount, genome, errors->string);
    }
else
    {
    if (compositeContainer)
        hPrintf("    <li><b>%s</b>: %s : composite track container has %d subtracks</li>\n", tdb->track, tdb->type, slCount(tdb->subtracks));
    else if (compositeView)
        hPrintf("    <li><b>%s</b>: %s : composite view of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
    else if (superChild)
	{
	if (isSupportedType(tdb->type))
	    hubSampleUrl(hub, db, tdb, chromCount, itemCount, genome,  errors->string);
	else
	    hPrintf("    <li><b>%s</b>: %s : superTrack child of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
	}
    else if (! depthSearch && bigDataUrl)
	{
        if (isSupportedType(tdb->type))
	    {
	    hubSampleUrl(hub, db, tdb, chromCount, itemCount, genome, errors->string);
	    }
	}
    else
	{
        if (isSupportedType(tdb->type))
	    {
	    hubSampleUrl(hub, db, tdb, chromCount, itemCount, genome, errors->string);
	    }
	else
	    hPrintf("    <li><b>%s</b>: %s (what is this)</li>\n", tdb->track, tdb->type);
        }
    }
if (allTrackSettings)
    {
    hPrintf("    <li><ul>\n");
    trackSettings(db, tdb, countTracks); /* show all settings */
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
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
boolean superChild = tdbIsSuperTrackChild(tdb);
boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
boolean protectedData = protectedTrack(db, tdb, tdb->track);
hashCountTrack(tdb, countTracks);

if (compositeContainer)
    hPrintf("    <li><b>%s</b>: %s : composite track container has %d subtracks</li>\n", tdb->track, tdb->type, slCount(tdb->subtracks));
else if (compositeView)
    hPrintf("    <li><b>%s</b>: %s : composite view of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
else if (superChild)
    {
    if (isSupportedType(tdb->type))
        sampleUrl(NULL, db, tdb, errorString);
    else
	hPrintf("    <li><b>%s</b>: %s : superTrack child of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
    }
else if (! depthSearch && bigDataUrl)
    {
    if (protectedData)
	hPrintf("    <li><b>%s</b>: %s : &lt;protected data&gt;</li>\n", tdb->track, tdb->type);
    else
	hPrintf("    <li><b>%s</b>: %s : %s</li>\n", tdb->track, tdb->type, bigDataUrl);
    }
else
    {
    if (isSupportedType(tdb->type))
        sampleUrl(NULL, db, tdb, errorString);
    else
        hPrintf("    <li><b>%s</b>: %s</li>\n", tdb->track, tdb->type);
    }

if (allTrackSettings)
    {
    hPrintf("    <li><ul>\n");
    trackSettings(db, tdb, countTracks); /* show all settings */
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

static unsigned largestChrom(char *db, char **nameReturn, int *chromCount)
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
if (chromCount)
    {
    sqlSafef(query, sizeof(query), "select count(*) from chromInfo");
    *chromCount = sqlQuickNum(conn, query);
    }
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
	int chromCount = 0;
        if (isEmpty(genome->twoBitPath))
            chromSize = largestChrom(defaultGenome, &chromName, &chromCount);
	hubCountOneTdb(hub, defaultGenome, tdb, bigDataIndex, countTracks, chromName, chromSize, defaultGenome);
	if (timeOutReached())
	    break;
	}	/*	for ( tdb = topTrackDb; tdb; tdb = tdb->next )	*/
    hPrintf("    <li>%d different track types</li>\n",countTracks->elCount - 1);
    /* add this single genome count to the overall multi-genome counts */
    if (countTracks->elCount)
	{
        hPrintf("        <li><ul>\n");
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
		hPrintf("        <li>%d - %s - not supported</li>\n", ptToInt(hel->val), hel->name);
	    }
        hPrintf("        </ul></li>\n");
	}
    }
else
    hPrintf("    <li>no trackTopDb ?</li>\n");

hPrintf("    </ul></li>\n");
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
    if (sameWord("description", hel->name) ||
	sameWord("defaultPos", hel->name) ||
	sameWord("organism", hel->name) ||
	sameWord("groups", hel->name) ||
	sameWord("twoBitPath", hel->name) ||
	sameWord("genome", hel->name)
	)
	continue;	// already output in header
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	hubTrackList(hub, tdb, genome);
    else
	hPrintf("    <li><b>%s</b>: %s</li>\n", hel->name, (char *)hel->val);
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

static void hubInfo(char *tag, char *val)
/* print one list element with the given tag and value, show <empty>
 * if not value present
 */
{
if (isNotEmpty(val))
    hPrintf("<li><b>%s</b>: '%s'</li>\n", tag, val);
else
    hPrintf("<li><b>%s</b>: &lt;empty&gt;</li>\n", tag);
}

static void genomeList(struct trackHub *hubTop)
/* follow the pointers from the trackHub to trackHubGenome and around
 * in a circle from one to the other to find all hub resources
 */
{
long totalAssemblyCount = 0;
struct trackHubGenome *genome = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub (<a href='%s/list/hubGenomes?hubUrl=%s' target=_blank>JSON example list hub genomes)</a></h4>\n", urlPrefix, hubTop->url);

if (NULL == genome)
    {
    hPrintf("<h4>odd error, can not find a genomeList ? at url: '%s'</h4>\n", hubTop->url);
    return;
    }

hPrintf("<ul>\n");
long lastTime = clock1000();
for ( ; genome; genome = genome->next )
    {
    ++totalAssemblyCount;
    char urlReference[2048];
    if (isNotEmpty(genome->twoBitPath))
	{
	hPrintf("<li><b>Assembly genome</b> '%s' <b>twoBitPath</b>: '%s'</li>\n", genome->name, genome->twoBitPath);
	char *chromName = NULL;
	struct chromInfo *ci = trackHubAllChromInfo(genome->name);
        unsigned chromSize = largestChromInfo(ci, &chromName);
	char sizeString[64];
	sprintLongWithCommas(sizeString, chromSize);
	hPrintf("<li><b>Sequence count</b> %d, <b>largest</b>: %s at %s bases</li>\n", slCount(ci), chromName, sizeString);
       safef(urlReference, sizeof(urlReference), " <a href='%s/getData/sequence?hubUrl=%s;genome=%s;chrom=%s;start=%u;end=%u' target=_blank>JSON example sequence output: %s:%u-%u</a>", urlPrefix, hubTop->url, genome->name, chromName, chromSize/4, (chromSize/4)+128, chromName, chromSize/4, (chromSize/4)+128);
        hPrintf("<li>%s</li>\n", urlReference);
	}
    safef(urlReference, sizeof(urlReference), " <a href='%s/list/tracks?hubUrl=%s;genome=%s%s' target=_blank>JSON example list tracks output</a>", urlPrefix, hubTop->url, genome->name, trackLeavesOnly ? ";trackLeavesOnly=1" : "");
    hPrintf("<li>%s</li>\n", urlReference);
    hubInfo("organism", genome->organism);
    hubInfo("name", genome->name);
    hubInfo("description", genome->description);
    hubInfo("groups", genome->groups);
    hubInfo("defaultPos", genome->defaultPos);
    hubInfo("trackDbFile", genome->trackDbFile);
    hubAssemblySettings(hubTop, genome);
    struct trackDb *tdbList = obtainTdb(genome, NULL);
    hubTrackList(hubTop, tdbList, genome);
    if (measureTiming)
	{
	long thisTime = clock1000();
	hPrintf("<li><em>processing time %s: %ld millis</em></li>\n", genome->name, thisTime - lastTime);
	hPrintf("<hr>\n");
        }
    if (timeOutReached())
	break;
    }
if (trackCounter->elCount)
    {
    hPrintf("    <li>total genome assembly count: %ld</li>\n", totalAssemblyCount);
    hPrintf("    <li>%ld total tracks counted, %d different track types:</li>\n", totalTracks, trackCounter->elCount);
    hPrintf("    <li><ul>\n");
    struct hashEl *hel, *helList = hashElListHash(trackCounter);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	hPrintf("    <li>%d - %s - total</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("    </ul></li>\n");
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

static struct hash *apiFunctionHash = NULL;

static void setupFunctionHash()
/* initialize the apiFunctionHash */
{
if (apiFunctionHash)
    return;	/* already done */

apiFunctionHash = hashNew(0);
hashAdd(apiFunctionHash, "list", &apiList);
hashAdd(apiFunctionHash, "getData", &apiGetData);
hashAdd(apiFunctionHash, "search", &apiSearch);
hashAdd(apiFunctionHash, "findGenome", &apiFindGenome);
hashAdd(apiFunctionHash, "liftOver", &apiLiftOver);
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
if (wordCount < 1 || wordCount > 2)
    return NULL;	/* only 2 words allowed */

struct hashEl *hel = hashLookup(apiFunctionHash, words[0]);
return hel;
}

static void tracksForUcscDb(char *db)
/* scan the specified database for all tracks */
{
struct hash *countTracks = hashNew(0);
char *chromName = NULL;
int chromCount = 0;
unsigned chromSize = largestChrom(db, &chromName, &chromCount);
char countString[64];
sprintLongWithCommas(countString, chromCount);
char sizeString[64];
sprintLongWithCommas(sizeString, chromSize);
hPrintf("<h4>Tracks in UCSC genome: '%s', chrom count: %s, longest chrom: %s : %s</h4>\n", db, countString, chromName, sizeString);

char urlReference[2048];
safef(urlReference, sizeof(urlReference), " <a href='%s/list/tracks?genome=%s%s' target=_blank>JSON output: list tracks</a>", urlPrefix, db, trackLeavesOnly ? ";trackLeavesOnly=1" : "");
hPrintf("<h4>%s</h4>\n", urlReference);

struct trackDb *tdbList = obtainTdb(NULL, db);
struct trackDb *tdb;
hPrintf("<ul>\n");
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
    hPrintf("        <ul>\n");
    struct hashEl *hel, *helList = hashElListHash(countTracks);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	if (sameOk("track count", hel->name))
	    continue;
	if (isSupportedType(hel->name))
	    hPrintf("        <li>%d - %s - supported</li>\n", ptToInt(hel->val), hel->name);
	else
	    hPrintf("        <li>%d - %s - not supported</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("        </ul>\n");
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
apiErrAbort(err429, err429Msg, "Your host, %s, has been sending too many requests lately and is "
       "unfairly loading our site, impacting performance for other users. "
       "Please contact genome-www@soe.ucsc.edu to ask that your site "
       "be reenabled.  Also, please consider downloading sequence and/or "
       "annotations in bulk -- see http://genome.ucsc.edu/downloads.html.",
       hogHost);
}

static void sendHogMessage(char *hogHost)
{
puts("Content-Type:text/html");
hPrintf("Status: %d %s\n", err429, err429Msg);
puts("Retry-After: 30");
puts("\n");

hPrintf("<!DOCTYPE HTML>\n");
hPrintf("<html lang='en'>\n");
hPrintf("<head>\n");
hPrintf("<meta charset=\"utf-8\">\n");
hPrintf("<title>Status %d %s</title></head>\n", err429, err429Msg);

hPrintf("<body><h1>Status %d %s</h1><p>\n", err429, err429Msg);
hPrintf("Your host, %s, has been sending too many requests lately and is "
       "unfairly loading our site, impacting performance for other users. "
       "Please contact genome-www@soe.ucsc.edu to ask that your site "
       "be reenabled.  Also, please consider downloading sequence and/or "
       "annotations in bulk -- see http://genome.ucsc.edu/downloads.html.",
       hogHost);
hPrintf("</p></body></html>\n");
cgiExitTime("hubApi hogExit", enteredMainTime);
exit(0);
}

static void hogExit()
/* bottleneck server requests exit */
{
char *hogHost = getenv("REMOTE_ADDR");
char *pathInfo = getenv("PATH_INFO");
/* nothing on incoming path, then display the WEB page instead */
if (sameOk("/",pathInfo))
    pathInfo = NULL;
if (isNotEmpty(pathInfo))
    {
    sendJsonHogMessage(hogHost);
    }
else
    {
    sendHogMessage(hogHost);
    }
}	/*	static void hogExit()	*/

/* name of radio button group */
#define RADIO_GROUP	"selectRadio"
/* button functions */
#define RADIO_PUBHUB	"pubHub"
#define RADIO_OTHERHUB	"otherHub"
#define RADIO_UCSCDB	"ucscDb"

static void selectionForm()
/* setup the selection pull-downs for source */
{
char *hubDropDown = cartUsualString(cart, "publicHubs", defaultHub);
char *urlDropDown = urlFromShortLabel(hubDropDown);
char *otherHubUrl = cartUsualString(cart, "urlHub", "");
char *ucscDb = cartUsualString(cart, "ucscGenome", defaultDb);

if (isEmpty(otherHubUrl))
    otherHubUrl = urlDropDown;

char *radioOn = cartUsualString(cart, RADIO_GROUP, RADIO_PUBHUB);

/* create border around table, but not inside the table with the data */
hPrintf("<table border=4>\n");
hPrintf("<tr><td><table border=0>\n");
hPrintf("<tr><th colspan=3>Select one of these three sources, and display options:</th></tr>\n");

int maxDbNameWidth = 0;
struct dbDb *dbList = ucscDbDb();
char **ucscDbList = NULL;
int listSize = slCount(dbList);
AllocArray(ucscDbList, listSize);
struct dbDb *el = dbList;
int ucscDataBaseCount = 0;
for ( ; el != NULL; el = el->next )
    {
    ucscDbList[ucscDataBaseCount++] = el->name;
    if (strlen(el->name) > maxDbNameWidth)
	maxDbNameWidth = strlen(el->name);
    }
maxDbNameWidth += 1;

hPrintf("<form action='%s' name='hubApiUrl' id='hubApiUrl' method='GET'>\n\n", urlPrefix);

hWrites("<tr><td>");
jsMakeTrackingRadioButton(RADIO_GROUP, "typeOneJs", RADIO_PUBHUB, radioOn);
hWrites("</td><th>");
hWrites("Select public hub:");
hWrites("</th><td>");

#define JBUFSIZE 2048
#define SMALLBUF 256
char javascript[JBUFSIZE];
struct slPair *events = NULL;
safef(javascript, sizeof(javascript), "this.lastIndex=this.selectedIndex;");
slPairAdd(&events, "focus", cloneString(javascript));

cgiMakeDropListClassWithIdStyleAndJavascript("publicHubs", "publicHubs",
    shortLabels, publicHubCount, hubDropDown, NULL, "width: 400px", events);

jsOnEventById("change", "publicHubs", "document.getElementById('"RADIO_GROUP"_"RADIO_PUBHUB"').checked=true;");
hWrites("</td></tr>\n");

hWrites("<tr><td>");
jsMakeTrackingRadioButton(RADIO_GROUP, "typeOneJs", RADIO_OTHERHUB, radioOn);
hWrites("</td><th>");
hWrites("enter a hub URL:");
hWrites("</th><td>");
hPrintf("<input type='text' name='urlHub' id='urlHub' size='60' value='%s'>\n", otherHubUrl);
jsOnEventById("change", "urlHub", "document.getElementById('"RADIO_GROUP"_"RADIO_OTHERHUB"').checked=true;");
hWrites("</td></tr>\n");

hWrites("<tr><td>");
jsMakeTrackingRadioButton(RADIO_GROUP, "typeOneJs", RADIO_UCSCDB, radioOn);
hWrites("</td><th>");
hWrites("select a UCSC database name:");
hWrites("</th><td>");
maxDbNameWidth *= 9;  // 9 should be font width here
char widthPx[SMALLBUF];
safef(widthPx, sizeof(widthPx), "width: %dpx", maxDbNameWidth);
cgiMakeDropListClassWithIdStyleAndJavascript("ucscGenome", "ucscGenome",
    ucscDbList, ucscDataBaseCount, ucscDb, NULL, widthPx, events);
jsOnEventById("change", "ucscGenome", "document.getElementById('"RADIO_GROUP"_"RADIO_UCSCDB"').checked=true;");
hWrites("</td></tr>\n");

allTrackSettings = cartUsualBoolean(cart, "allTrackSettings", FALSE);
hWrites("<tr><td>&nbsp;</td><th>display control:</th><td>");
hCheckBox("allTrackSettings", allTrackSettings);
hWrites("&nbsp;display all track settings for each track");
hWrites("</td></tr>\n");

trackLeavesOnly = cartUsualBoolean(cart, "trackLeavesOnly", trackLeavesOnly);
hWrites("<tr><td>&nbsp;</td><th>JSON list output:</th><td>");
hCheckBox("trackLeavesOnly", trackLeavesOnly);
hWrites("&nbsp;show only data tracks, do not show composite container information");
hWrites("</td></tr>\n");

jsonOutputArrays = cartUsualBoolean(cart, "jsonOutputArrays", jsonOutputArrays);
hWrites("<tr><td>&nbsp;</td><th>JSON output type:</th><td>");
hCheckBox("jsonOutputArrays", jsonOutputArrays);
hWrites("&nbsp;more array data than objects (default: mostly object output)");
hWrites("</td></tr>\n");

/* go button at the bottom of the table */
hWrites("<tr><td>&nbsp;</td><td align=center>");
hButton("sourceSelected", "go");
hWrites("</td><td>press 'go' after selections made</td></tr>\n");

hPrintf("</form>\n");

hPrintf("<tr><th colspan=3>(example JSON list output: <a href='/list/publicHubs' target=_blank>Public hubs</a>, and <a href='/list/ucscGenomes' target=_blank>UCSC database genomes</a>)</th></tr>\n");

hPrintf("</table>\n");
hPrintf("</td></tr></table>\n");

/* how does debug carry forward ? */
// if (debug)
//    cgiMakeHiddenVar("debug", "1");
}

static void apiRequest(char *pathInfo)
{
hPrintDisable();
/*expect no more than MAX_PATH_INFO number of words*/
char *words[MAX_PATH_INFO];
/* can immediately verify valid parameters right here right now */
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
char *db = cgiOptionalString("genome");
char *hubUrl = cgiOptionalString("hubUrl");
struct dyString *errorMsg = dyStringNew(128);

// first check for curated hubs
if (isEmpty(hubUrl) && isNotEmpty(db))
    {
    char *newHubUrl;
    if (hubConnectGetCuratedUrl(db, &newHubUrl))
        {
        hubUrl = newHubUrl;  // use curated hub hubUrl
        cgiVarSet("hubUrl", hubUrl);   // subsequent code grabs hubUrl from env
        }
    if (startsWith("hub_", db))
        {
        unsigned hubId = hubIdFromTrackName(db);
        struct hubConnectStatus *hub = hubFromId(hubId);
        if (hub)
            {
            // change up the arguments so it appears an assembly hub had been requested correctly
            db = trackHubSkipHubName(db);
            cgiVarSet("genome", db);
            hubUrl = hub->hubUrl;
            cgiVarSet("hubUrl", hubUrl);
            }
        }
    }

if (isEmpty(hubUrl) && isNotEmpty(db))
    {
    if ( ! isGenArk(db) )
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
	if (NULL == conn)
	    dyStringPrintf(errorMsg, "can not find genome='%s' for endpoint '%s'", db, pathInfo);
	else
	    hFreeConn(&conn);
	}
    }
if (isNotEmpty(start) || isNotEmpty(end))
    {
    long long llStart = -1;
    long long llEnd = -1;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        if (isNotEmpty(start))
            llStart = sqlLongLong(start);
        if (isNotEmpty(end))
            llEnd = sqlLongLong(end);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        if (isNotEmpty(errorMsg->string))
            dyStringPrintf(errorMsg, ", ");
        dyStringPrintf(errorMsg, "%s", errCatch->message->string);
        if (isNotEmpty(start) && (-1 == llStart))
            dyStringPrintf(errorMsg, ", can not recognize start coordinate: '%s'", start);
        if (isNotEmpty(end) && (-1 == llEnd))
            dyStringPrintf(errorMsg, ", can not recognize end coordinate: '%s'", end);
        }
    else
        {
        if ( (llStart < 0) || (llEnd < 0) || (llEnd <= llStart) )
            {
            if (isNotEmpty(errorMsg->string))
                dyStringPrintf(errorMsg, ", ");
            dyStringPrintf(errorMsg, "illegal start,end coordinates given: %s,%s, 'end' must be greater than 'start', and start greater than or equal to zero", start, end);
            }
        }
    errCatchFree(&errCatch);
    }

if (isNotEmpty(errorMsg->string))
    apiErrAbort(err400, err400Msg, "%s", errorMsg->string);

setupFunctionHash();
struct hashEl *hel = parsePathInfo(pathInfo, words);
/* verify valid API command */
if (hel)	/* have valid command */
    {
    hPrintDisable();
    void (*apiFunction)(char **) = hel->val;
    (*apiFunction)(words);
    return;
    }
 else
    apiErrAbort(err400, err400Msg, "no such command: '/%s", pathInfo);
    /* due to Apache rewrite rules, will never be called with this error */
}	/*	static void apiRequest(char *pathInfo)	*/

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
// measureTiming = isNotEmpty(cartOptionalString(cart, "measureTiming"));
char *database = NULL;
char *genome = NULL;

if (measureTiming)
    startProcessTiming();

cgiVarSet("ignoreCookie", "1");

getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);
initUrlPrefix();

trackLeavesOnly = cartUsualBoolean(cart, "trackLeavesOnly", trackLeavesOnly);
jsonOutputArrays = cartUsualBoolean(cart, "jsonOutputArrays", jsonOutputArrays);

char *pathInfo = getenv("PATH_INFO");
/* nothing on incoming path, then display the WEB page instead */
if (sameOk("/",pathInfo))
    pathInfo = NULL;

(void) hubPublicDbLoadAll();

webStartJWest(cart, database, "Genome Browser API");
// webStartGbNoBanner(cart, database, "UCSC JSON API interface");
// webStartGbOptionalBanner(cart, database, "UCSC JSON API interface", TRUE, FALSE);

hPrintf("<div class='container-fluid gbPage'>\n");
/* these style mentions need to go into custom css file */
hPrintf("<div style='border:10px solid white'>\n");

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

char *otherHubUrl = cartUsualString(cart, "urlHub", "");
char *hubDropDown = cartUsualString(cart, "publicHubs", defaultHub);
char *urlDropDown = urlFromShortLabel(hubDropDown);
char *ucscDb = cartUsualString(cart, "ucscGenome", defaultDb);
char *selectRadio = cartUsualString(cart, RADIO_GROUP, RADIO_PUBHUB);
char *urlInput = urlDropDown;	/* assume public hub */
if (debug)
    {
    hPrintf("<ul>\n");
    hPrintf("<li>otherHubUrl: '%s'</li>\n", otherHubUrl);
    hPrintf("<li>hubDropDown: '%s'</li>\n", hubDropDown);
    hPrintf("<li>urlDropDown: '%s'</li>\n", urlDropDown);
    hPrintf("<li>ucscDb: '%s'</li>\n", ucscDb);
    hPrintf("<li>urlInput: '%s'</li>\n", urlInput);
    hPrintf("<li>trackLeavesOnly: '%s'</li>\n", trackLeavesOnly ? "TRUE" : "FALSE");
    hPrintf("<li>jsonOutputArrays: '%s'</li>\n", jsonOutputArrays ? "TRUE" : "FALSE");
    hPrintf("</ul>\n");
    }
if (isEmpty(otherHubUrl))
    otherHubUrl = urlInput;

if (sameWord(RADIO_OTHERHUB, selectRadio))	/* requested other hub URL */
    urlInput = otherHubUrl;

long lastTime = clock1000();
struct trackHub *hub = errCatchTrackHubOpen(urlInput);
if (measureTiming)
    {
    long thisTime = clock1000();
    hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

hPrintf("<h3>Documentation: <a href='../../goldenPath/help/api.html'>API definitions/help</a>, and <a href='../../goldenPath/help/trackDb/trackDbHub.html' target=_blank>Track definition document</a> for definitions of track settings.</h3>\n");

if (debug)
    showCartDump();

hPrintf("<h2>Explore hub or database assemblies and tracks (v%s)</h2>\n", SRC_VERSION);

selectionForm();

/* these style mentions need to go into custom css file */
hPrintf("<div style='height:500px;overflow:scroll'>\n");

if (sameWord(RADIO_UCSCDB, selectRadio))  /* requested UCSC db track list */
    {
    tracksForUcscDb(ucscDb);
    }
else
    {
    hPrintf("<h3>%s url: <em>%s</em></h3>\n", sameWord(RADIO_PUBHUB,selectRadio) ? "Public hub" : "Other hub", urlInput);
    hPrintf("<ul>\n");
    hubInfo("hub name", hub->name);
    hubInfo("short label", hub->shortLabel);
    hubInfo("long label", hub->longLabel);
    hubInfo("genomes file", hub->genomesFile);
    hubInfo("default db", hub->defaultDb);
    hubInfo("description url", hub->descriptionUrl);
    hubInfo("email", hub->email);
    if (debug)
	{
	hubInfo("version", hub->version);	/* UCSC internal info */
	hubInfo("level", hub->level);		/* UCSC internal info */
	}
    hPrintf("</ul>\n");

    genomeList(hub);
    }

if (timedOut)
    hPrintf("<h1>Reached time out %ld seconds</h1>", timeOutSeconds);
if (measureTiming)
    hPrintf("<em>Overall total time: %ld millis</em><br>\n", clock1000() - enteredMainTime);

hPrintf("</div> <!-- end of text analysis output -->\n");
hPrintf("</div> <!-- end of surrounding border-->\n");
hPrintf("</div> <!-- end this page contents -->\n");

webIncludeFile("inc/jWestFooter.html");
webEndJWest();
// cartWebEnd();
}	/*	void doMiddle(struct cart *theCart)	*/

static void setGlobalCgiVars()
/* check for legal CGI variables and set global flags */
{
/* count the arguments to see if any occur more than once */
struct hash *varCounter = hashNew(0);
struct cgiVar *varList = cgiVarList();
struct cgiVar *el = varList;
for ( ; el; el = el->next)
    {
    hashIncInt(varCounter, el->name);
    }
struct hashCookie cookie = hashFirst(varCounter);
struct hashEl *hel = NULL;
for ( hel = hashNext(&cookie); hel; hel = hashNext(&cookie))
    {
    if (ptToInt(hel->val) > 1)
	apiErrAbort(err400, err400Msg, "parameter '%s' found %d times, only one instance allowed", hel->name, ptToInt(hel->val));
    }

char *trackLeaves = cgiOptionalString("trackLeavesOnly");
if (isNotEmpty(trackLeaves))
    {
    if (SETTING_IS_ON(trackLeaves))
	trackLeavesOnly = TRUE;
    else if (sameString("0", trackLeaves))
	trackLeavesOnly = FALSE;
    else
	apiErrAbort(err400, err400Msg, "unrecognized 'trackLeavesOnly=%s' argument, can only be =1 or =0", trackLeaves);
    }

char *jsonArray = cgiOptionalString("jsonOutputArrays");
if (isNotEmpty(jsonArray))
    {
    if (SETTING_IS_ON(jsonArray))
	jsonOutputArrays = TRUE;
    else if (sameString("0", jsonArray))
	jsonOutputArrays = FALSE;
    else
	apiErrAbort(err400, err400Msg, "unrecognized 'jsonOutputArrays=%s' argument, can only be =1 or =0", jsonArray);
    }

int maybeDebug = cgiOptionalInt("debug", 0);
if (1 == maybeDebug)
    debug = TRUE;

char *measTime = cgiOptionalString("measureTiming");
if (isNotEmpty(measTime) && sameWord("1", measTime))
    measureTiming = TRUE;
char *maxOut = cgiOptionalString("maxItemsOutput");
if (isNotEmpty(maxOut))
    {
    long long n = -2;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
	n = sqlLongLong(maxOut);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	apiErrAbort(err400, err400Msg, "can not recognize maxItemsOutput '%s' as a number", maxOut);
    else
	{
	if (n == -1)	/* can use -1 to indicate as much as allowed */
	    maxItemsOutput = maxItemLimit;
	else if (n > maxItemLimit)	/* safety check */
	    apiErrAbort(err400, err400Msg, "requested maxItemsOutput '%s' greater than maximum limit allowed: %d", maxOut, maxItemLimit);
	else if (n < 1)
	    apiErrAbort(err400, err400Msg, "requested maxItemsOutput '%s' can not be less than one", maxOut, maxItemLimit);
	else
	    maxItemsOutput = n;
	}
    }
}	/*	static void setGlobalCgiVars()	*/

static void redirectToHelp()
/* redirect to the help page */
{
puts("Content-Type:text/html");
hPrintf("Status: %d %s\n", err301, err301Msg);
hPrintf("Location: /goldenPath/help/api.html\n");
puts("\n");

hPrintf("<!DOCTYPE HTML>\n");
hPrintf("<html lang='en'>\n");
hPrintf("<head>\n");
hPrintf("<meta http-equiv='Refresh' content='0; url=/goldenPath/help/api.html' />\n");
hPrintf("</head>\n");
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
static char *excludeVars[] = {"Submit", "submit", "sourceSelected", "selectRadio", "ucscGenome", "publicHubs", "clade", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
verboseTimeInit();
/* similar delay system as in DAS server */
botDelay = hgBotDelayTimeFrac(delayFraction);
if (botDelay > 0)
    {
    if (botDelay > 2000)
        {
	sleep1000(botDelay);
	hogExit();
        return 0;
        }
    sleep1000(botDelay);
    }

setGlobalCgiVars();

#ifdef NOTUSED // the argument processing doesn't allow udcTimeout
int timeout = cgiOptionalInt("udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
#endif
setUdcCacheDir();

initSupportedTypes();

char *pathInfo = getenv("PATH_INFO");
char *cmd;
if (isNotEmpty(pathInfo)) /* can get to this immediately, no cart needed */
    apiRequest(pathInfo);
else if ((cmd = cgiOptionalString("cmd")) != NULL)
    apiRequest(cmd);
else
    {
    char *allowApiHtml = cfgOptionDefault("hubApi.allowHtml", "off");
    if (sameWord("on", allowApiHtml))
	{
	trackCounter = hashNew(0);
	cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
	}
    else
	redirectToHelp();
    }
cgiExitTime("hubApi", enteredMainTime);
return 0;
}
