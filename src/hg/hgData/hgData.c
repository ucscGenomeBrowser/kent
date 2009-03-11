/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.24 2009/03/11 08:56:17 mikep Exp $";

void doGet()
{
// these optional cgi strings are set to NULL if not present
logTime(NULL);
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
logTime("init(%s,%s,%s,%s:%c:%d-%d)", cmd, genome, track, chrom, *strand, start, end);
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
    logTime("%s(genome=%s)", cmd, genome);
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
    logTime("%s(genome=%s,track=%s)", cmd, genome, track);
    if (*genome)
	{
	// validate genome & track
	// modified date is greatest of all trackDb dates or genome.chromInfo 
	modified = max(genomeMod, trackDbLatestUpdateTime(genome));
	logTime("trackDbLatestUpdateTime(%s)", genome);
	if (*track)
	    modified = max(modified, trackMod);
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (*track)
		{
		hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
		if (!(hti = hFindTableInfo(genome, chrom, rootName)))
		    ERR_TABLE_INFO_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
		logTime("hFindTableInfo(%s,%s,%s)", genome, chrom, rootName);
		}
	    else 
		{
		if ( !(tdb = hTrackDb(genome, NULL)) )
		    ERR_TRACK_INFO_NOT_FOUND("<any>", genome);
		logTime("hTrackDb(%s,NULL)",genome);
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
    logTime("%s(genome=%s)", cmd, genome);
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
	    logTime("findSpecLatestUpdateTime(%s)", genome);
	    if (!notModifiedResponse(reqEtag, reqModified, modified))
		{
		char where[512];
		safef(where, sizeof(where), "searchTable = '%s'", track);
		hgp = hgPositionsFindWhere(genome, term, "dummyCgi", "dummyApp", NULL, TRUE, where);
		logTime("hgPositionsFindWhere(%s,%s,dummyCgi,dummyApp,NULL,TRUE,where=%s)", genome, term, where);
		//hgp = hgPositionsFindWhere(genome, term, NULL, NULL, NULL, TRUE, where);
		searchTracks(modified, hgp, genome, track, term);
		}
	    }
	else // we are searching through many tracks, so dont use cache (dont set last-modified)
	    {
	    hgp = hgPositionsFind(genome, term, "dummyCgi", "dummyApp", NULL, TRUE);
	    logTime("hgPositionsFind(%s,%s,dummyCgi,dummyApp,NULL,TRUE)", genome, term);
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
    logTime("%s(genome=%s,track=%s)", cmd, genome, track);
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
    logTime("trackDbLatestUpdateTime(%s)", genome);
    if (!notModifiedResponse(reqEtag, reqModified, modified))
	{
	hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
	if (!*chrom)
	    ERR_NO_CHROM;
	if (!(ci = hGetChromInfo(genome, chrom)))
	    ERR_CHROM_NOT_FOUND(genome, chrom);
	logTime("hGetChromInfo(%s,%s)", genome, chrom);
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
	logTime("hFindTableInfo(%s,%s,%s)", genome, chrom, rootName);
	// test how many rows we are about to receive
	int n = hGetBedRangeCount(genome, tdb->tableName, chrom, start, end, NULL);
	logTime("hGetBedRangeCount(%s,%s,%s,%d,%d,NULL) -> %d", genome, tdb->tableName, chrom, start, end, n);
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
		logTime("hGetBedRange(%s,%s,%s,%d,%d,NULL) -> %d", genome, tdb->tableName, chrom, start, end);
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
logTime("complete.");
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

