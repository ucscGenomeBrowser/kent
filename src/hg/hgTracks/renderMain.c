/* hgTrackRender - execute enough of browser to render one track. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "errabort.h"
#include "portable.h"
#include "cheapcgi.h"
#include "ra.h"
#include "hdb.h"
#include "hgTracks.h"

static void usage()
/* Print out usage and exit - just temporary. */
{
errAbort(
"hgTrackRender - execute enough of browser to render one track.\n"
"usage:\n"
"      hgTrackRender track.ra cart.ra output.ps\n"
"where track.ra is a trackDb.ra entry flattened out to include stuff inherited from parent\n"
"and cart.ra is a file with the cart settings.  Both .ra files should just have a single stanza\n"
);
}

struct track *trackFromSettingsHash(struct hash *settings)
/* Wrap a trackDb, and then a track around settings, and return track. */
{
struct trackDb *tdb = trackDbNew();
tdb->settingsHash = settings;
tdb->track = hashMustFindVal(settings, "track");
trackDbFieldsFromSettings(tdb);
trackDbAddTableField(tdb);
return trackFromTrackDb(tdb);
}

static void hgTrackRenderFromCommandLine(char *trackFile, char *cartFile, char *outFile)
/* Generate tracks from trackFile and cart from cartFile, both in .ra format.  Then
 * call makeActiveImage, the heart of the genome browser.*/
{
struct track *track, *trackList = NULL;

/* Load in cart file into a single hash and then wrap a cart around it. 
 * We need to set cart before we can make tracks. */
struct hash *cartHash = raReadSingle(cartFile);
cart = cartFromHash(cartHash);
database = hashMustFindVal(cartHash, "db");
position = cloneString(hashMustFindVal(cartHash, "position"));
if (!hgParseChromRange(NULL, position, &chromName, &winStart, &winEnd))
    errAbort("position not in chrom:start-end format");

uglyf("Got %d vars in %s\n", cartHash->elCount, cartFile);

/* Initialize layout. */
initTl();
setLayoutGlobals();

/* Make list of tracks out of track file. */
struct lineFile *lf = lineFileOpen(trackFile, TRUE);
struct hash *trackRa;
while ((trackRa = raNextRecord(lf)) != NULL)
    {
    track = trackFromSettingsHash(trackRa);
    slAddHead(&trackList, track);
    }
slReverse(&trackList);
lineFileClose(&lf);
uglyf("Got %d tracks in %s\n", slCount(trackList), trackFile);

/* Prepare track list for drawing. */
for (track = trackList; track != NULL; track = track->next)
    {
    track->loadItems(track);
    uglyf("%s has %d items\n", track->track, slCount(track->items));
    }
makeActiveImage(trackList, outFile);
}

int main(int argc, char *argv[])
{
/* Set up some timing since we're trying to optimize things very often. */
long enteredMainTime = clock1000();
uglyTime(NULL);
measureTiming = TRUE;

/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which
 * somehow can't be moved effectively into doMiddle. */
// htmlPushEarlyHandlers();

/* Set up cgi vars from command line. */
cgiSpoof(&argc, argv);

if (argc != 4)
    usage();

hgTrackRenderFromCommandLine(argv[1], argv[2], argv[3]);

if (measureTiming)
    {
    fprintf(stdout, "Overall total time: %ld millis<BR>\n",
    clock1000() - enteredMainTime);
    }
return 0;
}
