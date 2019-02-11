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
#include "jsonParse.h"
#include "jsonWrite.h"
#include "chromInfo.h"

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
static struct cart *cart;             /* CGI and other variables */
static struct hash *oldVars = NULL;
static struct hash *trackCounter = NULL;
static long totalTracks = 0;
static boolean measureTiming = FALSE;	/* set by CGI parameters */
static boolean allTrackSettings = FALSE;	/* checkbox setting */
static char **shortLabels = NULL;	/* public hub short labels in array */
struct hubPublic *publicHubList = NULL;
static int publicHubCount = 0;
static char *defaultHub = "Plants";
static char *defaultDb = "ce11";
static long enteredMainTime = 0;	/* will become = clock1000() on entry */
		/* to allow calculation of when to bail out, taking too long */
static long timeOutSeconds = 100;
static boolean timedOut = FALSE;

/* ######################################################################### */
static struct jsonWrite *jsonStartOutput()
/* begin json output */
{
time_t timeNow = time(NULL);
// struct tm tm;
// gmtime_r(&timeNow, &tm);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "apiVersion", "0.1");
jsonWriteString(jw, "source", "UCSantaCruz");
jsonWriteDateFromUnix(jw, "downloadTime", (long long) timeNow);
jsonWriteNumber(jw, "downloadTimeStamp", (long long) timeNow);
return jw;
}

static void jsonErrAbort(char *format, ...)
/* Issue an error message in json format. */
{
char errMsg[2048];
va_list args;
va_start(args, format);
vsnprintf(errMsg, sizeof(errMsg), format, args);
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "error", errMsg);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
exit(0);
}

static void hubPublicJsonData(struct jsonWrite *jw, struct hubPublic *el)
/* Print array data for one row from hubPublic table, order here
 * must be same as was stated in the columnName header element
 *  TODO: need to figure out how to use the order of the columns as
 *        they are in the 'desc' request
 */
{
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, el->hubUrl);
jsonWriteString(jw, NULL, el->shortLabel);
jsonWriteString(jw, NULL, el->longLabel);
jsonWriteString(jw, NULL, el->registrationTime);
jsonWriteNumber(jw, NULL, (long long)el->dbCount);
jsonWriteString(jw, NULL, el->dbList);
jsonWriteString(jw, NULL, el->descriptionUrl);
jsonWriteListEnd(jw);
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

static struct slName *trackList(struct trackDb *tdb, struct trackHubGenome *genome)
/* process the track list to show all tracks */
{
struct slName *retList = NULL;	/* for return of track list for 'genome' */
if (tdb)
    {
    struct hash *countTracks = hashNew(0);
    hPrintf("    <ul>\n");
    struct trackDb *track = tdb;
    for ( ; track; track = track->next )
	{
        struct slName *el = slNameNew(track->track);
        slAddHead(&retList, el);
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
return retList;
}	/*	static struct slName *trackList()	*/

static struct slName *assemblySettings(struct trackHubGenome *genome)
/* display all the assembly 'settingsHash' */
{
struct slName *retList = NULL;
hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(genome->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))	/* examine the trackDb structure */
	{
        struct trackDb *tdb = trackHubTracksForGenome(genome->trackHub, genome);
	retList = trackList(tdb, genome);
        }
    if (timeOutReached())
	break;
    }
hPrintf("    </ul>\n");
return retList;
}

static struct slName *genomeList(struct trackHub *hubTop, struct slName **dbTrackList, char *selectGenome)
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
    if (genome->settingsHash)
	{
	struct slName *trackList = assemblySettings(genome);
        if (dbTrackList)
	    *dbTrackList = trackList;
        }
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

