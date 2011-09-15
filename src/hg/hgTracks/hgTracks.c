/* hgTracks - the original, and still the largest module for the UCSC Human Genome
 * Browser main cgi script.  Currently contains most of the track framework, though
 * there's quite a bit of other framework type code in simpleTracks.c.  The main
 * routine got moved to create a new entry point to the bulk of the code for the
 * hgRenderTracks web service.  See mainMain.c for the main used by the hgTracks CGI. */

#include <pthread.h>
#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "portable.h"
#include "memalloc.h"
#include "localmem.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "gfxPoly.h"
#include "memgfx.h"
#include "hvGfx.h"
#include "psGfx.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hgFind.h"
#include "hgTracks.h"
#include "trashDir.h"
#include "grp.h"
#include "versionInfo.h"
#include "web.h"
#include "cds.h"
#include "cutterTrack.h"
#include "wikiTrack.h"
#include "ctgPos.h"
#include "bed.h"
#include "bigBed.h"
#include "bigWig.h"
#include "bedCart.h"
#include "udc.h"
#include "customTrack.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "cytoBand.h"
#include "ensFace.h"
#include "liftOver.h"
#include "pcrResult.h"
#include "wikiLink.h"
#include "jsHelper.h"
#include "mafTrack.h"
#include "hgConfig.h"
#include "encode.h"
#include "agpFrag.h"
#include "imageV2.h"
#include "suggest.h"
#include "search.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: doMiddle.c,v 1.1651 2010/06/11 17:53:06 larrym Exp $";

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "dirty", "hgt.reset",
            "hgt.in1", "hgt.in2", "hgt.in3", "hgt.inBase",
            "hgt.out1", "hgt.out2", "hgt.out3",
            "hgt.left1", "hgt.left2", "hgt.left3",
            "hgt.right1", "hgt.right2", "hgt.right3",
            "hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
            "hgt.tui", "hgt.hideAll", "hgt.visAllFromCt",
	    "hgt.psOutput", "hideControls", "hgt.toggleRevCmplDisp",
	    "hgt.collapseGroups", "hgt.expandGroups", "hgt.suggest",
	    "hgt.jump", "hgt.refresh", "hgt.setWidth",
            "hgt.trackImgOnly", "hgt.ideogramToo", "hgt.trackNameFilter", "hgt.imageV1", "hgt.suggestTrack", "hgt.setWidth",
             TRACK_SEARCH,         TRACK_SEARCH_ADD_ROW,     TRACK_SEARCH_DEL_ROW, TRACK_SEARCH_PAGER,
            "hgt.contentType",
            NULL };

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
boolean baseShowPos;           /* TRUE if should display full position at top of base track */
boolean baseShowAsm;           /* TRUE if should display assembly info at top of base track */
boolean baseShowScaleBar;      /* TRUE if should display scale bar at very top of base track */
boolean baseShowRuler;         /* TRUE if should display the basic ruler in the base track (default) */
char *baseTitle = NULL;        /* Title it should display top of base track (optional)*/
static char *userSeqString = NULL;  /* User sequence .fa/.psl file. */

/* These variables are set by getPositionFromCustomTracks() at the very
 * beginning of tracksDisplay(), and then used by loadCustomTracks(). */
char *ctFileName = NULL;    /* Custom track file. */
struct customTrack *ctList = NULL;  /* Custom tracks. */
boolean hasCustomTracks = FALSE;  /* whether any custom tracks are for this db*/
struct slName *browserLines = NULL; /* Custom track "browser" lines. */

boolean withNextItemArrows = FALSE; /* Display next feature (gene) navigation buttons near center labels? */
boolean withPriorityOverride = FALSE;   /* Display priority for each track to allow reordering */

int gfxBorder = hgDefaultGfxBorder; /* Width of graphics border. */
int guidelineSpacing = 12;  /* Pixels between guidelines. */

boolean withIdeogram = TRUE;            /* Display chromosome ideogram? */

int rulerMode = tvHide;         /* on, off, full */
struct hvGfx *hvgSide = NULL;     // An extra pointer to a side label image that can be built if needed

char *rulerMenu[] =
/* dropdown for ruler visibility */
    {
    "hide",
    "dense",
    "full"
    };

char *protDbName;               /* Name of proteome database for this genome. */
#define MAX_CONTROL_COLUMNS 6
#define LOW 1
#define MEDIUM 2
#define BRIGHT 3
#define MAXCHAINS 50000000
boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
int imagePixelHeight = 0;
boolean dragZooming = TRUE;
struct hash *oldVars = NULL;
struct jsonHashElement *jsonForClient = NULL;

boolean hideControls = FALSE;		/* Hide all controls? */
boolean trackImgOnly = FALSE;           /* caller wants just the track image and track table html */
boolean ideogramToo =  FALSE;           /* caller wants the ideoGram (when requesting just one track) */

/* Structure returned from findGenomePos.
 * We use this to to expand any tracks to full
 * that were found to contain the searched-upon
 * position string */
struct hgPositions *hgp = NULL;

/* Other global variables. */
struct trackHub *hubList = NULL;	/* List of all relevant hubs. */
struct group *groupList = NULL;    /* List of all tracks. */
char *browserName;              /* Test, preview, or public browser */
char *organization;             /* UCSC */

struct hash *trackHash = NULL; /* Hash of the tracks by their name. */

#ifdef DEBUG
void uglySnoopTrackList(int depth, struct track *trackList)
/* Print out some info on track list. */
{
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (stringIn("FaireH1h", track->track))
	{
	repeatCharOut(uglyOut, '+', depth);
        uglyf("%s pri=%g defPri=%g<BR>\n", track->track, track->priority, track->defaultPriority);
	}
    uglySnoopTrackList(depth+1, track->subtracks);
    }
}
#endif /* DEBUG */

struct track *trackFindByName(struct track *tracks, char *trackName)
/* find a track in tracks by name, recursively searching subtracks */
{
struct track *track;
for (track = tracks; track != NULL; track = track->next)
    {
    if (sameString(track->track, trackName))
        return track;
    else if (track->subtracks != NULL)
        {
        struct track *st = trackFindByName(track->subtracks, trackName);
        if (st != NULL)
            return st;
        }
    }
return NULL;
}

int tgCmpPriority(const void *va, const void *vb)
/* Compare to sort based on priority; use shortLabel as secondary sort key. */
{
const struct track *a = *((struct track **)va);
const struct track *b = *((struct track **)vb);
float dif = a->group->priority - b->group->priority;

if (dif == 0)
    dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
    /* secondary sort on label */
    return strcasecmp(a->shortLabel, b->shortLabel);
else
   return 1;
}

int trackRefCmpPriority(const void *va, const void *vb)
/* Compare based on priority. */
{
const struct trackRef *a = *((struct trackRef **)va);
const struct trackRef *b = *((struct trackRef **)vb);
return tgCmpPriority(&a->track, &b->track);
}

int gCmpPriority(const void *va, const void *vb)
/* Compare groups based on priority. */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
float dif = a->priority - b->priority;

if (dif == 0)
    return 0;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

void changeTrackVis(struct group *groupList, char *groupTarget, int changeVis)
/* Change track visibilities. If groupTarget is
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc.
 * If we are going back to default visibility, then reset the track
 * ordering also. */
{
struct group *group;
if (changeVis == -2)
    return;
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;
    if (groupTarget == NULL || sameString(group->name,groupTarget))
        {
        static char pname[512];
        /* if default vis then reset group priority */
        if (changeVis == -1)
            group->priority = group->defaultPriority;
    for (tr = group->trackList; tr != NULL; tr = tr->next)
        {
        struct track *track = tr->track;
	struct trackDb *tdb = track->tdb;
        if (changeVis == -1) // to default
                {
                if(tdbIsComposite(tdb))
                    {
                    safef(pname, sizeof(pname),"%s.*",track->track); //to remove all settings associated with this composite!
                    cartRemoveLike(cart,pname);
                    struct track *subTrack;
                    for(subTrack = track->subtracks;subTrack != NULL; subTrack = subTrack->next)
                        {
                        subTrack->visibility = tdb->visibility;
                        cartRemove(cart, subTrack->track);
                        }
                    }

                /* restore defaults */
                if (tdbIsSuperTrackChild(tdb) || tdbIsCompositeChild(tdb))
                    {
                    assert(tdb->parent != NULL && tdb->parent->track);
                    cartRemove(cart, tdb->parent->track);
                    if (withPriorityOverride)
                        {
                        safef(pname, sizeof(pname), "%s.priority",tdb->parent->track);
                        cartRemove(cart, pname);
                        }
                    }

                track->visibility = tdb->visibility;
                cartRemove(cart, track->track);

                /* set the track priority back to the default value */
                if (withPriorityOverride)
                    {
                    safef(pname, sizeof(pname), "%s.priority",track->track);
                    cartRemove(cart, pname);
                    track->priority = track->defaultPriority;
                    }
                }
            else // to changeVis value (Usually tvHide)
                {
                /* change to specified visibility */
                if (tdbIsSuperTrackChild(tdb))
                    {
                    assert(tdb->parent != NULL);
                    /* Leave supertrack members alone -- only change parent */
                    struct trackDb *parentTdb = tdb->parent;
                    if ((changeVis == tvHide && !parentTdb->isShow) ||
                        (changeVis != tvHide && parentTdb->isShow))
                        {
                        /* remove if setting to default vis */
                        cartRemove(cart, parentTdb->track);
                        }
                    else
                        cartSetString(cart, parentTdb->track,
                                    changeVis == tvHide ? "hide" : "show");
                    }
                else // Not super  child
                    {
                    if (changeVis == tdb->visibility)
                        /* remove if setting to default vis */
                        cartRemove(cart, track->track);
                    else
                        cartSetString(cart, track->track, hStringFromTv(changeVis));
                    track->visibility = changeVis;
                    }

                // Whether super child or not, if its a composite, then handle the children
                if (tdbIsComposite(tdb))
                    {
                    struct track *subtrack;
                    for(subtrack=track->subtracks;subtrack!=NULL;subtrack=subtrack->next)
                        {
                        if (changeVis == tvHide)
                            cartRemove(cart, subtrack->track); // Since subtrack level vis is an override, simply remove it to hide it
                        else
                            cartSetString(cart, subtrack->track, hStringFromTv(changeVis));
                        subtrack->visibility = changeVis;
                        }
                    }
                }
            }
        }
    }
slSort(&groupList, gCmpPriority);
}

int trackOffsetX()
/* Return x offset where track display proper begins. */
{
int x = gfxBorder;
if (withLeftLabels)
    x += tl.leftLabelWidth + gfxBorder;
return x;
}


static void mapBoxTrackUi(struct hvGfx *hvg, int x, int y, int width,
              int height, char *name, char *shortLabel, char *id)
/* Print out image map rectangle that invokes hgTrackUi. */
{
x = hvGfxAdjXW(hvg, x, width);
char *url = trackUrl(name, chromName);

if(theImgBox && curImgTrack)
    {
    struct imgSlice *curSlice = imgTrackSliceGetByType(curImgTrack,stButton);
    if(curSlice)
        sliceAddLink(curSlice,url,shortLabel);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
    hPrintf("HREF=\"%s\"", url);
    mapStatusMessage("%s controls", shortLabel);
    hPrintf(">\n");
    }
freeMem(url);
}

static void mapBoxToggleComplement(struct hvGfx *hvg, int x, int y, int width, int height,
    struct track *toggleGroup, char *chrom,
    int start, int end, char *message)
/*print out a box along the DNA bases that toggles a cart variable
 * "complement" to complement the DNA bases at the top by the ruler*/
{
struct dyString *ui = uiStateUrlPart(toggleGroup);
x = hvGfxAdjXW(hvg, x, width);
if(theImgBox && curImgTrack)
    {
    char link[512];
    safef(link,sizeof(link),"%s?complement_%s=%d&%s",
        hgTracksName(), database, !cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE),ui->string);
    imgTrackAddMapItem(curImgTrack,link,(char *)(message != NULL?message:NULL),x, y, x+width, y+height, NULL);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
    hPrintf("HREF=\"%s?complement_%s=%d",
        hgTracksName(), database, !cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE));
    hPrintf("&%s\"", ui->string);
    freeDyString(&ui);
    if (message != NULL)
        mapStatusMessage("%s", message);
    hPrintf(">\n");
    }
}

char *trackUrl(char *mapName, char *chromName)
/* Return hgTrackUi url; chromName is optional. */
{
char *encodedMapName = cgiEncode(mapName);
char buf[2048];
if(chromName == NULL)
    safef(buf, sizeof(buf), "%s?%s=%u&g=%s", hgTrackUiName(), cartSessionVarName(), cartSessionId(cart), encodedMapName);
else
    safef(buf, sizeof(buf), "%s?%s=%u&c=%s&g=%s", hgTrackUiName(), cartSessionVarName(), cartSessionId(cart), chromName, encodedMapName);
freeMem(encodedMapName);
return(cloneString(buf));
}

#ifdef REMOTE_TRACK_AJAX_CALLBACK
static boolean trackUsesRemoteData(struct track *track)
/* returns TRUE is this track has a remote datasource */
{
if (!IS_KNOWN(track->remoteDataSource))
    {
    SET_TO_NO(track->remoteDataSource);
    //if (track->bbiFile != NULL)   // FIXME: Chicken or the egg.  bigWig/bigBed "bbiFile" filled in by loadItems, but we don't want to load items.
    //    {
    //    if (!startsWith("/gbdb/",track->bbiFile->fileName))
    //        SET_TO_YES(track->remoteDataSource);
    //    }
    if (startsWithWord("bigWig",track->tdb->type) || startsWithWord("bigBed",track->tdb->type) ||
	startsWithWord("bam",track->tdb->type) || startsWithWord("vcfTabix", track->tdb->type))
        {
        SET_TO_YES(track->remoteDataSource);
        }
    }
return IS_YES(track->remoteDataSource);
}

boolean trackShouldUseAjaxRetrieval(struct track *track)
/* Tracks with remote data sources should berendered via an ajax callback */
{
return (theImgBox && !trackImgOnly && trackUsesRemoteData(track));
}
#endif///def REMOTE_TRACK_AJAX_CALLBACK

static int trackPlusLabelHeight(struct track *track, int fontHeight)
/* Return the sum of heights of items in this track (or subtrack as it may be)
 * and the center label(s) above the items (if any). */
{
if (trackShouldUseAjaxRetrieval(track))
    return REMOTE_TRACK_HEIGHT;

int y = track->totalHeight(track, limitVisibility(track));
if (isCenterLabelIncluded(track))
    y += fontHeight;
if (tdbIsComposite(track->tdb))
    {
    struct track *subtrack;
    for (subtrack = track->subtracks;  subtrack != NULL; subtrack = subtrack->next)
        {
        if (isSubtrackVisible(subtrack) &&  isCenterLabelIncluded(subtrack))
            y += fontHeight;
        }
    }
return y;
}

void drawColoredButtonBox(struct hvGfx *hvg, int x, int y, int w, int h,
                                int enabled, Color shades[])
/* draw button box, providing shades of the desired button color */
{
int light = shades[1], mid = shades[2], dark = shades[4];
if (enabled)
    {
    hvGfxBox(hvg, x, y, w, 1, light);
    hvGfxBox(hvg, x, y+1, 1, h-1, light);
    hvGfxBox(hvg, x+1, y+1, w-2, h-2, mid);
    hvGfxBox(hvg, x+1, y+h-1, w-1, 1, dark);
    hvGfxBox(hvg, x+w-1, y+1, 1, h-1, dark);
    }
else                /* try to make the button look as if
                 * it is already depressed */
    {
    hvGfxBox(hvg, x, y, w, 1, dark);
    hvGfxBox(hvg, x, y+1, 1, h-1, dark);
    hvGfxBox(hvg, x+1, y+1, w-2, h-2, light);
    hvGfxBox(hvg, x+1, y+h-1, w-1, 1, light);
    hvGfxBox(hvg, x+w-1, y+1, 1, h-1, light);
    }
}

void drawGrayButtonBox(struct hvGfx *hvg, int x, int y, int w, int h, int enabled)
/* Draw a gray min-raised looking button. */
{
    drawColoredButtonBox(hvg, x, y, w, h, enabled, shadesOfGray);
}

void drawBlueButtonBox(struct hvGfx *hvg, int x, int y, int w, int h, int enabled)
/* Draw a blue min-raised looking button. */
{
    drawColoredButtonBox(hvg, x, y, w, h, enabled, shadesOfSea);
}

void drawButtonBox(struct hvGfx *hvg, int x, int y, int w, int h, int enabled)
/* Draw a standard (gray) min-raised looking button. */
{
    drawGrayButtonBox(hvg, x, y, w, h, enabled);
}

void beforeFirstPeriod( char *str )
{
char *t = rindex( str, '.' );

if( t == NULL )
    return;
else
    str[strlen(str) - strlen(t)] = '\0';
}

static void drawBases(struct hvGfx *hvg, int x, int y, int width, int height,
                        Color color, MgFont *font, boolean complementSeq,
                        struct dnaSeq *thisSeq)
/* Draw evenly spaced bases. */
{
struct dnaSeq *seq;

if (thisSeq == NULL)
    seq = hDnaFromSeq(database, chromName, winStart, winEnd, dnaUpper);
else
    seq = thisSeq;

if (complementSeq)
    complement(seq->dna, seq->size);
spreadBasesString(hvg, x, y, width, height, color, font,
                                seq->dna, seq->size, FALSE);

if (thisSeq == NULL)
    freeDnaSeq(&seq);
}

void drawComplementArrow( struct hvGfx *hvg, int x, int y,
                                int width, int height, MgFont *font)
/* Draw arrow and create clickbox for complementing ruler bases */
{
boolean baseCmpl = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
// reverse arrow when base complement doesn't match display
char *text =  (baseCmpl == revCmplDisp) ? "--->" : "<---";
hvGfxTextRight(hvg, x, y, width, height, MG_BLACK, font, text);
mapBoxToggleComplement(hvg, x, y, width, height, NULL, chromName, winStart, winEnd,
                       "complement bases");
}

struct track *chromIdeoTrack(struct track *trackList)
/* Find chromosome ideogram track */
{
struct track *track;
for(track = trackList; track != NULL; track = track->next)
    {
    if(sameString(track->track, "cytoBandIdeo"))
	{
	if (hTableExists(database, track->table))
	    return track;
	else
	    return NULL;
	}
    }
return NULL;
}

void removeTrackFromGroup(struct track *track)
/* Remove track from group it is part of. */
{
struct trackRef *tr = NULL;
for(tr = track->group->trackList; tr != NULL; tr = tr->next)
    {
    if(tr->track == track)
	{
	slRemoveEl(&track->group->trackList, tr);
	break;
	}
    }
}


void fillInStartEndBands(struct track *ideoTrack, char *startBand, char *endBand, int buffSize)
/* Loop through the bands and fill in the one that the current window starts
   on and ends on. */
{
struct cytoBand *cb = NULL, *cbList = ideoTrack->items;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    /* If the start or end is encompassed by this band fill
       it in. */
    if(winStart >= cb->chromStart &&
       winStart <= cb->chromEnd)
	{
	safef(startBand, buffSize, "%s", cb->name);
	}
    /* End is > rather than >= due to odditiy in the
       cytoband track where the starts and ends of two
       bands overlaps by one. */
    if(winEnd > cb->chromStart &&
       winEnd <= cb->chromEnd)
	{
	safef(endBand, buffSize, "%s", cb->name);
	}
    }
}

void makeChromIdeoImage(struct track **pTrackList, char *psOutput,
                        struct tempName *ideoTn)
/* Make an ideogram image of the chromsome and our position in it.  If the
 * ideoTn parameter is not NULL, it is filled in if the ideogram is created. */
{
struct track *ideoTrack = NULL;
MgFont *font = tl.font;
char *mapName = "ideoMap";
struct hvGfx *hvg;
boolean doIdeo = TRUE;
boolean ideogramAvail = FALSE;
int ideoWidth = round(.8 *tl.picWidth);
int ideoHeight = 0;
int textWidth = 0;
struct tempName gifTn;
if (ideoTn == NULL)
    ideoTn = &gifTn;   // not returning value

ideoTrack = chromIdeoTrack(*pTrackList);

/* If no ideogram don't draw. */
if(ideoTrack == NULL)
    doIdeo = FALSE;
else if(trackImgOnly && !ideogramToo)
    {
    doIdeo = FALSE;
    }
else
    {
    ideogramAvail = TRUE;
    /* Remove the track from the group and track list. */
    removeTrackFromGroup(ideoTrack);
    slRemoveEl(pTrackList, ideoTrack);

    /* Fix for hide all button hiding the ideogram as well. */
    if(withIdeogram && ideoTrack->items == NULL)
	{
	ideoTrack->visibility = tvDense;
	ideoTrack->loadItems(ideoTrack);
	}
    limitVisibility(ideoTrack);

    /* If hidden don't draw. */
    if(ideoTrack->limitedVis == tvHide || !withIdeogram)
        doIdeo = FALSE;
    }
if(doIdeo)
    {
    char startBand[16];
    char endBand[16];
    char title[32];
    startBand[0] = endBand[0] = '\0';
    fillInStartEndBands(ideoTrack, startBand, endBand, sizeof(startBand));
    /* Start up client side map. */
    if (!psOutput)
        hPrintf("<MAP Name=%s>\n", mapName);
    /* Draw the ideogram. */
    ideoHeight = gfxBorder + ideoTrack->height;
    if (psOutput)
        {
        trashDirFile(ideoTn, "hgtIdeo", "hgtIdeo", ".ps");
        hvg = hvGfxOpenPostScript(ideoWidth, ideoHeight, ideoTn->forCgi);
        }
    else
        {
        trashDirFile(ideoTn, "hgtIdeo", "hgtIdeo", ".png");
        hvg = hvGfxOpenPng(ideoWidth, ideoHeight, ideoTn->forCgi, FALSE);
        }
    hvg->rc = revCmplDisp;
    initColors(hvg);
    ideoTrack->ixColor = hvGfxFindRgb(hvg, &ideoTrack->color);
    ideoTrack->ixAltColor = hvGfxFindRgb(hvg, &ideoTrack->altColor);
    hvGfxSetClip(hvg, 0, gfxBorder, ideoWidth, ideoTrack->height);
    if(sameString(startBand, endBand))
        safef(title, sizeof(title), "%s (%s)", chromName, startBand);
    else
        safef(title, sizeof(title), "%s (%s-%s)", chromName, startBand, endBand);
    textWidth = mgFontStringWidth(font, title);
    hvGfxTextCentered(hvg, 2, gfxBorder, textWidth, ideoTrack->height, MG_BLACK, font, title);
    ideoTrack->drawItems(ideoTrack, winStart, winEnd, hvg, textWidth+4, gfxBorder, ideoWidth-textWidth-4,
             font, ideoTrack->ixColor, ideoTrack->limitedVis);
    hvGfxUnclip(hvg);
    /* Save out picture and tell html file about it. */
    hvGfxClose(&hvg);
    /* Finish map. */
    if (!psOutput)
        hPrintf("</MAP>\n");
    }
hPrintf("<TABLE BORDER=0 CELLPADDING=0>");
if (doIdeo && !psOutput)
    {
    hPrintf("<TR><TD HEIGHT=5></TD></TR>");
    hPrintf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s id='chrom'>",
        ideoTn->forHtml, ideoWidth, ideoHeight, mapName);
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD HEIGHT=5></TD></TR></TABLE>\n");
    }
else
    hPrintf("<TR><TD HEIGHT=10></TD></TR></TABLE>\n");
if(ideoTrack != NULL)
    {
    ideoTrack->limitedVisSet = TRUE;
    ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
    }
}

char *pcrResultMapItemName(struct track *tg, void *item)
/* Stitch accession and display name back together (if necessary). */
{
struct linkedFeatures *lf = item;
return pcrResultItemAccName(lf->name, lf->extra);
}

void pcrResultLoad(struct track *tg)
/* Load locations of primer matches into linkedFeatures items. */
{
char *pslFileName, *primerFileName;
struct targetDb *target;
if (! pcrResultParseCart(database, cart, &pslFileName, &primerFileName, &target))
    return;

/* Don't free psl -- used in drawing phase by baseColor code. */
struct psl *pslList = pslLoadAll(pslFileName), *psl;
struct linkedFeatures *itemList = NULL;
if (target != NULL)
    {
    int rowOffset = hOffsetPastBin(database, chromName, target->pslTable);
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[2048];
    struct psl *tpsl;
    for (tpsl = pslList;  tpsl != NULL;  tpsl = tpsl->next)
	{
	char *itemAcc = pcrResultItemAccession(tpsl->tName);
	char *itemName = pcrResultItemName(tpsl->tName);
	/* Query target->pslTable to get target-to-genomic mapping: */
	safef(query, sizeof(query), "select * from %s where qName = '%s'",
	      target->pslTable, itemAcc);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct psl *gpsl = pslLoad(row+rowOffset);
	    if (sameString(gpsl->tName, chromName) && gpsl->tStart < winEnd && gpsl->tEnd > winStart)
		{
		struct psl *trimmed = pslTrimToQueryRange(gpsl, tpsl->tStart,
				      tpsl->tEnd);
		struct linkedFeatures *lf;
		char *targetStyle = cartUsualString(cart,
		     PCR_RESULT_TARGET_STYLE, PCR_RESULT_TARGET_STYLE_DEFAULT);
		if (sameString(targetStyle, PCR_RESULT_TARGET_STYLE_TALL))
		    {
		    lf = lfFromPslx(gpsl, 1, FALSE, FALSE, tg);
		    lf->tallStart = trimmed->tStart;
		    lf->tallEnd = trimmed->tEnd;
		    }
		else
		    {
		    lf = lfFromPslx(trimmed, 1, FALSE, FALSE, tg);
		    }
		lf->name = cloneString(itemAcc);
		char extraInfo[512];
		safef(extraInfo, sizeof(extraInfo), "%s|%d|%d",
		      (itemName ? itemName : ""), tpsl->tStart, tpsl->tEnd);
		lf->extra = cloneString(extraInfo);
		slAddHead(&itemList, lf);
		}
	    }
	}
    hFreeConn(&conn);
    }
