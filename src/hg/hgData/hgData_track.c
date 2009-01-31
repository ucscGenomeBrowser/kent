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

static char const rcsid[] = "$Id: hgData_track.c,v 1.1.2.2 2009/01/31 05:15:36 mikep Exp $";

void printTrackDbAsAnnoj(struct trackDb *tdb)
{
//struct trackDb *t;

}

static void printTrack(struct trackDb *tdb)
// print short header for one track (no 'html' desctiption)
{
printf("{\"tableName\": %s,", quote(tdb->tableName));
printf("\"type\": %s,", quote(tdb->type));
printf("\"parentTable\": %s,", quote(tdb->parentName));
printf("\"shortLabel\": %s,", quote(tdb->shortLabel));
printf("\"longLabel\": %s,", quote(tdb->longLabel));
printf("\"grp\": %s}", quote(tdb->grp));
}

static void printTracks(struct trackDb *tdb)
// print an array of tracks
{
struct trackDb *t;
printf("\"tracks\" : [\n");
for (t = tdb ; t ; t = t->next)
    {
    printTrack(t);
    printf("%c\n", t->next ? ',' : ' ');
    }
printf("]");
}

void printTrackInfo(char *db, char *track, struct trackDb *tdb)
// print database and track information
{
printf("{\"db\": %s,\n", quote(db));
if (track)
    printf("\"track\": %s,\n", quote(track));
printTracks(tdb);
printf("}\n");
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

