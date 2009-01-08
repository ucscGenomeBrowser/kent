/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.1 2009/01/08 05:22:37 mikep Exp $";
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
/data/dataCount/_DB_/_POSITION_  hgData?cmd=dataCount&db=_DB_&position=_POSITION_
/data/data/_DB_/_POSITION_       hgData?cmd=data&db=_DB_&position=_POSITION_

/data/database
*/


void doGet()
{
char *chrom; // chrom from position
int start, end;  // start,end from position
// these optional cgi strings are set to NULL if not present
AllocArray(chrom, 128);
char *cmd = cgiOptionalString("cmd");
char *db = cgiOptionalString("db");
char *track = cgiOptionalString("track");
char *position = cgiOptionalString("position");
// list information about all active databases
if (!cmd && !db && !track && !position)
    {
    okSendHeader();
    printf("/%s\n/databases\ndatabasesByColumn\ndatabase\n", HGDATA_URI);
    if (verboseLevel()>1)
      printf("method=[%s] uri=[%s] params=[%s] query_string=[%s] http_accept=[%s] path_info=[%s] script_name=[%s] path_translated=[%s] \n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string, getenv("QUERY_STRING"), getenv("HTTP_ACCEPT"), getenv("PATH_INFO"), getenv("SCRIPT_NAME"), getenv("PATH_TRANSLATED"));
    }
else if (sameOk("databases", cmd) || sameOk("databasesByColumns", cmd)) 
    {
    struct dbDbClade *dbs = hGetIndexedDbClade(NULL);
    if (!dbs)
	ERR_NO_DBS_FOUND;
    okSendHeader();
    printf("{ \n");
    if (sameOk("databases", cmd))
	printDb(dbs);
    else
	printDbByColumn(dbs);	    
    printf("}\n");
    dbDbCladeFreeList(&dbs);
    }
// list information about database "db"
else if (sameOk("database", cmd))
    {
    if (!db)
	ERR_NO_DATABASE;
    struct dbDbClade *dbs = hGetIndexedDbClade(db);
    if (!dbs)
	ERR_DB_NOT_FOUND(db);
    okSendHeader();
    printf("{\n");
    printOneDb(dbs);
    printf("}\n");
    dbDbCladeFreeList(&dbs);
    }
// list information about tracks in database "db"
else if (sameOk("tracks", cmd) || sameOk("tracksByColumns", cmd))
    {
    if (!db)
	ERR_NO_DATABASE;
    struct trackDb *tdb = hTrackDb(db, NULL);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n", db );
    if (sameOk("tracks", cmd))
	printTrackDb(tdb);	    
    else
	printTrackDbByColumn(tdb);
    printf("}\n");
    trackDbFree(&tdb);
    }
// list information about one track "track" in database "db"
else if (sameOk("track", cmd))
    {
    if (!db)
	ERR_NO_DATABASE;
    struct sqlConnection *conn = hAllocConn(db);
    struct trackDb *tdb = hTrackInfo(conn, track);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n", db, track);
    printOneTrackDb(tdb);
    printf("}\n");
    trackDbFree(&tdb);
    hFreeConn(&conn);
    }
else if (sameOk("chroms", cmd) || sameOk("chromsByColumns", cmd)) // list info for all chroms
    {
    if (!db)
	ERR_NO_DATABASE;
    struct chromInfo *ci = getAllChromInfo(db);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n\"chromCount\" : %d,\n", db, slCount(ci) );
    if (sameOk("chroms", cmd))
	printChroms(ci);
    else
	printChromsByColumn(ci);
    printf("}\n");
    chromInfoFree(&ci);
    }
else if (sameOk("chrom", cmd)) // info for one chrom
    {
    if (!db)
	ERR_NO_DATABASE;
    if (!chrom)
	ERR_NO_CHROM;
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n", db );
    printOneChrom(ci);
    printf("}\n");
    chromInfoFree(&ci);
    }
else if (sameOk("dataCount", cmd)) // get data from track
    {
    if (!db)
	ERR_NO_DATABASE;
    if (!hgParseChromRange(db, position, &chrom, &start, &end))
	ERR_CANT_PARSE_POSITION(position, db);
    struct trackDb *tdb = hTrackDbForTrack(db, track);
    int n = hGetBedRangeCount(db, tdb->tableName, chrom, start, end, NULL);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"rowCount\" : %d,\n}\n", db, track, tdb->tableName, chrom, start, end, n );
    trackDbFree(&tdb);
    }
else if (sameOk("data", cmd) ) // get data from track
    {
    if (!db)
	ERR_NO_DATABASE;
    char parsedChrom[HDB_MAX_CHROM_STRING];
    char rootName[256];
    if (!hgParseChromRange(db, position, &chrom, &start, &end))
	ERR_CANT_PARSE_POSITION(position, db);
    struct trackDb *tdb = hTrackDbForTrack(db, track);
    if (tdb == NULL)
	ERR_TRACK_NOT_FOUND(track, db);
    hParseTableName(db, tdb->tableName, rootName, parsedChrom);
    struct hTableInfo *hti = hFindTableInfo(db, chrom, rootName);
    if (hti == NULL)
	ERR_TABLE_NOT_FOUND(rootName, tdb->tableName, db);
    struct bed *b = hGetBedRange(db, tdb->tableName, chrom, start, end, NULL);
    okSendHeader();
    printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"position\" : \"%s\",\n\
\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"rowCount\" : %d,\n\
\"hasCDS\" : %s,\n\"hasBlocks\" : %s,\n\"type\" : \"%s\",\n", 
	db, track, tdb->tableName, position, hti->chromField, start, end, slCount(b), 
	(hti->hasCDS ? "true" : "false"), (hti->hasBlocks ? "true" : "false"),
	tdb->type );
    printBed(b, hti);
    printf("}\n");
    bedFree(&b);
    trackDbFree(&tdb);
    }
else
    ERR_INVALID_COMMAND(cmd);
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
