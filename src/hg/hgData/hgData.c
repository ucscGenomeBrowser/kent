/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"

#include "wiggle.h"


static char const rcsid[] = "$Id: hgData.c,v 1.1.2.22 2009/03/08 10:02:34 mikep Exp $";

#define MAX_WIG_LINES 500000

struct wiggleDataStream *wigOutRegion(char *genome, char *track, char *chrom, int start, int end, int maxOut, 
    int operations, int *count)
// operations: wigFetchNoOp || wigFetchStats || wigFetchRawStats || wigFetchBed || wigFetchDataArray || wigFetchAscii
//     doAscii = operations & wigFetchAscii;
//     doDataArray = operations & wigFetchDataArray;
//     doBed = operations & wigFetchBed;
//     doRawStats = operations & wigFetchRawStats;
//     doStats = (operations & wigFetchStats) || doRawStats;
//     doNoOp = operations & wigFetchNoOp;
{
int n;
boolean hasBin = FALSE;
/*struct bed *intersectBedList = NULL;
char *table2 = NULL;*/
char splitTableOrFileName[256];
struct wiggleDataStream *wds = wiggleDataStreamNew();
wds->setMaxOutput(wds, maxOut);
wds->setChromConstraint(wds, chrom);
wds->setPositionConstraint(wds, start, end);
if (!hFindSplitTable(genome, chrom, track, splitTableOrFileName, &hasBin))
    ERR_TRACK_NOT_FOUND(track, genome);
n = wds->getData(wds, genome, splitTableOrFileName, operations);
if (count)
    *count = n;
return wds;
}

//struct asciiDatum
/* a single instance of a wiggle data value (trying float here to save space */
//    unsigned chromStart;    /* Start position in chromosome, 0 relative */
//    float value;

static struct json_object *jsonAddWigAsciiData(struct json_object *o, int count, struct asciiDatum *data)
{
int i;
struct json_object *a = json_object_new_array();
if (count>0)
    json_object_object_add(o, "chrom_start", json_object_new_int(data[0].chromStart));
json_object_object_add(o, "ascii_data", a);
for ( i = 0; i < count; ++i)
    {
/*    struct json_object *d = json_object_new_array();
    json_object_array_add(a, d);*/
    // add one asciiDatum: (chromStart,value) pair
    json_object_array_add(a, json_object_new_double(data[i].value));
    }
return o;
}

//struct wigAsciiData
/* linked list of wiggle data in ascii form */
//    struct wigAsciiData *next;  /*      next in singly linked list      */
//    char *chrom;                /*      chrom name for this set of data */
//    unsigned span;              /*      span for this set of data       */
//    unsigned count;             /*      number of values in this block */
//    double dataRange;           /*      for resolution calculation */
//    struct asciiDatum *data;    /*      individual data items here */