else
    for (psl = pslList;  psl != NULL;  psl = psl->next)
    if (sameString(psl->tName, chromName) && psl->tStart < winEnd && psl->tEnd > winStart)
        {
        struct linkedFeatures *lf =
        lfFromPslx(psl, 1, FALSE, FALSE, tg);
        lf->name = cloneString("");
        lf->extra = cloneString("");
        slAddHead(&itemList, lf);
        }
slSort(&itemList, linkedFeaturesCmp);
tg->items = itemList;
}

char *pcrResultTrackItemName(struct track *tg, void *item)
/* If lf->extra is non-empty, return it (display name for item).
 * Otherwise default to item name. */
{
struct linkedFeatures *lf = item;
char *extra = (char *)lf->extra;
if (isNotEmpty(extra))
    {
    static char displayName[512];
    safecpy(displayName, sizeof(displayName), extra);
    char *ptr = strchr(displayName, '|');
    if (ptr != NULL)
	*ptr = '\0';
    if (isNotEmpty(displayName))
	return displayName;
    }
return lf->name;
}

struct track *pcrResultTg()
/* Make track of hgPcr results (alignments of user's submitted primers). */
{
struct trackDb *tdb = pcrResultFakeTdb();
struct track *tg = trackFromTrackDb(tdb);
tg->loadItems = pcrResultLoad;
tg->itemName = pcrResultTrackItemName;
tg->mapItemName = pcrResultMapItemName;
tg->exonArrows = TRUE;
tg->hasUi = TRUE;
return tg;
}

struct track *linkedFeaturesTg()
/* Return generic track for linked features. */
{
struct track *tg = trackNew();
linkedFeaturesMethods(tg);
tg->colorShades = shadesOfGray;
return tg;
}

void setTgDarkLightColors(struct track *tg, int r, int g, int b)
/* Set track color to r,g,b.  Set altColor to a lighter version
 * of the same. */
{
tg->colorShades = NULL;
tg->color.r = r;
tg->color.g = g;
tg->color.b = b;
tg->altColor.r = (r+255)/2;
tg->altColor.g = (g+255)/2;
tg->altColor.b = (b+255)/2;
}

void parseSs(char *ss, char **retPsl, char **retFa)
/* Parse out ss variable into components. */
{
static char buf[1024];
char *words[2];
int wordCount;

safecpy(buf, sizeof(buf), ss);
wordCount = chopLine(buf, words);
if (wordCount < 2)
    errAbort("Badly formated ss variable");
*retPsl = words[0];
*retFa = words[1];
}

boolean ssFilesExist(char *ss)
/* Return TRUE if both files in ss exist. */
{
char *faFileName, *pslFileName;
parseSs(ss, &pslFileName, &faFileName);
return fileExists(pslFileName) && fileExists(faFileName);
}

void loadUserPsl(struct track *tg)
/* Load up hgBlat results from table into track items. */
{
char *ss = userSeqString;
char buf2[3*512];
char *faFileName, *pslFileName;
struct lineFile *f;
struct psl *psl;
struct linkedFeatures *lfList = NULL, *lf;
enum gfType qt, tt;
int sizeMul = 1;

parseSs(ss, &pslFileName, &faFileName);
pslxFileOpen(pslFileName, &qt, &tt, &f);
if (qt == gftProt)
    {
    setTgDarkLightColors(tg, 0, 80, 150);
    tg->colorShades = NULL;
    sizeMul = 3;
    }
tg->itemName = linkedFeaturesName;
while ((psl = pslNext(f)) != NULL)
    {
    if (sameString(psl->tName, chromName) && psl->tStart < winEnd && psl->tEnd > winStart)
	{
	lf = lfFromPslx(psl, sizeMul, TRUE, FALSE, tg);
	sprintf(buf2, "%s %s", ss, psl->qName);
	lf->extra = cloneString(buf2);
	slAddHead(&lfList, lf);
	/* Don't free psl -- used in drawing phase by baseColor code. */
	}
    else
	pslFree(&psl);
    }
slSort(&lfList, linkedFeaturesCmpStart);
lineFileClose(&f);
tg->items = lfList;
}

static void addUserSeqBaseAndIndelSettings(struct trackDb *tdb)
/* If user sequence is a dna or rna alignment, add settings to enable
 * base-level differences and indel display. */
{
enum gfType qt, tt;
struct lineFile *lf;
char *faFileName, *pslFileName;
parseSs(userSeqString, &pslFileName, &faFileName);
pslxFileOpen(pslFileName, &qt, &tt, &lf);
lineFileClose(&lf);
if (qt != gftProt)
    {
    if (tdb->settingsHash == NULL)
	tdb->settingsHash = hashNew(0);
    hashAdd(tdb->settingsHash, BASE_COLOR_DEFAULT, cloneString("diffBases"));
    hashAdd(tdb->settingsHash, BASE_COLOR_USE_SEQUENCE, cloneString("ss"));
    hashAdd(tdb->settingsHash, SHOW_DIFF_BASES_ALL_SCALES, cloneString("."));
    hashAdd(tdb->settingsHash, INDEL_DOUBLE_INSERT, cloneString("on"));
    hashAdd(tdb->settingsHash, INDEL_QUERY_INSERT, cloneString("on"));
    hashAdd(tdb->settingsHash, INDEL_POLY_A, cloneString("on"));
    }
}

struct track *userPslTg()
/* Make track of user pasted sequence. */
{
struct track *tg = linkedFeaturesTg();
struct trackDb *tdb;
tg->track = "hgUserPsl";
tg->table = tg->track;
tg->canPack = TRUE;
tg->visibility = tvPack;
tg->longLabel = "Your Sequence from Blat Search";
tg->shortLabel = "Blat Sequence";
tg->loadItems = loadUserPsl;
tg->mapItemName = lfMapNameFromExtra;
tg->priority = 100;
tg->defaultPriority = tg->priority;
tg->groupName = "map";
tg->defaultGroupName = cloneString(tg->groupName);
tg->exonArrows = TRUE;

/* better to create the tdb first, then use trackFromTrackDb */
AllocVar(tdb);
tdb->track = cloneString(tg->track);
tdb->table = cloneString(tg->table);
tdb->visibility = tg->visibility;
tdb->shortLabel = cloneString(tg->shortLabel);
tdb->longLabel = cloneString(tg->longLabel);
tdb->grp = cloneString(tg->groupName);
tdb->priority = tg->priority;
tdb->type = cloneString("psl");
trackDbPolish(tdb);
addUserSeqBaseAndIndelSettings(tdb);
tg->tdb = tdb;
return tg;
}

char *oligoMatchSeq()
/* Return sequence for oligo matching. */
{
char *s = cartOptionalString(cart, oligoMatchVar);
if (s != NULL)
    {
    int len;
    tolowers(s);
    dnaFilter(s, s);
    len = strlen(s);
    if (len < 2)
       s = NULL;
    }
if (s == NULL)
    s = cloneString(oligoMatchDefault);
return s;
}

char *oligoMatchName(struct track *tg, void *item)
/* Return name for oligo, which is just the base position. */
{
struct bed *bed = item;
static char buf[22];
buf[0] = bed->strand[0];
sprintLongWithCommas(buf+1, bed->chromStart+1);
return buf;
}

char *dnaInWindow()
/* This returns the DNA in the window, all in lower case. */
{
static struct dnaSeq *seq = NULL;
if (seq == NULL)
    seq = hDnaFromSeq(database, chromName, winStart, winEnd, dnaLower);
return seq->dna;
}


void oligoMatchLoad(struct track *tg)
/* Create track of perfect matches to oligo on either strand. */
{
char *dna = dnaInWindow();
char *fOligo = oligoMatchSeq();
int oligoSize = strlen(fOligo);
char *rOligo = cloneString(fOligo);
char *rMatch = NULL, *fMatch = NULL;
struct bed *bedList = NULL, *bed;
char strand;
int count = 0, maxCount = 1000000;

if (oligoSize >= 2)
    {
    fMatch = stringIn(fOligo, dna);
    reverseComplement(rOligo, oligoSize);
    if (sameString(rOligo, fOligo))
        rOligo = NULL;
    else
    rMatch = stringIn(rOligo, dna);
    for (;;)
        {
	char *oneMatch = NULL;
	if (rMatch == NULL)
	    {
	    if (fMatch == NULL)
		break;
	    else
		{
		oneMatch = fMatch;
		fMatch = stringIn(fOligo, fMatch+1);
		strand = '+';
		}
	    }
	else if (fMatch == NULL)
	    {
	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else if (rMatch < fMatch)
	    {
	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else
	    {
	    oneMatch = fMatch;
	    fMatch = stringIn(fOligo, fMatch+1);
	    strand = '+';
	    }
	if (count < maxCount)
	    {
	    ++count;
	    AllocVar(bed);
	    bed->chromStart = winStart + (oneMatch - dna);
	    bed->chromEnd = bed->chromStart + oligoSize;
	    bed->strand[0] = strand;
	    slAddHead(&bedList, bed);
	    }
	else
	    break;
	}
    slReverse(&bedList);
    if (count < maxCount)
	tg->items = bedList;
    else
        warn("More than %d items in %s, suppressing display", maxCount, tg->shortLabel);
    }
}

struct track *oligoMatchTg()
/* Make track of perfect matches to oligomer. */
{
struct track *tg = trackNew();
char *oligo = oligoMatchSeq();
int oligoSize = strlen(oligo);
char *medOligo = cloneString(oligo);
static char longLabel[80];
struct trackDb *tdb;

/* Generate abbreviated strings. */
if (oligoSize >= 30)
    {
    memset(medOligo + 30-3, '.', 3);
    medOligo[30] = 0;
    }
touppers(medOligo);

bedMethods(tg);
tg->track = "oligoMatch";
tg->table = tg->track;
tg->canPack = TRUE;
tg->visibility = tvHide;
tg->hasUi = TRUE;
tg->shortLabel = cloneString(OLIGO_MATCH_TRACK_LABEL);
safef(longLabel, sizeof(longLabel),
    "Perfect Matches to Short Sequence (%s)", medOligo);
tg->longLabel = longLabel;
tg->loadItems = oligoMatchLoad;
tg->itemName = oligoMatchName;
tg->mapItemName = oligoMatchName;
tg->priority = 99;
tg->defaultPriority = tg->priority;
tg->groupName = "map";
tg->defaultGroupName = cloneString(tg->groupName);

AllocVar(tdb);
tdb->track = cloneString(tg->track);
tdb->table = cloneString(tg->table);
tdb->visibility = tg->visibility;
tdb->shortLabel = cloneString(tg->shortLabel);
tdb->longLabel = cloneString(tg->longLabel);
tdb->grp = cloneString(tg->groupName);
tdb->priority = tg->priority;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

static int doLeftLabels(struct track *track, struct hvGfx *hvg, MgFont *font,
                                int y)
/* Draw left labels.  Return y coord. */
{
struct slList *prev = NULL;

/* for sample tracks */
double minRangeCutoff, maxRangeCutoff;
double minRange, maxRange;
double min0, max0;
char minRangeStr[32];
char maxRangeStr[32];

int ymin, ymax;
int start;
int newy;
char o4[256];
char o5[256];
struct slList *item;
enum trackVisibility vis = track->limitedVis;
enum trackVisibility savedVis = vis;
Color labelColor = (track->labelColor ?
                        track->labelColor : track->ixColor);
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
if (vis == tvHide)
    return y;

/*  if a track can do its own left labels, do them after drawItems */
if (track->drawLeftLabels != NULL)
    return y + tHeight;

/*  Wiggle tracks depend upon clipping.  They are reporting
 *  totalHeight artifically high by 1 so this will leave a
 *  blank area one pixel high below the track.
 */
if (sameString("wig",track->tdb->type) || sameString("bedGraph",track->tdb->type))
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight-1);
else
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);

minRange = 0.0;
safef( o4, sizeof(o4),"%s.min.cutoff", track->track);
safef( o5, sizeof(o5),"%s.max.cutoff", track->track);
minRangeCutoff = max( atof(cartUsualString(cart,o4,"0.0"))-0.1,
                                track->minRange );
maxRangeCutoff = min( atof(cartUsualString(cart,o5,"1000.0"))+0.1,
                                track->maxRange);
if( sameString( track->table, "humMusL" ) ||
    sameString( track->table, "musHumL" ) ||
    sameString( track->table, "mm3Rn2L" ) ||
    sameString( track->table, "hg15Mm3L" ) ||
    sameString( track->table, "mm3Hg15L" ) ||
    sameString( track->table, "regpotent" ) ||
    sameString( track->table, "HMRConservation" )  )
    {
    int binCount = round(1.0/track->scaleRange);
    minRange = whichSampleBin( minRangeCutoff, track->minRange, track->maxRange, binCount );
    maxRange = whichSampleBin( maxRangeCutoff, track->minRange, track->maxRange ,binCount );
    min0 = whichSampleNum( minRange, track->minRange,track->maxRange, binCount );
    max0 = whichSampleNum( maxRange, track->minRange, track->maxRange, binCount );
    sprintf( minRangeStr, " "  );
    sprintf( maxRangeStr, " " );
    if( vis == tvFull && track->heightPer >= 74  )
        {
        samplePrintYAxisLabel( hvg, y+5, track, "1.0", min0, max0 );
        samplePrintYAxisLabel( hvg, y+5, track, "2.0", min0, max0 );
        samplePrintYAxisLabel( hvg, y+5, track, "3.0", min0, max0 );
        samplePrintYAxisLabel( hvg, y+5, track, "4.0", min0, max0 );
        samplePrintYAxisLabel( hvg, y+5, track, "5.0", min0, max0 );
        samplePrintYAxisLabel( hvg, y+5, track, "6.0", min0, max0 );
        }
    }
else
    {
    sprintf( minRangeStr, "%d", (int)round(minRangeCutoff));
    sprintf( maxRangeStr, "%d", (int)round(maxRangeCutoff));
    }
/* special label handling for wigMaf type tracks -- they
   display a left label in pack mode.  To use the full mode
   labeling, temporarily set visibility to full.
   Restore savedVis later */
if (startsWith("wigMaf", track->tdb->type) || startsWith("maf", track->tdb->type))
    vis = tvFull;

switch (vis)
    {
    case tvHide:
        break;  /* Do nothing; */
    case tvPack:
    case tvSquish:
	y += tHeight;
        break;
    case tvFull:
        if (isCenterLabelIncluded(track))
            y += fontHeight;
        start = 1;

        if( track->subType == lfSubSample && track->items == NULL )
            y += track->height;

        for (item = track->items; item != NULL; item = item->next)
            {
            char *rootName;
            char *name = track->itemName(track, item);
            int itemHeight = track->itemHeight(track, item);
            newy = y;

            if (track->itemLabelColor != NULL)
                labelColor = track->itemLabelColor(track, item, hvg);

            /* Do some fancy stuff for sample tracks.
             * Draw y-value limits for 'sample' tracks. */
            if (track->subType == lfSubSample )
                {
                if( prev == NULL )
                    newy += itemHeight;
                else
                    newy += sampleUpdateY(name,
                            track->itemName(track, prev), itemHeight);
                if( newy == y )
                    continue;

                if( track->heightPer > (3 * fontHeight ) )
                    {
                    ymax = y - (track->heightPer / 2) + (fontHeight / 2);
                    ymin = y + (track->heightPer / 2) - (fontHeight / 2);
                    hvGfxTextRight(hvg, leftLabelX, ymin, leftLabelWidth-1,
                                itemHeight, track->ixAltColor,
                                font, minRangeStr );
                    hvGfxTextRight(hvg, leftLabelX, ymax, leftLabelWidth-1,
                                itemHeight, track->ixAltColor,
                                font, maxRangeStr );
                    }
                prev = item;

                rootName = cloneString( name );
                beforeFirstPeriod( rootName );
                if( sameString( track->table, "humMusL" ) ||
                         sameString( track->table, "hg15Mm3L" ))
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
                             itemHeight, track->ixColor, font, "Mouse Cons");
                else if( sameString( track->table, "musHumL" ) ||
                         sameString( track->table, "mm3Hg15L"))
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
                                itemHeight, track->ixColor, font, "Human Cons");
                else if( sameString( track->table, "mm3Rn2L" ))
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
                                itemHeight, track->ixColor, font, "Rat Cons");
                else
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
                                itemHeight, track->ixColor, font, rootName );
                freeMem( rootName );
                start = 0;
                y = newy;
                }
            else
                {
                /* standard item labeling */
		if (highlightItem(track, item))
		    {
		    int nameWidth = mgFontStringWidth(font, name);
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    hvGfxBox(hvg, boxStart, y, nameWidth+1, itemHeight - 1,
		      labelColor);
		    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1,
			itemHeight, MG_WHITE, font, name);
		    }
		else
		    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
			itemHeight, labelColor, font, name);
		y += itemHeight;
		}
            }
        break;
    case tvDense:

        if (isCenterLabelIncluded(track))
            y += fontHeight;

        /*draw y-value limits for 'sample' tracks.
         * (always puts 0-100% range)*/
        if( track->subType == lfSubSample &&
                track->heightPer > (3 * fontHeight ) )
            {
            ymax = y - (track->heightPer / 2) + (fontHeight / 2);
            ymin = y + (track->heightPer / 2) - (fontHeight / 2);
            hvGfxTextRight(hvg, leftLabelX, ymin,
                        leftLabelWidth-1, track->lineHeight,
                        track->ixAltColor, font, minRangeStr );
            hvGfxTextRight(hvg, leftLabelX, ymax,
                        leftLabelWidth-1, track->lineHeight,
                        track->ixAltColor, font, maxRangeStr );
            }
        hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1,
                    track->lineHeight, labelColor, font,
                    track->shortLabel);
        y += track->height;
        break;
    }
/* NOTE: might want to just restore savedVis here for all track types,
   but I'm being cautious... */
if (sameString(track->tdb->type, "wigMaf"))
    vis = savedVis;
hvGfxUnclip(hvg);
return y;
}

static void doLabelNextItemButtons(struct track *track, struct track *parentTrack, struct hvGfx *hvg, MgFont *font, int y,
                  int trackPastTabX, int trackPastTabWidth, int fontHeight,
                  int insideHeight, Color labelColor)
/* If the track allows label next-item buttons (next gene), draw them. */
/* The button will cause hgTracks to run again with the additional CGI */
/* vars nextItem=trackName or prevItem=trackName, which will then  */
/* signal the browser to find the next thing on the track before it */
/* does anything else. */
{
int portWidth = insideWidth;
int portX = insideX;
#ifdef IMAGEv2_DRAG_SCROLL
// If a portal was established, then set the portal dimensions
int portalStart,chromStart;
double basesPerPixel;
if (theImgBox && imgBoxPortalDimensions(theImgBox,&chromStart,NULL,NULL,NULL,&portalStart,NULL,&portWidth,&basesPerPixel))
    {
    portX = (int)((portalStart - chromStart) / basesPerPixel);
    portX += gfxBorder;
    if (withLeftLabels)
        portX += tl.leftLabelWidth + gfxBorder;
    portWidth = portWidth-gfxBorder-insideX;
    }
#endif//def IMAGEv2_DRAG_SCROLL
int arrowWidth = insideHeight;
int arrowButtonWidth = arrowWidth + 2 * NEXT_ITEM_ARROW_BUFFER;
int rightButtonX = portX + portWidth - arrowButtonWidth - 1;
char buttonText[256];
Color fillColor = lightGrayIndex();
labelColor = blackIndex();
hvGfxNextItemButton(hvg, rightButtonX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, TRUE);
hvGfxNextItemButton(hvg, portX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, FALSE);
safef(buttonText, ArraySize(buttonText), "hgt.prevItem=%s", track->track);
mapBoxReinvoke(hvg, portX, y + 1, arrowButtonWidth, insideHeight, track, FALSE,
           NULL, 0, 0, (revCmplDisp ? "Next item" : "Prev item"), buttonText);
#ifdef IMAGEv2_SHORT_TOGGLE
char *label = (theImgBox ? track->longLabel : parentTrack->longLabel);
int width = portWidth - (2 * arrowButtonWidth);
int x = portX + arrowButtonWidth;
// make toggle cover only actual label
int size = mgFontStringWidth(font,label) + 12;  // get close enough to the label
if (width > size)
    {
    x += width/2 - size/2;
    width = size;
    }
mapBoxToggleVis(hvg, x, y + 1, width, insideHeight, (theImgBox ? track : parentTrack));
#else///ifndef IMAGEv2_SHORT_TOGGLE
mapBoxToggleVis(hvg, portX + arrowButtonWidth, y + 1, portWidth - (2 * arrowButtonWidth),
                insideHeight, (theImgBox ? track : parentTrack));
#endif///ndef IMAGEv2_SHORT_TOGGLE
safef(buttonText, ArraySize(buttonText), "hgt.nextItem=%s", track->track);
mapBoxReinvoke(hvg, portX + portWidth - arrowButtonWidth, y + 1, arrowButtonWidth, insideHeight, track, FALSE,
           NULL, 0, 0, (revCmplDisp ? "Prev item" : "Next item"), buttonText);
}

static int doCenterLabels(struct track *track, struct track *parentTrack,
                                struct hvGfx *hvg, MgFont *font, int y)
/* Draw center labels.  Return y coord */
{
if (track->limitedVis != tvHide)
    {
    if (isCenterLabelIncluded(track))
        {
        int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
        int trackPastTabWidth = tl.picWidth - trackPastTabX;
        int fontHeight = mgFontLineHeight(font);
        int insideHeight = fontHeight-1;
	boolean toggleDone = FALSE;
        char *label = track->longLabel;
        if (isCenterLabelConditional(track))
            label = track->tdb->parent->longLabel;
        Color labelColor = (track->labelColor ?
                            track->labelColor : track->ixColor);
        hvGfxTextCentered(hvg, insideX, y+1, insideWidth, insideHeight,
                            labelColor, font, label);
        if (track->nextItemButtonable && track->nextPrevItem && !tdbIsComposite(track->tdb))
            {
            if (withNextItemArrows || trackDbSettingOn(track->tdb, "nextItemButton"))
                {
                doLabelNextItemButtons(track, parentTrack, hvg, font, y, trackPastTabX,
                        trackPastTabWidth, fontHeight, insideHeight, labelColor);
                toggleDone = TRUE;
                }
            }
        if (!toggleDone)
            {
        #ifdef IMAGEv2_SHORT_TOGGLE
            // make toggle cover only actual label
            int size = mgFontStringWidth(font,label) + 12;  // get close enough to the label
            if (trackPastTabWidth > size)
                {
                trackPastTabX = insideX + insideWidth/2 - size/2;
                trackPastTabWidth = size;
                }
        #endif///def IMAGEv2_SHORT_TOGGLE
            mapBoxToggleVis(hvg, trackPastTabX, y+1,trackPastTabWidth, insideHeight,
                            (theImgBox ? track : parentTrack));
            }
        y += fontHeight;
        }
    y += track->totalHeight(track, track->limitedVis);
    }
return y;
}

static int doDrawItems(struct track *track, struct hvGfx *hvg, MgFont *font,
                                    int y, long *lastTime)
/* Draw track items.  Return y coord */
{
int fontHeight = mgFontLineHeight(font);
int pixWidth = tl.picWidth;
if (isCenterLabelIncluded(track))
    y += fontHeight;
if (track->limitedVis == tvPack)
    {
    hvGfxSetClip(hvg, gfxBorder+trackTabWidth+1, y,
        pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
    }
else
    hvGfxSetClip(hvg, insideX, y, insideWidth, track->height);
track->drawItems(track, winStart, winEnd, hvg, insideX, y, insideWidth,
                 font, track->ixColor, track->limitedVis);
if (measureTiming && lastTime)
    {
    long thisTime = clock1000();
    track->drawTime = thisTime - *lastTime;
    *lastTime = thisTime;
    }
hvGfxUnclip(hvg);
y += track->totalHeight(track, track->limitedVis);
return y;
}

static int doMapItems(struct track *track, struct hvGfx *hvg, int fontHeight, int y)
/* Draw map boxes around track items */
{
char *type = track->tdb->type;
int newy;
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int start = 1;
struct slList *item;
boolean isWig = (sameString("wig", type) || startsWith("wig ", type) ||
        startsWith("bedGraph", type));

if (isCenterLabelIncluded(track))
    y += fontHeight;
if (track->mapsSelf)
    {
    return y+track->height;
    }
if (track->subType == lfSubSample && track->items == NULL)
     y += track->lineHeight;

/* override doMapItems for hapmapLd track */
/* does not scale with subtracks right now, so this is commented out until it can be fixed
if (startsWith("hapmapLd",track->table))
    {
    y += round((double)(scaleForPixels(insideWidth)*insideWidth/2));
    return y;
    }
*/
for (item = track->items; item != NULL; item = item->next)
    {
    int height = track->itemHeight(track, item);

    /*wiggle tracks don't always increment height (y-value) here*/
    if( track->subType == lfSubSample )
        {
        newy = y;
        if( !start && item->next != NULL  )
            {
            newy += sampleUpdateY( track->itemName(track, item),
                             track->itemName(track, item->next),
                             height );
            }
        else if( item->next != NULL || start )
            newy += height;
        start = 0;
        y = newy;
        }
    else
        {
	if (track->mapItem == NULL)
	    track->mapItem = genericMapItem;
        if (!track->mapsSelf)
            {
	    track->mapItem(track, hvg, item, track->itemName(track, item),
                           track->mapItemName(track, item),
                           track->itemStart(track, item),
                           track->itemEnd(track, item), trackPastTabX,
                           y, trackPastTabWidth,height);
            }
        y += height;
        }
    }
/* Wiggle track's ->height is actually one less than what it returns from
 * totalHeight()... I think the least disruptive way to account for this
 * (and not touch Ryan Weber's Sample stuff) is to just correct here if
 * we see wiggle or bedGraph: */
if (isWig)
    y++;
return y;
}

static int doOwnLeftLabels(struct track *track, struct hvGfx *hvg,
                                                MgFont *font, int y)
/* Track draws it own, custom left labels */
{
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
Color labelColor = (track->labelColor ? track->labelColor : track->ixColor);
hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);
track->drawLeftLabels(track, winStart, winEnd,
		      hvg, leftLabelX, y, leftLabelWidth, tHeight,
		      isCenterLabelIncluded(track), font, labelColor,
		      track->limitedVis);
hvGfxUnclip(hvg);
y += tHeight;
return y;
}

