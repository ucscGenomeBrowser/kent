/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "bedGraph.h"
#include "bed.h"

#ifdef boolean
#undef boolean
// common.h defines boolean as int; json.h typedefs boolean as int.
#include <json/json.h>
#endif

#define GENOMEVAR "_GENOME_"
#define TRACKVAR "_TRACK_"
#define TERMVAR "_TERM_"
#define CHROMVAR "_CHROM_"
#define STARTVAR "_START_"
#define ENDVAR "_END_"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.5 2009/01/31 05:15:35 mikep Exp $";

void addVariable(struct json_object *var, char *variable, char *name, char *description)
{
struct json_object *body = json_object_new_object();
json_object_object_add(var, variable, body);
json_object_object_add(body, "name", json_object_new_string(name));
json_object_object_add(body, "description", json_object_new_string(description));
}

void addResource(struct json_object *res, char *resource, char *url, char *description, struct json_object *options)
// return a resource object with url, description, and options array (emtpy array if NULL)
{
struct json_object *body = json_object_new_object();
json_object_object_add(res, resource, body);
json_object_object_add(body, "url", json_object_new_string(url));
json_object_object_add(body, "description", json_object_new_string(description));
json_object_object_add(body, "options", options ? options : json_object_new_array());
}

struct json_object *addResourceOption(struct json_object *opts, char *name, char *value, char *description)
{
struct json_object *opt = json_object_new_object();
json_object_array_add(opts, opt);
json_object_object_add(opt, "name", json_object_new_string(name));
json_object_object_add(opt, "value", json_object_new_string(value));
json_object_object_add(opt, "description", json_object_new_string(description));
return opts;
}

void printUsage()
{
struct json_object *msg = json_object_new_object();
struct json_object *vars = json_object_new_object();
struct json_object *res = json_object_new_object();

json_object_object_add(msg, "variables", vars);
// list of standard variables used in genome queries
addVariable(vars, "genome", GENOMEVAR, "Genome assembly description (eg: hg18, mm9)");
addVariable(vars, "chromosome", CHROMVAR, "Chromsome (or scaffold) name (eg: chr1, scaffold_12345)");
addVariable(vars, "start", STARTVAR, "Chromsome start position (eg: 1234567)");
addVariable(vars, "end", ENDVAR, "Chromsome end position (eg: 1235000)");
addVariable(vars, "track", TRACKVAR, "Track of genome annotations");
addVariable(vars, "term", TERMVAR, "Generic term variable used in searches, keyword lookup, item lookup, etc.");

json_object_object_add(msg, "resources", res);
// List of genomes or chromosomes and details for one genome
addResource(res, "genome_list", "/data/genome", 
	"List of genomes with brief description (provides valid " GENOMEVAR " values)", NULL);
addResource(res, "genome_chroms", "/data/genome/"GENOMEVAR, 
	"Detailed info on genome "GENOMEVAR" including names and sizes of all chromosomes "
	"(or a redirect to a url /data/genome/"GENOMEVAR"/"CHROMVAR" for information with "
	"one randomly selected scaffold if this is genome with too many 'scaffold' chromosomes)",
	addResourceOption(json_object_new_array(), "all_chroms", 
	    json_object_get_string(json_object_new_boolean(1)), 
	    "force a list of all chromosomes even if there a large number"));
addResource(res, "genome_one_chrom", "/data/genome/" GENOMEVAR "/" CHROMVAR, 
	"Detailed info on genome "GENOMEVAR" including name and size of chromosome " CHROMVAR, NULL);
// List of tracks for a genome or information for one track in a genome
addResource(res, "track_list", "/data/track/info/" GENOMEVAR, 
	"List of all tracks in genome " GENOMEVAR, NULL);
addResource(res, "track_info", "/data/track/info/" GENOMEVAR "/" TRACKVAR, 
	"information for one tracks " TRACKVAR " in genome " GENOMEVAR, NULL);
// Count of items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_count_chrom", "/data/track/count/"GENOMEVAR"/"TRACKVAR"/"CHROMVAR,
        "Count of items in track "TRACKVAR" in chrom "CHROMVAR" in genome " GENOMEVAR, NULL);
addResource(res, "track_count_range", "/data/track/count/"GENOMEVAR"/"TRACKVAR"/"CHROMVAR"/"STARTVAR"/"ENDVAR,
        "Count of items in track "TRACKVAR" in range "CHROMVAR":"STARTVAR"-"ENDVAR" in genome " GENOMEVAR, NULL);
// Set of all items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_range_chrom", "/data/track/range/"GENOMEVAR"/"TRACKVAR"/"CHROMVAR,
        "List of items in track "TRACKVAR" in chrom "CHROMVAR" in genome " GENOMEVAR, NULL);
addResource(res, "track_range", "/data/track/count/"GENOMEVAR"/"TRACKVAR"/"CHROMVAR"/"STARTVAR"/"ENDVAR,
        "List of items in track "TRACKVAR" in range "CHROMVAR":"STARTVAR"-"ENDVAR" in genome " GENOMEVAR, NULL);
