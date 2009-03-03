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

static char const rcsid[] = "$Id: hgData_track.c,v 1.1.2.15 2009/03/03 07:44:28 mikep Exp $";

// /tracks                                            [list of all tracks in all genomes]
// /tracks/{genome}                                   [list of tracks for {genome}]
// /tracks/{track}/{genome}                           [track details for {track} in {genome}]


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
	safef(url, sizeof(url), PREFIX TRACKS_CMD "/%s/%s", genome, track);
    else
	safef(url, sizeof(url), PREFIX TRACKS_CMD "/%s", genome);
    }
  else
    {
    safef(url, sizeof(url), PREFIX TRACKS_CMD "%s", "");
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

static void jsonTableAddField(struct json_object *a, char *name, char *field)
{
if (field && strlen(field))
    {
    struct json_object *f = json_object_new_object();
    json_object_object_add(f, name, json_object_new_string(field));
    json_object_array_add(a, f);
    }
}

void jsonAddTableInfoOneTrack(struct json_object *o, struct hTableInfo *hti)
// Add table info such as columns names keyed off 'table_properties'
{
struct json_object *p = json_object_new_object();
struct json_object *f = json_object_new_array();
json_object_object_add(o, "table_properties", p);
json_object_object_add(p, "fields", f);
if (!hti)
    return;
json_object_object_add(p, "rootName", json_object_new_string(hti->rootName));
json_object_object_add(p, "isPos", json_object_new_boolean(hti->isPos));
json_object_object_add(p, "isSplit", json_object_new_boolean(hti->isSplit));
json_object_object_add(p, "hasBin", json_object_new_boolean(hti->hasBin));
json_object_object_add(p, "hasCDS", json_object_new_boolean(hti->hasCDS));
json_object_object_add(p, "hasBlocks", json_object_new_boolean(hti->hasBlocks));
json_object_object_add(p, "type", json_object_new_string(hti->type));
//jsonTableAddField(f, "chromField", hti->chromField); // queries are by chrom only
jsonTableAddField(f, "startField", hti->startField);
jsonTableAddField(f, "endField", hti->endField);
jsonTableAddField(f, "nameField", hti->nameField);
jsonTableAddField(f, "scoreField", hti->scoreField);
jsonTableAddField(f, "strandField", hti->strandField);
jsonTableAddField(f, "cdsStartField", hti->cdsStartField);
jsonTableAddField(f, "cdsEndField", hti->cdsEndField);
jsonTableAddField(f, "countField", hti->countField);
jsonTableAddField(f, "startsField", hti->startsField);
jsonTableAddField(f, "endsSizesField", hti->endsSizesField);
jsonTableAddField(f, "spanField", hti->spanField);
}

static void jsonAddViewInfoOneTrack(struct json_object *o, struct trackDb *tdb)
// Add view info keyed off 'view_properties'
{
int i;
struct json_object *view = json_object_new_object();
struct json_object *restList = json_object_new_array();
json_object_object_add(o, "view_properties", view); 
if (!tdb)
    return;
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
}

static void jsonAddOneTrackFull(struct json_object *arr, struct trackDb *tdb, struct hTableInfo *hti, char *genome)
// Detailed information for a single track added to the array 'arr'
{
struct json_object *item = json_object_new_object();
struct json_object *t = jsonOneTrack(tdb, genome);
json_object_array_add(arr, item); /* add this item to the array */
json_object_object_add(item, tdb->tableName, t); 
json_object_object_add(t, "description_html", json_object_new_string(tdb->html));
jsonAddViewInfoOneTrack(t, tdb);
jsonAddTableInfoOneTrack(t, hti);
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

void printTrackInfo(char *genome, char *track, struct trackDb *tdb, struct hTableInfo *hti, time_t modified)
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
if (*track)
    {
    addTrackUrl(i, "url_self", genome, tdb->tableName);
    addCountUrl(i, "url_count", tdb->tableName, genome, NULL, -1, -1);
    addCountChromUrl(i, "url_count_chrom", tdb->tableName, genome, NULL);
    addRangeUrls(i, tdb->tableName, genome, NULL, -1, -1);
    }
else
    {
    addTrackUrl(i, "url_self", genome, NULL);
    }
addTrackUrl(i, "url_track_list", genome, NULL);
addGenomeUrl(i, "url_genome_list", NULL, NULL);
addGenomeUrl(i, "url_genome_chroms", genome, NULL);
// add a genome object under the main 'tracks' and 'genome' objects
// add tracks and groups under their genome object
json_object_object_add(i,"track_count", json_object_new_int(slCount(tdb)));
json_object_object_add(i, "tracks", trackGen);
json_object_object_add(i, "groups", groupGen);
json_object_object_add(trackGen, genome, trackArr);
json_object_object_add(groupGen, genome, group);
// add tracks to the track array
if (*track)
    jsonAddOneTrackFull(trackArr, tdb, hti, genome);
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
