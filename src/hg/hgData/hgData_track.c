/* hgData_track - functions dealing with tracks. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

#ifdef boolean                                                             
#undef boolean                                                             
// common.h defines boolean as int; json.h typedefs boolean as int.        
#include <json/json.h>                                                     
#endif                                                                     

static char const rcsid[] = "$Id: hgData_track.c,v 1.1.2.8 2009/02/26 18:41:04 mikep Exp $";

// /tracks                                            [list of all tracks in all genomes]
// /tracks/{genome}                                   [list of tracks for {genome}]
// /tracks/{track}/{genome}                           [track details for {track} in {genome}]

time_t oneTrackDbUpdateTime(char *db, char *tblSpec)
/* get latest update time for a trackDb table, including handling profiles:tbl. 
 * Returns 0 if table doesnt exist
  */
{
char *tbl;
struct sqlConnection *conn = hAllocConnProfileTbl(db, tblSpec, &tbl);
time_t latest = sqlTableUpdateTime(conn, tbl);
hFreeConn(&conn);
return latest;
}

time_t trackDbLatestUpdateTime(char *db)
/* Get latest update time from each trackDb table. */
{
struct slName *tableList = hTrackDbList(), *one;
time_t latest = 0;
for (one = tableList; one != NULL; one = one->next)
    {
    latest = max(latest, oneTrackDbUpdateTime(db, one->name));
    }
slNameFreeList(&tableList);
return latest;
}


struct json_object *addTrackUrl(struct json_object *o, char *url_name, char *genome, char *track)
// Add a track url to object o with name 'url_name'.
// Genome must be supplied if track is supplied, otherwise can be NULL
// Track can be NULL
// Returns object o
{
char url[1024];
if (genome)
    {
    if (track)
	safef(url, sizeof(url), PREFIX TRACKS_CMD "/%s/%s%s", track, genome, JSON_EXT);
    else
	safef(url, sizeof(url), PREFIX TRACKS_CMD "/%s%s", genome, JSON_EXT);
    }
  else
    {
    safef(url, sizeof(url), PREFIX TRACKS_CMD "%s", JSON_EXT);
    }
json_object_object_add(o, url_name, json_object_new_string(url));
return o;
}

static struct json_object *jsonOneTrack(struct trackDb *tdb, char *genome)
// Brief information about a single track
{
char *url;
struct json_object *t = json_object_new_object();
json_object_object_add(t, "track", json_object_new_string(tdb->tableName)); /* Symbolic ID of Track */
json_object_object_add(t, "track_group", json_object_new_string(tdb->grp)); /* Which group track belongs to */
json_object_object_add(t, "track_priority", json_object_new_double(tdb->priority)); /* 0-100 - where to position with group.  0 is top */
json_object_object_add(t, "type", json_object_new_string(tdb->type)); /* Track type: bed, psl, genePred, etc. */
json_object_object_add(t, "short_label", json_object_new_string(tdb->shortLabel)); /* Short label displayed on left */
json_object_object_add(t, "long_label", json_object_new_string(tdb->longLabel)); /* Long label displayed in middle */
url = replaceChars(tdb->url, "$$", TERM_VAR);
json_object_object_add(t, "url_item", strlen(url) ? json_object_new_string(url) : NULL); /* URL to link to when they click on an item */
freez(&url);
addTrackUrl(t, "url_self", genome, tdb->tableName);
json_object_object_add(t, "parent_track", tdb->parentName ? json_object_new_string(tdb->parentName) : NULL); /* set if this is a supertrack member */
return t;
}

