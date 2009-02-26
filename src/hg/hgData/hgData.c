/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "bedGraph.h"
#include "bed.h"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.16 2009/02/26 20:10:00 mikep Exp $";

void doGet()
{
// these optional cgi strings are set to NULL if not present
struct tm *thisTime, *latestTime;
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
//char *authorization = getenv("Authorization");
//verboseSetLevel(2);
char *reqEtag = getenv("ETag");
time_t reqModified = strToTime(getenv("Modified"), "%a, %d %b %Y %H:%M:%S GMT");// Thu, 08 Jan 2009 17:45:18 GMT
char *cmd = cgiOptionalString(CMD_ARG);
char *format = cgiOptionalString(FORMAT_ARG);
char *genome = cgiOptionalString(GENOME_ARG);
char *track = cgiOptionalString(TRACK_ARG);
//char *term = cgiOptionalString(TERM_ARG);
char *chrom = cgiOptionalString(CHROM_ARG);
int start = cgiOptionalInt(START_ARG, 0);
int end = cgiOptionalInt(END_ARG, 0);
char rootName[HDB_MAX_TABLE_STRING];
char parsedChrom[HDB_MAX_CHROM_STRING];
char *trackType[4] = {NULL,NULL,NULL,NULL};
int typeWords;
time_t modified = 0;
AllocVar(thisTime);
AllocVar(latestTime);
// list information about all active genome databases
if (!cmd && !genome && !track)
    {
    // get modified time from CVS $Date field in file where code resides
    printUsage(reqEtag, reqModified);
    }
else if (sameOk(GENOMES_CMD, cmd)) 
    {
    // check for changes in dbDb, genomeClade, clade
    modified = hGetLatestUpdateTimeDbClade();
    if (genome)
	{
	if (!hDbIsActive(genome))
	    ERR_GENOME_NOT_FOUND(genome);
	modified = max(modified, hGetLatestUpdateTimeChromInfo(genome));
	}
    // send 304 not modified if all looks OK
    if (!notModifiedResponse(reqEtag, reqModified, modified))
	{
	if (genome)
	    {
	    if (!(dbs = hGetIndexedDbClade(genome)))
		ERR_GENOME_NOT_FOUND(genome);
	    if (chrom)
		{
		if (!(ci = hGetChromInfo(genome, chrom)))
		    ERR_CHROM_NOT_FOUND(genome, chrom);
		}
	    else
		{
		if (!(ci = getAllChromInfo(genome)))
		    ERR_NO_CHROMS_FOUND(genome);
		}
	    }
	else
	    if (!(dbs = hGetIndexedDbClade(NULL)))
		ERR_NO_GENOMES_FOUND;
	// TEST FOR number of chroms 
	// if (hChromCount(genome) < MAX_CHROM_COUNT || cgiBoolean(ALLCHROMS_ARG)
	printGenomes(dbs, ci, modified);
	}
    }
else if (sameOk(TRACKS_CMD, cmd))
    {
    if (genome)
	{
	if (!hDbIsActive(genome))
	    ERR_GENOME_NOT_FOUND(genome);
	// modified date is greatest of all trackDb dates or genome.chromInfo 
	modified = max(trackDbLatestUpdateTime(genome), hGetLatestUpdateTimeChromInfo(genome));
	if (track)
	    {
	    struct sqlConnection *conn = hAllocConn(genome);
	    if (!conn)
		ERR_NO_GENOME_DB_CONNECTION(genome);
	    if (!(tdb = hTrackInfo(conn, track)))
		ERR_TRACK_INFO_NOT_FOUND(track, genome);
	    modified = max(modified, sqlTableUpdateTime(conn, track)); // check if track has changed
	    }
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (track)
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
	    printTrackInfo(genome, tdb, hti, modified);
	    }
	}
    else
	{
        ERR_NOT_IMPLEMENTED("List of all tracks in all genomes");
	}
    }
else if (sameOk(SEARCH_CMD, cmd))
    {
    }
else if (sameOk(META_SEARCH_CMD, cmd))
    {
    }
else if (sameOk(COUNT_CMD, cmd) || sameOk(RANGE_CMD, cmd))
    {
    if (!genome)
	ERR_NO_GENOME;
    if (!hDbIsActive(genome))
	ERR_GENOME_NOT_FOUND(genome);
    // check all trackDbs and genome.chromInfo for changes
    modified = max(trackDbLatestUpdateTime(genome), hGetLatestUpdateTimeChromInfo(genome));
    if (!track)
	ERR_NO_TRACK;
    if (!(tdb = hTrackDbForTrack(genome, track)))
	ERR_TRACK_NOT_FOUND(track, genome);
    struct sqlConnection *conn = hAllocConn(genome);
    modified = max(modified, sqlTableUpdateTime(conn, track)); // check if track is modified
    if (!notModifiedResponse(reqEtag, reqModified, modified))
	{
	hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
	if (!chrom)
	    ERR_NO_CHROM;
	if (!(ci = hGetChromInfo(genome, chrom)))
	    ERR_CHROM_NOT_FOUND(genome, chrom);
	if (start<0)
	    start = 0;
	if (end<0)
	    end = 0;
	if (end==0)
	    end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
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
	    if (sameOk(trackType[0], "bed") || sameOk(trackType[0], "genePred"))  // need to test table type is supported format
		{
		b = hGetBedRange(genome, tdb->tableName, chrom, start, end, NULL);
		printBed(genome, track, tdb->type, chrom, start, end, ci->size, hti, n, b, format, modified);
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

