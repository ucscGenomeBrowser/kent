/* hgData_track - functions dealing with tracks. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_track.c,v 1.1.2.1 2009/01/13 15:48:54 mikep Exp $";

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