// defined below:
static int getMaxWindowToDraw(struct trackDb *tdb);

int doTrackMap(struct track *track, struct hvGfx *hvg, int y, int fontHeight,
           int trackPastTabX, int trackPastTabWidth)
/* Write out the map for this track. Return the new offset. */
{
int mapHeight = 0;
switch (track->limitedVis)
    {
    case tvPack:
    case tvSquish:
        y += trackPlusLabelHeight(track, fontHeight);
        break;
    case tvFull:
        if (!nextItemCompatible(track))
            {
            if (trackIsCompositeWithSubtracks(track))  //TODO: Change when tracks->subtracks are always set for composite
                {
                if (isCenterLabelIncluded(track))
                    y += fontHeight;
                struct track *subtrack;
                for (subtrack = track->subtracks;  subtrack != NULL;subtrack = subtrack->next)
                    {
                    if (isSubtrackVisible(subtrack))
                        {
                        if(subtrack->limitedVis == tvFull)
                            y = doMapItems(subtrack, hvg, fontHeight, y);
                        else
                            {
                            if (isCenterLabelIncluded(subtrack))
                                y += fontHeight;
                            if(theImgBox && subtrack->limitedVis == tvDense)
                                mapBoxToggleVis(hvg, trackPastTabX, y, trackPastTabWidth, track->lineHeight, subtrack);
                            y += subtrack->totalHeight(subtrack, subtrack->limitedVis);
                            }
                        }
                    }
                }
            else
		y = doMapItems(track, hvg, fontHeight, y);
            }
        else
            y += trackPlusLabelHeight(track, fontHeight);
        break;
    case tvDense:
        if (isCenterLabelIncluded(track))
            y += fontHeight;
        if (tdbIsComposite(track->tdb))
            mapHeight = track->height;
        else
            mapHeight = track->lineHeight;
        int maxWinToDraw = getMaxWindowToDraw(track->tdb);
        if (maxWinToDraw <= 1 || (winEnd - winStart) <= maxWinToDraw)
            mapBoxToggleVis(hvg, trackPastTabX, y, trackPastTabWidth, mapHeight, track);
        y += mapHeight;
        break;
    case tvHide:
    default:
        break;  /* Do nothing; */
    }
return y;
}

int computeScaleBar(int numBases, char scaleText[], int scaleTextSize)
/* Do some scalebar calculations and return the number of bases the scalebar will span. */
{
char *baseWord = "bases";
int scaleBases = 0;
int scaleBasesTextNum = 0;
int numFigs = (int)log10(numBases);
int frontNum = (int)(numBases/pow(10,numFigs));
if (frontNum == 1)
    {
    numFigs--;
    scaleBases = 5 * pow(10, numFigs);
    scaleBasesTextNum = 5 * pow(10, numFigs % 3);
    }
else if ((frontNum > 1) && (frontNum <= 4))
    {
    scaleBases = pow(10, numFigs);
    scaleBasesTextNum = pow(10, numFigs % 3);
    }
else if (frontNum > 4)
    {
    scaleBases = 2 * pow(10, numFigs);
    scaleBasesTextNum = 2 * pow(10, numFigs % 3);
    }
if ((numFigs >= 3) && (numFigs < 6))
    baseWord = "kb";
else if ((numFigs >= 6) && (numFigs < 9))
    baseWord = "Mb";
safef(scaleText, scaleTextSize, "%d %s", scaleBasesTextNum, baseWord);
return scaleBases;
}

enum trackVisibility limitedVisFromComposite(struct track *subtrack)
/* returns the subtrack visibility which may be limited by composite with multi-view dropdowns. */
{
if(tdbIsCompositeChild(subtrack->tdb))
    {
    if (!subtrack->limitedVisSet)
        {
        subtrack->visibility = tdbVisLimitedByAncestors(cart, subtrack->tdb, TRUE, TRUE);
        limitVisibility(subtrack);
        }
    }
else
    limitVisibility(subtrack);
return subtrack->limitedVis;
}

static int makeRulerZoomBoxes(struct hvGfx *hvg, struct cart *cart, int winStart,int winEnd,
	int insideWidth,int seqBaseCount,int rulerClickY,int rulerClickHeight)
/* Make hit boxes that will zoom program around ruler. */
{
int boxes = 30;
int winWidth = winEnd - winStart;
int newWinWidth = winWidth;
int i, ws, we = 0, ps, pe = 0;
int mid, ns, ne;
double wScale = (double)winWidth/boxes;
double pScale = (double)insideWidth/boxes;
char message[32];
char *zoomType = cartCgiUsualString(cart, RULER_BASE_ZOOM_VAR, ZOOM_3X);

safef(message, sizeof(message), "%s zoom", zoomType);
if (sameString(zoomType, ZOOM_1PT5X))
    newWinWidth = winWidth/1.5;
else if (sameString(zoomType, ZOOM_3X))
    newWinWidth = winWidth/3;
else if (sameString(zoomType, ZOOM_10X))
    newWinWidth = winWidth/10;
else if (sameString(zoomType, ZOOM_BASE))
    newWinWidth = insideWidth/tl.mWidth;
else
    errAbort("invalid zoom type %s", zoomType);

if (newWinWidth < 1)
    newWinWidth = 1;

for (i=1; i<=boxes; ++i)
    {
    ps = pe;
    ws = we;
    pe = round(pScale*i);
    we = round(wScale*i);
    mid = (ws + we)/2 + winStart;
    ns = mid-newWinWidth/2;
    ne = ns + newWinWidth;
    if (ns < 0)
        {
        ns = 0;
        ne -= ns;
        }
    if (ne > seqBaseCount)
        {
        ns -= (ne - seqBaseCount);
        ne = seqBaseCount;
        }
    if(!dragZooming)
        {
        mapBoxJumpTo(hvg, ps+insideX,rulerClickY,pe-ps,rulerClickHeight,NULL,
                        chromName, ns, ne, message);
        }
    }
return newWinWidth;
}

static int doDrawRuler(struct hvGfx *hvg,int *newWinWidth,int *rulerClickHeight,
	int rulerHeight, int yAfterRuler, int yAfterBases, MgFont *font,
	int fontHeight,boolean rulerCds)
/* draws the ruler. */
{
int scaleBarPad = 2;
int scaleBarHeight = fontHeight;
int scaleBarTotalHeight = fontHeight + 2 * scaleBarPad;
int titleHeight = fontHeight;
int baseHeight = fontHeight;
//int yAfterBases = yAfterRuler;
int showPosHeight = fontHeight;
int codonHeight = fontHeight;
struct dnaSeq *seq = NULL;
int rulerClickY = 0;
*rulerClickHeight = rulerHeight;

int y = rulerClickY;
hvGfxSetClip(hvg, insideX, y, insideWidth, yAfterRuler-y+1);
int relNumOff = winStart;

if (baseTitle)
    {
    hvGfxTextCentered(hvg, insideX, y, insideWidth, titleHeight,MG_BLACK, font, baseTitle);
    *rulerClickHeight += titleHeight;
    y += titleHeight;
    }
if (baseShowPos||baseShowAsm)
    {
    char txt[256];
    char numBuf[SMALLBUF];
    char *freezeName = NULL;
    freezeName = hFreezeFromDb(database);
    sprintLongWithCommas(numBuf, winEnd-winStart);
    if(freezeName == NULL)
	freezeName = "Unknown";
    if (baseShowPos&&baseShowAsm)
	safef(txt,sizeof(txt),"%s %s   %s (%s bp)",organism,
		freezeName, addCommasToPos(database, position), numBuf);
    else if (baseShowPos)
	safef(txt,sizeof(txt),"%s (%s bp)",addCommasToPos(database, position),numBuf);
    else
	safef(txt,sizeof(txt),"%s %s",organism,freezeName);
    hvGfxTextCentered(hvg, insideX, y, insideWidth, showPosHeight,MG_BLACK, font, txt);
    *rulerClickHeight += showPosHeight;
    freez(&freezeName);
    y += showPosHeight;
    }
if (baseShowScaleBar)
    {
    char scaleText[32];
    int numBases = winEnd-winStart;
    int scaleBases = computeScaleBar(numBases, scaleText, sizeof(scaleText));
    int scalePixels = (int)((double)insideWidth*scaleBases/numBases);
    int scaleBarX = insideX + (int)(((double)insideWidth-scalePixels)/2);
    int scaleBarEndX = scaleBarX + scalePixels;
    int scaleBarY = y + 0.5 * scaleBarTotalHeight;
    *rulerClickHeight += scaleBarTotalHeight;
    hvGfxTextRight(hvg, insideX, y + scaleBarPad,
	    (scaleBarX-2)-insideX, scaleBarHeight, MG_BLACK, font, scaleText);
    hvGfxLine(hvg, scaleBarX, scaleBarY, scaleBarEndX, scaleBarY, MG_BLACK);
    hvGfxLine(hvg, scaleBarX, y+scaleBarPad, scaleBarX,
	    y+scaleBarTotalHeight-scaleBarPad, MG_BLACK);
    hvGfxLine(hvg, scaleBarEndX, y+scaleBarPad, scaleBarEndX,
	    y+scaleBarTotalHeight-scaleBarPad, MG_BLACK);
    y += scaleBarTotalHeight;
    }
if (baseShowRuler)
    {
    hvGfxDrawRulerBumpText(hvg, insideX, y, rulerHeight, insideWidth, MG_BLACK,
		font, relNumOff, winBaseCount, 0, 1);
    }
*newWinWidth = makeRulerZoomBoxes(hvg, cart,winStart,winEnd,insideWidth,seqBaseCount,rulerClickY,*rulerClickHeight);

if (zoomedToBaseLevel || rulerCds)
    {
    Color baseColor = MG_BLACK;
    int start, end, chromSize;
    struct dnaSeq *extraSeq;
    /* extraSeq has extra leading & trailing bases
    * for translation in to amino acids */
    boolean complementRulerBases =
	    cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
    // gray bases if not matching the direction of display
    if (complementRulerBases != revCmplDisp)
	baseColor = MG_GRAY;

    /* get sequence, with leading & trailing 3 bases
     * used for amino acid translation */
    start = max(winStart - 3, 0);
    chromSize = hChromSize(database, chromName);
    end = min(winEnd + 3, chromSize);
    extraSeq = hDnaFromSeq(database, chromName, start, end, dnaUpper);
    if (start != winStart - 3 || end != winEnd + 3)
	{
	/* at chromosome boundaries, pad with N's to assure
	 * leading & trailing 3 bases */
	char header[4] = "NNN", trailer[4] = "NNN";
	int size = winEnd - winStart + 6;
	char *padded = (char *)needMem(size+1);
	header[max(3 - winStart, 0)] = 0;
	trailer[max(winEnd - chromSize + 3, 0)] = 0;
	safef(padded, size+1, "%s%s%s", header, extraSeq->dna, trailer);
	extraSeq = newDnaSeq(padded, strlen(padded), extraSeq->name);
	}

    /* for drawing bases, must clip off leading and trailing 3 bases */
    seq = cloneDnaSeq(extraSeq);
    seq = newDnaSeq(seq->dna+3, seq->size-6, seq->name);

    if (zoomedToBaseLevel)
	drawBases(hvg, insideX, y+rulerHeight, insideWidth, baseHeight,
	    baseColor, font, complementRulerBases, seq);

    /* set up clickable area to toggle ruler visibility */
	{
	char newRulerVis[100];
	safef(newRulerVis, 100, "%s=%s", RULER_TRACK_NAME,
		     rulerMode == tvFull ?
			    rulerMenu[tvDense] :
			    rulerMenu[tvFull]);
	mapBoxReinvoke(hvg, insideX, y+rulerHeight, insideWidth,baseHeight, NULL,
	    FALSE, NULL, 0, 0, "", newRulerVis);
	}
    if (rulerCds)
	{
	/* display codons */
	int frame;
	int firstFrame = 0;
	int mod;            // for determining frame ordering on display
	struct simpleFeature *sfList;
	double scale = scaleForWindow(insideWidth, winStart, winEnd);

	/* WARNING: tricky code to assure that an amino acid
	 * stays in the same frame line on the browser during panning.
	 * There may be a simpler way... */
	if (complementRulerBases)
	    mod = (chromSize - winEnd) % 3;
	else
	    mod = winStart % 3;
	if (mod == 0)
	    firstFrame = 0;
	else if (mod == 1)
	    firstFrame = 2;
	else if (mod == 2)
	    firstFrame = 1;

	y = yAfterBases;
	if (complementRulerBases)
	    reverseComplement(extraSeq->dna, extraSeq->size);
	for (frame = 0; frame < 3; frame++, y += codonHeight)
	    {
	    /* reference frame to start of chromosome */
	    int refFrame = (firstFrame + frame) % 3;

	    /* create list of codons in the specified coding frame */
	    sfList = baseColorCodonsFromDna(refFrame, winStart, winEnd,
					 extraSeq, complementRulerBases);
	    /* draw the codons in the list, with alternating colors */
	    baseColorDrawRulerCodons(hvg, sfList, scale, insideX, y,
				codonHeight, font, winStart, MAXPIXELS,
				zoomedToCodonLevel);
	    }
	}
    }
hvGfxUnclip(hvg);
return y;
}

static void rAddToTrackHash(struct hash *trackHash, struct track *trackList)
/* Add list and any children of list to hash. */
{
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    hashAddUnique(trackHash, track->track, track);
    rAddToTrackHash(trackHash, track->subtracks);
    }
}

struct hash *makeGlobalTrackHash(struct track *trackList)
/* Create a global track hash and returns a pointer to it. */
{
trackHash = newHash(8);
rAddToTrackHash(trackHash, trackList);
return trackHash;
}


void makeActiveImage(struct track *trackList, char *psOutput)
/* Make image and image map. */
{
struct track *track;
MgFont *font = tl.font;
struct hvGfx *hvg;
struct tempName gifTn;
char *mapName = "map";
int fontHeight = mgFontLineHeight(font);
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackTabX = gfxBorder;
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int pixWidth, pixHeight;
int y=0;
int titleHeight = fontHeight;
int scaleBarPad = 2;
int scaleBarHeight = fontHeight;
int scaleBarTotalHeight = fontHeight + 2 * scaleBarPad;
int showPosHeight = fontHeight;
int rulerHeight = fontHeight;
int baseHeight = fontHeight;
int basePositionHeight = rulerHeight;
int codonHeight = fontHeight;
int rulerTranslationHeight = codonHeight * 3;        // 3 frames
int yAfterRuler = gfxBorder;
int yAfterBases = yAfterRuler;  // differs if base-level translation shown
boolean rulerCds = zoomedToCdsColorLevel;
int rulerClickHeight = 0;
int newWinWidth = 0;

/* Figure out dimensions and allocate drawing space. */
pixWidth = tl.picWidth;

leftLabelX = gfxBorder;
leftLabelWidth = insideX - gfxBorder*3;

struct image *theOneImg  = NULL; // No need to be global, only the map needs to be global
struct image *theSideImg = NULL; // Because dragScroll drags off end of image, the side label gets seen. Therefore we need 2 images!!
//struct imgTrack *curImgTrack = NULL; // Make this global for now to avoid huge rewrite
struct imgSlice *curSlice    = NULL; // No need to be global, only the map needs to be global
struct mapSet   *curMap      = NULL; // Make this global for now to avoid huge rewrite
// Set up imgBox dimensions
int sliceWidth[stMaxSliceTypes]; // Just being explicit
int sliceOffsetX[stMaxSliceTypes];
int sliceHeight        = 0;
int sliceOffsetY       = 0;
char *rulerTtl = NULL;
if(theImgBox)
// theImgBox is a global for now to avoid huge rewrite of hgTracks.  It is started
// prior to this in doTrackForm()
    {
    rulerTtl = (dragZooming?"drag select or click to zoom":"click to zoom 3x");
    hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
    hPrintf("<input type='hidden' name='c' value='%s'>\n", chromName);
    hPrintf("<input type='hidden' name='l' value='%d'>\n", winStart);
    hPrintf("<input type='hidden' name='r' value='%d'>\n", winEnd);
    hPrintf("<input type='hidden' name='pix' value='%d'>\n", tl.picWidth);
    #ifdef IMAGEv2_DRAG_SCROLL
    // If a portal was established, then set the global dimensions to the entire image size
    if(imgBoxPortalDimensions(theImgBox,&winStart,&winEnd,&(tl.picWidth),NULL,NULL,NULL,NULL,NULL))
        {
        pixWidth = tl.picWidth;
        winBaseCount = winEnd - winStart;
        insideWidth = tl.picWidth-gfxBorder-insideX;
        }
    #endif//def IMAGEv2_DRAG_SCROLL
    memset((char *)sliceWidth,  0,sizeof(sliceWidth));
    memset((char *)sliceOffsetX,0,sizeof(sliceOffsetX));
    if (withLeftLabels)
        {
        sliceWidth[stButton]   = trackTabWidth + 1;
        sliceWidth[stSide]     = leftLabelWidth - sliceWidth[stButton] + 1;
        sliceOffsetX[stSide]   = (revCmplDisp? (tl.picWidth - sliceWidth[stSide] - sliceWidth[stButton]) : sliceWidth[stButton]);
        sliceOffsetX[stButton] = (revCmplDisp? (tl.picWidth - sliceWidth[stButton]) : 0);
        }
    sliceOffsetX[stData] = (revCmplDisp?0:sliceWidth[stSide] + sliceWidth[stButton]);
    sliceWidth[stData]   = tl.picWidth - (sliceWidth[stSide] + sliceWidth[stButton]);
    }
struct flatTracks *flatTracks = NULL;
struct flatTracks *flatTrack = NULL;

if (rulerMode != tvFull)
    {
    rulerCds = FALSE;
    }

/* Figure out height of each visible track. */
pixHeight = gfxBorder;
if (rulerMode != tvHide)
    {
    if (!baseShowRuler && !baseTitle && !baseShowPos && !baseShowAsm && !baseShowScaleBar && !zoomedToBaseLevel && !rulerCds)
        {
        warn("Can't turn everything off in base position track.  Turning ruler back on");
        baseShowRuler = TRUE;
        cartSetBoolean(cart, BASE_SHOWRULER, TRUE);
        }

    if (baseTitle)
        basePositionHeight += titleHeight;

    if (baseShowPos||baseShowAsm)
        basePositionHeight += showPosHeight;

    if (baseShowScaleBar)
        basePositionHeight += scaleBarTotalHeight;

    if (!baseShowRuler)
        {
        basePositionHeight -= rulerHeight;
        rulerHeight = 0;
        }

    if (zoomedToBaseLevel)
        basePositionHeight += baseHeight;

    yAfterRuler += basePositionHeight;
    yAfterBases = yAfterRuler;
    pixHeight += basePositionHeight;
    if (rulerCds)
        {
        yAfterRuler += rulerTranslationHeight;
        pixHeight += rulerTranslationHeight;
        }
    }

boolean safeHeight = TRUE;
/* firefox on Linux worked almost up to 34,000 at the default 620 width */
#define maxSafeHeight   32000
/* Hash tracks/subtracks, limit visibility and calculate total image height: */
for (track = trackList; track != NULL; track = track->next)
    {
    if(tdbIsCompositeChild(track->tdb)) // When single track is requested via AJAX, it could be a subtrack
        limitedVisFromComposite(track);
    else
        limitVisibility(track);

    if (!safeHeight)
        {
        track->limitedVis = tvHide;
        track->limitedVisSet = TRUE;
        continue;
        }

    if (tdbIsComposite(track->tdb))
        {
        struct track *subtrack;
        for (subtrack = track->subtracks; subtrack != NULL;
                        subtrack = subtrack->next)
            {
            if (!isSubtrackVisible(subtrack))
                continue;

            // subtrack vis can be explicit or inherited from composite/view.  Then it could be limited because of pixel height
            limitedVisFromComposite(subtrack);
            //assert(subtrack->limitedVisSet); // This is no longer a valid assertion, since visible track with no items items will not have limitedVisSet

            if (subtrack->limitedVis != tvHide)
                {
                subtrack->hasUi = track->hasUi;
                flatTracksAdd(&flatTracks,subtrack,cart);
                }
            }
        }
    else if (track->limitedVis != tvHide)
        flatTracksAdd(&flatTracks,track,cart);
    }
flatTracksSort(&flatTracks); // Now we should have a perfectly good flat track list!
struct track *prevTrack = NULL;
for (flatTrack = flatTracks,prevTrack=NULL; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;
    assert(track->limitedVis != tvHide);
    int totalHeight = pixHeight+trackPlusLabelHeight(track,fontHeight);
    if (maxSafeHeight < totalHeight)
        {
        char numBuf[SMALLBUF];
        sprintLongWithCommas(numBuf, maxSafeHeight);
        if (safeHeight)  // Only one message
            warn("Image is over %s pixels high (%d pix) at the following track which is now hidden:<BR>\"%s\".%s", numBuf, totalHeight, track->tdb->longLabel,
                (flatTrack->next != NULL?"<BR>Additional tracks may have also been hidden at this zoom level.":""));
        safeHeight = FALSE;
        track->limitedVis = tvHide;
        track->limitedVisSet = TRUE;
        }
    if (track->limitedVis != tvHide)
        {
        track->prevTrack = prevTrack; // Important for keeping track of conditional center labels!
        pixHeight += trackPlusLabelHeight(track, fontHeight);
        prevTrack = track;
        }
    }

imagePixelHeight = pixHeight;
if (psOutput)
    {
    hvg = hvGfxOpenPostScript(pixWidth, pixHeight, psOutput);
    hvgSide = hvg; // Always only one image
    }
else
    {
    boolean transparentImage = FALSE;
    if (theImgBox!=NULL)
        transparentImage = TRUE;   // transparent because BG (blue ruler lines) is separate image

    trashDirFile(&gifTn, "hgt", "hgt", ".png");
    hvg = hvGfxOpenPng(pixWidth, pixHeight, gifTn.forCgi, transparentImage);

    if(theImgBox)
        {
        // Adds one single image for all tracks (COULD: build the track by track images)
        theOneImg = imgBoxImageAdd(theImgBox,gifTn.forHtml,NULL,pixWidth, pixHeight,FALSE);
        theSideImg = theOneImg; // Unlkess this is overwritten below, there is a single image
        }
    hvgSide = hvg; // Unlkess this is overwritten below, there is a single image

    if (theImgBox && theImgBox->showPortal && withLeftLabels)
        {
        // TODO: It would be great to make the images smaller, but keeping both the same full size for now
        struct tempName gifTnSide;
        trashDirFile(&gifTnSide, "hgt", "side", ".png");
        hvgSide = hvGfxOpenPng(pixWidth, pixHeight, gifTnSide.forCgi, transparentImage);

        // Also add the side image
        theSideImg = imgBoxImageAdd(theImgBox,gifTnSide.forHtml,NULL,pixWidth, pixHeight,FALSE);
        hvgSide->rc = revCmplDisp;
        initColors(hvgSide);
        }
    }
hvg->rc = revCmplDisp;
initColors(hvg);

/* Start up client side map. */
hPrintf("<MAP id='map' Name=%s>\n", mapName);

/* Find colors to draw in. */
findTrackColors(hvg, trackList);

// Good to go ahead and add all imgTracks regardless of buttons, left label, centerLabel, etc.
if(theImgBox)
    {
    if (rulerMode != tvHide)
        {
        curImgTrack = imgBoxTrackFindOrAdd(theImgBox,NULL,RULER_TRACK_NAME,rulerMode,FALSE,IMG_FIXEDPOS); // No tdb, no centerlabel, not reorderable
        }

    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis != tvHide)
            {
            if(track->labelColor == track->ixColor && track->ixColor == 0)
                track->ixColor = hvGfxFindRgb(hvg, &track->color);
            int order = flatTrack->order;
            curImgTrack = imgBoxTrackFindOrAdd(theImgBox,track->tdb,NULL,track->limitedVis,isCenterLabelIncluded(track),order);
            if (trackShouldUseAjaxRetrieval(track))
                imgTrackMarkForAjaxRetrieval(curImgTrack,TRUE);
            }
        }
    }