static struct json_object *jsonWigAscii(int count, struct wiggleDataStream *wds, struct wigAsciiData *ascii)
{
struct json_object *w = json_object_new_object();
struct json_object *a = json_object_new_array();
json_object_object_add(w, "count", json_object_new_int(count));

json_object_object_add(w, "ascii_count", json_object_new_int(slCount(wds->ascii)));
json_object_object_add(w, "bed_count", json_object_new_int(slCount(wds->bed)));
json_object_object_add(w, "stats_count", json_object_new_int(slCount(wds->stats)));
json_object_object_add(w, "array_count", json_object_new_int(slCount(wds->array)));
json_object_object_add(w, "rowsRead", json_object_new_int(wds->rowsRead));
json_object_object_add(w, "validPoints", json_object_new_int(wds->validPoints));
json_object_object_add(w, "noDataPoints", json_object_new_int(wds->noDataPoints));
json_object_object_add(w, "bytesRead", json_object_new_int(wds->bytesRead));
json_object_object_add(w, "bytesSkipped", json_object_new_int(wds->bytesSkipped));
json_object_object_add(w, "valuesMatched", json_object_new_int(wds->valuesMatched));
json_object_object_add(w, "totalBedElements", json_object_new_int(wds->totalBedElements));
json_object_object_add(w, "db", wds->db ? json_object_new_string(wds->db) : NULL);
json_object_object_add(w, "tblName", wds->tblName ? json_object_new_string(wds->tblName) : NULL);
json_object_object_add(w, "wibFile", wds->wibFile ? json_object_new_string(wds->wibFile) : NULL);
json_object_object_add(w, "ascii", wds->ascii ? json_object_new_int((int)((void *)wds->ascii-NULL)) : NULL);
json_object_object_add(w, "bed", wds->bed ? json_object_new_int((int)((void *)wds->bed-NULL)) : NULL);
json_object_object_add(w, "stats", wds->stats ? json_object_new_int((int)((void *)wds->stats-NULL)) : NULL);
json_object_object_add(w, "array", wds->array ? json_object_new_int((int)((void *)wds->array-NULL)) : NULL);

json_object_object_add(w, "data", a);
for ( ; ascii ; ascii = ascii->next)
    {
    struct json_object *d = json_object_new_object();
    json_object_array_add(a, d);
    json_object_object_add(d, "chrom", json_object_new_string(ascii->chrom));
    json_object_object_add(d, "span", json_object_new_int(ascii->span));
    json_object_object_add(d, "count", json_object_new_int(ascii->count));
    json_object_object_add(d, "data_range", json_object_new_double(ascii->dataRange));
    jsonAddWigAsciiData(d, ascii->count, ascii->data);
    }
return w;
}
//     /*  data return structures  */
//     struct wigAsciiData *ascii; /*      list of wiggle data values */
//     struct bed *bed;            /*      data in bed format      */
//     struct wiggleStats *stats;  /*      list of wiggle stats    */
//     struct wiggleArray *array;  /*      one big in-memory array of data */


void printWigCount(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand)
{
int count;
struct wiggleDataStream *wds = wigOutRegion(genome, track, chrom, start, end, MAX_WIG_LINES, wigFetchNoOp, &count);
wiggleDataStreamFree(&wds);
}

void printWig(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand)
// print wig records which intersect this start-end range
{
time_t modified = 0;
int count;
// options: wigFetchStats || wigFetchRawStats || wigFetchBed || wigFetchDataArray || wigFetchAscii
struct wiggleDataStream *wds = wigOutRegion(genome, track, chrom, start, end, MAX_WIG_LINES, wigFetchDataArray|wigFetchAscii|wigFetchBed, &count);
// ascii: asciiData->count
// bed:   slCount(wds->bed)
struct json_object *w = jsonWigAscii(count, wds, wds->ascii);
wiggleDataStreamFree(&wds);
okSendHeader(modified, TRACK_EXPIRES);
printf(json_object_to_json_string(w));
json_object_put(w);
}

