/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "bedGraph.h"
#include "bed.h"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.4 2009/01/13 15:48:53 mikep Exp $";

void doGet()
{
// these optional cgi strings are set to NULL if not present
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
char *cmd = cgiOptionalString("cmd");
char *action = cgiOptionalString("action");
char *db = cgiOptionalString("db");
char *track = cgiOptionalString("track");
// get chrom from 'chrom' or 'assembly'
// if both specified then use chrom
char *chrom = cgiUsualString("chrom",cgiOptionalString("assembly"));
char *format = cgiOptionalString("format");
// get coordinates from start/end or left/right
// if both start/end and left/right specified then use start/end
int start = cgiOptionalInt("start",cgiOptionalInt("left",0));
int end = cgiOptionalInt("end",cgiOptionalInt("right",0));
MJP(2); verbose(2,"cmd=[%s] db=[%s] track=[%s] chrom=[%s] start=%d end=%d\n", cmd, db, track, chrom, start,  end);
// list information about all active genome databases
if (!cmd && !db && !track)
    {
    okSendHeader(NULL);
    printf( "/data/databases          - information for all databases\n"
	    "/data/databases/DB       - information for database DB\n"
	    "/data/genome/DB          - database and chromosome information for DB\n"
	    "/data/genome/DB/CHROM    - database and chromosome information for CHROM in DB\n"
	    "/data/trackInfo/DB       - summary information for all tracks in DB\n"
	    "/data/trackInfo/DB/TRACK - information for one track TRACK in DB\n"
	    "/data/track/count/DB/TRACK/CHROM(/START/STOP)?\n"
            "                         - count of rows for chromosome CHROM in track TRACK in db DB\n"
	    "/data/track/range/DB/TRACK/CHROM(/START/STOP)?\n"
            "                         - rows for chromosome CHROM in track TRACK in db DB\n"
	    );
    if (verboseLevel()>1)
      printf("method=[%s] uri=[%s] params=[%s] query_string=[%s] http_accept=[%s] path_info=[%s] script_name=[%s] path_translated=[%s] \n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string, getenv("QUERY_STRING"), getenv("HTTP_ACCEPT"), getenv("PATH_INFO"), getenv("SCRIPT_NAME"), getenv("PATH_TRANSLATED"));
    }
// List of databases and clade information, or just for one if 'db' specified
else if (sameOk("databases", cmd)) 
    {
    MJP(2); verbose(2,"databases code\n");
    if (!(dbs = hGetIndexedDbClade(db)))
	ERR_DB_NOT_FOUND(db);
    okSendHeader(NULL);
    printDbs(dbs);
    }
// list information about genome "db"
else if (sameOk("genome", cmd)) // list database and chrom info
    {
    MJP(2); verbose(2,"genome code\n");
    if (!db)
	ERR_NO_DATABASE;
    if (!(dbs = hGetIndexedDbClade(db)))
	ERR_DB_NOT_FOUND(db);
    if (chrom)
	{
	if (!(ci = hGetChromInfo(db, chrom)))
	    ERR_CHROM_NOT_FOUND(db, chrom);
	}
    else
	ci = getAllChromInfo(db);
    if (format && differentString(FMT_JSON_ANNOJ,format))
        ERR_BAD_FORMAT(format);
    okSendHeader(NULL);
    if (sameOk(FMT_JSON_ANNOJ,format))  
	printGenomeAsAnnoj(dbs, ci);
    else 
	printGenome(dbs, ci);
    }
// list information about tracks in database "db"
else if (sameOk("trackInfo", cmd))
    {
    MJP(2); verbose(2,"trackInfo code db=%s track=%s \n", db, track);
    if (!db)
        ERR_NO_DATABASE;
    if (!track)
	{
	if (!(tdb = hTrackDb(db, NULL)))
	    ERR_TRACK_INFO_NOT_FOUND("<any>", db);
	}
    else
	{
	struct sqlConnection *conn = hAllocConn(db);
	if (!conn)
	    ERR_NO_DB_CONNECTION(db);
	if (!(tdb = hTrackInfo(conn, track)))
	    ERR_TRACK_INFO_NOT_FOUND(track, db);
	}
    okSendHeader(NULL);
    printTrackInfo(db, track, tdb);
    }
else if (sameOk("track", cmd) ) // get data from track using track 'type'
    {
    MJP(2); verbose(2,"data code\n");
    char *trackType[4] = {NULL,NULL,NULL,NULL};
    int typeWords;
    char parsedChrom[HDB_MAX_CHROM_STRING];
    char rootName[256];
    if (!db)
        ERR_NO_DATABASE;
    if (!track)
	ERR_NO_TRACK;
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
    // test how many rows we are about to receive
    int n = hGetBedRangeCount(db, tdb->tableName, chrom, start, end, NULL);
    if (sameOk("count",action))
	{
	okSendHeader(NULL);
	printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"count\" : %d\n}\n", db, track, tdb->tableName, chrom, start, end, n );
	}
    else if (sameOk("range", action))
	{
	MJP(2); verbose(2,"tdb tableName=[%s] action=%s chrom=%s root=%s type=[%s]\n", tdb->tableName, action, chrom, rootName, tdb->type);
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
		printBed(n, b, db, track, tdb->type, chrom, start, end, hti);
	    }
	else
	    {
	    // note: bedGraph not supported: where is graphColumn specified (see hg/hgTracks/bedGraph.c)
	    // note: genePred not supported: there are different schemas with fields not in C struct (c.f. refGene & knownGene tables)
	    // note: bed N not supported: need to figure out how to support different schemas like bed3, 4+, 6+, etc
	    ERR_BAD_TRACK_TYPE(track, tdb->type);
	    }
	}
    else if (sameOk("syndicate",action))
	{
	okSendHeader(NULL);
	printf(
"{\"success\":true,\"data\":{\"institution\":{\"name\":\"UCSC\",\"url\":\"http:\\/\\/genome.ucsc.edu\",\"logo\":\"images\\/title.jpg\"},\"engineer\":{\"name\":\"Michael Pheasant\",\"email\":\"mikep@soe.ucsc.edu\"},\"service\":{\"title\":\"Known Genes\",\"species\":\"Human\",\"access\":\"public\",\"version\":\"hg18\",\"format\":\"Unspecified\",\"server\":\"\",\"description\":\"The UCSC Known Genes.\"}}}\n");
	}
    else
	// note: bedGraph not supported: where is graphColumn specified (see hg/hgTracks/bedGraph.c)
	// note: genePred not supported: there are different schemas with fields not in C struct (c.f. refGene & knownGene tables)
	// note: bed N not supported: need to figure out how to support different schemas like bed3, 4+, 6+, etc
	ERR_BAD_ACTION(action, track, db);
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