/* Draw mini-buttons. */
if (withLeftLabels && psOutput == NULL)
    {
    int butOff;
    boolean grayButtonGroup = FALSE;
    struct group *lastGroup = NULL;
    y = gfxBorder;
    if (rulerMode != tvHide)
        {
        /* draw button for Base Position pseudo-track */
        int height = basePositionHeight;
        if (rulerCds)
            height += rulerTranslationHeight;
        if(theImgBox)
            {
            // Mini-buttons (side label slice) for ruler
            sliceHeight      = height + 1;
            sliceOffsetY     = 0;
            curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stButton,NULL,NULL,sliceWidth[stButton],sliceHeight,sliceOffsetX[stButton],sliceOffsetY); // flatTracksButton is all html, no jpg
            }
        else if(!trackImgOnly) // Side buttons only need to be drawn when drawing page with js advanced features off  // TODO: Should remove wasted pixels too
            drawGrayButtonBox(hvgSide, trackTabX, y, trackTabWidth, height, TRUE);
        mapBoxTrackUi(hvgSide, trackTabX, y, trackTabWidth, height,
              RULER_TRACK_NAME, RULER_TRACK_LABEL, "ruler");
        y += height + 1;
        }

    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        int h, yStart = y, yEnd;
        if (track->limitedVis != tvHide)
            {
            y += trackPlusLabelHeight(track, fontHeight);
            yEnd = y;
            h = yEnd - yStart - 1;

            /* alternate button colors for track groups*/
            if (track->group != lastGroup)
                grayButtonGroup = !grayButtonGroup;
            lastGroup = track->group;
            if(theImgBox)
                {
                // Mini-buttons (side label slice) for tracks
                sliceHeight      = yEnd - yStart;
                sliceOffsetY     = yStart - 1;
                curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
                curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stButton,NULL,NULL,sliceWidth[stButton],sliceHeight,sliceOffsetX[stButton],sliceOffsetY); // flatTracksButton is all html, no jpg
                }
            else if(!trackImgOnly) // Side buttons only need to be drawn when drawing page with js advanced features off
                {
                if (grayButtonGroup)
                    drawGrayButtonBox(hvgSide, trackTabX, yStart, trackTabWidth, h, track->hasUi);
                else
                    drawBlueButtonBox(hvgSide, trackTabX, yStart, trackTabWidth, h, track->hasUi);
                }

            if (track->hasUi)
                {
                if(tdbIsCompositeChild(track->tdb))
                    {
                    struct trackDb *parent = tdbGetComposite(track->tdb);
                    mapBoxTrackUi(hvgSide, trackTabX, yStart, trackTabWidth, (yEnd - yStart - 1),
                        parent->track, parent->shortLabel, track->track);
                    }
                else
                    mapBoxTrackUi(hvgSide, trackTabX, yStart, trackTabWidth, h, track->track, track->shortLabel, track->track);
                }
            }
        }
    butOff = trackTabX + trackTabWidth;
    leftLabelX += butOff;
    leftLabelWidth -= butOff;
    }

if (withLeftLabels)
    {
    if (theImgBox == NULL)
        {
        Color lightRed = hvGfxFindColorIx(hvgSide, 255, 180, 180);

        hvGfxBox(hvgSide, leftLabelX + leftLabelWidth, 0,
            gfxBorder, pixHeight, lightRed);
        }
    y = gfxBorder;
    if (rulerMode != tvHide)
        {
        if(theImgBox)
            {
            // side label slice for ruler
            sliceHeight      = basePositionHeight + (rulerCds ? rulerTranslationHeight : 0) + 1;
            sliceOffsetY     = 0;
            curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,sliceWidth[stSide],sliceHeight,sliceOffsetX[stSide],sliceOffsetY);
            curMap      = sliceMapFindOrStart(curSlice,RULER_TRACK_NAME,NULL); // No common linkRoot
            }
        if (baseTitle)
            {
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, titleHeight,
                MG_BLACK, font, WIN_TITLE_LABEL);
            y += titleHeight;
            }
        if (baseShowPos||baseShowAsm)
            {
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, showPosHeight,
                MG_BLACK, font, WIN_POS_LABEL);
            y += showPosHeight;
            }
        if (baseShowScaleBar)
            {
            y += scaleBarPad;
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, scaleBarHeight,
                MG_BLACK, font, SCALE_BAR_LABEL);
            y += scaleBarHeight + scaleBarPad;
            }
        if (baseShowRuler)
            {
            char rulerLabel[SMALLBUF];
            char *shortChromName = cloneString(chromName);
            safef(rulerLabel,ArraySize(rulerLabel),":%s",shortChromName);
            int labelWidth = mgFontStringWidth(font,rulerLabel);
            while ((labelWidth > 0) && (labelWidth > leftLabelWidth))
                {
                int len = strlen(shortChromName);
                shortChromName[len-1] = 0;
                safef(rulerLabel,ArraySize(rulerLabel),":%s",shortChromName);
                labelWidth = mgFontStringWidth(font,rulerLabel);
                }
            if (hvgSide->rc)
            safef(rulerLabel,ArraySize(rulerLabel),":%s",shortChromName);
            else
                safef(rulerLabel,ArraySize(rulerLabel),"%s:",shortChromName);
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, rulerHeight,
                MG_BLACK, font, rulerLabel);
            y += rulerHeight;
            freeMem(shortChromName);
            }
        if (zoomedToBaseLevel || rulerCds)
            {
            /* disable complement toggle for HIV because HIV is single stranded RNA */
            if (!hIsGsidServer())
                    drawComplementArrow(hvgSide,leftLabelX, y,
                                        leftLabelWidth-1, baseHeight, font);
            if (zoomedToBaseLevel)
                y += baseHeight;
            }
            if (rulerCds)
                y += rulerTranslationHeight;
        }
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis == tvHide)
            continue;
         if(theImgBox)
            {
            // side label slice for tracks
            sliceHeight      = trackPlusLabelHeight(track, fontHeight);
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,sliceWidth[stSide],sliceHeight,sliceOffsetX[stSide],sliceOffsetY);
            curMap      = sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            }
        if (trackShouldUseAjaxRetrieval(track))
            y += REMOTE_TRACK_HEIGHT;
        else
            {
        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox && track->limitedVis != tvDense)
                y += sliceHeight;
            else
        #endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL
                y = doLeftLabels(track, hvgSide, font, y);
            }
        }
    }
else
    {
    leftLabelX = leftLabelWidth = 0;
    }

/* Draw guidelines. */
if (withGuidelines)
    {
    struct hvGfx *bgImg = hvg; // Default to the one image
    boolean exists = FALSE;
    if(theImgBox)
        {
        struct tempName gifBg;
        char base[64];
        safef(base,sizeof(base),"blueLines%d-%s%d-%d",pixWidth,(revCmplDisp?"r":""),insideX,guidelineSpacing);  // reusable file needs width, leftLabel start and guidelines
        exists = trashDirReusableFile(&gifBg, "hgt", base, ".png");
        if (exists && cgiVarExists("hgt.reset")) // exists means don't remake bg image.
            exists = FALSE;                       // However, for the time being, rebuild when user presses "default tracks"

        if (!exists)
            {
            bgImg = hvGfxOpenPng(pixWidth, pixHeight, gifBg.forCgi, TRUE);
            bgImg->rc = revCmplDisp;
            }
        imgBoxImageAdd(theImgBox,gifBg.forHtml,NULL,pixWidth, pixHeight,TRUE); // Adds BG image
        }

    if (!exists)
        {
        int x;
        Color lightBlue = hvGfxFindRgb(bgImg, &guidelineColor);

        hvGfxSetClip(bgImg, insideX, 0, insideWidth, pixHeight);
        y = gfxBorder;

        for (x = insideX+guidelineSpacing-1; x<pixWidth; x += guidelineSpacing)
            hvGfxBox(bgImg, x, 0, 1, pixHeight, lightBlue);
        hvGfxUnclip(bgImg);
        if(bgImg != hvg)
            hvGfxClose(&bgImg);
        }
    }

/* Show ruler at top. */
if (rulerMode != tvHide)
    {
    if(theImgBox)
        {
        // data slice for ruler
        sliceHeight      = basePositionHeight + (rulerCds ? rulerTranslationHeight : 0) + 1;
        sliceOffsetY     = 0;
        curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
        curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stData,theOneImg,rulerTtl,sliceWidth[stData],sliceHeight,sliceOffsetX[stData],sliceOffsetY);
        curMap      = sliceMapFindOrStart(curSlice,RULER_TRACK_NAME,NULL); // No common linkRoot
        }
    y = doDrawRuler(hvg,&newWinWidth,&rulerClickHeight,rulerHeight,yAfterRuler,yAfterBases,font,fontHeight,rulerCds);
    }

/* Draw center labels. */
if (withCenterLabels)
    {
    hvGfxSetClip(hvg, insideX, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis == tvHide)
            continue;

        if(theImgBox)
            {
            // center label slice of tracks Must always make, even if the centerLabel is empty
            sliceHeight      = fontHeight;
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stCenter,theOneImg,NULL,sliceWidth[stData],sliceHeight,sliceOffsetX[stData],sliceOffsetY);
            curMap      = sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            if (isCenterLabelConditional(track))
                imgTrackUpdateCenterLabelSeen(curImgTrack,isCenterLabelConditionallySeen(track)?clNowSeen:clNotSeen);
            }
        if (trackShouldUseAjaxRetrieval(track))
            y += REMOTE_TRACK_HEIGHT;
        else
            y = doCenterLabels(track, track, hvg, font, y);
        }
    hvGfxUnclip(hvg);
    }

/* Draw tracks. */
    {
    long lastTime = 0;
    y = yAfterRuler;
    if (measureTiming)
        lastTime = clock1000();
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis == tvHide)
                continue;

        int centerLabelHeight = (isCenterLabelIncluded(track) ? fontHeight : 0);
        int yStart = y + centerLabelHeight;
        int yEnd   = y + trackPlusLabelHeight(track, fontHeight);
        if(theImgBox)
            {
            // data slice of tracks
            sliceOffsetY     = yStart;
            sliceHeight      = yEnd - yStart - 1;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            if(sliceHeight > 0)
                {
                curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stData,theOneImg,NULL,sliceWidth[stData],sliceHeight,sliceOffsetX[stData],sliceOffsetY);
                curMap      = sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
                }
            }
        if (trackShouldUseAjaxRetrieval(track))
            y += REMOTE_TRACK_HEIGHT;
        else
            y = doDrawItems(track, hvg, font, y, &lastTime);

        if (theImgBox && track->limitedVis == tvDense && tdbIsCompositeChild(track->tdb))
            mapBoxToggleVis(hvg, 0, yStart,tl.picWidth, sliceHeight,track); // Strange mabBoxToggleLogic handles reverse complement itself so x=0, width=tl.picWidth

        if(yEnd!=y)
            warn("Slice height does not add up.  Expecting %d != %d actual",yEnd - yStart - 1,y-yStart);
        }
    y++;
    }
/* if a track can draw its left labels, now is the time since it
 *  knows what exactly happened during drawItems
 */
if (withLeftLabels)
    {
    y = yAfterRuler;
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis == tvHide)
                continue;
        if(theImgBox)
            {
            // side label slice of tracks
            sliceHeight      = trackPlusLabelHeight(track, fontHeight);
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,sliceWidth[stSide],sliceHeight,sliceOffsetX[stSide],sliceOffsetY);
            curMap      = sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            }

        if (trackShouldUseAjaxRetrieval(track))
            y += REMOTE_TRACK_HEIGHT;
    #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
        else if (track->drawLeftLabels != NULL && (theImgBox == NULL || track->limitedVis == tvDense))
    #else ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
        else if (track->drawLeftLabels != NULL)
    #endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
            y = doOwnLeftLabels(track, hvgSide, font, y);
        else
            y += trackPlusLabelHeight(track, fontHeight);
        }
    }


/* Make map background. */
y = yAfterRuler;
for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;
    if (track->limitedVis != tvHide)
        {
        if(theImgBox)
            {
            // Set imgTrack in case any map items will be set
            sliceHeight      = trackPlusLabelHeight(track, fontHeight);
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            }
        y = doTrackMap(track, hvg, y, fontHeight, trackPastTabX, trackPastTabWidth);
        }
    }

/* Finish map. */
hPrintf("</MAP>\n");

jsonHashAddBoolean(jsonForClient, "dragSelection", dragZooming);
jsonHashAddBoolean(jsonForClient, "inPlaceUpdate", IN_PLACE_UPDATE && advancedJavascriptFeaturesEnabled(cart));

if(rulerClickHeight)
    {
    jsonHashAddNumber(jsonForClient, "rulerClickHeight", rulerClickHeight);
    }
if(newWinWidth)
    {
    jsonHashAddNumber(jsonForClient, "newWinWidth", newWinWidth);
    }

/* Save out picture and tell html file about it. */
if(hvgSide != hvg)
    hvGfxClose(&hvgSide);
hvGfxClose(&hvg);

#ifdef SUPPORT_CONTENT_TYPE
char *type = cartUsualString(cart, "hgt.contentType", "html");
if(sameString(type, "jsonp"))
    {
    struct jsonHashElement *json = newJsonHash(newHash(8));

    printf("Content-Type: application/json\n\n");
    jsonHashAddString(json, "track", cartString(cart, "hgt.trackNameFilter"));
    jsonHashAddNumber(json, "height", pixHeight);
    jsonHashAddNumber(json, "width", pixWidth);
    jsonHashAddString(json, "img", gifTn.forHtml);
    printf("%s(", cartString(cart, "jsonp"));
    hPrintEnable();
    jsonPrint((struct jsonElement *) json, NULL, 0);
    hPrintDisable();
    printf(")\n");
    return;
    }
else if(sameString(type, "png"))
    {
    // following is (currently dead) experimental code to bypass hgml and return png's directly - see redmine 4888
    printf("Content-Disposition: filename=hgTracks.png\nContent-Type: image/png\n\n");

    char buf[4096];
    FILE *fd = fopen(gifTn.forCgi, "r");
    if(fd == NULL)
        // fail some other way (e.g. HTTP 500)?
        errAbort("Couldn't open png for reading");
    while(TRUE)
        {
        size_t n = fread(buf, 1, sizeof(buf), fd);
        if(n)
            fwrite(buf, 1, n, stdout);
        else
            break;
        }
    fclose(fd);
    unlink(gifTn.forCgi);
    return;
    }
#endif

if(theImgBox)
    {
    imageBoxDraw(theImgBox);
    #ifdef IMAGEv2_DRAG_SCROLL
    // If a portal was established, then set the global dimensions back to the portal size
    if(imgBoxPortalDimensions(theImgBox,NULL,NULL,NULL,NULL,&winStart,&winEnd,&(tl.picWidth),NULL))
        {
        pixWidth = tl.picWidth;
        winBaseCount = winEnd - winStart;
        insideWidth = tl.picWidth-gfxBorder-insideX;
        }
    #endif//def IMAGEv2_DRAG_SCROLL
    imgBoxFree(&theImgBox);
    }
else
    {
    char *titleAttr = dragZooming ? "title='click or drag mouse in base position track to zoom in'" : "";
    hPrintf("<IMG SRC='%s' BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s %s id='trackMap'",
        gifTn.forHtml, pixWidth, pixHeight, mapName, titleAttr);
    hPrintf("><BR>\n");
    }
flatTracksFree(&flatTracks);
}

static void appendLink(struct hotLink **links, char *url, char *name, char *id)
{
// append to list of links for later printing and/or communication with javascript client
struct hotLink *link;
AllocVar(link);
link->name = cloneString(name);
link->url = cloneString(url);
link->id = cloneString(id);
slAddTail(links, link);
}

static void printEnsemblAnchor(char *database, char* archive,
                               char *chrName, int start, int end, struct hotLink **links)
/* Print anchor to Ensembl display on same window. */
{
char *scientificName = hScientificName(database);
char *dir = ensOrgNameFromScientificName(scientificName);
struct dyString *ensUrl;
char *name;
int localStart, localEnd;

name = chrName;

if (sameWord(scientificName, "Takifugu rubripes"))
    {
    /* for Fugu, must give scaffold, not chr coordinates */
    /* Also, must give "chrom" as "scaffold_N", name below. */
    if (differentWord(chromName,"chrM") &&
    !hScaffoldPos(database, chromName, winStart, winEnd,
                        &name, &localStart, &localEnd))
        /* position doesn't appear on Ensembl browser.
         * Ensembl doesn't show scaffolds < 2K */
        return;
    }
else if (sameWord(scientificName, "Gasterosteus aculeatus"))
    {
    if (differentWord("chrM", chrName))
    {
    char *fixupName = replaceChars(chrName, "chr", "group");
    name = fixupName;
    }
    }
else if (sameWord(scientificName, "Ciona intestinalis"))
    {
    if (stringIn("chr0", chrName))
	{
	char *fixupName = replaceChars(chrName, "chr0", "chr");
	name = fixupName;
	}
    }
else if (sameWord(scientificName, "Saccharomyces cerevisiae"))
    {
    if (stringIn("2micron", chrName))
	{
	char *fixupName = replaceChars(chrName, "2micron", "2-micron");
	name = fixupName;
	}
    }

if (sameWord(chrName, "chrM"))
    name = "chrMt";
ensUrl = ensContigViewUrl(database, dir, name, seqBaseCount, start+1, end, archive);
appendLink(links, ensUrl->string, "Ensembl", "ensemblLink");
/* NOTE: you can not freeMem(dir) because sometimes it is a literal
 * constant */
freeMem(scientificName);
dyStringFree(&ensUrl);
}

void makeHgGenomeTrackVisible(struct track *track)
/* This turns on a track clicked from hgGenome, even if it was previously */
/* hidden manually and there are cart vars to support that. */
{
struct hashEl *hels;
struct hashEl *hel;
char prefix[SMALLBUF];
/* First check if the click was from hgGenome.  If not, leave. */
/* get the names of the tracks in the cart */
safef(prefix, sizeof(prefix), "%s_", hggGraphPrefix);
hels = cartFindPrefix(cart, prefix);
/* loop through them and compare them to the track passed into this */
/* function. */
for (hel = hels; hel != NULL; hel = hel->next)
    {
    struct trackDb *subtrack;
    char *subtrackName = hel->val;
    /* check non-subtrack. */
    if (sameString(track->tdb->track, subtrackName))
	{
	track->visibility = tvFull;
	track->tdb->visibility = tvFull;
	cartSetString(cart, track->tdb->track, "full");
	}
    else if (track->tdb->subtracks != NULL)
	{
	struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(track->tdb->subtracks);
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	    {
	    subtrack = tdbRef->val;
	    if (sameString(subtrack->track, subtrackName))
		{
		char selName[SMALLBUF];
		track->visibility = tvFull;
		cartSetString(cart, track->tdb->track, "full");
		track->tdb->visibility = tvFull;
		subtrack->visibility = tvFull;
		safef(selName, sizeof(selName), "%s_sel", subtrackName);
		cartSetBoolean(cart, selName, TRUE);
		}
	    }
	slFreeList(&tdbRefList);
	}
    }
hashElFreeList(&hels);
}

struct sqlConnection *remoteTrackConnection(struct track *tg)
/* Get a connection to remote database as specified in remoteSql settings... */
{
if (!tg->isRemoteSql)
    {
    internalErr();
    return NULL;
    }
else
    {
    return sqlConnectRemote(tg->remoteSqlHost, tg->remoteSqlUser, tg->remoteSqlPassword,
        tg->remoteSqlDatabase);
    }
}

void addTdbListToTrackList(struct trackDb *tdbList, char *trackNameFilter,
	struct track **pTrackList)
/* Convert a list of trackDb's to tracks, and append these to trackList. */
{
struct trackDb *tdb, *next;
struct track *track;
TrackHandler handler;
tdbSortPrioritiesFromCart(cart, &tdbList);
for (tdb = tdbList; tdb != NULL; tdb = next)
    {
    next = tdb->next;
    if(trackNameFilter != NULL && strcmp(trackNameFilter, tdb->track))
        // suppress loading & display of all tracks except for the one passed in via trackNameFilter
        continue;
    if (sameString(tdb->type, "downloadsOnly")) // These tracks should not even be seen by6 hgTracks. (FIXME: Until we want to see them in cfg list and searchTracks!)
        continue;
    track = trackFromTrackDb(tdb);
    track->hasUi = TRUE;
    if (slCount(tdb->subtracks) != 0)
        {
        tdbSortPrioritiesFromCart(cart, &(tdb->subtracks));
	if (trackDbLocalSetting(tdb, "compositeTrack"))
	    makeCompositeTrack(track, tdb);
	else if (trackDbLocalSetting(tdb, "container"))
	    makeContainerTrack(track, tdb);
        }
    else
        {
        handler = lookupTrackHandler(tdb->table);
        if (handler != NULL)
            handler(track);
        }
    if (cgiVarExists("hgGenomeClick"))
	makeHgGenomeTrackVisible(track);
    if (track->loadItems == NULL)
        warn("No load handler for %s; possible missing trackDb `type' or `subTrack' attribute", tdb->track);
    else if (track->drawItems == NULL)
        warn("No draw handler for %s", tdb->track);
    else
        slAddHead(pTrackList, track);
    }
}

void loadFromTrackDb(struct track **pTrackList)
/* Load tracks from database, consulting handler list. */
{
char *trackNameFilter = cartOptionalString(cart, "hgt.trackNameFilter");
struct trackDb *tdbList;
if(trackNameFilter == NULL)
    tdbList = hTrackDb(database);
else
    tdbList = hTrackDbForTrack(database, trackNameFilter);
addTdbListToTrackList(tdbList, trackNameFilter, pTrackList);
}

static int getScoreFilter(char *trackName)
/* check for score filter configuration setting */
{
char optionScoreStr[256];

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", trackName);
return cartUsualInt(cart, optionScoreStr, 0);
}

void ctLoadSimpleBed(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct customTrack *ct = tg->customPt;
struct bed *bed, *nextBed, *list = NULL;
int scoreFilter = getScoreFilter(ct->tdb->track);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn =
        hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
             NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	    if (scoreFilter && bed->score < scoreFilter)
		continue;
	slAddHead(&list, bed);
	}
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = nextBed)
	{
	nextBed = bed->next;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart
		&& sameString(chromName, bed->chrom))
	    {
	    if (scoreFilter && bed->score < scoreFilter)
		continue;
	    slAddHead(&list, bed);
	    }
	}
    }
slSort(&list, bedCmp);
tg->items = list;
}

void ctLoadBed9(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
boolean useItemRgb = FALSE;
int scoreFilter = getScoreFilter(ct->tdb->track);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int rowOffset;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
             NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 9);
	if (scoreFilter && bed->score < scoreFilter)
	    continue;
	bed8To12(bed);
	lf = lfFromBed(bed);
	if (useItemRgb)
	    {
	    lf->extra = (void *)USE_ITEM_RGB;   /* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
        if (scoreFilter && bed->score < scoreFilter)
            continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart
		&& sameString(chromName, bed->chrom))
	    {
	    bed8To12(bed);
	    lf = lfFromBed(bed);
	    if (useItemRgb)
		{
		lf->extra = (void *)USE_ITEM_RGB;   /* signal for coloring */
		lf->filterColor=bed->itemRgb;
		}
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}


void ctLoadBed8(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
int scoreFilter = getScoreFilter(ct->tdb->track);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
             NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	    if (scoreFilter && bed->score < scoreFilter)
		continue;
	bed8To12(bed);
	lf = lfFromBed(bed);
	slAddHead(&lfList, lf);
	}
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
	if (scoreFilter && bed->score < scoreFilter)
	    continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart
		&& sameString(chromName, bed->chrom))
	    {
	    bed8To12(bed);
	    lf = lfFromBed(bed);
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void ctLoadGappedBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
boolean useItemRgb = FALSE;
int scoreFilter = getScoreFilter(ct->tdb->track);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
             NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	lf = lfFromBed(bed);
	    if (scoreFilter && bed->score < scoreFilter)
		continue;
	if (useItemRgb)
	    {
	    lf->extra = (void *)USE_ITEM_RGB;   /* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
        if (scoreFilter && bed->score < scoreFilter)
            continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart
		&& sameString(chromName, bed->chrom))
	    {
	    lf = lfFromBed(bed);
	    if (useItemRgb)
		{
		lf->extra = (void *)USE_ITEM_RGB; /* signal for coloring */
		lf->filterColor=bed->itemRgb;
		}
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void ctLoadColoredExon(struct track *tg)
/* Convert bed info in window to linked features series for custom track. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;
    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
             NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	lfs = lfsFromColoredExonBed(bed);
	slAddHead(&lfsList, lfs);
	}
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
	if (bed->chromStart < winEnd && bed->chromEnd > winStart
		&& sameString(chromName, bed->chrom))
	    {
	    lfs = lfsFromColoredExonBed(bed);
	    slAddHead(&lfsList, lfs);
	    }
	}
    }
slReverse(&lfsList);
slSort(&lfsList, linkedFeaturesSeriesCmp);
tg->items = lfsList;
}

char *ctMapItemName(struct track *tg, void *item)
/* Return composite item name for custom tracks. */
{
char *itemName = tg->itemName(tg, item);
static char buf[512];
if (strlen(itemName) > 0)
    safef(buf, sizeof(buf), "%s %s", ctFileName, itemName);
else
    safef(buf, sizeof(buf), "%s NoItemName", ctFileName);
return buf;
}


void coloredExonMethodsFromCt(struct track *tg)
/* same as coloredExonMethods but different loader. */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = ctLoadColoredExon;
tg->canPack = TRUE;
}

void dontLoadItems(struct track *tg)
/* No-op loadItems when we aren't going to try. */
{
}