static void jsonAddTableInfoOneTrack(struct json_object *o, struct hTableInfo *hti)
// Add table info such as columns names 
{
if (!hti)
    return;
json_object_object_add(o, "rootName", json_object_new_string(hti->rootName));
json_object_object_add(o, "isPos", json_object_new_boolean(hti->isPos));
json_object_object_add(o, "isSplit", json_object_new_boolean(hti->isSplit));
json_object_object_add(o, "hasBin", json_object_new_boolean(hti->hasBin));
json_object_object_add(o, "chromField", json_object_new_string(hti->chromField));
json_object_object_add(o, "startField", json_object_new_string(hti->startField));
json_object_object_add(o, "endField", json_object_new_string(hti->endField));
json_object_object_add(o, "nameField", json_object_new_string(hti->nameField));
json_object_object_add(o, "scoreField", json_object_new_string(hti->scoreField));
json_object_object_add(o, "strandField", json_object_new_string(hti->strandField));
json_object_object_add(o, "cdsStartField", json_object_new_string(hti->cdsStartField));
json_object_object_add(o, "cdsEndField", json_object_new_string(hti->cdsEndField));
json_object_object_add(o, "countField", json_object_new_string(hti->countField));
json_object_object_add(o, "startsField", json_object_new_string(hti->startsField));
json_object_object_add(o, "endsSizesField", json_object_new_string(hti->endsSizesField));
json_object_object_add(o, "spanField", json_object_new_string(hti->spanField));
json_object_object_add(o, "hasCDS", json_object_new_boolean(hti->hasCDS));
json_object_object_add(o, "hasBlocks", json_object_new_boolean(hti->hasBlocks));
json_object_object_add(o, "type", json_object_new_string(hti->type));
}

static void jsonAddOneTrackFull(struct json_object *arr, struct trackDb *tdb, struct hTableInfo *hti, char *genome)
// Detailed information for a single track added to the array 'arr'
{
struct json_object *item = json_object_new_object();
struct json_object *view = json_object_new_object();
struct json_object *info = json_object_new_object();
struct json_object *t = jsonOneTrack(tdb, genome);
struct json_object *restList = json_object_new_array();
int i;
json_object_array_add(arr, item); /* add this item to the array */
json_object_object_add(item, tdb->tableName, t); 
json_object_object_add(t, "view_properties", view); 
json_object_object_add(t, "table_properties", info); 
json_object_object_add(t, "description_html", json_object_new_string(tdb->html)); /* Some html to display about the track */
// properties of the view
json_object_object_add(view, "color_r", json_object_new_int(tdb->colorR)); /* Color red component 0-255 */
json_object_object_add(view, "color_g", json_object_new_int(tdb->colorG)); /* Color green component 0-255 */
json_object_object_add(view, "color_b", json_object_new_int(tdb->colorB)); /* Color blue component 0-255 */
json_object_object_add(view, "alt_color_r", json_object_new_int(tdb->altColorR)); /* Light color red component 0-255 */
json_object_object_add(view, "alt_color_g", json_object_new_int(tdb->altColorG)); /* Light color green component 0-255 */
json_object_object_add(view, "alt_color_b", json_object_new_int(tdb->altColorB)); /* Light color blue component 0-255 */
json_object_object_add(view, "use_score", json_object_new_boolean(tdb->useScore)); /* true if use score, false if not */
// tdb->settings crashes the json print output for some reason to do with embedded '%s'.
// need to instead figure out how to get all the individual settings from settings hash, if we care. 
json_object_object_add(view, "settings", json_object_new_string("")); //json_object_new_string(tdb->settings ? tdb->settings : "")); /* Name/value pairs for track-specific stuff */
//    struct hash *settingsHash;  /* Hash for settings. Not saved in database.  Don't use directly, rely on trackDbSetting to access. */
json_object_object_add(view, "restrict_count", json_object_new_int(tdb->restrictCount)); /* Number of chromosomes this is on (0=all though!) */
json_object_object_add(view, "restrict_list", restList); /* List of chromosomes this is on ([]=all though!) */
for (i = 0; i < tdb->restrictCount ; ++i)
    json_object_array_add(restList, json_object_new_string(tdb->restrictList[i]));
// Properties of the table
if (hti)
    {
    jsonAddTableInfoOneTrack(info, hti);
    }
}

static void jsonAddTracks(struct json_object *arr, struct trackDb *tdb, char *genome)
// Add brief information for every track to the array 'arr'
//   Format: [{ _track_name_ => {_track_details_},... }]
{
struct json_object *item;
for ( ; tdb ; tdb = tdb->next)
    {
    item = json_object_new_object();
    json_object_object_add(item, tdb->tableName, jsonOneTrack(tdb, genome));
    json_object_array_add(arr, item);
    }
}

