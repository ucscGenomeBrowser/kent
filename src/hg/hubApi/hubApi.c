/* hubApi - access mechanism to hub data resources. */
#include "dataApi.h"

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

/* Global Variables */
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
ret->descriptionUrl = cloneString(row[6]);
return ret;
}

struct hubPublic *hubPublicLoadAll()
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
    hPrintf("        <ul>\n");
    struct hashEl *hel;
    struct hashCookie hc = hashFirst(countTracks);
    while ((hel = hashNext(&hc)) != NULL)
        hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
    hPrintf("        </ul>\n");
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

static void trackSettings(struct trackDb *tdb, struct hash *countTracks)
/* process the settingsHash for a trackDb, recursive when subtracks */
{
hPrintf("    <ul>\n");
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
    hPrintf("   <li>has %d subtrack(s)</li>\n", slCount(tdb->subtracks));
    for (tdbEl = tdb->subtracks; tdbEl; tdbEl = tdbEl->next)
	{
        hPrintf("<li>subtrack: %s of parent: %s : type: '%s'</li>\n", tdbEl->track, tdbEl->parent->track, tdbEl->type);
	hashCountTrack(tdbEl, countTracks);
	trackSettings(tdbEl, countTracks);
	}
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

static void countOneTdb(struct trackDb *tdb, char *bigDataIndex,
    struct hash *countTracks)
{
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
// char *compositeTrack = trackDbSetting(tdb, "compositeTrack");
boolean compositeContainer = tdbIsComposite(tdb);
boolean compositeView = tdbIsCompositeView(tdb);
// char *superTrack = trackDbSetting(tdb, "superTrack");
boolean superChild = tdbIsSuperTrackChild(tdb);
boolean depthSearch = cartUsualBoolean(cart, "depthSearch", FALSE);
hashCountTrack(tdb, countTracks);

if (depthSearch && bigDataUrl)
    {
    long chromCount = 0;
    long itemCount = 0;
    struct dyString *errors = newDyString(1024);
    int retVal = bbiBriefMeasure(tdb->type, bigDataUrl, bigDataIndex, &chromCount, &itemCount, errors);
    if (retVal)
        {
            hPrintf("    <li>%s : %s : <font color='red'>ERROR: %s</font></li>\n", tdb->track, tdb->type, errors->string);
        }
    else
        {
        if (startsWithWord("bigBed", tdb->type))
            hPrintf("    <li>%s : %s : %ld chroms : %ld item count</li>\n", tdb->track, tdb->type, chromCount, itemCount);
        else if (startsWithWord("bigWig", tdb->type))
            hPrintf("    <li>%s : %s : %ld chroms : %ld bases covered</li>\n", tdb->track, tdb->type, chromCount, itemCount);
        else
            hPrintf("    <li>%s : %s : %ld chroms : %ld count</li>\n", tdb->track, tdb->type, chromCount, itemCount);
        }
    }
else
    {
    if (compositeContainer)
        hPrintf("    <li>%s : %s : composite track container</li>\n", tdb->track, tdb->type);
    else if (compositeView)
        hPrintf("    <li>%s : %s : composite view of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
    else if (superChild)
        hPrintf("    <li>%s : %s : superTrack child of parent: %s</li>\n", tdb->track, tdb->type, tdb->parent->track);
    else if (! depthSearch)
        hPrintf("    <li>%s : %s : %s</li>\n", tdb->track, tdb->type, bigDataUrl);
    else
        hPrintf("    <li>%s : %s</li>\n", tdb->track, tdb->type);
    }
if (allTrackSettings)
    {
    hPrintf("    <ul>\n");
    trackSettings(tdb, countTracks); /* show all settings */
    hPrintf("    </ul>\n");
    }
return;
}	/*	static void countOneTdb(struct trackDb *tdb,
	 *	    char *bigDataIndex, struct hash *countTracks)
	 */

static void hubTrackList(struct trackDb *topTrackDb, struct trackHubGenome *genome)
/* process the track list in a hub to show all tracks */
{
if (topTrackDb)
    {
    struct hash *countTracks = hashNew(0);
    hPrintf("    <ul>\n");
    struct trackDb *tdb = NULL;
    for ( tdb = topTrackDb; tdb; tdb = tdb->next )
	{
	char *bigDataIndex = NULL;
	char *relIdxUrl = trackDbSetting(topTrackDb, "bigDataIndex");
	if (relIdxUrl != NULL)
	    bigDataIndex = trackHubRelativeUrl(genome->trackDbFile, relIdxUrl);
	countOneTdb(tdb, bigDataIndex, countTracks);
	if (timeOutReached())
	    break;
	}	/*	for ( tdb = topTrackDb; tdb; tdb = tdb->next )	*/
    hPrintf("    <li>%d different track types</li>\n", countTracks->elCount);
    /* add this single genome count to the overall multi-genome counts */
    if (countTracks->elCount)
	{
        hPrintf("        <ol>\n");
	struct hashEl *hel, *helList = hashElListHash(countTracks);
	slSort(&helList, hashElCmpIntValDesc);
	for (hel = helList; hel; hel = hel->next)
	    {
            int prevCount = ptToInt(hashFindVal(trackCounter, hel->name));
	    if (differentStringNullOk("track count", hel->name))
		totalTracks += ptToInt(hel->val);
	    hashReplace(trackCounter, hel->name, intToPt(prevCount + ptToInt(hel->val)));
	    hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
	    }
        hPrintf("        </ol>\n");
	}
    hPrintf("    </ul>\n");
    }
else
    hPrintf("    <li>no trackTopDb</li>\n");
}	/*	static struct trackDb *hubTrackList()	*/

static struct trackDb *assemblySettings(struct trackHubGenome *genome)
/* display all the assembly 'settingsHash' */
{
struct trackDb *tdb = obtainTdb(genome, NULL);

hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(genome->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	{
	hubTrackList(tdb, genome);
        }
    if (timeOutReached())
	break;
    }
hPrintf("    </ul>\n");
return tdb;
}

struct slName *genomeList(struct trackHub *hubTop, struct trackDb **dbTrackList, char *selectGenome)
/* follow the pointers from the trackHub to trackHubGenome and around
 * in a circle from one to the other to find all hub resources
 * return slName list of the genomes in this track hub
 * optionally, return the trackList from this hub for the specified genome
 */
{
struct slName *retList = NULL;

long totalAssemblyCount = 0;
struct trackHubGenome *genome = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub</h4>\n");
hPrintf("<ul>\n");
long lastTime = clock1000();
for ( ; genome; genome = genome->next )
    {
    if (selectGenome)	/* is only one genome requested ?	*/
	{
	if ( differentStringNullOk(selectGenome, genome->name) )
	    continue;
	}
    ++totalAssemblyCount;
    struct slName *el = slNameNew(genome->name);
    slAddHead(&retList, el);
    if (genome->organism)
	{
	hPrintf("<li>%s - %s - %s</li>\n", genome->organism, genome->name, genome->description);
	}
    else
	{	/* can there be a description when organism is empty ? */
	hPrintf("<li>%s</li>\n", genome->name);
	}
    struct trackDb *tdb = assemblySettings(genome);
    if (dbTrackList)
	*dbTrackList = tdb;
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
    hPrintf("    <li>total genome assembly count: %ld</li>\n", totalAssemblyCount);
    hPrintf("    <li>%ld total tracks counted, %d different track types:</li>\n", totalTracks, trackCounter->elCount);
    hPrintf("    <ol>\n");
    struct hashEl *hel, *helList = hashElListHash(trackCounter);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	hPrintf("    <li>%d - %s - total</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("    </ol>\n");
    }
hPrintf("</ul>\n");
return retList;
}	/*	static struct slName *genomeList ()	*/

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

static void apiFunctionSwitch(char *pathInfo)
/* given a pathInfo string: /command/subCommand/etc...
 *  parse that and decide on which function to acll
 */
{
hPrintDisable();	/* turn off all normal HTML output, doing JSON output */

/* the leading slash has been removed from the pathInfo, therefore, the
 * chop will have the first word in words[0]
 */
char *words[MAX_PATH_INFO];/*expect no more than MAX_PATH_INFO number of words*/
int wordCount = chopByChar(pathInfo, '/', words, ArraySize(words));
if (wordCount < 2)
    apiErrAbort("unknown endpoint command: '/%s'", pathInfo);

struct hashEl *hel = hashLookup(apiFunctionHash, words[0]);
if (hel == NULL)
    apiErrAbort("no such command: '%s' for endpoint '/%s'", words[0], pathInfo);
void (*apiFunction)(char **) = hel->val;
// void (*apiFunction)(char **) = hashMustFindVal(apiFunctionHash, words[0]);

(*apiFunction)(words);

}	/*	static void apiFunctionSwitch(char *pathInfo)	*/

static void tracksForUcscDb(char *db)
/* scan the specified database for all tracks */
{
struct hash *countTracks = hashNew(0);
hPrintf("<p>Tracks in UCSC genome: '%s'<br>\n", db);
struct trackDb *tdbList = obtainTdb(NULL, db);
struct trackDb *tdb;
hPrintf("<ul>\n");
for (tdb = tdbList; tdb != NULL; tdb = tdb->next )
    {
    countOneTdb(tdb, NULL, countTracks);
    if (timeOutReached())
	break;
    }
int trackCount = ptToInt(hashFindVal(countTracks, "track count"));
hPrintf("    <li>%d total tracks counted, %d different track types</li>\n", trackCount, countTracks->elCount);
if (countTracks->elCount)
    {
    hPrintf("        <ol>\n");
    struct hashEl *hel, *helList = hashElListHash(countTracks);
    slSort(&helList, hashElCmpIntValDesc);
    for (hel = helList; hel; hel = hel->next)
	{
	hPrintf("        <li>%d - %s</li>\n", ptToInt(hel->val), hel->name);
	}
    hPrintf("        </ol>\n");
    }
hPrintf("</ul>\n");
hPrintf("</p>\n");
}	// static void tracksForUcscDb(char * db)

static void showExamples(char *url, struct trackHubGenome *hubGenome, char *ucscDb)
{

hPrintf("<h2>Example URLs to return json data structures:</h2>\n");
hPrintf("<ol>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/list/publicHubs' target=_blank>list public hubs</a> <em>/cgi-bin/hubApi/list/publicHubs</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/list/ucscGenomes' target=_blank>list database genomes</a> <em>/cgi-bin/hubApi/list/ucscGenomes</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/list/hubGenomes?hubUrl=%s' target=_blank>list genomes from specified hub</a> <em>/cgi-bin/hubApi/list/hubGenomes?hubUrl=%s</em></li>\n", url, url);
hPrintf("<li><a href='/cgi-bin/hubApi/list/tracks?hubUrl=%s&hubUrl=%s&genome=%s' target=_blank>list tracks from specified hub and genome</a> <em>/cgi-bin/hubApi/list/tracks?hubUrl=%s&genome=%s</em></li>\n", url, url, hubGenome->name, url, hubGenome->name);
hPrintf("<li><a href='/cgi-bin/hubApi/list/tracks?db=%s' target=_blank>list tracks from specified UCSC database</a> <em>/cgi-bin/hubApi/list/tracks?db=%s</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/list/chromosomes?db=%s' target=_blank>list chromosomes from specified UCSC database</a> <em>/cgi-bin/hubApi/list/chromosomes?db=%s</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/list/chromosomes?db=%s&track=gap' target=_blank>list chromosomes from specified track from UCSC databaset</a> <em>/cgi-bin/hubApi/list/chromosomes?db=%s&track=gap</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/sequence?db=%s&chrom=chrM' target=_blank>get sequence from specified database and chromosome</a> <em>/cgi-bin/hubApi/getData/sequence?db=%s&chrom=chrM</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/sequence?db=%s&chrom=chrM&start=0&end=128' target=_blank>get sequence from specified database, chromosome with start,end coordinates</a> <em>/cgi-bin/hubApi/getData/sequence?db=%s&chrom=chrM&start=0&end=128</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?db=%s&track=gold' target=_blank>get entire track data from specified database and track name (gold == Assembly)</a> <em>/cgi-bin/hubApi/getData/track?db=%s&track=gold</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?db=%s&chrom=chrM&track=gold' target=_blank>get track data from specified database, chromosome and track name (gold == Assembly)</a> <em>/cgi-bin/hubApi/getData/track?db=%s&chrom=chrM&track=gold</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?db=%s&chrom=chrI&track=gold&start=107680&end=186148' target=_blank>get track data from specified database, chromosome, track name, start and end coordinates</a> <em>/cgi-bin/hubApi/getData/track?db=%s&chrom=chr1&track=gold&start=107680&end=186148</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt&genome=hg19&track=ultraConserved' target=_blank>get entire track data from specified hub and track name</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt&genome=hg19&track=ultraConserved</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrCp&track=assembly_' target=_blank>get track data from specified hub, chromosome and track name (full chromosome)</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrCp&track=assembly_</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chr1&track=assembly_&start=0&end=14309681' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chr1&track=assembly_&start=0&end=14309681</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&track=gc5Base_' target=_blank>get all track data from specified hub and track name</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&track=gc5Base</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrMt&track=gc5Base_&start=143600&end=143685' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrMt&track=gc5Base&start=143600&end=143685</em></li>\n");
hPrintf("</ol>\n");

hPrintf("<h2>Example URLs to generate errors:</h2>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrI&track=assembly_&start=0&end=14309681' target=_blank>get track data from specified hub, chromosome, track name, start and end coordinates</a> <em>/cgi-bin/hubApi/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt&genome=_araTha1&chrom=chrI&track=assembly_&start=0&end=14309681</em></li>\n");
hPrintf("<ol>\n");
hPrintf("</ol>\n");
}	/*	static void showExamples()	*/

static void showCartDump()
/* for information purposes only during development, will become obsolete */
{
hPrintf("<h4>cart dump</h4>");
hPrintf("<pre>\n");
cartDump(cart);
hPrintf("</pre>\n");
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

char *docRoot = cfgOptionDefault("browser.documentRoot", DOCUMENT_ROOT);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *pathInfo = getenv("PATH_INFO");

if (isNotEmpty(pathInfo))
    {
    puts("Content-Type:application/json");
    puts("\n");
    /* skip the first leading slash to simplify chopByChar parsing */
    pathInfo += 1;
    setupFunctionHash();
    apiFunctionSwitch(pathInfo);
    return;
    }
puts("Content-Type:text/html");
puts("\n");

(void) hubPublicLoadAll();

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

cartWebStart(cart, database, "access mechanism to hub data resources");

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

long lastTime = clock1000();
struct trackHub *hub = errCatchTrackHubOpen(urlInput);
if (measureTiming)
    {
    long thisTime = clock1000();
    hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

// hPrintf("<h3>ucscDb: '%s'</h2>\n", ucscDb);

struct trackHubGenome *hubGenome = hub->genomeList;

showExamples(urlInput, hubGenome, ucscDb);

showCartDump();

hPrintf("<form action='%s' name='hubApiUrl' id='hubApiUrl' method='GET'>\n\n", "../cgi-bin/hubApi");

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
hPrintf("<input type='text' name='urlHub' id='urlHub' size='60' value='%s'>\n", urlInput);
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

if (sameWord("go", goUcscDb))	/* requested UCSC db track list */
    {
    tracksForUcscDb(ucscDb);
    }
else
    {
    hPrintf("<p>URL: %s - %s<br>\n", urlInput, sameWord("go",goPublicHub) ? "public hub" : "other hub");
    hPrintf("name: %s<br>\n", hub->shortLabel);
    hPrintf("description: %s<br>\n", hub->longLabel);
    hPrintf("default db: '%s'<br>\n", isEmpty(hub->defaultDb) ? "(none available)" : hub->defaultDb);
    printf("docRoot:'%s'<br>\n", docRoot);

    if (hub->genomeList)
	(void) genomeList(hub, NULL, NULL);	/* ignore returned list */
    hPrintf("</p>\n");
    }


if (timedOut)
    hPrintf("<h1>Reached time out %ld seconds</h1>", timeOutSeconds);
if (measureTiming)
    hPrintf("<em>Overall total time: %ld millis</em><br>\n", clock1000() - enteredMainTime);

cartWebEnd();
}	/*	void doMiddle(struct cart *theCart)	*/

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
static char *excludeVars[] = {"Submit", "submit", NULL,};

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