struct track *newCustomTrack(struct customTrack *ct)
/* Make up a new custom track. */
{
struct track *tg = NULL;
struct trackDb *tdb = ct->tdb;
boolean useItemRgb = FALSE;
char *typeOrig = tdb->type;
char *typeDupe = cloneString(typeOrig);
char *typeParam = typeDupe;
char *type = nextWord(&typeParam);

if (ct->dbTrack)
    {
    // make sure we can connect
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    hFreeConn(&conn);
    }

useItemRgb = bedItemRgb(tdb);

if (sameString(type, "maf"))
    {
    tg = trackFromTrackDb(tdb);
    tg->canPack = TRUE;

    wigMafMethods(tg, tdb, 0, NULL);
    if (!ct->dbTrack)
    errAbort("custom maf tracks must be in database");

    struct mafPriv *mp;
    AllocVar(mp);
    mp->ct = ct;

    tg->customPt = mp;
    tg->nextItemButtonable = FALSE;
    }
else if (sameString(type, "wig"))
    {
    tg = trackFromTrackDb(tdb);
    if (ct->dbTrack)
    tg->loadItems = wigLoadItems;
    else
    tg->loadItems = ctWigLoadItems;
    tg->customPt = ct;
    tg->nextItemButtonable = FALSE;
    }
else if (sameString(type, "bigWig"))
    {
    tg = trackFromTrackDb(tdb);
    tg->bbiFile = ct->bbiFile;
    tg->nextItemButtonable = FALSE;
    if (trackShouldUseAjaxRetrieval(tg))
        tg->loadItems = dontLoadItems;
    }
else if (sameString(type, "bigBed"))
    {
    struct bbiFile *bbi = ct->bbiFile;

    /* Find field counts, and from that revise the tdb->type to be more complete. */
    char extra = (bbi->fieldCount > bbi->definedFieldCount ? '+' : '.');
    char typeBuf[64];
    safef(typeBuf, sizeof(typeBuf), "bigBed %d %c", bbi->definedFieldCount, extra);
    tdb->type = cloneString(typeBuf);

    /* Finish wrapping track around tdb. */
    tg = trackFromTrackDb(tdb);
    tg->bbiFile = bbi;
    tg->nextItemButtonable = FALSE;
    if (trackShouldUseAjaxRetrieval(tg))
        tg->loadItems = dontLoadItems;
    }
else if (sameString(type, "bedGraph"))
    {
    tg = trackFromTrackDb(tdb);
    tg->canPack = FALSE;
    tg->customPt = ct;
    ct->wigFile = ctFileName;
    tg->mapItemName = ctMapItemName;
    tg->nextItemButtonable = FALSE;
    }
else if (sameString(type, "bed"))
    {
    tg = trackFromTrackDb(tdb);
    if (ct->fieldCount < 8)
	{
	tg->loadItems = ctLoadSimpleBed;
	}
    else if (useItemRgb && ct->fieldCount == 9)
	{
	tg->loadItems = ctLoadBed9;
	}
    else if (ct->fieldCount < 12)
	{
	tg->loadItems = ctLoadBed8;
	}
    else if (ct->fieldCount == 15)
	{
	char *theType = trackDbSetting(tdb, "type");
	if (theType && sameString(theType, "expRatio"))
	    {
	    tg = trackFromTrackDb(tdb);
	    expRatioMethodsFromCt(tg);
	    }
	else
	    tg->loadItems = ctLoadGappedBed;
	}
    else
	{
	tg->loadItems = ctLoadGappedBed;
	}
    tg->mapItemName = ctMapItemName;
    tg->canPack = TRUE;
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "chromGraph"))
    {
    tdb->type = NULL;   /* Swap out type for the moment. */
    tg = trackFromTrackDb(tdb);
    chromGraphMethodsCt(tg);
    tg->nextItemButtonable = FALSE;
    tdb->type = typeOrig;
    }
else if (sameString(type, "array"))
    {
    tg = trackFromTrackDb(tdb);
    expRatioMethodsFromCt(tg);
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "coloredExon"))
    {
    tg = trackFromTrackDb(tdb);
    coloredExonMethodsFromCt(tg);
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "encodePeak"))
    {
    tg = trackFromTrackDb(tdb);
    encodePeakMethodsCt(tg);
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "bam"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    bamMethods(tg);
    if (trackShouldUseAjaxRetrieval(tg))
        tg->loadItems = dontLoadItems;
    tg->mapItemName = ctMapItemName;
    hashAdd(tdb->settingsHash, BASE_COLOR_USE_SEQUENCE, cloneString("lfExtra"));
    hashAdd(tdb->settingsHash, BASE_COLOR_DEFAULT, cloneString("diffBases"));
    hashAdd(tdb->settingsHash, SHOW_DIFF_BASES_ALL_SCALES, cloneString("."));
    hashAdd(tdb->settingsHash, INDEL_DOUBLE_INSERT, cloneString("on"));
    hashAdd(tdb->settingsHash, INDEL_QUERY_INSERT, cloneString("on"));
    hashAdd(tdb->settingsHash, INDEL_POLY_A, cloneString("on"));
    hashAdd(tdb->settingsHash, "showDiffBasesMaxZoom", cloneString("100"));
    }
else if (sameString(type, "vcfTabix"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    vcfTabixMethods(tg);
    if (trackShouldUseAjaxRetrieval(tg))
        tg->loadItems = dontLoadItems;
    tg->mapItemName = ctMapItemName;
    }
else if (sameString(type, "makeItems"))
    {
    tg = trackFromTrackDb(tdb);
    makeItemsMethods(tg);
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "bedDetail"))
    {
    tg = trackFromTrackDb(tdb);
    bedDetailCtMethods(tg, ct);
    tg->mapItemName = ctMapItemName; /* must be here to see ctMapItemName */
    }
else if (sameString(type, "pgSnp"))
    {
    tg = trackFromTrackDb(tdb);
    pgSnpCtMethods(tg);
    //tg->mapItemName = ctMapItemName;
    tg->customPt = ct;
    }
else
    {
    errAbort("Unrecognized custom track type %s", type);
    }
if (!ct->dbTrack)
    tg->nextItemButtonable = FALSE;
tg->hasUi = TRUE;
tg->customTrack = TRUE;// Explicitly declare this a custom track for flatTrack ordering

freez(&typeDupe);
return tg;
}

char *getPositionFromCustomTracks()
/* Parses custom track data to get the position variable
 * return - The first chromosome position variable found in the
 * custom track data.  */
{
char *pos = NULL;
struct slName *bl = NULL;

ctList = customTracksParseCart(database, cart, &browserLines, &ctFileName);

for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;
    char *dupe = cloneString(bl->name);

    wordCount = chopLine(dupe, words);
    if (wordCount >= 3)
        {
        char *command = words[1];
        if (sameString(command, "position"))
            pos = cloneString(words[2]);
        }
    freez(&dupe);
    if (pos != NULL)
        break;
    }
return pos;
}

void loadCustomTracks(struct track **pTrackList)
/* Load up custom tracks and append to list. */
{
struct customTrack *ct;
struct track *tg;
struct slName *bl;

/* build up browser lines from cart variables set by hgCustom */
char *visAll = cartCgiUsualString(cart, "hgt.visAllFromCt", NULL);
if (visAll)
    {
    char buf[SMALLBUF];
    safef(buf, sizeof buf, "browser %s %s", visAll, "all");
    slAddTail(&browserLines, slNameNew(buf));
    }
struct hashEl *visEl;
struct hashEl *visList = cartFindPrefix(cart, "hgtct.");
for (visEl = visList; visEl != NULL; visEl = visEl->next)
    {
    char buf[256];
    safef(buf, sizeof buf, "browser %s %s", cartString(cart, visEl->name),
                chopPrefix(cloneString(visEl->name)));
    slAddTail(&browserLines, slNameNew(buf));
    cartRemove(cart, visEl->name);
    }
hashElFreeList(&visList);

/* The loading is now handled by getPositionFromCustomTracks(). */
/* Process browser commands in custom track. */
for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;

    wordCount = chopLine(bl->name, words);
    if (wordCount > 1)
        {
	char *command = words[1];
	if (sameString(command, "hide")
            || sameString(command, "dense")
            || sameString(command, "pack")
            || sameString(command, "squish")
            || sameString(command, "full"))
	    {
	    if (wordCount > 2)
		{
		int i;
		for (i=2; i<wordCount; ++i)
		    {
		    char *s = words[i];
		    struct track *tg;
		    boolean toAll = sameWord(s, "all");
		    for (tg = *pTrackList; tg != NULL; tg = tg->next)
			{
			if (toAll || sameString(s, tg->track))
			    {
			    if (hTvFromString(command) == tg->tdb->visibility)
				/* remove if setting to default vis */
				cartRemove(cart, tg->track);
			    else
				cartSetString(cart, tg->track, command);
			    /* hide or show supertrack enclosing this track */
			    if (tdbIsSuperTrackChild(tg->tdb))
				{
				assert(tg->tdb->parentName != NULL);
				cartSetString(cart, tg->tdb->parentName,
					    (sameString(command, "hide") ?
						"hide" : "show"));
				}
			    }
			}
		    }
		}
	    }
	else if (sameString(command, "position"))
	    {
	    if (wordCount < 3)
		errAbort("Expecting 3 words in browser position line");
	    if (!hgIsChromRange(database, words[2]))
		errAbort("browser position needs to be in chrN:123-456 format");
	    hgParseChromRange(database, words[2], &chromName, &winStart, &winEnd);

		/*Fix a start window of -1 that is returned when a custom track position
		  begins at 0
		*/
		if (winStart < 0)
		    {
		    winStart = 0;
		    }
	    }
	else if (sameString(command, "pix"))
	    {
	    if (wordCount != 3)
		errAbort("Expecting 3 words in pix line");
	    trackLayoutSetPicWidth(&tl, words[2]);
	    }
	}
    }
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    hasCustomTracks = TRUE;
    tg = newCustomTrack(ct);
    slAddHead(pTrackList, tg);
    }
}

static void addTracksFromTrackHub(int id, char *hubUrl, struct track **pTrackList,
	struct trackHub **pHubList)
/* Load up stuff from data hub and append to list. The hubUrl points to
 * a trackDb.ra format file.  */
{
/* Load trackDb.ra file and make it into proper trackDb tree */
char hubName[8];
safef(hubName, sizeof(hubName), "hub_%d",id);
struct trackHub *hub = trackHubOpen(hubUrl, hubName);
if (hub != NULL)
    {
    struct trackHubGenome *hubGenome = trackHubFindGenome(hub, database);
    if (hubGenome != NULL)
	{
	struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
	tdbList = trackDbLinkUpGenerations(tdbList);
	tdbList = trackDbPolishAfterLinkup(tdbList, database);
	trackDbPrioritizeContainerItems(tdbList);
	addTdbListToTrackList(tdbList, NULL, pTrackList);
	if (tdbList != NULL)
	    slAddHead(pHubList, hub);
	}
    }
}

void loadTrackHubs(struct track **pTrackList, struct trackHub **pHubList)
/* Load up stuff from data hubs and append to lists. */
{
struct hubConnectStatus *hub, *hubList =  hubConnectStatusListFromCart(cart);
for (hub = hubList; hub != NULL; hub = hub->next)
    {
    if (isEmpty(hub->errorMessage))
	{

        /* error catching in so it won't just abort  */
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
	    addTracksFromTrackHub(hub->id, hub->hubUrl, pTrackList, pHubList);
        errCatchEnd(errCatch);
        if (errCatch->gotError)
	    hubSetErrorMessage( errCatch->message->string, hub->id);
	else
	    hubSetErrorMessage(NULL, hub->id);
        errCatchFree(&errCatch);
	}
    }
hubConnectStatusFreeList(&hubList);
}

boolean restrictionEnzymesOk()
/* Check to see if it's OK to do restriction enzymes. */
{
return (hTableExists("hgFixed", "cutters") &&
    hTableExists("hgFixed", "rebaseRefs") &&
    hTableExists("hgFixed", "rebaseCompanies"));
}

static void fr2ScaffoldEnsemblLink(char *archive, struct hotLink **links)
/* print out Ensembl link to appropriate scaffold there */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
safef(query, sizeof(query),
"select * from chrUn_gold where chrom = '%s' and chromStart<%u and chromEnd>%u",
chromName, winEnd, winStart);
sr = sqlGetResult(conn, query);

int itemCount = 0;
struct agpFrag *agpItem = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpFragFree(&agpItem);  // if there is a second one
    agpItem = agpFragLoad(row+1);
    ++itemCount;
    if (itemCount > 1)
	break;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (1 == itemCount)
    {   // verify *entirely* within single contig
    if ((winEnd <= agpItem->chromEnd) &&
	(winStart >= agpItem->chromStart))
	{
	int agpStart = winStart - agpItem->chromStart;
	int agpEnd = agpStart + winEnd - winStart;
	hPuts("<TD ALIGN=CENTER>");
	printEnsemblAnchor(database, archive, agpItem->frag,
                           agpStart, agpEnd, links);
	hPrintf("%s</A></TD>", "Ensembl");
	}
    }
agpFragFree(&agpItem);  // the one we maybe used
}

void hotLinks()
/* Put up the hot links bar. */
{
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);
char *orgEnc = cgiEncode(organism);
boolean psOutput = cgiVarExists("hgt.psOutput");
struct hotLink *link, *links = NULL;

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#2636D1\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR>\n");
hPrintf("<TD><TABLE BORDER=\"0\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"../index.html?org=%s&db=%s&%s=%u\" class=\"topbar\">Home</A>&nbsp;&nbsp;</TD>",
    orgEnc, database, cartSessionVarName(), cartSessionId(cart));

if (hIsGisaidServer())
    {
    /* disable hgGateway for gisaid for now */
    //hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Sequence View Gateway</A>&nbsp;&nbsp;</TD>", orgEnc, database);
    hPrintf(
    "<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gisaidTable?gisaidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks&%s=%u\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
    '%', '%',
    cartSessionVarName(),
    cartSessionId(cart),
    "Select Subjects");
    }
else
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Sequence View Gateway</A>&nbsp;&nbsp;</TD>", orgEnc, database);
    hPrintf(
    "<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gsidTable?gsidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
    '%', '%', "Select Subjects");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s&%s=%u\" class=\"topbar\">Genomes</A>&nbsp;&nbsp;</TD>", orgEnc, database, cartSessionVarName(), cartSessionId(cart));
    }
if (psOutput)
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTracks?hgTracksConfigPage=notSetorg=%s&db=%s&%s=%u\" class='topbar'>Genome Browser</A>&nbsp;&nbsp;</TD>", orgEnc, database, cartSessionVarName(), cartSessionId(cart));
    }
if (gotBlat)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgBlat?%s\" class=\"topbar\">Blat</A>&nbsp;&nbsp;</TD>", uiVars->string);
    }
if (hIsGisaidServer())
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gisaidTable?db=%s&%s=%u\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
       database,
       cartSessionVarName(),
       cartSessionId(cart),
       "Table View");
    }
else if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gsidTable?db=%s\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
       database, "Table View");
    }
else
    {
    /* disable TB for CGB servers */
    if (!hIsCgbServer())
	{
	    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTables?db=%s&%s=%u\" "
		    "class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
		    database, cartSessionVarName(), cartSessionId(cart),
	"Tables");
	}
    }

if (hgNearOk(database))
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgNear?%s\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
                 uiVars->string, "Gene Sorter");
    }
if (hgPcrOk(database))
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgPcr?%s\" class=\"topbar\">PCR</A>&nbsp;&nbsp;</TD>", uiVars->string);
    }
if (!psOutput)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\" class=\"topbar\" id='dnaLink'>"
        "%s</A>&nbsp;&nbsp;</TD>",  hgcNameAndSettings(),
        winStart, chromName, winStart, winEnd, database, uiVars->string, "DNA");
    }

if (!psOutput)
    {
    /* disable Convert function for CGB servers for the time being */
    if (!hIsCgbServer())
    if (liftOverChainForDb(database) != NULL)
        {
        hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgConvert?%s&db=%s",
		uiVars->string, database);
        hPrintf("\" class=\"topbar\">Convert</A>&nbsp;&nbsp;</TD>");
        }
    }

char ensVersionString[256];
char ensDateReference[256];
ensGeneTrackVersion(database, ensVersionString, ensDateReference,
    sizeof(ensVersionString));

if (!psOutput)
    {
    if (differentWord(database,"susScr2"))
        {
        /* Print Ensembl anchor for latest assembly of organisms we have
        * supported by Ensembl == if versionString from trackVersion exists */
        if (sameWord(database,"hg19"))
            {
            printEnsemblAnchor(database, NULL, chromName, winStart, winEnd, &links);
            }
        else if (sameWord(database,"hg18"))
            {
            printEnsemblAnchor(database, "ncbi36", chromName, winStart, winEnd, &links);
            }
        else if (sameWord(database,"oryCun2") || sameWord(database,"anoCar2") || sameWord(database,"calJac3"))
            {
            printEnsemblAnchor(database, NULL, chromName, winStart, winEnd, &links);
            }
        else if (ensVersionString[0])
            {
            char *archive = NULL;
            if (ensDateReference[0] && differentWord("current", ensDateReference))
                archive = cloneString(ensDateReference);
            /*  Can we perhaps map from a UCSC random chrom to an Ensembl contig ? */
            if (isUnknownChrom(database, chromName))
                {
                //	which table to check
                char *ctgPos = "ctgPos";

                if (sameWord(database,"fr2"))
                    fr2ScaffoldEnsemblLink(archive, &links);
                else if (hTableExists(database, ctgPos))
                    /* see if we are entirely within a single contig */
                    {
                    struct sqlConnection *conn = hAllocConn(database);
                    struct sqlResult *sr = NULL;
                    char **row = NULL;
                    char query[256];
                    safef(query, sizeof(query),
            "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u",
                    ctgPos, chromName, winEnd, winStart);
                    sr = sqlGetResult(conn, query);

                    int itemCount = 0;
                    struct ctgPos *ctgItem = NULL;
                    while ((row = sqlNextRow(sr)) != NULL)
                        {
                        ctgPosFree(&ctgItem);   // if there is a second one
                        ctgItem = ctgPosLoad(row);
                        ++itemCount;
                        if (itemCount > 1)
                            break;
                        }
                    sqlFreeResult(&sr);
                    hFreeConn(&conn);
                    if (1 == itemCount)
                        {   // verify *entirely* within single contig
                        if ((winEnd <= ctgItem->chromEnd) &&
                            (winStart >= ctgItem->chromStart))
                            {
                            int ctgStart = winStart - ctgItem->chromStart;
                            int ctgEnd = ctgStart + winEnd - winStart;
                            printEnsemblAnchor(database, archive, ctgItem->contig,
                                               ctgStart, ctgEnd, &links);
                            }
                        }
                    ctgPosFree(&ctgItem);   // the one we maybe used
                    }
                }
            else
                {
                printEnsemblAnchor(database, archive, chromName, winStart, winEnd, &links);
                }
            }
        }
    }