// Details for one item in a track in a genome, or a list of all items matching a term
addResource(res, "track_item", "/data/track/item/"GENOMEVAR"/"TRACKVAR"/"TERMVAR,
        "Detailed information for item "TERMVAR" in track "TRACKVAR" in genome " GENOMEVAR, NULL);
addResource(res, "track_item", "/data/track/list/"GENOMEVAR"/"TRACKVAR"/"TERMVAR,
        "List of items matching "TERMVAR" in track "TRACKVAR" in genome "GENOMEVAR, NULL);
// print the usage message
printf(json_object_to_json_string(msg));
}

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
char *term = cgiOptionalString("term");
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
    if (verboseLevel()>1)
      printf("method=[%s] uri=[%s] params=[%s] query_string=[%s] http_accept=[%s] path_info=[%s] script_name=[%s] path_translated=[%s] \n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string, getenv("QUERY_STRING"), getenv("HTTP_ACCEPT"), getenv("PATH_INFO"), getenv("SCRIPT_NAME"), getenv("PATH_TRANSLATED"));
    printUsage();
    }
// List of genome databases and clade information, or just for one if 'db' specified
else if (sameOk("genomes", cmd)) 
    {
    MJP(2); verbose(2,"genomes code\n");
    if (!(dbs = hGetIndexedDbClade(db)))
	ERR_GENOME_NOT_FOUND(db);
    okSendHeader(NULL);
    printDbs(dbs);
    }
// list information about genome "db"
else if (sameOk("genome", cmd)) // list database and chrom info
    {
    MJP(2); verbose(2,"genome code\n");
    if (!db)
	ERR_NO_GENOME;
    if (!(dbs = hGetIndexedDbClade(db)))
	ERR_GENOME_NOT_FOUND(db);
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
        ERR_NO_GENOME;
    if (!track)
	{
	if (!(tdb = hTrackDb(db, NULL)))
	    ERR_TRACK_INFO_NOT_FOUND("<any>", db);
	}
    else
	{
	struct sqlConnection *conn = hAllocConn(db);
	if (!conn)
	    ERR_NO_GENOME_DB_CONNECTION(db);
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
        ERR_NO_GENOME;
    if (!track)
	ERR_NO_TRACK;
    if (!(tdb = hTrackDbForTrack(db, track)))
        ERR_TRACK_NOT_FOUND(track, db);
    hParseTableName(db, tdb->tableName, rootName, parsedChrom);
    // queries which require only db & table 
    if (sameOk("item", action))
	{
	MJP(2); verbose(2,"tdb tableName=[%s] action=%s term=%s db=%s root=%s type=[%s]\n", tdb->tableName, action, term, db, rootName, tdb->type);
	typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
	if (format && differentString(FMT_JSON_ANNOJ,format))
	    ERR_BAD_FORMAT(format);
	MJP(2); verbose(2,"type=%s %s=%d code (%s,%s,%s,%d,%d)\n", trackType[0], FMT_JSON_ANNOJ, sameOk(FMT_JSON_ANNOJ,format), db, tdb->tableName, chrom, start, end);
	okSendHeader(NULL);
	if (sameOk(FMT_JSON_ANNOJ,format)) 
	    printItemAsAnnoj(db, track, trackType[0], term);
	else // default format is BED
	    printItem(db, track, trackType[0], term);
	}
    else if (sameOk("syndicate",action))
	{
	okSendHeader(NULL);
	printf(
"{\"success\":true,\"data\":{\"institution\":{\"name\":\"UCSC\",\"url\":\"http:\\/\\/genome.ucsc.edu\",\"logo\":\"images\\/title.jpg\"},\"engineer\":{\"name\":\"Michael Pheasant\",\"email\":\"mikep@soe.ucsc.edu\"},\"service\":{\"title\":\"Known Genes\",\"species\":\"Human\",\"access\":\"public\",\"version\":\"hg18\",\"format\":\"Unspecified\",\"server\":\"\",\"description\":\"The UCSC Known Genes.\"}}}\n");
	}
    else
	{
        // queries which require location
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
		ERR_BAD_TRACK_TYPE(track, tdb->type);
		}
	    }
        else
	    {
	    // note: bedGraph not supported: where is graphColumn specified (see hg/hgTracks/bedGraph.c)
	    // note: genePred not supported: there are different schemas with fields not in C struct (c.f. refGene & knownGene tables)
	    // note: bed N not supported: need to figure out how to support different schemas like bed3, 4+, 6+, etc
	    ERR_BAD_ACTION(action, track, db);
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
MJP(2); verbose(2,"method=[%s] uri=[%s] params=[%s]\n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string);

if (sameOk(cgiRequestMethod(), "GET"))
    doGet();
else 
    errAbort("request_method %s not implemented\n", cgiRequestMethod());
return 0;
}

