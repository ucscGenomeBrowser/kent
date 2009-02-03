/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "bedGraph.h"
#include "bed.h"

#define GENOME_VAR "_GENOME_"
#define GENOME_ARG "genome"
#define TRACK_VAR "_TRACK_"
#define TRACK_ARG "track"
#define TERM_VAR "_TERM_"
#define TERM_ARG "term"
#define CHROM_VAR "_CHROM_"
#define CHROM_ARG "chrom"
#define START_VAR "_START_"
#define START_ARG "start"
#define END_VAR "_END_"
#define END_ARG "end"

#define CMD_ARG       "cmd"
#define ACTION_ARG    "action"
#define ALLCHROMS_ARG "all_chroms"

#define INFO_ACTION   "info"
#define ITEM_ACTION   "item"
#define SEARCH_ACTION "search"
#define COUNT_ACTION  "count"
#define RANGE_ACTION  "range"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.8 2009/02/03 22:13:13 mikep Exp $";

struct json_object *jsonContact()
{
struct json_object *c = json_object_new_object();
json_object_object_add(c, "name", json_object_new_string("UCSC"));
json_object_object_add(c, "url", json_object_new_string("http://genome.ucsc.edu"));
json_object_object_add(c, "logo", json_object_new_string("/images/title.jpg"));
json_object_object_add(c, "contact", json_object_new_string("genome@soe.ucsc.edu"));
return c;
}

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

json_object_object_add(msg, "institution", jsonContact());
// list of standard variables used in genome queries
json_object_object_add(msg, "variables", vars);
addVariable(vars, GENOME_ARG, GENOME_VAR, "Genome assembly description (eg: hg18, mm9)");
addVariable(vars, CHROM_ARG,  CHROM_VAR,  "Chromsome (or scaffold) name (eg: chr1, scaffold_12345)");
addVariable(vars, START_ARG,  START_VAR,  "Chromsome start position (eg: 1234567)");
addVariable(vars, END_ARG,    END_VAR,    "Chromsome end position (eg: 1235000)");
addVariable(vars, TRACK_ARG,  TRACK_VAR,  "Track of genome annotations");
addVariable(vars, TERM_ARG,   TERM_VAR,   "Generic term variable used in searches, keyword lookup, item lookup, etc.");
// List of genomes or chromosomes and details for one genome
json_object_object_add(msg, "resources", res);
addResource(res, "genome_list", "/data/genome",
	"List of genomes with brief description and genome hierarchy (provides valid " GENOME_VAR " values)", NULL);
addResource(res, "genome_chroms", "/data/genome/"GENOME_VAR, 
	"Detailed information on genome "GENOME_VAR" including names and sizes of all chromosomes "
	"(or a redirect to a url /data/genome/"GENOME_VAR"/"CHROM_VAR" for information with "
	"one randomly selected scaffold if this is genome with too many 'scaffold' chromosomes)",
	addResourceOption(json_object_new_array(), ALLCHROMS_ARG, 
	    json_object_get_string(json_object_new_boolean(1)), 
	    "Force a list of all chromosomes even if there a large number"));
addResource(res, "genome_one_chrom", "/data/genome/" GENOME_VAR "/" CHROM_VAR, 
	"Detailed info on genome "GENOME_VAR" including name and size of chromosome " CHROM_VAR, NULL);
// List of tracks for a genome or information for one track in a genome
addResource(res, "track_list", "/data/track/info/" GENOME_VAR, 
	"List of all tracks in genome " GENOME_VAR, NULL);
addResource(res, "track_info", "/data/track/info/" GENOME_VAR "/" TRACK_VAR, 
	"Information for one track " TRACK_VAR " in genome " GENOME_VAR, NULL);
// Count of items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_count_chrom", "/data/track/count/"GENOME_VAR"/"TRACK_VAR"/"CHROM_VAR,
        "Count of items in track "TRACK_VAR" in chrom "CHROM_VAR" in genome " GENOME_VAR, NULL);
addResource(res, "track_count", "/data/track/count/"GENOME_VAR"/"TRACK_VAR"/"CHROM_VAR"/"START_VAR"/"END_VAR,
        "Count of items in track "TRACK_VAR" in range "CHROM_VAR":"START_VAR"-"END_VAR" in genome " GENOME_VAR, NULL);