if (!psOutput)
    {
    char buf[2056];
    /* Print NCBI MapView anchor */
    if (sameString(database, "hg18"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&build=previous&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "hg19"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "mm8"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=10090&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "danRer2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7955&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "galGal3"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9031&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "canFam2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9615&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "rheMac2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9544&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "panTro2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9598&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "anoGam1"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7165&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (sameString(database, "bosTau6"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9913&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink");
        }
    if (startsWith("oryLat", database))
        {
        safef(buf, sizeof(buf), "http://medaka.utgenome.org/browser_ens_jump.php?revision=version1.0&chr=chromosome%s&start=%d&end=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "UTGB", "medakaLink");
        }
    if (sameString(database, "cb3"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/seq/gbrowse/briggsae?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink");
        }
    if (sameString(database, "cb4"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_briggsae?name=%s:%d-%d",
            chromName, winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink");
        }
    if (sameString(database, "ce10"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_elegans?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink");
        }
    if (sameString(database, "ce4"))
        {
        safef(buf, sizeof(buf), "http://ws170.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink");
        }
    if (sameString(database, "ce2"))
        {
        safef(buf, sizeof(buf), "http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink");
        }
    }

for(link = links; link != NULL; link = link->next)
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"%s\" TARGET=\"_blank\" class=\"topbar\" id=\"%s\">%s</A>&nbsp;&nbsp;</TD>\n", link->url, link->id, link->name);

if (!psOutput)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\" id='pdfLink' class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",cartSessionVarName(),
        cartSessionId(cart), "PDF/PS");
    }

if (!psOutput)
    {
    if (wikiLinkEnabled())
        {
        printf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgSession?%s=%u"
        "&hgS_doMainPage=1\" class=\"topbar\">Session</A>&nbsp;&nbsp;</TD>",
        cartSessionVarName(), cartSessionId(cart));
        }
    }

if (hIsGisaidServer())
    {
    //hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"/goldenPath/help/gisaidTutorial.html#SequenceView\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgNotYet\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }
else
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"/goldenPath/help/gsidTutorial.html#SequenceView\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }

hPuts("<TD colspan=20>&nbsp;</TD></TR></TABLE></TD>");
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}

static void setSuperTrackHasVisibleMembers(struct trackDb *tdb)
/* Determine if any member tracks are visible -- currently
 * recording this in the parent's visibility setting */
{
tdb->visibility = tvDense;
}

boolean superTrackHasVisibleMembers(struct trackDb *tdb)
/* Determine if any member tracks are visible -- currently
 * recording this in the parent's visibility setting */
{
if (!tdbIsSuper(tdb))
    return FALSE;
return (tdb->visibility != tvHide);
}

int hubCmpAlpha(const void *va, const void *vb)
/* Compare to sort hubs based on name */
{
const struct trackHub *a = *((struct trackHub **)va);
const struct trackHub *b = *((struct trackHub **)vb);

return strcmp(a->shortLabel, b->shortLabel);
}

static void groupTracks(struct trackHub *hubList, struct track **pTrackList,
	struct group **pGroupList, int vis)
/* Make up groups and assign tracks to groups.
 * If vis is -1, restore default groups to tracks. */
{
struct group *unknown = NULL;
struct group *group, *list = NULL;
struct hash *hash = newHash(8);
struct track *track;
struct trackRef *tr;
struct grp* grps = hLoadGrps(database);
struct grp *grp;
float minPriority = 100000; // something really large

/* build group objects from database. */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    /* deal with group reordering */
    float priority = grp->priority;
    // we want to get the minimum priority over 1 (which is custom tracks)
    if ((priority > 1.0) && (priority < minPriority)) minPriority = priority;
    if (withPriorityOverride)
        {
        char cartVar[512];
        safef(cartVar, sizeof(cartVar), "%s.priority",grp->name);
        if (vis != -1)
            priority = (float)cartUsualDouble(cart, cartVar, grp->priority);
        if (priority == grp->priority)
            cartRemove(cart, cartVar);
        }
    /* create group object; add to list and hash */
    AllocVar(group);
    group->name = cloneString(grp->name);
    group->label = cloneString(grp->label);
    group->defaultPriority = grp->priority;
    group->priority = priority;
    group->defaultIsClosed = grp->defaultIsClosed;
    slAddHead(&list, group);
    hashAdd(hash, grp->name, group);
    }
grpFreeList(&grps);

/* build group objects from hub */
    {
    int count = slCount(hubList);

    if (count) // if we have track hubs
	{
	slSort(&hubList, hubCmpAlpha);	// alphabetize
	minPriority -= 1.0;             // priority is 1-based
	// the idea here is to get enough room between priority 1
	// (which is custom tracks) and the group with the next
	// priority number, so that the hub nestle inbetween the
	// custom tracks and everything else at the top of the list
	// of track groups
	double priorityInc = (0.9 * minPriority) / count;
	double priority = 1.0 + priorityInc;

	struct trackHub *hub;
	for (hub = hubList; hub != NULL; hub = hub->next)
	    {
	    AllocVar(group);
	    group->name = cloneString(hub->name);
	    group->label = cloneString(hub->shortLabel);
	    group->defaultPriority = group->priority = priority;
	    priority += priorityInc;
	    slAddHead(&list, group);
	    hashAdd(hash, group->name, group);
	    }
	}
    }

/* Loop through tracks and fill in their groups.
 * If necessary make up an unknown group. */
for (track = *pTrackList; track != NULL; track = track->next)
    {
    /* handle track reordering feature -- change group assigned to track */
    if (withPriorityOverride)
        {
        char *groupName = NULL;
        char cartVar[256];

        /* belt and suspenders -- accomodate inconsistent track/trackDb
         * creation.  Note -- with code cleanup, these default variables
         * could be retired, and the tdb versions used as defaults */
        if (!track->defaultGroupName)
            {
            if (track->tdb && track->tdb->grp)
                track->defaultGroupName = cloneString(track->tdb->grp);
            else
                track->defaultGroupName = cloneString("other");
            }
        if (tdbIsSuperTrackChild(track->tdb))
            {
            assert(track->tdb->parentName != NULL);
            /* supertrack member must be in same group as its super */
            /* determine supertrack group */
            safef(cartVar, sizeof(cartVar), "%s.group",track->tdb->parentName);
            groupName = cloneString(                                              //1
                    cartUsualString(cart, cartVar, track->tdb->parent->grp));
            track->tdb->parent->grp = cloneString(groupName);                     //2
            }
        else
            {
            /* get group */
            safef(cartVar, sizeof(cartVar), "%s.group",track->track);
            groupName = cloneString(                                              //1
                    cartUsualString(cart, cartVar, track->defaultGroupName));
            }
        if (vis == -1)
            groupName = track->defaultGroupName;                                  //0
        track->groupName = cloneString(groupName);  // wasting a few clones!      //3
        if (sameString(groupName, track->defaultGroupName))
            cartRemove(cart, cartVar);

        /* get priority */
        safef(cartVar, sizeof(cartVar), "%s.priority",track->track);
        float priority = (float)cartUsualDouble(cart, cartVar,
                                                    track->defaultPriority);
        /* remove cart variables that are the same as the trackDb settings */
/*  UGLY - add me back when tdb->priority is no longer pre-clobbered by cart var value
        if (priority == track->defaultPriority)
            cartRemove(cart, cartVar);
*/
        track->priority = priority;
        }

    /* assign group object to track */
    if (track->groupName == NULL)
        group = NULL;
    else
	group = hashFindVal(hash, track->groupName);
    if (group == NULL)
        {
	if (unknown == NULL)
	    {
	    AllocVar(unknown);
	    unknown->name = cloneString("other");
	    unknown->label = cloneString("other");
	    unknown->priority = 1000000;
	    slAddHead(&list, unknown);
	    }
	group = unknown;
	}
    track->group = group;
    }

/* Sort tracks by combined group/track priority, and
 * then add references to track to group. */
slSort(pTrackList, tgCmpPriority);
for (track = *pTrackList; track != NULL; track = track->next)
    {
    AllocVar(tr);
    tr->track = track;
    slAddHead(&track->group->trackList, tr);
    }

/* Straighten things out, clean up, and go home. */
for (group = list; group != NULL; group = group->next)
    slReverse(&group->trackList);
slSort(&list, gCmpPriority);
hashFree(&hash);
*pGroupList = list;
}

void groupTrackListAddSuper(struct cart *cart, struct group *group)
/* Construct a new track list that includes supertracks, sort by priority,
 * and determine if supertracks have visible members.
 * Replace the group track list with this new list.
 * Shared by hgTracks and configure page to expand track list,
 * in contexts where no track display functions (which don't understand
 * supertracks) are invoked. */
{
struct trackRef *newList = NULL, *tr, *ref;
struct hash *superHash = hashNew(8);

if (!group || !group->trackList)
    return;
for (tr = group->trackList; tr != NULL; tr = tr->next)
    {
    struct track *track = tr->track;
    AllocVar(ref);
    ref->track = track;
    slAddHead(&newList, ref);
    if (tdbIsSuperTrackChild(track->tdb))
        {
        assert(track->tdb->parentName != NULL);
        if (hTvFromString(cartUsualString(cart, track->track,
                        hStringFromTv(track->tdb->visibility))) != tvHide)
            setSuperTrackHasVisibleMembers(track->tdb->parent);
        assert(track->parent == NULL);
        track->parent = hashFindVal(superHash, track->tdb->parentName);
        if (track->parent)
            continue;
        /* create track and reference for the supertrack */
        struct track *superTrack = track->parent = trackFromTrackDb(track->tdb->parent);
        track->parent = superTrack;
        if (trackHash != NULL)
            hashAddUnique(trackHash,superTrack->track,superTrack);
        superTrack->hasUi = TRUE;
        superTrack->group = group;
        superTrack->groupName = cloneString(group->name);
        superTrack->defaultGroupName = cloneString(group->name);

        /* handle track reordering */
        char cartVar[256];
        safef(cartVar, sizeof(cartVar), "%s.priority",track->tdb->parentName);
        float priority = (float)cartUsualDouble(cart, cartVar,
                                        track->tdb->parent->priority);
        /* remove cart variables that are the same as the trackDb settings */
        if (priority == track->tdb->parent->priority)
            cartRemove(cart, cartVar);
        superTrack->priority = priority;

        AllocVar(ref);
        ref->track = superTrack;
        slAddHead(&newList, ref);
        hashAdd(superHash, track->tdb->parentName, superTrack);
        }
    }
slSort(&newList, trackRefCmpPriority);
hashFree(&superHash);
/* we could free the old track list here, but it's a trivial amount of mem */
group->trackList = newList;
}

void topButton(char *var, char *label)
/* create a 3 or 4-char wide button for top line of display.
 * 3 chars wide for odd-length labels, 4 for even length.
 * Pad with spaces so label is centered */
{
char paddedLabel[5] = "    ";
int len = strlen(label);
if (len > 4)
    {
    /* truncate */
    /* or maybe errabort ? */
    label[3] = 0;
    len = 4;
    }
if (len % 2 != 0)
    paddedLabel[3] = 0;
if (len == strlen(paddedLabel))
    strcpy(paddedLabel, label);
else
    {
    int i;
    for (i=0; i<len; i++)
        paddedLabel[i+1] = label[i];
    }
#if IN_PLACE_UPDATE
hButtonWithOnClick(var, paddedLabel, NULL, "return navigateButtonClick(this);");
#else
hButton(var, paddedLabel);
#endif
}

void limitSuperTrackVis(struct track *track)
/* Limit track visibility by supertrack parent */
{
if(tdbIsSuperTrackChild(track->tdb))
    {
    assert(track->tdb->parent != NULL);
    if (sameString("hide", cartUsualString(cart, track->tdb->parent->track,
                                    track->tdb->parent->isShow ? "show" : "hide")))
        track->visibility = tvHide;
    }
}

struct track *rFindTrackWithTable(char *tableName, struct track *trackList)
/* Recursively search through track list looking for first one that matches table. */
{
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (sameString(tableName, track->table))
         return track;
    struct track *subTrack = rFindTrackWithTable(tableName, track->subtracks);
    if (subTrack != NULL)
         return subTrack;
    }
return NULL;
}

static void setSearchedTrackToPackOrFull(struct track *trackList)
/* Open track associated with search position if any.   Also open its parents
 * if any.  At the moment parents include composites but not supertracks. */
{
if (NULL != hgp && NULL != hgp->tableList && NULL != hgp->tableList->name)
    {
    char *tableName = hgp->tableList->name;
    struct track *matchTrack = rFindTrackWithTable(tableName, trackList);
    if (matchTrack != NULL)
	{
	struct track *track;
	for (track = matchTrack; track != NULL; track = track->parent)
	    cartSetString(cart, track->track, hCarefulTrackOpenVis(database, track->track));
	}
    }
}


struct track *getTrackList( struct group **pGroupList, int vis)
/* Return list of all tracks, organizing by groups.
 * If vis is -1, restore default groups to tracks.
 * Shared by hgTracks and configure page. */
{
struct track *track, *trackList = NULL;
registerTrackHandlers();
/* Load regular tracks, blatted tracks, and custom tracks.
 * Best to load custom last. */
loadFromTrackDb(&trackList);
if (pcrResultParseCart(database, cart, NULL, NULL, NULL))
    slSafeAddHead(&trackList, pcrResultTg());
if (userSeqString != NULL) slSafeAddHead(&trackList, userPslTg());
slSafeAddHead(&trackList, oligoMatchTg());
if (restrictionEnzymesOk())
    {
    slSafeAddHead(&trackList, cuttersTg());
    }
if (wikiTrackEnabled(database, NULL))
    {
    addWikiTrack(&trackList);
    struct sqlConnection *conn = wikiConnect();
    if (sqlTableExists(conn, "variome"))
        addVariomeWikiTrack(&trackList);
    wikiDisconnect(&conn);
    }

if (cartOptionalString(cart, "hgt.trackNameFilter") == NULL)
    { // If a single track was asked for and it is from a hub, then it is already in trackList
    loadTrackHubs(&trackList, &hubList);
    slReverse(&hubList);
    }
loadCustomTracks(&trackList);
groupTracks(hubList, &trackList, pGroupList, vis);
setSearchedTrackToPackOrFull(trackList);
if (cgiOptionalString( "hideTracks"))
    changeTrackVis(groupList, NULL, tvHide);

/* Get visibility values if any from ui. */
for (track = trackList; track != NULL; track = track->next)
    {
    char *s = cartOptionalString(cart, track->track);
    if (cgiOptionalString("hideTracks"))
	{
	s = cgiOptionalString(track->track);
	if (s != NULL && (hTvFromString(s) != track->tdb->visibility))
	    {
	    cartSetString(cart, track->track, s);
	    }
	}
    if (s != NULL && !track->limitedVisSet)
	track->visibility = hTvFromString(s);
    if (tdbIsCompositeChild(track->tdb))
        track->visibility = tdbVisLimitedByAncestry(cart, track->tdb, FALSE);
    else if (tdbIsComposite(track->tdb) && track->visibility != tvHide)
	{
	struct trackDb *parent = track->tdb->parent;
	char *parentShow = NULL;
	if (parent)
	    parentShow = cartUsualString(cart, parent->track,
			 parent->isShow ? "show" : "hide");
	if (!parent || sameString(parentShow, "show"))
	    compositeTrackVis(track);
	}
    }
return trackList;
}

void doNextPrevItem(boolean goNext, char *trackName)
/* In case a next item arrow was clicked on a track, change */
/* position (i.e. winStart, winEnd, etc.) based on what track it was */
{
struct track *track = trackFindByName(trackList, trackName);
if ((track != NULL) && (track->nextPrevItem != NULL))
    track->nextPrevItem(track, goNext);
}

char *collapseGroupVar(char *name)
/* Construct cart variable name for collapsing group */
{
static char varName[256];
safef(varName, sizeof(varName),
        "%s%s_%s_%s", "hgt", "group", name, "close");
return (cloneString(varName));
}

boolean isCollapsedGroup(struct group *grp)
/* Determine if group is collapsed */
{
return cartUsualInt(cart, collapseGroupVar(grp->name), grp->defaultIsClosed);
}

void collapseGroupGoodies(boolean isOpen, boolean wantSmallImage,
                                char **img, char **indicator, char **otherState)
/* Get image, char representation of image, and 'otherState' (1 or 0)
 * for a group, based on whether it is collapsed, and whether we want
 * larger or smaller image for collapse box */
{
if (otherState)
    *otherState = (isOpen ? "1" : "0");
if (indicator)
    *indicator = (isOpen ? "-" : "+");
if (img)
    {
    if (wantSmallImage)
        *img = (isOpen ? "../images/remove_sm.gif" : "../images/add_sm.gif");
    else
        *img = (isOpen ? "../images/remove.gif" : "../images/add.gif");
    }
}

void collapseGroup(char *name, boolean doCollapse)
/* Set cart variable to cause group to collapse */
{
cartSetBoolean(cart, collapseGroupVar(name), doCollapse);
}

void myControlGridStartCell(struct controlGrid *cg, boolean isOpen, char *id)
/* Start a new cell in control grid; support Javascript open/collapsing by including id's in tr's.
   id is used as id prefix (a counter is added to make id's unique). */
{
static int counter = 1;
if (cg->columnIx == cg->columns)
    controlGridEndRow(cg);
if (!cg->rowOpen)
    {
#if 0
    /* This is unnecessary, b/c we can just use a blank display attribute to show the element rather
       than figuring out what the browser specific string is to turn on display of the tr;
       however, we may want to put in browser specific strings in the future, so I'm leaving this
       code in as a reference. */
    char *ua = getenv("HTTP_USER_AGENT");
    char *display = ua && stringIn("MSIE", ua) ? "block" : "table-row";
#endif
    // use counter to ensure unique tr id's (prefix is used to find tr's in javascript).
    printf("<tr %sid='%s-%d'>", isOpen ? "" : "style='display: none' ", id, counter++);
    cg->rowOpen = TRUE;
    }
if (cg->align)
    printf("<td align=%s>", cg->align);
else
    printf("<td>");
}

static void pruneRedundantCartVis(struct track *trackList)
/* When the config page or track form has been submitted, there usually
 * are many track visibility cart variables that have not been changed
 * from the default.  To keep down cart bloat, prune those out before we
 * save the cart.  changeTrackVis does this too, but this is for the
 * more common case where track visibilities are tweaked. */
{
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    char *cartVis = cartOptionalString(cart, track->track);
    if (cartVis != NULL && hTvFromString(cartVis) == track->tdb->visibility)
    cartRemove(cart, track->track);
    }
}

static int getMaxWindowToDraw(struct trackDb *tdb)
/* If trackDb setting maxWindowToDraw exists and is a sensible size, return it, else 0. */
{
if (tdb == NULL)
    return 0;
char *maxWinToDraw = trackDbSettingClosestToHome(tdb, "maxWindowToDraw");
if (isNotEmpty(maxWinToDraw))
    {
    unsigned maxWTD = sqlUnsigned(maxWinToDraw);
    if (maxWTD > 1)
    return maxWTD;
    }
return 0;
}

static void drawMaxWindowWarning(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis)
/* This is a stub drawItems handler to be swapped in for the usual drawItems when the window
 * size is larger than the threshold specified by trackDb setting maxWindowToDraw. */
{
int maxWinToDraw = getMaxWindowToDraw(tg->tdb);
char commafied[256];
sprintLongWithCommas(commafied, maxWinToDraw);
char message[512];
safef(message, sizeof(message), "zoom in to <= %s bases to view items", commafied);
Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
hvGfxBox(hvg, xOff, yOff, width, tg->heightPer, yellow);
hvGfxTextCentered(hvg, xOff, yOff, width, tg->heightPer, MG_BLACK, font, message);
}

static void checkMaxWindowToDraw(struct track *tg)
/* If (winEnd - winStart) > trackDb setting maxWindowToDraw, force track to a dense line
 * that will ask the user to zoom in closer to see track items and return TRUE so caller
 * can skip loading items. */
{
int maxWinToDraw = getMaxWindowToDraw(tg->tdb);
if (tdbIsComposite(tg->tdb))
    {
    struct track *subtrack;
    for (subtrack = tg->subtracks;  subtrack != NULL;  subtrack = subtrack->next)
	{
	if (!isSubtrackVisible(subtrack))
	    continue;
	maxWinToDraw = getMaxWindowToDraw(subtrack->tdb);
	if (maxWinToDraw > 1 && (winEnd - winStart) > maxWinToDraw)
	    {
	    subtrack->loadItems = dontLoadItems;
	    subtrack->drawItems = drawMaxWindowWarning;
	    subtrack->limitedVis = tvDense;
	    subtrack->limitedVisSet = TRUE;
	    }
	}
    }
else if (maxWinToDraw > 1 && (winEnd - winStart) > maxWinToDraw)
    {
    tg->loadItems = dontLoadItems;
    tg->drawItems = drawMaxWindowWarning;
    tg->limitedVis = tvDense;
    tg->limitedVisSet = TRUE;
    }
}

void printTrackInitJavascript(struct track *trackList)
{
hPrintf("<input type='hidden' id='%s' name='%s' value=''>\n", hgtJsCommand, hgtJsCommand);
hPrintf("<script type='text/javascript'>\n");
hPrintf( "function hgTracksInitTracks()\n{\n");

struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (startsWithWord("makeItems", track->tdb->type) )
        hPrintf("setUpMakeItemsDrag(\"%s\");\n", track->track);
    }

hPrintf( "}\n");
hPrintf("</script>\n");
}

void jsCommandDispatch(char *command, struct track *trackList)
/* Dispatch a command sent to us from some javaScript event.
 * This gets executed after the track list is built, but before
 * the track->loadItems methods are called.  */
{
if (startsWithWord("makeItems", command))
    makeItemsJsCommand(command, trackList, trackHash);
else
    warn("Unrecognized jsCommand %s", command);
}

void parentChildCartCleanup(struct track *trackList,struct cart *newCart,struct hash *oldVars)
/* When composite/view settings changes, remove subtrack specific vis
   When superTrackChild is found and selected, shape superTrack to match. */
{
struct lm *lm = lmInit(0);	/* Speed tweak cleanup with scatch memory pool. */
struct track *track = trackList;
for (;track != NULL; track = track->next)
    {
    boolean shapedByubtrackOverride = FALSE;
    boolean cleanedByContainerSettings = FALSE;

    // Top-down 'cleanup' MUST GO BEFORE bottom up reshaping.
    cleanedByContainerSettings = cartTdbTreeCleanupOverrides(track->tdb,newCart,oldVars, lm);

    if (tdbIsContainer(track->tdb))
        {
        shapedByubtrackOverride = cartTdbTreeReshapeIfNeeded(cart,track->tdb);
        if(shapedByubtrackOverride)
            track->visibility = tdbVisLimitedByAncestors(cart,track->tdb,TRUE,TRUE);
        }

    if ((shapedByubtrackOverride || cleanedByContainerSettings) && tdbIsSuperTrackChild(track->tdb))  // Either cleanup may require supertrack intervention
        { // Need to update track visibility
        // Unfortunately, since supertracks are not in trackList, this occurs on superChildren,
        // So now we need to find the supertrack and take changed cart values of its children
        struct slRef *childRef;
        for(childRef = track->tdb->parent->children;childRef != NULL;childRef = childRef->next)
            {
            struct trackDb * childTdb = childRef->val;
            struct track *child = hashFindVal(trackHash, childTdb->track);
            char *cartVis = cartOptionalString(cart,child->track);
            if (cartVis)
                child->visibility = hTvFromString(cartVis);
            }
        }
    }
lmCleanup(&lm);
}


struct paraFetchData
    {
    struct paraFetchData *next;
    struct track *track;
    boolean done;
    };

static boolean isTrackForParallelLoad(struct track *track)
/* Is this a track that should be loaded in parallel ? */
{
char *bdu = trackDbSetting(track->tdb, "bigDataUrl");
return (startsWithWord("bigWig"  , track->tdb->type)
     || startsWithWord("bigBed"  , track->tdb->type)
     || startsWithWord("bam"     , track->tdb->type)
     || startsWithWord("vcfTabix", track->tdb->type))
     && (bdu && strstr(bdu,"://"))
     && (track->subtracks == NULL);
}

static void findLeavesForParallelLoad(struct track *trackList, struct paraFetchData **ppfdList)
/* Find leaves of track tree that are remote network resources for parallel-fetch loading */
{
struct track *track;
if (!trackList)
    return;
for (track = trackList; track != NULL; track = track->next)
    {

    if (track->visibility != tvHide)
	{
	if (isTrackForParallelLoad(track))
	    {
	    struct paraFetchData *pfd;
	    AllocVar(pfd);
	    pfd->track = track;  // need pointer to be stable
	    slAddHead(ppfdList, pfd);
	    track->parallelLoading = TRUE;
	    }
	struct track *subtrack;
        for (subtrack=track->subtracks; subtrack; subtrack=subtrack->next)
	    {
	    if (isTrackForParallelLoad(subtrack))
		{
		if (tdbVisLimitedByAncestors(cart,subtrack->tdb,TRUE,TRUE) != tvHide)
		    {
		    struct paraFetchData *pfd;
		    AllocVar(pfd);
		    pfd->track = subtrack;  // need pointer to be stable
		    slAddHead(ppfdList, pfd);
		    subtrack->parallelLoading = TRUE;
		    }
		}
	    }
	}
    }
}

static pthread_mutex_t pfdMutex = PTHREAD_MUTEX_INITIALIZER;
static struct paraFetchData *pfdList = NULL, *pfdRunning = NULL, *pfdDone = NULL, *pfdNeverStarted = NULL;

static void *remoteParallelLoad(void *threadParam)
/* Each thread loads tracks in parallel until all work is done. */
{
pthread_t *pthread = threadParam;
struct paraFetchData *pfd = NULL;
pthread_detach(*pthread);  // this thread will never join back with it's progenitor
    // Canceled threads that might leave locks behind,
    // so the theads are detached and will be neither joined nor canceled.
boolean allDone = FALSE;
while(1)
    {
    pthread_mutex_lock( &pfdMutex );
    if (!pfdList)
	{
	allDone = TRUE;
	}
    else
	{  // move it from the waiting queue to the running queue
	pfd = slPopHead(&pfdList);
	slAddHead(&pfdRunning, pfd);
	}
    pthread_mutex_unlock( &pfdMutex );
    if (allDone)
	return NULL;

    long thisTime = 0, lastTime = 0;

    if (measureTiming)
	lastTime = clock1000();

    /* protect against errAbort */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	pfd->done = FALSE;
	checkMaxWindowToDraw(pfd->track);
	pfd->track->loadItems(pfd->track);
	pfd->done = TRUE;
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	pfd->track->networkErrMsg = cloneString(errCatch->message->string);
	pfd->done = TRUE;
	}
    errCatchFree(&errCatch);

    if (measureTiming)
	{
	thisTime = clock1000();
	pfd->track->loadTime = thisTime - lastTime;
	}

    pthread_mutex_lock( &pfdMutex );
    slRemoveEl(&pfdRunning, pfd);  // this list will not be huge
    slAddHead(&pfdDone, pfd);
    pthread_mutex_unlock( &pfdMutex );

    }
}

static int remoteParallelLoadWait(int maxTimeInSeconds)
/* Wait, checking to see if finished (completed or errAborted).
 * If timed-out or never-ran, record error status.
 * Return error count. */
{
int maxTimeInMilliseconds = 1000 * maxTimeInSeconds;
struct paraFetchData *pfd;
int errCount = 0;
int waitTime = 0;
while(1)
    {
    sleep1000(50); // milliseconds
    waitTime += 50;
    boolean done = TRUE;
    pthread_mutex_lock( &pfdMutex );
    if (pfdList || pfdRunning)
	done = FALSE;
    pthread_mutex_unlock( &pfdMutex );
    if (done)
        break;
    if (waitTime >= maxTimeInMilliseconds)
        break;
    }
pthread_mutex_lock( &pfdMutex );
pfdNeverStarted = pfdList;
pfdList = NULL;  // stop the workers from starting any more waiting track loads
for (pfd = pfdNeverStarted; pfd; pfd = pfd->next)
    {
    // track was never even started
    char temp[256];
    safef(temp, sizeof temp, "Ran out of time (%d milliseconds) unable to process  %s", maxTimeInMilliseconds, pfd->track->track);
    pfd->track->networkErrMsg = cloneString(temp);
    ++errCount;
    }
for (pfd = pfdRunning; pfd; pfd = pfd->next)
    {
    // unfinished track
    char temp[256];
    safef(temp, sizeof temp, "Timeout %d milliseconds exceeded processing %s", maxTimeInMilliseconds, pfd->track->track);
    pfd->track->networkErrMsg = cloneString(temp);
    ++errCount;
    }
for (pfd = pfdDone; pfd; pfd = pfd->next)
    {
    // some done tracks may have errors
    if (pfd->track->networkErrMsg)
        ++errCount;
    }
pthread_mutex_unlock( &pfdMutex );
return errCount;
}

static void printTrackTiming()
{
hPrintf("<span class='trackTiming'>track, load time, draw time, total<br />\n");
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->visibility == tvHide)
        continue;
    if (trackIsCompositeWithSubtracks(track))  //TODO: Change when tracks->subtracks are always set for composite
        {
        struct track *subtrack;
        for (subtrack = track->subtracks; subtrack != NULL;
             subtrack = subtrack->next)
            if (isSubtrackVisible(subtrack))
                hPrintf("%s, %d, %d, %d<br />\n", subtrack->shortLabel,
                            subtrack->loadTime, subtrack->drawTime,
                            subtrack->loadTime + subtrack->drawTime);
        }
    else
        {
        hPrintf("%s, %d, %d, %d<br />\n",
		    track->shortLabel, track->loadTime, track->drawTime,
		    track->loadTime + track->drawTime);
        if (startsWith("wigMaf", track->tdb->type))
            if (track->subtracks)
                if (track->subtracks->loadTime)
                    hPrintf("&nbsp; &nbsp; %s wiggle, load %d<br />\n",
                                track->shortLabel, track->subtracks->loadTime);
        }
    }
hPrintf("</span>\n");
}


void doTrackForm(char *psOutput, struct tempName *ideoTn)
/* Make the tracks display form with the zoom/scroll buttons and the active
 * image.  If the ideoTn parameter is not NULL, it is filled in if the
 * ideogram is created.  */
{
struct group *group;
struct track *track;
char *freezeName = NULL;
boolean hideAll = cgiVarExists("hgt.hideAll");
boolean defaultTracks = cgiVarExists("hgt.reset");
boolean showedRuler = FALSE;
boolean showTrackControls = cartUsualBoolean(cart, "trackControlsOnMain", TRUE);
long thisTime = 0, lastTime = 0;
char *clearButtonJavascript;

basesPerPixel = ((float)winBaseCount) / ((float)insideWidth);
zoomedToBaseLevel = (winBaseCount <= insideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(winBaseCount/3) * tl.mWidth) <= insideWidth;
zoomedToCdsColorLevel = (winBaseCount <= insideWidth*3);

if (psOutput != NULL)
   {
   hPrintDisable();
   hideControls = TRUE;
   withNextItemArrows = FALSE;
   withNextExonArrows = FALSE;
   hgFindMatches = NULL;
   }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackHeaderForm\" id=\"TrackHeaderForm\" METHOD=\"GET\">\n\n", hgTracksName());
jsonHashAddNumber(jsonForClient, "insideX", insideX);
jsonHashAddBoolean(jsonForClient, "revCmplDisp", revCmplDisp);

#ifdef NEW_JQUERY
hPrintf("<script type='text/javascript'>var newJQuery=true;</script>\n");
#else///ifndef NEW_JQUERY
hPrintf("<script type='text/javascript'>var newJQuery=false;</script>\n");
#endif///ndef NEW_JQUERY
if (hPrintStatus()) cartSaveSession(cart);
clearButtonJavascript = "document.TrackHeaderForm.position.value=''; document.getElementById('suggest').value='';";

/* See if want to include sequence search results. */
userSeqString = cartOptionalString(cart, "ss");
if (userSeqString && !ssFilesExist(userSeqString))
    {
    userSeqString = NULL;
    cartRemove(cart, "ss");
    }
if (!hideControls)
    hideControls = cartUsualBoolean(cart, "hideControls", FALSE);
if (measureTiming)
    measureTime("Time before getTrackList");
trackList = getTrackList(&groupList, defaultTracks ? -1 : -2);
if (measureTiming)
    measureTime("getTrackList");
makeGlobalTrackHash(trackList);
/* Tell tracks to load their items. */