static void jsonAddGroups(struct json_object *grp, struct trackDb *tdb)
// Add groups of tracks into the groups 'grp':
//    g = { {_group_ : [ {_track_: _priority}, ... ]}, ...}
{
struct trackDb *t;
struct json_object *trackArr;
//struct json_object *track;
struct hashEl *el;
struct hash *hGrp = newHash(0);
for (t = tdb ; t ; t = t->next)
    {
    if (!t->grp)
	errAbort("no group in trackDb for table %s\n", t->tableName);
    el = hashStore(hGrp, t->grp);
    if (!el->val) 
	{
	// put a track array as a value for the group 
        // put the group into the 'grp' container 
	trackArr = json_object_new_array();
	json_object_object_add(grp, t->grp, trackArr);
	el->val = trackArr;
	}
    else
	trackArr = (struct json_object *)el->val;
    // put the track and priority into the track array for this group
    //track = json_object_new_object();
    //json_object_object_add(track, t->tableName, json_object_new_double(t->priority));
    json_object_array_add(trackArr, json_object_new_string(t->tableName));
    }
freeHash(&hGrp);
}

void printTrackInfo(char *genome, struct trackDb *tdb, struct hTableInfo *hti, time_t modified)
// Print genome and track information for the genome 
// If only one track is specified, print full details including html description page
// 
// tracks => {_genome_ => [{ _track_name_ => {_track_details_},... }] }
// groups => {_genome_ => [{_group_name_ => [{_track_name_: _track_priority_},... ]}] }
{
struct json_object *i         = json_object_new_object();
struct json_object *trackGen  = json_object_new_object();
struct json_object *trackArr  = json_object_new_array();
struct json_object *groupGen  = json_object_new_object();
struct json_object *group     = json_object_new_object();
// add a genome object under the main 'tracks' and 'genome' objects
// add tracks and groups under their genome object
addTrackUrl(i, "url_self", genome, NULL);
addTrackUrl(i, "url_tracks", NULL, NULL);
addGenomeUrl(i, "url_genomes", NULL, NULL);
addGenomeUrl(i, "url_genome", genome, NULL);
json_object_object_add(i, "tracks", trackGen);
json_object_object_add(i, "groups", groupGen);
json_object_object_add(trackGen, genome, trackArr);
json_object_object_add(groupGen, genome, group);
// add tracks to the track array
if (tdb && slCount(tdb)==1)
    {
/*    addCountChromUrl(i, "url_count_chrom", genome, tdb->tableName);
    addCountUrl(i, "url_count", genome, tdb->tableName);
    addRangeChromUrl(i, "url_range_chrom", genome, tdb->tableName);
    addRangeUrl(i, "url_range", genome, tdb->tableName);*/
    jsonAddOneTrackFull(trackArr, tdb, hti, genome);
    }
else
    jsonAddTracks(trackArr, tdb, genome);
jsonAddGroups(group, tdb);
okSendHeader(modified, TRACK_EXPIRES);
printf(json_object_to_json_string(i));
json_object_put(i);
}

void printItemAsAnnoj(char *db, char *track, char *type, char *term)
// print out a description for a track item
{
char buf[2048];
struct json_object *item = json_object_new_object();
json_object_object_add(item, "success", json_object_new_boolean(1));
json_object_object_add(item, "assembly", json_object_new_string(""));
json_object_object_add(item, "start", json_object_new_int(0));
json_object_object_add(item, "end", json_object_new_int(0));
safef(buf, sizeof(buf), "%s <a target=\"_blank\" href=\"http://www.google.com/search?q=%s\">Google</a> <a href=\"http://genome.ucsc.edu/cgi-bin/hgGene?hgg_gene=%s&amp;hgg_type=%s&amp;db=%s\">UCSC Genome Browser</a>", term, term, term, track, db);
json_object_object_add(item, "description", json_object_new_string(buf));
printf(json_object_to_json_string(item));
}

void printItem(char *db, char *track, char *type, char *term)
// print out a description for a track item
{
printItemAsAnnoj(db, track, type, term);
}
