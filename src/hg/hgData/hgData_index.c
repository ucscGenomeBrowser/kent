/* hgData_index - index page. */
#include "common.h"
#include "hgData.h"
//#include "dystring.h"


static char const rcsid[] = "$Id: hgData_index.c,v 1.1.2.9 2009/02/27 11:27:43 mikep Exp $";

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

void printUsage(char *reqEtag, time_t reqModified)
{
// dont need to continue if this changes to this file have not been committed
char *version = "$Date: 2009/02/27 11:27:43 $"; // local time, not GMT
time_t modified = strToTime(version, "$" "Date: %Y/%m/%d %T " "$");// careful CVS doesnt mangle format
if (notModifiedResponse(reqEtag, reqModified, modified))
    return;
struct json_object *msg = json_object_new_object();
struct json_object *vars = json_object_new_object();
struct json_object *res = json_object_new_object();
okSendHeader(modified, INDEX_EXPIRES);
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

// /genomes                                           [list of genomes]
// /genomes/{genome}                                  [genome + list of chroms]
// /genomes/{genome}/{chrom}                          [genome + chrom]
// /tracks                                            [list of all tracks in all genomes]
// /tracks/{genome}                                   [list of tracks for {genome}]
// /tracks/{genome}/{track}                           [track details for {track} in {genome}]
// /count/{track}/{genome}/{chrom}                    [count of data items in {track} in all of {genome}:{chrom}]
// /count/{track}/{genome}/{chrom}/{start}-{end}      [count of data items in {track} in range {genome}:{chrom}:{start}-{end}]
// /range/{track}/{genome}/{chrom}                    [list of data items in {track} in all of {genome}:{chrom}]
// /range/{track}/{genome}/{chrom}/{start}-{end}      [list of data items in {track} in range {genome}:{chrom}:{start}-{end}]
// /search/{term?}                                    [search for any data item matching {term} in any track in any genome]
// /search/{genome}/{term?}                           [search for any data item matching {term} in all tracks in {genome}]
// /search/{track}/{genome}/{term?}                   [search for any data item matching {term} in {track} in {genome}]
// /meta_search/{term?}                               [metadata search for track (description) matching {term} in any genome]
// /meta_search/{genome}/{term?}                      [metadata search for track (description) matching {term} in {genome}]

json_object_object_add(msg, "resources", res);
addResource(res, "genome_list", 
	PREFIX GENOMES_CMD,
	"List of genomes with brief description and genome hierarchy (provides valid " GENOME_VAR " values)", NULL);
addResource(res, "genome_chroms", 
	PREFIX GENOMES_CMD "/" GENOME_VAR, 
	"Detailed information on genome "GENOME_VAR" including names and sizes of all chromosomes "
	"(or a redirect to a url " PREFIX GENOMES_CMD "/" GENOME_VAR "/" CHROM_VAR" for information with "
	"one randomly selected scaffold if this is genome with too many 'scaffold' chromosomes)",
	addResourceOption(json_object_new_array(), ALLCHROMS_ARG, 
	    json_object_get_string(json_object_new_boolean(1)), 
	    "Force a list of all chromosomes even if there a large number"));
addResource(res, "genome_one_chrom", 
	PREFIX GENOMES_CMD "/" GENOME_VAR  "/" CHROM_VAR, 
	"Detailed info on genome "GENOME_VAR" including name and size of chromosome " CHROM_VAR, NULL);

// List of tracks for a genome or information for one track in a genome
addResource(res, "track_list", 
	PREFIX TRACKS_CMD , 
	"List of all tracks in all genomes (NOT IMPLEMENTED) ", NULL);
addResource(res, "track_list_genome", 
	PREFIX TRACKS_CMD "/" GENOME_VAR, 
	"List of all tracks in genome " GENOME_VAR, NULL);
addResource(res, "track_info", 
	PREFIX TRACKS_CMD "/" GENOME_VAR "/" TRACK_VAR , 
	"Info for track " TRACK_VAR " in genome " GENOME_VAR, NULL);

// Count of items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_count_chrom", 
	PREFIX COUNT_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR,
        "Count of items in track " TRACK_VAR " in chrom " CHROM_VAR " in genome " GENOME_VAR, NULL);