// honor defaultImgOrder
if(cgiVarExists("hgt.defaultImgOrder"))
    {
    char wildCard[32];
    safef(wildCard,sizeof(wildCard),"*_%s",IMG_ORDER_VAR);
    cartRemoveLike(cart, wildCard);
    }
parentChildCartCleanup(trackList,cart,oldVars); // Subtrack settings must be removed when composite/view settings are updated
if (measureTiming)
    measureTime("parentChildCartCleanup");


/* Honor hideAll and visAll variables */
if (hideAll || defaultTracks)
    {
    int vis = (hideAll ? tvHide : -1);
    changeTrackVis(groupList, NULL, vis);
    }

/* Before loading items, deal with the next/prev item arrow buttons if pressed. */
if (cgiVarExists("hgt.nextItem"))
    doNextPrevItem(TRUE, cgiUsualString("hgt.nextItem", NULL));
else if (cgiVarExists("hgt.prevItem"))
    doNextPrevItem(FALSE, cgiUsualString("hgt.prevItem", NULL));

if(advancedJavascriptFeaturesEnabled(cart) && !psOutput && !cartUsualBoolean(cart, "hgt.imageV1", FALSE))
    {
    // Start an imagebox (global for now to avoid huge rewrite of hgTracks)
    // Set up imgBox dimensions
    int sideSliceWidth  = 0;   // Just being explicit
    if (withLeftLabels)
        sideSliceWidth   = (insideX - gfxBorder*3) + 2;
    theImgBox = imgBoxStart(database,chromName,winStart,winEnd,(!revCmplDisp),sideSliceWidth,tl.picWidth);
    #ifdef IMAGEv2_DRAG_SCROLL
    // Define a portal with a default expansion size, then set the global dimensions to the full image size
    if(imgBoxPortalDefine(theImgBox,&winStart,&winEnd,&(tl.picWidth),0))
        {
        winBaseCount = winEnd - winStart;
        insideWidth = tl.picWidth-gfxBorder-insideX;
        }
    #endif//def IMAGEv2_DRAG_SCROLL
    }

char *jsCommand = cartCgiUsualString(cart, hgtJsCommand, "");
if (!isEmpty(jsCommand))
   {
   cartRemove(cart, hgtJsCommand);
   jsCommandDispatch(jsCommand, trackList);
   }


/* adjust visibility */
for (track = trackList; track != NULL; track = track->next)
    {
    /* adjust track visibility based on supertrack just before load loop */
    if (tdbIsSuperTrackChild(track->tdb))
        limitSuperTrackVis(track);

    /* remove cart priority variables if they are set
       to the default values in the trackDb */
    if(!hTrackOnChrom(track->tdb, chromName))
	{
	track->limitedVis = tvHide;
	track->limitedVisSet = TRUE;
	}
    }
/* pre-load remote tracks in parallel */
int ptMax = atoi(cfgOptionDefault("parallelFetch.threads", "20"));  // default number of threads for parallel fetch.
pthread_t *threads = NULL;
if (ptMax > 0)     // parallelFetch.threads=0 to disable parallel fetch
    {
    findLeavesForParallelLoad(trackList, &pfdList);
    /* launch parallel threads */
    ptMax = min(ptMax, slCount(pfdList));
    if (ptMax > 0)
	{
	AllocArray(threads, ptMax);
	int pt;
	for (pt = 0; pt < ptMax; ++pt)
	    {
	    int rc = pthread_create(&threads[pt], NULL, remoteParallelLoad, &threads[pt]);
	    if (rc)
		{
		errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
		}
	    }
	}
    }
/* load regular tracks */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->visibility != tvHide)
	{
	if (!track->parallelLoading)
	    {
	    if (measureTiming)
		lastTime = clock1000();

	    checkMaxWindowToDraw(track);
	    track->loadItems(track);

	    if (measureTiming)
		{
		thisTime = clock1000();
		track->loadTime = thisTime - lastTime;
		}
	    }
	}
    }
if (ptMax > 0)
    {
    /* wait for remote parallel load to finish */
    remoteParallelLoadWait(atoi(cfgOptionDefault("parallelFetch.timeout", "90")));  // wait up to default 90 seconds.
    if (measureTiming)
	measureTime("Waiting for parallel (%d thread) remote data fetch", ptMax);
    }

printTrackInitJavascript(trackList);

/* Generate two lists of hidden variables for track group visibility.  Kludgy,
   but required b/c we have two different navigation forms on this page, but
   we want open/close changes in the bottom form to be submitted even if the user
   submits via the top form. */
struct dyString *trackGroupsHidden1 = newDyString(1000);
struct dyString *trackGroupsHidden2 = newDyString(1000);
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->trackList != NULL)
        {
        int looper;
        for(looper=1;looper<=2;looper++)
            {
            boolean isOpen = !isCollapsedGroup(group);
            char buf[1000];
            safef(buf, sizeof(buf), "<input type='hidden' name=\"%s\" id=\"%s_%d\" value=\"%s\">\n", collapseGroupVar(group->name), collapseGroupVar(group->name), looper, isOpen ? "0" : "1");
            dyStringAppend(looper == 1 ? trackGroupsHidden1 : trackGroupsHidden2, buf);
            }
        }
    }

#ifdef IMAGEv2_DRAG_SCROLL
if(theImgBox)
    {
    // If a portal was established, then set the global dimensions back to the portal size
    if(imgBoxPortalDimensions(theImgBox,NULL,NULL,NULL,NULL,&winStart,&winEnd,&(tl.picWidth),NULL))
        {
        winBaseCount = winEnd - winStart;
        insideWidth = tl.picWidth-gfxBorder-insideX;
        }
    }
#endif//def IMAGEv2_DRAG_SCROLL
/* Center everything from now on. */
hPrintf("<CENTER>\n");

// info for drag selection javascript
jsonHashAddNumber(jsonForClient, "winStart", winStart);
jsonHashAddNumber(jsonForClient, "winEnd", winEnd);
jsonHashAddString(jsonForClient, "chromName", chromName);

if(trackImgOnly && !ideogramToo)
    {
    struct track *ideoTrack = chromIdeoTrack(trackList);
    if (ideoTrack)
        {
        ideoTrack->limitedVisSet = TRUE;
        ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
        }
    makeActiveImage(trackList, psOutput);
    fflush(stdout);
    return;  // bail out b/c we are done
    }


if (!hideControls)
    {
    /* set white-space to nowrap to prevent buttons from wrapping when screen is
     * narrow */
    hPrintf("<DIV STYLE=\"white-space:nowrap;\">\n");
    hotLinks();

    /* Show title . */
    freezeName = hFreezeFromDb(database);
    if(freezeName == NULL)
    freezeName = "Unknown";
    hPrintf("<span style='font-size:x-large;'><B>");
    if (startsWith("zoo",database) )
	{
	hPrintf("%s %s on %s June 2002 Assembly %s target1",
	    organization, browserName, organism, freezeName);
	}
    else
	{
	if (sameString(organism, "Archaea"))
	    {
	    hPrintf("%s %s on Archaeon %s Assembly",
		organization, browserName, freezeName);
	    }
	else
	    {
	    if (stringIn(database, freezeName))
		hPrintf("%s %s on %s %s Assembly",
			organization, browserName, organism, freezeName);
	    else
		hPrintf("%s %s on %s %s Assembly (%s)",
			organization, browserName, organism, freezeName, database);
	    }
	}
    hPrintf("</B></span><BR>\n");

    /* This is a clear submit button that browsers will use by default when enter is pressed in position box. */
    hPrintf("<INPUT TYPE=IMAGE BORDER=0 NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\">");
    /* Put up scroll and zoom controls. */
#ifndef USE_NAVIGATION_LINKS
    hWrites("move ");
#if IN_PLACE_UPDATE
    hButtonWithOnClick("hgt.left3", "<<<", "move 95% to the left", "return navigateButtonClick(this);");
    hButtonWithOnClick("hgt.left2", " <<", "move 47.5% to the left", "return navigateButtonClick(this);");
    hButtonWithOnClick("hgt.left1", " < ", "move 10% to the left", "return navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right1", " > ", "move 10% to the right", "return navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right2", ">> ", "move 47.5% to the right", "return navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right3", ">>>", "move 95% to the right", "return navigateButtonClick(this);");
#else
    hButtonWithMsg("hgt.left3", "<<<", "move 95% to the left");
    hButtonWithMsg("hgt.left2", " <<", "move 47.5% to the left");
    hButtonWithMsg("hgt.left1", " < ", "move 10% to the left");
    hButtonWithMsg("hgt.right1", " > ", "move 10% to the right");
    hButtonWithMsg("hgt.right2", ">> ", "move 47.5% to the right");
    hButtonWithMsg("hgt.right3", ">>>", "move 95% to the right");
#endif
    hWrites(" zoom in ");
    /* use button maker that determines padding, so we can share constants */
    topButton("hgt.in1", ZOOM_1PT5X);
    topButton("hgt.in2", ZOOM_3X);
    topButton("hgt.in3", ZOOM_10X);
    topButton("hgt.inBase", ZOOM_BASE);
    hWrites(" zoom out ");
    topButton("hgt.out1", ZOOM_1PT5X);
    topButton("hgt.out2", ZOOM_3X);
    topButton("hgt.out3", ZOOM_10X);
    hWrites("<div style='height:1em;'></div>\n");
#endif//ndef USE_NAVIGATION_LINKS

    if (showTrackControls)
	{
	/* Break into a second form so that zooming and scrolling
	 * can be done with a 'GET' so that user can back up from details
	 * page without Internet Explorer popping up an annoying dialog.
	 * Do rest of page as a 'POST' so that the ultra-long URL from
	 * all the track controls doesn't break things.  IE URL limit
	 * is 2000 bytes, but some firewalls impose a ~1000 byte limit.
	 * As a side effect of breaking up the page into two forms
	 * we need to repeat the position in a hidden variable here
	 * so that zoom/scrolling always has current position to work
	 * from. */
    #if IN_PLACE_UPDATE
        // This 'dirty' field is used to check if js/ajax changes to the page have occurred.
        // If so and it is reached by the back button, a page reload will occur instead.
        hPrintf("<INPUT TYPE='text' style='display:none;' name='dirty' id='dirty' VALUE='false'>\n");
        // Unfortunately this does not work in IE, so that browser will get the reload only after this full load.
        // NOTE: Larry and I have seen that the new URL is not even used, but this will abort the page load and hasten the isDirty() check in hgTracks.js
        hPrintf("<script type='text/javascript'>if (document.getElementById('dirty').value == 'true') {document.getElementById('dirty').value = 'false'; window.location = '%s?hgsid=%d';}</script>\n",hgTracksName(),cart->sessionId);
    #endif/// IN_PLACE_UPDATE
	hPrintf("<INPUT TYPE=HIDDEN id='positionHidden' NAME=\"position\" "
	    "VALUE=\"%s:%d-%d\">", chromName, winStart+1, winEnd);
	    hPrintf("\n%s", trackGroupsHidden1->string);
	hPrintf("</CENTER></FORM>\n");
	hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" id=\"TrackForm\" METHOD=\"POST\">\n\n", hgTracksName());
	    hPrintf("%s", trackGroupsHidden2->string);
	    freeDyString(&trackGroupsHidden1);
	    freeDyString(&trackGroupsHidden2);
	if (!psOutput) cartSaveSession(cart);   /* Put up hgsid= as hidden variable. */
	clearButtonJavascript = "document.TrackForm.position.value=''; document.getElementById('suggest').value='';";
	hPrintf("<CENTER>");
	}


    /* Make line that says position. */
	{
	char buf[256];
	char *survey = cfgOptionEnv("HGDB_SURVEY", "survey");
	char *surveyLabel = cfgOptionEnv("HGDB_SURVEY_LABEL", "surveyLabel");
	    char *javascript = "onchange=\"document.location = '/cgi-bin/hgTracks?db=' + document.TrackForm.db.options[document.TrackForm.db.selectedIndex].value;\"";
	    if (containsStringNoCase(database, "zoo"))
		{
		hPuts("Organism ");
		printAssemblyListHtmlExtra(database, javascript);
		}

	sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);
	position = cloneString(buf);
	hWrites("position/search ");
	hTextVar("position", addCommasToPos(database, position), 30);
	sprintLongWithCommas(buf, winEnd - winStart);
	if(dragZooming && assemblySupportsGeneSuggest(database))
            hPrintf(" <a title='click for help on gene search box' target='_blank' href='../goldenPath/help/geneSearchBox.html'>gene</a> "
                    "<input type='text' size='8' name='hgt.suggest' id='suggest'>\n"
                    "<input type='hidden' name='hgt.suggestTrack' id='suggestTrack' value='%s'>\n", assemblyGeneSuggestTrack(database)
                    );
	hWrites(" ");
	hButtonWithOnClick("hgt.jump", "jump", NULL, "jumpButtonOnClick()");
	hOnClickButton(clearButtonJavascript,"clear");
	hPrintf(" size <span id='size'>%s</span> bp. ", buf);
	hWrites(" ");
	hButton("hgTracksConfigPage", "configure");
	if (survey && differentWord(survey, "off"))
            hPrintf("&nbsp;&nbsp;<span style='background-color:yellow;'><A HREF='%s' TARGET=_BLANK><EM><B>%s</EM></B></A></span>\n", survey, surveyLabel ? surveyLabel : "Take survey");
	hPutc('\n');
	}
    }

/* Make chromsome ideogram gif and map. */
makeChromIdeoImage(&trackList, psOutput, ideoTn);

#ifdef USE_NAVIGATION_LINKS
    hPrintf("<TABLE BORDER=0 CELLPADDING=0 width='%d'><tr style='font-size:small;'>\n",tl.picWidth);//min(tl.picWidth, 800));
    hPrintf("<td width='40' align='left'><a href='?hgt.left3=1' title='move 95&#37; to the left'>&lt;&lt;&lt;</a>\n");
    hPrintf("<td width='30' align='left'><a href='?hgt.left2=1' title='move 47.5&#37; to the left'>&lt;&lt;</a>\n");
    #ifdef IMAGEv2_DRAG_SCROLL
    if(!advancedJavascriptFeaturesEnabled(cart))
    #endif//def IMAGEv2_DRAG_SCROLL
        hPrintf("<td width='20' align='left'><a href='?hgt.left1=1' title='move 10&#37; to the left'>&lt;</a>\n");

    hPrintf("<td>&nbsp;</td>\n"); // Without 'width=' this cell expand to table with, forcing other cells to the sides.
    hPrintf("<td width='40' align='left'><a href='?hgt.in1=1' title='zoom in 1.5x'>&gt;&nbsp;&lt;</a>\n");
    hPrintf("<td width='60' align='left'><a href='?hgt.in2=1' title='zoom in 3x'>&gt;&gt;&nbsp;&lt;&lt;</a>\n");
    hPrintf("<td width='80' align='left'><a href='?hgt.in3=1' title='zoom in 10x'>&gt;&gt;&gt;&nbsp;&lt;&lt;&lt;</a>\n");
    hPrintf("<td width='40' align='left'><a href='?hgt.inBase=1' title='zoom in to base range'>&gt;<i>base</i>&lt;</a>\n");

    hPrintf("<td>&nbsp;</td>\n"); // Without 'width=' this cell expand to table with, forcing other cells to the sides.
    hPrintf("<td width='40' align='right'><a href='?hgt.out1=1' title='zoom out 1.5x'>&lt;&nbsp;&gt;</a>\n");
    hPrintf("<td width='60' align='right'><a href='?hgt.out2=1' title='zoom out 3x'>&lt;&lt;&nbsp;&gt;&gt;</a>\n");
    hPrintf("<td width='80' align='right'><a href='?hgt.out3=1' title='zoom out 10x'>&lt;&lt;&lt;&nbsp;&gt;&gt;&gt;</a>\n");
        hPrintf("<td>&nbsp;</td>\n"); // Without 'width=' this cell expand to table with, forcing other cells to the sides.
    #ifdef IMAGEv2_DRAG_SCROLL
    if(!advancedJavascriptFeaturesEnabled(cart))
    #endif//ndef IMAGEv2_DRAG_SCROLL
        hPrintf("<td width='20' align='right'><a href='?hgt.right1=1' title='move 10&#37; to the right'>&gt;</a>\n");

    hPrintf("<td width='30' align='right'><a href='?hgt.right2=1' title='move 47.5&#37; to the right'>&gt;&gt;</a>\n");
    hPrintf("<td width='40' align='right'><a href='?hgt.right3=1' title='move 95&#37; to the right'>&gt;&gt;&gt;</a>\n");
    hPrintf("</tr></table>\n");
#endif//def USE_NAVIGATION_LINKS

/* Make clickable image and map. */
makeActiveImage(trackList, psOutput);
fflush(stdout);

if(trackImgOnly)
    {
    // bail out b/c we are done
    if (measureTiming)
        {
        printTrackTiming();
        }
    return;
    }

if (!hideControls)
    {
    struct controlGrid *cg = NULL;

    /* note a trick of WIDTH=27 going on here.  The 6,15,6 widths following
     * go along with this trick */
    hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1 WIDTH=%d COLS=%d><TR>\n",
        tl.picWidth, 27);
#ifndef USE_NAVIGATION_LINKS
    hPrintf("<TD COLSPAN=6 ALIGN=left NOWRAP>");
    hPrintf("move start<BR>");
#if IN_PLACE_UPDATE
    hButtonWithOnClick("hgt.dinkLL", " < ", "move start position to the left", "return navigateButtonClick(this);");
    hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
    hButtonWithOnClick("hgt.dinkLR", " > ", "move start position to the right", "return navigateButtonClick(this);");
#else
    hButton("hgt.dinkLL", " < ");
    hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
    hButton("hgt.dinkLR", " > ");
#endif
    hPrintf("</TD>");
    hPrintf("<td width='30'>&nbsp;</td>\n");
#endif//ndef USE_NAVIGATION_LINKS
    hPrintf("<TD COLSPAN=15 style=\"white-space:normal\">"); // allow this text to wrap
    hWrites("Click on a feature for details. ");
    hWrites(dragZooming ? "Click or drag in the base position track to zoom in. " : "Click on base position to zoom in around cursor. ");
    hWrites("Click side bars for track options. ");
    hWrites("Drag side bars or labels up or down to reorder tracks. ");
#ifdef IMAGEv2_DRAG_SCROLL
    hWrites("Drag tracks left or right to new position. ");
#endif//def IMAGEv2_DRAG_SCROLL
//#if !defined(IMAGEv2_DRAG_SCROLL) && !defined(USE_NAVIGATION_LINKS)
    hPrintf("</TD>");
#ifndef USE_NAVIGATION_LINKS
    hPrintf("<td width='30'>&nbsp;</td>\n");
    hPrintf("<TD COLSPAN=6 ALIGN=right NOWRAP>");
    hPrintf("move end<BR>");
#if IN_PLACE_UPDATE
    hButtonWithOnClick("hgt.dinkRL", " < ", "move end position to the left", "return navigateButtonClick(this);");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButtonWithOnClick("hgt.dinkRR", " > ", "move end position to the right", "return navigateButtonClick(this);");
#else
    hButton("hgt.dinkRL", " < ");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButton("hgt.dinkRR", " > ");
#endif
    hPrintf("</TD>");
#endif//ndef USE_NAVIGATION_LINKS
    hPrintf("</TR></TABLE>\n");

    /* Display bottom control panel. */

    if(isSearchTracksSupported(database,cart))
        {
        cgiMakeButtonWithMsg(TRACK_SEARCH, TRACK_SEARCH_BUTTON,TRACK_SEARCH_HINT);
        hPrintf(" ");
        }
    hButtonWithMsg("hgt.reset", "default tracks","Display only default tracks");
	hPrintf("&nbsp;");
    hButtonWithMsg("hgt.defaultImgOrder", "default order","Display current tracks in their default order");
    // if (showTrackControls)  - always show "hide all", Hiram 2008-06-26
	{
	hPrintf("&nbsp;");
	hButtonWithMsg("hgt.hideAll", "hide all","Hide all currently visibile tracks");
	}

    hPrintf(" ");
    hPrintf("<INPUT TYPE='button' VALUE='%s' onClick='document.customTrackForm.submit();return false;' title='%s'>",
        hasCustomTracks ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL,
        hasCustomTracks ? "Manage your custom tracks" : "Add your own custom tracks");

    hPrintf(" ");
    hPrintf("<INPUT TYPE='button' VALUE='track hubs' onClick='document.trackHubForm.submit();return false;' title='Import tracks from hubs'>");

    hPrintf(" ");
    hButtonWithMsg("hgTracksConfigPage", "configure","Configure image and track selection");
    hPrintf(" ");

    if (!hIsGsidServer())
	{
        hButtonWithMsg("hgt.toggleRevCmplDisp", "reverse",
            revCmplDisp?"Show forward strand at this location":"Show reverse strand at this location");
        hPrintf(" ");
	}

    hButtonWithOnClick("hgt.setWidth", "resize", "Resize image width to browser window size", "hgTracksSetWidth()");
    hPrintf(" ");

    hButtonWithMsg("hgt.refresh", "refresh","Refresh image");

    hPrintf("<BR>\n");

    if( chromosomeColorsMade )
        {
        hPrintf("<B>Chromosome Color Key:</B><BR> ");
        hPrintf("<IMG SRC = \"../images/new_colorchrom.gif\" BORDER=1 WIDTH=596 HEIGHT=18 ><BR>\n");
        }

    if (showTrackControls)
	{
	/* Display viewing options for each track. */
	/* Chuck: This is going to be wrapped in a table so that
	 * the controls don't wrap around randomly */
	hPrintf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
	hPrintf("<tr><td align='left'>\n");

	hButtonWithOnClick("hgt.collapseGroups", "collapse all", "collapse all track groups", "return setAllTrackGroupVisibility(false)");
	hPrintf("</td>");

	hPrintf("<td colspan='%d' align='CENTER' nowrap>"
	   "Use drop-down controls below and press refresh to alter tracks "
	   "displayed.<BR>"
	   "Tracks with lots of items will automatically be displayed in "
	   "more compact modes.</td>\n", MAX_CONTROL_COLUMNS - 2);

	hPrintf("<td align='right'>");
	hButtonWithOnClick("hgt.expandGroups", "expand all", "expand all track groups", "return setAllTrackGroupVisibility(true)");
	hPrintf("</td></tr>");

	if (!hIsGsidServer())
	    {
	    cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
	    }
	else
	    {
	    /* 4 cols fit GSID's display better */
	    cg = startControlGrid(4, "left");
	    }
	for (group = groupList; group != NULL; group = group->next)
	    {
	    if (group->trackList == NULL)
		continue;

	    struct trackRef *tr;

	    /* check if group section should be displayed */
	    char *otherState;
	    char *indicator;
	    char *indicatorImg;
	    boolean isOpen = !isCollapsedGroup(group);
	    collapseGroupGoodies(isOpen, TRUE, &indicatorImg,
				    &indicator, &otherState);
	    hPrintf("<TR>");
	    cg->rowOpen = TRUE;
	    if (!hIsGsidServer())
                hPrintf("<th align=\"left\" colspan=%d class='blueToggleBar'>",MAX_CONTROL_COLUMNS);
	    else
                hPrintf("<th align=\"left\" colspan=%d class='blueToggleBar'>",MAX_CONTROL_COLUMNS-1);

            hPrintf("<table style='width:100%%;'><tr><td style='text-align:left;'>");
            hPrintf("\n<A NAME=\"%sGroup\"></A>",group->name);
            hPrintf("<IMG class='toggleButton' onclick=\"return toggleTrackGroupVisibility(this, '%s');\" id=\"%s_button\" src=\"%s\" alt=\"%s\" title='%s this group'>&nbsp;&nbsp;",
                    group->name, group->name, indicatorImg, indicator,isOpen?"Collapse":"Expand");
            hPrintf("</td><td style='text-align:center; width:90%%;'>\n<B>%s</B>", group->label);
            hPrintf("</td><td style='text-align:right;'>\n");
            hPrintf("<input type='submit' name='hgt.refresh' value='refresh' title='Update image with your changes'>\n");
            hPrintf("</td></tr></table></th>\n");
	    controlGridEndRow(cg);

	    /* First track group that is not the custom track group (#1)
	     * or a track hub, gets the Base Position track
	     * unless it's collapsed. */
	    if (!showedRuler && !isHubTrack(group->name) &&
		    differentString(group->name, "user") )
		{
		char *url = trackUrl(RULER_TRACK_NAME, chromName);
		showedRuler = TRUE;
		myControlGridStartCell(cg, isOpen, group->name);
		hPrintf("<A HREF=\"%s\">", url);
		hPrintf(" %s<BR> ", RULER_TRACK_LABEL);
		hPrintf("</A>");
		hDropListClassWithStyle("ruler", rulerMenu,
			sizeof(rulerMenu)/sizeof(char *), rulerMenu[rulerMode],
			rulerMode == tvHide ? "hiddenText" : "normalText",
			TV_DROPDOWN_STYLE);
		controlGridEndCell(cg);
		freeMem(url);
		}

	    /* Add supertracks to  track list, sort by priority and
	     * determine if they have visible member tracks */
	    groupTrackListAddSuper(cart, group);

	    /* Display track controls */
	    for (tr = group->trackList; tr != NULL; tr = tr->next)
		{
		struct track *track = tr->track;
		if (tdbIsSuperTrackChild(track->tdb))
		    /* don't display supertrack members */
		    continue;
		myControlGridStartCell(cg, isOpen, group->name);
		if (track->hasUi)
		    {
		    char *url = trackUrl(track->track, chromName);
		    char *longLabel = replaceChars(track->longLabel, "\"", "&quot;");
                    hPrintPennantIcon(track->tdb);

                    // Print an icon before the title when one is defined
                    hPrintf("<A HREF=\"%s\" title=\"%s\">", url, longLabel);

		    freeMem(url);
		    freeMem(longLabel);
		    }
		hPrintf(" %s", track->shortLabel);
		if (tdbIsSuper(track->tdb))
		    hPrintf("...");
		hPrintf("<BR> ");
		if (track->hasUi)
		    hPrintf("</A>");

		if (hTrackOnChrom(track->tdb, chromName))
		    {
		    if (tdbIsSuper(track->tdb))
			superTrackDropDown(cart, track->tdb,
					    superTrackHasVisibleMembers(track->tdb));
		    else
			{
			/* check for option of limiting visibility to one mode */
			hTvDropDownClassVisOnly(track->track, track->visibility,
				    track->canPack, (track->visibility == tvHide) ?
				    "hiddenText" : "normalText",
				    trackDbSetting(track->tdb, "onlyVisibility"));
			}
		    }
		else
		    /* If track is not on this chrom print an informational
		    message for the user. */
		    hPrintf("[No data-%s]", chromName);
		controlGridEndCell(cg);
		}
	    /* now finish out the table */
	    if (group->next != NULL)
		controlGridEndRow(cg);
	    }
	endControlGrid(&cg);
	}

    if (measureTiming)
        printTrackTiming();

    hPrintf("</DIV>\n");
    }