static void dbDbJsonData(struct jsonWrite *jw, struct dbDb *el)
/* Print out dbDb table element in JSON format.
 * must be same as was stated in the columnName header element
 *  TODO: need to figure out how to use the order of the columns as
 *        they are in the 'desc' request
 */
{
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, el->name);
jsonWriteString(jw, NULL, el->description);
jsonWriteString(jw, NULL, el->nibPath);
jsonWriteString(jw, NULL, el->organism);
jsonWriteString(jw, NULL, el->defaultPos);
jsonWriteNumber(jw, NULL, (long long)el->active);
jsonWriteNumber(jw, NULL, (long long)el->orderKey);
jsonWriteString(jw, NULL, el->genome);
jsonWriteString(jw, NULL, el->scientificName);
jsonWriteString(jw, NULL, el->htmlPath);
jsonWriteNumber(jw, NULL, (long long)el->hgNearOk);
jsonWriteNumber(jw, NULL, (long long)el->hgPbOk);
jsonWriteString(jw, NULL, el->sourceName);
jsonWriteNumber(jw, NULL, (long long)el->taxId);
jsonWriteListEnd(jw);
}

static boolean tableColumns(struct jsonWrite *jw, char *table)
/* output the column names for the given table
 * return: TRUE on error, FALSE on success
 */
{
jsonWriteListStart(jw, "columnNames");
char query[1024];
struct sqlConnection *conn = hConnectCentral();
sqlSafef(query, sizeof(query), "desc %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
row = sqlNextRow(sr);
if (NULL == row)
    {
    jsonErrAbort("ERROR: can not 'desc' table '%s'", table);
    return TRUE;
    }
while ((row = sqlNextRow(sr)) != NULL)
    jsonWriteString(jw, NULL, row[0]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
jsonWriteListEnd(jw);
return FALSE;
}

static void jsonPublicHubs()
/* output the hubPublic SQL table */
{
struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, hubPublicTableName());
hDisconnectCentral(&conn);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');
struct hubPublic *el = publicHubList;
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
jsonWriteString(jw, "tableName", hubPublicTableName());
tableColumns(jw, hubPublicTableName());
jsonWriteListStart(jw, "publicHubData");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}

static int dbDbCmpName(const void *va, const void *vb)
/* Compare two dbDb elements: name, ignore case. */
{
const struct dbDb *a = *((struct dbDb **)va);
const struct dbDb *b = *((struct dbDb **)vb);
return strcasecmp(a->name, b->name);
}

static struct dbDb *ucscDbDb()
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

static void jsonDbDb()
/* output the dbDb SQL table */
{
struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, "dbDb");
hDisconnectCentral(&conn);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');
struct dbDb *dbList = ucscDbDb();
struct dbDb *el;
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
jsonWriteString(jw, "tableName", "dbDb");
tableColumns(jw, "dbDb");
jsonWriteListStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}