addResource(res, "track_count", 
	PREFIX COUNT_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR "/" START_VAR "-" END_VAR,
        "Count of items in track " TRACK_VAR " in range " CHROM_VAR ":" START_VAR "-" END_VAR " in genome " GENOME_VAR, NULL);

// Set of all items in a track in a whole chromosome or a range within a chromosome 
addResource(res, "track_range_chrom", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR,
        "List of items in track " TRACK_VAR " in chrom " CHROM_VAR " in genome " GENOME_VAR
	". Result format is a JSON derivative of the UCSC BED format.", NULL); 
addResource(res, "track_range_chrom_annoj_flat", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR ANNOJ_FLAT_FMT,
        "List of items in track " TRACK_VAR " in chrom " CHROM_VAR " in genome " GENOME_VAR
	". Result format is AnnoJ Model Flat (http://www.annoj.org).", NULL);
addResource(res, "track_range_chrom_annoj_nested", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR ANNOJ_NESTED_FMT,
        "List of items in track " TRACK_VAR " in chrom " CHROM_VAR " in genome " GENOME_VAR
	". Result format is AnnoJ Model Flat (http://www.annoj.org).", NULL);

addResource(res, "track_range", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR "/" START_VAR "-" END_VAR,
        "List of items in track " TRACK_VAR " in range " CHROM_VAR ":" START_VAR "-" END_VAR " in genome " GENOME_VAR
	". Result format is a JSON derivative of the UCSC BED format.", NULL); 
addResource(res, "track_range_annoj_flat", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR "/" START_VAR "-" END_VAR ANNOJ_FLAT_FMT,
        "List of items in track " TRACK_VAR " in range " CHROM_VAR ":" START_VAR "-" END_VAR " in genome " GENOME_VAR
	". Result format is AnnoJ Model Flat (http://www.annoj.org).", NULL);
addResource(res, "track_range_annoj_nested", 
	PREFIX RANGE_CMD "/" TRACK_VAR "/" GENOME_VAR "/" CHROM_VAR "/" START_VAR "-" END_VAR ANNOJ_NESTED_FMT,
        "List of items in track " TRACK_VAR " in range " CHROM_VAR ":" START_VAR "-" END_VAR " in genome " GENOME_VAR
	". Result format is AnnoJ Model Flat (http://www.annoj.org).", NULL);

// Details for one item in a track in a genome, or a list of all items matching a term
addResource(res, "search_item", 
	PREFIX SEARCH_CMD "/" TERM_VAR,
        "List of items matching " TERM_VAR " (NOT IMPLEMENTED)", NULL);
addResource(res, "search_item_genome", 
	PREFIX SEARCH_CMD "/" GENOME_VAR "/" TERM_VAR,
        "List of items matching " TERM_VAR " in genome " GENOME_VAR " (NOT IMPLEMENTED)", NULL);
addResource(res, "search_item_genome_track", 
	PREFIX SEARCH_CMD "/" TRACK_VAR "/" GENOME_VAR "/" TERM_VAR,
        "List of items matching " TERM_VAR " in track " TRACK_VAR " in genome " GENOME_VAR, NULL);

// Details for one item in a track in a genome, or a list of all items matching a term
addResource(res, "meta_search_item", 
	PREFIX META_SEARCH_CMD "/" TERM_VAR,
        "List of tracks matching " TERM_VAR " (NOT IMPLEMENTED)", NULL);
addResource(res, "meta_search_item_genome", 
	PREFIX META_SEARCH_CMD "/" GENOME_VAR "/" TERM_VAR,
        "List of tracks matching " TERM_VAR " in genome " GENOME_VAR " (NOT IMPLEMENTED)", NULL);

printf(json_object_to_json_string(msg));
json_object_put(msg);
}