if (showTrackControls)
    hButton("hgt.refresh", "refresh");
hPrintf("</CENTER>\n");

#ifdef SLOW
/* We'll rely on the end of program to do the cleanup.
 * It turns out that the 'free' routine on Linux is
 * quite slow.  For chromosome level views the browser
 * spends about 1/3 of it's time doing the cleanup
 * below if it's enabled.  Since we really don't
 * need to reclaim this memory at this point I'm
 * taking this out.  Please don't delete the code though.
 * I'll like to keep it for testing now and then. -jk. */

/* Clean up. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->visibility != tvHide)
	{
	if (track->freeItems != NULL)
	    track->freeItems(track);
	lmCleanup(&track->lm);
	}
    }
#endif /* SLOW */
hPrintf("</FORM>\n");

/* hidden form for custom tracks CGI */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

/* hidden form for track hub CGI */
hPrintf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
cgiMakeHiddenVar(hgHubConnectCgiDestUrl, "../cgi-bin/hgTracks");
cartSaveSession(cart);
hPrintf("</FORM>\n");

pruneRedundantCartVis(trackList);
if (measureTiming)
    measureTime("Done with trackForm");
}

static void toggleRevCmplDisp()
/* toggle the reverse complement display mode */
{
// forces complement bases to match display
revCmplDisp = !revCmplDisp;
cartSetBooleanDb(cart, database, REV_CMPL_DISP, revCmplDisp);
cartSetBooleanDb(cart, database, COMPLEMENT_BASES_VAR, revCmplDisp);
}

void zoomToSize(int newSize)
/* Zoom so that center stays in same place,
 * but window is new size.  If necessary move
 * center a little bit to keep it from going past
 * edges. */
{
int center = ((long long int)winStart + (long long int)winEnd)/2;
if (center < 0)
    errAbort("zoomToSize: error computing center: %d = (%d + %d)/2\n",
    center, winStart, winEnd);
if (newSize > seqBaseCount)
    newSize = seqBaseCount;
winStart = center - newSize/2;
winEnd = winStart + newSize;
if (winStart <= 0)
    {
    winStart = 0;
    winEnd = newSize;
    }
else if (winEnd > seqBaseCount)
    {
    winEnd = seqBaseCount;
    winStart = winEnd - newSize;
    }
winBaseCount = winEnd - winStart;
}

void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
double newSizeDbl = (winBaseCount*amount + 0.5);
int newSize;
if (newSizeDbl > seqBaseCount)
    newSize = seqBaseCount;
else if (newSizeDbl < 1.0)
    newSize = 1;
else
    newSize = (int)newSizeDbl;
zoomToSize(newSize);
}

void zoomToBaseLevel()
/* Set things so that it's zoomed to base level. */
{
zoomToSize(insideWidth/tl.mWidth);
if (rulerMode == tvHide)
    cartSetString(cart, "ruler", "dense");
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
int offset;
int newStart, newEnd;
if (revCmplDisp)
    amount = -amount;
offset = (int)(amount * winBaseCount);
/* Make sure don't scroll of ends. */
newStart = winStart + offset;
newEnd = winEnd + offset;
if (newStart < 0)
    offset = -winStart;
else if (newEnd > seqBaseCount)
    offset = seqBaseCount - winEnd;

/* Move window. */
winStart += offset;
winEnd += offset;
}

void dinkWindow(boolean start, int dinkAmount)
/* Move one end or other of window a little. */
{
if (revCmplDisp)
    {
    start = !start;
    dinkAmount = -dinkAmount;
    }
if (start)
   {
   winStart += dinkAmount;
   if (winStart < 0) winStart = 0;
   }
else
   {
   winEnd += dinkAmount;
   if (winEnd > seqBaseCount)
       winEnd = seqBaseCount;
   }
}

int dinkSize(char *var)
/* Return size to dink. */
{
char *stringVal = cartOptionalString(cart, var);
double x;
int insideX = trackOffsetX(); /* The global versions of these are not yet set */
int insideWidth = tl.picWidth-gfxBorder-insideX;
double guideBases = (double)guidelineSpacing * (double)(winEnd - winStart)
    / ((double)insideWidth);

if (stringVal == NULL || !isdigit(stringVal[0]))
    {
    stringVal = "1";
    cartSetString(cart, var, stringVal);
    }
x = atof(stringVal);
int ret = round(x*guideBases);

return (ret == 0) ? 1 : ret;
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn, ideoPsTn;
char *pdfFile = NULL, *ideoPdfFile = NULL;
ZeroVar(&ideoPsTn);
trashDirFile(&psTn, "hgt", "hgt", ".eps");

hotLinks();
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.");
doTrackForm(psTn.forCgi, &ideoPsTn);

// postscript
printf("<UL>\n");
printf("<LI><A HREF=\"%s\">Click here</A> "
       "to download the current browser graphic in PostScript.\n", psTn.forCgi);
if (strlen(ideoPsTn.forCgi))
    printf("<LI><A HREF=\"%s\">Click here</A> "
           "to download the current chromosome ideogram in PostScript.\n", ideoPsTn.forCgi);
printf("</UL>\n");

pdfFile = convertEpsToPdf(psTn.forCgi);
if (strlen(ideoPsTn.forCgi))
    ideoPdfFile = convertEpsToPdf(ideoPsTn.forCgi);
if(pdfFile != NULL)
    {
    printf("<BR>PDF can be viewed with Adobe Acrobat Reader.\n");
    printf("<UL>\n");
    printf("<LI><A TARGET=_blank HREF=\"%s\">Click here</A> "
       "to download the current browser graphic in PDF.\n", pdfFile);
    if (ideoPdfFile != NULL)
        printf("<LI><A TARGET=_blank HREF=\"%s\">Click here</A> "
               "to download the current chromosome ideogram in PDF.\n", ideoPdfFile);
    printf("</UL>\n");
    freez(&pdfFile);
    freez(&ideoPdfFile);
    }
else
    printf("<BR><BR>PDF format not available");

    printf("<a href='../cgi-bin/hgTracks'><input type='button' VALUE='Return to Browser'></a>\n");
}

boolean isGenome(char *pos)
/* Return TRUE if pos is genome. */
{
pos = trimSpaces(pos);
return(sameWord(pos, "genome") || sameWord(pos, "hgBatch"));
}

void setRulerMode()
/* Set the rulerMode variable from cart. */
{
char *s = cartUsualString(cart, RULER_TRACK_NAME, "dense");
if (sameWord(s, "full") || sameWord(s, "on"))
    rulerMode = tvFull;
else if (sameWord(s, "dense"))
    rulerMode = tvDense;
else
    rulerMode = tvHide;
}

void setLayoutGlobals()
/* Figure out basic dimensions of display.  */
{
withIdeogram = cartUsualBoolean(cart, "ideogram", TRUE);
withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);
withCenterLabels = cartUsualBoolean(cart, "centerLabels", TRUE);
withGuidelines = cartUsualBoolean(cart, "guidelines", TRUE);
if (!cartUsualBoolean(cart, "hgt.imageV1", FALSE))
    withNextItemArrows = cartUsualBoolean(cart, "nextItemArrows", FALSE);

withNextExonArrows = cartUsualBoolean(cart, "nextExonArrows", TRUE);
if (!hIsGsidServer())
    {
    revCmplDisp = cartUsualBooleanDb(cart, database, REV_CMPL_DISP, FALSE);
    }
withPriorityOverride = cartUsualBoolean(cart, configPriorityOverride, FALSE);
insideX = trackOffsetX();
insideWidth = tl.picWidth-gfxBorder-insideX;

}

void tracksDisplay()
/* Put up main tracks display. This routine handles zooming and
 * scrolling. */
{
char newPos[256];
char *defaultPosition = hDefaultPos(database);
char titleVar[256];
position = getPositionFromCustomTracks();
if (NULL == position)
    {
    position = cloneString(cartUsualString(cart, "position", NULL));
    }

/* default if not set at all, as would happen if it came from a URL with no
 * position. Otherwise tell them to go back to the gateway. Also recognize
 * "default" as specifying the default position. */
if (((position == NULL) || sameString(position, "default"))
    && (defaultPosition != NULL))
    position = cloneString(defaultPosition);
if (sameString(position, ""))
    {
    hUserAbort("Please go back and enter a coordinate rangeor a search term in the \"position\" field.<br>For example: chr22:20100000-20200000.\n");
    }

chromName = NULL;
winStart = 0;
if (isGenome(position) || NULL ==
    (hgp = findGenomePos(database, position, &chromName, &winStart, &winEnd, cart)))
    {
    if (winStart == 0)  /* number of positions found */
        {
        freeMem(position);
        position = cloneString(cartUsualString(cart, "lastPosition", defaultPosition));
        hgp = findGenomePos(database, position, &chromName, &winStart, &winEnd,cart);
        if(hgp != NULL && position != defaultPosition)
            cartSetString(cart, "position", position);
        }
    }

/* After position is found set up hash of matches that should
   be drawn with names highlighted for easy identification. */
createHgFindMatchHash();

/* This means that no single result was found
I.e., multiple results may have been found and are printed out prior to this code*/
if (NULL == chromName)
    {
    return;
    }

seqBaseCount = hChromSize(database, chromName);
winBaseCount = winEnd - winStart;

/* Figure out basic dimensions of display.  This
 * needs to be done early for the sake of the
 * zooming and dinking routines. */
setLayoutGlobals();

baseShowPos = cartUsualBoolean(cart, BASE_SHOWPOS, FALSE);
baseShowAsm = cartUsualBoolean(cart, BASE_SHOWASM, FALSE);
baseShowScaleBar = cartUsualBoolean(cart, BASE_SCALE_BAR, TRUE);
baseShowRuler = cartUsualBoolean(cart, BASE_SHOWRULER, TRUE);
safef(titleVar,sizeof(titleVar),"%s_%s", BASE_TITLE, database);
baseTitle = cartUsualString(cart, titleVar, "");
if (sameString(baseTitle, ""))
    baseTitle = NULL;

if  (cgiVarExists("hgt.toggleRevCmplDisp"))
    toggleRevCmplDisp();
setRulerMode();

/* Do zoom/scroll if they hit it. */
if (cgiVarExists("hgt.left3"))
    relativeScroll(-0.95);
else if (cgiVarExists("hgt.left2"))
    relativeScroll(-0.475);
else if (cgiVarExists("hgt.left1"))
    relativeScroll(-0.1);
else if (cgiVarExists("hgt.right1"))
    relativeScroll(0.1);
else if (cgiVarExists("hgt.right2"))
    relativeScroll(0.475);
else if (cgiVarExists("hgt.right3"))
    relativeScroll(0.95);
else if (cgiVarExists("hgt.inBase"))
    zoomToBaseLevel();
else if (cgiVarExists("hgt.in3"))
    zoomAroundCenter(1.0/10.0);
else if (cgiVarExists("hgt.in2"))
    zoomAroundCenter(1.0/3.0);
else if (cgiVarExists("hgt.in1"))
    zoomAroundCenter(1.0/1.5);
else if (cgiVarExists("hgt.out1"))
    zoomAroundCenter(1.5);
else if (cgiVarExists("hgt.out2"))
    zoomAroundCenter(3.0);
else if (cgiVarExists("hgt.out3"))
    zoomAroundCenter(10.0);
else if (cgiVarExists("hgt.dinkLL"))
    dinkWindow(TRUE, -dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkLR"))
    dinkWindow(TRUE, dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkRL"))
    dinkWindow(FALSE, -dinkSize("dinkR"));
else if (cgiVarExists("hgt.dinkRR"))
    dinkWindow(FALSE, dinkSize("dinkR"));

/* Clip chromosomal position to fit. */
if (winEnd < winStart)
    {
    int temp = winEnd;
    winEnd = winStart;
    winStart = temp;
    }
else if (winStart == winEnd)
    {
    winStart -= 1;
    winEnd += 1;
    }

if (winStart < 0)
    {
    winStart = 0;
    }

if (winEnd > seqBaseCount)
    {
    winEnd = seqBaseCount;
    }

if (winStart > seqBaseCount)
    {
    winStart = seqBaseCount - 1000;
    }

winBaseCount = winEnd - winStart;
if (winBaseCount <= 0)
    hUserAbort("Window out of range on %s", chromName);
/* Save computed position in cart. */
sprintf(newPos, "%s:%d-%d", chromName, winStart+1, winEnd);
cartSetString(cart, "org", organism);
cartSetString(cart, "db", database);
cartSetString(cart, "position", newPos);
if (cgiVarExists("hgt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL, NULL);
}

void chromInfoTotalRow(int count, long long total)
/* Make table row with total number of sequences and size from chromInfo. */
{
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
printf("Total: %d", count);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
printLongWithCommas(stdout, total);
cgiTableFieldEnd();
cgiTableRowEnd();
}

void chromInfoRowsChrom()
/* Make table rows of chromosomal chromInfo name & size, sorted by name. */
{
struct slName *chromList = hAllChromNames(database);
struct slName *chromPtr = NULL;
long long total = 0;

slSort(&chromList, chrSlNameCmp);
for (chromPtr = chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    unsigned size = hChromSize(database, chromPtr->name);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf("<A HREF=\"%s?%s=%u&position=%s\">%s</A>",
       hgTracksName(), cartSessionVarName(), cartSessionId(cart),
       chromPtr->name, chromPtr->name);
    cgiTableFieldEnd();
    cgiTableFieldStartAlignRight();
    printLongWithCommas(stdout, size);
    puts("&nbsp;&nbsp;");
    cgiTableFieldEnd();
    cgiTableRowEnd();
    total += size;
    }
chromInfoTotalRow(slCount(chromList), total);
slFreeList(&chromList);
}

void chromInfoRowsNonChrom(int limit)
/* Make table rows of non-chromosomal chromInfo name & size, sorted by size. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
long long total = 0;
char query[512];
char msg1[512], msg2[512];
int seqCount = 0;
boolean truncating;

seqCount = sqlQuickNum(conn, "select count(*) from chromInfo");
truncating = (limit > 0) && (seqCount > limit);

if (!truncating)
    {
    sr = sqlGetResult(conn, "select chrom,size from chromInfo order by size desc");
    }
else
    {

    safef(query, sizeof(query), "select chrom,size from chromInfo order by size desc limit %d", limit);
    sr = sqlGetResult(conn, query);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned size = sqlUnsigned(row[1]);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf("<A HREF=\"%s?%s=%u&position=%s\">%s</A>",
       hgTracksName(), cartSessionVarName(), cartSessionId(cart),
       row[0], row[0]);
    cgiTableFieldEnd();
    cgiTableFieldStartAlignRight();
    printLongWithCommas(stdout, size);
    puts("&nbsp;&nbsp;");
    cgiTableFieldEnd();
    cgiTableRowEnd();
    total += size;
    }
if (!truncating)
    {
    chromInfoTotalRow(seqCount, total);
    }
else
    {
    safef(msg1, sizeof(msg1), "Limit reached");
    safef(msg2, sizeof(msg2), "%d rows displayed", limit);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    puts(msg1);
    cgiTableFieldEnd();
    cgiSimpleTableFieldStart();
    puts(msg2);
    cgiTableFieldEnd();
    sqlFreeResult(&sr);
    safef(query, sizeof(query), "select count(*),sum(size) from chromInfo");
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	unsigned scafCount = sqlUnsigned(row[0]);
	unsigned totalSize = sqlUnsigned(row[1]);
	cgiTableRowEnd();
	safef(msg1, sizeof(msg1), "contig/scaffold<BR>count:");
	safef(msg2, sizeof(msg2), "total size:");
	cgiSimpleTableRowStart();
	cgiSimpleTableFieldStart();
	puts(msg1);
	cgiTableFieldEnd();
	cgiSimpleTableFieldStart();
	puts(msg2);
	cgiTableFieldEnd();
	cgiTableRowEnd();
	cgiSimpleTableRowStart();
	cgiSimpleTableFieldStart();
	printLongWithCommas(stdout, scafCount);
	cgiTableFieldEnd();
	cgiSimpleTableFieldStart();
	printLongWithCommas(stdout, totalSize);
	cgiTableFieldEnd();
	}
    cgiTableRowEnd();
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void chromInfoPage()
/* Show list of chromosomes (or scaffolds, etc) on which this db is based. */
{
char *position = cartUsualString(cart, "position", hDefaultPos(database));
char *defaultChrom = hDefaultChrom(database);
char *freeze = hFreezeFromDb(database);
struct dyString *title = dyStringNew(512);
if (stringIn(database, freeze))
    dyStringPrintf(title, "%s %s Browser Sequences",
		   hOrganism(database), freeze);
else
    dyStringPrintf(title, "%s %s (%s) Browser Sequences",
		   hOrganism(database), freeze, database);
webStartWrapperDetailedNoArgs(cart, database, "", title->string, FALSE, FALSE, FALSE, FALSE);
printf("<FORM ACTION=\"%s\" NAME=\"posForm\" METHOD=GET>\n", hgTracksName());
cartSaveSession(cart);

puts("Enter a position, or click on a sequence name to view the entire "
     "sequence in the genome browser.<P>");
puts("position ");
hTextVar("position", addCommasToPos(database, position), 30);
cgiMakeButton("Submit", "submit");
puts("<P>");

hTableStart();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
puts("Sequence name &nbsp;");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
puts("Length (bp) including gaps &nbsp;");
cgiTableFieldEnd();
cgiTableRowEnd();

if ((startsWith("chr", defaultChrom) || startsWith("Group", defaultChrom)) &&
    hChromCount(database) < 100)
    chromInfoRowsChrom();
else
    chromInfoRowsNonChrom(1000);

hTableEnd();
cgiDown(0.9);

hgPositionsHelpHtml(organism, database);
puts("</FORM>");
dyStringFree(&title);
webEndSectionTables();
}


void resetVars()
/* Reset vars except for position and database. */
{
static char *except[] = {"db", "position", NULL};
char *cookieName = hUserCookie();
int sessionId = cgiUsualInt(cartSessionVarName(), 0);
char *hguidString = findCookieData(cookieName);
int userId = (hguidString == NULL ? 0 : atoi(hguidString));
struct cart *oldCart = cartNew(userId, sessionId, NULL, NULL);
cartRemoveExcept(oldCart, except);
cartCheckout(&oldCart);
cgiVarExcludeExcept(except);
}

static void addDataHubs(struct cart *cart)
{
hubCheckForNew(database, cart);
cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
}

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char *debugTmp = NULL;
/* Uncomment this to see parameters for debugging. */
/* struct dyString *state = NULL; */
/* Initialize layout and database. */
cart = theCart;

measureTiming = hPrintStatus() && isNotEmpty(cartOptionalString(cart, "measureTiming"));
if (measureTiming)
    measureTime("Get cart of %d for user:%u session:%u", theCart->hash->elCount,
	    theCart->userId, theCart->sessionId);
/* #if 1 this to see parameters for debugging. */
/* Be careful though, it breaks if custom track
 * is more than 4k */
#if  0
state = cgiUrlString();
printf("State: %s\n", state->string);
#endif
getDbAndGenome(cart, &database, &organism, oldVars);

protDbName = hPdbFromGdb(database);
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

if (hIsGisaidServer())
    {
    validateGisaidUser(cart);
    }

setUdcCacheDir();
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);

initTl();

char *configPageCall = cartCgiUsualString(cart, "hgTracksConfigPage", "notSet");

dragZooming = advancedJavascriptFeaturesEnabled(cart);

/* Do main display. */

if (cartUsualBoolean(cart, "hgt.trackImgOnly", FALSE))
    {
    trackImgOnly = TRUE;
    ideogramToo = cartUsualBoolean(cart, "hgt.ideogramToo", FALSE);
    hideControls = TRUE;
    withNextItemArrows = FALSE;
    withNextExonArrows = FALSE;
    hgFindMatches = NULL;     // XXXX necessary ???
    }

jsonForClient = newJsonHash(newHash(8));
jsonHashAddString(jsonForClient, "cgiVersion", CGI_VERSION);
boolean searching = differentString(cartUsualString(cart, TRACK_SEARCH,"0"), "0");

if(!trackImgOnly)
    {
    // Write out includes for css and js files
    hWrites(commonCssStyles());
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("utils.js", NULL);
    jsIncludeFile("ajax.js", NULL);
    if(dragZooming && !searching)
        {
        jsIncludeFile("jquery.imgareaselect.js", NULL);
#ifndef NEW_JQUERY
        webIncludeResourceFile("autocomplete.css");
        jsIncludeFile("jquery.autocomplete.js", NULL);
#endif///ndef NEW_JQUERY
        }
    jsIncludeFile("autocomplete.js", NULL);
    jsIncludeFile("hgTracks.js", NULL);

#ifdef LOWELAB
    jsIncludeFile("lowetooltip.js", NULL);
#endif

    if(advancedJavascriptFeaturesEnabled(cart))
        {
        webIncludeResourceFile("jquery-ui.css");
        if (!searching) // NOT doing search
            {
            webIncludeResourceFile("jquery.contextmenu.css");
            jsIncludeFile("jquery.contextmenu.js", NULL);
            webIncludeResourceFile("ui.dropdownchecklist.css");
            jsIncludeFile("ui.dropdownchecklist.js", NULL);
#ifdef NEW_JQUERY
            jsIncludeFile("ddcl.js", NULL);
#endif///def NEW_JQUERY
            }
        }

    hPrintf("<div id='hgTrackUiDialog' style='display: none'></div>\n");
    // XXXX stole this and '.hidden' from bioInt.css - needs work
    hPrintf("<div id='warning' class='ui-state-error ui-corner-all hidden' style='font-size: 0.75em; display: none;' onclick='$(this).hide();'><p><span class='ui-icon ui-icon-alert' style='float: left; margin-right: 0.3em;'></span><strong></strong><span id='warningText'></span> (click to hide)</p></div>\n");
    }

/* check for new data hub */
if (cartVarExists(cart, hgHubDataText))
    {
    addDataHubs(cart);
    }

if (cartVarExists(cart, "chromInfoPage"))
    {
    cartRemove(cart, "chromInfoPage");
    chromInfoPage();
    }
else if (differentString(cartUsualString(cart, TRACK_SEARCH,"0"),"0"))
    {
    doSearchTracks(groupList);
    }
else if (sameWord(configPageCall, "configure") ||
    sameWord(configPageCall, "configure tracks and display"))
    {
    cartRemove(cart, "hgTracksConfigPage");
    configPage();
    }
else if (cartVarExists(cart, configHideAll))
    {
    cartRemove(cart, configHideAll);
    configPageSetTrackVis(tvHide);
    }
else if (cartVarExists(cart, configShowAll))
    {
    cartRemove(cart, configShowAll);
    configPageSetTrackVis(tvDense);
    }
else if (cartVarExists(cart, configDefaultAll))
    {
    cartRemove(cart, configDefaultAll);
    configPageSetTrackVis(-1);
    }
else if (cartVarExists(cart, configHideAllGroups))
    {
    cartRemove(cart, configHideAllGroups);
    struct grp *grp = NULL, *grps = hLoadGrps(database);
    for (grp = grps; grp != NULL; grp = grp->next)
        collapseGroup(grp->name, TRUE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configShowAllGroups))
    {
    cartRemove(cart, configShowAllGroups);
    struct grp *grp = NULL, *grps = hLoadGrps(database);
    for (grp = grps; grp != NULL; grp = grp->next)
        collapseGroup(grp->name, FALSE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configHideEncodeGroups))
    {
    /* currently not used */
    cartRemove(cart, configHideEncodeGroups);
    struct grp *grp = NULL, *grps = hLoadGrps(database);
    for (grp = grps; grp != NULL; grp = grp->next)
        if (startsWith("encode", grp->name))
            collapseGroup(grp->name, TRUE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configShowEncodeGroups))
    {
    /* currently not used */
    cartRemove(cart, configShowEncodeGroups);
    struct grp *grp = NULL, *grps = hLoadGrps(database);
    for (grp = grps; grp != NULL; grp = grp->next)
        if (startsWith("encode", grp->name))
            collapseGroup(grp->name, FALSE);
    configPageSetTrackVis(-2);
    }
else
    {
    tracksDisplay();
    }

jsonHashAddBoolean(jsonForClient, "measureTiming", measureTiming);
hPrintf("<script type='text/javascript'>\n");
jsonPrint((struct jsonElement *) jsonForClient, "hgTracks", 0);
hPrintf("</script>\n");

}