static void chromInfoJsonOutput(FILE *f, char *db)
/* for given db, if there is a track, list the chromosomes in that track,
 * for no track, simply list the chromosomes in the sequence
 */
{
char *table = cgiOptionalString("track");
struct sqlConnection *conn = hAllocConn(db);
/* in trackDb language: track == table */
if (table)
    {
    if (! sqlTableExists(conn, table))
	jsonErrAbort("ERROR: endpoint: /list/chromosomes?db=%&table=%s ERROR table does not exist", db, table);
    if (sqlColumnExists(conn, table, "chrom"))
	{
	char *dataTime = sqlTableUpdate(conn, table);
	time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
	replaceChar(dataTime, ' ', 'T');
        struct jsonWrite *jw = jsonStartOutput();
	jsonWriteString(jw, "genome", db);
	jsonWriteString(jw, "track", table);
	jsonWriteString(jw, "dataTime", dataTime);
	jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
	freeMem(dataTime);
        struct slPair *list = NULL;
	char query[2048];
        sqlSafef(query, sizeof(query), "select distinct chrom from %s", table);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
    	{
            int size = hChromSize(db, row[0]);
	    slAddHead(&list, slPairNew(row[0], intToPt(size)));
    	}
	sqlFreeResult(&sr);
        slPairIntSort(&list);
        slReverse(&list);
        jsonWriteNumber(jw, "chromCount", (long long)slCount(list));
	jsonWriteObjectStart(jw, "chromosomes");
        struct slPair *el = list;
        for ( ; el != NULL; el = el->next )
            jsonWriteNumber(jw, el->name, (long long)ptToInt(el->val));
	jsonWriteObjectEnd(jw);	/* chromosomes */
        jsonWriteObjectEnd(jw);	/* top level */
        fputs(jw->dy->string,stdout);
	}
    else
	{
	jsonErrAbort("ERROR: table '%s' is not a position table, no chromosomes for genome: '%s'", table, db);
	}
    }
else
    {
    char *dataTime = sqlTableUpdate(conn, "chromInfo");
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');
    struct chromInfo *ciList = createChromInfoList(NULL, db);
    struct chromInfo *el = ciList;
    struct jsonWrite *jw = jsonStartOutput();
    jsonWriteString(jw, "genome", db);
    jsonWriteString(jw, "dataTime", dataTime);
    jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
    freeMem(dataTime);
    jsonWriteNumber(jw, "chromCount", (long long)slCount(ciList));
    jsonWriteObjectStart(jw, "chromosomes");
    for ( ; el != NULL; el = el->next )
	{
        jsonWriteNumber(jw, el->chrom, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    jsonWriteObjectEnd(jw);	/* top level */
    fputs(jw->dy->string,stdout);
    }
hFreeConn(&conn);
}

static void trackDbJsonOutput(char *db, FILE *f)
/* return track list from specified UCSC database name */
{
struct sqlConnection *conn = hAllocConn(db);
char *dataTime = sqlTableUpdate(conn, "trackDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');
hFreeConn(&conn);
struct trackDb *tdbList = hTrackDb(db);
struct trackDb *el;
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "db", db);
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
jsonWriteListStart(jw, "tracks");
for (el = tdbList; el != NULL; el = el->next )
    jsonWriteString(jw, NULL, el->track);
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}	/*	static void trackDbJsonOutput(char *db, FILE *f)	*/

#define MAX_PATH_INFO 32
static void apiList(char *words[MAX_PATH_INFO])
/* 'list' function words[1] is the subCommand */
{
if (sameWord("publicHubs", words[1]))
    jsonPublicHubs();
else if (sameWord("ucscGenomes", words[1]))
    jsonDbDb();
else if (sameWord("hubGenomes", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
    if (isEmpty(hubUrl))
	jsonErrAbort("ERROR: must supply hubUrl='http:...' some URL to a hub for /list/genomes");

    struct trackHub *hub = trackHubOpen(hubUrl, "");
    if (hub->genomeList)
	{
        struct jsonWrite *jw = jsonStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
        jsonWriteListStart(jw, "genomes");
	struct slName *theList = genomeList(hub, NULL, NULL);
	slNameSort(&theList);
	struct slName *el = theList;
	for ( ; el ; el = el->next )
	    {
	    jsonWriteString(jw, NULL, el->name);
	    }
	jsonWriteListEnd(jw);
	jsonWriteObjectEnd(jw);
        fputs(jw->dy->string,stdout);
	}
    }
else if (sameWord("tracks", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
      jsonErrAbort("ERROR: must supply hubUrl or db name to return track list");
    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        trackDbJsonOutput(db, stdout);	// only need db for this function
	return;
	}
    if (isEmpty(genome) || isEmpty(hubUrl))
	{
        if (isEmpty(genome))
	    warn("# must supply genome='someName' the name of a genome in a hub for /list/tracks\n");
	if (isEmpty(hubUrl))
            jsonErrAbort("ERROR: must supply hubUrl='http:...' some URL to a hub for /list/genomes");
	}
    struct trackHub *hub = trackHubOpen(hubUrl, "");
    if (hub->genomeList)
	{
	struct slName *dbTrackList = NULL;
	(void) genomeList(hub, &dbTrackList, genome);
        struct jsonWrite *jw = jsonStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
        jsonWriteListStart(jw, "genome");
	slNameSort(&dbTrackList);
	struct slName *el = dbTrackList;
	for ( ; el != NULL; el = el->next )
	    {
            jsonWriteString(jw, NULL, el->name);
	    }
	jsonWriteListEnd(jw);
	jsonWriteObjectEnd(jw);
        fputs(jw->dy->string,stdout);
	}
    }
else if (sameWord("chromosomes", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
//    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
        jsonErrAbort("ERROR: must supply hubUrl or db name to return chromosome list");

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        chromInfoJsonOutput(stdout, db);
	return;
	}
    }
else
    jsonErrAbort("do not recognize endpoint: '/list/%s' request", words[1]);
}

static struct hash *apiFunctionHash = NULL;

static void setupFunctionHash()
/* initialize the apiFunctionHash */
{
if (apiFunctionHash)
    return;

apiFunctionHash = hashNew(0);
hashAdd(apiFunctionHash, "list", &apiList);
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
    jsonErrAbort("unknown endpoint command: '/%s'", pathInfo);

struct hashEl *hel = hashLookup(apiFunctionHash, words[0]);
if (hel == NULL)
    jsonErrAbort("no such command: '%s' for endpoint '/%s'", words[0], pathInfo);
void (*apiFunction)(char **) = hel->val;
// void (*apiFunction)(char **) = hashMustFindVal(apiFunctionHash, words[0]);

(*apiFunction)(words);

}	/*	static void apiFunctionSwitch(char *pathInfo)	*/

static void tracksForUcscDb(char * ucscDb)
{
hPrintf("<p>Tracks in UCSC genome: '%s'<br>\n", ucscDb);
struct trackDb *tdbList = hTrackDb(ucscDb);
struct trackDb *track;
hPrintf("<ul>\n");
for (track = tdbList; track != NULL; track = track->next )
    {
    hPrintf("<li>%s</li>\n", track->track);
    if (allTrackSettings)
        trackSettings(track); /* show all settings */
    }
hPrintf("</ul>\n");
hPrintf("</p>\n");
}

static void doMiddle(struct cart *theCart)
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

if (isNotEmpty(pathInfo))
    {
    /* skip the first leading slash to simplify chopByChar parsing */
    pathInfo += 1;
    setupFunctionHash();
    apiFunctionSwitch(pathInfo);
    return;
    }

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
struct trackHub *hub = trackHubOpen(urlInput, "");
if (measureTiming)
    {
    long thisTime = clock1000();
    hPrintf("<em>hub open time: %ld millis</em><br>\n", thisTime - lastTime);
    }

hPrintf("<h3>ucscDb: '%s'</h2>\n", ucscDb);

struct trackHubGenome *hubGenome = hub->genomeList;

hPrintf("<h2>Example URLs to return json data structures:</h2>\n");
hPrintf("<ul>\n");
hPrintf("<li><a href='https://jsonlint.com/?json=https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/publicHubs' target=_blank>list public hubs</a> <em>/cgi-bin/hubApi/list/publicHubs</em></li>\n");
hPrintf("<li><a href='https://jsonlint.com/?json=https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/ucscGenomes' target=_blank>list database genomes</a> <em>/cgi-bin/hubApi/list/ucscGenomes</em></li>\n");
hPrintf("<li><a href='/cgi-bin/hubApi/list/hubGenomes?hubUrl=%s' target=_blank>list genomes from specified hub</a> <em>/cgi-bin/hubApi/list/hubGenomes?hubUrl='%s'</em></li>\n", urlInput, urlInput);
hPrintf("<li><a href='/cgi-bin/hubApi/list/tracks?hubUrl=%s&genome=%s' target=_blank>list tracks from specified hub and genome</a> <em>/cgi-bin/hubApi/list/tracks?hubUrl='%s&genome=%s'</em></li>\n", urlInput, hubGenome->name, urlInput, hubGenome->name);
hPrintf("<li><a href='/cgi-bin/hubApi/list/tracks?db=%s' target=_blank>list tracks from specified UCSC database</a> <em>/cgi-bin/hubApi/list/tracks?db='%s'</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/list/chromosomes?db=%s' target=_blank>list chromosomes from specified UCSC database</a> <em>/cgi-bin/hubApi/list/chromosomes?db='%s'</em></li>\n", ucscDb, ucscDb);
hPrintf("<li><a href='/cgi-bin/hubApi/list/chromosomes?db=%s&track=gap' target=_blank>list chromosomes from specified track from UCSC databaset</a> <em>/cgi-bin/hubApi/list/chromosomes?db='%s'&track=gap</em></li>\n", ucscDb, ucscDb);
hPrintf("</ul>\n");

hPrintf("<h4>cart dump</h4>");
hPrintf("<pre>\n");
cartDump(cart);
hPrintf("</pre>\n");

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
