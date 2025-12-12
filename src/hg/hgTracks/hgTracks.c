/* hgTracks - the original, and still the largest module for the UCSC Human Genome
 * Browser main cgi script.  Currently contains most of the track framework, though
 * there's quite a bit of other framework type code in simpleTracks.c.  The main
 * routine got moved to create a new entry point to the bulk of the code for the
 * hgRenderTracks web service.  See mainMain.c for the main used by the hgTracks CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <pthread.h>
#include <limits.h>
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
#include "pcrResult.h"
#include "jsHelper.h"
#include "mafTrack.h"
#include "hgConfig.h"
#include "encode.h"
#include "agpFrag.h"
#include "imageV2.h"
#include "suggest.h"
#include "search.h"
#include "errCatch.h"
#include "iupac.h"
#include "botDelay.h"
#include "chromInfo.h"
#include "extTools.h"
#include "basicBed.h"
#include "customFactory.h"
#include "dupTrack.h"
#include "genbank.h"
#include "bigWarn.h"
#include "wigCommon.h"
#include "knetUdc.h"
#include "hex.h"
#include <openssl/sha.h>
#include "customComposite.h"
#include "chromAlias.h"
#include "jsonWrite.h"
#include "cds.h"
#include "cacheTwoBit.h"
#include "cartJson.h"
#include "wikiLink.h"
#include "decorator.h"
#include "decoratorUi.h"
#include "mouseOver.h"
#include "exportedDataHubs.h"

//#include "bed3Sources.h"

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of its cart
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "dirty", "hgt.reset",
            "hgt.in1", "hgt.in2", "hgt.in3", "hgt.inBase",
            "hgt.out1", "hgt.out2", "hgt.out3", "hgt.out4",
            "hgt.left1", "hgt.left2", "hgt.left3",
            "hgt.right1", "hgt.right2", "hgt.right3",
            "hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
            "hgt.tui", "hgt.hideAll", "hgt.visAllFromCt",
	    "hgt.psOutput", "hideControls", "hgt.toggleRevCmplDisp",
	    "hgt.collapseGroups", "hgt.expandGroups", "hgt.suggest",
	    "hgt.jump", "hgt.refresh", "hgt.setWidth",
            "hgt.trackImgOnly", "hgt.ideogramToo", "hgt.trackNameFilter", "hgt.imageV1", "hgt.suggestTrack", "hgt.setWidth",
             TRACK_SEARCH,         TRACK_SEARCH_ADD_ROW,     TRACK_SEARCH_DEL_ROW, TRACK_SEARCH_PAGER,
            "hgt.contentType", "hgt.positionInput", "hgt.internal",
            "sortExp", "sortSim", "hideTracks", "ignoreCookie","dumpTracks",hgsMergeCart,"ctTest",
            NULL };

boolean genomeIsRna = FALSE;    // is genome RNA instead of DNA

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

boolean withNextItemArrows = FALSE; /* Display next feature (gene) navigation buttons */
boolean withPriorityOverride = FALSE;   /* Display priority for each track to allow reordering */

int gfxBorder = hgDefaultGfxBorder; /* Width of graphics border. */
int guidelineSpacing = 12;  /* Pixels between guidelines. */

boolean withIdeogram = TRUE;            /* Display chromosome ideogram? */

int rulerMode = tvHide;         /* on, off, full */
struct hvGfx *hvgSide = NULL;   // Extra pointer to a sideLabel image that can be built if needed

char *rulerMenu[] =
/* dropdown for ruler visibility */
    {
    "hide",
    "dense",
    "full"
    };

char *protDbName;               /* Name of proteome database for this genome. */
#define MAX_CONTROL_COLUMNS 8
#define LOW 1
#define MEDIUM 2
#define BRIGHT 3
#define MAXCHAINS 50000000
boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
int imagePixelHeight = 0;
struct hash *oldVars = NULL;
struct jsonElement *jsonForClient = NULL;

boolean hideControls = FALSE;           /* Hide all controls? */
boolean trackImgOnly = FALSE;           /* caller wants just the track image and track table html */
boolean ideogramToo =  FALSE;           /* caller wants the ideoGram (when requesting just one track) */

/* Structure returned from resolvePosition.
 * We use this to to expand any tracks to full
 * that were found to contain the searched-upon
 * position string */
struct hgPositions *hgp = NULL;

/* Other global variables. */
struct group *groupList = NULL;    /* List of all tracks. */
char *browserName;              /* Test, preview, or public browser */
char *organization;             /* UCSC */

/* mouseOver popUp global data, each track that wants to send json
 * data will need to know the json file name from mouseOverJson
 * and will write the track data to mouseOverJson.  The structure
 * of the data in the json output will depend upon what the track needs
 * to send.
 */
boolean enableMouseOver = FALSE;
struct tempName *mouseOverJsonFile = NULL;
struct jsonWrite *mouseOverJson = NULL;

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
float dif = 0;
if (a->group && b->group)
    dif = a->group->priority - b->group->priority;
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

static int makeInt(double input)
/* make an int out of an input.  Probably should use floor() and such. */
{
if (input == 0)
    return 0;
if (input < 0)
   return -1;
else if (input == 0.0)
   return 0;
else
   return 1;
}

int gCmpPriority(const void *va, const void *vb)
/* Compare groups based on priority, paying attention to hub groups */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
float dif = a->priority - b->priority;
int iDif = makeInt(dif);
boolean aIsHub = startsWith("hub_", a->name);
boolean bIsHub = startsWith("hub_", b->name);

if (aIsHub)
    {
    if (bIsHub)
        {
        char *aNullPos = strchr(&a->name[4], '_');
        char *bNullPos = strchr(&b->name[4], '_');
        if ((aNullPos != NULL) && (bNullPos != NULL) )
            *aNullPos = *bNullPos = 0;

        int strDiff = strcmp(a->name, b->name);
        int ret = 0;
        if (strDiff == 0)   // same hub
            ret = iDif;
        else
            ret = strDiff;

        if ((aNullPos != NULL) && (bNullPos != NULL) )
            *aNullPos = *bNullPos = '_';

        return ret;
        }
    else
        return -1;
    }
else if (bIsHub)
    return 1;

return iDif;
}

void changeTrackVisExclude(struct group *groupList, char *groupTarget, int changeVis, struct hash *excludeHash)
/* Change track visibilities. If groupTarget is
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc.
 * If we are going back to default visibility, then reset the track
 * ordering also. 
 * If excludeHash is not NULL then don't change the visibility of the group names in that hash.
 */
{
struct group *group;
if (changeVis == -2)
    return;
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;
    if (excludeHash && hashLookup(excludeHash, group->name))
        continue;

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
                if (tdbIsComposite(tdb))
                    {
                    safef(pname, sizeof(pname),"%s.*",track->track); //to remove all settings associated with this composite!
                    cartRemoveLike(cart,pname);
                    struct track *subTrack;
                    for (subTrack = track->subtracks;subTrack != NULL; subTrack = subTrack->next)
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
                // if we're called on the path that has excludeHash set
                // we also want to set the supertrack children's visbilities
                if (!tdbIsSuperTrackChild(tdb) || (excludeHash != NULL))
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
                    for (subtrack=track->subtracks;subtrack!=NULL;subtrack=subtrack->next)
                        {
                        if (changeVis == tvHide)               // Since subtrack level vis is an
                            {
                            cartRemove(cart, subtrack->track); // override, simply remove to hide
                            if (excludeHash != NULL) // if we're loading an RTS, but probably we should always do this
                                {
                                char selName[4096];
                                safef(selName, sizeof(selName), "%s_sel", subtrack->track);
                                cartRemove(cart, selName);
                                }
                            }
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

void changeTrackVis(struct group *groupList, char *groupTarget, int changeVis)
/* Change track visibilities. If groupTarget is
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc.
 * If we are going back to default visibility, then reset the track
 * ordering also. */
{
changeTrackVisExclude(groupList, groupTarget, changeVis, NULL);
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

if (theImgBox && curImgTrack)
    {
    struct imgSlice *curSlice = imgTrackSliceGetByType(curImgTrack,stButton);
    if (curSlice)
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
if (theImgBox && curImgTrack)
    {
    char link[512];
    safef(link,sizeof(link),"%s?complement_%s=%d&%s", hgTracksName(), database,
          !cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE),ui->string);
    imgTrackAddMapItem(curImgTrack,link,(char *)(message != NULL?message:NULL),x, y, x+width, y+height, NULL, NULL);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
    hPrintf("HREF=\"%s?complement_%s=%d", hgTracksName(), database,
            !cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE));
    hPrintf("&%s\"", ui->string);
    dyStringFree(&ui);
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
char *hgTrackUi = hTrackUiForTrack(mapName);
if(chromName == NULL)
    safef(buf, sizeof(buf), "%s?%s=%s&db=%s&g=%s", hgTrackUi, cartSessionVarName(), cartSessionId(cart), database, encodedMapName);
else
    safef(buf, sizeof(buf), "%s?%s=%s&db=%s&c=%s&g=%s", hgTrackUi, cartSessionVarName(), cartSessionId(cart), database, cgiEncode(chromName), encodedMapName);
freeMem(encodedMapName);
return(cloneString(buf));
}

static boolean isCompositeInAggregate(struct track *track)
// Check to see if this is a custom composite in aggregate mode.
{
if (!isCustomComposite(track->tdb))
    return FALSE;

char *aggregateVal = cartOrTdbString(cart, track->tdb, "aggregate", NULL);
if ((aggregateVal == NULL) || sameString(aggregateVal, "none"))
    return FALSE;

struct track *subtrack = NULL;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
        break;
    }
if (subtrack == NULL)
    return FALSE;

for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    subtrack->mapsSelf = FALSE;	/* Round about way to tell wig not to do own mapping. */
    }

multiWigContainerMethods(track);
//struct wigCartOptions *wigCart = wigCartOptionsNew(cart, track->tdb, 0, NULL);
//track->wigCartData = (void *) wigCart;
//track->lineHeight = wigCart->defaultHeight;
//wigCart->isMultiWig = TRUE;
//wigCart->autoScale = wiggleScaleAuto;
//wigCart->defaultHeight = track->lineHeight;
//struct wigGraphOutput *wgo = setUpWgo(xOff, yOff, width, tg->height, numTracks, wigCart, hvg);
//tg->wigGraphOutput = wgo;

return TRUE;
}

static int trackPlusLabelHeight(struct track *track, int fontHeight)
/* Return the sum of heights of items in this track (or subtrack as it may be)
 * and the center label(s) above the items (if any). */
{
enum trackVisibility vis = limitVisibility(track);
int y = track->totalHeight(track, vis);
if (isCenterLabelIncluded(track))
    y += fontHeight;
if (tdbIsComposite(track->tdb) && !isCompositeInAggregate(track))
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
if (genomeIsRna)
    toRna(seq->dna);
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
    if(sameString(trackHubSkipHubName(track->track), "cytoBandIdeo"))
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
    if (winStart >= cb->chromStart
    &&  winStart <= cb->chromEnd)
        {
        safef(startBand, buffSize, "%s", cb->name);
        }
    /* End is > rather than >= due to odditiy in the
       cytoband track where the starts and ends of two
       bands overlaps by one. */
    if (winEnd >  cb->chromStart
    &&  winEnd <= cb->chromEnd)
        {
        safef(endBand, buffSize, "%s", cb->name);
        }
    }
}


boolean makeChromIdeoImage(struct track **pTrackList, char *psOutput,
                        struct tempName *ideoTn)
/* Make an ideogram image of the chromosome and our position in it.  If the
 * ideoTn parameter is not NULL, it is filled in if the ideogram is created. */
{
struct track *ideoTrack = NULL;
MgFont *font = tl.font;
char *mapName = "ideoMap";
struct hvGfx *hvg;
boolean doIdeo = TRUE;
int ideoWidth = round(.8 *tl.picWidth);
int ideoHeight = 0;
int textWidth = 0;
struct tempName pngTn;
boolean nukeIdeoFromList = FALSE;
if (ideoTn == NULL)
    ideoTn = &pngTn;   // not returning value

ideoTrack = chromIdeoTrack(*pTrackList);

/* If no ideogram don't draw. */
if(ideoTrack == NULL)
    {
    doIdeo = FALSE;
    }
else if (trackImgOnly && !ideogramToo)
    {
    doIdeo = FALSE;
    }
else
    {
    /* Remove the track from the group and track list. */
    removeTrackFromGroup(ideoTrack);
    slRemoveEl(pTrackList, ideoTrack);
    nukeIdeoFromList = TRUE;

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
// TODO use DIV in future (can update entire div at once in hgTracks.js)
//hPrintf("<DIV id='chromIdeoDiv'>\n");
// FYI from testing, I see that there is code that inserts warning error messages
//  right before ideoMap, so any changes to that name or adding the DIV would require
//  updating the warning-insertion target name.
if(doIdeo)
    {
    char startBand[1024];
    char endBand[1024];
    char title[1024];
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
    maybeNewFonts(hvg);
    hvg->rc = revCmplDisp;
    initColors(hvg);
    ideoTrack->ixColor = hvGfxFindRgb(hvg, &ideoTrack->color);
    ideoTrack->ixAltColor = hvGfxFindRgb(hvg, &ideoTrack->altColor);
    hvGfxSetClip(hvg, 0, gfxBorder, ideoWidth, ideoTrack->height);
    if (virtMode)
	{
    	if (!sameString(virtModeShortDescr,""))
    	    safef(title, sizeof(title), "%s (%s)", displayChromName, virtModeShortDescr);
	else
    	    safef(title, sizeof(title), "%s (%s)", displayChromName, virtModeType);
	}
    else if (isEmpty(startBand))
        safef(title, sizeof(title), "%s", displayChromName);
    else if (sameString(startBand, endBand))
        safef(title, sizeof(title), "%s (%s)", displayChromName, startBand);
    else
        safef(title, sizeof(title), "%s (%s-%s)", displayChromName, startBand, endBand);
    textWidth = mgFontStringWidth(font, title);
    hvGfxTextCentered(hvg, 2, gfxBorder, textWidth, ideoTrack->height, MG_BLACK, font, title);
    // cytoBandDrawAt() clips x based on insideX+insideWidth,
    // but in virtMode we may be in a window that is smaller than the ideo width
    // so temporarily set them to the actual ideo graphic offset and width
    int saveInsideX = insideX;
    int saveInsideWidth = insideWidth;
    insideX = textWidth+4;
    insideWidth = ideoWidth-insideX;
    ideoTrack->drawItems(ideoTrack, winStart, winEnd, hvg, insideX, gfxBorder,
                         insideWidth, font, ideoTrack->ixColor, ideoTrack->limitedVis);
    insideX = saveInsideX;
    insideWidth = saveInsideWidth;
    hvGfxUnclip(hvg);
    /* Save out picture and tell html file about it. */
    hvGfxClose(&hvg);
    /* Finish map. */
    if (!psOutput)
	{
        hPrintf("</MAP>\n");
	jsInline("$('area.cytoBand').on(\"click\", function(){return false;});\n");
	}
    }

// create an empty hidden-map place holder which can change dynamically with ajax callbacks.
if (!doIdeo && !psOutput)
    {
    hPrintf("<MAP Name=%s>\n", mapName);
    hPrintf("</MAP>\n");
    }

hPrintf("<TABLE BORDER=0 CELLPADDING=0>");
if (!psOutput)
    {
    // by default, create an empty hidden ideo place holder for future dynamic ajax update
    char *srcPath = "";
    char *style = "display: none;";
    if (doIdeo)
	{
	srcPath = ideoTn->forHtml;
	style = "display: inline;";
	}
    hPrintf("<TR><TD HEIGHT=5></TD></TR>");
    hPrintf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s id='chrom' style='%s'>",
            srcPath, ideoWidth, ideoHeight, mapName, style);
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD HEIGHT=5></TD></TR></TABLE>\n");
    }
else
    hPrintf("<TR><TD HEIGHT=10></TD></TR></TABLE>\n");
//hPrintf("</DIV>\n");  // TODO use DIV in future
if(ideoTrack != NULL)
    {
    ideoTrack->limitedVisSet = TRUE;
    ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
    }
return nukeIdeoFromList;
}

char *pcrResultMapItemName(struct track *tg, void *item)
/* Stitch accession and display name back together (if necessary). */
{
struct linkedFeatures *lf = item;
return pcrResultItemAccName(lf->name, lf->extra, (struct psl *)lf->original);
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
struct sqlConnection *conn = NULL;
struct sqlResult *sr;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    // pcr result matches to a targetDb are of the format transcript__gene
    if (stringIn("__", psl->tName))
        {
        if (conn == NULL)
            conn = hAllocConn(database);
        int rowOffset = hOffsetPastBin(database, chromName, target->pslTable);
        char **row;
        char query[2048];
        char *itemAcc = pcrResultItemAccession(psl->tName);
        char *itemName = pcrResultItemName(psl->tName);
        /* Query target->pslTable to get target-to-genomic mapping: */
        sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'",
              target->pslTable, itemAcc);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            struct psl *gpsl = pslLoad(row+rowOffset);
            if (sameString(gpsl->tName, chromName) && gpsl->tStart < winEnd && gpsl->tEnd > winStart)
                {
                struct psl *trimmed = pslTrimToQueryRange(gpsl, psl->tStart,
                              psl->tEnd);
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
                      (itemName ? itemName : ""), psl->tStart, psl->tEnd);
                lf->extra = cloneString(extraInfo);
                        // now that there may be more than one primer pair result
                        // in the pcrResults list, we need make sure we are using the
                        // right primer pair later
                        ((struct psl *)(lf->original))->qName = cloneString(psl->qName);
                slAddHead(&itemList, lf);
                }
            }
        }
    else
        {
        if (sameString(psl->tName, chromName) && psl->tStart < winEnd && psl->tEnd > winStart)
            {
            struct linkedFeatures *lf = lfFromPslx(psl, 1, FALSE, FALSE, tg);
            lf->name = cloneString("");
            lf->extra = cloneString("");
            lf->original = psl;
            slAddHead(&itemList, lf);
            }
        }
    }
hFreeConn(&conn);
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
tg->color.a = 255;
tg->altColor.r = (r+255)/2;
tg->altColor.g = (g+255)/2;
tg->altColor.b = (b+255)/2;
tg->altColor.a = 255;
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

        // set mouse over and color to indicate query position and strand
        char over[256];
        safef(over, sizeof over, "%d-%d of %d bp, strand %c", psl->qStart, psl->qEnd, psl->qSize, psl->strand[0]);
        lf->mouseOver = cloneString(over);
        if (sameString(psl->strand, "+"))
            lf->filterColor = MAKECOLOR_32(0,0,0);
        else
            lf->filterColor = MAKECOLOR_32(0,0,150);

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
tg->priority = 103;
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
tdb->canPack = tg->canPack;
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
    iupacFilter(s, s);
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

char *stringInWrapper(char *needle, char *haystack)
/* Wrapper around string in to make it so it's a function rather than a macro. */
{
return stringIn(needle, haystack);
}

void oligoMatchLoad(struct track *tg)
/* Create track of perfect matches to oligo on either strand.
 *
 * Note that if you are extending this code, there is also a parallel copy
 * in src/hg/utils/oligoMatch/ that should be kept up to date!
 *
 */
{
char *dna = dnaInWindow();
char *fOligo = oligoMatchSeq();
toDna(fOligo);
char *(*finder)(char *needle, char *haystack) = (anyIupac(fOligo) ? iupacIn : stringInWrapper);
int oligoSize = strlen(fOligo);
char *rOligo = cloneString(fOligo);
char *rMatch = NULL, *fMatch = NULL;
struct bed *bedList = NULL, *bed;
char strand;
int count = 0, maxCount = 1000000;

if (oligoSize >= 2)
    {
    fMatch = finder(fOligo, dna);
    iupacReverseComplement(rOligo, oligoSize);
    if (sameString(rOligo, fOligo))
        rOligo = NULL;
    else
	rMatch = finder(rOligo, dna);
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
		fMatch = finder(fOligo, fMatch+1);
		strand = '+';
		}
	    }
	else if (fMatch == NULL)
	    {
	    oneMatch = rMatch;
	    rMatch = finder(rOligo, rMatch+1);
	    strand = '-';
	    }
	else if (rMatch < fMatch)
	    {
	    oneMatch = rMatch;
	    rMatch = finder(rOligo, rMatch+1);
	    strand = '-';
	    }
	else
	    {
	    oneMatch = fMatch;
	    fMatch = finder(fOligo, fMatch+1);
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
tg->priority = OLIGO_MATCH_TRACK_PRIORITY;
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
tdb->canPack = tg->canPack;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

Color maybeDarkerLabels(struct track *track, struct hvGfx *hvg, Color color)
/* For tracks having light track display but needing a darker label */
{
if (trackDbSetting(track->tdb, "darkerLabels"))
    {
    struct hsvColor hsv = mgRgbToHsv(mgColorIxToRgb(NULL, color));
    // check if really pale
    if (hsv.s < 500 ||(hsv.h > 40.0 && hsv.h < 150.0))
        return somewhatDarkerColor(hvg, color);
    return slightlyDarkerColor(hvg, color);
    }
return color;
}

boolean isCenterLabelsPackOff(struct track *track)
/* Check for trackDb setting to suppress center labels of composite in pack mode */
{
if (!track || !track->tdb)
    return FALSE;
char *centerLabelsPack = trackDbSetting(track->tdb, "centerLabelsPack");
return (centerLabelsPack && sameWord(centerLabelsPack, "off"));
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
int newy;
char o4[256];
char o5[256];
struct slList *item;
enum trackVisibility vis = track->limitedVis;
Color labelColor = (track->labelColor ?
                        track->labelColor : track->ixColor);
labelColor = maybeDarkerLabels(track, hvg, labelColor);
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
    maxRange = whichSampleBin( maxRangeCutoff, track->minRange, track->maxRange, binCount );
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
if (startsWith("bigMaf", track->tdb->type) || startsWith("wigMaf", track->tdb->type) || startsWith("maf", track->tdb->type))
    vis = tvFull;
/* behave temporarily like pack for these */
if (track->limitedVis == tvFull && isTypeBedLike(track))
    vis = tvPack;

switch (vis)
    {
    case tvHide:
        break;  /* Do nothing; */
    case tvPack:
        if (isCenterLabelsPackOff(track))
            // draw left labels for pack mode track with center labels off
            {
            if (isCenterLabelIncluded(track))
                y += fontHeight;
            hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, track->lineHeight, labelColor, font, 
                                track->shortLabel);
            y += track->height;
            }
        else
            y += tHeight;
        break;
    case tvSquish:
	y += tHeight;
        break;
    case tvFull:
    case tvShow:
        if (isCenterLabelIncluded(track))
            y += fontHeight;

        if( track->subType == lfSubSample && track->items == NULL )
            y += track->height;

        for (item = track->items; item != NULL; item = item->next)
            {
            char *rootName;
            char *name = track->itemName(track, item);
            int itemHeight = track->itemHeight(track, item);
            //warn(" track %s, itemHeight %d\n", track->shortLabel, itemHeight);
            newy = y;

            if (track->itemLabelColor != NULL)
                labelColor = track->itemLabelColor(track, item, hvg);
            else
                labelColor = maybeDarkerLabels(track, hvg, labelColor);

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
        if (track->subType == lfSubSample && track->heightPer > (3 * fontHeight))
            {
            int ymax = y - (track->heightPer / 2) + (fontHeight / 2);
            int ymin = y + (track->heightPer / 2) - (fontHeight / 2);
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
hvGfxUnclip(hvg);
return y;
}

void setGlobalsFromWindow(struct window *window);  // FORWARD DECLARATION

static void doLabelNextItemButtons(struct track *track, struct track *parentTrack,
                                   struct hvGfx *hvg, MgFont *font, int y,
                                   int trackPastTabX, int trackPastTabWidth, int fontHeight,
                                   int insideHeight, Color labelColor)
/* If the track allows label next-item buttons (next gene), draw them. */
/* The button will cause hgTracks to run again with the additional CGI */
/* vars nextItem=trackName or prevItem=trackName, which will then  */
/* signal the browser to find the next thing on the track before it */
/* does anything else. */
{
int portWidth = fullInsideWidth;
int portX = fullInsideX;
// If a portal was established, then set the portal dimensions
long portalStart,chromStart;
double basesPerPixel;
if (theImgBox
&& imgBoxPortalDimensions(theImgBox,&chromStart,NULL,NULL,NULL,&portalStart,NULL,
                          &portWidth,&basesPerPixel))
    {
    portX = (int)((portalStart - chromStart) / basesPerPixel);
    portX += gfxBorder;
    if (withLeftLabels)
        portX += tl.leftLabelWidth + gfxBorder;
    portWidth = portWidth-gfxBorder-insideX;
    }
int arrowWidth = insideHeight;
int arrowButtonWidth = arrowWidth + 2 * NEXT_ITEM_ARROW_BUFFER;
int rightButtonX = portX + portWidth - arrowButtonWidth - 1;
char buttonText[256];
Color fillColor = lightGrayIndex();
labelColor = blackIndex();
hvGfxNextItemButton(hvg, rightButtonX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth,
                    labelColor, fillColor, TRUE);
hvGfxNextItemButton(hvg, portX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth,
                    labelColor, fillColor, FALSE);

safef(buttonText, ArraySize(buttonText), "hgt.prevItem=%s", track->track);

mapBoxReinvoke(hvg, portX, y + 1, arrowButtonWidth, insideHeight, track, FALSE,
               NULL, 0, 0, (revCmplDisp ? "Next item" : "Prev item"), buttonText);

#ifdef IMAGEv2_SHORT_TOGGLE
// LIKELY UNUSED
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

// use the last window globals instead of the first
struct window *w=windows;
while(w->next)
    w = w->next;
setGlobalsFromWindow(w); // use last window

safef(buttonText, ArraySize(buttonText), "hgt.nextItem=%s", track->track);
mapBoxReinvoke(hvg, portX + portWidth - arrowButtonWidth, y + 1, arrowButtonWidth, insideHeight,
               track, FALSE, NULL, 0, 0, (revCmplDisp ? "Prev item" : "Next item"), buttonText);

setGlobalsFromWindow(windows); // restore first window

}

static int doCenterLabels(struct track *track, struct track *parentTrack,
                                struct hvGfx *hvg, MgFont *font, int y, int fullInsideWidth )
/* Draw center labels.  Return y coord */
{
if (track->limitedVis != tvHide)
    {
    MgFont *labelfont;
    if (isCenterLabelIncluded(track))
        {
        int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
        int trackPastTabWidth = tl.picWidth - trackPastTabX;
        int fontHeight = mgFontLineHeight(font);
        int insideHeight = fontHeight-1;
	boolean toggleDone = FALSE;
        char *label = track->longLabel;
        Color labelColor = (track->labelColor ?
                            track->labelColor : track->ixColor);
        if (isCenterLabelConditional(track))
            {
            struct trackDb* tdbComposite = tdbGetComposite(track->tdb);
            if (tdbComposite != NULL)
                {
                label = tdbComposite->longLabel;
                labelColor = hvGfxFindColorIx(hvg, tdbComposite->colorR,
                                                tdbComposite->colorG, tdbComposite->colorB);

                // under this condition the characters that have descenders end up bleeding
                // over into tracks that don't actually draw the label which 
                // results in grek.  In this condition make the center label font smaller
                int fontsize = findBiggest(fontHeight - 4);
                char size[1024];
                safef(size, sizeof size, "%d", fontsize);
                labelfont = mgFontForSizeAndStyle(size, "medium");
                }
            }
        else
            labelfont = font;
        labelColor = maybeDarkerLabels(track, hvg, labelColor);
        hvGfxTextCentered(hvg, insideX, y+1, fullInsideWidth, insideHeight,
                          labelColor, labelfont, label);
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

static void doPreDrawItems(struct track *track, struct hvGfx *hvg, MgFont *font,
                                    int y, long *lastTime)
/* Do Pre-Draw track items. */
{
int fontHeight = mgFontLineHeight(font);
if (isCenterLabelIncluded(track))
    y += fontHeight;
if (track->preDrawItems)
    track->preDrawItems(track, winStart, winEnd, hvg, insideX, y, insideWidth,
                 font, track->ixColor, track->limitedVis);
if (measureTiming && lastTime)
    {
    long thisTime = clock1000();
    track->drawTime = thisTime - *lastTime;
    *lastTime = thisTime;
    }
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
    track->drawTime += thisTime - *lastTime;
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
labelColor = maybeDarkerLabels(track, hvg, labelColor);
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
            if (trackIsCompositeWithSubtracks(track))  // TODO: Change when tracks->subtracks
                {                                      //       are always set for composite
                if (isCenterLabelIncluded(track))
                    y += fontHeight;
                struct track *subtrack;
                for (subtrack = track->subtracks;  subtrack != NULL;subtrack = subtrack->next)
                    {
                    if (isSubtrackVisible(subtrack))
                        {
                        if (subtrack->limitedVis == tvFull)
                            y = doMapItems(subtrack, hvg, fontHeight, y);
                        else
                            {
                            if (isCenterLabelIncluded(subtrack))
                                y += fontHeight;
                            if (theImgBox && subtrack->limitedVis == tvDense)
                                mapBoxToggleVis(hvg, trackPastTabX, y, trackPastTabWidth,
                                                track->lineHeight, subtrack);
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

long computeScaleBar(long numBases, char scaleText[], int scaleTextSize)
/* Do some scalebar calculations and return the number of bases the scalebar will span. */
{
char *baseWord = "bases";
long scaleBases = 0;
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
else if ((numFigs >= 9) && (numFigs < 12))
    baseWord = "Gb";
safef(scaleText, scaleTextSize, "%d %s", scaleBasesTextNum, baseWord);
return scaleBases;
}

enum trackVisibility limitedVisFromComposite(struct track *subtrack)
/* returns the subtrack visibility which may be limited by composite with multi-view dropdowns. */
{
if (tdbIsCompositeChild(subtrack->tdb))
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

static int calcNewWinWidth(struct cart *cart, int winStart, int winEnd, int insideWidth)
/* Calc width of hit boxes that will zoom program around ruler. */
// TODO GALT
// probably should change this to use full and virt vars and make it
// go across the entire full image width. Especially since this is about
// zooming on the virt chrom when user clicks on scalebar or chrom-ruler
// Default is to just zoom current window in by 3x to 1/3 of current width.
{
int winWidth = winEnd - winStart;
int newWinWidth = winWidth;
char message[32];
char *zoomType = cartCgiUsualString(cart, RULER_BASE_ZOOM_VAR, ZOOM_3X);

safef(message, sizeof(message), "%s zoom", zoomType);
if (sameString(zoomType, ZOOM_1PT5X))
    newWinWidth = winWidth/1.5;
else if (sameString(zoomType, ZOOM_3X))
    newWinWidth = winWidth/3;
else if (sameString(zoomType, ZOOM_10X))
    newWinWidth = winWidth/10;
else if (sameString(zoomType, ZOOM_100X))
    newWinWidth = winWidth/100;
else if (sameString(zoomType, ZOOM_BASE) || sameString(zoomType, "base")) // old sessions contain the old zoom type
    newWinWidth = insideWidth/tl.mWidth;
else
    errAbort("invalid zoom type %s", zoomType);

if (newWinWidth < 1)
    newWinWidth = 1;

return newWinWidth;
}

static void drawScaleBar(
    struct hvGfx *hvg,
    MgFont *font,
    int fontHeight,
    int yAfterRuler,
    int y,
    int scaleBarTotalHeight
    )
/* draws the scale bar */
{
int scaleBarPad = 2;
int scaleBarHeight = fontHeight;

// can have one for entire multi-window image
char scaleText[32];
long numBases = 0;
struct window *w;
for (w=windows; w; w=w->next)
    numBases += (w->winEnd - w->winStart);
long scaleBases = computeScaleBar(numBases, scaleText, sizeof(scaleText));
int scalePixels = (int)((double)fullInsideWidth*scaleBases/numBases);
int scaleBarX = fullInsideX + (int)(((double)fullInsideWidth-scalePixels)/2);
int scaleBarEndX = scaleBarX + scalePixels;
int scaleBarY = y + 0.5 * scaleBarTotalHeight;
hvGfxTextRight(hvg, fullInsideX, y + scaleBarPad,
	       (scaleBarX-2)-fullInsideX, scaleBarHeight, MG_BLACK, font, scaleText);
hvGfxLine(hvg, scaleBarX, scaleBarY, scaleBarEndX, scaleBarY, MG_BLACK);
hvGfxLine(hvg, scaleBarX, y+scaleBarPad, scaleBarX,
	  y+scaleBarTotalHeight-scaleBarPad, MG_BLACK);
hvGfxLine(hvg, scaleBarEndX, y+scaleBarPad, scaleBarEndX,
	  y+scaleBarTotalHeight-scaleBarPad, MG_BLACK);
if(cartUsualBoolean(cart, BASE_SHOWASM_SCALEBAR, TRUE))
    {
    int fHeight = vgGetFontPixelHeight(hvg->vg, font);
    hvGfxText(hvg, scaleBarEndX + 10,
	      y + (scaleBarTotalHeight - fHeight)/2 + ((font == mgSmallFont()) ?  1 : 0),
	      MG_BLACK, font, trackHubSkipHubName(database));
    }
}

static int doDrawRuler(struct hvGfx *hvg, int *rulerClickHeight,
                       int rulerHeight, int yAfterRuler, int yAfterBases, MgFont *font,
                       int fontHeight, boolean rulerCds, int scaleBarTotalHeight, struct window *window)
/* draws the ruler. */
{
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
    if (window == windows) // first window, only need to do once
	{
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, fullInsideX, y, fullInsideWidth, yAfterRuler-y+1);
	hvGfxTextCentered(hvg, fullInsideX, y, fullInsideWidth, titleHeight,MG_BLACK, font, baseTitle);
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, insideX, y, insideWidth, yAfterRuler-y+1);
	}
    *rulerClickHeight += titleHeight;
    y += titleHeight;
    }
if (baseShowPos||baseShowAsm)
    {
    if (window == windows) // first window, only need to do once
	{
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, fullInsideX, y, fullInsideWidth, yAfterRuler-y+1);
	char txt[256];
	char numBuf[SMALLBUF];
	char *freezeName = NULL;
	freezeName = hFreezeFromDb(database);
	sprintLongWithCommas(numBuf, virtWinEnd-virtWinStart);
	if (freezeName == NULL)
	    freezeName = cloneString("Unknown");
	if (baseShowPos&&baseShowAsm)
	    safef(txt,sizeof(txt),"%s %s   %s (%s bp)",trackHubSkipHubName(organism),
		    freezeName, addCommasToPos(database, position), numBuf);
	else if (baseShowPos)
	    safef(txt,sizeof(txt),"%s (%s bp)",addCommasToPos(database, position),numBuf);
	else
	    safef(txt,sizeof(txt),"%s %s",trackHubSkipHubName(organism),freezeName);
	freez(&freezeName);
	hvGfxTextCentered(hvg, fullInsideX, y, fullInsideWidth, showPosHeight,MG_BLACK, font, txt);
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, insideX, y, insideWidth, yAfterRuler-y+1);
	}
    *rulerClickHeight += showPosHeight;
    y += showPosHeight;
    }
if (baseShowScaleBar)
    {
    if (window == windows) // first window
	{
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, fullInsideX, y, fullInsideWidth, yAfterRuler-y+1);
	drawScaleBar(hvg, font, fontHeight, yAfterRuler, y, scaleBarTotalHeight);
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, insideX, y, insideWidth, yAfterRuler-y+1);
	}
    y += scaleBarTotalHeight;
    *rulerClickHeight += scaleBarTotalHeight;
    }
if (baseShowRuler && (insideWidth >=36))
    {
    hvGfxDrawRulerBumpText(hvg, insideX, y, rulerHeight, insideWidth, MG_BLACK,
                           font, relNumOff, winBaseCount, 0, 1);
    }

if (zoomedToBaseLevel || rulerCds)
    {
    Color baseColor = MG_BLACK;
    int start, end, chromSize;
    struct dnaSeq *extraSeq;
    /* extraSeq has extra leading & trailing bases
    * for translation in to amino acids */
    boolean complementRulerBases = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
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
              rulerMode == tvFull ? rulerMenu[tvDense] : rulerMenu[tvFull]);
	// GALT TODO should this be fullInsideX and fullInsideWidth?
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

static void logTrackList(struct dyString *dy, struct track *trackList)
/* add visibile tracks to dyString, recursively called */
{
if (trackList == NULL)
    return;

struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    int vis = track->limitedVisSet ? track->limitedVis : track->visibility;
    if (vis)
	{
	logTrackList(dy, track->subtracks);
	if (dy->stringSize)
	    dyStringAppendC(dy, ',');
	dyStringPrintf(dy,"%s:%d", track->track, vis);
	}
    }
}

static void logTrackVisibilities (char *hgsid, struct track *trackList, char *position)
/* log visibile tracks and hgsid */
{
struct dyString *dy = dyStringNew(1024);

// build up dyString
logTrackList(dy, trackList);

// put out ~1024 bye blocks to error_log because otherwise
// Apache will chop up the lines
char *begin = dy->string;
char *ptr = begin;
int count = 0;
for(ptr=begin; ((ptr = strchr(ptr, ',')) != NULL); ptr++)
    {
    if (ptr - begin > 800)
	{
	*ptr = 0;
	fprintf(stderr, "trackLog %d %s %s %s\n", count++, database, hgsid, begin);
	begin = ptr+1;
	}
    }
fprintf(stderr, "trackLog %d %s %s %s\n", count++, database, hgsid, begin);
fprintf(stderr, "trackLog position %s %s %s %s\n",  database, hgsid, position, CGI_VERSION);

dyStringFree(&dy);
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

struct highlightVar
// store highlight information
{
struct highlightVar *next;
char *db;
char *chrom;
long chromStart;
long chromEnd;
char *hexColor;
};

struct highlightVar *parseHighlightInfo()
// Parse highlight info from cart var to a linked list of highlightVar structs
// Accepts four input formats for the highlight variable:
// 0) chrom:start-end (format in very old carts)
// 1) db.chrom:start-end (format in very old carts)
// 2) db.chrom:start-end#hexColor|db.chrom:start-end#hexColor|... (old format)
// 3) db#chrom#start#end#hexColor|db#chrom#start#end#hexColor|... (current format, to allow . in seq names)
//
{
struct highlightVar *hlList = NULL;
char *highlightDef = cartOptionalString(cart, "highlight");
if(highlightDef)
    {
    char *hlArr[4096];
    int hlCount = chopByChar(cloneString(highlightDef), '|', hlArr, ArraySize(hlArr));
    int i;
    for (i=0; i<hlCount; i++)
        {
        char *oneHl = hlArr[i];
        struct highlightVar *h;
        char *chromStart, *chromEnd;
        AllocVar(h);
        if (countSeparatedItems(oneHl, '#')==5)
            // the new format: db#chrom#start#end#color
            {
            h->db     = cloneNextWordByDelimiter(&oneHl,'#');
            h->chrom  = cloneNextWordByDelimiter(&oneHl,'#');
            chromStart = cloneNextWordByDelimiter(&oneHl,'#');
            chromEnd = cloneNextWordByDelimiter(&oneHl,'#');
            h->hexColor = cloneString(oneHl);
            }
        else  // the syntax only used in old saved sessions
            // the old format: db.chr:start-end followed optionally by #color
            // or just chr:start-end
            {
            if (strchr(oneHl, '.')== NULL)
                h->db = database;
             else
                h->db     = cloneNextWordByDelimiter(&oneHl,'.');

            h->chrom  = cloneNextWordByDelimiter(&oneHl,':');
            chromStart = cloneNextWordByDelimiter(&oneHl,'-');
            chromEnd = cloneNextWordByDelimiter(&oneHl,'#');
            if (oneHl && *oneHl != '\0')
                h->hexColor = cloneString(oneHl);
            }

        if (!isEmpty(chromStart) && !isEmpty(chromEnd) &&
                isNumericString(chromStart) && isNumericString(chromEnd) &&
                !isEmpty(h->db) && !isEmpty(h->chrom))
            {
            // long to handle virt chrom coordinates
            h->chromStart = atol(chromStart);
            h->chromEnd   = atol(chromEnd);
            // Typically not zero based, unless we have previously saved the highlight
            // as a result of the multi-region code
            if (h->chromStart > 0)
                {
                h->chromStart--;
                }
            slAddHead(&hlList, h);
            }
        }

    slReverse(&hlList);
    }

return hlList;
}

static void drawHighlights(struct cart *cart, struct hvGfx *hvg, int imagePixelHeight)
// Draw the highlight regions in the image.  Only done if theImgBox is not defined.
// Thus it is done for ps/pdf and view image but for html output the highlights are drawn 
// on the javascript client by hgTracks.js > imageV2 > drawHighlights()
{
struct highlightVar *hlList = parseHighlightInfo();

if(hlList && theImgBox == NULL) // Only highlight region when imgBox is not used. (pdf and show-image)
    {
    struct highlightVar *h;
    for (h=hlList; h; h=h->next)
        {
        if (virtualSingleChrom()) // DISGUISE VMODE
            {
            if ((h->db && sameString(h->db, database))
            &&  (h->chrom && sameString(h->chrom,chromName)))
                {
                char position[1024];
                safef(position, sizeof position, "%s:%ld-%ld", h->chrom, h->chromStart, h->chromEnd);
                char *newPosition = undisguisePosition(position); // UN-DISGUISE VMODE
                if (startsWith(OLD_MULTI_REGION_CHROM, newPosition))
                   newPosition = replaceChars(newPosition, OLD_MULTI_REGION_CHROM, MULTI_REGION_CHROM);
                if (startsWith(MULTI_REGION_CHROM, newPosition))
                    {
                    parseVPosition(newPosition, &h->chrom, &h->chromStart, &h->chromEnd);
                    }
                }
            }

        if ((h->db && sameString(h->db, database))
        &&  (h->chrom && sameString(h->chrom,virtChromName))
        &&  (h->chromEnd != 0)
        &&  (h->chromStart <= virtWinEnd && h->chromEnd >= virtWinStart))
            {
            h->chromStart = max(h->chromStart, virtWinStart);
            h->chromEnd = min(h->chromEnd, virtWinEnd);
            double pixelsPerBase = (double)fullInsideWidth/(virtWinEnd - virtWinStart);
            int startPixels = pixelsPerBase * (h->chromStart - virtWinStart); // floor
            if (startPixels < 0)
                startPixels *= -1;  // reverse complement
            int width = pixelsPerBase * (double)(h->chromEnd - h->chromStart) + 0.5; // round up
            if (width < 2)
                width = 2;

            // Default color to light blue, but if setting has color, use it.
            // 179 for alpha because javascript uses 0.7 opacity, *255 ~= 179
            unsigned int hexColor = MAKECOLOR_32_A(170, 255, 255,179);
            if (h->hexColor)
                {
                long rgb = strtol(h->hexColor,NULL,16); // Big and little Endians
                hexColor = MAKECOLOR_32_A( ((rgb>>16)&0xff), ((rgb>>8)&0xff), (rgb&0xff), 179 );
                }
            hvGfxBox(hvg, fullInsideX + startPixels, 0, width, imagePixelHeight, hexColor);
            }
        }
    }
}

struct hash *makeGlobalTrackHash(struct track *trackList)
/* Create a global track hash and returns a pointer to it. */
{
trackHash = newHash(8);
rAddToTrackHash(trackHash, trackList);
return trackHash;
}

//void domAddMenu(char *afterMenuId, char *newMenuId, char *label)
///* Append a new drop down menu after a given menu, by changing the DOM with jquery  */
//{
//printf("$('#%s').last().after('<li class=\"menuparent\" id=\"%s\"><span>%s</span>"
//    "<ul style=\"display: none; visibility: hidden;\"></ul></li>');\n",
//    afterMenuId, newMenuId, label);
//}
//
//void domAppendToMenu(char *menuId, char *url, char *label)
///* Add an entry to a drop down menu, by changing the DOM with jquery  */
//{
////printf("$('#%s ul').last().after('<li><a target=\"_BLANK\" href=\"%s\">%s</a></li>');\n", menuId, url, label);
//printf("$('#%s ul').append('<li><a target=\"_BLANK\" href=\"%s\">%s</a></li>');\n", menuId, url, label);
//}

//void menuBarAppendExtTools()
///* printf a little javascript that adds entries to a menu */
//{
//    char url[SMALLBUF];
//    safef(url,ArraySize(url),"hgTracks?%s=%s&hgt.redirectTool=crispor",cartSessionVarName(), cartSessionId(cart));
//    printf("<script>\n");
//    printf("jQuery(document).ready( function() {\n");
//    domAddMenu("view", "sendto", "Send to");
//    domAppendToMenu("sendto", url, "Tefor CRISPR sites");
//    printf("});\n");
//    printf("</script>\n");
//}


char *rgbColorToString(struct rgbColor color)
/* make rgbColor into printable string */
{
char buf[256];
safef(buf, sizeof buf, "rgbColor r:%d g:%d b:%d", color.r, color.g, color.b);
return cloneString(buf);
}

char *trackDumpString(struct track *track)
/* Write out info on track to string */
{
struct dyString *dy = dyStringNew(256);

dyStringPrintf(dy, "next: %lu\n", (unsigned long)track->next);
//    struct track *next;   /* Next on list. */
//
dyStringPrintf(dy, "track: %s\n", track->track);
    //char *track;             /* Track symbolic name. Name on image map etc. Same as tdb->track. */
dyStringPrintf(dy, "table: %s\n", track->table);
    //char *table;             /* Table symbolic name. Name of database table. Same as tdb->table.*/
dyStringPrintf(dy, "visibility: %s\n", hStringFromTv(track->visibility));
    //enum trackVisibility visibility; /* How much of this want to see. */
dyStringPrintf(dy, "limitedVis: %s\n", hStringFromTv(track->limitedVis));
    //enum trackVisibility limitedVis; /* How much of this actually see. */
dyStringPrintf(dy, "limitedVisSet: %d\n", track->limitedVisSet);
    //boolean limitedVisSet;	     /* Is limited visibility set? */

dyStringPrintf(dy, "longLabel: %s\n", track->longLabel);
    //char *longLabel;           /* Long label to put in center. */
dyStringPrintf(dy, "shortLabel: %s\n", track->shortLabel);
    //char *shortLabel;          /* Short label to put on side. */

dyStringPrintf(dy, "mapsSelf: %d\n", track->mapsSelf);
    //bool mapsSelf;          /* True if system doesn't need to do map box. */
dyStringPrintf(dy, "drawName: %d\n", track->drawName);
    //bool drawName;          /* True if BED wants name drawn in box. */

dyStringPrintf(dy, "colorShades: %lu\n", (unsigned long)track->colorShades);
    //Color *colorShades;	       /* Color scale (if any) to use. */
dyStringPrintf(dy, "color: %s\n", rgbColorToString(track->color));
    //struct rgbColor color;     /* Main color. */
dyStringPrintf(dy, "ixColor: %u\n", track->ixColor);
    //Color ixColor;             /* Index of main color. */
dyStringPrintf(dy, "altColorShades: %lu\n", (unsigned long)track->altColorShades);
    //Color *altColorShades;     /* optional alternate color scale */
dyStringPrintf(dy, "altColor: %s\n", rgbColorToString(track->altColor));
    //struct rgbColor altColor;  /* Secondary color. */
dyStringPrintf(dy, "ixAltColor: %u\n", track->ixAltColor);
    //Color ixAltColor;

    //void (*loadItems)(struct track *tg);
    /* loadItems loads up items for the chromosome range indicated.   */

dyStringPrintf(dy, "items: %lu\n", (unsigned long) track->items);
    //void *items;               /* Some type of slList of items. */

    //char *(*itemName)(struct track *tg, void *item);
    /* Return name of one of an item to display on left side. */

    //char *(*mapItemName)(struct track *tg, void *item);
    /* Return name to associate on map. */

    //int (*totalHeight)(struct track *tg, enum trackVisibility vis);
	/* Return total height. Called before and after drawItems.
	 * Must set the following variables. */
dyStringPrintf(dy, "height: %d\n", track->height);
    //int height;                /* Total height - must be set by above call. */
dyStringPrintf(dy, "lineHeight: %d\n", track->lineHeight);
    //int lineHeight;            /* Height per item line including border. */
dyStringPrintf(dy, "heightPer: %d\n", track->heightPer);
    //int heightPer;             /* Height per item line minus border. */

    //int (*itemHeight)(struct track *tg, void *item);
    /* Return height of one item. */

    //int (*itemRightPixels)(struct track *tg, void *item);
    /* Return number of pixels needed to right of item for additional labeling. (Optional) */

    //void (*drawItems)(struct track *tg, int seqStart, int seqEnd,
	//struct hvGfx *hvg, int xOff, int yOff, int width,
	//MgFont *font, Color color, enum trackVisibility vis);
    /* Draw item list, one per track. */

    //void (*drawItemAt)(struct track *tg, void *item, struct hvGfx *hvg,
        //int xOff, int yOff, double scale,
	//MgFont *font, Color color, enum trackVisibility vis);
    /* Draw a single option.  This is optional, but if it's here
     * then you can plug in genericDrawItems into the drawItems,
     * which takes care of all sorts of things including packing. */

    //int (*itemStart)(struct track *tg, void *item);
    /* Return start of item in base pairs. */

    //int (*itemEnd)(struct track *tg, void *item);
    /* Return end of item in base pairs. */

    //void (*freeItems)(struct track *tg);
    /* Free item list. */

    //Color (*itemColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color of item (optional). */

    //Color (*itemNameColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color for the item's name (optional). */

    //Color (*itemLabelColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color for the item's label (optional). */

    //void (*mapItem)(struct track *tg, struct hvGfx *hvg, void *item,
                    //char *itemName, char *mapItemName, int start, int end,
                    //int x, int y, int width, int height);
    /* Write out image mapping for a given item */

dyStringPrintf(dy, "hasUi: %d\n", track->hasUi);
    //boolean hasUi;	/* True if has an extended UI page. */
dyStringPrintf(dy, "wigCartData: %lu\n", (unsigned long)track->wigCartData);
    //void *wigCartData;  /* pointer to wigCart */
dyStringPrintf(dy, "extraUiData: %lu\n", (unsigned long)track->extraUiData);
    //void *extraUiData;	/* Pointer for track specific filter etc. data. */

    //void (*trackFilter)(struct track *tg);
    /* Stuff to handle user interface parts. */

dyStringPrintf(dy, "customPt: %lu\n", (unsigned long)track->customPt);
    //void *customPt;  /* Misc pointer variable unique to track. */
dyStringPrintf(dy, "customInt: %d\n", track->customInt);
    //int customInt;   /* Misc int variable unique to track. */
dyStringPrintf(dy, "subType: %d\n", track->subType);
    //int subType;     /* Variable to say what subtype this is for similar tracks to share code. */

    /* Stuff for the various wig incarnations - sample, wig, bigWig */
dyStringPrintf(dy, "minRange: %.2f, maxRange: %.2f\n", track->minRange, track->maxRange);
    //float minRange, maxRange;	  /*min and max range for sample tracks 0.0 to 1000.0*/
dyStringPrintf(dy, "scaleRange: %.2f\n", track->scaleRange);
    //float scaleRange;             /* What to scale samples by to get logical 0-1 */
dyStringPrintf(dy, "graphUpperLimit: %e, grapLowerLimit: %e\n", track->graphUpperLimit, track->graphLowerLimit);
    //double graphUpperLimit, graphLowerLimit;	/* Limits of actual data in window for wigs. */
dyStringPrintf(dy, "preDrawContainer: %lu\n", (unsigned long)track->preDrawContainer);
    //struct preDrawContainer *preDrawContainer;  /* Numbers to graph in wig, one per pixel */
    //struct preDrawContainer *(*loadPreDraw)(struct track *tg, int seqStart, int seqEnd, int width);
dyStringPrintf(dy, "wigGraphOutput: %lu\n", (unsigned long)track->wigGraphOutput);
    //struct wigGraphOutput *wigGraphOutput;  /* Where to draw wig - different for transparency */
    /* Do bits that load the predraw buffer.  Called to set preDrawContainer */

dyStringPrintf(dy, "bbiFile: %lu\n", (unsigned long)track->bbiFile);
    //struct bbiFile *bbiFile;	/* Associated bbiFile for bigWig or bigBed. */

dyStringPrintf(dy, "bedSize: %d\n", track->bedSize);
    //int bedSize;		/* Number of fields if a bed file. */
dyStringPrintf(dy, "isBigBed: %d\n", track->isBigBed);
    //boolean isBigBed;		/* If a bed, is it a bigBed? */

dyStringPrintf(dy, "isRemoteSql: %d\n", track->isRemoteSql);
    //boolean isRemoteSql;	/* Is using a remote mySQL connection. */
dyStringPrintf(dy, "remoteSqlHost: %s\n", track->remoteSqlHost);
    //char *remoteSqlHost;	/* Host machine name for remote DB. */
dyStringPrintf(dy, "remoteSqlUser: %s\n", track->remoteSqlUser);
    //char *remoteSqlUser;	/* User name for remote DB. */
dyStringPrintf(dy, "remoteSqlPassword: %s\n", track->remoteSqlPassword);
    //char *remoteSqlPassword;	/* Password for remote DB. */
dyStringPrintf(dy, "remoteSqlDatabase: %s\n", track->remoteSqlDatabase);
    //char *remoteSqlDatabase;	/* Database in remote DB. */
dyStringPrintf(dy, "remoteSqlTable: %s\n", track->remoteSqlTable);
    //char *remoteSqlTable;	/* Table name in remote DB. */

dyStringPrintf(dy, "otherDb: %s\n", track->otherDb);
    //char *otherDb;		/* Other database for an axt track. */

dyStringPrintf(dy, "private: %d\n", track->private);
    //unsigned short private;	/* True(1) if private, false(0) otherwise. */
dyStringPrintf(dy, "priority: %.2f\n", track->priority);
    //float priority;   /* Tracks are drawn in priority order. */
dyStringPrintf(dy, "defaultPriority: %.2f\n", track->defaultPriority);
    //float defaultPriority;   /* Tracks are drawn in priority order. */
dyStringPrintf(dy, "groupName: %s\n", track->groupName);
    //char *groupName;	/* Name of group if any. */
dyStringPrintf(dy, "group: %lu\n", (unsigned long)track->group);
    //struct group *group;  /* Group this track is associated with. */
dyStringPrintf(dy, "defaultGroupName: %s\n", track->defaultGroupName);
    //char *defaultGroupName;  /* default Group this track is associated with. */
dyStringPrintf(dy, "canPack: %d\n", track->canPack);
    //boolean canPack;	/* Can we pack the display for this track? */
dyStringPrintf(dy, "spaceSaver: %lu\n", (unsigned long)track->ss);
    //struct spaceSaver *ss;  /* Layout when packed. */

dyStringPrintf(dy, "tdb: %lu\n", (unsigned long)track->tdb);
    //struct trackDb *tdb; /*todo:change visibility, etc. to use this */
// ADDED by GALT
if (track->tdb)
    dyStringPrintf(dy, "tdb settings:\n%s\n", track->tdb->settings);

dyStringPrintf(dy, "expScale: %.2f\n", track->expScale);
    //float expScale;	/* What to scale expression tracks by. */
dyStringPrintf(dy, "expTable: %s\n", track->expTable);
    //char *expTable;	/* Expression table in hgFixed. */

    // factorSource
dyStringPrintf(dy, "sourceCount: %ld\n", track->sourceCount);
    //long sourceCount;	/* Number of sources for factorSource tracks. */
dyStringPrintf(dy, "sources: %lu\n", (unsigned long)track->sources);
    //struct expRecord **sources;  /* Array of sources */
dyStringPrintf(dy, "sourceRightPixels: %d\n", track->sourceRightPixels);
    //int sourceRightPixels;	/* Number of pixels to right we'll need. */

    // exon/Next
dyStringPrintf(dy, "exonArrows: %d\n", track->exonArrows);
    //boolean exonArrows;	/* Draw arrows on exons? */
dyStringPrintf(dy, "exonArrowsAlways: %d\n", track->exonArrowsAlways);
    //boolean exonArrowsAlways;	/* Draw arrows on exons even with introns showing? */
dyStringPrintf(dy, "nextExonButtonable: %d\n", track->nextExonButtonable);
    //boolean nextExonButtonable; /* Use the next-exon buttons? */
dyStringPrintf(dy, "nextItemButtonable: %d\n", track->nextItemButtonable);
    //boolean nextItemButtonable; /* Use the next-gene buttons? */

dyStringPrintf(dy, "itemAttrTbl: %lu\n", (unsigned long)track->itemAttrTbl);
    //struct itemAttrTbl *itemAttrTbl;  /* relational attributes for specific items (color) */

    /* fill in left label drawing area */
dyStringPrintf(dy, "labelColor: %u\n", track->labelColor);
    //Color labelColor;   /* Fixed color for the track label (optional) */

    //void (*drawLeftLabels)(struct track *tg, int seqStart, int seqEnd,
    //                       struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    //                       boolean withCenterLabels, MgFont *font,
    //                       Color color, enum trackVisibility vis);

dyStringPrintf(dy, "subtracks: %lu\n", (unsigned long)track->subtracks);
    //struct track *subtracks;   /* list of subsidiary tracks that are
                                //loaded and drawn by this track.  This
                                //is used for "composite" tracks, such
                                //as "mafWiggle */
dyStringPrintf(dy, "parent: %lu\n", (unsigned long)track->parent);
    //struct track *parent;	/* Parent track if any */
dyStringPrintf(dy, "prevTrack: %lu\n", (unsigned long)track->prevTrack);
    //struct track *prevTrack;    // if not NULL, points to track immediately above in the image.
                                //    Needed by ConditionalCenterLabel logic

    //void (*nextPrevExon)(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w, int h, boolean next);
    /* Function will draw the button on a track item and assign a map */
    /* box to it as well, so that a click will move the browser window */
    /* to the next (or previous if next==FALSE) item. This is meant to */
    /* jump to parts of an item already partially in the window but is */
    /* hanging off the edge... e.g. the next exon in a gene. */

    //void (*nextPrevItem)(struct track *tg, boolean next);
    /* If this function is given, it can dictate where the browser loads */
    /* up based on whether a next-item button on the longLabel line of */
    /* the track was pressed (as opposed to the next-item buttons on the */
    /* track items themselves... see nextPrevExon() ). This is meant for */
    /* going to the next/previous item currently unseen in the browser, */
    /* e.g. the next gene. SO FAR THIS IS UNIMPLEMENTED. */

    //char *(*itemDataName)(struct track *tg, char *itemName);
    /* If not NULL, function to translated an itemName into a data name.
     * This is can be used for looking up sequence, CDS, etc. It is used
     * to support item names that have uniqueness identifiers added to deal
     * with multiple alignments.  The resulting value should *not* be freed,
     * and it should be assumed that it might only remain valid for a short
     * period of time.*/

dyStringPrintf(dy, "loadTime: %d\n", track->loadTime);
    //int loadTime;	/* Time it takes to load (for performance tuning) */
dyStringPrintf(dy, "drawTime: %d\n", track->drawTime);
    //int drawTime;	/* Time it takes to draw (for performance tuning) */

dyStringPrintf(dy, "remoteDataSource: %d\n", track->remoteDataSource);
    //enum enumBool remoteDataSource; /* The data for this track is from a remote source */
                   /* Slow retrieval means image can be rendered via an AJAX callback. */
dyStringPrintf(dy, "customTrack: %d\n", track->customTrack);
    //boolean customTrack; /* Need to explicitly declare this is a custom track */
dyStringPrintf(dy, "syncChildVisToSelf: %d\n", track->syncChildVisToSelf);
    //boolean syncChildVisToSelf;	/* If TRUE sync visibility to of children to self. */
dyStringPrintf(dy, "networkErrMsg: %s\n", track->networkErrMsg);
    //char *networkErrMsg;        /* Network layer error message */
dyStringPrintf(dy, "parallelLoading: %d\n", track->parallelLoading);
    //boolean parallelLoading;    /* If loading in parallel, usually network resources. */
dyStringPrintf(dy, "summary: %lu\n", (unsigned long) track->summary);
    //struct bbiSummaryElement *summary;  /* for bigBed */
dyStringPrintf(dy, "summAll: %lu\n", (unsigned long) track->sumAll);
    //struct bbiSummaryElement *sumAll;   /* for bigBed */
dyStringPrintf(dy, "drawLabelInBox: %d\n", track->drawLabelInBox);
    //boolean drawLabelInBox;     /* draw labels into the features instead of next to them */
    //};

return dyStringCannibalize(&dy);
}

char *makeDumpURL(char *text)
/* Make a temp file to hold big dump. Return URL to it. */
{
// trackDump output is quite large
struct tempName trkDmp;
trashDirFile(&trkDmp, "hgt", "hgt", ".txt");
FILE *f = mustOpen(trkDmp.forCgi, "w");
fprintf(f, "%s", text);
carefulClose(&f);
return cloneString(trkDmp.forHtml);
}

char *makeTrackDumpLink(struct track *track)
/* Make a track dump to trash, and return html link to it */
{
char *tds = trackDumpString(track);
char *url = makeDumpURL(tds);
char buf[1024];
safef(buf, sizeof buf, "<A HREF=%s>URL</A>", url);
freeMem(tds);
freeMem(url);
return cloneString(buf);
}

boolean regionsAreInOrder(struct virtRegion *virtRegion1, struct virtRegion *virtRegion2)
/* Return TRUE if the regions are on the same chrom and non-overlapping
 * and are in-order, i.e. region 1 appears before region2. */
{
if (sameString(virtRegion1->chrom, virtRegion2->chrom) && virtRegion1->end <= virtRegion2->start)
    return TRUE;
return FALSE;
}

/* --- Virtual Chromosome Functions --- */

boolean virtualSingleChrom()
/* Return TRUE if using virtual single chromosome mode */
{
return (sameString(virtModeType,"exonMostly") || sameString(virtModeType,"geneMostly"));
}

void parseVPosition(char *position, char **pChrom, long *pStart, long *pEnd)
/* parse Virt position */
{
if (!position)
    {
    errAbort("position NULL");
    }
char *vPos = cloneString(position);
stripChar(vPos, ',');
char *colon = strchr(vPos, ':');
if (!colon)
    errAbort("position has no colon");
char *dash = strchr(vPos, '-');
if (!dash)
    errAbort("position has no dash");
*colon = 0;
*dash = 0;
*pChrom = cloneString(vPos);
*pStart = atol(colon+1) - 1;
*pEnd = atol(dash+1);
}

void parseNVPosition(char *position, char **pChrom, int *pStart, int *pEnd)
/* parse NonVirt position */
{
if (!position)
    {
    errAbort("position NULL");
    }
char *vPos = cloneString(position);
stripChar(vPos, ',');
char *colon = strchr(vPos, ':');
if (!colon)
    errAbort("position has no colon");
char *dash = strchr(vPos, '-');
if (!dash)
    errAbort("position has no dash");
*colon = 0;
*dash = 0;
*pChrom = cloneString(vPos);
*pStart = atoi(colon+1) - 1;
*pEnd = atoi(dash+1);
}

char *disguisePositionVirtSingleChrom(char *position) // DISGUISE VMODE
/* Hide the virt position, convert to real single chrom span.
 * position should be virt chrom span.
 * Can handle anything in the virt single chrom. */
{
/* parse Virt position */
char *chrom = NULL;
long start = 0;
long end = 0;
parseVPosition(position, &chrom, &start, &end);
if (!sameString(chrom, MULTI_REGION_VIRTUAL_CHROM_NAME))
    return position; // return original
struct window *windows = makeWindowListFromVirtChrom(start, end);
char *nonVirtChromName = windows->chromName;
if (!sameString(nonVirtChromName, chromName))
    return position; // return original
int nonVirtWinStart  = windows->winStart;
int nonVirtWinEnd    = windows->winEnd;
struct window *w;
for (w=windows->next; w; w=w->next)
    {
    if (!sameString(w->chromName, nonVirtChromName))
	return position; // return original
    if (w->winEnd < nonVirtWinEnd)
	return position; // return original
    nonVirtWinEnd = w->winEnd;
    }
char nvPos[256];
safef(nvPos, sizeof nvPos, "%s:%d-%d", nonVirtChromName, nonVirtWinStart+1, nonVirtWinEnd);
slFreeList(&windows);
return cloneString(nvPos);
}



char *undisguisePosition(char *position) // UN-DISGUISE VMODE
/* Find the virt position
 * position should be real chrom span.
 * Limitation: can only convert things in the current windows set. */
{
/* parse NonVirt position */
char *chrom = NULL;
int start = 0;
int end = 0;
parseNVPosition(position, &chrom, &start, &end);
if (!sameString(chrom, chromName))
    return position; // return original
long newStart = -1;
long newEnd = -1;
struct window *lastW = NULL;
struct window *w = NULL;
for (w = windows; w; w=w->next)
    {
    //  double check chrom is same thoughout all windows, otherwise warning, return original value
    if (!sameString(w->chromName, chromName))
	{
	return position; // return original
	}
    // check that the regions are ascending and non-overlapping
    if (lastW && w->winStart < lastW->winEnd)
	{
	return position; // return original
	}
    // overlap with position?
    //  if intersection,
    if (w->winEnd > start && end > w->winStart)
	{
	int s = max(start, w->winStart);
	int e = min(end, w->winEnd);
	long cs = s - w->winStart + w->virtStart;
	long ce = e - w->winStart + w->virtStart;
	if (newStart == -1)
	    newStart = cs;
	newEnd = ce;
	}
    lastW = w;
   }
if (newStart == -1) // none of the windows intersected with the position
    return position; // return original
//  return new virt undisguised position as a string
char newPos[1024];
safef (newPos, sizeof newPos, "%s:%ld-%ld", MULTI_REGION_CHROM, (newStart+1), newEnd);
return cloneString(newPos);
}


char *windowsSpanPosition()
/* Return a position string that spans all the windows.
 * Windows should be on same chrom and ascending and non-overlapping.*/
{
char buf[256];
char *chromName = windows->chromName;
int start = windows->winStart;
struct window *w = windows, *last = NULL;
while(w->next) // find last window
    {
    last = w;
    w = w->next;
    if (!sameString(chromName, w->chromName))
	errAbort("windowsSpanPosition: expected all windows to be on the same chrom but found %s and %s", chromName, w->chromName);
    if (w->winStart < last->winEnd)
	errAbort("windowsSpanPosition: expected all windows to be ascending non-overlapping, found %d < %d", w->winStart, last->winEnd);
    }
int end = w->winEnd;
safef(buf, sizeof buf, "%s:%d-%d", chromName, start+1, end);
return cloneString(buf);
}

void padVirtRegions(int windowPadding)
/* Pad virt regions with windowPadding bases
 *
 * NOTE a simple padding would not worry about merging or order. Just expand the beginning and end of each region.

 * NOTE this assumes that the regions are in order, but tolerates hiccups in order.

 * DONE make it handle multiple chromosomes
 *
 * TODO what about just modifying the original list directly?

 * I do not know if this is handling merging correctly.
 * DONE Maybe I should just add the padding directly into the exon-fetch-merge code.
 * I have looked at that earlier, and it should work easily.
 * It might also have the advantage of not having to create a duplicate list?
 *
 * TODO how do I test that the output is correct.
 *  if the input has ordered non-duplicate regions, then the output should be likewise.
 * */
{
int regionCount = 0;
long regionBases=0;
struct virtRegion *virtRegion, *lastVirtRegion = NULL;
int  leftWindowPadding = 0;
int rightWindowPadding = 0;
struct virtRegion *v, *newList = NULL;
char *lastChrom = NULL;
int chromSize = -1;
for(virtRegion=virtRegionList; virtRegion; virtRegion = virtRegion->next)
    {
    AllocVar(v);
    if (!sameOk(virtRegion->chrom, lastChrom))
	{
	chromSize = hChromSize(database, virtRegion->chrom);
	}
    v->chrom = virtRegion->chrom; // TODO is cloning the string needed?
    leftWindowPadding = windowPadding;
    rightWindowPadding = windowPadding;
    if (lastVirtRegion && regionsAreInOrder(lastVirtRegion, virtRegion))
	{
	int distToPrevRegion = virtRegion->start - lastVirtRegion->end;
	if (distToPrevRegion < (2*windowPadding))
	    {
	    leftWindowPadding = distToPrevRegion/2;
	    }
	}
    if (virtRegion->next && regionsAreInOrder(virtRegion, virtRegion->next))
	{
	int distToNextRegion = virtRegion->next->start - virtRegion->end;
	if (distToNextRegion < (2*windowPadding))
	    {
	    rightWindowPadding = (distToNextRegion+1)/2;
	    // +1 to balance for odd number of bases between, arbitrarily adding it to right side
	    }
	}
    v->start = virtRegion->start - leftWindowPadding;
    v->end = virtRegion->end + rightWindowPadding;
    if (v->start < 0)
	v->start = 0;
    if (v->end >= chromSize)
	v->end = chromSize;
    regionBases += (v->end - v->start);
    slAddHead(&newList, v);

    lastVirtRegion = virtRegion;
    lastChrom = virtRegion->chrom;
    ++regionCount;
    }
slReverse(&newList);
virtRegionList = newList; // update new list --
// TODO should the old one be freed? if so, the chrom name should use cloneString
}


void makeVirtChrom()
/* build virtual chrom array from virt region list */
{
virtRegionCount = slCount(virtRegionList);
AllocArray(virtChrom, virtRegionCount);
struct virtRegion *v;
int i = 0;
long totalBases = 0;
for(v=virtRegionList;v;v=v->next,++i)
    {
    virtChrom[i].virtPos = totalBases;
    virtChrom[i].virtRegion = v;
    totalBases += (v->end - v->start);
    }
virtSeqBaseCount = totalBases;
}

void virtChromBinarySearch(long target, long *resultIndex, int *resultOffset)
/* Do a binary search for the target position in the virtual chrom.
 * Return virtChrom Index and Offset if found.
 * Return -1 if target out of range. TODO maybe change to errAbort*/
{
//The binary search will either return match or out-of-range.
long index = -1;
int offset = -1;
long a = 0; // start of the array
long b = virtRegionCount - 1; // end of array where N = count of regions
if (target >=0 && target < virtSeqBaseCount)
    {
    while(1)
	{
	long c = (a + b) / 2;

	if (a > b)
	    {
	    index = b;
	    break;
	    }
	else if (target == virtChrom[c].virtPos)
	    {
	    // (exact match)
	    index = c;
	    break;
	    }
	else if (target > virtChrom[c].virtPos)
	    {
	    a = c + 1;
	    }
	else if (target < virtChrom[c].virtPos)
	    {
	    b = c - 1;
	    }
	else
	    {
	    // probably should not happen
	    errAbort("Unexpected outcome in virtChromBinarySearch.");
	    }
	}
    }
else if (target == virtSeqBaseCount)
    { // tolerate this special case
    index = virtRegionCount - 1;
    }
if (index >= 0)
    offset = target - virtChrom[index].virtPos;
*resultIndex = index;
*resultOffset = offset;
}

void testVirtChromBinarySearch()
/* test virt chrom binary search */
{
warn("about to test virt chrom binary search: virtSeqBaseCount=%ld virtRegionCount=%d", virtSeqBaseCount, virtRegionCount);
long i;
int o;
long target = -1;
virtChromBinarySearch(target, &i, &o);
if (i != -1)
    errAbort("target %ld result %ld %d ", target, i, o);
for(target=0; target < virtSeqBaseCount; ++target)
    {
    virtChromBinarySearch(target, &i, &o);
    if (virtChrom[i].virtPos + o != target)
	errAbort("target = %ld result %ld %d ", target, i, o);
    struct virtRegion *v = virtChrom[i].virtRegion;
    int size = v->end - v->start;
    if (o > size)
	errAbort("target = %ld result %ld %d > size=%d", target, i, o, size);
    }
target = virtSeqBaseCount;
virtChromBinarySearch(target, &i, &o);
if (i != virtRegionCount - 1)
    errAbort("target = virtSeqBaseCount = %ld result %ld %d ", target, i, o);
target = virtSeqBaseCount+1;
virtChromBinarySearch(target, &i, &o);
if (i != -1)
    errAbort("target = virtSeqBaseCount = %ld result %ld %d ", target, i, o);
warn("Got past test virt chrom binary search");
}

struct window *makeWindowListFromVirtChrom(long virtWinStart, long virtWinEnd)
/* make list of windows from virtual position on virtualChrom */
{
// quick check of virt win start and end
if (virtWinEnd == virtWinStart)
    return NULL;
if (virtWinStart < 0 || virtWinStart >= virtSeqBaseCount)
    errAbort("unexpected error. virtWinStart=%ld out of range. virtSeqBaseCount=%ld", virtWinStart, virtSeqBaseCount);
if (virtWinEnd < 0 || virtWinEnd > virtSeqBaseCount)
    errAbort("unexpected error. virtWinEnd=%ld out of range. virtSeqBaseCount=%ld", virtWinEnd, virtSeqBaseCount);
if (virtWinEnd - virtWinStart < 1)
    errAbort("unexpected error. virtual window size < 1 base. virtWinStart=%ld virtWinEnd=%ld virtSeqBaseCount=%ld", virtWinStart, virtWinEnd, virtSeqBaseCount);
long virtIndexStart;
int  virtOffsetStart;
virtChromBinarySearch(virtWinStart, &virtIndexStart, &virtOffsetStart);
if (virtIndexStart == -1)
    errAbort("unexpected failure to find target virtWinStart %ld in virtChrom Array", virtWinStart);

long virtIndexEnd;
int  virtOffsetEnd;
virtChromBinarySearch(virtWinEnd, &virtIndexEnd, &virtOffsetEnd);
if (virtIndexEnd == -1)
    errAbort("unexpected failure to find target virtWinEnd %ld in virtChrom Array", virtWinEnd);

// create new windows list from virt chrom
struct window *windows = NULL;
long i = virtIndexStart;
int winCount = 0;
long basesInWindows = 0; // TODO not actually using this variable
for(i=virtIndexStart; i <= virtIndexEnd; ++i)
    {
    struct window *w;
    AllocVar(w);
    w->organism = organism; // obsolete
    w->database = database; // obsolete
    struct virtRegion *v = virtChrom[i].virtRegion;
    long virtPos = virtChrom[i].virtPos;
    w->chromName = v->chrom;
    if (i == virtIndexStart)
	{
	w->winStart = v->start + virtOffsetStart;
	w->virtStart = virtPos + virtOffsetStart;
	}
    else
	{
	w->winStart = v->start;
	w->virtStart = virtPos;
	}
    if (i == virtIndexEnd)
	{
	if (virtOffsetEnd == 0)
	    continue;
	w->winEnd = v->start + virtOffsetEnd;
	}
    else
	{
	w->winEnd = v->end;
	}
    w->virtEnd = w->virtStart + (w->winEnd - w->winStart);
    w->regionOdd = i % 2;
    basesInWindows += (w->winEnd - w->winStart);
    slAddHead(&windows, w);
    ++winCount;
    }
slReverse(&windows);
if (basesInWindows != (virtWinEnd-virtWinStart)) // was virtWinBaseCount but now I call this routine for highlight pos as well as virt pos.
    errAbort("makeWindowListFromVirtChrom: unexpected error basesInWindows(%ld) != virtWinBaseCount(%ld)", basesInWindows, virtWinEnd-virtWinStart);
return windows;
}

char *nonVirtPositionFromWindows()
/* Must have created the virtual chrom first.
 * Currently just a hack to use windows,
 * use the (first) window(s) from that to get real chrom name start end */
{
// assumes makeWindowListFromVirtChrom() has already been called.
if (!windows)
    errAbort("nonVirtPositionFromWindows() unexpected error, windows list not initialized yet");
char *nonVirtChromName = windows->chromName;
int nonVirtWinStart  = windows->winStart;
int nonVirtWinEnd    = windows->winEnd;
// Extending this through more of the windows,
// if it is on the same chrom and maybe not too far separated.
struct window *w;
for (w=windows->next; w; w=w->next)
    {
    if (sameString(w->chromName, nonVirtChromName) && w->winEnd > nonVirtWinEnd)
	nonVirtWinEnd = w->winEnd;
    }
// TODO Also consider preserving the original bases in windows width (with clipping).
char nvPos[256];
safef(nvPos, sizeof nvPos, "%s:%d-%d", nonVirtChromName, nonVirtWinStart+1, nonVirtWinEnd);
return cloneString(nvPos);
}

char *nonVirtPositionFromHighlightPos()
/* Must have created the virtual chrom first.
 * Currently just a hack to use windows,
 * use the (first) window(s) from that to get real chrom name start end */
{
struct highlightVar *h = parseHighlightInfo();

if (!(h && h->db && sameString(h->db, database) && 
        sameString(h->chrom, MULTI_REGION_VIRTUAL_CHROM_NAME)))
    return NULL;

struct window *windows = makeWindowListFromVirtChrom(h->chromStart, h->chromEnd);

char *nonVirtChromName = windows->chromName;
int nonVirtWinStart  = windows->winStart;
int nonVirtWinEnd    = windows->winEnd;
// Extending this through more of the windows,
// if it is on the same chrom and maybe not too far separated.
struct window *w;
for (w=windows->next; w; w=w->next)
    {
    if (sameString(w->chromName, nonVirtChromName) && w->winEnd > nonVirtWinEnd)
	nonVirtWinEnd = w->winEnd;
    }
// TODO Also consider preserving the original bases in windows width (with clipping).
char nvPos[256];
safef(nvPos, sizeof nvPos, "%s.%s:%d-%d#%s", h->db, nonVirtChromName, nonVirtWinStart+1, nonVirtWinEnd, h->hexColor);
return cloneString(nvPos);
}

void allocPixelsToWindows()
/* Allocate pixels to windows, sets insideWidth and insideX
 *
 * TODO currently uses a strategy that places a window at a pixel location
 * directly, because of round-off and missing small windows this can occasionally
 * lead to gaps not covered by any pixel.  Consider replacing it with something
 * that tries not to leave many gaps -- but how to do it without distortion of some window sizes?
 * */
{
double pixelsPerBase = (double)fullInsideWidth / virtWinBaseCount;
long basesUsed = 0;
int windowsTooSmall = 0;
struct window **pWindows = &windows;
struct window *window;
int winCount = slCount(windows);
for(window=windows;window;window=window->next)
    {
    int basesInWindow = window->winEnd - window->winStart;
    int pixelsInWindow = 0.5 + (double)basesInWindow * pixelsPerBase; // should this round up ? + 0.5?
    window->insideWidth = pixelsInWindow;
    window->insideX = fullInsideX + basesUsed * pixelsPerBase;
    basesUsed += basesInWindow;

    if (pixelsInWindow < 1) // remove windows less than one pixel from the list
	{
	*pWindows = window->next;
	--winCount;
	++windowsTooSmall;
	}
    else
	{
	pWindows = &window->next;
	}
    }
}

struct positionMatch *virtChromSearchForPosition(char *chrom, int start, int end, boolean findNearest)
/* Search the virtual chrom for the query chrom, start, end position
 *
 * TODO GALT: Intially this can be a simple brute-force search of the entire virtChrom array.
 *
 * However, this will need to be upgraded to using a rangeTree or similar structure
 * to rapidly return multiple regions that overlap the query position.
 * */
{
struct positionMatch *list=NULL, *p;
int nearestRegion = -1;
boolean nearestAfter = TRUE;
int nearestDistance = INT_MAX;
int i;
struct virtRegion *v = NULL;
long virtPos = 0;
for(i=0; i < virtRegionCount; ++i)
    {
    v = virtChrom[i].virtRegion;
    virtPos = virtChrom[i].virtPos;
    if (sameString(v->chrom,chrom))
	{ // TODO will we need to support finding closest misses too?
	if (v->end > start && end > v->start) // overlap
	    {
	    int s = max(start, v->start);
	    int e = min(end  , v->end  );
	    AllocVar(p);
	    p->virtStart = virtPos + (s - v->start);
	    p->virtEnd   = virtPos + (e - v->start);
	    slAddHead(&list, p);
	    }
	else if (findNearest && (!list))
	    {
	    int thisDist = v->start - start;
	    boolean thisAfter = TRUE;
	    if (thisDist < 0) // absolute value
		{
		thisDist = -thisDist;
		thisAfter = FALSE;
		}
	    if (thisDist < nearestDistance)
		{
		nearestRegion = i;
		nearestDistance = thisDist;
		nearestAfter = thisAfter;
		}
	    }
	}
    }
if (findNearest && (!list) && nearestRegion != -1)
    {
    i = nearestRegion;
    v = virtChrom[i].virtRegion;
    virtPos = virtChrom[i].virtPos;
    AllocVar(p);
    if (nearestAfter)
	{
    	p->virtStart = virtPos;
	p->virtEnd   = p->virtStart + (end - start);
	}
    else
	{
    	p->virtEnd = virtPos + (v->end - v->start);
	p->virtStart = p->virtEnd - (end - start);
	}
    if (p->virtEnd > virtSeqBaseCount)
	p->virtEnd = virtSeqBaseCount;
    slAddHead(&list, p);
    }
slReverse(&list);
return list;
}

int matchVPosCompare(const void *elem1, const void *elem2)
/* compare elements based on vPos */
{
const struct positionMatch *a = *((struct positionMatch **)elem1);
const struct positionMatch *b = *((struct positionMatch **)elem2);
if (a->virtStart == b->virtStart)
    errAbort("matchVPosCompare error should not happen: 2 elements being sorted have the same virtStart=%ld", a->virtStart);
else if (a->virtStart > b->virtStart)
    return 1;
return -1;
}

void matchSortOnVPos(struct positionMatch **pList)
/* Sort positions by virtPos
 * pList will be sorted by chrom, start, end but we want it ordered by vPos
 */
{
slSort(pList, matchVPosCompare);
}


struct positionMatch *matchMergeContiguousVPos(struct positionMatch *list)
/* Merge contiguous matches spanning multiple touching windows */
{
struct positionMatch *newList = NULL;
struct positionMatch *m;
long lastStart = -1;
long lastEnd = -1;
struct positionMatch *lastM = NULL;
boolean inMerge = FALSE;
long mergeStart = -1;
long mergeEnd = -1;
if (!list)
    return NULL;
for(m=list; 1; m=m->next)
// special loop condition allows it to stop AFTER it goes thru the loop once as m=NULL.
// this flushes out the last value.
    {
    if (inMerge)
	{
	if (m && m->virtStart == lastEnd)
	    {
	    // continue merging, do nothing.
	    // maybe could be freeing a skipped node here.
	    }
	else
	    {
	    inMerge = FALSE;
	    mergeEnd = lastEnd;
	    // create new merged node and add it to new list
	    struct positionMatch *n;
	    AllocVar(n);
	    n->virtStart = mergeStart;
	    n->virtEnd   = mergeEnd;
	    slAddHead(&newList, n);
	    }
	}
    else
	{
	if (m && m->virtStart == lastEnd)
	    {
	    inMerge = TRUE;
	    mergeStart = lastStart;
	    }
	else if (lastM) // just transfer the last node unmodified to the new list.
	    {
	    slAddHead(&newList, lastM);
	    }
	}
    if (!m)
	break;
    lastStart = m->virtStart;
    lastEnd = m->virtEnd;
    lastM = m;
    }
slReverse(&newList);
return newList;
}


// ----------------------------------


void initVirtRegionsFromSOD1Hardwired()
/* create a testing regionlist from SOD1 (uc002ypa.3) hardwired */
{
virtRegionList = NULL;
char *chrom="chr21";
//int exonStarts[] = {33031934,33036102,33038761,33039570,33040783};
//int exonEnds[]   = {33032154,33036199,33038831,33039688,33041243};
// BASE-level zoom in two 20 bp exons and their on-screen junction
int exonStarts[] = {33032134,33036102};
int exonEnds[]   = {33032154,33036122};
int i;
for(i=0; i<ArraySize(exonStarts); ++i)
    {
    struct virtRegion *v;
    AllocVar(v);
    v->chrom = cloneString(chrom);
    v->start = exonStarts[i];
    v->end   = exonEnds[i];
    v->strand[0] = '+';
    v->strand[1] = 0;
    slAddHead(&virtRegionList, v);
    }
slReverse(&virtRegionList);
}

void initVirtRegionsFromEmGeneTable(char *ucscTranscriptId)
/* create a testing regionlist from knownGene transcript with given id */
{
struct sqlConnection *conn = hAllocConn(database);
virtRegionList = NULL;
struct sqlResult *sr;
char **row;
int rowOffset = 0;
if (hIsBinned(database, emGeneTable)) // skip first bin column if any
    ++rowOffset;
char query[256];
sqlSafef(query, sizeof(query), "select * from %s where name='%s'", emGeneTable, ucscTranscriptId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gene = genePredLoad(row+rowOffset);
    int i;
    for(i=0; i< gene->exonCount; ++i)
	{
	struct virtRegion *v;
	AllocVar(v);
	v->chrom = cloneString(gene->chrom);
	v->start = gene->exonStarts[i];
	v->end   = gene->exonEnds[i];
	v->strand[0] = gene->strand[0];
    	v->strand[1] = 0;
	slAddHead(&virtRegionList, v);
	}
    genePredFree(&gene);
    }
sqlFreeResult(&sr);
slReverse(&virtRegionList);
hFreeConn(&conn);
}

boolean initSingleAltHaplotype(char *haplotypeId)
/* create a testing regionlist from haplotype with given id */
{
struct sqlConnection *conn = hAllocConn(database);
if (!virtRegionList) // this should already contain allchroms.
    errAbort("unexpected error in initSingleAltHaplotype: virtRegionList is NULL, should contain all chroms");
struct virtRegion *after = virtRegionList;
virtRegionList = NULL;
struct sqlResult *sr;
char **row;
char *table = endsWith(haplotypeId, "_fix") ? "fixLocations" : "altLocations";
if (! hTableExists(database, table))
    {
    warn("initSingleAltHaplotype: table '%s' not found in database %s, "
         "can't find %s", table, database, haplotypeId);
    return FALSE;
    }

// where is the alt haplo placed?
char query[256];
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd from %s "
         "where name rlike '^%s(:[0-9-]+)?'", table, haplotypeId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (!row)
    {
    warn("no haplotype found for [%s] in %s", haplotypeId, table);
    return FALSE;
    }
char *haploChrom = cloneString(row[0]);
int haploStart = sqlUnsigned(row[1]);
int haploEnd   = sqlUnsigned(row[2]);
sqlFreeResult(&sr);
// what is the size of the alt haplo?
int haploSize = hChromSize(database, haplotypeId); // hopefully this will work

// insert into list replacing original haploChrom record
struct virtRegion *before = NULL;
boolean found = FALSE;
long offset = 0;
struct virtRegion *v;
while ((v=slPopHead(&after)))
    {
    if (sameString(haploChrom, v->chrom))
	{
	found = TRUE;
	break;
	}
    offset += (v->end - v->start);
    slAddHead(&before, v);
    }
if (!found)
    {
    warn("initSingleAltHaplotype: chrom %s on which alt %s is placed is not found in all chroms list!", haploChrom, haplotypeId);
    return FALSE;
    }
slReverse(&before);

// for now, make virtchrom with just one chrom plus its haplo in the middle

defaultVirtWinStart = 0;
defaultVirtWinEnd = 0;;

AllocVar(v);
v->chrom = haploChrom;
v->start = 0;
v->end   = haploStart;
defaultVirtWinStart = v->end - haploSize;
if (defaultVirtWinStart < 0)
    defaultVirtWinStart = 0;
defaultVirtWinEnd = v->end;
slAddHead(&virtRegionList, v);


AllocVar(v);
v->chrom = haplotypeId;
v->start = 0;
v->end   = haploSize;
defaultVirtWinEnd += haploSize;
slAddHead(&virtRegionList, v);

AllocVar(v);
v->chrom = haploChrom;
v->start = haploEnd;
int chromSize = hChromSize(database, haploChrom); // size of the regular chrom
v->end   = chromSize;
defaultVirtWinEnd += haploSize;
if (defaultVirtWinEnd > chromSize)
    defaultVirtWinEnd = chromSize;
slAddHead(&virtRegionList, v);

slReverse(&virtRegionList);
hFreeConn(&conn);

defaultVirtWinStart += offset;
defaultVirtWinEnd += offset;

// concatenate the 3 lists.
virtRegionList = slCat(before,slCat(virtRegionList,after));

return TRUE;
}


void initVirtRegionsFromKnownCanonicalGenes(char *table)
// OBSOLETED by initVirtRegionsFromEMGeneTableExons()
/* Create a regionlist from knownCanonical genes (not exons) */
// I was not expecting it, but 20% of the KC genes overlap with others.
// Some are cDNAs, some are non-coding RNAs, some just look like junk.
// But I have to add special logic to accomodate them.
// Currently I just merge until there is no more overlap,
// and then I start a new region. So 30K regions should reduce to about 24K regions or less.
{
struct sqlConnection *conn = hAllocConn(database);
virtRegionList = NULL;
struct sqlResult *sr;
char **row;
char query[256];
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd from %s where chrom not like '%%_hap_%%' and chrom not like '%%_random'", table);
sr = sqlGetResult(conn, query);
char chrom[256] = "";
int start = -1;
int end = -1;
char lastChrom[256] = "";
int lastStart = -1;
int lastEnd = -1;
boolean firstTime = TRUE;
boolean isEOF = FALSE;
while (1)
    {
    boolean printIt = FALSE;
    row = sqlNextRow(sr);
    if (row)
	{
	safecpy(chrom, sizeof chrom, row[0]);
	start = sqlUnsigned(row[1]);
    	end   = sqlUnsigned(row[2]);
	if (sameString(chrom, lastChrom))
	    {
	    if (start <= lastEnd)
		{
		// overlap detected in knownCanonical
		// extend current region
		if (end > lastEnd)
		    lastEnd = end;
		}
	    else
		{
		printIt = TRUE;
		}
	    }
	else
	    {
	    printIt = TRUE;
	    }
	}
    else
	{
	printIt = TRUE;
	isEOF = TRUE;
	}


    if (printIt)
	{
	if (firstTime)
	    {
	    firstTime = FALSE;
	    }
	else
	    {
	    struct virtRegion *v;
	    AllocVar(v);
	    v->chrom = cloneString(lastChrom);
	    v->start = lastStart;
	    v->end   = lastEnd;
	    v->strand[0] = '.';  // TODO we should probably just remove the strand field
	    v->strand[1] = 0;
	    slAddHead(&virtRegionList, v);
	    }
	}

    if (isEOF)
	break;

    if (printIt)
	{
	safecpy(lastChrom, sizeof lastChrom, chrom);
	lastStart = start;
	lastEnd = end;
	}


    }
sqlFreeResult(&sr);
slReverse(&virtRegionList);
hFreeConn(&conn);
}

struct kce
// keep list of overlapping genes and their exons
{
struct kce *next;
struct genePred *gene;
int exonNumber;
};

int findBestKce(struct kce *list, struct kce **pBestKce, struct kce **pPrevKce)
// find best kce by having minimum exon start
// TODO could replace this with a heap or a doubly-linked list
{
int best = -1;
struct kce *e, *prev = NULL;
for(e=list; e; prev=e, e=e->next)
    {
    int start = e->gene->exonStarts[e->exonNumber];
    if ((start < best) || (best == -1))
	{
	best = start;
	*pBestKce = e;
	*pPrevKce = prev;
	}
    }
return best;
}

static void padExons(struct genePred *gene, int chromSize, int padding)
/* pad all of the exons */
{
int i;
for(i=0; i < gene->exonCount; ++i)
    {
    gene->exonStarts[i] -= padding;
    if (gene->exonStarts[i] < 0)
	gene->exonStarts[i] = 0;
    gene->exonEnds[i] += padding;
    if (gene->exonEnds[i] > chromSize)
	gene->exonEnds[i] = chromSize;
    }
}

static void convertGenePredGeneToExon(struct genePred *gene)
/* convert gene into a gene with just one exon that spans the entire gene */
{
if (gene->exonCount < 1)
    errAbort("unexpected input in convertGenePredGeneToExon(), gene->exonCount=%d < 1", gene->exonCount);
gene->exonEnds[0] = gene->exonEnds[gene->exonCount - 1];
gene->exonCount = 1;
}

void initVirtRegionsFromEMGeneTableExons(boolean showNoncoding, char *knownCanonical, char *knownToTag, boolean geneMostly)
/* Create a regionlist from knownGene exons. */
// Merge exon regions that overlap.

// DONE Jim indicated that he would prefer it to include all transcripts, not just knownCanonical.

// DONE Jim also suggested that we might want to handle padding right here in this step.
// After thinking about it, I do not think it would be very hard because we are merging already.
// Basically, just take the record from the db table row, add padding to start and end,
// and clip for chromosome size.

// TODO If we keep it at full genome level (instead of single chrom), then there is an apparent
// sorting issue because although they are sorted on disk, they are usually sorted by chrom alphabetically
// so that chr11 (not chr2) comes after chr1.  Instead of trying to specify the sort order in the query,
// which is slow, or trying to read one chrom at a time in the sorted order which is also slow, we can instead
// just fetch them in their native order, and then create a duplicate array and copy the contents
// to it in memory, one chunk per chrom, which would be very fast, but temporarily require duplicate vchrom array mem.
// Not sure what to do about assemblies with many scaffolds.
//
// Adding support for extra options from Gencode hg38 so we can filter for
// comprehensive, splice-variants, non-coding subsets.

{
struct sqlConnection *conn = hAllocConn(database);
virtRegionList = NULL;
struct sqlResult *sr;
char **row;
int rowOffset = 0;
struct dyString *query = NULL;
int padding = emPadding;
if (sameString(virtModeType, "geneMostly"))
    padding = gmPadding;
// knownCanonical Hash
struct hash *kcHash = NULL;
if (knownCanonical) // filter out alt splicing variants
    {
    // load up hash of canonical transcriptIds
    query = sqlDyStringCreate("select transcript from %s"
	//" where chrom not like '%%_hap_%%' and chrom not like '%%_random'"
	, knownCanonical);
    if (virtualSingleChrom())
	sqlDyStringPrintf(query, " where chrom='%s'", chromName);
    kcHash = newHash(10);
    sr = sqlGetResult(conn, dyStringContents(query));
    while ((row = sqlNextRow(sr)) != NULL)
	{
	hashAdd(kcHash, row[0], NULL);
	}
    sqlFreeResult(&sr);
    dyStringFree(&query);
    }
// knownToTag basic hash
struct hash *ktHash = NULL;
if (knownToTag) // filter out all but Basic
    {
    // load up hash of canonical transcriptIds
    query = sqlDyStringCreate("select name from %s where value='basic'", knownToTag);
    ktHash = newHash(10);
    sr = sqlGetResult(conn, dyStringContents(query));
    while ((row = sqlNextRow(sr)) != NULL)
	{
	hashAdd(ktHash, row[0], NULL);
	}
    sqlFreeResult(&sr);
    dyStringFree(&query);
    }
setEMGeneTrack();
if (!emGeneTable)
    errAbort("Unexpected error, emGeneTable=NULL in initVirtRegionsFromEMGeneTableExons");
if (hIsBinned(database, emGeneTable)) // skip first bin column if any
    ++rowOffset;
query = sqlDyStringCreate("select * from %s", emGeneTable);
if (virtualSingleChrom())
    sqlDyStringPrintf(query, " where chrom='%s'", chromName);
// TODO GALT may have to change this to in-memory sorting?
// refGene is out of order because of genbank continuous loading
// also, using where chrom= causes it to use indexes which disturb order returned.
sqlDyStringPrintf(query, " order by chrom, txStart");
sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);

char chrom[256] = "";
int start = -1;
int end = -1;
char lastChrom[256] = "";
int lastStart = -1;
int lastEnd = -1;
int chromSize = -1;
char lastChromSizeChrom[256] = "";
boolean firstTime = TRUE;
boolean isEOF = FALSE;
struct kce *kceList = NULL, *bestKce = NULL, *prevKce = NULL;
struct genePred *gene = NULL;
while (1)
    {

    while(1) // get input if possible
	{
	
	boolean readIt = FALSE;
	if (!gene)
	    readIt = TRUE;
	if (isEOF)
	    readIt = FALSE;
	if (readIt)
	    {
	    row = sqlNextRow(sr);
	    if (row)
		{
		gene = genePredLoad(row+rowOffset);
		if (geneMostly)
		    convertGenePredGeneToExon(gene);
		if (!sameString(lastChromSizeChrom, gene->chrom))
		    {
		    chromSize = hChromSize(database, gene->chrom);
		    safecpy(lastChromSizeChrom, sizeof lastChromSizeChrom, gene->chrom);
		    }
		if (padding > 0)
		    padExons(gene, chromSize, padding); // handle padding
		}
	    else
		{
		isEOF = TRUE;
		}
	    }
	if (gene && !showNoncoding && (gene->cdsStart == gene->cdsEnd))
	    {
	    //skip non-coding gene
	    genePredFree(&gene);
	    }
	if (gene && knownCanonical && !hashLookup(kcHash, gene->name))
	    {
	    //skip gene not in knownCanonical hash
	    genePredFree(&gene);
	    }
	if (gene && knownToTag && !hashLookup(ktHash, gene->name))
	    {
	    // skip gene not in knownToTag Basic hash
	    genePredFree(&gene);
	    }
	boolean transferIt = FALSE;
	if (gene && !kceList)
	    {
	    transferIt = TRUE;
	    }
	else if (gene && kceList)
	    {
	    // TODO need to check the chrom equality first
	    int best = findBestKce(kceList, &bestKce, &prevKce);
	    if (sameString(gene->chrom, chrom))
		{
		if (gene->exonStarts[0] < best)
		    transferIt = TRUE;
		}
	    }
	if (transferIt)
	    {
	    // add gene to kce list
	    struct kce *kce;
	    AllocVar(kce);
	    kce->gene = gene;
	    kce->exonNumber = 0;
	    slAddHead(&kceList, kce);
	    safecpy(chrom, sizeof chrom, gene->chrom);
	    gene = NULL; // do not free since it is still in use
	    }
	if (gene && kceList && !transferIt)
	    break;
	if (isEOF && !gene)
	    {
	    if (kceList) // flush out the last of the items in kcelist
		findBestKce(kceList, &bestKce, &prevKce);
	    break;
	    }
	}


    boolean printIt = FALSE;

    if (kceList)
	{

	safecpy(chrom, sizeof chrom, bestKce->gene->chrom);
	start = bestKce->gene->exonStarts[bestKce->exonNumber];
	end   = bestKce->gene->exonEnds[bestKce->exonNumber];

	if (sameString(chrom, lastChrom))
	    {
	    if (start <= lastEnd)
		{
		// overlap detected, extend current region
		if (end > lastEnd)
		    {
		    lastEnd = end;
		    }
		}
	    else
		{
		printIt = TRUE;
		}
	    }
	else
	    {
	    printIt = TRUE;
	    }
	}
    else
	{
	printIt = TRUE;
	isEOF = TRUE;
	}

    if (printIt)
	{
	if (firstTime)
	    {
	    firstTime = FALSE;
	    }
	else
	    {
	    struct virtRegion *v;
	    AllocVar(v);
	    v->chrom = cloneString(lastChrom);
	    v->start = lastStart;
	    v->end   = lastEnd;
	    v->strand[0] = '.';  // TODO we should probably just remove the strand field
	    v->strand[1] = 0;
	    slAddHead(&virtRegionList, v);

	    }
	}

    if (isEOF && !kceList && !gene)
	break;

    if (printIt)
	{
	safecpy(lastChrom, sizeof lastChrom, chrom);
	lastStart = start;
	lastEnd = end;
	}

    ++bestKce->exonNumber;
    if (bestKce->exonNumber >= bestKce->gene->exonCount)
	{  // remove from kceList
	genePredFree(&bestKce->gene);
	if (prevKce)
	    prevKce->next = bestKce->next;
	else
	    kceList = bestKce->next;
	freeMem(bestKce);
	}

    }
sqlFreeResult(&sr);
slReverse(&virtRegionList);
hashFree(&kcHash);
hashFree(&ktHash);
hFreeConn(&conn);
}


void testRegionList()
/* check if it is ascending non-overlapping regions.
(this is not always a requirement in the most general case, i.e. user-regions)
*/
{
char lastChrom[256];
int lastEnd = -1;
struct virtRegion *v;
for (v=virtRegionList; v; v=v->next)
    {
    if (sameString(v->chrom,lastChrom))
	{
	if (v->end < v->start)
	    errAbort("check of region list reveals invalid region %s:%d-%d", v->chrom, v->start, v->end);
	if (lastEnd > v->start)
	    errAbort("check of region list reveals overlapping regions region %s:%d-%d lastEnd=%d",  v->chrom, v->start, v->end, lastEnd);
	}
    else
	{
	safecpy(lastChrom, sizeof lastChrom, v->chrom);
	}
    lastEnd = v->end;
    }
}


// multi-window variables global to hgTracks


void setGlobalsFromWindow(struct window *window)
/* set global window values */
{
currentWindow = window;
organism  = window->organism;
database  = window->database;
chromName = window->chromName;
displayChromName = chromAliasGetDisplayChrom(database, cart, window->chromName);
winStart  = window->winStart;
winEnd    = window->winEnd;
insideX   = window->insideX;
insideWidth = window->insideWidth;
winBaseCount = winEnd - winStart;
}


void initExonStep()
/* create exon-like pattern with exonSize and stepSize */
{
int winCount = cartUsualInt(cart, "demo2NumWindows", demo2NumWindows);
int i;
int exonSize = cartUsualInt(cart, "demo2WindowSize", demo2WindowSize);  //200; //9974; //200;
int intronSize = cartUsualInt(cart, "demo2StepSize", demo2StepSize); //200; //15000; // really using it like stepSize as that allows overlapping windows.
struct virtRegion *v;
for(i=0;i<winCount;++i)
    {
    AllocVar(v);
    //chr21:33,031,597-33,041,570
    v->chrom = "chr21";
    v->start = 33031597 - 1 + i*(intronSize);
    v->end = v->start + exonSize; //33041570;
    slAddHead(&virtRegionList, v);
    }
slReverse(&virtRegionList);
//if (winCount >= 2)
//    withNextExonArrows = FALSE;	/* Display next exon navigation buttons near center labels? */
//warn("winCount=%d, exonSize=%d, intronSize=%d", winCount, exonSize, intronSize);
}

void initAllChroms()
/* initialize virt region list for main chromosomes */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int winCount = 0;
char query[1024];
sqlSafef(query, sizeof query,
"select chrom, size from chromInfo"
" where chrom     like 'chr%%'"
" and   chrom not like '%%_random'"
" and   chrom not like 'chrUn%%'"
);
// allow alternate haplotypes for now
//" and   chrom not like '%_hap%'"
//" and   chrom not like '%_alt'"
//warn("%s",query);
struct virtRegion *v;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned chromSize = sqlUnsigned(row[1]);
    AllocVar(v);
    v->chrom = cloneString(row[0]);
    v->start = 1 - 1;
    v->end   = chromSize;
    slAddHead(&virtRegionList, v);
    ++winCount;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&virtRegionList);
}



void initWindowsAltLoci()
/* initialize window list showing alt (alternate haplotype)*/
{

struct virtRegion *v;
//chr1:153520530-153700530
AllocVar(v);
v->chrom = "chr1";
v->start = 153520530 - 1;
v->end   = 153700530;
//chr1:153,520,529-154,045,739  as a single window
slAddHead(&virtRegionList, v);

//chr1_GL383518v1_alt:1-182439
AllocVar(v);
v->chrom = "chr1_GL383518v1_alt";
v->start = 1 - 1;
v->end   = 182439;
slAddHead(&virtRegionList, v);

//chr1:153865739-154045739
AllocVar(v);
v->chrom = "chr1";
v->start = 153865739 - 1;
v->end   = 154045739;
slAddHead(&virtRegionList, v);

slReverse(&virtRegionList);

}

void checkmultiRegionsBedInput()
/* Check if multiRegionsBedInput needs processing.
 * If BED submitted, see if it has changed, and if so, save it to trash
 * and update cart and global vars. Uses sha1 hash for faster change check. */
{
enum custRgnType { empty, url, trashFile };
enum custRgnType oldType = empty;
enum custRgnType newType = empty;

// OLD input

char *newMultiRegionsBedUrl = NULL;
multiRegionsBedUrl = cartUsualString(cart, "multiRegionsBedUrl", multiRegionsBedUrl);
char multiRegionsBedUrlSha1Name[1024];
safef(multiRegionsBedUrlSha1Name, sizeof multiRegionsBedUrlSha1Name, "%s.sha1", multiRegionsBedUrl);
if (!multiRegionsBedUrl)
    {
    multiRegionsBedUrl = "";
    cartSetString(cart, "multiRegionsBedUrl", multiRegionsBedUrl);
    }
if (sameString(multiRegionsBedUrl,""))
    oldType = empty;
else if (strstr(multiRegionsBedUrl,"://"))
    oldType = url;
else
    oldType = trashFile;

// NEW input

char *multiRegionsBedInput = cartOptionalString(cart, "multiRegionsBedInput");
if (!multiRegionsBedInput)
    return;

// create cleaned up dyString from input.
// remove blank lines, trim leading and trailing spaces, change CRLF from TEXTAREA input to LF.
struct dyString *dyInput = dyStringNew(1024);
char *input = cloneString(multiRegionsBedInput);  // make a copy, linefile modifies
struct lineFile *lf = lineFileOnString("multiRegionsBedInput", TRUE, input);
char *line;
int lineSize;
while (lineFileNext(lf, &line, &lineSize))
    {
    line = trimSpaces(line);
    if (sameString(line, "")) // skip blank lines
	continue;
    dyStringAppend(dyInput,line);
    dyStringAppend(dyInput,"\n");
    }
lineFileClose(&lf);

// test multiRegionsBedInput. empty? url? trashFile?
input = cloneString(dyInput->string);  // make a copy, linefile modifies
lf = lineFileOnString("multiRegionsBedInput", TRUE, input);
int lineCount = 0;
while (lineFileNext(lf, &line, &lineSize))
    {
    ++lineCount;
    if (lineCount==1 &&
	(startsWithNoCase("http://" ,line)
      || startsWithNoCase("https://",line)
      || startsWithNoCase("ftp://"  ,line)))
	{
	// new value is a URL. set vars and cart.
	newMultiRegionsBedUrl = cloneString(line);
	newType = url;
	}
    break;
    }
lineFileClose(&lf);
if (newType != url)
    {
    if (lineCount == 0)  // there are no non-blank lines
	{	
	newMultiRegionsBedUrl = "";
	newType = empty;
	}
    else
	newType = trashFile;
    }

char newSha1[(SHA_DIGEST_LENGTH + 1) * 2];
if (newType==trashFile)
    {
    // calculate sha1 checksum on new input.
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)dyInput->string, dyInput->stringSize, hash);
    hexBinaryString(hash, SHA_DIGEST_LENGTH, newSha1, (SHA_DIGEST_LENGTH + 1) * 2);
    }

// compare input sha1 to trashFile sha1 to see if same
boolean filesAreSame = FALSE;
if (oldType==trashFile && newType==trashFile)
    {
    lf = lineFileMayOpen(multiRegionsBedUrlSha1Name, TRUE);
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (sameString(line, newSha1))
	    filesAreSame = TRUE;
	}
    lineFileClose(&lf);
    }

// save new trashFile unless no changes.
if (newType==trashFile && (!(oldType==trashFile && filesAreSame) ))
    {
    struct tempName bedTn;
    trashDirFile(&bedTn, "hgt", "custRgn", ".bed");
    FILE *f = mustOpen(bedTn.forCgi, "w");
    mustWrite(f, dyInput->string, dyInput->stringSize);
    carefulClose(&f);
    // new value is a trash file.
    newMultiRegionsBedUrl = cloneString(bedTn.forCgi);
    // save new input sha1 to trash file.
    safef(multiRegionsBedUrlSha1Name, sizeof multiRegionsBedUrlSha1Name, "%s.sha1", bedTn.forCgi);
    f = mustOpen(multiRegionsBedUrlSha1Name, "w");
    mustWrite(f, newSha1, strlen(newSha1));
    carefulClose(&f);
    }

dyStringFree(&dyInput);

// if new value, set vars and cart
if (newMultiRegionsBedUrl)
    {
    multiRegionsBedUrl = newMultiRegionsBedUrl;
    cartSetString(cart, "multiRegionsBedUrl", multiRegionsBedUrl);
    }
cartRemove(cart, "multiRegionsBedInput");
}


boolean initVirtRegionsFromBedUrl(time_t *bedDateTime)
/* Read custom regions from BED URL */
{
multiRegionsBedUrl = cartUsualString(cart, "multiRegionsBedUrl", multiRegionsBedUrl);
int bedPadding = 0; // default no padding
if (sameString(multiRegionsBedUrl,""))
    {
    warn("No BED or BED URL specified.");
    return FALSE;
    }

struct lineFile *lf = NULL;
if (strstr(multiRegionsBedUrl,"://"))
    {
    lf = lineFileUdcMayOpen(multiRegionsBedUrl, FALSE);
    if (!lf)
	{
	warn("Unable to open [%s] with udc", multiRegionsBedUrl);
	return FALSE;
	}
    *bedDateTime = udcTimeFromCache(multiRegionsBedUrl, NULL);
    }
else
    {
    lf = lineFileMayOpen(multiRegionsBedUrl, TRUE);
    if (!lf)
	{
	warn("BED custom regions file [%s] not found.", multiRegionsBedUrl);
	return FALSE;
	}
    *bedDateTime = 0;
    // touch corresponding .sha1 file to save it from trash cleaner.
    char multiRegionsBedUrlSha1Name[1024];
    safef(multiRegionsBedUrlSha1Name, sizeof multiRegionsBedUrlSha1Name, "%s.sha1", multiRegionsBedUrl);
    if (fileExists(multiRegionsBedUrlSha1Name))
	readAndIgnore(multiRegionsBedUrlSha1Name);	
    }
char *line;
int lineSize;
int expectedFieldCount = -1;
struct bed *bed, *bedList = NULL;
while (lineFileNext(lf, &line, &lineSize))
    {
    // Process comments for keywords like database, shortDesc, and maybe others
    if (startsWith("#",line))
	{
	if (startsWith("#database ",line))
	    {
	    char *dbFromBed = line+strlen("#database ");
	    if (!sameString(database,dbFromBed))
		{
		warn("Multi-Region BED URL error: The database (%s) specified in input does not match current database %s",
		   dbFromBed, database);
		return FALSE;
		}
	    }
	if (startsWith("#shortDesc ",line))
	    {
	    virtModeShortDescr = cloneString(line+strlen("#shortDesc "));
	    }
	if (startsWith("#padding ",line))
	    {
	    bedPadding = sqlSigned(line+strlen("#padding "));
	    }
	continue;
	}

    char *row[15];
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 3)
	{
        warn("%s doesn't appear to be in BED format. 3 or more fields required, got %d",
                multiRegionsBedUrl, numFields);
	return FALSE;
	}
    if (expectedFieldCount == -1)
	{
	expectedFieldCount = numFields;
	}
    else
	{
	if (numFields != expectedFieldCount)
	    errAbort("Multi-Region BED was detected to have %d columns. But this row has %d columns. "
	    "All rows except comment lines should have the same number of columns", numFields, expectedFieldCount);
	}

    AllocVar(bed);
    // All fields are standard BED fields, no bedplus fields supported at this time.
    // note: this function does not validate chrom name or end beyond chrom size
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	loadAndValidateBed(row, numFields, numFields+0, lf, bed, NULL, TRUE); // can errAbort
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	warn("%s", errCatch->message->string);
	return FALSE;
	}
    errCatchFree(&errCatch);

    bed->chrom=cloneString(bed->chrom);  // loadAndValidateBed does not do it for speed. but bedFree needs it.

    struct chromInfo *ci = hGetChromInfo(database, bed->chrom);
    if (ci == NULL)
	{
        warn("Couldn't find chromosome/scaffold %s in database", bed->chrom);
	return FALSE;
	}
    if (bed->chromEnd > ci->size)
	{
        warn("BED chromEnd %u > size %u for chromosome/scaffold %s", bed->chromEnd, ci->size, bed->chrom);
	return FALSE;
	}
    if (!(bed->chromEnd > bed->chromStart)) // do not allow empty regions
	{
        warn("BED chromEnd %u must be greater than chromStart %u %s", bed->chromEnd, bed->chromStart, bed->chrom);
	return FALSE;
	}

    slAddHead(&bedList, bed);

    struct virtRegion *v;
    if (numFields < 12)
	{
	AllocVar(v);
	v->chrom = cloneString(bed->chrom);
	v->start = bed->chromStart;
	v->end   = bed->chromEnd;
	slAddHead(&virtRegionList, v);
	}
    else
	{
	int e;
	for (e = 0; e < bed->blockCount; ++e)
	    {
	    AllocVar(v);
	    v->chrom = cloneString(bed->chrom);
	    v->start = bed->chromStart + bed->chromStarts[e];
	    v->end   = v->start + bed->blockSizes[e];
	    slAddHead(&virtRegionList, v);
	    }
	}

    }
lineFileClose(&lf);
bedFreeList(&bedList);
slReverse(&virtRegionList);
if (bedPadding > 0)
    padVirtRegions(bedPadding);
return TRUE;
}

void restoreSavedVirtPosition()
/* Set state from lastDbPosCart.
 * This involves parsing the extra state that was saved.*/
{

struct hashEl *el, *elList = hashElListHash(lastDbPosCart->hash);
for (el = elList; el != NULL; el = el->next)
    {
    char *cartVar = el->name;
    char *cartVal = cartOptionalString(lastDbPosCart, cartVar);
    if (cartVal)
	{
	/* do we need this feature?
	if (sameString(cartVal,"(null)"))
	    cartRemove(cart, cartVar);
	else
        */	
	cartSetString(cart, cartVar, cartVal);
	}
    }
hashElFreeList(&elList);

}

void lastDbPosSaveCartSetting(char *cartVar)
/* Save var and value from cart into lastDbPosCart. */
{
cartSetString(lastDbPosCart, cartVar, cartUsualString(cart, cartVar, NULL));
}

void dySaveCartSetting(struct dyString *dy, char *cartVar, boolean saveBoth)
/* Grab var and value from cart, save as var=val to dy string. */
{
if (dy->stringSize > 0)
    dyStringAppend(dy, " ");
dyStringPrintf(dy, "%s=%s", cartVar, cartUsualString(cart, cartVar, NULL));
if (saveBoth)
    lastDbPosSaveCartSetting(cartVar);
}


boolean initRegionList()
/* initialize window list */
{

checkmultiRegionsBedInput();

struct virtRegion *v;
virtRegionList = NULL;
virtModeExtraState = "";   // This is state that determines if the virtChrom has changed
lastDbPosCart = cartOfNothing();  // USED to store and restore cart settings related to position and virtMode
struct dyString *dy = dyStringNew(256);  // used to build virtModeExtraState

if (sameString(virtModeType, "default"))
    {
    // Single window same as normal window
    // mostly good to test nothing was broken with single window
    AllocVar(v);

    v->chrom = chromName;
    v->start = 0;
    v->end = hChromSize(database, chromName);

    virtWinStart = winStart;
    virtWinEnd   = winEnd;

    slAddHead(&virtRegionList, v);
    slReverse(&virtRegionList);
    }
else if (sameString(virtModeType, "exonMostly")
      || sameString(virtModeType, "geneMostly"))
    {

    // Gencode settings: comprehensive, alt-splice, non-coding

    char *knownCanonical = NULL;  // show splice-variants, not filtered out via knownCanonical
    boolean showNoncoding = TRUE; // show non-coding where cdsStart==cdsEnd
    char *knownToTag = NULL;      // show comprehensive set not filtered by knownToTag
    char varName[SMALLBUF];
    boolean geneMostly = FALSE;

    lastDbPosSaveCartSetting("emGeneTable");

    //DISGUISE makes obsolete dySaveCartSetting(dy, "emGeneTable");
    //DISGUISE makes obsolete dySaveCartSetting(dy, "emPadding");
    if (sameString(virtModeType, "geneMostly"))
	geneMostly = TRUE;
    if (sameString(emGeneTable, "knownGene"))
	{
	// test cart var knownGene.show.noncoding
        // check for alternate table name.
        // if found, set and pass to gene-table reading routine

        // Some code borrowed from simpleTracks.c::loadKnownGene()

	safef(varName, sizeof(varName), "%s.show.noncoding", emGeneTable);
	showNoncoding = cartUsualBoolean(cart, varName, TRUE);
	//DISGUISE makes obsolete dySaveCartSetting(dy, varName);
	safef(varName, sizeof(varName), "%s.show.spliceVariants", emGeneTable);
	boolean showSpliceVariants = cartUsualBoolean(cart, varName, TRUE);
	//DISGUISE makes obsolete dySaveCartSetting(dy, varName);
	if (!showSpliceVariants)
	    {
	    char *canonicalTable = trackDbSettingOrDefault(emGeneTrack->tdb, "canonicalTable", "knownCanonical");
	    if (hTableExists(database, canonicalTable))
		knownCanonical = canonicalTable;
	    }
	safef(varName, sizeof(varName), "%s.show.comprehensive", emGeneTable);
	boolean showComprehensive = cartUsualBoolean(cart, varName, FALSE);
	//DISGUISE makes obsolete dySaveCartSetting(dy, varName);
	if (!showComprehensive)
	    {
	    if (hTableExists(database, "knownToTag"))
		{
		knownToTag = "knownToTag";
		}
	    }

	}
    if (sameString(emGeneTable, "refGene"))
	{
	char varName[SMALLBUF];
	safef(varName, sizeof(varName), "%s.hideNoncoding", emGeneTable);
	showNoncoding = !cartUsualBoolean(cart, varName, FALSE);
	//DISGUISE makes obsolete dySaveCartSetting(dy, varName);
	}

    initVirtRegionsFromEMGeneTableExons(showNoncoding, knownCanonical, knownToTag, geneMostly);
    if (!virtRegionList)
	{
	warn("No genes found on chrom %s for track %s, returning to default view", chromName, emGeneTrack->shortLabel);
	return FALSE;   // regionList is empty, nothing found.
	}
    if (geneMostly)
	virtModeShortDescr = "genes";
    else
	virtModeShortDescr = "exons";
    // DISGUISE makes obsolete dyStringPrintf(dy," %s %s", dy->string, knownCanonical, knownToTag);
    }
else if (sameString(virtModeType, "kcGenes")) // TODO obsolete
    {
    initVirtRegionsFromKnownCanonicalGenes("knownCanonical");
    virtModeShortDescr = "genes";
    }
else if (sameString(virtModeType, "customUrl"))
    {
    // custom regions from BED URL
    virtModeShortDescr = "customUrl";  // can be overridden by comment in input bed file
    time_t bedDateTime = 0;
    if (!initVirtRegionsFromBedUrl(&bedDateTime))
	{
	return FALSE; // return to default mode
	}
    dySaveCartSetting(dy, "multiRegionsBedUrl", TRUE);
    dyStringPrintf(dy, " %ld", (long)bedDateTime);
    }
else if (sameString(virtModeType, "singleTrans"))
    {
    singleTransId = cartUsualString(cart, "singleTransId", singleTransId);
    if (sameString(singleTransId, ""))
	{
	warn("Single transcript Id should not be blank");
	return FALSE; // return to default mode
	}
    setEMGeneTrack();
    dySaveCartSetting(dy, "singleTransId", TRUE);
    }
else if (sameString(virtModeType, "singleAltHaplo"))
    {
    singleAltHaploId = cartUsualString(cart, "singleAltHaploId", singleAltHaploId); // default is chr6_cox_hap2
    initAllChroms();  // we want to default to full genome view.
    if (!initSingleAltHaplotype(singleAltHaploId))
	{
	virtRegionList = NULL;
	return FALSE; // return to default mode
	}
    // was "single haplo" but that might confuse some users.
    virtModeShortDescr = endsWith(singleAltHaploId, "_fix") ? "fix patch" : "alt haplo";
    dySaveCartSetting(dy, "singleAltHaploId", TRUE);
    }
else if (sameString(virtModeType, "allChroms"))
    { // TODO more work on this mode
    //warn("show all regular chromosomes (not alts)\n"
	//"Warning must turn off all tracks except big*");
    initAllChroms();
    }
else if (sameString(virtModeType, "demo1"))
    {
    // demo two windows on two chroms (default posn chr21, and same loc on chr22

    //chr21:33,031,597-33,041,570
    AllocVar(v);
    //chr21:33,031,597-33,041,570
    v->chrom = "chr21";
    v->start = 33031597 - 1;
    v->end = 33041570;
    slAddHead(&virtRegionList, v);

    struct virtRegion *v2;

    AllocVar(v2);
    //chr22:33,031,597-33,041,570
    v2->chrom = "chr22";
    v2->start = 33031597 - 1;
    v2->end = 33041570;
    slAddHead(&virtRegionList, v2);

    slReverse(&virtRegionList);
    }
else if (sameString(virtModeType, "demo2"))
    {
    // demo multiple (70) windows on one chrom chr21 def posn, window size and step exon-like
    initExonStep();
    }
else if (sameString(virtModeType, "demo4"))
    {
    // demo multiple (20) windows showing exons from TITIN gene uc031rqd.1.
    initVirtRegionsFromEmGeneTable("uc031rqd.1"); // TITIN // "uc002ypa.3"); // SOD1
    }
else if (sameString(virtModeType, "demo5"))
    {
    // demo alt locus on hg38
    //  Shows alt chrom surrounded by preceding and following regions of same size from reference genome.
    initWindowsAltLoci();
    }
else if (sameString(virtModeType, "demo6"))
    {
    // demo SOD1
    //  Shows zoomed in exon-exon junction from SOD1 gene, between exon1 and exon2.

    initVirtRegionsFromSOD1Hardwired();

    }
else
    {
    // Unrecognized virtModeType
    return FALSE; // return to default mode
    }

virtModeExtraState = dyStringCannibalize(&dy);

if (!virtRegionList)
    return FALSE;   // regionList is empty, nothing found.

return TRUE;

}

boolean isLimitedVisHiddenForAllWindows(struct track *track)
/* Check if track limitedVis == hidden for all windows.
 * Return true if all are hidden */
{
boolean result = TRUE;
for(;track;track=track->nextWindow)
    if (track->limitedVis != tvHide)
	result = FALSE;
return result;
}

boolean anyWindowHaveItems(struct track *track)
/* Check if track limitedVis == hidden for all windows.
 * Return true if all are hidden */
{
for(;track;track=track->nextWindow)
    {
    if (slCount(track->items) > 0)
	return TRUE;
    }
return FALSE;
}

boolean isTypeBedLike(struct track *track)
/* Check if track type is BED-like packable thing (but not rmsk or joinedRmsk) */
{ // TODO GALT do we have all the types needed?
// TODO could it be as simple as whether track->items exists?
char *typeLine = track->tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return FALSE;
wordCount = chopLine(cloneString(typeLine), words);
if (wordCount <= 0)
    return FALSE;
type = words[0];
// NOTE: if type is missing here, full mode fails to return an hgTracks object
if (
(  sameWord(type, "bed")
|| sameWord(type, "bed5FloatScore")
|| sameWord(type, "bed6FloatScore")
|| sameWord(type, "bedDetail")
|| sameWord(type, "bigBed")
|| sameWord(type, "bigGenePred")
|| sameWord(type, "broadPeak")
|| sameWord(type, "chain")
|| sameWord(type, "factorSource")
|| sameWord(type, "genePred")
|| sameWord(type, "gvf")
|| sameWord(type, "narrowPeak")
|| sameWord(type, "bigNarrowPeak")
|| sameWord(type, "psl")
|| sameWord(type, "barChart")
|| sameWord(type, "bigBarChart")
|| sameWord(type, "bedMethyl")
|| sameWord(type, "interact")
|| sameWord(type, "bigInteract")
|| sameWord(type, "bigRmsk")
|| sameWord(type, "bigQuickLiftChain")
|| sameWord(type, "bigLolly")
//|| track->loadItems == loadSimpleBed
//|| track->bedSize >= 3 // should pick up several ENCODE BED-Plus types.
)
&& track->canPack
   )
    {
    return TRUE;
    }

return FALSE;
}

boolean isTypeUseItemNameAsKey(struct track *track)
/* Check if track type is like expRatio and key is just item name, to link across multi regions */
{
char *typeLine = track->tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return FALSE;
wordCount = chopLine(cloneString(typeLine), words);
if (wordCount <= 0)
    return FALSE;
type = words[0];
if (sameWord(type, "expRatio"))
    {
    // track is like expRatio, needs one row per item
    return TRUE;
    }
return FALSE;
}

boolean isTypeUseMapItemNameAsKey(struct track *track)
/* Check if track type is like interact and uses map item name to link across multi regions */
{
char *typeLine = track->tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return FALSE;
wordCount = chopLine(cloneString(typeLine), words);
if (wordCount <= 0)
    return FALSE;
type = words[0];
if (sameWord(type, "interact") || sameWord(type, "bigInteract"))
        return TRUE;
return FALSE;
}

void setFlatTrackMaxHeight(struct flatTracks *flatTrack, int fontHeight)
/* for each flatTrack, figure out maximum height needed from all windows */
{
struct track *track = flatTrack->track;
int maxHeight = 0;
struct track *winTrack;
struct window *window;
for (window=windows, winTrack=track; window; window=window->next, winTrack=winTrack->nextWindow)
    {
    setGlobalsFromWindow(window);

    int trackHeight =  trackPlusLabelHeight(winTrack, fontHeight);

    if (trackHeight > maxHeight)
	maxHeight = trackHeight;
    }
setGlobalsFromWindow(windows); // first window

flatTrack->maxHeight = maxHeight;
}

boolean doHideEmptySubtracksNoMultiBed(struct cart *cart, struct track *track)
/* TRUE if hideEmptySubtracks is enabled, but there is no multiBed */
{
char *multiBedFile = NULL;
char *subtrackIdFile = NULL;
boolean hideEmpties = compositeHideEmptySubtracks(cart, track->tdb, &multiBedFile, &subtrackIdFile);
if (hideEmpties && (multiBedFile == NULL || subtrackIdFile == NULL))
        return TRUE;
return FALSE;
}

struct hash *getNonEmptySubtracks(struct track *track)
{
/* Support setting to suppress display of empty subtracks. 
 * If multiBed is available, return hash with subtrack names as keys
 */

char *multiBedFile = NULL;
char *subtrackIdFile = NULL;
if (!compositeHideEmptySubtracks(cart, track->tdb, &multiBedFile, &subtrackIdFile))
    return NULL;
if (!multiBedFile)
    return NULL;

// load multiBed items in window
// TODO: filters here ?
// TODO:  protect against temporary network error ? */
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  bigBedFileOpenAlias(multiBedFile, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
char *row[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct hash *nonEmptySubtracksHash = hashNew(0);
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, row, ArraySize(row));
    // TODO: do this in bed3Sources.c
    char *idList = row[4];
    struct slName *ids = slNameListFromComma(idList);
    struct slName *id = NULL;
    for (id = ids; id != NULL; id = id->next)
        hashStore(nonEmptySubtracksHash, id->name);
    // TODO: free some stuff ?
    }

// read file containing ids of subtracks 
struct lineFile *lf = udcWrapShortLineFile(subtrackIdFile, NULL, 0); 
char *words[11];
int ct = 0;
while ((ct = lineFileChopNext(lf, words, sizeof words)) != 0)
    {
    char *id = words[0];
    if (hashLookup(nonEmptySubtracksHash, id))
        {
        int i;
        for (i=1; i<ct; i++)
            {
            hashStore(nonEmptySubtracksHash, cloneString(words[i]));
            }
        }
    }
lineFileClose(&lf);
return nonEmptySubtracksHash;
}

static void expandSquishyPackTracks(struct track *trackList)
/* Step through track list and duplicated tracks with squishyPackPoint defined */
{
if (windows->next)   // don't go into squishyPack mode if in multi-exon mode.
    return;

struct track *nextTrack = NULL, *track;
for (track = trackList; track != NULL; track = nextTrack)
    {
    nextTrack = track->next;

    if ((track->visibility != tvPack) || checkIfWiggling(cart, track))
        continue;

    char *string = cartOrTdbString(cart, track->tdb,  "squishyPackPoint", NULL);
    if (string != NULL)
        {
        double squishyPackPoint = atof(string);

        /* clone the track */
        char buffer[strlen(track->track) + strlen("Squinked") + 1];
        safef(buffer, sizeof buffer, "%sSquinked", track->track);

        struct track *squishTrack = CloneVar(track);
        squishTrack->tdb = CloneVar(track->tdb);
        squishTrack->tdb->originalTrack = squishTrack->tdb->track;
        squishTrack->tdb->track = cloneString(buffer);
        squishTrack->tdb->next = NULL;
        squishTrack->visibility = tvSquish;
        squishTrack->limitedVis = tvSquish;
        hashAdd(trackHash, squishTrack->tdb->track, squishTrack);
        struct linkedFeatures *lf = track->items;

        /* distribute the items based on squishyPackPoint */
        track->items = NULL;
        squishTrack->items = NULL;
        struct linkedFeatures *nextLf;
        for(; lf; lf = nextLf)
            {
            nextLf = lf->next;

            // if this is a hgFind match, it always is in pack, not squish
            if ((hgFindMatches != NULL) && hashLookup(hgFindMatches, lf->name))
                slAddHead(&track->items, lf);
            else if (lf->squishyPackVal > squishyPackPoint)
                slAddHead(&squishTrack->items, lf);
            else
                slAddHead(&track->items, lf);
            }

        // if the squish track has no items, don't bother including it
        if (slCount(squishTrack->items) == 0)
            continue;

        slReverse(&track->items);
        slReverse(&squishTrack->items);
        
        squishTrack->track = cloneString(buffer);
        squishTrack->originalTrack = cloneString(track->track);
        squishTrack->shortLabel = cloneString(track->shortLabel);
        squishTrack->longLabel = cloneString(track->longLabel);

        /* insert the squished track */
        track->next = squishTrack;
        squishTrack->next = nextTrack;
        }
    }
}

boolean forceWiggle; // we've run out of space so all tracks become coverage tracks

static void addPreFlatTrack(struct trackRef **list, struct track *track)
{
struct trackRef *trackRef;
AllocVar(trackRef);
trackRef->track = track;
slAddHead(list, trackRef);
}

void makeActiveImage(struct track *trackList, char *psOutput)
/* Make image and image map. */
{
struct track *track;
MgFont *font = tl.font;
struct hvGfx *hvg;
struct tempName pngTn;
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
struct image *theSideImg = NULL; // Because dragScroll drags off end of image,
                                 //    the side label gets seen. Therefore we need 2 images!!
//struct imgTrack *curImgTrack = NULL; // Make this global for now to avoid huge rewrite
struct imgSlice *curSlice    = NULL; // No need to be global, only the map needs to be global
//struct mapSet   *curMap      = NULL; // Make this global for now to avoid huge rewrite
// Set up imgBox dimensions
int sliceWidth[stMaxSliceTypes]; // Just being explicit
int sliceOffsetX[stMaxSliceTypes];
int sliceHeight        = 0;
int sliceOffsetY       = 0;
char *rulerTtl = NULL;

if (theImgBox)
// if theImgBox == NULL then we are rendering a simple single image such as right-click View image.
// theImgBox is a global for now to avoid huge rewrite of hgTracks.  It is started
// prior to this in doTrackForm()
    {
    rulerTtl = "drag select or click to zoom";
    hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
    hPrintf("<input type='hidden' name='c' value='%s'>\n", chromName);
    hPrintf("<input type='hidden' name='l' value='%d'>\n", winStart);
    hPrintf("<input type='hidden' name='r' value='%d'>\n", winEnd);
    hPrintf("<input type='hidden' name='pix' value='%d'>\n", tl.picWidth);
    // If a portal was established, then set the global dimensions to the entire expanded image size
    if (imgBoxPortalDimensions(theImgBox,&virtWinStart,&virtWinEnd,&(tl.picWidth),NULL,NULL,NULL,NULL,NULL))
        {
        pixWidth = tl.picWidth;
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth - gfxBorder - fullInsideX;
        }
    memset((char *)sliceWidth,  0,sizeof(sliceWidth));
    memset((char *)sliceOffsetX,0,sizeof(sliceOffsetX));
    if (withLeftLabels)
        {
        sliceWidth[stButton]   = trackTabWidth + 1;
        sliceWidth[stSide]     = leftLabelWidth - sliceWidth[stButton] + 1;
        sliceOffsetX[stSide]   =
                          (revCmplDisp ? (tl.picWidth - sliceWidth[stSide] - sliceWidth[stButton])
                                       : sliceWidth[stButton]);
        sliceOffsetX[stButton] = (revCmplDisp ? (tl.picWidth - sliceWidth[stButton]) : 0);
        }
    sliceOffsetX[stData] = (revCmplDisp ? 0 : sliceWidth[stSide] + sliceWidth[stButton]);
    sliceWidth[stData]   = tl.picWidth - (sliceWidth[stSide] + sliceWidth[stButton]);
    }
struct flatTracks *flatTracks = NULL;
struct flatTracks *flatTrack = NULL;

// There are two ways to get the amino acids to show up:
// 1) either by setting the ruler track viz to full
// 2) or by checking the box on the ruler track's trackUi page.
// Any selection on the trackUi page takes precedence. 
if (rulerMode != tvFull)
    {
    rulerCds = FALSE;
    }

// the code below will only use the checkbox on trackUi if a setting on the trackUi page has been made. 
if (cartVarExists(cart, BASE_SHOWCODONS) && zoomedToCdsColorLevel)
    rulerCds = cartUsualBoolean(cart, BASE_SHOWCODONS, TRUE);

/* Figure out height of each visible track. */
pixHeight = gfxBorder;

// figure out height of ruler
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

expandSquishyPackTracks(trackList);

/* Hash tracks/subtracks, limit visibility and calculate total image height: */

// For multiple windows, sets height and visibility
//       as well as making the flatTrack list.

// TODO there is a chance that for pack and squish
// I might need to trigger a track-height check here
// since it is after all items are loaded for all windows
// but before things are checked for overflow or limitedVis?
// The fixed non-overflow tracks like knownGene used to initialize
// ss and track height during loadItems(). That was delayed
// because we now need all windows to be fully loaded before
// calculating their joint ss layout and height.

// set heights and visibilities.
struct window *window = NULL;

// we may come through this way twice if we exceed the 32k limit for screen image
retry:
for(window=windows;window;window=window->next)
    {
    setGlobalsFromWindow(window);
    trackList = window->trackList;
    for (track = trackList; track != NULL; track = track->next)
	{
	if (tdbIsCompositeChild(track->tdb)) // When single track is requested via AJAX,
	    {
	    limitedVisFromComposite(track);  // it could be a subtrack
	    }
	else
	    {
	    limitVisibility(track);
	    }

	if (tdbIsComposite(track->tdb))
	    {
	    struct track *subtrack;
	    for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
		{
// JC - debug - should I be removing the next two lines?
		if (!isSubtrackVisible(subtrack))
		    continue;

		// subtrack vis can be explicit or inherited from composite/view.
		// Then it could be limited because of pixel height
		limitedVisFromComposite(subtrack);

		if (subtrack->limitedVis != tvHide)
		    {
		    subtrack->hasUi = track->hasUi;
		    }
		}
	    }
	}
    }
trackList = windows->trackList;
setGlobalsFromWindow(windows); // first window

struct slName *orderedWiggles = NULL;
char *sortTrack;
char *wigOrder = NULL;
if ((sortTrack = cgiOptionalString( "sortSim")) != NULL)
    {
    char buffer[1024];
    safef(buffer, sizeof buffer,  "simOrder_%s", sortTrack);
    wigOrder = cartString(cart, buffer);
    }

if ((sortTrack = cgiOptionalString( "sortExp")) != NULL)
    {
    char buffer[1024];
    safef(buffer, sizeof buffer,  "expOrder_%s", sortTrack);
    wigOrder = cartString(cart, buffer);
    }

if (wigOrder != NULL)
    {
    orderedWiggles = slNameListFromString(wigOrder, ' ');
    struct slName *name = orderedWiggles;
    // if we're sorting, remove existing sort order for this composite
    for(; name; name = name->next)
        {
        char buffer[1024];
        safef(buffer, sizeof buffer,  "%s_imgOrd", name->name);
        cartRemove(cart, buffer);
        }
    }

// Construct pre-flatTracks 
struct trackRef *preFlatTracks = NULL;
for (track = trackList; track != NULL; track = track->next)
    {
    if (isLimitedVisHiddenForAllWindows(track))
        continue;

    if (tdbIsComposite(track->tdb))
        {
        struct track *subtrack;
        if (isCompositeInAggregate(track))
            addPreFlatTrack(&preFlatTracks, track);
        else
            {
            boolean doHideEmpties = doHideEmptySubtracksNoMultiBed(cart, track);
                // If multibed was found, it has been used to suppress loading,
                // and subtracks lacking items in window are already set hidden
            for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
                {
                if (!isSubtrackVisible(subtrack))
                    continue;

                if (!isLimitedVisHiddenForAllWindows(subtrack) && 
                        !(doHideEmpties && !anyWindowHaveItems(subtrack)))
                        // Ignore subtracks with no items in windows

                    {
                    addPreFlatTrack(&preFlatTracks, subtrack);
                    }
                }
            }
        }
    else
	{	
        addPreFlatTrack(&preFlatTracks, track);
	}
    }
slReverse(&preFlatTracks);

// check total height
#define MAXSAFEHEIGHT "maxTrackImageHeightPx"
int maxSafeHeight = atoi(cfgOptionDefault(MAXSAFEHEIGHT, "32000"));
boolean safeHeight = TRUE;
struct trackRef *pfRef;
int tmpPixHeight = pixHeight;
for(pfRef = preFlatTracks; pfRef; pfRef = pfRef->next)
    {
    struct track *pf = pfRef->track;
    int totalHeight = tmpPixHeight+trackPlusLabelHeight(pf,fontHeight);
    if (totalHeight > maxSafeHeight)
        {
        if (!forceWiggle)
            {
            char buffer[1024];
            sprintLongWithCommas(buffer, totalHeight);
            warn("Image was over 32,000 pixels high (%s pix). All bed tracks are now displayed as density graphs. Zoom in to restore previous display modes.", buffer);
            forceWiggle = TRUE;
            goto retry;
            }
        char numBuf[SMALLBUF];
        sprintLongWithCommas(numBuf, maxSafeHeight);
        if (safeHeight)  // Only one message
            warn("Image is over %s pixels high (%d pix) at the following track which is now "
                 "hidden:<BR>\"%s\".%s", numBuf, totalHeight, pf->tdb->longLabel,
                 (pf->next != NULL ?
                      "\nAdditional tracks may have also been hidden at this zoom level." : ""));
        safeHeight = FALSE;
	struct track *winTrack;
	for(winTrack=pf;winTrack;winTrack=winTrack->nextWindow)
	    {
	    pf->limitedVis = tvHide;
	    pf->limitedVisSet = TRUE;
	    }
        }
    if (!isLimitedVisHiddenForAllWindows(pf))
        tmpPixHeight += trackPlusLabelHeight(pf, fontHeight);
    }
pixHeight = tmpPixHeight;

// Construct flatTracks
for(; preFlatTracks; preFlatTracks = preFlatTracks->next)
    flatTracksAdd(&flatTracks,preFlatTracks->track,cart, orderedWiggles);

flatTracksSort(&flatTracks); // Now we should have a perfectly good flat track list!

if (orderedWiggles)
    {
    // save order to cart
    struct flatTracks *ft;
    char buffer[4096];
    int count = 1;
    for(ft = flatTracks; ft; ft = ft->next)
        {
        safef(buffer, sizeof buffer, "%s_imgOrd", ft->track->track);
        cartSetInt(cart, buffer, count++);
        }
    }

// for each track, figure out maximum height needed from all windows
for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;

    if (track->limitedVis == tvHide)
	{
	continue;
	}

    setFlatTrackMaxHeight(flatTrack, fontHeight);

    }


// fill out track->prevTrack, and check for maxSafeHeight
struct track *prevTrack = NULL;
for (flatTrack = flatTracks,prevTrack=NULL; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;
    if (track->limitedVis == tvHide)
        continue;
    if (!isLimitedVisHiddenForAllWindows(track))
        {
	struct track *winTrack;
	for(winTrack=track;winTrack;winTrack=winTrack->nextWindow)
	    { // TODO this is currently still only using one prev track value.
	    winTrack->prevTrack = prevTrack; // Important for keeping track of conditional center labels!
	    }
        // ORIG pixHeight += trackPlusLabelHeight(track, fontHeight);
	if (!theImgBox) // prevTrack may have altered the height, so recalc height
	    setFlatTrackMaxHeight(flatTrack, fontHeight);
        prevTrack = track;
        }
    }

// allocate hvg png of pixWidth, pixHeight
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

    if (measureTiming)
        measureTime("Time at start of obtaining trash hgt png image file");
    trashDirFile(&pngTn, "hgt", "hgt", ".png");
    if (enableMouseOver)
	{   /* created here at this time to get the same name as .png file
	     * it is copied from pngTn since if we repeated trashFileDir()
	     * to get the name, it could be different since there is a
	     * timestamp involved in making the name.
	     */
        /* will open this file upon successful exit to write the data */
	AllocVar(mouseOverJsonFile);
	char *tmpStr = cloneString(pngTn.forCgi);
	char *jsonStr = replaceChars(tmpStr, ".png", ".json");
	safef(mouseOverJsonFile->forCgi, ArraySize(mouseOverJsonFile->forCgi), "%s", jsonStr);
	freeMem(tmpStr);
	freeMem(jsonStr);
	tmpStr = cloneString(pngTn.forHtml);
        jsonStr = replaceChars(tmpStr, ".png", ".json");
	safef(mouseOverJsonFile->forHtml, ArraySize(mouseOverJsonFile->forHtml), "%s", jsonStr);
	freeMem(tmpStr);
	freeMem(jsonStr);
	}
    hvg = hvGfxOpenPng(pixWidth, pixHeight, pngTn.forCgi, transparentImage);

    if (theImgBox)
        {
        // Adds one single image for all tracks (COULD: build the track by track images)
        theOneImg = imgBoxImageAdd(theImgBox,pngTn.forHtml,NULL,pixWidth, pixHeight,FALSE);
        theSideImg = theOneImg; // Unlkess this is overwritten below, there is a single image
        }

    hvgSide = hvg; // Unless this is overwritten below, there is a single image

    if (theImgBox && theImgBox->showPortal && withLeftLabels)
        {
        // TODO: It would be great to make the two images smaller,
        //       but keeping both the same full size for now
        struct tempName pngTnSide;
        trashDirFile(&pngTnSide, "hgtSide", "side", ".png");
        hvgSide = hvGfxOpenPng(pixWidth, pixHeight, pngTnSide.forCgi, transparentImage);

        // Also add the side image
        theSideImg = imgBoxImageAdd(theImgBox,pngTnSide.forHtml,NULL,pixWidth, pixHeight,FALSE);
        hvgSide->rc = revCmplDisp;
        initColors(hvgSide);
        }
    }
maybeNewFonts(hvg);
maybeNewFonts(hvgSide);
hvg->rc = revCmplDisp;
initColors(hvg);

/* Start up client side map. */
hPrintf("<MAP id='map' Name=%s>\n", mapName);

for (window=windows; window; window=window->next)
    {
    /* Find colors to draw in. */
    findTrackColors(hvg, window->trackList);
    }


// Good to go ahead and add all imgTracks regardless of buttons, left label, centerLabel, etc.
if (theImgBox)
    {
    if (rulerMode != tvHide)
        {
        curImgTrack = imgBoxTrackFindOrAdd(theImgBox,NULL,RULER_TRACK_NAME,rulerMode,FALSE,
                                           IMG_FIXEDPOS); // No tdb, no centerLbl, not reorderable
        }

    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
	if (!isLimitedVisHiddenForAllWindows(track))
            {
	    struct track *winTrack;
	    for (winTrack=track; winTrack; winTrack=winTrack->nextWindow)
		{
		if (winTrack->labelColor == winTrack->ixColor && winTrack->ixColor == 0)
		    {
		    winTrack->ixColor = hvGfxFindRgb(hvg, &winTrack->color);
		    }
		}
            int order = flatTrack->order;
            curImgTrack = imgBoxTrackFindOrAdd(theImgBox,track->tdb,NULL,track->limitedVis,
                                               isCenterLabelIncluded(track),order);
            if (flatTrack->track->originalTrack != NULL)
                curImgTrack->linked = TRUE;
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
        if (theImgBox)
            {
            // Mini-buttons (side label slice) for ruler
            sliceHeight      = height + 1;
            sliceOffsetY     = 0;
            curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stButton,NULL,NULL,
                                                   sliceWidth[stButton],sliceHeight,
                                                   sliceOffsetX[stButton],sliceOffsetY);
            }
        else if (!trackImgOnly) // Side buttons only need to be drawn when drawing page with js
            {                   // advanced features off  // TODO: Should remove wasted pixels too
            drawGrayButtonBox(hvgSide, trackTabX, y, trackTabWidth, height, TRUE);
            }
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
            // ORIG y += trackPlusLabelHeight(track, fontHeight);
	    y += flatTrack->maxHeight;
            yEnd = y;
            h = yEnd - yStart - 1;

            /* alternate button colors for track groups*/
            if (track->group != lastGroup)
                grayButtonGroup = !grayButtonGroup;
            lastGroup = track->group;
            if (theImgBox)
                {
                // Mini-buttons (side label slice) for tracks
                sliceHeight      = yEnd - yStart;
                sliceOffsetY     = yStart - 1;
                curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
                curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stButton,NULL,NULL,
                                                       sliceWidth[stButton],sliceHeight,
                                                       sliceOffsetX[stButton],sliceOffsetY);
                }
            else if (!trackImgOnly) // Side buttons only need to be drawn when drawing page
                {                   // with js advanced features off
                if (grayButtonGroup)
                    drawGrayButtonBox(hvgSide, trackTabX, yStart, trackTabWidth, h, track->hasUi);
                else
                    drawBlueButtonBox(hvgSide, trackTabX, yStart, trackTabWidth, h, track->hasUi);
                }

            if (track->hasUi)
                {
                char *title = track->track;
                if (track->originalTrack != NULL)
                    title = track->originalTrack;
                if (tdbIsCompositeChild(track->tdb))
                    {
                    struct trackDb *parent = tdbGetComposite(track->tdb);
                    mapBoxTrackUi(hvgSide, trackTabX, yStart, trackTabWidth, (yEnd - yStart - 1),
                                  parent->track, parent->shortLabel, track->track);
                    }
                else
                    mapBoxTrackUi(hvgSide, trackTabX, yStart, trackTabWidth, h, title,
                                  track->shortLabel, title);
                }
            }
        }
    butOff = trackTabX + trackTabWidth;
    leftLabelX += butOff;
    leftLabelWidth -= butOff;
    }


/* Draw left labels. */

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
        if (theImgBox)
            {
            // side label slice for ruler
            sliceHeight      = basePositionHeight + (rulerCds ? rulerTranslationHeight : 0) + 1;
            sliceOffsetY     = 0;
            curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,
                                                   sliceWidth[stSide],sliceHeight,
                                                   sliceOffsetX[stSide],sliceOffsetY);
            (void) sliceMapFindOrStart(curSlice,RULER_TRACK_NAME,NULL); // No common linkRoot
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
            char *shortChromName = cloneString(displayChromName );
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
            drawComplementArrow(hvgSide,leftLabelX, y, leftLabelWidth-1, baseHeight, font);
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
        if (theImgBox)
            {
            // side label slice for tracks
            //ORIG sliceHeight      = trackPlusLabelHeight(track, fontHeight);
	    sliceHeight      = flatTrack->maxHeight;
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,
                                                   sliceWidth[stSide],sliceHeight,
                                                   sliceOffsetX[stSide],sliceOffsetY);
            (void) sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            }

        boolean doWiggle = checkIfWiggling(cart, track);
        if (doWiggle && isEmpty(track->networkErrMsg))
            track->drawLeftLabels = wigLeftLabels;
    #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
        if (theImgBox && track->limitedVis != tvDense)
            y += sliceHeight;
        else
    #endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL
            {
            setGlobalsFromWindow(windows); // use GLOBALS from first window
            int ynew = 0;
            /* rmskJoined tracks are non-standard in FULL mode
               they are just their track height, not per-item height
             */
            if (startsWith("rmskJoined", track->track))
                ynew = flatTrack->maxHeight + y;
            else
                ynew = doLeftLabels(track, hvgSide, font, y);

            y += flatTrack->maxHeight;
            if ((ynew - y) > flatTrack->maxHeight)
                { // TODO should be errAbort?
                warn("doLeftLabels(y=%d) returned new y value %d that is too high - should be %d at most.",
                    y, ynew, flatTrack->maxHeight);
                }
            }
        }
    }
else
    {
    leftLabelX = leftLabelWidth = 0;
    }


/* Draw guidelines. */

if (virtMode && emAltHighlight)
    withGuidelines = TRUE;  // we cannot draw the alternating backgrounds without guidelines layer

if (withGuidelines)
    {
    struct hvGfx *bgImg = hvg; // Default to the one image
    boolean exists = FALSE;
    if (theImgBox)
        {
        struct tempName gifBg;
        char base[64];
	if (virtMode) // window separators
	    {
	    safecpy(base,sizeof(base),"winSeparators");  // non-reusable temp file
	    trashDirFile(&gifBg, "hgt", base, ".png");
	    exists = FALSE;
	    }
	else  // re-usable guidelines
	    {
	    safef(base,sizeof(base),"blueLines%d-%s%d-%d",pixWidth,(revCmplDisp?"r":""),fullInsideX,
		  guidelineSpacing);  // reusable file needs width, leftLabel start and guidelines
	    exists = trashDirReusableFile(&gifBg, "hgt", base, ".png");
	    if (exists && cgiVarExists("hgt.reset")) // exists means don't remake bg image. However,
		exists = FALSE;                      // for now, rebuild on "default tracks" request
	    }
        if (!exists)
            {
            bgImg = hvGfxOpenPng(pixWidth, pixHeight, gifBg.forCgi, TRUE);
            bgImg->rc = revCmplDisp;
            }
        imgBoxImageAdd(theImgBox,gifBg.forHtml,NULL,pixWidth, pixHeight,TRUE); // Adds BG image
        }

    if (!exists)
        {

        hvGfxSetClip(bgImg, fullInsideX, 0, fullInsideWidth, pixHeight);
        y = gfxBorder;

	if (virtMode)
	    {
	    // vertical windows separators

	    if (emAltHighlight)
		{
		// light blue alternating backgrounds
		Color lightBlue = hvGfxFindRgb(bgImg, &multiRegionAltColor);
		for (window=windows; window; window=window->next) // background under every other window
		    {
		    if (window->regionOdd)
                        hvGfxBox(bgImg, window->insideX, 0, window->insideWidth, pixHeight, lightBlue);
		    }
		}
	    else
		{
		// red vertical lines
		Color lightRed = hvGfxFindRgb(bgImg, &vertWindowSeparatorColor);
		for (window=windows->next; window; window=window->next) // skip first window, not needed
		    hvGfxBox(bgImg, window->insideX, 0, 1, pixHeight, lightRed);
		}
	    }
	else
	    {
	    int x;
	    Color lightBlue = hvGfxFindRgb(bgImg, &guidelineColor);
	    for (x = fullInsideX+guidelineSpacing-1; x<pixWidth; x += guidelineSpacing)
		hvGfxBox(bgImg, x, 0, 1, pixHeight, lightBlue);
	    }

        hvGfxUnclip(bgImg);
        if (bgImg != hvg)
            hvGfxClose(&bgImg);
        }
    }

if (theImgBox == NULL)  // imageV2 highlighting is done by javascript. This does pdf and view-image highlight
    drawHighlights(cart, hvg, imagePixelHeight);

/* Draw ruler */

if (rulerMode != tvHide)
    {
    newWinWidth = calcNewWinWidth(cart,virtWinStart,virtWinEnd,fullInsideWidth);

    if (theImgBox)
	{
	// data slice for ruler
	sliceHeight      = basePositionHeight + (rulerCds ? rulerTranslationHeight : 0) + 1;
	sliceOffsetY     = 0;
	curImgTrack = imgBoxTrackFind(theImgBox,NULL,RULER_TRACK_NAME);
	curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stData,theOneImg,rulerTtl,
					       sliceWidth[stData],sliceHeight,
					       sliceOffsetX[stData],sliceOffsetY);
	(void) sliceMapFindOrStart(curSlice,RULER_TRACK_NAME,NULL); // No common linkRoot
	}

    // need to have real winBaseCount to draw ruler scale

    for (window=windows; window; window=window->next)
	{
	setGlobalsFromWindow(window);

	if (theImgBox)
	    {
	    // Show window positions as mouseover
	    if (virtMode)
		{
		char position[256];
		safef(position, sizeof position, "%s:%d-%d", window->chromName, window->winStart+1, window->winEnd);
		int x = window->insideX;
		if (revCmplDisp)
		    x = tl.picWidth - (x + window->insideWidth);
		imgTrackAddMapItem(curImgTrack, "#", position,
		    x, sliceOffsetY, x+window->insideWidth, sliceOffsetY+sliceHeight, RULER_TRACK_NAME, NULL);
		}

	    }

	y = doDrawRuler(hvg,&rulerClickHeight,rulerHeight,yAfterRuler,yAfterBases,font,
			fontHeight,rulerCds, scaleBarTotalHeight, window);
	}

    setGlobalsFromWindow(windows); // first window

    }



/* Draw center labels. */

if (withCenterLabels)
    {
    setGlobalsFromWindow(windows); // first window

    hvGfxSetClip(hvg, fullInsideX, gfxBorder, fullInsideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;
        if (track->limitedVis == tvHide)
            continue;

        if (theImgBox)
            {
            // center label slice of tracks Must always make, even if the centerLabel is empty
            sliceHeight      = fontHeight;
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stCenter,theOneImg,NULL,
                                                   sliceWidth[stData],sliceHeight,
                                                   sliceOffsetX[stData],sliceOffsetY);
            (void) sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            if (isCenterLabelConditional(track)) // sometimes calls track height, especially when no data there
		{
                imgTrackUpdateCenterLabelSeen(curImgTrack,isCenterLabelConditionallySeen(track) ?
                                                                            clNowSeen : clNotSeen);
		}
            }

        int savey = y;
        y = doCenterLabels(track, track, hvg, font, y, fullInsideWidth); // calls track height
        y = savey + flatTrack->maxHeight;
        }
    hvGfxUnclip(hvg);

    setGlobalsFromWindow(windows); // first window
    }



/* Draw tracks. */

    { // brace allows local vars

    long lastTime = 0;
    y = yAfterRuler;
    if (measureTiming)
        lastTime = clock1000();

    // first do predraw
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        track = flatTrack->track;

	if (isLimitedVisHiddenForAllWindows(track))
            continue;

        struct track *winTrack;

        // do preDraw
        if (track->preDrawItems)
            {
            for (window=windows, winTrack=track; window; window=window->next, winTrack=winTrack->nextWindow)
                {
                setGlobalsFromWindow(window);
                if (winTrack->limitedVis == tvHide)
                    {
                    warn("Draw tracks skipping %s because winTrack->limitedVis=hide", winTrack->track);
                    continue;
                    }
                if (insideWidth >= 1)  // do not try to draw if width < 1.
                    {
                    doPreDrawItems(winTrack, hvg, font, y, &lastTime);
                    }
                }
            }

        setGlobalsFromWindow(windows); // first window
        // do preDrawMultiRegion across all windows, e.g. wig autoScale
        if (track->preDrawMultiRegion)
            {
            track->preDrawMultiRegion(track);
            }
        y += flatTrack->maxHeight;
        }

    // now do the actual draw
    y = yAfterRuler;
    for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
        {
        int savey = y;
        struct track *winTrack;
        track = flatTrack->track;
	if (isLimitedVisHiddenForAllWindows(track))
            continue;

        int centerLabelHeight = (isCenterLabelIncluded(track) ? fontHeight : 0);
        int yStart = y + centerLabelHeight;
	int yEnd   = y + flatTrack->maxHeight;

        if (theImgBox)
            {
            // data slice of tracks
            sliceOffsetY     = yStart;
            sliceHeight      = yEnd - yStart - 1;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            if (sliceHeight > 0)
                {
                curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stData,theOneImg,NULL,
                                                       sliceWidth[stData],sliceHeight,
                                                       sliceOffsetX[stData],sliceOffsetY);
                (void) sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
                }
            }
        // doDrawItems
        for (window=windows, winTrack=track; window; window=window->next, winTrack=winTrack->nextWindow)
            {
            setGlobalsFromWindow(window);
            if (winTrack->limitedVis == tvHide)
                {
                warn("Draw tracks skipping %s because winTrack->limitedVis=hide", winTrack->track);
                continue;
                }
            if (insideWidth >= 1)  // do not try to draw if width < 1.
                {
                int ynew = doDrawItems(winTrack, hvg, font, y, &lastTime);
                if ((ynew-y) > flatTrack->maxHeight)  // so compiler does not complain ynew is not used.
                    errAbort("oops track too high!");
                }
            }
        setGlobalsFromWindow(windows); // first window
        y = savey + flatTrack->maxHeight;

        if (theImgBox && tdbIsCompositeChild(track->tdb) &&
                (track->limitedVis == tvDense ||
                 (track->limitedVis == tvPack && centerLabelHeight == 0)))
            mapBoxToggleVis(hvg, 0, yStart,tl.picWidth, sliceHeight,track);
            // Strange mapBoxToggleLogic handles reverse complement itself so x=0,width=tl.picWidth

        if (yEnd != y)
            warn("Slice height for track %s does not add up.  Expecting %d != %d actual",
                 track->shortLabel, yEnd - yStart - 1, y - yStart);
        }

    calcWiggleOrdering(cart, flatTracks);
    y++;
    }

/* post draw tracks leftLabels */

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
        if (theImgBox)
            {
            // side label slice of tracks
	    sliceHeight      = flatTrack->maxHeight;
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            curSlice    = imgTrackSliceUpdateOrAdd(curImgTrack,stSide,theSideImg,NULL,
                                                   sliceWidth[stSide],sliceHeight,
                                                   sliceOffsetX[stSide],sliceOffsetY);
            (void) sliceMapFindOrStart(curSlice,track->tdb->track,NULL); // No common linkRoot
            }

        if (track->drawLeftLabels != NULL)
	    {
	    setGlobalsFromWindow(windows);
            y = doOwnLeftLabels(track, hvgSide, font, y);
	    setGlobalsFromWindow(windows); // first window
	    }
        else
	    y += flatTrack->maxHeight;
        }
    }

/* Make map background. */

y = yAfterRuler;
for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;
    if (track->limitedVis != tvHide)
        {
        if (theImgBox)
            {
            // Set imgTrack in case any map items will be set
	    sliceHeight      = flatTrack->maxHeight;
            sliceOffsetY     = y;
            curImgTrack = imgBoxTrackFind(theImgBox,track->tdb,NULL);
            }

	setGlobalsFromWindow(windows); // first window
        doTrackMap(track, hvg, y, fontHeight, trackPastTabX, trackPastTabWidth);

	y += flatTrack->maxHeight;
        }
    }

/* Finish map. */
hPrintf("</MAP>\n");

// turn off inPlaceUpdate when rows in imgTbl can arbitrarily reappear and disappear (see redmine #7306 and #6944)
jsonObjectAdd(jsonForClient, "inPlaceUpdate", newJsonBoolean(withLeftLabels && withCenterLabels));
jsonObjectAdd(jsonForClient, "rulerClickHeight", newJsonNumber(rulerClickHeight));
if(newWinWidth)
    {
    jsonObjectAdd(jsonForClient, "newWinWidth", newJsonNumber(newWinWidth));
    }

/* Save out picture and tell html file about it. */
if (hvgSide != hvg)
    hvGfxClose(&hvgSide);
hvGfxClose(&hvg);
if (measureTiming)
    measureTime("Time completed writing trash hgt png image file");

#ifdef SUPPORT_CONTENT_TYPE
char *type = cartUsualString(cart, "hgt.contentType", "html");
if(sameString(type, "jsonp"))
    {
    struct jsonElement *json = newJsonObject(newHash(8));

    printf("Content-Type: application/json\n\n");
    errAbortSetDoContentType(FALSE);
    jsonObjectAdd(json, "track", newJsonString(cartString(cart, "hgt.trackNameFilter")));
    jsonObjectAdd(json, "height", newJsonNumber(pixHeight));
    jsonObjectAdd(json, "width", newJsonNumber(pixWidth));
    jsonObjectAdd(json, "img", newJsonString(pngTn.forHtml));
    printf("%s(", cartString(cart, "jsonp"));
    hPrintEnable();
    jsonPrint((struct jsonElement *) json, NULL, 0);
    hPrintDisable();
    printf(")\n");
    return;
    }
else if(sameString(type, "png") || sameString(type, "pdf") || sameString(type, "eps"))
    {
    // following code bypasses html and return png's directly - see redmine 4888
    // NB: Pretty sure the pdf and eps options here are never invoked.  I don't see any
    // calls that would activate eps output, and pdf is locked behind an unused ifdef
    char *file;
    if(sameString(type, "pdf"))
        {
        printf("Content-Disposition: filename=hgTracks.pdf\nContent-Type: application/pdf\n\n");
        file = convertEpsToPdf(psOutput);
        unlink(psOutput);
        }
    else if(sameString(type, "eps"))
        {
        printf("Content-Disposition: filename=hgTracks.eps\nContent-Type: application/eps\n\n");
        file = psOutput;
        }
    else
        {
        printf("Content-Disposition: filename=hgTracks.png\nContent-Type: image/png\n\n");
        file = pngTn.forCgi;
        }

    char buf[4096];
    FILE *fd = fopen(file, "r");
    if(fd == NULL)
        // fail some other way (e.g. HTTP 500)?
        errAbort("Couldn't open png for reading");
    while (TRUE)
        {
        size_t n = fread(buf, 1, sizeof(buf), fd);
        if(n)
            fwrite(buf, 1, n, stdout);
        else
            break;
        }
    fclose(fd);
    unlink(file);
    return;
    }
#endif///def SUPPORT_CONTENT_TYPE

if (theImgBox)
    {
    imageBoxDraw(theImgBox);
    // If a portal was established, then set the global dimensions back to the portal size
    if (imgBoxPortalDimensions(theImgBox,NULL,NULL,NULL,NULL,&virtWinStart,&virtWinEnd,&(tl.picWidth),NULL))
        {
        pixWidth = tl.picWidth;
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth - gfxBorder - fullInsideX;
        }
    imgBoxFree(&theImgBox);
    }
else
    {
    char *titleAttr = "title='click or drag mouse in base position track to zoom in'";
    hPrintf("<IMG SRC='%s' BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s %s id='trackMap'",
            pngTn.forHtml, pixWidth, pixHeight, mapName, titleAttr);
    hPrintf("><BR>\n");
    }
flatTracksFree(&flatTracks);
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
    if (sameString(tdb->type, "downloadsOnly")) // These tracks should not even be seen by hgTracks.
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
        boolean avoidHandler = trackDbSettingOn(tdb, "avoidHandler");
        if (!avoidHandler && ( handler = lookupTrackHandlerClosestToHome(tdb)) != NULL)
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

int loadFromTrackDb(struct track **pTrackList)
/* Load tracks from database, consulting handler list. */
/* returns cartVersion desired. */
{
char *trackNameFilter = cartOptionalString(cart, "hgt.trackNameFilter");
struct trackDb *tdbList;
int trackDbCartVersion = 0;

if(trackNameFilter == NULL)
    tdbList = hTrackDbWithCartVersion(database, &trackDbCartVersion);
else
    {
    tdbList = hTrackDbForTrack(database, trackNameFilter);

    if (tdbList && tdbList->parent)        // we want to give the composite parent a chance to load and set options
        {
        while(tdbList->parent)
            {
            if (tdbList->parent->subtracks == NULL)     // we don't want to go up to a supertrack
                break;
            tdbList = tdbList->parent;
            }
        trackNameFilter = tdbList->track;
        }
    }
addTdbListToTrackList(tdbList, trackNameFilter, pTrackList);
return trackDbCartVersion;
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
            lf->useItemRgb = TRUE;
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
                lf->useItemRgb = TRUE;
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
            lf->useItemRgb = TRUE;
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
                lf->useItemRgb = TRUE;
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
    }
else if (startsWith("big", type))
    {
    struct bbiFile *bbi = ct->bbiFile;

    /* Find field counts, and from that revise the tdb->type to be more complete. */
    char extra = (bbi->fieldCount > bbi->definedFieldCount ? '+' : '.');
    char typeBuf[64];
    if (startsWithWord("bigBed", type))
	safef(typeBuf, sizeof(typeBuf), "bigBed %d %c", bbi->definedFieldCount, extra);
    else
	safecpy(typeBuf, sizeof(typeBuf), type);
    tdb->type = cloneString(typeBuf);

    /* Finish wrapping track around tdb. */
    tg = trackFromTrackDb(tdb);
    tg->bbiFile = bbi;
    tg->nextItemButtonable = TRUE;
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
else if (sameString(type, "bam") || sameString(type, "cram"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    bamMethods(tg);
    tg->mapItemName = ctMapItemName;
    }
else if (sameString(type, "vcfTabix"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    vcfTabixMethods(tg);
    tg->mapItemName = ctMapItemName;
    }
else if (sameString(type, "vcfPhasedTrio"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    vcfPhasedMethods(tg);
    tg->mapItemName = ctMapItemName;
    }
else if (sameString(type, "vcf"))
    {
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    vcfMethods(tg);
    tg->mapItemName = ctMapItemName;
    }
else if (sameString(type, "makeItems"))
    {
    tg = trackFromTrackDb(tdb);
    makeItemsMethods(tg);
    tg->nextItemButtonable = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "bedTabix")  || sameString(type, "longTabix"))
    {
    knetUdcInstall();
    tg = trackFromTrackDb(tdb);
    tg->customPt = ct;
    tg->mapItemName = ctMapItemName; /* must be here to see ctMapItemName */
    }
else if (sameString(type, "bedDetail"))
    {
    tg = trackFromTrackDb(tdb);
    bedDetailCtMethods(tg, ct);
    tg->mapItemName = ctMapItemName; /* must be here to see ctMapItemName */
    }
    else if (sameString(type, "adjacency"))
    {
    extern void adjacencyMethods(struct track *track);

    tg = trackFromTrackDb(tdb);
    adjacencyMethods(tg);
    //tg->mapItemName = ctMapItemName;
    tg->customPt = ct;
    }
else if (sameString(type, "pgSnp"))
    {
    tg = trackFromTrackDb(tdb);
    pgSnpCtMethods(tg);
    //tg->mapItemName = ctMapItemName;
    tg->customPt = ct;
    }
else if (sameString(type, "barChart"))
    {
    tg = trackFromTrackDb(tdb);
    barChartCtMethods(tg);
    tg->customPt = ct;
    }
else if (sameString(type, "interact"))
    {
    tg = trackFromTrackDb(tdb);
    interactCtMethods(tg);
    tg->customPt = ct;
    }
else if (sameString(type, "hic"))
    {
    tg = trackFromTrackDb(tdb);
    hicCtMethods(tg);
    }
else if (sameString(type, "bedMethyl"))
    {
    tg = trackFromTrackDb(tdb);
    bedMethylCtMethods(tg);
    tg->customPt = ct;
    }
else
    {
    errAbort("Unrecognized custom track type %s", type);
    }
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

static void rHashList(struct hash *hash, struct track *list)
/* Add list of tracks to hash including subtracks */
{
struct track *track;
for (track = list; track != NULL; track = track->next)
    {
    hashAdd(hash, track->track, track);
    if (track->subtracks != NULL)
       rHashList(hash, track->subtracks);
    }
}

static struct hash *hashTracksAndSubtracksFromList(struct track *list)
/* Make a hash and put all of tracks and subtracks on it. */
{
struct hash *hash = hashNew(12);
rHashList(hash, list);
return hash;
}

void makeDupeTracks(struct track **pTrackList)
/* Make up dupe tracks and append to list. Have to also crawl
 * through list to add subtracks */
{
if (!dupTrackEnabled())
    return;

struct dupTrack *dupList = dupTrackListFromCart(cart);
if (dupList == NULL)
    return;

/* Make up hash of tracks for quick finding of sources */
struct hash *trackHash = hashTracksAndSubtracksFromList(*pTrackList);

struct dupTrack *dup;
for (dup = dupList; dup != NULL; dup = dup->next)
    {
    struct track *source = hashFindVal(trackHash, dup->source);
    if (source != NULL)
        {
	struct track *track = CloneVar(source);
	struct trackDb *tdb = track->tdb = dupTdbFrom(source->tdb, dup);
	track->track = dup->name;
	track->shortLabel = tdb->shortLabel;
	track->longLabel = tdb->longLabel;
	struct trackDb *parentTdb = tdb->parent;
	if (parentTdb == NULL)
	    slAddHead(pTrackList, track);
	else
	    {
	    if (tdbIsFolder(parentTdb))
	        {
		refAdd(&parentTdb->children, tdb);
		slAddHead(pTrackList, track);
		}
	    else
	        {
		/* Add to parent Tdb */
		slAddHead(&parentTdb->subtracks, tdb);

		/* The parentTrack may correspond to the parent or grandParent tdb, look both places */
		struct track *parentTrack = hashFindVal(trackHash, parentTdb->track);
		if (parentTrack == NULL && parentTdb->parent != NULL)
		    parentTrack = hashFindVal(trackHash, parentTdb->parent->track);

		if (parentTrack != NULL)
		    slAddHead(&parentTrack->subtracks, track);
		else
		    warn("can't find parentTdb %s in makeDupeTracks", parentTdb->track);
		}
	    }

        if (track->wigCartData)
            {
            char *typeLine = tdb->type, *words[8];
            int wordCount = 0;
            words[0] = NULL;
            if (typeLine != NULL)
                wordCount = chopLine(cloneString(typeLine), words);
            track->wigCartData = wigCartOptionsNew(cart, track->tdb, wordCount, words);
            }
	}
    }
hashFree(&trackHash);
}


void loadTrackHubs(struct track **pTrackList, struct grp **pGrpList)
/* Load up stuff from data hubs and append to lists. */
{
struct trackDb *tdbList = hubCollectTracks(database, pGrpList);

addTdbListToTrackList(tdbList, NULL, pTrackList);
}

boolean restrictionEnzymesOk()
/* Check to see if it's OK to do restriction enzymes. */
{
return (sqlDatabaseExists("hgFixed") && hTableExists("hgFixed", "cutters") &&
        hTableExists("hgFixed", "rebaseRefs") &&
        hTableExists("hgFixed", "rebaseCompanies"));
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

static void rPropagateGroup(struct track *track, struct group *group)
// group should spread to multiple levels of children.
{
struct track *subtrack = track->subtracks;
for ( ;subtrack != NULL;subtrack = subtrack->next)
    {
    subtrack->group = group;
    rPropagateGroup(subtrack, group);
    }
}

static void groupTracks(struct track **pTrackList,
	struct group **pGroupList, struct grp *grpList, int vis)
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
boolean foundMap = FALSE;

/* build group objects from database. */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    if (sameString(grp->name, "map"))
        foundMap = TRUE;
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
    group->errMessage = grp->errMessage;
    slAddHead(&list, group);
    hashAdd(hash, grp->name, group);
    }
grpFreeList(&grps);

double priorityInc;
double priority = 1.00001;
if (grpList)
    {
    minPriority -= 1.0;             // priority is 1-based
    // the idea here is to get enough room between priority 1
    // (which is custom tracks) and the group with the next
    // priority number, so that the hub nestle inbetween the
    // custom tracks and everything else at the top of the list
    // of track groups
    priorityInc = (0.9 * minPriority) / slCount(grpList);
    priority = 1.0 + priorityInc;
    }
for(; grpList; grpList = grpList->next)
    {
    AllocVar(group);
    group->name = cloneString(grpList->name);
    group->label = cloneString(grpList->label);
    group->defaultPriority = group->priority = priority;
    group->errMessage = grpList->errMessage;
    group->defaultIsClosed = grpList->defaultIsClosed;
    priority += priorityInc;
    slAddHead(&list, group);
    hashAdd(hash, group->name, group);
    }
//
// If there isn't a map group, make one and set the priority so it will be right after custom tracks
// and hub groups.
if (!foundMap)
    {
    AllocVar(group);
    group->name = cloneString("map");
    group->label = cloneString("Mapping and Sequencing");
    group->defaultPriority = priority;
    group->priority = priority;
    group->defaultIsClosed = FALSE;
    slAddHead(&list, group);
    hashAdd(hash, "map", group);
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
    if ((group == NULL) && (track->groupName != NULL))
        {
        char *hubName = trackHubGetHubName(track->groupName);
        if (hubName != NULL)
            group = hashFindVal(hash, hubName);
        }
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
    rPropagateGroup(track, group);
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

void groupTrackListAddSuper(struct cart *cart, struct group *group, struct hash *superHash)
/* Construct a new track list that includes supertracks, sort by priority,
 * and determine if supertracks have visible members.
 * Replace the group track list with this new list.
 * Shared by hgTracks and configure page to expand track list,
 * in contexts where no track display functions (which don't understand
 * supertracks) are invoked. */
{
struct trackRef *newList = NULL, *tr, *ref;

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
hButtonWithOnClick(var, paddedLabel, NULL, "return imageV2.navigateButtonClick(this);");
}

void limitSuperTrackVis(struct track *track)
/* Limit track visibility by supertrack parent */
{
if (tdbIsSuperTrackChild(track->tdb))
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
// Open track associated with search position if any. Also open its parents if any.
{
if (NULL != hgp && NULL != hgp->tableList && NULL != hgp->tableList->name)
    {
    char *tableName = hgp->tableList->name;
    struct track *matchTrack = rFindTrackWithTable(tableName, trackList);
    if (matchTrack != NULL)
        tdbSetCartVisibility(matchTrack->tdb, cart, hCarefulTrackOpenVis(database, matchTrack->track));
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

// load the track list and check to see if we need to rewrite the cart
int cartVersionFromTrackDb = loadFromTrackDb(&trackList);
int cartVersionFromCart = cartGetVersion(cart);
if (cartVersionFromTrackDb > cartVersionFromCart)
    cartRewrite(cart, cartVersionFromTrackDb, cartVersionFromCart);

if (measureTiming)
    measureTime("Time after trackDbLoad ");
if (pcrResultParseCart(database, cart, NULL, NULL, NULL))
    slSafeAddHead(&trackList, pcrResultTg());
if (userSeqString != NULL)
    slSafeAddHead(&trackList, userPslTg());
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

struct grp *grpList = NULL;
if (cartOptionalString(cart, "hgt.trackNameFilter") == NULL)
    { // If a single track was asked for and it is from a hub, then it is already in trackList
    loadTrackHubs(&trackList, &grpList);
    }
loadCustomTracks(&trackList);
makeDupeTracks(&trackList);
groupTracks( &trackList, pGroupList, grpList, vis);
setSearchedTrackToPackOrFull(trackList);
char *rtsLoad = cgiOptionalString( "rtsLoad");
if (rtsLoad)  // load a recommended track set using the merge method
    {
    // store session name and user
    char *otherUserName = cartOptionalString(cart, hgsOtherUserName);
    char *otherUserSessionName = rtsLoad;

    // Hide all tracks except custom tracks
    struct hash *excludeHash = newHash(2);
    hashStore(excludeHash, "user");
    changeTrackVisExclude(groupList, NULL, tvHide, excludeHash);

    // delete any ordering we have
    char wildCard[32];
    safef(wildCard,sizeof(wildCard),"*_%s",IMG_ORDER_VAR);
    cartRemoveLike(cart, wildCard);

    // now we have to restart to load the session since that happens at cart initialization
    
    char newUrl[4096];
    safef(newUrl, sizeof newUrl,
        "./hgTracks?"
        hgsOtherUserSessionName "=%s"
        "&" hgsOtherUserName "=%s"
        "&" hgsMergeCart "=on"
        "&" hgsDoOtherUser "=submit"
	"&hgsid=%s"
        , otherUserSessionName, otherUserName,cartSessionId(cart));

    cartCheckout(&cart);   // make sure cart records all our changes above

    // output the redirect and exit
    printf("<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0;URL=%s\">", newUrl);
    exit(0);
    }

boolean hideTracks = cgiOptionalString( "hideTracks") != NULL;
if (hideTracks)
    changeTrackVis(groupList, NULL, tvHide);    // set all top-level tracks to hide

/* Get visibility values if any from ui. */
struct hash *superTrackHash = newHash(5);  // cache whether supertrack is hiding tracks or not
char buffer[4096];

// Check to see if we have a versioned default gene track and let the knownGene 
// cart variable determine its visibility
char *defaultGeneTrack = NULL;
char *knownDb = hdbDefaultKnownDb(database);
if (differentString(knownDb, database))
    defaultGeneTrack = hdbGetMasterGeneTrack(knownDb);

if (defaultGeneTrack)
    {
    char *s = cartOptionalString(cart, "knownGene");
    if ((s != NULL) && (differentString(s, "hide")))
        cartSetString(cart, defaultGeneTrack, s);
    }

for (track = trackList; track != NULL; track = track->next)
    {
    // deal with any supertracks we're seeing for the first time
    if (tdbIsSuperTrackChild(track->tdb))
        {
        struct hashEl *hel = NULL;

        if ((hel = hashLookup(superTrackHash, track->tdb->parent->track)) == NULL)   // we haven't seen this guy
            {
            // first deal with visibility of super track
            char *s = hideTracks ? cgiOptionalString(track->tdb->parent->track) : cartOptionalString(cart, track->tdb->parent->track);
            if (s)
                {
                track->tdb->parent->visibility = hTvFromString(s) ;
                cartSetString(cart, track->tdb->parent->track, s);
                }
            else if (startsWith("hub_", track->tdb->parent->track))
                {
                s = hideTracks ? cgiOptionalString( trackHubSkipHubName(track->tdb->parent->track)) :  cartOptionalString( cart, trackHubSkipHubName(track->tdb->parent->track));
                if (s)
                    {
                    cartSetString(cart, track->tdb->parent->track, s);
                    cartRemove(cart, trackHubSkipHubName(track->tdb->parent->track)); // remove the undecorated version
                    track->tdb->parent->visibility = hTvFromString(s) ;
                    }
                }
            
            // now look to see if we have a _hideKids statement to turn off all subtracks (including the current one)
            unsigned hideKids = 0;
            char *usedThis = buffer;
            safef(buffer, sizeof buffer, "%s_hideKids", track->tdb->parent->track);

            s = cartOptionalString(cart, buffer);
            if (s == NULL && startsWith("hub_", track->tdb->parent->track))
                s = cartOptionalString(cart, usedThis = trackHubSkipHubName(buffer));

            if (s != NULL)
                {
                hideKids = 1;
                cartRemove(cart, usedThis);  // we don't want this hanging out in the cart
                }

            // mark this as having been addressed
            hel = hashAddInt(superTrackHash, track->tdb->parent->track, hideKids );  
            }

        if ( ptToInt(hel->val) == 1)    // we want to hide this track
            {
            if (tvHide == track->tdb->visibility)
                /* remove if setting to default vis */
                cartRemove(cart, track->track);
            else
                cartSetString(cart, track->track, "hide");
            track->visibility = tvHide;
            }
        }
    
    // we use cgiOptionString because the above code may have turned off the track in the cart if
    // the user requested that all the default tracks be turned off
    char *s = hideTracks ? cgiOptionalString(track->track) : cartOptionalString(cart, track->track);

    if (s != NULL)
        {
        if (!track->limitedVisSet)
            {
            track->visibility = hTvFromString(s); 
            cartSetString(cart, track->track, s);
            }
        }
    else
        {
        // maybe this track is on the URL without the hub_ prefix
        if (startsWith("hub_", track->track))
            s = cgiOptionalString(trackHubSkipHubName(track->track));
        if (s != NULL && !track->limitedVisSet)
            {
            track->visibility = hTvFromString(s);
            cartSetString(cart, track->track, s);   // add the decorated visibility to the cart
            cartRemove(cart, trackHubSkipHubName(track->track)); // remove the undecorated version
            }
        }

    // now deal with composite track children
    if (tdbIsComposite(track->tdb) || tdbIsMultiTrack(track->tdb))
        {
        char *usedThis = buffer;

        // first check to see if we've been asked to hide all the subtracks
        boolean hideKids = FALSE;
        safef(buffer, sizeof buffer, "%s_hideKids", track->track);

        s = cartOptionalString(cart, buffer);
        if (s == NULL && startsWith("hub_", track->track))
            s = cartOptionalString(cart, usedThis = trackHubSkipHubName(buffer));
        if (s != NULL)
            hideKids = TRUE;
        cartRemove(cart, usedThis);   // we don't want these _hideKids variables in the cart

        // now see if we have any specified visibilities
        struct track *subtrack;
        for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
            {
            boolean undecoratedVis = FALSE;
            char *s = hideTracks ? cgiOptionalString( subtrack->track) : cartOptionalString(cart, subtrack->track);
            if (s == NULL && startsWith("hub_", subtrack->track))
                {
                undecoratedVis = TRUE;
                s = hideTracks ? cgiOptionalString(trackHubSkipHubName(subtrack->track)) : cartOptionalString(cart, trackHubSkipHubName(subtrack->track));
                }

            safef(buffer, sizeof buffer, "%s_sel", subtrack->track);
            if (s != NULL)
                {
                subtrack->visibility = hTvFromString(s);
                cartSetString(cart, subtrack->track, s);
                if (sameString("hide", s))
                    cartSetString(cart, buffer, "0");
                else
                    cartSetString(cart, buffer, "1");
                if (undecoratedVis)
                    cartRemove(cart, trackHubSkipHubName(subtrack->track)); // remove the undecorated version
                }
            else if (hideKids && isSubtrackVisible(subtrack))
                {
                cartSetString(cart, buffer, "0");
                subtrack->subTrackVis = tvHide;
                subtrack->subTrackVisSet = TRUE;
                }
            }
        }
    }
return trackList;
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

static boolean shouldBreakAll(char *text)
/* Check if any word in the label exceeds the cell width and should
 * be broken. */
{
/* Must match htdocs/style/HGStyle.css .trackListId between min and max width
 * it CSS to allow room for different character width calculations in web
 * browser. */
static const int MAX_WIDTH_CHARS = 20;
int wordLen = 0;
for (char *textPtr = text; *textPtr != '\0'; textPtr++)
    {
    if (isspace(*textPtr))
        {
        if (wordLen > MAX_WIDTH_CHARS)
            return TRUE;  // early exit
        wordLen = 0;
        }
    else
        {
        wordLen++;
        }
    }
// check gain if there are no spaces.
return (wordLen > MAX_WIDTH_CHARS);
}

void myControlGridStartCell(struct controlGrid *cg, boolean isOpen, char *id, boolean breakAll)
/* Start a new cell in control grid; support Javascript open/collapsing by including id's in tr's.
   id is used as id prefix (a counter is added to make id's unique). The breakAll arguments
   indicates if breaking anywhere to prevent cell overflow is allowed vs only word-break.
*/
{
static int counter = 1;
if (cg->columnIx == cg->columns)
    controlGridEndRow(cg);
if (!cg->rowOpen)
    {
    // use counter to ensure unique tr id's (prefix is used to find tr's in javascript).
    printf("<tr %sid='%s-%d'>", isOpen ? "" : "style='display: none' ", id, counter++);
    cg->rowOpen = TRUE;
    }
char *cls = breakAll ? "trackLabelTd trackLabelTdBreakAll" : "trackLabelTd";
if (cg->align)
    printf("<td class='%s' align=%s>", cls, cg->align);
else
    printf("<td class='%s'>", cls);
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
    if (track->parent)  // has super track
        pruneRedundantCartVis(track->parent);
        
    char *cartVis = cartOptionalString(cart, track->track);
    if (cartVis == NULL)
        continue;

    if (tdbIsSuper(track->tdb))
        {
        if ((sameString("hide", cartVis) && (track->tdb->isShow == 0)) ||
            (sameString("show", cartVis) && (track->tdb->isShow == 1)))
            cartRemove(cart, track->track);
        }
    else if (hTvFromString(cartVis) == track->tdb->visibility)
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
	    subtrack->preDrawItems = NULL;
	    subtrack->limitedVis = tvDense;
	    subtrack->limitedVisSet = TRUE;
	    }
	}
    }
else if (maxWinToDraw > 1 && (winEnd - winStart) > maxWinToDraw)
    {
    tg->loadItems = dontLoadItems;
    tg->drawItems = drawMaxWindowWarning;
    tg->preDrawItems = NULL;
    tg->limitedVis = tvDense;
    tg->limitedVisSet = TRUE;
    }
}

void printTrackInitJavascript(struct track *trackList)
{
hPrintf("<input type='hidden' id='%s' name='%s' value=''>\n", hgtJsCommand, hgtJsCommand);
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
        if (shapedByubtrackOverride)
            track->visibility = tdbVisLimitedByAncestors(cart,track->tdb,TRUE,TRUE);
        }
    if ((shapedByubtrackOverride || cleanedByContainerSettings)
    &&  tdbIsSuperTrackChild(track->tdb))  // Either cleanup may require supertrack intervention
        {   // Need to update track visibility
            // Unfortunately, since supertracks are not in trackList, this occurs on superChildren,
            // So now we need to find the supertrack and take changed cart values of its children
        struct slRef *childRef;
        for (childRef = track->tdb->parent->children;childRef != NULL;childRef = childRef->next)
            {
            struct trackDb * childTdb = childRef->val;
            struct track *child = hashFindVal(trackHash, childTdb->track);
            if (child != NULL && child->track!=NULL)
                {
                char *cartVis = cartOptionalString(cart,child->track);
                if (cartVis)
                    child->visibility = hTvFromString(cartVis);
                }
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
    boolean doLoadSummary;
    };

static boolean isTrackForParallelLoad(struct track *track)
/* Is this a track that should be loaded in parallel ? */
{
char *bdu = trackDbSetting(track->tdb, "bigDataUrl");

return customFactoryParallelLoad(bdu, track->tdb->type, database, TRUE) && (track->subtracks == NULL);
}

static void findLeavesForParallelLoad(struct track *trackList, struct paraFetchData **ppfdList, boolean doLoadSummary)
/* Find leaves of track tree that are remote network resources for parallel-fetch loading */
{
struct track *track;
if (!trackList)
    return;
for (track = trackList; track != NULL; track = track->next)
    {

    char *quickLiftFile = trackDbSetting(track->tdb, "quickLiftUrl");
    if (doLoadSummary && quickLiftFile)
      continue;    
 
    if (doLoadSummary && !track->loadSummary)
      continue;    

    if (doLoadSummary && startsWith("bigLolly", track->tdb->type)) 
      continue;    

    if (track->visibility != tvHide)
	{
	if (isTrackForParallelLoad(track))
	    {
	    struct paraFetchData *pfd;
	    AllocVar(pfd);
	    pfd->track = track;  // need pointer to be stable
            pfd->doLoadSummary = doLoadSummary;
	    slAddHead(ppfdList, pfd);
	    track->parallelLoading = TRUE;
	    }
	struct track *subtrack;
        for (subtrack=track->subtracks; subtrack; subtrack=subtrack->next)
	    {

	    char *quickLiftFile = cloneString(trackDbSetting(subtrack->tdb, "quickLiftUrl"));
	    if (doLoadSummary && quickLiftFile)
	      continue;    

	    if (doLoadSummary && !subtrack->loadSummary)
	      continue;    

	    if (doLoadSummary && startsWith("bigLolly", subtrack->tdb->type)) 
	      continue;    

	    if (isTrackForParallelLoad(subtrack))
		{
		if (tdbVisLimitedByAncestors(cart,subtrack->tdb,TRUE,TRUE) != tvHide)
		    {
		    struct paraFetchData *pfd;
		    AllocVar(pfd);
		    pfd->track = subtrack;  // need pointer to be stable
		    pfd->doLoadSummary = doLoadSummary;
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

static void checkHideEmptySubtracks(struct track *tg)
/* Suppress queries on subtracks w/o data in window (identified from multiIntersect file) */
{
if (!tdbIsComposite(tg->tdb))
    return;
struct hash *nonEmptySubtracksHash = getNonEmptySubtracks(tg);
if (!nonEmptySubtracksHash)
    return;
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (!isSubtrackVisible(subtrack))
        continue;
    if (!hashLookup(nonEmptySubtracksHash, trackHubSkipHubName(subtrack->track)))
        {
        subtrack->loadItems = dontLoadItems;
        subtrack->limitedVis = tvHide;
        subtrack->limitedVisSet = TRUE;
        }
    }
}


int tdbHasDecorators(struct track *track)
{
struct slName *decoratorSettings = trackDbSettingsWildMatch(track->tdb, "decorator.*");
if (decoratorSettings)
    {
    slNameFreeList(&decoratorSettings);
    return 1;
    }
return 0;
}


void loadDecorators(struct track *track)
/* Load decorations from a source (file) and put them into decorators, then add those
 * decorators to the list of the track's decorators.  Within the new decorators, each
 * of the decorations should have been entered into a hash table that is indexed by the
 * name of the linked feature.  That way when we're drawing the linked feature, we'll
 * be able to quickly locate the associated decorations.
 */
{
struct trackDb *decoratorTdbs = getTdbsForDecorators(track->tdb);
for (struct trackDb *decoratorTdb = decoratorTdbs; decoratorTdb != NULL;
        decoratorTdb = decoratorTdb->next)
    {
    char *decoratorUrl = trackDbSetting(decoratorTdb, "bigDataUrl");
    if (!decoratorUrl)
        {
        warn ("Decorator tags are present as %s, but no decorator file specified (bigDataUrl is missing)",
                decoratorTdb->track);
        continue;
        }

    struct bbiFile *bbi = NULL;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        if (isValidBigDataUrl(decoratorUrl,TRUE, database, TRUE))
            bbi = bigBedFileOpenAlias(decoratorUrl, chromAliasFindAliases);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        warn ("Network error while attempting to retrieve decorations from %s (track %s) - %s",
                decoratorUrl, track->track, dyStringContents(errCatch->message));
        continue;
        }
    errCatchFree(&errCatch);

    struct asObject *decoratorFileAsObj = asParseText(bigBedAutoSqlText(bbi));
    if (!asColumnNamesMatchFirstN(decoratorFileAsObj, decorationAsObj(), DECORATION_NUM_COLS))
        {
        warn ("Decoration file associated with track %s (%s) does not match the expected format - see decoration.as",
                track->track, decoratorUrl);
        continue;
        }

    struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, decoratorTdb);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *result = bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);

    struct mouseOverScheme *mouseScheme = mouseOverSetupForBbi(decoratorTdb, bbi);

    // insert resulting entries into track->decorators, leaving room for having multiple sources applied
    // to the same track in the future.
    if (track->decoratorGroup == NULL)
        track->decoratorGroup = newDecoratorGroup();

    struct decorator* newDecorators = decoratorListFromBbi(decoratorTdb, chromName, result, filters, bbi, mouseScheme);
    track->decoratorGroup->decorators = slCat(track->decoratorGroup->decorators, newDecorators);
    for (struct decorator *d = track->decoratorGroup->decorators; d != NULL; d = d->next)
        d->group = track->decoratorGroup;
    lmCleanup(&lm);
    bigBedFileClose(&bbi);
    }
}

static void *remoteParallelLoad(void *x)
/* Each thread loads tracks in parallel until all work is done. */
{
struct paraFetchData *pfd = NULL;
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

        if (pfd->doLoadSummary)
	    {
	    pfd->track->loadSummary(pfd->track);
	    }
	else
            {
	    checkMaxWindowToDraw(pfd->track);
	    checkHideEmptySubtracks(pfd->track);
	    pfd->track->loadItems(pfd->track); 
	    if (tdbHasDecorators(pfd->track))
		{
		loadDecorators(pfd->track);
		decoratorMethods(pfd->track);
		}
            }

	pfd->done = TRUE;
	}
    errCatchEnd(errCatch);
    if (errCatch->gotWarning)
        {
        // do something intelligent to permit reporting of warnings
        // Can't pass it to warn yet - the fancy warnhandlers aren't ready
        }
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
    safef(temp, sizeof temp, "Track timed out: %s took more than %d milliseconds to load. Zoom in or increase max load time via menu 'Genome Browser > Configure'", pfd->track->track, maxTimeInMilliseconds);
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

static int avgLoadTime(struct track *track)
/* calculate average loadtime across all windows */
{
int totalLoadTime = 0;
int winCount = 0;
while(track)
    {
    ++winCount;
    totalLoadTime += track->loadTime;
    track = track->nextWindow;
    }
return (((float)totalLoadTime / winCount) + 0.5);
}

static int avgDrawTime(struct track *track)
/* calculate average drawtime across all windows */
{
int totalDrawTime = 0;
int winCount = 0;
while(track)
    {
    ++winCount;
    totalDrawTime += track->drawTime;
    track = track->nextWindow;
    }
return (((float)totalDrawTime / winCount) + 0.5);
}

static int avgWigMafLoadTime(struct track *track)
/* calculate average wigMaf loadtime across all windows */
{
int totalLoadTime = 0;
int winCount = 0;
while(track)
    {
    ++winCount;
    if (startsWith("wigMaf", track->tdb->type))
	if (track->subtracks)
	    if (track->subtracks->loadTime)
		totalLoadTime += track->subtracks->loadTime;
    track = track->nextWindow;
    }
return (((float)totalLoadTime / winCount) + 0.5);
}

static void printTrackTiming()
{
hPrintf("<span class='trackTiming'>track, load time, draw time, total (first window)<br />\n");
if (virtMode)
    hPrintf("<span class='trackTiming'><idiv style='color:red' >average for all windows in red</idiv><br />\n");
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
		{
                hPrintf("%s, %d, %d, %d<br />\n", subtrack->shortLabel,
                            subtrack->loadTime, subtrack->drawTime,
                            subtrack->loadTime + subtrack->drawTime);
		if (virtMode)
		    {
		    int avgLoad = avgLoadTime(subtrack);
		    int avgDraw = avgDrawTime(subtrack);
		    hPrintf("<idiv style='color:red' >%s, %d, %d, %d</idiv><br />\n", subtrack->shortLabel,
				avgLoad, avgDraw,
				avgLoad + avgDraw);
		    }
		}
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
	if (virtMode)
	    {
	    int avgLoad = avgLoadTime(track);
	    int avgDraw = avgDrawTime(track);
	    hPrintf("<idiv style='color:red' >%s, %d, %d, %d</idiv><br />\n", track->shortLabel,
			avgLoad, avgDraw,
			avgLoad + avgDraw);
	    int avgWigMafLoad = avgWigMafLoadTime(track);
	    if (avgWigMafLoad > 0)
		{
		hPrintf("<idiv style='color:red' >&nbsp; &nbsp; %s wiggle, load %d</idiv><br />\n",
			    track->shortLabel, avgWigMafLoad);
		}
	    }
    }
hPrintf("</span>\n");
}

void initTrackList()
/* need to init tracklist, sometimes early */
{
if (!trackList)
    {
    if (measureTiming)
	measureTime("Time before getTrackList");
    boolean defaultTracks = cgiVarExists("hgt.reset");
    trackList = getTrackList(&groupList, defaultTracks ? -1 : -2);
    if (measureTiming)
	measureTime("Time after visibilities");
    makeGlobalTrackHash(trackList);
    }
}

struct track *getTrackListForOneTrack(char *trackName)
/* Fetch trackList for a single trackName using hgt.trackNameFilter. */
{
struct track *saveTrackList = trackList;
struct group *saveGroupList = groupList;
char *saveTrackNameFilter = cloneString(cartOptionalString(cart, "hgt.trackNameFilter"));
// This is an attempt to both get around the limitation imposed by ajax callback hgt.trackNameFilter,
//  and also to try to optimize it a little so that callbacks only have to load a trackList containing
//  only the emGeneTable.
cartSetString(cart, "hgt.trackNameFilter", trackName);
initTrackList(); // initialize trackList early if needed
struct track *returnTrackList = trackList;
cartRemove(cart, "hgt.trackNameFilter");
// restore
if (saveTrackNameFilter)
    {
    cartSetString(cart, "hgt.trackNameFilter", saveTrackNameFilter);
    }
trackList = saveTrackList;
groupList = saveGroupList;
return returnTrackList;
}

void doNextPrevItem(boolean goNext, char *trackName)
/* In case a next item arrow was clicked on a track, change */
/* position (i.e. winStart, winEnd, etc.) based on what track it was */
{
// create new trackList with just trackName
struct track *myTrackList = getTrackListForOneTrack(trackName);
struct track *track = trackFindByName(myTrackList, trackName);
if ((track != NULL) && (track->nextPrevItem != NULL))
    {
    // custom track big* tracks have pre-opened handle which we should not use
    // because that same bbiFile will get used later in the full track list
    track->bbiFile = NULL;
    track->nextPrevItem(track, goNext);
    }
}


void findBestEMGeneTable(struct track *myTrackList)
/* Find the best gene table to use for exonMostly and geneMostly. */
{
// TODO add support for choosing any gene table of type genePred, genePreExt, bigGenePred

// TODO add support for assembly hubs
if (trackHubDatabase(database)) // assembly hub? not supported yet
    return; // any table-name matches might just be coincidence in an assembly hub
            // although the hub_ prefix on the track name would help prevent name collisions.

char *orderedTables[] =
{"knownGene", "refGene", "ensGene",
 "flybaseGene", "sangerGene", "augustusGene", "genscan"};
int i, len;
for(i=0, len=ArraySize(orderedTables); i <len; ++i)
    {
    char *table = orderedTables[i];
    emGeneTrack = rFindTrackWithTable(table, myTrackList);
    if (emGeneTrack)
	{
	emGeneTable = table;
	cartSetString(cart, "emGeneTable", emGeneTable);
	break;
	}
    }
}

void setEMGeneTrack()
/* Find the track for the gene table to use for exonMostly and geneMostly. */
{
if (emGeneTable) // we already have it!
    return;
if (trackHubDatabase(database)) // assembly hub? not supported yet
    return;
emGeneTable = cloneString(cartOptionalString(cart, "emGeneTable"));
if (emGeneTable)
    {
    struct track *myTrackList = getTrackListForOneTrack(emGeneTable);
    emGeneTrack = rFindTrackWithTable(emGeneTable, myTrackList);
    }
if (!emGeneTable || !emGeneTrack)
    {
    cartRemove(cart, "emGeneTable");
    // It is preferable not to create a complete track list early on,
    //  but now we need one to find the best default emGeneTable and track.
    initTrackList();
    findBestEMGeneTable(trackList);
    }
}

boolean windowsHaveMultipleChroms()
/* Are there multiple different chromosomes in the windows list? */
{
struct window *window;
for (window=windows->next; window; window=window->next)
    {
    if (!sameString(window->chromName,windows->chromName))
	return TRUE;
    }
return FALSE;
}

static void setSharedLimitedVisAcrossWindows(struct track *track)
/* Look for lowest limitedVis across all windows
 * if found, set all windows to same lowest limited vis. */
{
enum trackVisibility sharedVis = 99;
struct track *tg;
for (tg=track; tg; tg=tg->nextWindow)
    {
    if (tg->limitedVisSet)
	{
	if (tg->limitedVis < sharedVis)
	    sharedVis = tg->limitedVis;
	}
    }
if (sharedVis != 99)
    {
    for (tg=track; tg; tg=tg->nextWindow)
	{
	tg->limitedVis = sharedVis;
	tg->limitedVisSet = TRUE;
	}
    }
}

static void setSharedErrorsAcrossWindows(struct track *track)
/* Look for network errors across all windows
 * if found, set all windows to same errMsg and set bigWarn track handlers. */
{
char *sharedErrMsg = NULL;
struct track *tg;
for (tg=track; tg; tg=tg->nextWindow)
    {
    if (!sharedErrMsg && tg->networkErrMsg)
	{
	sharedErrMsg = tg->networkErrMsg;
	break;
	}
    }
if (sharedErrMsg)
    {
    for (tg=track; tg; tg=tg->nextWindow)
	{
	tg->networkErrMsg = sharedErrMsg;
	tg->drawItems = bigDrawWarning;
	tg->totalHeight = bigWarnTotalHeight;
	}
    }
}


void outCollectionsToJson()
/* Output the current collections to the hgTracks JSON block. */
{
struct grp *groupList = NULL;
char buffer[4096];
safef(buffer, sizeof buffer, "%s-%s", customCompositeCartName, database);
char *hubFile = cartOptionalString(cart, buffer);

if (hubFile != NULL)
    {
    char *hubName = hubNameFromUrl(hubFile);
    struct trackDb *hubTdbs = hubCollectTracks( database,  &groupList);
    struct trackDb *tdb;
    struct jsonElement *jsonList = NULL;
    for(tdb = hubTdbs; tdb;  tdb = tdb->next)
        {
        if (sameString(tdb->grp, hubName))
            {
            if (jsonList == NULL)
                jsonList = newJsonList(NULL);

            struct jsonElement *collection = newJsonObject(newHash(4));
            jsonObjectAdd(collection, "track", newJsonString(tdb->track));
            jsonObjectAdd(collection, "shortLabel", newJsonString(tdb->shortLabel));
            jsonListAdd(jsonList, collection);
            }
        }
    if (jsonList != NULL)
        jsonObjectAdd(jsonForClient, "collections", jsonList);
    }
}

// TODO: move to a file in lib (cheapcgi?)
char *cgiVarStringSort(char *cgiVarString)
/* Return cgi var string sorted alphabetically by vars */
{
struct slName *vars = slNameListFromString(cgiVarString, '&');
slNameSort(&vars);
char *cgiString = slNameListToString(vars, '&');
return cgiString;
}

char *cgiTrackVisString(char *cgiVarString)
/* Filter cgi var string (var=val&) to just track visibilities, but equivalence
 * dense/pack/squish/full by replacing as 'on'.
 * Return string with track data vars in alphabetic order */
{
// TODO: use cheapcgi.cgiParsedVarsNew() to parse and get list ?
#define MAX_CGI_VARS 1000
// NOTE: Ana featured sessions have: 473, 308, 288
char *cgiVars[MAX_CGI_VARS];
int ct = chopByChar(cgiVarString, '&', cgiVars, sizeof cgiVars);

char *cgiVar = NULL;
char *val = NULL;
int i;
struct dyString *dsCgiTrackVis = dyStringCreate("db=%s", database);
for (i=0; i<ct; i++)
    {
    // TODO: attention to memory allocation
    cgiVar = cloneString(cgiVars[i]);
    val = rStringIn("=", cgiVar);
    if (!val)     // expect always
        continue;
    val++;
    if (sameString(val, "hide") || sameString(val, "dense") || sameString(val, "squish") ||
        sameString(val, "pack") || sameString(val, "full") || sameString(val, "show"))
        {
        char *p = val;
        *--p = 0;
        dyStringPrintf(dsCgiTrackVis, "&%s=%s", cgiVar, sameString(val, "hide") ? "hide" : "on");
        }
    else if (stringIn("_sel=", cgiVar) && sameString(val, "1"))
        {
    val -= 5;
    *val = 0;
        dyStringAppend(dsCgiTrackVis, "&");
        dyStringAppend(dsCgiTrackVis, cloneString(cgiVars[i]));
        }
    else
        {
        val = rStringIn("_imgOrd=", cgiVar);
        if (!val)
            val = rStringIn(".showCfg=", cgiVar);
        if (!val)
            continue;
        *val = 0;
        }
    }
return cgiVarStringSort(dyStringCannibalize(&dsCgiTrackVis));
}

// TODO: move to lib. Used by hgSession and hgTracks
static void outIfNotPresent(struct cart *cart, struct dyString *dy, char *track, int tdbVis)
/* Output default trackDb visibility if it's not mentioned in the cart. */
{
char *cartVis = cartOptionalString(cart, track);
if (cartVis == NULL)
    {
    if (dy)
        dyStringPrintf(dy,"&%s=%s", track, hStringFromTv(tdbVis));
    else
        printf("%s %s\n", track, hStringFromTv(tdbVis));
    }
}

// TODO: move to lib. Used by hgSession and hgTracks
static void outDefaultTracks(struct cart *cart, struct dyString *dy)
/* Output the default trackDb visibility for all tracks
 * in trackDb if the track is not mentioned in the cart. */
{
struct hash *parentHash = newHash(5);
struct track *track;
for (track=trackList; track; track=track->next)
    {
    struct trackDb *parent = track->tdb->parent;
    if (parent)
        {
        if (hashLookup(parentHash, parent->track) == NULL)
            {
            hashStore(parentHash, parent->track);
            if (parent->isShow)
                outIfNotPresent(cart, dy, parent->track, tvShow);
            }
        }
    if (track->tdb->visibility != tvHide)
        outIfNotPresent(cart, dy, track->tdb->track, track->tdb->visibility);
    }
// Put a variable in the cart that says we put the default 
// visibilities in it.
if (dy)
    dyStringPrintf(dy,"&%s=on", CART_HAS_DEFAULT_VISIBILITY);
else
    printf("%s on", CART_HAS_DEFAULT_VISIBILITY);
}

boolean hasSessionChanged()
{
/* Have any tracks been hidden or added ? */

// get featured session from database
char *sessionName = cartString(cart, "hgS_otherUserSessionName");
if (!sessionName)
    return FALSE;
struct sqlConnection *conn = hConnectCentral();
char query[1000];
sqlSafef(query, sizeof query,
        "SELECT contents FROM namedSessionDb where sessionName='%s'",
                replaceChars(sessionName, " ", "%20"));
char *cartString = sqlQuickNonemptyString(conn, query);
hDisconnectCentral(&conn);

// TODO: use cheapcgi.cgiParsedVarsNew() to parse and get list ?
#define MAX_SESSION_LEN 100000
char *curSessCart = (char *)needMem(MAX_SESSION_LEN);
cgiDecodeFull(cartString, curSessCart, MAX_SESSION_LEN);
char *curSessVisTracks = cgiTrackVisString(curSessCart);

// get track-related vars from current cart
struct dyString *dsCgiVars = dyStringNew(0);
cartEncodeState(cart, dsCgiVars);
outDefaultTracks(cart, dsCgiVars);
char *this = dyStringCannibalize(&dsCgiVars);
// TODO: again, better parsing
char *this2 = replaceChars(this, "%2D", "-");
char *thisSessVars = replaceChars(this2, "%2B", "+");
char *thisSessVisTracks = cgiTrackVisString(thisSessVars);

//freeMem(curSessCart);
boolean isSessChanged = FALSE;
if (differentString(curSessVisTracks, thisSessVisTracks))
    {
    isSessChanged = TRUE;
    }
return isSessChanged;
}

static void printMultiRegionButton()
/* Print button that launches multi-region configuration pop-up */
{
boolean isPressed = FALSE;
if (differentString(virtModeType, "default"))
    isPressed = TRUE;
char buf[256];
safef(buf, sizeof buf, "Configure %s multi-region display mode", 
                        isPressed ? "or exit" : "");
hButtonNoSubmitMaybePressed("hgTracksConfigMultiRegionPage", "Multi-region", buf,
            "popUpHgt.hgTracks('multi-region config'); return false;", isPressed);
}

static void printTrackDelIcon(struct track *track)
/* little track icon after track name. Github uses SVG elements for all icons, apparently that is faster */
/* we probably should have a library with all the icons, at least for the <svg> part */
{
    hPrintf("<div title='Delete this custom track' data-track='%s' class='trackDeleteIcon'><svg xmlns='http://www.w3.org/2000/svg' height='0.8em' viewBox='0 0 448 512'><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><path d='M135.2 17.7C140.6 6.8 151.7 0 163.8 0H284.2c12.1 0 23.2 6.8 28.6 17.7L320 32h96c17.7 0 32 14.3 32 32s-14.3 32-32 32H32C14.3 96 0 81.7 0 64S14.3 32 32 32h96l7.2-14.3zM32 128H416V448c0 35.3-28.7 64-64 64H96c-35.3 0-64-28.7-64-64V128zm96 64c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16zm96 0c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16zm96 0c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16z'/></svg></div>", track->track);

}

static void printTrackLink(struct track *track)
/* print a link hgTrackUi with shortLabel and various icons and mouseOvers */
{
if (sameOk(track->groupName, "user"))
    printTrackDelIcon(track);

if (track->hasUi)
    {
    char *url = trackUrl(track->track, chromName);
    char *longLabel = replaceChars(track->longLabel, "\"", "&quot;");

    struct dyString *dsMouseOver = dyStringCreate("%s", longLabel);
    struct trackDb *tdb = track->tdb;

    if (tdbIsSuper(tdb))
        dyStringPrintf(dsMouseOver, " - container, %d tracks ", slCount(tdb->children));
    else if (tdbIsComposite(tdb))
        dyStringPrintf(dsMouseOver, " - container, %d subtracks ", slCount(tdb->subtracks));

    // Print icons before the title when any are defined
    hPrintIcons(track->tdb);

    hPrintf("<A class='trackLink' HREF=\"%s\" data-group='%s' data-track='%s' title=\"%s\">", url, track->groupName, track->track, dyStringCannibalize(&dsMouseOver));

    freeMem(url);
    freeMem(longLabel);
    }

char *encodedShortLabel = htmlEncode(track->shortLabel);
hPrintf("%s", encodedShortLabel);
freeMem(encodedShortLabel);
if (track->hasUi)
    hPrintf("</A>");

hPrintf("<BR>");
}

static void printSearchHelpLink()
/* print the little search help link next to the go button */
{
char *url = cfgOptionDefault("searchHelpUrl","../goldenPath/help/query.html");
char *label = cfgOptionDefault("searchHelpLabel", "Examples");
if (!url || isEmpty(url))
    return;

printf("<div id='searchHelp'><a target=_blank title='Documentation on what you can enter into the Genome Browser search box' href='%s'>%s</a></div>", url, label);
}

static void printPatchNote()
{
    if (endsWith(chromName, "_fix") || endsWith(chromName, "_alt") || endsWith(chromName, "_hap"))
        {
        puts("<span id='patchNote'><svg xmlns='http://www.w3.org/2000/svg' height='1em' viewBox='0 0 512 512'><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><path d='M256 32c14.2 0 27.3 7.5 34.5 19.8l216 368c7.3 12.4 7.3 27.7 .2 40.1S486.3 480 472 480H40c-14.3 0-27.6-7.7-34.7-20.1s-7-27.8 .2-40.1l216-368C228.7 39.5 241.8 32 256 32zm0 128c-13.3 0-24 10.7-24 24V296c0 13.3 10.7 24 24 24s24-10.7 24-24V184c0-13.3-10.7-24-24-24zm32 224a32 32 0 1 0 -64 0 32 32 0 1 0 64 0z'/></svg>");
        puts("<a href='https://genome.ucsc.edu/FAQ/FAQreleases.html#patches' target=_blank>");
        //puts("<svg xmlns='http://www.w3.org/2000/svg' height='1em' viewBox='0 0 512 512'><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><path d='M464 256A208 208 0 1 0 48 256a208 208 0 1 0 416 0zM0 256a256 256 0 1 1 512 0A256 256 0 1 1 0 256zm169.8-90.7c7.9-22.3 29.1-37.3 52.8-37.3h58.3c34.9 0 63.1 28.3 63.1 63.1c0 22.6-12.1 43.5-31.7 54.8L280 264.4c-.2 13-10.9 23.6-24 23.6c-13.3 0-24-10.7-24-24V250.5c0-8.6 4.6-16.5 12.1-20.8l44.3-25.4c4.7-2.7 7.6-7.7 7.6-13.1c0-8.4-6.8-15.1-15.1-15.1H222.6c-3.4 0-6.4 2.1-7.5 5.3l-.4 1.2c-4.4 12.5-18.2 19-30.6 14.6s-19-18.2-14.6-30.6l.4-1.2zM224 352a32 32 0 1 1 64 0 32 32 0 1 1 -64 0z'/></svg>");
        puts("Patch sequence</a></span>");
        }
}

static void printDatabaseInfoHtml(char* database) 
/* print database-specific piece of HTML defined in hg.conf, works also with Genark hubs */
{
char *cfgPrefix = database;
if (trackHubDatabase(cfgPrefix))
    // hub IDs look like hub_1234_GCA_1232.2, so skip the hub_1234 part
    cfgPrefix = hubConnectSkipHubPrefix(cfgPrefix);
char *cfgName = catTwoStrings(cfgPrefix,"_html");
char *html = cfgOption(cfgName);
if (html)
    puts(html);
}

void printShortcutButtons(struct cart *cart, bool hasCustomTracks, bool revCmplDisp, bool multiRegionButtonTop)
/* Display bottom control panel. */
{
if (isSearchTracksSupported(database,cart))
    {
    cgiMakeButtonWithMsg(TRACK_SEARCH, TRACK_SEARCH_BUTTON,TRACK_SEARCH_HINT);
    }


hPrintf("&nbsp;");
// Not a submit button, because this is not a CGI function, it only calls Javascript function
hPrintf("<button id='highlightThis' title='Add a highlight that covers the entire region shown<br><i>Keyboard shortcut:</i> h, "
        "then m'>Highlight</button>");
jsInlineF("$('#highlightThis').on(\"click\", function(ev) { highlightCurrentPosition('add'); return false; } );");

hPrintf("&nbsp;");
hButtonWithMsg("hgt.hideAll", "Hide all","Hide all currently visible tracks - keyboard shortcut: h, then a");

hPrintf(" ");
hPrintf("<INPUT TYPE='button' id='ct_add' VALUE='%s' title='%s'>",
        hasCustomTracks ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL,
        hasCustomTracks ? "Manage your custom tracks - keyboard shortcut: c, then t" : "Add your own custom tracks - keyboard shortcut: c, then t");
jsOnEventById("click", "ct_add", "document.customTrackForm.submit(); return false;");

hPrintf(" ");
hButtonWithMsg("hgTracksConfigPage", "Configure","Configure image and track selection - keyboard shortcut: c, then f");
hPrintf(" ");

if (!multiRegionButtonTop)
    {
    printMultiRegionButton();
    hPrintf(" ");
    }
hButtonMaybePressed("hgt.toggleRevCmplDisp", "Reverse",
                       revCmplDisp ? "Show forward strand at this location - keyboard shortcut: r, then v"
                                   : "Show reverse strand at this location - keyboard shortcut: r, then v",
                       NULL, revCmplDisp);
hPrintf(" ");

hButtonWithOnClick("hgt.setWidth", "Resize", "Resize image width to browser window size - keyboard shortcut: r, then s", "hgTracksSetWidth()");

// put the track download interface behind hg.conf control
if (cfgOptionBooleanDefault("showDownloadUi", TRUE))
    jsInline("var showDownloadButton = true;\n");

// remove the hg.conf option once this feature is released
if (cfgOptionBooleanDefault("showIgv", FALSE))
    {
    puts(" <button id='hgtIgv' type='button' "
            "title='Add an IGV.js window below the UCSC Browser, to open files from "
            "your local harddisk or server' >Add IGV Tracks</button>");
    //jsInline("document.getElementById('hgtIgv').addEventListener('click', onIgvClick);");
    }

}

#ifdef NOTNOW
static void printAliases(char *name)
/* Print out the aliases for this sequence. */
{
struct slName *names = chromAliasFindAliases(name);

printf("<div id='aliases'><a title='");
for(;names; names = names->next)
    printf("%s;",names->name);
printf("'>Aliases</a></div>");
}
#endif


unsigned getParaLoadTimeout()
// get the parallel load timeout in seconds (defaults to 90)
{
char *paraLoadTimeoutStr = cartOptionalString(cart, "parallelFetch.timeout");
if (paraLoadTimeoutStr == NULL)
    paraLoadTimeoutStr = cfgOptionDefault("parallelFetch.timeout", "90");  // wait up to default 90 seconds.

unsigned paraLoadTimeout = sqlUnsigned(paraLoadTimeoutStr);

return paraLoadTimeout;
}

void doTrackForm(char *psOutput, struct tempName *ideoTn)
/* Make the tracks display form with the zoom/scroll buttons and the active
 * image.  If the ideoTn parameter is not NULL, it is filled in if the
 * ideogram is created.  */
{
#ifdef GRAPH_BUTTON_ON_QUICKLIFT
int graphCount = 0;
#endif
int disconCount = 0;
struct group *group;
struct track *track;
char *freezeName = NULL;
boolean hideAll = cgiVarExists("hgt.hideAll");
boolean hideTracks = cgiOptionalString( "hideTracks") != NULL;
boolean defaultTracks = cgiVarExists("hgt.reset");
boolean showedRuler = FALSE;
boolean showTrackControls = cartUsualBoolean(cart, "trackControlsOnMain", TRUE);
boolean multiRegionButtonTop = cfgOptionBooleanDefault(MULTI_REGION_CFG_BUTTON_TOP, TRUE);
long thisTime = 0, lastTime = 0;

basesPerPixel = ((float)virtWinBaseCount) / ((float)fullInsideWidth);
zoomedToBaseLevel = (virtWinBaseCount <= fullInsideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(virtWinBaseCount/3) * tl.mWidth) <= fullInsideWidth;
zoomedToCodonNumberLevel = (ceil(virtWinBaseCount/3) * tl.mWidth * 5) <= fullInsideWidth;
zoomedToCdsColorLevel = (virtWinBaseCount <= fullInsideWidth*3);


if (psOutput != NULL)
   {
   hPrintDisable();
   hideControls = TRUE;
   withNextItemArrows = FALSE;
   withNextExonArrows = FALSE;
   hgFindMatchesShowHighlight = FALSE;
   }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackHeaderForm\" id=\"TrackHeaderForm\" METHOD=\"GET\">\n\n", hgTracksName());
jsonObjectAdd(jsonForClient, "insideX", newJsonNumber(insideX)); 
jsonObjectAdd(jsonForClient, "revCmplDisp", newJsonBoolean(revCmplDisp));

if (hPrintStatus()) cartSaveSession(cart);

/* See if want to include sequence search results. */
userSeqString = cartOptionalString(cart, "ss");
if (userSeqString && !ssFilesExist(userSeqString))
    {
    userSeqString = NULL;
    cartRemove(cart, "ss");
    }
if (!hideControls)
    hideControls = cartUsualBoolean(cart, "hideControls", FALSE);

initTrackList();
//warn("slCount(trackList) after getTrackList: %d", slCount(trackList));
/* Tell tracks to load their items. */


// honor defaultImgOrder
if (cgiVarExists("hgt.defaultImgOrder"))
    {
    char wildCard[32];
    safef(wildCard,sizeof(wildCard),"*_%s",IMG_ORDER_VAR);
    cartRemoveLike(cart, wildCard);
    }

// Subtrack settings must be removed when composite/view settings are updated
parentChildCartCleanup(trackList,cart,oldVars);
if (measureTiming)
    measureTime("parentChildCartCleanup");


/* Honor hideAll and visAll variables */
if (hideAll || defaultTracks)
    {
    int vis = (hideAll ? tvHide : -1);
    changeTrackVis(groupList, NULL, vis);
    }

if(!psOutput && !cartUsualBoolean(cart, "hgt.imageV1", FALSE))
    {

   // re-establish the enlarged portal
   if (imgBoxPortalDimensions(theImgBox,&virtWinStart,&virtWinEnd,&(tl.picWidth),NULL,NULL,NULL,NULL,NULL))
        {
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth - gfxBorder - fullInsideX;
        }

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

    /* hide tracks not on any windows chromNames */
    boolean hideIt = TRUE;
    struct window *w;
    for (w = windows; w; w=w->next)
        {
        if (hTrackOnChrom(track->tdb, w->chromName))
            hideIt = FALSE;
        }
    if (hideIt)
        {
        track->limitedVis = tvHide;
        track->limitedVisSet = TRUE;
        }
    }

if (cartUsualBoolean(cart, "dumpTracks", FALSE))
    {
    struct dyString *dy = dyStringNew(1024);
    logTrackList(dy, trackList);

    printf("Content-type: text/html\n\n");
    printf("%s\n", dy->string);
    exit(0);
    }

if (sameString(cfgOptionDefault("trackLog", "off"), "on"))
    logTrackVisibilities(cartSessionId(cart), trackList, position);


/////////////////

// NEED TO LOAD ALL WINDOWS NOW
//
//   Need to load one window at a time!
//
//   The use of the global values for a window
//   means that differerent threads cannot use different global window values.
//   Threads must run on just one window value at a time.
//
//   Begin by making a copy of the track structure for visible tracks
//   for all windows.


// COPY TRACK STRUCTURES for other windows.

// TODO: due to an issue where some loading code is modifying the visibility
// of subtracks from hide to visible, I am forced to remove the optimization
// of cloning ONLY non-hidden tracks and subtracks.  If the offending code
// can be identified and moved into a step proceding the track cloning,
// then we can return to that optimization.
//	if (track->visibility != tvHide)
//		if (subtrack->visibility != tvHide)

windows->trackList = trackList;  // save current track list in window
struct window *window;
for (window=windows; window->next; window=window->next)
    {
    struct track *newTrackList = NULL;
    for (track = trackList; track != NULL; track = track->next)
	{
        isCompositeInAggregate(track); // allow track to recognize its true self
	track->nextWindow = NULL;
	//if (track->visibility != tvHide)  // Unable to use this optimization at present
	    {
	    struct track *copy;
	    AllocVar(copy);
	    memmove(copy,track,sizeof(struct track));
	    copy->next = NULL;
	    copy->bbiFile = NULL;  // bigDataUrl custom tracks have already been opened, will re-open for other windows.
	    copy->nextWindow = NULL;
	    copy->prevWindow = track;
	    slAddHead(&newTrackList, copy);
	    track->nextWindow = copy;

	    // copy subtracks.
	    copy->subtracks = NULL;
	    struct track *subtrack;
	    for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
		{
		//if (subtrack->visibility != tvHide)  // Unable to use this optimization at present
		    {
		    struct track *subcopy;
		    AllocVar(subcopy);
		    memmove(subcopy,subtrack,sizeof(struct track));
		    subcopy->next = NULL;
		    subcopy->bbiFile = NULL;  // bigDataUrl custom tracks have already been opened, will re-open for other windows.
		    subcopy->nextWindow = NULL;
		    subcopy->prevWindow = subtrack;
		    slAddHead(&copy->subtracks, subcopy);
		    subtrack->nextWindow = subcopy;
		    }
		}
	    slReverse(&copy->subtracks);
	    }
	}
    slReverse(&newTrackList);
    trackList = newTrackList;
    window->next->trackList = trackList;  // save new track list in window
    }
trackList = windows->trackList;  // restore original track list


// Loop over each window loading all tracks
trackLoadingInProgress = TRUE;

int doLoadLoop;
boolean doLoadSummary = FALSE;

for (doLoadLoop=0; doLoadLoop < 2; ++doLoadLoop)
    {

    for (window=windows; window; window=window->next)
	{
	trackList = window->trackList;  // set track list
	setGlobalsFromWindow(window);

	/* pre-load remote tracks in parallel */
	int ptMax = atoi(cfgOptionDefault("parallelFetch.threads", "20"));  // default number of threads for parallel fetch.
	int pfdListCount = 0;
	pthread_t *threads = NULL;
	if (ptMax > 0)     // parallelFetch.threads=0 to disable parallel fetch
	    {
	    findLeavesForParallelLoad(trackList, &pfdList, doLoadSummary);
	    pfdListCount = slCount(pfdList);

	    /* launch parallel threads */
	    ptMax = min(ptMax, pfdListCount);
	    if (ptMax > 0)
		{
		AllocArray(threads, ptMax);
		/* Create threads */
		int pt;
		for (pt = 0; pt < ptMax; ++pt)
		    {
		    int rc = pthread_create(&threads[pt], NULL, remoteParallelLoad, NULL);
		    if (rc)
			{
			errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
			}
		    pthread_detach(threads[pt]);  // this thread will never join back with it's progenitor
			// Canceled threads that might leave locks behind,
			// so the theads are detached and will be neither joined nor canceled.
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

		    checkHideEmptySubtracks(track);

		    checkIfWiggling(cart, track);

		    if (doLoadSummary)
			{
			if (track->loadSummary)
			    {
			    if (!track->subtracks)
				{
				char *quickLiftFile = trackDbSetting(track->tdb, "quickLiftUrl");
				if (!quickLiftFile)
				    {
				    if (!startsWith("bigLolly", track->tdb->type))
					{
					track->loadSummary(track);
					}
				    }
				}
			    }
			struct track *subtrack;
			for (subtrack=track->subtracks; subtrack; subtrack=subtrack->next)
			    {
			    if (tdbVisLimitedByAncestors(cart,subtrack->tdb,TRUE,TRUE) != tvHide)
				{
				if (!subtrack->parallelLoading)
				    {
				    char *quickLiftFile = trackDbSetting(subtrack->tdb, "quickLiftUrl");
				    if (!quickLiftFile)
					{
					if (!startsWith("bigLolly", subtrack->tdb->type))
					    {
					    if (subtrack->loadSummary)
						subtrack->loadSummary(subtrack);
					    }
					}
				    }
				}
			    }
			}
		    else
			{
			track->loadItems(track);
			if (tdbHasDecorators(track))
			    {
			    loadDecorators(track);
			    decoratorMethods(track);
			    }
			}

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
	    remoteParallelLoadWait(getParaLoadTimeout());  // wait up to default 90 seconds.
	    if (measureTiming)
		measureTime("Waiting for parallel (%d threads for %d tracks) remote data fetch", ptMax, pfdListCount);
	    }
	}

    doLoadSummary = TRUE;
    }

trackLoadingInProgress = FALSE;

setGlobalsFromWindow(windows); // first window // restore globals
trackList = windows->trackList;  // restore track list

// Some loadItems() calls will have already set limitedVis.
// Look for lowest limitedVis across all windows
// if found, set all windows to same lowest limitedVis
for (track = trackList; track != NULL; track = track->next)
    {
    setSharedLimitedVisAcrossWindows(track);
    struct track *sub;
    for (sub=track->subtracks; sub; sub=sub->next)
	{
	setSharedLimitedVisAcrossWindows(sub);
	}
    }

// Look for network errors across all windows
// if found, set all windows to same errMsg and set bigWarn track handlers.
for (track = trackList; track != NULL; track = track->next)
    {
    setSharedErrorsAcrossWindows(track);
    struct track *sub;
    for (sub=track->subtracks; sub; sub=sub->next)
	{
	setSharedErrorsAcrossWindows(sub);
	}
    }



//////////////  END OF MULTI-WINDOW LOOP


printTrackInitJavascript(trackList);

/* Generate two lists of hidden variables for track group visibility.  Kludgy,
   but required b/c we have two different navigation forms on this page, but
   we want open/close changes in the bottom form to be submitted even if the user
   submits via the top form. */
struct dyString *trackGroupsHidden1 = dyStringNew(1000);
struct dyString *trackGroupsHidden2 = dyStringNew(1000);
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->trackList != NULL)
        {
        int looper;
        for (looper=1;looper<=2;looper++)
            {
            boolean isOpen = !isCollapsedGroup(group);
            char buf[1000];
            safef(buf, sizeof(buf), "<input type='hidden' name=\"%s\" id=\"%s_%d\" value=\"%s\">\n",
		collapseGroupVar(group->name), collapseGroupVar(group->name), looper, isOpen ? "0" : "1");
            dyStringAppend(looper == 1 ? trackGroupsHidden1 : trackGroupsHidden2, buf);
            }
        }
    }

if (theImgBox)
    {
    // If a portal was established, then set the global dimensions back to the portal size
    if (imgBoxPortalDimensions(theImgBox,NULL,NULL,NULL,NULL,&virtWinStart,&virtWinEnd,&(tl.picWidth),NULL))
        {
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth-gfxBorder-fullInsideX;
        }
    }
/* Center everything from now on. */
hPrintf("<CENTER>\n");

outCollectionsToJson();

jsonObjectAdd(jsonForClient, "winStart", newJsonNumber(virtWinStart));
jsonObjectAdd(jsonForClient, "winEnd", newJsonNumber(virtWinEnd));
jsonObjectAdd(jsonForClient, "chromName", newJsonString(virtChromName));

// Tell javascript about multiple windows info
if (virtMode)
    {
    // pre windows
    long preVWinStart = virtWinStart - virtWinBaseCount;
    if (preVWinStart < 0)
	preVWinStart = 0;
    long preVWinEnd = virtWinStart;
    struct window *preWindows = makeWindowListFromVirtChrom(preVWinStart, preVWinEnd);
    struct jsonElement *jsonForList = newJsonList(NULL);
    for(window=preWindows;window;window=window->next)
	{
	struct jsonElement *jsonForWindow = NULL;
	jsonForWindow = newJsonObject(newHash(8));
	jsonObjectAdd(jsonForWindow, "chromName",   newJsonString(window->chromName));
	jsonObjectAdd(jsonForWindow, "winStart",    newJsonNumber(window->winStart));
	jsonObjectAdd(jsonForWindow, "winEnd",      newJsonNumber(window->winEnd));
	jsonObjectAdd(jsonForWindow, "insideX",     newJsonNumber(window->insideX));
	jsonObjectAdd(jsonForWindow, "insideWidth", newJsonNumber(window->insideWidth));
	jsonObjectAdd(jsonForWindow, "virtStart",   newJsonNumber(window->virtStart));
	jsonObjectAdd(jsonForWindow, "virtEnd",     newJsonNumber(window->virtEnd));
	jsonListAdd(jsonForList, jsonForWindow);
	}
    slReverse(&jsonForList->val.jeList);
    jsonObjectAdd(jsonForClient, "windowsBefore", jsonForList);

    jsonForList = newJsonList(NULL);
    for(window=windows;window;window=window->next)
	{
	struct jsonElement *jsonForWindow = NULL;
	jsonForWindow = newJsonObject(newHash(8));
	jsonObjectAdd(jsonForWindow, "chromName",   newJsonString(window->chromName));
	jsonObjectAdd(jsonForWindow, "winStart",    newJsonNumber(window->winStart));
	jsonObjectAdd(jsonForWindow, "winEnd",      newJsonNumber(window->winEnd));
	jsonObjectAdd(jsonForWindow, "insideX",     newJsonNumber(window->insideX));
	jsonObjectAdd(jsonForWindow, "insideWidth", newJsonNumber(window->insideWidth));
	jsonObjectAdd(jsonForWindow, "virtStart",   newJsonNumber(window->virtStart));
	jsonObjectAdd(jsonForWindow, "virtEnd",     newJsonNumber(window->virtEnd));
	jsonListAdd(jsonForList, jsonForWindow);
	}
    slReverse(&jsonForList->val.jeList);
    jsonObjectAdd(jsonForClient, "windows", jsonForList);

    // post windows
    long postVWinStart = virtWinEnd;
    long postVWinEnd = virtWinEnd + virtWinBaseCount;
    if (postVWinEnd > virtSeqBaseCount)
	postVWinEnd = virtSeqBaseCount;
    struct window *postWindows = makeWindowListFromVirtChrom(postVWinStart, postVWinEnd);
    jsonForList = newJsonList(NULL);
    for(window=postWindows;window;window=window->next)
	{
	struct jsonElement *jsonForWindow = NULL;
	jsonForWindow = newJsonObject(newHash(8));
	jsonObjectAdd(jsonForWindow, "chromName",   newJsonString(window->chromName));
	jsonObjectAdd(jsonForWindow, "winStart",    newJsonNumber(window->winStart));
	jsonObjectAdd(jsonForWindow, "winEnd",      newJsonNumber(window->winEnd));
	jsonObjectAdd(jsonForWindow, "insideX",     newJsonNumber(window->insideX));
	jsonObjectAdd(jsonForWindow, "insideWidth", newJsonNumber(window->insideWidth));
	jsonObjectAdd(jsonForWindow, "virtStart",   newJsonNumber(window->virtStart));
	jsonObjectAdd(jsonForWindow, "virtEnd",     newJsonNumber(window->virtEnd));
	jsonListAdd(jsonForList, jsonForWindow);
	}
    slReverse(&jsonForList->val.jeList);
    jsonObjectAdd(jsonForClient, "windowsAfter", jsonForList);

    jsonForList = newJsonList(NULL);
    // also store js nonVirtPosition
    jsonObjectAdd(jsonForClient, "nonVirtPosition", newJsonString(cartString(cart, "nonVirtPosition")));
    jsonObjectAdd(jsonForClient, "virtChromChanged", newJsonBoolean(virtChromChanged));
    jsonObjectAdd(jsonForClient, "virtualSingleChrom", newJsonBoolean(virtualSingleChrom())); // DISGUISE POS
    jsonObjectAdd(jsonForClient, "virtModeType", newJsonString(virtModeType));
    }

char dbPosKey[256];
safef(dbPosKey, sizeof(dbPosKey), "position.%s", database);
jsonObjectAdd(jsonForClient, "lastDbPos", newJsonString(cartString(cart, dbPosKey)));

// hide chromIdeo
if ((trackImgOnly && !ideogramToo)
|| (sameString(virtModeType, "customUrl") && windowsHaveMultipleChroms()) // Special case hide by request
)
    {
    for(window=windows;window;window=window->next)
	{
	struct track *ideoTrack = chromIdeoTrack(window->trackList);
	if (ideoTrack)
	    {
	    ideoTrack->limitedVisSet = TRUE;
	    ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
	    }
	}
    }

if (trackImgOnly && !ideogramToo)
    {
    // right-click to change viz 
    makeActiveImage(trackList, psOutput);
    fflush(stdout);
    return;  // bail out b/c we are done
    }

if (!hideControls)
    {
    /* set white-space to nowrap to prevent buttons from wrapping when screen is
     * narrow */
    hPrintf("<DIV STYLE=\"white-space:nowrap;\">\n");
    printMenuBar();
    //menuBarAppendExtTools();

    /* Show title */
    freezeName = hFreezeFromDb(database);
    if(freezeName == NULL)
        freezeName = "Unknown";
    hPrintf("<span id='assemblyName' style='font-size:large;'><B>");

    // for these assemblies, we do not display the year, to save space and reduce clutter
    // Their names must include a "(" character
    char* noYearDbs[] = { "hg19", "hg38", "mm39", "mm10" };

    if ( stringArrayIx(database, noYearDbs, ArraySize(noYearDbs)) != -1 )
        {
        // freezeName is e.g. "Feb. 2009 (GRCh37/hg19)"
        char *afterParen = skipBeyondDelimit(freezeName, '(');
        afterParen--; // move back one char
        hPrintf("%s %s on %s %s", organization, browserName, organism, afterParen);
        }
    else if (startsWith("zoo",database) )
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
		hPrintf("%s %s on %s %s",
			organization, browserName, organism, freezeName);
	    else
		hPrintf("%s %s on %s %s (%s)",
			organization, browserName, trackHubSkipHubName(organism), freezeName, trackHubSkipHubName(database));
	    }
	}
    hPrintf("</B></SPAN>");

    //hPrintf("<span target=_blank title='Show details about this assembly' id='gatewayLink'>"
            //"<a href='hgGateway?hgsid=%s'>Assembly Info</a></span>", cartSessionId(cart));

    printDatabaseInfoHtml(database);

    // Disable recommended track set panel when changing tracks, session, database
    char *sessionLabel = cartOptionalString(cart, hgsOtherUserSessionLabel);
    char *oldDb = hashFindVal(oldVars, "db");
    if (sessionLabel)
        {
        if (defaultTracks || hideAll || hideTracks ||
            (oldDb && differentString(database, oldDb)) ||
            !hasRecTrackSet(cart) ||
            sameString(sessionLabel, "off"))
                cartRemove(cart, hgsOtherUserSessionLabel);
        }
    sessionLabel = cartOptionalString(cart, hgsOtherUserSessionLabel);
    if (sessionLabel)
        {
        char *panel = "recTrackSetsPanel";
        boolean isSessChanged = FALSE;
        if (recTrackSetsChangeDetectEnabled())
            isSessChanged = hasSessionChanged();
        struct dyString *hoverText = dyStringNew(0);
        dyStringPrintf(hoverText, "Your browser is displaying the %s track set%s. "
                                " Click to change to another.", sessionLabel,
                                isSessChanged ? 
                                ", with changes (added or removed tracks) you have requested" : "");
        // TODO: cleanup layout tweaking for FF on IE10
        hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");
        hPrintf("<span id='spacer' style='display: inline; padding-left: 10px;' >&nbsp;</span>");

        hPrintf("<span id='%s' class='gbSessionLabelPanel' style='display: inline-block;' title='%s'>",
                        panel, dyStringCannibalize(&hoverText));
        hPrintf("<span id='recTrackSetLabel' class='gbSessionLabelText gbSessionChangeIndicator %s' "
                        "style='margin-right: 3px;'>%s</span>",
                        isSessChanged ? "gbSessionChanged" : "", sessionLabel);
        hPrintf("<i id='removeSessionPanel' title='Close' class='fa fa-remove' "
                        "style='color: #a9a9a9; font-size:smaller; vertical-align: super;'></i>");
        hPrintf("</span>");

        jsOnEventById("click", "recTrackSetLabel", "showRecTrackSetsPopup(); return false;");
        jsOnEventById("click", "removeSessionPanel", "removeSessionPanel(); return false;");
        }
    hPrintf("<BR>\n");

    /* This is a clear submit button that browsers will use by default when enter is pressed in position box. */
    hPrintf("<INPUT TYPE=IMAGE BORDER=0 NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\">");
    /* Put up scroll and zoom controls. */
#ifndef USE_NAVIGATION_LINKS
    hWrites("Move ");
    hButtonWithOnClick("hgt.left3", "<<<", "Move 95% to the left",
                       "return imageV2.navigateButtonClick(this);");
    hButtonWithOnClick("hgt.left2", " <<", "Move 47.5% to the left",
                       "return imageV2.navigateButtonClick(this);");
    hButtonWithOnClick("hgt.left1", " < ", "Move 10% to the left",
                       "return imageV2.navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right1", " > ", "Move 10% to the right",
                       "return imageV2.navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right2", ">> ", "Move 47.5% to the right",
                       "return imageV2.navigateButtonClick(this);");
    hButtonWithOnClick("hgt.right3", ">>>", "Move 95% to the right",
                       "return imageV2.navigateButtonClick(this);");
    hWrites(" Zoom in ");
    /* use button maker that determines padding, so we can share constants */
    topButton("hgt.in1", ZOOM_1PT5X);
    topButton("hgt.in2", ZOOM_3X);
    topButton("hgt.in3", ZOOM_10X);
    topButton("hgt.inBase", ZOOM_BASE);
    hWrites(" Zoom out ");
    topButton("hgt.out1", ZOOM_1PT5X);
    topButton("hgt.out2", ZOOM_3X);
    topButton("hgt.out3", ZOOM_10X);
    topButton("hgt.out4", ZOOM_100X);
    hWrites("<div style='height:0.3em;'></div>\n");
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
        // This 'dirty' field is used to check if js/ajax changes to the page have occurred.
        // If so and it is reached by the back button, a page reload will occur instead.
	char buf[256];
	if (virtualSingleChrom()) // DISGUISE VMODE
	    safef(buf, sizeof buf, "%s", windowsSpanPosition());
	else
	    safef(buf, sizeof buf, "%s:%ld-%ld", virtChromName, virtWinStart+1, virtWinEnd);
        hPrintf("<INPUT TYPE='text' style='display:none;' name='dirty' id='dirty' VALUE='false'>\n");
        hPrintf("<INPUT TYPE=HIDDEN id='positionHidden' name='position' "
                "VALUE=\"%s\">", buf);
        hPrintf("\n%s", trackGroupsHidden1->string);
        hPrintf("</CENTER></FORM>\n");
        hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" id=\"TrackForm\" METHOD=\"POST\">\n\n",
                hgTracksName());
	    hPrintf("%s", trackGroupsHidden2->string);
	    dyStringFree(&trackGroupsHidden1);
	    dyStringFree(&trackGroupsHidden2);
	if (!psOutput) cartSaveSession(cart);   /* Put up hgsid= as hidden variable. */
	hPrintf("<CENTER>");
	}


    /* Make line that says position. */
	{
	char buf[256];
        char *javascript = "document.location = '/cgi-bin/hgTracks?db=' + document.TrackForm.db.options[document.TrackForm.db.selectedIndex].value;";
        if (containsStringNoCase(database, "zoo"))
            {
            hPuts("Organism ");
            printAssemblyListHtmlExtra(database, "change", javascript);
            }

        // multi-region button on position line, initially under hg.conf control
        if (multiRegionButtonTop)
            {
            printMultiRegionButton();
            hPrintf(" ");
            }

	if (virtualSingleChrom()) // DISGUISE VMODE
	    safef(buf, sizeof buf, "%s", windowsSpanPosition());
	else
	    safef(buf, sizeof buf, "%s:%ld-%ld", virtChromName, virtWinStart+1, virtWinEnd);
	
	position = cloneString(buf);

        // position box
        char *pressedClass = "";
        char *showVirtRegions = "";
        if (differentString(virtModeType, "default"))
            {
            pressedClass = "pressed";
            showVirtRegions = "show multi-region position ranges and ";
            }
	hPrintf("<span class='positionDisplay %s' id='positionDisplay' "
                "title='click to %s copy chromosome range to input box'>%s</span>", 
                        pressedClass, showVirtRegions, addCommasToPos(database, position));
	hPrintf("<input type='hidden' name='position' id='position' value='%s'>\n", buf);
	sprintLongWithCommas(buf, virtWinEnd - virtWinStart);
	hPrintf(" <span id='size'>%s</span> bp. ", buf);
	hPrintf("<input class='positionInput' type='text' name='hgt.positionInput' id='positionInput'"
                        " size='%d'>\n", multiRegionButtonTop ? 51 : 61);
	hWrites(" ");
	hButton("goButton", "Search");

        printSearchHelpLink();
        // printAliases(displayChromName);

        printPatchNote();

	if (!trackHubDatabase(database))
	    {
            jsonObjectAdd(jsonForClient, "assemblySupportsGeneSuggest", newJsonBoolean(assemblySupportsGeneSuggest(database)));
            if (assemblySupportsGeneSuggest(database))
                hPrintf("<input type='hidden' name='hgt.suggestTrack' id='suggestTrack' value='%s'>\n", assemblyGeneSuggestTrack(database));
	    }

        // hg.conf controlled links

        // database-specific link: 2 hg.conf settings, format <db>_TopLink{Label}
        struct slName *dbLinks = cfgNamesWithPrefix(database);

        struct slName *link;
        char *dbTopLink = NULL, *dbTopLinkLabel = NULL;
        for (link = dbLinks; link != NULL; link = link->next)
            {
            char *name = cloneString(link->name);
            char *setting = chopPrefixAt(link->name, '_');
            if (sameString(setting, "TopLink"))
                dbTopLink = cfgOption(name);
            else if (sameString(setting, "TopLinkLabel"))
                dbTopLinkLabel = cfgOption(name);
            }
        if (dbTopLink && dbTopLinkLabel)
            {
            hPrintf("&nbsp;&nbsp;<a href='%s' target='_blank'><em><b>%s</em></b></a>\n",
                dbTopLink, dbTopLinkLabel);
            }
        
        // generic link
	char *survey = cfgOptionEnv("HGDB_SURVEY", "survey");
	char *surveyLabel = cfgOptionEnv("HGDB_SURVEY_LABEL", "surveyLabel");
	if (survey && differentWord(survey, "off"))
            hPrintf("&nbsp;&nbsp;<span style='background-color:yellow;'>"
                    "<A HREF='%s' TARGET=_BLANK><EM><B>%s</EM></B></A></span>\n",
                    survey, surveyLabel ? surveyLabel : "Take survey");

        // a piece of HTML, can be a link or anything else
	char *hgTracksNoteHtml = cfgOption("hgTracksNoteHtml");
	if (hgTracksNoteHtml)
            puts(hgTracksNoteHtml);

	hPutc('\n');
	}
    }

// TODO GALT  how to handle ideos?
boolean nukeIdeoFromList = FALSE;
for(window=windows;window;window=window->next)
    {
    setGlobalsFromWindow(window);

    if (window == windows) // first window
	{	
	/* Make chromosome ideogram gif and map. */
	nukeIdeoFromList = makeChromIdeoImage(&trackList, psOutput, ideoTn);
	window->trackList = trackList;  // the variable may have been updated.
	// TODO make this not just be centered over the entire image,
	// but rather centered over the individual chromosome.
	// notice that it modifies trackList, and visibility settings potentially need parallelization for windows
	}
    else
	{
	// TODO should be more than this. But at least this makes the same trackList mods to the other windows.
	if (nukeIdeoFromList)
	    {
	    struct track *ideoTrack = chromIdeoTrack(window->trackList);
	    if (ideoTrack)
		{
		slRemoveEl(&window->trackList, ideoTrack);
		}
	    }
	}

    }
setGlobalsFromWindow(windows); // first window // restore globals

/* DBG - a message box to display information from the javascript
hPrintf("<div id='mouseDbg'><span id='dbgMouseOver'><p>. . . dbgMouseOver</p></span></div>\n");
 */

#ifdef USE_NAVIGATION_LINKS
hPrintf("<TABLE BORDER=0 CELLPADDING=0 width='%d'><tr style='font-size:small;'>\n",
        tl.picWidth);//min(tl.picWidth, 800));
hPrintf("<td width='40' align='left'><a href='?hgt.left3=1' "
        "title='move 95&#37; to the left'>&lt;&lt;&lt;</a>\n");
hPrintf("<td width='30' align='left'><a href='?hgt.left2=1' "
        "title='move 47.5&#37; to the left'>&lt;&lt;</a>\n");
hPrintf("<td width='20' align='left'><a href='?hgt.left1=1' "
        "title='move 10&#37; to the left'>&lt;</a>\n");

hPrintf("<td>&nbsp;</td>\n"); // Without width cell expands table with, forcing others to sides
hPrintf("<td width='40' align='left'><a href='?hgt.in1=1' "
        "title='zoom in 1.5x'>&gt;&nbsp;&lt;</a>\n");
hPrintf("<td width='60' align='left'><a href='?hgt.in2=1' "
        "title='zoom in 3x'>&gt;&gt;&nbsp;&lt;&lt;</a>\n");
hPrintf("<td width='80' align='left'><a href='?hgt.in3=1' "
        "title='zoom in 10x'>&gt;&gt;&gt;&nbsp;&lt;&lt;&lt;</a>\n");
hPrintf("<td width='40' align='left'><a href='?hgt.inBase=1' "
        "title='zoom in to base range'>&gt;<i>base</i>&lt;</a>\n");

hPrintf("<td>&nbsp;</td>\n"); // Without width cell expands table with, forcing others to sides
hPrintf("<td width='40' align='right'><a href='?hgt.out1=1' "
        "title='zoom out 1.5x'>&lt;&nbsp;&gt;</a>\n");
hPrintf("<td width='60' align='right'><a href='?hgt.out2=1' "
        "title='zoom out 3x'>&lt;&lt;&nbsp;&gt;&gt;</a>\n");
hPrintf("<td width='80' align='right'><a href='?hgt.out3=1' "
        "title='zoom out 10x'>&lt;&lt;&lt;&nbsp;&gt;&gt;&gt;</a>\n");
hPrintf("<td width='80' align='right'><a href='?hgt.out4=1' "
        "title='zoom out 100x'>&lt;&lt;&lt;&nbsp;&gt;&gt;&gt;</a>\n");
hPrintf("<td>&nbsp;</td>\n"); // Without width cell expands table width, forcing others to sides
hPrintf("<td width='20' align='right'><a href='?hgt.right1=1' "
        "title='move 10&#37; to the right'>&gt;</a>\n");

hPrintf("<td width='30' align='right'><a href='?hgt.right2=1' "
        "title='move 47.5&#37; to the right'>&gt;&gt;</a>\n");
hPrintf("<td width='40' align='right'><a href='?hgt.right3=1' """
        "title='move 95&#37; to the right'>&gt;&gt;&gt;</a>\n");
hPrintf("</tr></table>\n");
#endif///def USE_NAVIGATION_LINKS


/* Make clickable image and map. */
makeActiveImage(trackList, psOutput);
fflush(stdout);

if (trackImgOnly)
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
    if (cartUsualBoolean(cart, "showDinkButtons", FALSE))
        {
        hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1 WIDTH=%d COLS=%d><TR>\n",
                tl.picWidth, 27);

        hPrintf("<TD COLSPAN=6 ALIGN=left NOWRAP>");
        hPrintf("<span class='moveButtonText'>Move start</span><br>");
        hButtonWithOnClick("hgt.dinkLL", " < ", "Move start position to the left",
                           "return imageV2.navigateButtonClick(this);");
        hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
        hButtonWithOnClick("hgt.dinkLR", " > ", "Move start position to the right",
                           "return imageV2.navigateButtonClick(this);");
        hPrintf("</TD>");
        hPrintf("<td width='30'>&nbsp;</td>\n");

        hPrintf("<TD COLSPAN=6 ALIGN=right NOWRAP>");
        hPrintf("<span class='moveButtonText'>Move end</span><br>");
        hButtonWithOnClick("hgt.dinkRL", " < ", "Move end position to the left",
                                  "return imageV2.navigateButtonClick(this);");
        hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
        hButtonWithOnClick("hgt.dinkRR", " > ", "Move end position to the right",
                                  "return imageV2.navigateButtonClick(this);");
        hPrintf("</TD>");
        hPrintf("</TR></TABLE>\n");
        }

    if( chromosomeColorsMade )
        {
        hPrintf("<B>Chromosome Color Key:</B><BR> ");
        hPrintf("<IMG SRC = \"../images/new_colorchrom.gif\" BORDER=1 WIDTH=596 HEIGHT=18 ><BR>\n");
        }
    if (doPliColors)
        {
        hPrintf("<div id='gnomadColorKeyLegend'>");
        hPrintf("<B>gnomAD Loss-of-Function Constraint (LOEUF) Color Key:</B><BR> ");
        hPrintf("<table style=\"border: 1px solid black\"><tr>\n");
        hPrintf("<td style=\"background-color:rgb(244,0,2)\">&lt; 0.1</td>\n");
        hPrintf("<td style=\"background-color:rgb(240,74,3)\">&lt; 0.2</td>\n");
        hPrintf("<td style=\"background-color:rgb(233,127,5)\">&lt; 0.3</td>\n");
        hPrintf("<td style=\"background-color:rgb(224,165,8)\">&lt; 0.4</td>\n");
        hPrintf("<td style=\"background-color:rgb(210,191,13)\">&lt; 0.5</td>\n");
        hPrintf("<td style=\"background-color:rgb(191,210,22)\">&lt; 0.6</td>\n");
        hPrintf("<td style=\"background-color:rgb(165,224,26)\">&lt; 0.7</td>\n");
        hPrintf("<td style=\"background-color:rgb(127,233,58)\">&lt; 0.8</td>\n");
        hPrintf("<td style=\"background-color:rgb(74,240,94)\">&lt; 0.9</td>\n");
        hPrintf("<td style=\"background-color:rgb(0,244,153)\">&ge; 0.9</td>\n");
        hPrintf("<td style=\"color: white; background-color:rgb(160,160,160)\">No LOEUF score</td>\n");
        hPrintf("</tr></table>\n");
        hPrintf("</div>"); // gnomadColorKeyLegend
        }

    if (showTrackControls)
	{
	/* Display viewing options for each track. */
        /* Chuck: This is going to be wrapped in a table so that
         * the controls don't wrap around randomly */
        hPrintf("<table id='trackCtrlTable' border=0 cellspacing=1 cellpadding=1>\n");

        // since this is all a huge table (which it shouldn't be), the only way to add whitespace between two rows is to add an empty row
        // since padding and margin are not allowed on table rows. (One day, we will remove this table)
        hPrintf("<tr style='height:5px'><td></td></tr>\n");

        hPrintf("<tr><td align='left'>\n");

        hButtonWithOnClick("hgt.collapseGroups", "Collapse all", "Collapse all track groups",
                           "return vis.expandAllGroups(false)");
        hPrintf("</td>");

        hPrintf("<td colspan='%d' class='controlButtons' align='CENTER' nowrap>\n", MAX_CONTROL_COLUMNS - 2);

        printShortcutButtons(cart, hasCustomTracks, revCmplDisp, multiRegionButtonTop);
        hPrintf("</td>\n");

        hPrintf("<td align='right'>");
        hButtonWithOnClick("hgt.expandGroups", "Expand all", "Expand all track groups",
                           "return vis.expandAllGroups(true)");
        hPrintf("</td></tr>");

        cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
        struct hash *superHash = hashNew(8);
	for (group = groupList; group != NULL; group = group->next)
	    {
	    if ((group->trackList == NULL) && (group->errMessage == NULL))
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
            if (group->errMessage)
                hPrintf("<th align=\"left\" colspan=%d class='errorToggleBar'>",MAX_CONTROL_COLUMNS);
            else if (startsWith("Hub", group->label))
                hPrintf("<th align=\"left\" colspan=%d class='hubToggleBar'>",MAX_CONTROL_COLUMNS);
            else if (startsWith("QuickLift", group->label))
                hPrintf("<th align=\"left\" colspan=%d class='quickToggleBar'>",MAX_CONTROL_COLUMNS);
            else
                hPrintf("<th align=\"left\" colspan=%d class='nativeToggleBar'>",MAX_CONTROL_COLUMNS);
            hPrintf("<table style='width:100%%;'><tr><td style='text-align:left;'>");
            hPrintf("\n<A NAME=\"%sGroup\"></A>",group->name);

	    char idText[256];
	    safef(idText, sizeof idText, "%s_button", group->name);
            hPrintf("<IMG class='toggleButton'"
                    " id='%s' src=\"%s\" alt=\"%s\" title='%s this group'>&nbsp;&nbsp;",
                    idText, indicatorImg, indicator,isOpen?"Collapse":"Expand");
	    jsOnEventByIdF("click", idText, "return vis.toggleForGroup(this, '%s');", group->name);

            if (isHubTrack(group->name))
		{

                if (strstr(group->label, "Collections"))
                    {
                    safef(idText, sizeof idText, "%s_edit", group->name);
                    hPrintf("<input name=\"hubEditButton\" id='%s'"
                        " type=\"button\" value=\"edit\">\n", idText);
                    jsOnEventByIdF("click", idText,
                        "document.editHubForm.submit();return true;");
                    }
                }

            hPrintf("</td><td style='text-align:center; width:90%%;'>\n<B>%s</B>", group->label);
            
            char *hubName = hubNameFromGroupName(group->name);
            struct trackHub *hub = grabHashedHub(hubName);
            if (hub && hub->url)
                {
                puts("&nbsp;");
                char infoText[10000];
                if (startsWith("QuickLift", group->label))
                    safef(infoText, sizeof infoText, "This is a quickLift track group. It contains tracks that are annotation on %s that have been lifted up to this assembly, as well as a track (Alignment Differences) showing mismatches and indels between %s and this assembly.", hub->defaultDb, hub->defaultDb);
                else
                    safef(infoText, sizeof infoText, "A track hub is a list of tracks produced and hosted by external data providers. The UCSC browser group is not responsible for them. This hub is loaded from %s", hub->url);
                printInfoIconColor(infoText, "white");
                }

            hPrintf("</td><td style='text-align:right;'>\n");
            
            if (hubName)
		{
                if (cfgOptionBooleanDefault("groupDropdown", FALSE) && hub && hub->genomeList && hub->genomeList->next)
                    {
                    puts("<span style='font-size:13px'>Genomes: </span><select style='width:7em' name='db'>");
                    for (struct trackHubGenome *thg = hub->genomeList; thg != NULL; thg = thg->next)
                        {
                        if (!sameWord(thg->name, database))
                            printf("<option value='%s'>%s</option>\n", thg->name, thg->name);
                        }
                    puts("</select>");
                    }

                // visibility: hidden means that the element takes up space so the center alignment is not disturbed.
                if ((hub != NULL) && !startsWith("QuickLift", group->label))
                    {
                    if (hub->descriptionUrl == NULL)
                        {
                        hPrintf("<a title='The track hub authors have not provided a descriptionUrl with background "
                                "information about this track hub. ");
                        if (hub->email)
                            hPrintf("The authors can be reached at %s. ", hub->email);
                        hPrintf("This link leads to our documentation page about the descriptionUrl statement in hub.txt. ");
                        hPrintf("' href='../goldenPath/help/hgTrackHubHelp.html#hub.txt' "
                                "style='color:#FFF; font-size: 13px;' target=_blank>No Info</a>");
                        }
                    else
                        {
                        hPrintf("<a title='Link to documentation about this track hub, provided by the track hub authors (not UCSC). ");
                        if (hub->email)
                            hPrintf("The authors can be reached at %s", hub->email);
                        hPrintf("' href='%s' "
                            "style='color:#FFF; font-size: 13px;' target=_blank>Info</a>", hub->descriptionUrl);
                        }
                    hPrintf("&nbsp;&nbsp;");


                    }
                }

            hPrintf("<button type='button' class=\"hgtButtonHideGroup\" data-group-name=\"%s\" "
                    "title='Hide all tracks in this group'>Hide group</button>&nbsp;",
                    group->name);

            if (hub)
                {
		safef(idText, sizeof idText, "%s_%d_disconn", hubName, disconCount);
                disconCount++;
                hPrintf("<input name=\"hubDisconnectButton\" title='Disconnect third-party track hub and remove all tracks' "
                        "id='%s'"
                    " type=\"button\" value=\"Disconnect\">\n", idText);
		jsOnEventByIdF("click", idText,
                    "document.disconnectHubForm.elements['hubId'].value='%s';"
                    "document.disconnectHubForm.submit();return true;",
		    hubName + strlen(hubTrackPrefix));

#ifdef GRAPH_BUTTON_ON_QUICKLIFT
		safef(idText, sizeof idText, "%s_%d_graph", hubName, graphCount);
                graphCount++;
                hPrintf("<input name=\"graphButton\" id='%s'"
                    " type=\"button\" value=\"Graph\">\n", idText);
#endif
		}

            hPrintf("<input type='submit' name='hgt.refresh' value='Refresh' "
                    "title='Update image with your changes. Any of the refresh buttons on this page may be used.'>\n");
            hPrintf("</td></tr></table></th>\n");
            controlGridEndRow(cg);

            /* Base Position track goes into map group, which will always exist. */
            if (!showedRuler && sameString(group->name, "map") )
		{
		char *url = trackUrl(RULER_TRACK_NAME, chromName);
		showedRuler = TRUE;
		myControlGridStartCell(cg, isOpen, group->name, FALSE);
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

	    /* Add supertracks to track list, sort by priority and
	     * determine if they have visible member tracks */
	    groupTrackListAddSuper(cart, group, superHash);

	    /* Display track controls */
            if (group->errMessage)
                {
		myControlGridStartCell(cg, isOpen, group->name,
                                       shouldBreakAll(group->errMessage));
                hPrintf("%s", group->errMessage);
		controlGridEndCell(cg);
                }

	    for (tr = group->trackList; tr != NULL; tr = tr->next)
		{
		struct track *track = tr->track;
		if (tdbIsSuperTrackChild(track->tdb))
		    /* don't display supertrack members */
		    continue;
		myControlGridStartCell(cg, isOpen, group->name,
                                       shouldBreakAll(track->shortLabel));

                printTrackLink(track);

		if (hTrackOnChrom(track->tdb, chromName))
		    {
		    if (tdbIsSuper(track->tdb))
			superTrackDropDown(cart, track->tdb,
					    superTrackHasVisibleMembers(track->tdb));
		    else
                        {
                        /* check for option of limiting visibility to one mode */
                        hTvDropDownClassVisOnly(track->track, track->visibility,
                                                rTdbTreeCanPack(track->tdb),
                                                (track->visibility == tvHide) ? "hiddenText"
                                                                              : "normalText",
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
        hashFree(&superHash);
	endControlGrid(&cg);

        jsOnEventBySelector(".hgtButtonHideGroup", "click", "onHideAllGroupButtonClick(event)");
	}

    if (measureTiming)
        printTrackTiming();

    hPrintf("</DIV>\n");
    }
if (showTrackControls)
    hButton("hgt.refresh", "Refresh");

if (sameString(database, "wuhCor1"))
    {
    puts("<p class='centeredCol'>\n"
         "For information about this browser and related resources, see "
         "<a target='blank' href='../covid19.html'>COVID-19 Research at UCSC</a>.</p>");
    // GISAID wants this displayed on any page that shows any GISAID data
    puts("<p class='centeredCol'>\n"
         "GISAID data displayed in the Genome Browser are subject to GISAID's\n"
         "<a href='https://www.gisaid.org/registration/terms-of-use/' "
         "target=_blank>Terms and Conditions</a>.\n"
         "SARS-CoV-2 genome sequences and metadata are available for download from\n"
         "<a href='https://gisaid.org' target=_blank>GISAID</a> EpiCoV&trade;.\n"
         "</p>");
    }

// add hidden link as a trap for web spiders, for log analysis one day to get an idea what the spider IPs 
// are
hPrintf("<a href='/notExist.html' style='display:none'>Invisible link</a>");

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

// TODO GALT cleanup sibs too? probably can do for window copies but low priority.
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

/* hidden form for composite builder CGI */
hPrintf("<FORM ACTION='%s' NAME='editHubForm'>", hgCollectionName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

/* hidden form for track hub CGI */
hPrintf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

// this is the form for the disconnect hub button
hPrintf("<FORM ACTION=\"%s\" NAME=\"disconnectHubForm\">\n",  "../cgi-bin/hgTracks");
cgiMakeHiddenVar("hubId", "");
cgiMakeHiddenVar(hgHubDoDisconnect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
cartSaveSession(cart);
puts("</FORM>");

// put the track download interface behind hg.conf control
if (cfgOptionBooleanDefault("showMouseovers", FALSE))
    jsInline("var showMouseovers = true;\n");

// if the configure page allows hgc popups tell the javascript about it
if (cfgOptionBooleanDefault("canDoHgcInPopUp", FALSE) && cartUsualBoolean(cart, "doHgcInPopUp", TRUE))
    jsInline("var doHgcInPopUp = true;\n");

if (cfgOptionBooleanDefault("greyBarIcons", FALSE))
    jsInline("var greyBarIcons = true;\n");

// TODO GALT nothing to do here.
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

void zoomToSize(long newSize)
/* Zoom so that center stays in same place,
 * but window is new size.  If necessary move
 * center a little bit to keep it from going past
 * edges. */
{
long center = ((long long)virtWinStart + (long long)virtWinEnd)/2;
if (center < 0)
    errAbort("zoomToSize: error computing center: %ld = (%ld + %ld)/2\n",
             center, virtWinStart, virtWinEnd);
if (newSize > virtSeqBaseCount)
    newSize = virtSeqBaseCount;
virtWinStart = center - newSize/2;
virtWinEnd = virtWinStart + newSize;
if (virtWinStart <= 0)
    {
    virtWinStart = 0;
    virtWinEnd = newSize;
    }
else if (virtWinEnd > virtSeqBaseCount)
    {
    virtWinEnd = virtSeqBaseCount;
    virtWinStart = virtWinEnd - newSize;
    }
virtWinBaseCount = virtWinEnd - virtWinStart;
}

void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
double newSizeDbl = (virtWinBaseCount*amount + 0.5);
long newSize;
if (newSizeDbl > virtSeqBaseCount)
    newSize = virtSeqBaseCount;
else if (newSizeDbl < 1.0)
    newSize = 1;
else
    newSize = (long)newSizeDbl;
zoomToSize(newSize);
}

void zoomToBaseLevel()
/* Set things so that it's zoomed to base level. */
{
zoomToSize(fullInsideWidth/tl.mWidth);
if (rulerMode == tvHide)
    cartSetString(cart, "ruler", "dense");
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
long offset;
long newStart, newEnd;
if (revCmplDisp)
    amount = -amount;
offset = (long)(amount * virtWinBaseCount);
/* Make sure don't scroll of ends. */
newStart = virtWinStart + offset;
newEnd = virtWinEnd + offset;
if (newStart < 0)
    offset = -virtWinStart;
else if (newEnd > virtSeqBaseCount)
    offset = virtSeqBaseCount - virtWinEnd;

// at high zoom levels, offset can be very small: make sure that we scroll at least one bp
if (amount > 0 && offset==0)
    offset = 1;
if (amount < 0 && offset==0)
    offset = -1;

/* Move window. */
virtWinStart += offset;
virtWinEnd += offset;
}

void dinkWindow(boolean start, long dinkAmount)
/* Move one end or other of window a little. */
{
if (revCmplDisp)
    {
    start = !start;
    dinkAmount = -dinkAmount;
    }
if (start)
    {
    virtWinStart += dinkAmount;
    if (virtWinStart < 0)
	virtWinStart = 0;
    }
else
    {
    virtWinEnd += dinkAmount;
    if (virtWinEnd > virtSeqBaseCount)
       virtWinEnd = virtSeqBaseCount;
    }
}

long dinkSize(char *var)
/* Return size to dink. */
{
char *stringVal = cartOptionalString(cart, var);
double x;
int fullInsideX = trackOffsetX(); /* The global versions of these are not yet set */
int fullInsideWidth = tl.picWidth-gfxBorder-fullInsideX;
double guideBases = (double)guidelineSpacing * (double)(virtWinEnd - virtWinStart)
                    / ((double)fullInsideWidth);

if (stringVal == NULL || !isdigit(stringVal[0]))
    {
    stringVal = "1";
    cartSetString(cart, var, stringVal);
    }
x = atof(stringVal);
long ret = round(x*guideBases);

return (ret == 0) ? 1 : ret;
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn, ideoPsTn;
char *pdfFile = NULL, *ideoPdfFile = NULL;
ZeroVar(&ideoPsTn);
trashDirFile(&psTn, "hgt", "hgt", ".eps");

if(!trackImgOnly)
    {
    printMenuBar();

    printf("<div style=\"margin: 10px\">\n");
    printf("<H1>PDF Output</H1>\n");
    printf("PDF images can be printed with Acrobat Reader "
           "and edited by many drawing programs such as Adobe "
           "Illustrator or Inkscape.<BR>");
    }
doTrackForm(psTn.forCgi, &ideoPsTn);

pdfFile = convertEpsToPdf(psTn.forCgi);
if (strlen(ideoPsTn.forCgi))
    ideoPdfFile = convertEpsToPdf(ideoPsTn.forCgi);
if (pdfFile != NULL)
    {
    printf("<UL style=\"margin-top:5px;\">\n");
    printf("<LI>Download <A TARGET=_blank HREF=\"%s\">"
       "the current browser graphic in PDF</A>\n", pdfFile);
    if (ideoPdfFile != NULL)
        printf("<LI>Download <A TARGET=_blank HREF=\"%s\">"
               "the current chromosome ideogram in PDF</A>\n", ideoPdfFile);
    printf("</UL>\n");
    freez(&pdfFile);
    freez(&ideoPdfFile);

    printf("If you require a bitmap image in <b>PNG</b> or <b>TIFF format</b>, note that PDF format \n");
    printf("is vector-based and can be opened in any software that opens PDF/SVG vector files. You can use these to edit the screenshot.\n");
    printf("Many publishers also accept PDF or SVG files for figures. <br><br>\n");
    printf("If your publisher requires bitmap images such as PNG or TIFF, use your vector graphics program to export to these \n");
    printf("formats. For this export, use the publishers recommended dpi (dots per inch value), usually 300 dpi.\n");

    // see redmine #1077
    printf("<div style=\"margin-top:15px\">Tips for producing quality images for publication:</div>\n");
    printf("<UL style=\"margin-top:0px\">\n");
    printf("<LI>Add assembly name and chromosome range to the image on the\n"
        "<A HREF=\"hgTrackUi?g=ruler\">configuration page of the base position track</A>.\n");
    printf("<LI>If using the default genes track (e.g. GENCODE for hg38 or UCSC Genes for older assemblies),\n"
           "consider showing only one transcript per gene by turning off splice variants on the track configuration page.\n");
    printf("<LI>Increase the font size and remove the light blue vertical guidelines in the \n"
        "<A HREF=\"hgTracks?hgTracksConfigPage=configure\">image configuration menu</A>.");
    printf("<LI>Change the size of the image in the image configuration menu\n"
            "to make it look more square.\n");
    printf("</UL>\n");
    printf("</div>\n");


    }
else
    printf("<BR><BR>PDF format not available");

printf("<a href='%s?%s=%s'><input type='button' VALUE='Return to Browser'></a>\n",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart));
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
    {
    withNextItemArrows = cartUsualBoolean(cart, "nextItemArrows", FALSE);
    withNextExonArrows = cartUsualBoolean(cart, "nextExonArrows", TRUE);
    }
withExonNumbers = cartUsualBoolean(cart, "exonNumbers", TRUE);
emAltHighlight = cartUsualBoolean(cart, "emAltHighlight", FALSE);
revCmplDisp = cartUsualBooleanDb(cart, database, REV_CMPL_DISP, FALSE);
emPadding = cartUsualInt(cart, "emPadding", emPadding);
gmPadding = cartUsualInt(cart, "gmPadding", gmPadding);
withPriorityOverride = cartUsualBoolean(cart, configPriorityOverride, FALSE);
fullInsideX = trackOffsetX();
fullInsideWidth = tl.picWidth-gfxBorder-fullInsideX;
}

static boolean resolvePosition(char **pPosition)
/* Position may be an already-resolved chr:start-end, or a search term.
 * If it is a search term:
 * 1 match ==> set globals chromName, winStart, winEnd, return TRUE.
 * 0 matches ==> switch back to lastPosition, hopefully get 1 match from that;
 * set globals chromName, winStart, winEnd, return TRUE.  If no lastPosition, try w/hDefaultPos().
 * multiple matches ==> Display a page with links to match positions, return FALSE. */
{
boolean resolved = TRUE;
struct dyString *dyWarn = dyStringNew(0);
boolean noShort = (cartOptionalString(cart, "noShort") != NULL);
hgp = hgFindSearch(cart, pPosition, &chromName, &winStart, &winEnd, hgTracksName(), dyWarn, NULL);
displayChromName = chromAliasGetDisplayChrom(database, cart, chromName);
if (isNotEmpty(dyWarn->string))
    {
    if (!noShort) // we're not on the second pass of the search
        warn("%s", dyWarn->string);
    }

if (!noShort && hgp->singlePos)
    {
    createHgFindMatchHash();
    }
else 
    {
    char *menuStr = menuBar(cart, database);
    if (menuStr)
        puts(menuStr);
    hgPositionsHtml(database, hgp, hgTracksName(), cart);
    resolved = FALSE;
    }
cartSetString(cart, "position", *pPosition);
return resolved;
}

void parseVirtPosition(char *position)
/* parse virtual position
 *  TODO this is just temporary */
{
if (!position)
    {
    errAbort("position NULL");
    }
char *vPos = cloneString(position);
stripChar(vPos, ',');
char *colon = strchr(vPos, ':');
if (!colon)
    errAbort("position has no colon");
char *dash = strchr(vPos, '-');
if (!dash)
    errAbort("position has no dash");
*colon = 0;
*dash = 0;
virtWinStart = atol(colon+1) - 1;
virtWinEnd = atol(dash+1);
}

void parseNonVirtPosition(char *position)
/* parse non-virtual position */
{
if (!position)
    {
    errAbort("position NULL");
    }
char *vPos = cloneString(position);
stripChar(vPos, ',');
char *colon = strchr(vPos, ':');
if (!colon)
    errAbort("position has no colon");
char *dash = strchr(vPos, '-');
if (!dash)
    errAbort("position has no dash");
*colon = 0;
*dash = 0;
chromName = cloneString(vPos);
displayChromName = chromAliasGetDisplayChrom(database, cart, chromName);
winStart = atol(colon+1) - 1;
winEnd = atol(dash+1);
}

boolean findNearestVirtMatch(char *chrom, int start, int end, boolean findNearest, long *retVirtStart, long *retVirtEnd)
/* find nearest match on virt chrom.
 * findNearest flag means of no direct hits found, take the closest miss. */
{
// search for one or more overlapping windows
struct positionMatch *mList = virtChromSearchForPosition(chrom, start, end, findNearest);

// sort positions by virtPos (will be sorted by chrom, start, end)
matchSortOnVPos(&mList);

// merge contiguous matches spanning multiple touching windows
mList = matchMergeContiguousVPos(mList);

// TODO search for the best match in pList
// TODO this is crude, needs to fix, just finds the largest match:
struct positionMatch *p, *best = NULL;
long bigSpan = 0;
for (p=mList; p; p=p->next)
    {
    long span = p->virtEnd - p->virtStart;
    if (span > bigSpan)
	{
	bigSpan = span;
	best = p;
	}
    }
if (best) // TODO do something better
    {
    // return the new location
    *retVirtStart = best->virtStart;
    *retVirtEnd   = best->virtEnd;
    }
else
    {
    return FALSE;
    }
return TRUE;
}

void remapHighlightPos()
// Remap non-virt highlight position if any to new virtMode chrom.
{
if (virtualSingleChrom())
    return;
struct highlightVar *h = parseHighlightInfo();
if (h && h->db && sameString(h->db, database))
    {
    long virtStart = 0, virtEnd = 0;
    if (findNearestVirtMatch(h->chrom, h->chromStart, h->chromEnd, FALSE, &virtStart, &virtEnd)) // try to find the nearest match
	{
	// save new highlight position to cart var
	char cartVar[1024];
	safef(cartVar, sizeof cartVar, "%s.%s:%ld-%ld#%s", h->db, MULTI_REGION_VIRTUAL_CHROM_NAME, 
                virtStart, virtEnd, h->hexColor);
	cartSetString(cart, "highlight", cartVar);
	}
    else
	{
	// erase the highlight cartvar if it has no overlap with the new virt chrom
	cartRemove(cart, "highlight");
	}
    }
}

static void setupTimeWarning()
/* add javascript that outputs a warning message if page takes too long to load */
{
char *maxTimeStr = cfgOption("warnSeconds");
if (!maxTimeStr)
    return;

double maxTime = atof(maxTimeStr);
struct dyString *dy = dyStringNew(150);

// The idea of the timer below is that if the page takes a ton of time to load, the message will come up while the page still builds
// However, due to gzip compression of the page and also most of the time spent in the CGI itself, the timer only starts
// when most of the work has been completed. So the timer is only useful if what takes long is our own hgTracks javascript
dyStringPrintf(dy, "var warnTimingTimer = setTimeout( function() { hgtWarnTiming(%f)}, %f);\n", maxTime, maxTime*1000);
// In most cases, since the timer does not work well in practice, the following will always work: 
// once the page is ready, we check how long it took to load.
dyStringPrintf(dy, "$(document).ready( function() { clearTimeout(warnTimingTimer); hgtWarnTiming(%f)});\n", maxTime);

jsInline(dy->string);
dyStringFree(&dy);
}


void tracksDisplay()
/* Put up main tracks display. This routine handles zooming and
 * scrolling. */
{
char titleVar[256];
char *oldPosition = cartUsualString(cart, "oldPosition", "");
boolean findNearest = cartUsualBoolean(cart, "findNearest", FALSE);
cartRemove(cart, "findNearest");

boolean positionIsVirt = FALSE;
position = getPositionFromCustomTracks();
if (NULL == position)
    {
    position = cartGetPosition(cart, database, &lastDbPosCart);
    if (sameOk(cgiOptionalString("position"), "lastDbPos"))
	{
        restoreSavedVirtPosition();
	}
    if (startsWith(OLD_MULTI_REGION_CHROM, position))
        position = replaceChars(position, OLD_MULTI_REGION_CHROM, MULTI_REGION_CHROM);
    if (startsWith(MULTI_REGION_CHROM, position))
	{
	position = stripCommas(position); // sometimes the position string arrives with commas in it.
	positionIsVirt = TRUE;
	}
    }

if (sameString(position, ""))
    {
    hUserAbort("Please go back and enter a coordinate range or a search term in the \"search term\" field.<br>For example: chr22:20100000-20200000.\n");
    }

if (!positionIsVirt)
    {
    if (! resolvePosition(&position))
        return;
    }

virtMode = cartUsualBoolean(cart, "virtMode", FALSE);

/* Figure out basic dimensions of display.  This
 * needs to be done early for the sake of the
 * zooming and dinking routines. */
setLayoutGlobals();

virtModeType = cartUsualString(cart, "virtModeType", virtModeType);

if (positionIsVirt && virtualSingleChrom())
    {
    // we need chromName to be set before initRegionList() gets called.
    position = cartUsualString(cart, "nonVirtPosition", "");
    if (!sameString(position,""))
	parseNonVirtPosition(position);
    }

// TODO GALT do we need to add in other types that now depend on emGeneTable too? maybe singleTrans?
//   OR maybe this code should just be part of initRegionList()
if (sameString(virtModeType, "exonMostly") || sameString(virtModeType, "geneMostly"))
    {
    setEMGeneTrack();
    if (!emGeneTable) // there is no available gene table, undo exonMostly or geneMostly
	{
	//warn("setEMGeneTrack unable to find default gene track");
	virtModeType = "default";
	cartSetString(cart, "virtModeType", virtModeType);
	}
    }

lastVirtModeType = cartUsualString(cart, "lastVirtModeType", lastVirtModeType);

while(TRUE)
    {
    if (sameString(virtModeType, "default") && !(sameString(lastVirtModeType, "default")))
	{ // RETURNING TO DEFAULT virtModeType
	virtModeType = "default";
	cartSetString(cart, "virtModeType", virtModeType);
	findNearest = TRUE;
	if (positionIsVirt)
	    position = cartUsualString(cart, "nonVirtPosition", "");
	char *nvh = cartUsualString(cart, "nonVirtHighlight", NULL);
	if (nvh)
	    cartSetString(cart, "highlight", nvh);
	if (!sameString(position,""))
	    parseNonVirtPosition(position);
	}

    if (initRegionList())   // initialize the region list, sets virtModeExtraState
	{
	break;
	}
    else
	{ // virt mode failed, forced to return to default
	virtModeType = "default";
	cartSetString(cart, "virtModeType", virtModeType);
	position = cloneString(hDefaultPos(database));
        resolvePosition(&position);
	positionIsVirt=FALSE;
	virtMode=FALSE;
	}
    }

// PAD padding of exon regions is now being done inside the fetch/merge.
//if (emPadding > 0)
    //padVirtRegions(emPadding); // this old routine does not handle multiple chroms yet

//testRegionList(); // check if it is ascending non-overlapping regions. (this is not the case with custom user-defined-regions)

makeVirtChrom();

//testVirtChromBinarySearch();

// ajax callback to convert chrom position to virt chrom position
if (cartVarExists(cart, "hgt.convertChromToVirtChrom"))
    {
    position = cartString(cart, "hgt.convertChromToVirtChrom");
    char nvh[256];
    safef(nvh, sizeof nvh, "%s.%s", database, position);
    cartSetString(cart, "nonVirtHighlight", nvh);
    parseNonVirtPosition(position);
    if (findNearestVirtMatch(chromName, winStart, winEnd, FALSE, &virtWinStart, &virtWinEnd))
	{
	struct jsonElement *jsonForConvert = NULL;
	jsonForConvert = newJsonObject(newHash(8));
	jsonObjectAdd(jsonForConvert, "virtWinStart", newJsonNumber(virtWinStart));
	jsonObjectAdd(jsonForConvert, "virtWinEnd", newJsonNumber(virtWinEnd));

	struct dyString *dy = dyStringNew(1024);
	jsonDyStringPrint(dy, (struct jsonElement *) jsonForConvert, "convertChromToVirtChrom", 0);
	jsInline(dy->string);
	dyStringFree(&dy);
	}
    return;
    }

lastVirtModeExtraState = cartUsualString(cart, "lastVirtModeExtraState", lastVirtModeExtraState);

// DISGUISED POSITION
if (startsWith(OLD_MULTI_REGION_CHROM, position))
    position = replaceChars(position, OLD_MULTI_REGION_CHROM, MULTI_REGION_CHROM);
if (!startsWith(MULTI_REGION_CHROM, position) && (virtualSingleChrom()))
    {
    // "virtualSingleChrom trying to find best vchrom location corresponding to chromName, winStart, winEnd
    findNearest = TRUE;

     // try to find the nearest match
    if (!(chromName && findNearestVirtMatch(chromName, winStart, winEnd, findNearest, &virtWinStart, &virtWinEnd)))
	{ // create 10k window near middle of vchrom
	warn("Your new regions are not near previous location. Using middle of new coordinates.");
	virtWinStart = virtSeqBaseCount / 2;
	virtWinEnd = virtWinStart + 10000;
	if (virtWinEnd > virtSeqBaseCount)
	    virtWinEnd = virtSeqBaseCount;
	}
    virtMode = TRUE;
    }

// when changing modes (or state like padding), first try to revert to plain non-virt position
if (!sameString(virtModeType, "default")
 && !sameString(lastVirtModeType, "default")
 && !(sameString(virtModeType, lastVirtModeType) && sameString(virtModeExtraState, lastVirtModeExtraState)))
    { // CHANGE FROM ONE NON-DEFAULT virtMode to another.
    virtChromChanged = TRUE;    // virtChrom changed
    lastVirtModeType = "default";
    cartSetString(cart, "lastVirtModeType", lastVirtModeType); // I think I do not need this
    lastVirtModeExtraState = "";
    findNearest = TRUE;
    position = cartUsualString(cart, "nonVirtPosition", "");
    if (!sameString(position,""))
	parseNonVirtPosition(position);
    char *nvh = cartUsualString(cart, "nonVirtHighlight", "");
    if (!sameString(nvh, "")) // REMOVE? not needed probably
	{
	cartSetString(cart, "highlight", nvh);
	}
    }

// virt mode has not changed
if (sameString(virtModeType, lastVirtModeType)
 && sameString(virtModeExtraState, lastVirtModeExtraState))
    {
    if (virtMode)
	{
	if (positionIsVirt)
	    {
	    parseVirtPosition(position);
	    }
	else
	    {	
	    // Is this a new position to navigate to
	    // or just an old inherited position.
	    position = stripCommas(position);  // sometimes the position string arrives with commas in it.
	    if (!sameString(position, oldPosition))
		{
		if (!findNearestVirtMatch(chromName, winStart, winEnd, findNearest, &virtWinStart, &virtWinEnd))
		    {
		    // errAbort has kind of harsh behavior, and does not work well with ajax anyways
		    warn("Location not found in Multi-Region View. "
		    "To return to default view at that location, "
		    "click <a href=%s?%s=%s&position=%s:%d-%d&virtModeType=default>here</a>.\n"
		    , hgTracksName(), cartSessionVarName(), cartSessionId(cart), chromName, winStart+1, winEnd);
		    // try to resume using oldPosition
		    parseVirtPosition(oldPosition);
		    }
		}
	    }
	}
    else
	{
	if (positionIsVirt)
	    errAbort("positionIsVirt=%d but virtMode=%d", positionIsVirt, virtMode);
	}


    }
else
    {

    if (sameString(virtModeType,"default"))  // we are leaving virtMode
	{
	virtMode = FALSE;
        cartRemove(cart, "virtShortDesc");
	}
    else
	{

	// ENTERING VIRTMODE

	// First time initialization

	findNearest = TRUE;

	// For now, do this manually here:
	// sets window to full genome size, which for these demos should be small except for allChroms
	if (sameString(virtModeType, "exonMostly") || 
            sameString(virtModeType, "geneMostly") || 
            sameString(virtModeType, "kcGenes") ||
            (sameString(virtModeType, "customUrl") && 
                    !cartUsualBoolean(cart, MULTI_REGION_BED_WIN_FULL, FALSE)))
	    {
	    // trying to find best vchrom location corresponding to chromName, winStart, winEnd);
	    // try to find the nearest match
	    if (!(chromName && findNearestVirtMatch(chromName, winStart, winEnd, findNearest, &virtWinStart, &virtWinEnd)))
		{ // create 10k window near middle of vchrom
		warn("Your new regions are not near previous location. Using middle of new coordinates.");
		virtWinStart = virtSeqBaseCount / 2;
		virtWinEnd = virtWinStart + 10000;
		if (virtWinEnd > virtSeqBaseCount)
		    virtWinEnd = virtSeqBaseCount;
		}
	    virtMode = TRUE;
	    }
	else if (sameString(virtModeType, "singleAltHaplo"))
	    {
	    virtWinStart = defaultVirtWinStart;
	    virtWinEnd = defaultVirtWinEnd;
	    virtMode = TRUE;
	    }
	else if (!sameString(virtModeType, "default"))
	    { // try to set view to entire vchrom
	    virtWinStart = 0;
	    virtWinEnd = virtSeqBaseCount;
	    virtMode = TRUE;
	    // TODO what if the full-vchrom view has "too many windows"
	    // check if virtRegionCount > 4000?
	    }

	remapHighlightPos();

	}

    }

if (virtMode)
    virtChromName = MULTI_REGION_CHROM;
else
    virtChromName = displayChromName;

virtWinBaseCount = virtWinEnd - virtWinStart;


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
else if (cgiVarExists("hgt.out4"))
    zoomAroundCenter(100.0);
else if (cgiVarExists("hgt.dinkLL"))
    dinkWindow(TRUE, -dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkLR"))
    dinkWindow(TRUE, dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkRL"))
    dinkWindow(FALSE, -dinkSize("dinkR"));
else if (cgiVarExists("hgt.dinkRR"))
    dinkWindow(FALSE, dinkSize("dinkR"));

/* Before loading items, deal with the next/prev item arrow buttons if pressed. */
if (cgiVarExists("hgt.nextItem"))
    doNextPrevItem(TRUE, cgiUsualString("hgt.nextItem", NULL));
else if (cgiVarExists("hgt.prevItem"))
    doNextPrevItem(FALSE, cgiUsualString("hgt.prevItem", NULL));


/* Clip chromosomal position to fit. */
if (virtWinEnd < virtWinStart)
    {
    // swap start and end (user entered coordinates backwards)
    long temp = virtWinEnd;
    virtWinEnd = virtWinStart;
    virtWinStart = temp;
    }
else if (virtWinStart == virtWinEnd)
    {
    // Size 0 window
    virtWinStart -= 1;
    virtWinEnd += 1;
    }

if (virtWinStart < 0)
    {
    virtWinStart = 0;
    }

if (virtWinEnd > virtSeqBaseCount)
    {
    virtWinEnd = virtSeqBaseCount;
    }

if (virtWinStart > virtSeqBaseCount)
    {
    virtWinStart = virtSeqBaseCount - 1000;
    }

virtWinBaseCount = virtWinEnd - virtWinStart;
if (virtWinBaseCount <= 0)
    hUserAbort("Window out of range on %s", virtChromName);


if (!cartUsualBoolean(cart, "hgt.psOutput", FALSE)
 && !cartUsualBoolean(cart, "hgt.imageV1" , FALSE))
    {

    // TODO GALT Guidelines broken on virtChrom for 3X.
    //  works in demo0 or real chrom. Only the guidelines seem to be messed up.
    //  Other stuff works. 1X works too.
    // Since we are not using 3X for now, I will leave this for a future fix.
    // To test 3X, do make clean; make CFLAGS=-DIMAGEv2_DRAG_SCROLL_SZ=3

    // Start an imagebox (global for now to avoid huge rewrite of hgTracks)
    // Set up imgBox dimensions
    int sideSliceWidth  = 0;   // Just being explicit
    if (withLeftLabels)
        sideSliceWidth   = (fullInsideX - gfxBorder*3) + 2;

    //  for the 3X expansion effect to work, this needs to happen BEFORE we create the windows list
    //    in makeWindowListFromVirtChrom()
    theImgBox = imgBoxStart(database,virtChromName,virtWinStart,virtWinEnd,
                            (!revCmplDisp),sideSliceWidth,tl.picWidth);
    // Define a portal with a default expansion size,
    // then set the global dimensions to the full image size
    if (imgBoxPortalDefine(theImgBox,&virtWinStart,&virtWinEnd,&(tl.picWidth),0))
        {
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth - gfxBorder - fullInsideX;
        }

    }


// For portal 3x expansion to work right, it would have to take effect, at least temporarily,
// right here before we call makeWindowListFromVirtChrom().
windows = makeWindowListFromVirtChrom(virtWinStart, virtWinEnd); // creates windows, sets chrom, winStart, winEnd from virtual chrom
if (slCount(windows) > 4000) // TODO a more graceful response
	errAbort("Too many windows in view. Unable to display image at requested zoom level.");

allocPixelsToWindows(); // sets windows insideWidth and insideX

if (theImgBox)
    {
    // If a portal was established, then set the global dimensions back to the portal size
    if (imgBoxPortalDimensions(theImgBox,NULL,NULL,NULL,NULL,&virtWinStart,&virtWinEnd,&(tl.picWidth),NULL))
        {
        virtWinBaseCount = virtWinEnd - virtWinStart;
        fullInsideWidth = tl.picWidth-gfxBorder-fullInsideX;
        }
    }

setGlobalsFromWindow(windows); // first window

seqBaseCount = hChromSize(database, chromName);

/* Save computed position in cart. */
cartSetString(cart, "org", organism);
cartSetString(cart, "db", database);

char newPos[256];

// disguise the cart pos var
if (virtualSingleChrom()) // DISGUISE VMODE
    safef(newPos, sizeof newPos, "%s", windowsSpanPosition());
else // usual
    safef(newPos, sizeof newPos, "%s:%ld-%ld", virtChromName, virtWinStart+1, virtWinEnd);

position = cloneString(newPos);
cartSetString(cart, "position", position);
cartSetString(cart, "oldPosition", position);
//cartSetString(cart, "lastPosition", position);  // this is set in cart.c

cartSetBoolean(cart, "virtMode", virtMode);
cartSetString(cart, "virtModeType", virtModeType);
virtModeType = cartString(cart, "virtModeType"); // refresh the pointer after changing hash


lastVirtModeType=virtModeType;
cartSetString(cart, "lastVirtModeType", lastVirtModeType);
lastVirtModeType = cartString(cart, "lastVirtModeType"); // refresh

lastVirtModeExtraState=virtModeExtraState;
cartSetString(cart, "lastVirtModeExtraState", lastVirtModeExtraState);
lastVirtModeExtraState = cartString(cart, "lastVirtModeExtraState"); // refresh


// save a quick position to use if user leaves virtMode.
if (virtMode)
    cartSetString(cart, "nonVirtPosition", nonVirtPositionFromWindows());
else
    cartRemove(cart, "nonVirtPosition");

// save a highlight position to use if user leaves virtMode.
char *nvh = NULL;
if (virtMode)
   nvh = nonVirtPositionFromHighlightPos();
if (virtMode && nvh)
    cartSetString(cart, "nonVirtHighlight", nvh);
else
    cartRemove(cart, "nonVirtHighlight");

// save lastDbPos. save the current position and other important cart vars related to virtual view.

lastDbPosSaveCartSetting("position");
lastDbPosSaveCartSetting("nonVirtPosition");
lastDbPosSaveCartSetting("virtMode");
lastDbPosSaveCartSetting("virtModeType");
lastDbPosSaveCartSetting("lastVirtModeType");
lastDbPosSaveCartSetting("lastVirtModeExtraState");

cartSetDbPosition(cart, database, lastDbPosCart);

if (cartUsualBoolean(cart, "hgt.psOutput", FALSE))
    handlePostscript();
else
    doTrackForm(NULL, NULL);

boolean gotExtTools = extToolsEnabled();
setupHotkeys(gotExtTools);
if (gotExtTools)
    printExtMenuData(chromName);
if (recTrackSetsEnabled())
    printRecTrackSets();
if (exportedDataHubsEnabled())
    printExportedDataHubs(database);
setupTimeWarning();
}

static void chromInfoTotalRow(int count, long long total, boolean hasAlias)
/* Make table row with total number of sequences and size from chromInfo. */
{
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
printf("Total: %d", count);
cgiTableFieldEnd();
cgiTableFieldStartAlignRight();
printLongWithCommas(stdout, total);
puts("&nbsp;&nbsp;");
cgiTableFieldEnd();
if (hasAlias)
    {
    cgiSimpleTableFieldStart();
    puts("&nbsp");
    cgiTableFieldEnd();
    }
cgiTableRowEnd();
}

static char *chrAliases(struct slName *names, char *sequenceName)
/* names is already a list of aliases, return csv string
 * of alias names and avoid reproducing the sequenceName
 */
{
/* to avoid duplicate names in the returned list, remember them */
struct hash *nameHash = newHashExt(8, TRUE);	/* TRUE == on stack memory */
if (NULL == names)
    return NULL;
if (isNotEmpty(sequenceName))
    hashAddInt(nameHash, sequenceName, 1);
struct dyString *returned = dyStringNew(512);
int wordCount = 0;
for( ; names; names = names->next)
    {
    if (hashLookup(nameHash, names->name) != NULL)
	continue;
    if (isNotEmpty(names->name))
	{
	if (wordCount)
	    dyStringPrintf(returned, ", %s",names->name);
        else
	    dyStringPrintf(returned, "%s", names->name);
	++wordCount;
	hashAddInt(nameHash, names->name, 1);
	}
    }
return dyStringCannibalize(&returned);
}

void chromInfoRowsChromExt(char *sortType)
/* Make table rows of chromosomal chromInfo name & size, sorted by name. */
{
struct slName *chromList = hAllChromNames(database);
struct slName *chromPtr = NULL;
long long total = 0;
boolean hasAlias = hTableExists(database, "chromAlias");
/* key is database sequence name, value is an alias name, can be multiple
 *   entries for the same sequence name.  NULL if no chromAlias available
 */

if (sameString(sortType,"default"))
    slSort(&chromList, chrSlNameCmp);
else if (sameString(sortType,"withAltRandom"))
    slSort(&chromList, chrSlNameCmpWithAltRandom);
else
    errAbort("unknown sort type in chromInfoRowsChromExt: %s", sortType);

for (chromPtr = chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    unsigned size = hChromSize(database, chromPtr->name);
    char *aliasNames = chrAliases(chromAliasFindAliases(chromPtr->name), chromPtr->name);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    htmlPrintf("<A HREF=\"%s|none|?%s|url|=%s|url|&position=%s|url|\">%s</A>",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart),
           chromPtr->name, chromPtr->name);
    cgiTableFieldEnd();
    cgiTableFieldStartAlignRight();
    printLongWithCommas(stdout, size);
    puts("&nbsp;&nbsp;");
    cgiTableFieldEnd();
    if (hasAlias)
	{
	cgiSimpleTableFieldStart();
	if (aliasNames)
            htmlPrintf("%s", aliasNames);
	else
            htmlPrintf("&nbsp;");
        cgiTableFieldEnd();
        }
    cgiTableRowEnd();
    total += size;
    }
chromInfoTotalRow(slCount(chromList), total, hasAlias);
slFreeList(&chromList);
}

void chromInfoRowsChrom()
/* Make table rows of chromosomal chromInfo name & size, sorted by name. */
{
chromInfoRowsChromExt("default");
}

static int  chromInfoCmpSize(const void *va, const void *vb)
/* Compare to sort based on chrom size */
{
const struct chromInfo *a = *((struct chromInfo **)va);
const struct chromInfo *b = *((struct chromInfo **)vb);

return b->size - a->size;
}

void chromInfoRowsNonChromTrackHub(boolean hasAlias, int limit)
/* Make table rows of non-chromosomal chromInfo name & size */
/* leaks chromInfo list */
{
struct chromInfo *chromInfo = trackHubAllChromInfo(database);
slSort(&chromInfo, chromInfoCmpSize);
int seqCount = slCount(chromInfo);
long long total = 0;
char msg1[512], msg2[512];
boolean truncating;
int lineCount = 0;

truncating = (limit > 0) && (seqCount > limit);

for( ;lineCount < seqCount && (chromInfo != NULL); ++lineCount, chromInfo = chromInfo->next)
    {
    unsigned size = chromInfo->size;
    if (lineCount < limit)
	{
	char *aliasNames = chrAliases(chromAliasFindAliases(chromInfo->chrom), chromInfo->chrom);
	cgiSimpleTableRowStart();
	cgiSimpleTableFieldStart();
	htmlPrintf("<A HREF=\"%s|none|?%s|url|=%s|url|&position=%s|url|\">%s</A>",
	    hgTracksName(), cartSessionVarName(), cartSessionId(cart),
	    chromInfo->chrom,chromInfo->chrom);
	cgiTableFieldEnd();
	cgiTableFieldStartAlignRight();
	printLongWithCommas(stdout, size);
	puts("&nbsp;&nbsp;");
	cgiTableFieldEnd();
	if (hasAlias)
	    {
	    cgiSimpleTableFieldStart();
	    if (aliasNames)
		htmlPrintf("%s", aliasNames);
	    else
		htmlPrintf("&nbsp;");
	    cgiTableFieldEnd();
        }
	cgiTableRowEnd();
	}
    total += size;
    }
if (!truncating)
    {
    chromInfoTotalRow(seqCount, total, hasAlias);
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

    unsigned scafCount = seqCount;
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
    printLongWithCommas(stdout, total);
    cgiTableFieldEnd();
    cgiTableRowEnd();
    }
}

void chromInfoRowsNonChrom(boolean hasAlias, int limit)
/* Make table rows of non-chromosomal chromInfo name & size, sorted by size. */
{
if (trackHubDatabase(database))
    {
    chromInfoRowsNonChromTrackHub(hasAlias, limit);
    return;
    }

struct sqlConnection *conn = hAllocConn(database);
/* key is database sequence name, value is an alias name, can be multiple
 *   entries for the same sequence name.  NULL if no chromAlias available
 */
struct sqlResult *sr = NULL;
char **row = NULL;
long long total = 0;
char query[512];
char msg1[512], msg2[512];
int seqCount = 0;
boolean truncating;

sqlSafef(query, sizeof query, "select count(*) from chromInfo");
seqCount = sqlQuickNum(conn, query);
truncating = (limit > 0) && (seqCount > limit);

if (!truncating)
    {
    sqlSafef(query, sizeof query, "select chrom,size from chromInfo order by size desc");
    sr = sqlGetResult(conn, query);
    }
else
    {

    sqlSafef(query, sizeof(query), "select chrom,size from chromInfo order by size desc limit %d", limit);
    sr = sqlGetResult(conn, query);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned size = sqlUnsigned(row[1]);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    htmlPrintf("<A HREF=\"%s|none|?%s|url|=%s|url|&position=%s|url|\">%s</A>",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart),
           row[0], row[0]);
    char *aliasNames = chrAliases(chromAliasFindAliases(row[0]), row[0]);
    cgiTableFieldEnd();
    cgiTableFieldStartAlignRight();
    printLongWithCommas(stdout, size);
    puts("&nbsp;&nbsp;");
    cgiTableFieldEnd();
    if (hasAlias)
        {
        cgiSimpleTableFieldStart();
        if (aliasNames)
            htmlPrintf("%s", aliasNames);
        else
            htmlPrintf("&nbsp;");
        cgiTableFieldEnd();
        }
    cgiTableRowEnd();
    total += size;
    }
if (!truncating)
    {
    chromInfoTotalRow(seqCount, total, hasAlias);
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
    sqlSafef(query, sizeof(query), "select count(*),sum(size) from chromInfo");
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	unsigned scafCount = sqlUnsigned(row[0]);
	long long totalSize = sqlLongLong(row[1]);
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

static void chromSizesDownloadLinks(boolean hasAlias, char *hubAliasFile, char *chromSizesFile)
/* Show link to chrom.sizes file at end of chromInfo table (unless this is a hub) */
{
puts("<p>");
if (! trackHubDatabase(database) || hubConnectIsCurated(trackHubSkipHubName(database)))
    {
    char *db = trackHubSkipHubName(database);
    puts("Download the table below as a text file: ");
    printf("<a href='http%s://%s/goldenPath/%s/bigZips/%s.chrom.sizes' target=_blank>%s.chrom.sizes</a>",
           cgiAppendSForHttps(), hDownloadsServer(), db, db, db);
    puts("&nbsp;&nbsp;");

    if (hasAlias)
	{
	/* see if this database has the chromAlias.txt download file */
	char aliasFile[1024];
        safef(aliasFile, sizeof aliasFile, "http%s://%s/goldenPath/%s/bigZips/%s.chromAlias.txt", cgiAppendSForHttps(), hDownloadsServer(), db, db);
        struct udcFile *file = udcFileMayOpen(aliasFile, udcDefaultDir());
	if (file)
	    {
	    udcFileClose(&file);
	    printf("<a href='%s' target=_blank>%s.chromAlias.txt</a>", aliasFile, db);
	    }
	else
	    puts("&nbsp");
	}
    }
else if (hubAliasFile)
    {
    puts("Download the table below as a text file: ");
    if (chromSizesFile)
	{
        if (startsWith("/gbdb/genark/", chromSizesFile))
	    {
            char *stripped = cloneString(chromSizesFile);
            stripString(stripped, "/gbdb/genark/");
            printf("<a href='https://%s/hubs/%s' target=_blank>%s.chrom.sizes.txt</a>", hDownloadsServer(), stripped, trackHubSkipHubName(database));
	    }
	    else
	    {
	    printf("<a href='%s' target=_blank>%s.chrom.sizes.txt</a>", chromSizesFile, trackHubSkipHubName(database));
	    }
        puts("&nbsp;&nbsp;");
	}
    else
        puts("&nbsp");

    char *aliasUrl;
    /* this URL reference needs to be a text file to work as a click in the
     *    html page.  Both files chromAlias.bb and chromAlias.txt exist.
     */
    if (endsWith(hubAliasFile, "chromAlias.bb"))
       aliasUrl = replaceChars(hubAliasFile, "chromAlias.bb", "chromAlias.txt");
    else
        aliasUrl = cloneString(hubAliasFile);

    if (startsWith("/gbdb/genark/", aliasUrl))
	{
	stripString(aliasUrl, "/gbdb/genark/");
	printf("<a href='https://%s/hubs/%s' target=_blank>%s.chromAlias.txt</a>", hDownloadsServer(), aliasUrl, trackHubSkipHubName(database));
	}
	else
	{
	printf("<a href='%s' target=_blank>%s.chromAlias.txt</a>", aliasUrl, trackHubSkipHubName(database));
	}
    }
puts("</p>");
}

void chromInfoPage()
/* Show list of chromosomes (or scaffolds, etc) on which this db is based. */
{
boolean hasAlias = FALSE;
char *chromSizesFile = NULL;
char *aliasFile = NULL;
if (trackHubDatabase(database))
    {	/* either one of these files present will work */
    aliasFile = trackHubAliasFile(database);
    if (aliasFile)
        {
            hasAlias = TRUE;
        } else {
            aliasFile = trackHubAliasBbFile(database);
            if (aliasFile)
               hasAlias = TRUE;
        }
    chromSizesFile = trackHubChromSizes(database);
    }
else
    hasAlias = hTableExists(database, "chromAlias");

char *position = cartUsualString(cart, "position", hDefaultPos(database));
char *defaultChrom = hDefaultChrom(database);
char *freeze = hFreezeFromDb(database);
struct dyString *title = dyStringNew(512);
if (freeze == NULL)
    dyStringPrintf(title, "%s Browser Sequences",
		   hOrganism(database));
else if (stringIn(database, freeze))
    dyStringPrintf(title, "%s %s Browser Sequences",
		   hOrganism(database), freeze);
else
    dyStringPrintf(title, "%s %s (%s) Browser Sequences",
		   trackHubSkipHubName(hOrganism(database)), freeze, trackHubSkipHubName(database));
webStartWrapperDetailedNoArgs(cart, database, "", title->string, FALSE, FALSE, FALSE, FALSE);
printf("<FORM ACTION=\"%s\" NAME=\"posForm\" METHOD=GET>\n", hgTracksName());
cartSaveSession(cart);

puts("Enter a position, or click on a sequence name to view the entire "
     "sequence in the genome browser.<P>");
puts("Position ");
hTextVar("position", addCommasToPos(database, position), 30);
cgiMakeButton("Submit", "Submit");
puts("<P>");

chromSizesDownloadLinks(hasAlias, aliasFile, chromSizesFile);

hTableStart();
puts("<thead style='position:sticky; top:0; background-color: white;'>");


cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
puts("Sequence name &nbsp;");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
puts("Length (bp) including gaps &nbsp;");
cgiTableFieldEnd();
if (hTableExists(database, "chromAlias"))
    {
    cgiSimpleTableFieldStart();
    puts("Alias sequence names &nbsp;");
    cgiTableFieldEnd();
    }
else if (hasAlias)
    {
    cgiSimpleTableFieldStart();
    puts("Alias sequence names &nbsp;");
    cgiTableFieldEnd();
    }
cgiTableRowEnd();
puts("</thead>");

if (sameString(database,"hg38"))
    chromInfoRowsChromExt("withAltRandom");
else if (trackHubDatabase(database))
    chromInfoRowsNonChrom(hasAlias, 1000);
else if ((startsWith("chr", defaultChrom) || startsWith("Group", defaultChrom)) &&
    hChromCount(database) < 100)
    chromInfoRowsChrom();
else
    chromInfoRowsNonChrom(hasAlias, 1000);

hTableEnd();
cgiDown(0.9);

hgPositionsHelpHtml(organism, database);
puts("</FORM>");
dyStringFree(&title);
webEndSectionTables();
}	/*	void chromInfoPage()	*/

void resetVars()
/* Reset vars except for position and database. */
{
static char *except[] = {"db", "position", NULL};
char *cookieName = hUserCookie();
char *sessionId = cgiOptionalString(cartSessionVarName());
char *userId = findCookieData(cookieName);
struct cart *oldCart = cartNew(userId, sessionId, NULL, NULL);
cartRemoveExcept(oldCart, except);
cartCheckout(&oldCart);
cgiVarExcludeExcept(except);
}

void setupHotkeys(boolean gotExtTools)
/* setup keyboard shortcuts and a help dialog for it */
{
struct dyString *dy = dyStringNew(1024);
// wire the keyboard hotkeys

// the javascript click handler does not seem to call the submit handler, so I'm calling submit manually, every time.

// left
dyStringPrintf(dy,"Mousetrap.bind('ctrl+j', function() { $('input[name=\"hgt.left1\"]').submit().click(); return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('j', function() { $('input[name=\"hgt.left2\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('J', function() { $('input[name=\"hgt.left3\"]').submit().click() }); \n");

// right
dyStringPrintf(dy,"Mousetrap.bind('ctrl+l', function() { $('input[name=\"hgt.right1\"]').submit().click(); return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('l', function() { $('input[name=\"hgt.right2\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('L', function() { $('input[name=\"hgt.right3\"]').submit().click() }); \n");

// zoom in
dyStringPrintf(dy,"Mousetrap.bind('ctrl+i', function() { $('input[name=\"hgt.in1\"]').submit().click(); return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('i', function() { $('input[name=\"hgt.in2\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('I', function() { $('input[name=\"hgt.in3\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('b', function() { $('input[name=\"hgt.inBase\"]').submit().click() }); \n");

// zoom out
dyStringPrintf(dy,"Mousetrap.bind('ctrl+k', function() { $('input[name=\"hgt.out1\"]').click(); return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('k', function() { $('input[name=\"hgt.out2\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('K', function() { $('input[name=\"hgt.out3\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('0', function() { $('input[name=\"hgt.out4\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('ctrl+k', function() { $('input[name=\"hgt.out1\"]').submit().click(); return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('k', function() { $('input[name=\"hgt.out2\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('K', function() { $('input[name=\"hgt.out3\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('0', function() { $('input[name=\"hgt.out4\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('1', function() { zoomTo(50);} ); \n");
dyStringPrintf(dy,"Mousetrap.bind('2', function() { zoomTo(500);} ); \n");
dyStringPrintf(dy,"Mousetrap.bind('3', function() { zoomTo(5000);} ); \n");
dyStringPrintf(dy,"Mousetrap.bind('4', function() { zoomTo(50000);} ); \n");
dyStringPrintf(dy,"Mousetrap.bind('5', function() { zoomTo(500000);} ); \n");
dyStringPrintf(dy,"Mousetrap.bind('6', function() { zoomTo(5000000);} ); \n");

// buttons
dyStringPrintf(dy,"Mousetrap.bind('c f', function() { $('input[name=\"hgTracksConfigPage\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('t s', function() { $('input[name=\"hgt_tSearch\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('h a', function() { $('input[name=\"hgt.hideAll\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('d t', function() { $('#defaultTracksMenuLink')[0].click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('d o', function() { $('#defaultTrackOrderMenuLink')[0].click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('c t', function() { document.customTrackForm.submit();return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('t h', function() { document.trackHubForm.submit();return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('t c', function() { document.editHubForm.submit();return false; }); \n");
dyStringPrintf(dy,"Mousetrap.bind('r s', function() { $('input[name=\"hgt.setWidth\"]').submit().click(); }); \n");
dyStringPrintf(dy,"Mousetrap.bind('r f', function() { $('input[name=\"hgt.refresh\"]')[0].click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('r v', function() { $('input[name=\"hgt.toggleRevCmplDisp\"]').submit().click() }); \n");
dyStringPrintf(dy,"Mousetrap.bind('v d', function() { gotoGetDnaPage() }); \n"); // anon. function because gotoGetDnaPage is sometimes not loaded yet.
dyStringPrintf(dy,"Mousetrap.bind('c h', function() { hubQuickConnect() }); \n"); // anon. function because gotoGetDnaPage is sometimes not loaded yet.

// highlight
dyStringPrintf(dy,"Mousetrap.bind('h c', function() { highlightCurrentPosition('clear'); }); \n");
dyStringPrintf(dy,"Mousetrap.bind('h m', function() { highlightCurrentPosition('add'); }); \n");
//dyStringPrintf(dy,"Mousetrap.bind('h n', function() { highlightCurrentPosition('new'); }); \n"); superfluos as it is just hc + hm?

// focus
dyStringPrintf(dy,"Mousetrap.bind('/', function() { $('input[name=\"hgt.positionInput\"]').focus(); return false; }, 'keydown'); \n");
dyStringPrintf(dy,"Mousetrap.bind('?', function() { showHotkeyHelp() } );\n");

// menu
if (gotExtTools)
    dyStringPrintf(dy,"Mousetrap.bind('s t', function() { showExtToolDialog() } ); \n");

// multi-region views
dyStringPrintf(dy,"Mousetrap.bind('e v', function() { window.location.href='%s?%s=%s&virtModeType=exonMostly'; });  \n",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart));
dyStringPrintf(dy,"Mousetrap.bind('d v', function() { window.location.href='%s?%s=%s&virtModeType=default'; });  \n",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart));

dyStringPrintf(dy,"Mousetrap.bind('v s', function() { window.location.href='%s?chromInfoPage=&%s=%s'; });  \n",
           hgTracksName(), cartSessionVarName(), cartSessionId(cart));

// links to a few tools
dyStringPrintf(dy,"Mousetrap.bind('t b', function() { $('#blatMenuLink')[0].click()});\n");
dyStringPrintf(dy,"Mousetrap.bind('t i', function() { $('#ispMenuLink')[0].click()});\n");
dyStringPrintf(dy,"Mousetrap.bind('t t', function() { $('#tableBrowserMenuLink')[0].click()});\n");
dyStringPrintf(dy,"Mousetrap.bind('c r', function() { $('#cartResetMenuLink')[0].click()});\n");
dyStringPrintf(dy,"Mousetrap.bind('s s', function() { $('#sessionsMenuLink')[0].click()});\n");
dyStringPrintf(dy,"Mousetrap.bind('p s', function() { $('#publicSessionsMenuLink')[0].click()});\n");

// also add an entry to the help menu that shows the keyboard shortcut help dialog
dyStringPrintf(dy,"$(document).ready(addKeyboardHelpEntries);\n");

jsInline(dy->string);
dyStringFree(&dy);

// help dialog
hPrintf("<div style=\"display:none\" id=\"hotkeyHelp\" title=\"Keyboard shortcuts\">\n");
hPrintf("<table style=\"width:600px; border-color:#666666; border-collapse:collapse\">\n");
hPrintf("<tr><td style=\"width:18ch\">left 10&#37;</td><td width=\"auto\" class=\"hotkey\">ctrl+j</td>  <td style=\"width:24ch\"> track search</td><td class=\"hotkey\">t then s</td>               </tr>\n"); // percent sign
hPrintf("<tr><td> left 1/2 screen</td><td class=\"hotkey\">j</td>   <td> default tracks</td><td class=\"hotkey\">d then t</td>             </tr>\n");
hPrintf("<tr><td> left one screen</td><td class=\"hotkey\">J</td>   <td> default order</td><td class=\"hotkey\">d then o</td>              </tr>\n");
hPrintf("<tr><td> right 10&#37;</td><td class=\"hotkey\">ctrl+l</td><td> hide all</td><td class=\"hotkey\">h then a</td>                   </tr>\n"); // percent sign
hPrintf("<tr><td> right 1/2 screen</td><td class=\"hotkey\">l</td>  <td> custom tracks</td><td class=\"hotkey\">c then t</td>              </tr>\n");
hPrintf("<tr><td> right one screen</td><td class=\"hotkey\">L</td>  <td> track collections</td><td class=\"hotkey\">t then c</td>                 </tr>\n");
hPrintf("<tr><td> jump to position box</td><td class=\"hotkey\">/</td>  <td> track hubs</td><td class=\"hotkey\">t then h</td>                 </tr>\n");
hPrintf("<tr><td> zoom in 1.5x</td><td class=\"hotkey\">ctrl+i</td> <td> configure</td><td class=\"hotkey\">c then f</td>                  </tr>\n");
hPrintf("<tr><td> zoom in 3x</td><td class=\"hotkey\">i</td>        <td> reverse</td><td class=\"hotkey\">r then v</td>                    </tr>\n");
hPrintf("<tr><td> zoom in 10x</td><td class=\"hotkey\">I</td>       <td> resize</td><td class=\"hotkey\">r then s</td>                     </tr>\n");
hPrintf("<tr><td> zoom in base level</td><td class=\"hotkey\">b</td><td> refresh</td><td class=\"hotkey\">r then f</td>                    </tr>\n");
hPrintf("<tr><td> zoom out 1.5x</td><td class=\"hotkey\">ctrl+k</td><td> view chrom names</td><td class=\"hotkey\">v then s</td><td class='hotkey'></td>              </tr>\n");
hPrintf("<tr><td> zoom out 3x</td><td class=\"hotkey\">k</td>");
if (gotExtTools)
    hPrintf("<td>send to external tool</td><td class=\"hotkey\">s then t</td>");
hPrintf("               </tr>\n");
hPrintf("<tr><td> zoom out 10x</td><td class=\"hotkey\">K</td>      <td> exon view</td><td class=\"hotkey\">e then v</td>                  </tr>\n");
hPrintf("<tr><td> zoom out 100x</td><td class=\"hotkey\">0</td>     <td> default view</td><td class=\"hotkey\">d then v</td>               </tr>\n");
hPrintf("<tr><td> zoom to ...</td><td class=\"hotkey\"></td><td> view DNA</td><td class='hotkey'>v then d</td></tr>\n");
hPrintf("<tr><td> &nbsp;50bp (1 zero)</td><td class=\"hotkey\">1</td><td>Reset all User Settings</td><td class='hotkey'>c then r</td></tr>\n");
hPrintf("<tr><td> &nbsp;500bp (2 zeros)</td><td class=\"hotkey\">2</td><td>Tools - BLAT</td><td class='hotkey'>t then b</td></tr>\n");
hPrintf("<tr><td> &nbsp;5000bp (3 zeros)</td><td class=\"hotkey\">3</td><td>Tools - Table Browser</td><td class='hotkey'>t then t</td></tr>\n");
hPrintf("<tr><td> &nbsp;50kbp (4 zeros)</td><td class=\"hotkey\">4</td><td>Tools - PCR</td><td class='hotkey'>t then i</td></tr>\n");
hPrintf("<tr><td> &nbsp;500kbp (5 zeros)</td><td class=\"hotkey\">5</td><td>My Sessions</td><td class='hotkey'>s then s</td></tr>\n");
hPrintf("<tr><td> &nbsp;5Mbp (6 zeros)</td><td class=\"hotkey\">6</td><td>Public Sessions</td><td class='hotkey'>p then s</td></tr>\n");
hPrintf("<tr><td> highlight all (mark)</td><td class=\"hotkey\">h then m</td><td>clear all Highlights</td><td class='hotkey'>h then c</td></tr>\n");
hPrintf("<tr><td> quick connect hub</td><td class=\"hotkey\">c then h</td>  <td></td>                 </tr>\n");
hPrintf("</table>\n");
hPrintf("<img style=\"margin:8px\" src=\"../images/shortcutHelp.png\">");
hPrintf("</div>\n");
}

static void checkAddHighlight()
/* If the cart variable addHighlight is set, merge it into the highlight variable. */
{
char *newHighlight = cartOptionalString(cart, "addHighlight");
if (newHighlight)
    {
    char *existing = cartOptionalString(cart, "highlight");
    if (isNotEmpty(existing))
        {
        // Add region only if it is not already in the existing highlight setting.
        char *alreadyIn = strstr(existing, newHighlight);
        int len = strlen(newHighlight);
        if (! (alreadyIn && (alreadyIn[len] == '|' || alreadyIn[len] == '\0')))
            {
            struct dyString *dy = dyStringCreate("%s|%s", newHighlight, existing);
            cartSetString(cart, "highlight", dy->string);
            }
        }
    else
        cartSetString(cart, "highlight", newHighlight);
    cartRemove(cart, "addHighlight");
    }
}

void notify (char *msg, char *msgId)
/* print a message into a hidden DIV tag, and call Javascript to move the DIV under the
 * tableHeaderForm element and un-hide it. Less obtrusive than a warn() message but still hard to miss. */
{
jsInlineF("notifBoxSetup(\"hgTracks\", \"%s\", \"%s\");\n", msgId, msg);
jsInlineF("notifBoxShow(\"hgTracks\", \"%s\");\n", msgId);
}

static boolean noPixVariableSetAndInteractive(void) 
{
/* if the user is a humand and there is no pix variable in the cart, then run a
 * piece of javascript that determines the screen size and reloads the current
 * page, with the &pix=xxx variable added and return true. */
return (isEmpty(cartOptionalString(cart, "pix")) && 
    !sameOk(cgiRequestMethod(NULL), "POST") && // page reload after POST would lose all vars
    !cartUsualBoolean(cart, "hgt.trackImgOnly", FALSE) && // skip if we're hgRenderTracks  = no Javascript
    !cgiWasSpoofed() && // we're not run from the command line
    !sameOk(cgiUserAgent(), "rtracklayer")); // rtracklayer has no javascript, so skip, see https://github.com/lawremi/rtracklayer/issues/113
}

extern boolean issueBotWarning;

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
cart = theCart;

measureTiming = hPrintStatus() && isNotEmpty(cartOptionalString(cart, "measureTiming"));

if (measureTiming)
    measureTime("Got cart: %d elements, userId=%s (=cookie), sessionId=%s", theCart->hash->elCount,
	    theCart->userId, theCart->sessionId);

if (noPixVariableSetAndInteractive())
{
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("utils.js", NULL);
    jsInlineF("addPixAndReloadPage();");
    return;
}

if (measureTiming)
    measureTime("Startup (bottleneck delay %d ms, not applied if under %d) ", botDelayMillis, hgBotDelayCurrWarnMs()) ;

char *mouseOverEnabled = cfgOptionDefault("mouseOverEnabled", "on");
if (sameWordOk(mouseOverEnabled, "on"))
    {
    /* can not use mouseOver in any virtual mode */
    char *isMultiRegion = cartUsualString(cart, "virtModeType", "default");
    if (sameWordOk(isMultiRegion, "default"))
	{
	enableMouseOver = TRUE;
	/* mouseOverJsonFile will be initializes and created at the same
	 * time as the browser .png image file
	 */
	mouseOverJson = jsonWriteNew();
	jsonWriteObjectStart(mouseOverJson, NULL);
	/* this jsonWrite structure will finish off upon successful exit.
	 * each track will start a list with the track name:
	 *   jsonWriteListStart(mouseOverJson, tg->track);
	 */
	}
    }
else
    enableMouseOver = FALSE;

if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

// hide the link "Back to Genome Browser" in the "Genome Browser" menu, since we're on the genome browser now
jsInline("$('#backToBrowserLi').remove();");

char *debugTmp = NULL;
/* Uncomment this to see parameters for debugging. */
/* struct dyString *state = NULL; */
/* #if 1 this to see parameters for debugging. */
/* Be careful though, it breaks if custom track
 * is more than 4k */
#if 0
printf("State: %s\n", cgiUrlString()->string);
#endif

getDbAndGenome(cart, &database, &organism, oldVars);
if (measureTiming)
    measureTime("before chromAliasSetup");
chromAliasSetup(database);
if (measureTiming)
    measureTime("after chromAliasSetup");

genomeIsRna = !isHubTrack(database) && hgPdbOk(database);

initGenbankTableNames(database);

protDbName = hPdbFromGdb(database);
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);

// tell UDC where to put its statistics file
char *udcLogFile;
if ((udcLogFile =  cfgOption("udcLog")) != NULL)
    {
    FILE *fp = mustOpen(udcLogFile, "a");
    udcSetLog(fp);
    }

initTl();

char *configPageCall = cartCgiUsualString(cart, "hgTracksConfigPage", "notSet");
char *configMultiRegionPageCall = cartCgiUsualString(cart, "hgTracksConfigMultiRegionPage", "notSet");

checkAddHighlight();

/* Do main display. */

if (cartUsualBoolean(cart, "hgt.trackImgOnly", FALSE))
    {
    trackImgOnly = TRUE;
    ideogramToo = cartUsualBoolean(cart, "hgt.ideogramToo", FALSE);
    hideControls = TRUE;
    withNextItemArrows = FALSE;
    withNextExonArrows = FALSE;
    hgFindMatchesShowHighlight = FALSE;
    }

jsonForClient = newJsonObject(newHash(8));
jsonObjectAdd(jsonForClient, "cgiVersion", newJsonString(CGI_VERSION));
boolean searching = differentString(cartUsualString(cart, TRACK_SEARCH,"0"), "0");

if(!trackImgOnly)
    {
    // Write out includes for css and js files
    hWrites(commonCssStyles());
    jsIncludeFile("mousetrap.min.js", NULL);
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("utils.js", NULL);
    jsIncludeFile("ajax.js", NULL);
    jsIncludeFile("jquery.watermarkinput.js", NULL);
    if(!searching)
        {
        jsIncludeFile("jquery.history.js", NULL);
        jsIncludeFile("jquery.imgareaselect.js", NULL);
        }
    jsIncludeFile("autocomplete.js", NULL);
    jsIncludeFile("es5-shim.4.0.3.min.js", NULL);
    jsIncludeFile("es5-sham.4.0.3.min.js", NULL);
    jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
    jsIncludeFile("autocompleteCat.js", NULL);
    jsIncludeFile("hgTracks.js", NULL);
    jsIncludeFile("hui.js", NULL);
    jsIncludeFile("spectrum.min.js", NULL);

    // remove the hg.conf option once this feature is released
    if (cfgOptionBooleanDefault("showIgv", FALSE))
        {
        jsIncludeFile("igv.min.js", NULL);
        jsIncludeFile("igvFileHelper.js", NULL);
        }

#ifdef LOWELAB
    jsIncludeFile("lowetooltip.js", NULL);
#endif///def LOWELAB

    webIncludeResourceFile("spectrum.min.css");
    webIncludeResourceFile("jquery-ui.css");
    if (enableMouseOver)
      webIncludeResourceFile("mouseOver.css");

    if (!searching)     // NOT doing search
        {
        webIncludeResourceFile("jquery.contextmenu.css");
        jsIncludeFile("jquery.contextmenu.js", NULL);
        webIncludeResourceFile("ui.dropdownchecklist.css");
        jsIncludeFile("ui.dropdownchecklist.js", NULL);
        jsIncludeFile("ddcl.js", NULL);
        if (cfgOptionBooleanDefault("showTutorial", TRUE))
            {
            jsIncludeFile("shepherd.min.js", NULL);
            webIncludeResourceFile("shepherd.css");
            jsIncludeFile("tutorialPopup.js", NULL);
            jsIncludeFile("basicTutorial.js",NULL);
            jsIncludeFile("clinicalTutorial.js",NULL);
            // if the user is logged in, we won't show the notification
            // that a tutorial is available, just leave the link in the
            // blue bar under "Help"
            if (wikiLinkUserName())
                jsInline("var userLoggedIn = true;");
            // if the CGI variable startTutorial=true is present (in that exact
            // spelling/case), immediately start the tutorial, for example
            // when the user clicks a link from a help page. Note that this
            // means it is a one time link that won't work on refresh because
            // the variable isn't saved onto the URL
            if (sameOk(cgiOptionalString("startTutorial"), "true"))
                {
                jsInline("var startTutorialOnLoad = true;");
                }
            if (sameOk(cgiOptionalString("startClinical"), "true"))
                {
                jsInline("var startClinicalOnLoad = true;");
                }
            }
        }

    hPrintf("<div id='hgTrackUiDialog' style='display: none'></div>\n");
    hPrintf("<div id='hgTracksDialog' style='display: none'></div>\n");
    if (cfgOptionBooleanDefault("canDoHgcInPopUp", FALSE))
        {
        jsIncludeFile("hgc.js", NULL);
        jsIncludeFile("alleles.js", NULL);
        hPrintf("<div id='hgcDialog' style='display: none'></div>\n");
        }

    cartFlushHubWarnings();
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
else if (sameWord(configMultiRegionPageCall, "multi-region"))
    {
    cartRemove(cart, "hgTracksConfigMultiRegionPage");
    configMultiRegionPage();
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

if (cartVarExists(cart, "hgt.convertChromToVirtChrom"))
    {
    cartRemove(cart, "hgt.convertChromToVirtChrom");
    return;
    }

jsonObjectAdd(jsonForClient, "measureTiming", newJsonBoolean(measureTiming));
// js code needs to know if a highlightRegion is defined for this db
checkAddHighlight(); // call again in case tracksDisplay's call to resolvePosition changed vars
char *highlightDef = cartOptionalString(cart, "highlight");
if (highlightDef)
    jsonObjectAdd(jsonForClient, "highlight", newJsonString(highlightDef));

char *prevColor = cartOptionalString(cart, "prevHlColor");
if (prevColor)
    jsonObjectAdd(jsonForClient, "prevHlColor", newJsonString(prevColor));

jsonObjectAdd(jsonForClient, "enableHighlightingDialog",
	      newJsonBoolean(cartUsualBoolean(cart, "enableHighlightingDialog", TRUE)));

// add the center label height if center labels are present
if (withCenterLabels)
    jsonObjectAdd(jsonForClient, "centerLabelHeight",
              newJsonNumber(tl.fontHeight));

struct dyString *dy = dyStringNew(1024);
jsonDyStringPrint(dy, (struct jsonElement *) jsonForClient, "hgTracks", 0);
jsInline(dy->string);
dyStringFree(&dy);

dy = dyStringNew(1024);
// always write the font-size, it's useful for other javascript functions
dyStringPrintf(dy, "window.browserTextSize=%s;\n", tl.textSize);
// do not have a JsonFile available when PDF/PS output
if (enableMouseOver && isNotEmpty(mouseOverJsonFile->forCgi))
    {
    jsonWriteObjectEnd(mouseOverJson);
    /* if any data was written, it is longer than 4 bytes */
    if (strlen(mouseOverJson->dy->string) > 4)
	{
	FILE *trashJson = mustOpen(mouseOverJsonFile->forCgi, "w");
	fputs(mouseOverJson->dy->string,trashJson);
	carefulClose(&trashJson);
	}

    hPrintf("<div id='mouseOverVerticalLine' class='mouseOverVerticalLine'></div>\n");
    hPrintf("<div id='mouseOverText' class='mouseOverText'></div>\n");
    dyStringPrintf(dy, "window.mouseOverEnabled=true;\n");
    }
else
    {
    dyStringPrintf(dy, "window.mouseOverEnabled=false;\n");
    }
jsInline(dy->string);
dyStringFree(&dy);

if (measureTiming)
    measureTime("Time at end of doMiddle, next up cart write");

if (cartOptionalString(cart, "udcTimeout"))
    {
    char buf[5000];
    safef(buf, sizeof(buf), "A hub refresh (udcTimeout) setting is active. "
	"This is useful when developing hubs, but it reduces "
	"performance. To clear the setting, click "
	"<A HREF='hgTracks?hgsid=%s|url|&udcTimeout=[]'>here</A>.",cartSessionId(cart));
    notify(buf, "udcTimeout");
    }
#ifdef DEBUG
if (cdsQueryCache != NULL)
    cacheTwoBitRangesPrintStats(cdsQueryCache, stderr);
#endif /* DEBUG */
}

void labelTrackAsFilteredNumber(struct track *tg, unsigned numOut)
/* add text to track long label to indicate filter is active. Also add doWiggle/windowsize label. */
{
if (numOut > 0)
    tg->longLabel = labelAsFilteredNumber(tg->longLabel, numOut);

if (cartOrTdbBoolean(cart, tg->tdb, "doWiggle", FALSE))
    labelTrackAsDensity(tg);
else if (winTooBigDoWiggle(cart, tg))
    labelTrackAsDensityWindowSize(tg);
}

void labelTrackAsFiltered(struct track *tg)
/* add text to track long label to indicate filter is active */
{
tg->longLabel = labelAsFiltered(tg->longLabel);

// also label parent composite track filtered
struct trackDb *parentTdb = tdbGetComposite(tg->tdb);
if (parentTdb)
    parentTdb->longLabel = labelAsFiltered(parentTdb->longLabel);
}

static char *labelAddNote(char *label, char *note)
/* add parenthesized text to label */
// TODO: move to lib/trackDbCustom.c
{
char buffer[2048];
safef(buffer, sizeof buffer, " (%s)", note);
if (stringIn(note, label))
    return label;
return (catTwoStrings(label, buffer));
}

void labelTrackAsHideEmpty(struct track *tg)
/* Add text to track long label to indicate empty subtracks are hidden,
 * but avoid adding to subtrack labels */
{
#define EMPTY_SUBTRACKS_HIDDEN "empty subtracks hidden"
struct trackDb *parentTdb = tdbGetComposite(tg->tdb);
if (parentTdb)
    parentTdb->longLabel = labelAddNote(parentTdb->longLabel, EMPTY_SUBTRACKS_HIDDEN);
else
    tg->longLabel = labelAddNote(tg->longLabel, EMPTY_SUBTRACKS_HIDDEN);
}

void labelTrackAsDensity(struct track *tg)
/* Add text to track long label to indicate density mode */
{
tg->longLabel = labelAddNote(tg->longLabel, "item density shown");
}

void labelTrackAsDensityWindowSize(struct track *tg)
/* Add text to track long label to indicate density mode because window size exceeds some threshold */
{
tg->longLabel = labelAddNote(tg->longLabel, "item density shown - zoom in for individual items or use squish or dense mode");
}


