/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "bedGraph.h"
#include "bed.h"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.3 2009/01/09 20:53:28 mikep Exp $";
#define HGDATA_URI "/data"

/* Commands : 
else if (sameOk("chroms", cmd) || sameOk("chromsByColumns", cmd)) // list info for all chroms
    if (sameOk("chroms", cmd))
else if (sameOk("chrom", cmd)) // info for one chrom
else if (sameOk("dataCount", cmd)) // get data from track
else if (sameOk("data", cmd) || sameOk("dataByColumns", cmd) ) // get data from track
    if (sameOk("data", cmd))
if (sameOk(cgiRequestMethod(), "GET"))

/data/databases
/data/databasesByColumn
/data/databases/_DB_

^/data/(.+)/

/data/databases                  hgData?cmd=databases
/data/databasesByColumn          hgData?cmd=databases?ByColumn
/data/database/_DB_              hgData?cmd=database&db=hg18
/data/tracks                     hgData?cmd=tracks
/data/tracks/byColumn            hgData?cmd=tracksByColumn&db=_DB_
/data/track/_DB_/_TRACK_         hgData?cmd=track&db=hg18&track=knownGene
/data/chroms/_DB_                hgData?cmd=chroms&db=_DB_
/data/chromsByColumns/_DB_       hgData?cmd=chromsByColumns&db=_DB_
/data/chrom/_DB_/_CHROM_         hgData?cmd=chrom&db=hg18&chrom=hg18
/data/dataCount/_DB_/_POSITION_  hgData?cmd=dataCount&db=_DB_&chrom=_POSITION_
/data/data/_DB_/_POSITION_       hgData?cmd=data&db=_DB_&chrom=_POSITION_

/data/database
*/