// Set of all items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_range_chrom", "/data/track/range/"GENOME_VAR"/"TRACK_VAR"/"CHROM_VAR,
        "List of items in track "TRACK_VAR" in chrom "CHROM_VAR" in genome "GENOME_VAR
	". Default format is a JSON derivative of the UCSC BED format.", 
	    addResourceOption(
	    addResourceOption(json_object_new_array(), 
	    FORMAT_ARG,
            json_object_get_string(json_object_new_string(ANNOJ_FLAT_FMT)),
            "Return data in AnnoJ Flat Model format"),
	    FORMAT_ARG,
            json_object_get_string(json_object_new_string(ANNOJ_NESTED_FMT)),
            "Return data in AnnoJ Nested Model format"));
addResource(res, "track_range", "/data/track/range/"GENOME_VAR"/"TRACK_VAR"/"CHROM_VAR"/"START_VAR"/"END_VAR,
        "List of items in track "TRACK_VAR" in range "CHROM_VAR":"START_VAR"-"END_VAR" in genome "GENOME_VAR
	". Default format is a JSON derivative of the UCSC BED format.", 
	    addResourceOption(
	    addResourceOption(json_object_new_array(), 
	    FORMAT_ARG,
            json_object_get_string(json_object_new_string(ANNOJ_FLAT_FMT)),
            "Return data in AnnoJ Flat Model format"),
	    FORMAT_ARG,
            json_object_get_string(json_object_new_string(ANNOJ_NESTED_FMT)),
            "Return data in AnnoJ Nested Model format"));
// Details for one item in a track in a genome, or a list of all items matching a term
addResource(res, "track_item", "/data/track/item/"GENOME_VAR"/"TRACK_VAR"/"TERM_VAR,
        "Detailed information for item "TERM_VAR" in track "TRACK_VAR" in genome " GENOME_VAR, NULL);
addResource(res, "track_item", "/data/track/list/"GENOME_VAR"/"TRACK_VAR"/"TERM_VAR,
        "List of items matching "TERM_VAR" in track "TRACK_VAR" in genome "GENOME_VAR, NULL);
// print the usage message
printf(json_object_to_json_string(msg));
json_object_put(msg);
}