void doGet()
{
// these optional cgi strings are set to NULL if not present
struct tm *thisTime, *latestTime;
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
struct hgPositions *hgp = NULL;
//struct sqlConnection *conn = NULL;
//char *authorization = getenv("Authorization");
//verboseSetLevel(2);
char *reqEtag = getenv("ETag");
time_t reqModified = strToTime(getenv("Modified"), "%a, %d %b %Y %H:%M:%S GMT");// Thu, 08 Jan 2009 17:45:18 GMT
char *cmd = cgiUsualString(CMD_ARG, "");
char *format = cgiUsualString(FORMAT_ARG, "");
char *genome = cgiUsualString(GENOME_ARG, "");
char *track = cgiUsualString(TRACK_ARG, "");
char *term = cgiUsualString(TERM_ARG, "");
char *strand = cgiUsualString(STRAND_ARG, "+");
if (!*strand)
    strand = "+";
char *chrom = cgiUsualString(CHROM_ARG, "");
int start = stripToInt(cgiUsualString(START_ARG, NULL)); // MJP FIXME: should do redirect to the new url if start matched [,.]
int end = stripToInt(cgiUsualString(END_ARG, NULL)); // MJP FIXME: should do redirect to the new url if end matched [,.]
char rootName[HDB_MAX_TABLE_STRING];
char parsedChrom[HDB_MAX_CHROM_STRING];
char *trackType[4] = {NULL,NULL,NULL,NULL};
int typeWords;
time_t modified = 0, genomeMod = 0, trackMod = 0;
AllocVar(thisTime);
AllocVar(latestTime);
// list information about all active genome databases
MJP(2); verbose(2,"cmd=[%s] genome=[%s] track=[%s] pos=[%s:%c:%d-%d]\n", cmd, genome, track, chrom, *strand, start, end);
if (*genome)
    {
    genomeMod = validateGenome(genome);
    if (*track)
	trackMod = validateLoadTrackDb(genome, track, &tdb);
    if (*chrom)
	validateChrom(genome, chrom, &ci);
    }
if (!*cmd)
    {
    // get modified time from CVS $Date field in file where code resides
    printUsage(reqEtag, reqModified);
    }
else if (sameOk(GENOMES_CMD, cmd)) 
    {
    // no genome -> get all genomes
    // genome, no chrom => get genome & all chroms
    // genome, chrom => get genome & one chrom
    // check for changes in dbDb, genomeClade, clade
    modified = hGetLatestUpdateTimeDbClade();
    if (*genome)
	{
	modified = max(modified, genomeMod);
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (!(dbs = hGetIndexedDbClade(genome)))
		errAbort("Db and clade not found for genome [%s]\n", genome);
	    if (!*chrom)
		{
		if (!(ci = getAllChromInfo(genome)))
		    ERR_NO_CHROMS_FOUND(genome);
		// TEST FOR number of chroms 
		// if (hChromCount(genome) < MAX_CHROM_COUNT || cgiBoolean(ALLCHROMS_ARG)
		}
	    printGenomes(genome, chrom, dbs, ci, modified);
	    }
	}
    else
	{
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (!(dbs = hGetIndexedDbClade(NULL)))
		ERR_NO_GENOMES_FOUND;
	    printGenomes(genome, chrom, dbs, ci, modified);
	    }
	}
    }
else if (sameOk(TRACKS_CMD, cmd))
    {
    if (*genome)
	{
	// validate genome & track
	// modified date is greatest of all trackDb dates or genome.chromInfo 
	modified = max(genomeMod, trackDbLatestUpdateTime(genome));
	if (*track)
	    modified = max(modified, trackMod);
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (*track)
		{
		hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
		if (!(hti = hFindTableInfo(genome, chrom, rootName)))
		    ERR_TABLE_INFO_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
		}
	    else 
		{
		if ( !(tdb = hTrackDb(genome, NULL)) )
		    ERR_TRACK_INFO_NOT_FOUND("<any>", genome);
		}
	    printTrackInfo(genome, track, tdb, hti, modified);
	    }
	}
    else
	{
        ERR_NOT_IMPLEMENTED("List of all tracks in all genomes");
	}
    }
else if (sameOk(SEARCH_CMD, cmd))
    {
    if (*genome)
	{
	// If no search term, send empty result
	if (!*term)
	    searchTracks(0, NULL, NULL, NULL, NULL);
	// if a track was specified just search that
	if (*track)
	    { // check if genome, track, or spec table(s) have changed
	    modified = max(genomeMod, trackMod);
	    modified = max(modified, findSpecLatestUpdateTime(genome));
	    if (!notModifiedResponse(reqEtag, reqModified, modified))
		{
		char where[512];
		safef(where, sizeof(where), "searchTable = '%s'", track);
		hgp = hgPositionsFindWhere(genome, term, "dummyCgi", "dummyApp", NULL, TRUE, where);
		//hgp = hgPositionsFindWhere(genome, term, NULL, NULL, NULL, TRUE, where);
		searchTracks(modified, hgp, genome, track, term);
		}
	    }
	else // we are searching through many tracks, so dont use cache (dont set last-modified)
	    {
	    hgp = hgPositionsFind(genome, term, "dummyCgi", "dummyApp", NULL, TRUE);
	    //hgp = hgPositionsFind(genome, term, NULL, NULL, NULL, TRUE);
	    searchTracks(0, hgp, genome, track, term);
	    }
	}
    else
	ERR_NOT_IMPLEMENTED("Search over all genomes");
    }