void doGet()
{
// these optional cgi strings are set to NULL if not present
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
char *cmd = cgiOptionalString("cmd");
char *db = cgiOptionalString("db");
char *track = cgiOptionalString("track");
char *chrom = cgiOptionalString("chrom");
char *format = cgiOptionalString("format");
int start = cgiOptionalInt("start",0);
int end   = cgiOptionalInt("end",0);
MJP(2); verbose(2,"cmd=[%s] db=[%s] track=[%s] chrom=[%s] start=%d end=%d\n", cmd, db, track, chrom, start,  end);
// list information about all active databases
if (!cmd && !db && !track)
    {
    okSendHeader(NULL);
    printf("/%s\n/databases\ndatabasesByColumn\ndatabase\n", HGDATA_URI);
    if (verboseLevel()>1)
      printf("method=[%s] uri=[%s] params=[%s] query_string=[%s] http_accept=[%s] path_info=[%s] script_name=[%s] path_translated=[%s] \n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string, getenv("QUERY_STRING"), getenv("HTTP_ACCEPT"), getenv("PATH_INFO"), getenv("SCRIPT_NAME"), getenv("PATH_TRANSLATED"));
    }
else if (sameOk("databases", cmd) || sameOk("databasesByColumns", cmd)) 
    {
    MJP(2); verbose(2,"databases code\n");
    if (!(dbs = hGetIndexedDbClade(NULL)))
	ERR_NO_DBS_FOUND;
    okSendHeader(NULL);
    printf("{\n");
    if (sameOk("databases", cmd))
	printDb(dbs);
    else
	printDbByColumn(dbs);	    
    printf("}\n");
    }
// list information about database "db"
else if (sameOk("database", cmd))
    {
    MJP(2); verbose(2,"database code\n");
    if (!db)
	ERR_NO_DATABASE;
    if (!(dbs = hGetIndexedDbClade(db)))
	ERR_DB_NOT_FOUND(db);
    okSendHeader(NULL);
    printf("{\n");
    printOneDb(dbs);
    printf("}\n");
    }
// list information about tracks in database "db"
else if (sameOk("tracks", cmd) || sameOk("tracksByColumns", cmd))
    {
    MJP(2); verbose(2,"tracks code\n");
    if (!db)
	ERR_NO_DATABASE;
    if (!(tdb = hTrackDb(db, NULL)))
        ERR_TRACK_NOT_FOUND(track, db);
    okSendHeader(NULL);
    printf("{ \"db\" : \"%s\",\n", db );
    if (sameOk("tracks", cmd))
	printTrackDb(tdb);	    
    else
	printTrackDbByColumn(tdb);
    printf("}\n");
    }
// list information about one track "track" in database "db"
else if (sameOk("track", cmd))
    {
    MJP(2); verbose(2,"track code\n");
    if (!db)
	ERR_NO_DATABASE;
    struct sqlConnection *conn = hAllocConn(db);
    if (!conn)
        ERR_NO_DB_CONNECTION(db);
    if (!(tdb = hTrackInfo(conn, track)))
        ERR_TRACK_INFO_NOT_FOUND(track, db);
    okSendHeader(NULL);
    printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n", db, track);
    printOneTrackDb(tdb);
    printf("}\n");
    hFreeConn(&conn);
    }
else if (sameOk("chroms", cmd) || sameOk("chromsByColumns", cmd)) // list info for all chroms
    {
    MJP(2); verbose(2,"chroms code\n");
    if (!db)
	ERR_NO_DATABASE;
    ci = getAllChromInfo(db); // if no chroms found, just print emtpy list
    okSendHeader(NULL);
    printf("{ \"db\" : \"%s\",\n\"chromCount\" : %d,\n", db, slCount(ci) );
    if (sameOk("chroms", cmd))
	printChroms(ci);
    else
	printChromsByColumn(ci);
    printf("}\n");
    }
else if (sameOk("chrom", cmd)) // info for one chrom
    {
    MJP(2); verbose(2,"chrom code\n");
    if (!db)
	ERR_NO_DATABASE;
    if (!chrom)
	ERR_NO_CHROM;
    if (!(ci = hGetChromInfo(db, chrom)))
	ERR_CHROM_NOT_FOUND(db, chrom);
    okSendHeader(NULL);
    printf("{ \"db\" : \"%s\",\n", db );
    printOneChrom(ci);
    printf("}\n");
    }
else if (sameOk("dataCount", cmd)) // get data from track
    {
    MJP(2); verbose(2,"dataCount code\n");
    if (!db)
	ERR_NO_DATABASE;
    if (!chrom)
	ERR_NO_CHROM;
    if (!(ci = hGetChromInfo(db, chrom)))
	ERR_CHROM_NOT_FOUND(db, chrom);
    if (start<0)
	start = 0;
    if (end<0)
	end = 0;
    if (end==0)
	end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
    if (!(tdb = hTrackDbForTrack(db, track)))
        ERR_TRACK_NOT_FOUND(track, db);
    int n = hGetBedRangeCount(db, tdb->tableName, chrom, start, end, NULL);
    okSendHeader(NULL);
    printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"rowCount\" : %d,\n}\n", db, track, tdb->tableName, chrom, start, end, n );
    }
else if (sameOk("data", cmd) ) // get data from track using track 'type'
    {
    MJP(2); verbose(2,"data code\n");
    char *trackType[4] = {NULL,NULL,NULL,NULL};
    int typeWords;
    char parsedChrom[HDB_MAX_CHROM_STRING];
    char rootName[256];
    if (!db)
        ERR_NO_DATABASE;
    if (!chrom)
	ERR_NO_CHROM;
    if (!(ci = hGetChromInfo(db, chrom)))
	ERR_CHROM_NOT_FOUND(db, chrom);
    if (start<0)
	start = 0;
    if (end<0)
	end = 0;
    if (end==0)
	end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
    if (!(tdb = hTrackDbForTrack(db, track)))
        ERR_TRACK_NOT_FOUND(track, db);
    hParseTableName(db, tdb->tableName, rootName, parsedChrom);
    if (!(hti = hFindTableInfo(db, chrom, rootName)))
        ERR_TABLE_NOT_FOUND(tdb->tableName, chrom, rootName, db);
    MJP(2); verbose(2,"tdb tableName=[%s] chrom=%s root=%s type=[%s]\n", tdb->tableName, chrom, rootName, tdb->type);
    typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
    if (format && differentString(FMT_JSON_ANNOJ,format))
	ERR_BAD_FORMAT(format);
    if (1)  // need to test table type is supported format
	{
	MJP(2); verbose(2,"type=bed %s=%d code (%s,%s,%s,%d,%d)\n", FMT_JSON_ANNOJ, sameOk(FMT_JSON_ANNOJ,format), db, tdb->tableName, chrom, start, end);
	b = hGetBedRange(db, tdb->tableName, chrom, start, end, NULL);
	okSendHeader(NULL);
	if (sameOk(FMT_JSON_ANNOJ,format)) 
	    printBedAsAnnoj(b, hti);
	else // default format is BED
	    printBed(b, db, track, tdb->type, chrom, start, end, hti);
	}
    else
	{
	// note: bedGraph not supported: where is graphColumn specified (see hg/hgTracks/bedGraph.c)
	// note: genePred not supported: there are different schemas with fields not in C struct (c.f. refGene & knownGene tables)
	// note: bed N not supported: need to figure out how to support different schemas like bed3, 4+, 6+, etc
	ERR_BAD_TRACK_TYPE(track, tdb->type);
	}
    }
else if (sameOk("dataAsBed", cmd) ) // get data from track in 'BED' format
    {
    MJP(2); verbose(2,"dataAsBed code\n");
    char parsedChrom[HDB_MAX_CHROM_STRING];
    char rootName[256];
    if (!db)
        ERR_NO_DATABASE;
    if (!chrom)
        ERR_NO_CHROM;
    if (!(ci = hGetChromInfo(db, chrom)))
        ERR_CHROM_NOT_FOUND(db, chrom);
    if (start<0)
        start = 0;
    if (end<0)
        end = 0;
    if (end==0)
	end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
    if (!(tdb = hTrackDbForTrack(db, track)))
        ERR_TRACK_NOT_FOUND(track, db);
    hParseTableName(db, tdb->tableName, rootName, parsedChrom);
    if (!(hti = hFindTableInfo(db, chrom, rootName)))
        ERR_TABLE_NOT_FOUND(tdb->tableName, chrom, rootName, db);
    MJP(2); verbose(2,"tdb tableName=[%s] chrom=%s root=%s type=[%s]\n", tdb->tableName, chrom, rootName, tdb->type);
    b = hGetBedRange(db, tdb->tableName, chrom, start, end, NULL);
    okSendHeader(NULL);
    printBed(b, db, track, tdb->type, chrom, start, end, hti);
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
MJP(2); verbose(2,"method=[%s] uri=[%s] params=[%s]\n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string);

if (sameOk(cgiRequestMethod(), "GET"))
    doGet();
else 
    errAbort("request_method %s not implemented\n", cgiRequestMethod());
return 0;
}