void doGet()
{
// these optional cgi strings are set to NULL if not present
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
char *cmd = cgiOptionalString(CMD_ARG);
char *action = cgiOptionalString(ACTION_ARG);
char *format = cgiOptionalString(FORMAT_ARG);
char *genome = cgiOptionalString(GENOME_ARG);
char *track = cgiOptionalString(TRACK_ARG);
char *term = cgiOptionalString(TERM_ARG);
char *chrom = cgiOptionalString(CHROM_ARG);
int start = cgiOptionalInt(START_ARG, 0);
int end = cgiOptionalInt(END_ARG, 0);
char rootName[HDB_MAX_TABLE_STRING];
char parsedChrom[HDB_MAX_CHROM_STRING];
char *trackType[4] = {NULL,NULL,NULL,NULL};
int typeWords;
if (verboseLevel()>1)
    printf("method=[%s] uri=[%s] params=[%s] query_string=[%s] http_accept=[%s] path_info=[%s] script_name=[%s] path_translated=[%s] \n",cgiRequestMethod(), cgiRequestUri(), cgiUrlString()->string, getenv("QUERY_STRING"), getenv("HTTP_ACCEPT"), getenv("PATH_INFO"), getenv("SCRIPT_NAME"), getenv("PATH_TRANSLATED"));
// list information about all active genome databases
if (!cmd && !genome && !track)
    {
    okSendHeader(NULL);
    printUsage();
    }
// List of genome databases and clade information, or just for one if 'genome' specified
//  /data/genome  List of genomes with brief description and genome hierarchy (provides valid _GENOME_ values)
//  /data/genome/_GENOME_  Detailed info on genome _GENOME_ including names and sizes of all chromosomes 
//      (or a redirect to a url /data/genome/_GENOME_/_CHROM_ for information with one randomly selected 
//      scaffold if this is genome with too many 'scaffold' chromosomes)
//    options:
//      "all_chroms=true" Force a list of all chromosomes even if there a large number
//  /data/genome/_GENOME_/_CHROM_  Detailed info on genome _GENOME_ including name and size of chromosome _CHROM_
else if (sameOk(GENOME_ARG, cmd)) 
    {
    dbs = hGetIndexedDbClade(genome); 
    if (!genome)
	{
	if (!dbs)
	    ERR_NO_GENOMES_FOUND;
	}
    else
	{
	if (!dbs)
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
    okSendHeader(NULL);
    printGenomes(dbs, ci);
    }
// List of all tracks in genome or detail for one track in genome
//    /data/track/info/_GENOME_
//    /data/track/info/_GENOME_/_TRACK_
// Count of items in a track in a whole chromosome or a range within a chromosome
//    /data/track/count/_GENOME_/_TRACK_/_CHROM_
//    /data/track/count/_GENOME_/_TRACK_/_CHROM_/_START_/_END_
// List of items in a track in a whole chromosome or a range within a chromosome
//    /data/track/range/_GENOME_/_TRACK_/_CHROM_
//    /data/track/range/_GENOME_/_TRACK_/_CHROM_/_START_/_END_
else if (sameOk(TRACK_ARG, cmd))
    {
    if (!genome)
        ERR_NO_GENOME;
    if (sameOk(INFO_ACTION, action))
	{
	if (track==NULL)
	    {
	    if (!(tdb = hTrackDb(genome, NULL)))
		ERR_TRACK_INFO_NOT_FOUND("<any>", genome);
	    }
	else
	    {
	    struct sqlConnection *conn = hAllocConn(genome);
	    if (!conn)
		ERR_NO_GENOME_DB_CONNECTION(genome);
	    if (!(tdb = hTrackInfo(conn, track)))
		ERR_TRACK_INFO_NOT_FOUND(track, genome);
	    //hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
	    //if (!(hti = hFindTableInfo(genome, chrom, rootName)))
	//	ERR_TABLE_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
	    }
	okSendHeader(NULL);
	printTrackInfo(genome, tdb, hti);
	}
    else if (sameOk(SEARCH_ACTION, action))
	{
	}
    else if (sameOk(ITEM_ACTION, action))
	{
	}
    else if (sameOk(COUNT_ACTION, action) || sameOk(RANGE_ACTION, action))
	{
        // queries which require location
	if (!track)
	    ERR_NO_TRACK;
	if (!(tdb = hTrackDbForTrack(genome, track)))
	    ERR_TRACK_NOT_FOUND(track, genome);
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
        if (sameOk(COUNT_ACTION, action))
            {
	    if (differentString(track, tdb->tableName))
		errAbort("track name (%s) does not match table name (%s)\n", track, tdb->tableName);
            okSendHeader(NULL);
            printBedCount(genome, track, chrom, start, end, hti, n);
            }
        else if (sameOk("range", action))
            {
            typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
            if (sameOk(trackType[0], "bed") || sameOk(trackType[0], "genePred"))  // need to test table type is supported format
                {
                b = hGetBedRange(genome, tdb->tableName, chrom, start, end, NULL);
                okSendHeader(NULL);
                printBed(genome, track, tdb->type, chrom, start, end, hti, n, b, format);
                }
            else
                {
                ERR_BAD_TRACK_TYPE(track, tdb->type);
                }
            }
	else
	    {
	    errAbort("action %s unknown\n", action);
	    }
	}
    else
	{
	ERR_BAD_ACTION(action, track, genome);
	}
    }
else if (0 && sameOk("track", cmd) ) // get data from track using track 'type'
    {
    if (!genome)
        ERR_NO_GENOME;
    if (!track)
	ERR_NO_TRACK;
    if (!(tdb = hTrackDbForTrack(genome, track)))
        ERR_TRACK_NOT_FOUND(track, genome);
    hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
    // queries which require only db & table 
    if (sameOk("item", action))
	{
	typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
	okSendHeader(NULL);
	printItem(genome, track, trackType[0], term);
	}
    else
	{
        // queries which require location
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
        if (sameOk("count",action))
	    {
	    okSendHeader(NULL);
	    printf("{ \"genome\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"count\" : %d\n}\n", genome, track, tdb->tableName, chrom, start, end, n );
	    }
        else if (sameOk("range", action))
	    {
	    typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
	    if (1)  // need to test table type is supported format
		{
		b = hGetBedRange(genome, tdb->tableName, chrom, start, end, NULL);
		okSendHeader(NULL);
		//printBed(n, b, genome, track, tdb->type, chrom, start, end, hti);
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
	    ERR_BAD_ACTION(action, track, genome);
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