else if (sameOk(META_SEARCH_CMD, cmd))
    {
    }
else if (sameOk(COUNT_CMD, cmd) || sameOk(RANGE_CMD, cmd))
    {
    if (!*genome)
	ERR_NO_GENOME;
    if (!*track)
	ERR_NO_TRACK;
    if (*format && 
	  (differentString(format, ANNOJ_FLAT_FMT) && 
	   differentString(format, ANNOJ_NESTED_FMT)))
	ERR_BAD_FORMAT(format);
    // check genome.chromInfo, the track, or the trackDbs for changes
    modified = max(genomeMod, trackMod);
    modified = max(modified, trackDbLatestUpdateTime(genome));
    if (!notModifiedResponse(reqEtag, reqModified, modified))
	{
	hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
	if (!*chrom)
	    ERR_NO_CHROM;
	if (!(ci = hGetChromInfo(genome, chrom)))
	    ERR_CHROM_NOT_FOUND(genome, chrom);
	if (end<start)
	    ERR_START_AFTER_END(start, end);
	if (start<0) // MJP FIXME: should do redirect to the new url with start d set to 0
	    start = 0;
	if (end<0) // MJP FIXME: should do redirect to the new url with end set to 0
	    end = 0;
	else if (end==0)
	    end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
	else if (end > ci->size) // MJP FIXME: should do redirect to the new url with end set to chromSize
	    end = ci->size;

	if (!(hti = hFindTableInfo(genome, chrom, rootName)))
	    ERR_TABLE_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
	// test how many rows we are about to receive
	int n = hGetBedRangeCount(genome, tdb->tableName, chrom, start, end, NULL);
	if (sameOk(COUNT_CMD, cmd))
	    {
	    if (differentString(track, tdb->tableName))
		errAbort("track name (%s) does not match table name (%s)\n", track, tdb->tableName);
	    printBedCount(genome, track, chrom, start, end, ci->size, hti, n, modified);
	    }
	else if (sameOk(RANGE_CMD, cmd))
	    {
	    typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
	    // handle different track types
	    if (sameOk(trackType[0], "bed") || sameOk(trackType[0], "genePred"))  
		{
		b = hGetBedRange(genome, tdb->tableName, chrom, start, end, NULL);
		printBed(genome, track, tdb->type, chrom, start, end, ci->size, hti, n, b, format, modified);
		}
	    else if (sameOk(trackType[0], "wigMaf"))  
		{
		printMaf(genome, track, chrom, ci->size, start, end, strand);
		}
	    else if (sameOk(trackType[0], "wig"))  
		{
		printWig(genome, track, chrom, ci->size, start, end, strand);
		}
	    else
		{
		ERR_BAD_TRACK_TYPE(track, tdb->type);
		}
	    }
	}
    }
else
    {
    ERR_INVALID_COMMAND(cmd);
    }
dbDbCladeFreeList(&dbs);
chromInfoFreeList(&ci);
trackDbFree(&tdb);
freez(&hti);
bedFreeList(&b);
hgPositionsFree(&hgp);
}

int main(int argc, char *argv[])
/* Process command line.
 * Stateless server; No "CART" required for REST interface */
{
cgiSpoof(&argc, argv); // spoof cgi vars if running from command line
initCgiInputMethod(cgiRequestMethod()); // otherwise initialize from request_method
verboseSetLevel(cgiOptionalInt("verbose", 1));

if (sameOk(cgiRequestMethod(), "GET"))
    doGet();
else 
    errAbort("request_method %s not implemented\n", cgiRequestMethod());
return 0;
}

