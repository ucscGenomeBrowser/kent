/* renderMain - execute enough of browser to render list of tracks without touching cart or
 * writing form. . */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "options.h"
#include "errabort.h"
#include "portable.h"
#include "cheapcgi.h"
#include "ra.h"
#include "hdb.h"
#include "hgTracks.h"
#include "imageV2.h"

static void usage()
/* Print out usage and exit - just temporary. */
{
errAbort(
"hgRenderTracks - execute enough of browser to render a list of tracks\n"
"usage:\n"
"      hgTrackRender track.ra cart.ra output.ps\n"
"where track.ra is a trackDb.ra entry flattened out to include stuff inherited from parent\n"
"and cart.ra is a file with the cart settings.  Both .ra files should just have a single stanza\n"
);
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct trackDb *hTrackDbForTrackAndAncestors(char *db, char *track);
/* Load trackDb object for a track. If need be grab its ancestors too. 
 * This does not load children. hTrackDbForTrack will handle children, and
 * is actually faster if being called on lots of tracks.  This function
 * though is faster on one or two tracks. */

static void hashIntoHash(struct hash *newStuff, struct hash *hash)
/* Add newStuff into hash. */
{
struct hashCookie cookie = hashFirst(newStuff);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    hashAdd(hash, hel->name, hel->val);
}

struct track *trackFromSettingsHash(struct hash *settings)
/* Wrap a trackDb, and then a track around settings, and return track. */
{
char *trackName = hashMustFindVal(settings, "track");
struct trackDb *tdb = hTrackDbForTrackAndAncestors(database, trackName);
hashIntoHash(settings, tdb->settingsHash);
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

verboseTime(2, "Before load %d tracks", slCount(trackList));
/* Prepare track list for drawing. */
for (track = trackList; track != NULL; track = track->next)
    {
    track->loadItems(track);
    }
verboseTime(2, "After load");

/* Initialize global image box. */
int sideSliceWidth  = 0;   // Just being explicit
if (withLeftLabels)
    sideSliceWidth   = (insideX - gfxBorder*3) + 2;
theImgBox = imgBoxStart(database,chromName,winStart,winEnd,(!revCmplDisp),sideSliceWidth,tl.picWidth);
makeActiveImage(trackList, NULL);
verboseTime(2,"After makeActiveImage");
}

int main(int argc, char *argv[])
{
if(argc == 1)
    {
    // CGI call

    // htmlPushEarlyHandlers(); XXXX do I need to do this?

    hPrintDisable();
    oldVars = hashNew(10);
    struct cart *cart = cartForSession(hUserCookie(), excludeVars, oldVars);

    // setup approriate CGI variables which tell hgTracks code what to do.
    cartSetBoolean(cart, "hgt.trackImgOnly", TRUE);
    if(cartVarExists(cart, "jsonp"))
        {
        cartSetString(cart, "hgt.contentType", "jsonp");
        cartSetString(cart, "hgt.trackNameFilter", cartString(cart, "track"));
        }
    else
        {
        cartSetString(cart, "hgt.contentType", "png");
        cartSetBoolean(cart, "hgt.imageV1", TRUE);
        }
    doMiddle(cart);
    }
else
    {
    // XXXX remove this code ... well, maybe not - this still might be useful for a stand-alone renderer.

    // command line call

    /* Set up some timing since we're trying to optimize things very often. */
    long enteredMainTime = clock1000();
    verboseTimeInit();
    /* Push very early error handling - this is just
     * for the benefit of the cgiVarExists, which
     * somehow can't be moved effectively into doMiddle. */
    // htmlPushEarlyHandlers();

    /* Set up cgi vars from command line. */
    // cgiSpoof(&argc, argv);
    optionInit(&argc, argv, options);

    if (argc != 4)
        usage();

    hgTrackRenderFromCommandLine(argv[1], argv[2], argv[3]);

    verbose(2, "Overall total time: %ld millis<BR>\n", clock1000() - enteredMainTime);
    }
return 0;
}
