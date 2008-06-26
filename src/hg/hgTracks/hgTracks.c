/* hgTracks - Human Genome browser main cgi script. */
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
#include "bed.h"
#include "bedCart.h"
#include "customTrack.h"
#include "cytoBand.h"
#include "ensFace.h"
#include "liftOver.h"
#include "pcrResult.h"
#include "wikiLink.h"
#include "jsHelper.h"
#include "mafTrack.h"

static char const rcsid[] = "$Id: hgTracks.c,v 1.1486.2.1 2008/06/26 18:58:58 aamp Exp $";

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
boolean baseShowPos;           /* TRUE if should display full position at top of base track */
boolean baseShowAsm;           /* TRUE if should display assembly info at top of base track */
boolean baseShowScaleBar;      /* TRUE if should display scale bar at very top of base track */ 
boolean baseShowRuler;         /* TRUE if should display the basic ruler in the base track (default) */
char *baseTitle = NULL;        /* Title it should display top of base track (optional)*/
static char *userSeqString = NULL;	/* User sequence .fa/.psl file. */

/* These variables are set by getPositionFromCustomTracks() at the very 
 * beginning of tracksDisplay(), and then used by loadCustomTracks(). */
char *ctFileName = NULL;	/* Custom track file. */
struct customTrack *ctList = NULL;  /* Custom tracks. */
boolean hasCustomTracks = FALSE;  /* whether any custom tracks are for this db*/
struct slName *browserLines = NULL; /* Custom track "browser" lines. */

boolean withNextItemArrows = FALSE;	/* Display next feature (gene) navigation buttons near center labels? */
boolean withPriorityOverride = FALSE;	/* Display priority for each track to allow reordering */

int gfxBorder = hgDefaultGfxBorder;	/* Width of graphics border. */
int guidelineSpacing = 12;	/* Pixels between guidelines. */

boolean withIdeogram = TRUE;            /* Display chromosome ideogram? */
boolean ideogramAvail = FALSE;           /* Is the ideogram data available for this genome? */

int rulerMode = tvHide;         /* on, off, full */

char *rulerMenu[] =
/* dropdown for ruler visibility */
    {
    "hide",
    "dense",
    "full"
    };

static long enteredMainTime = 0;	/* time at beginning of main()	*/
char *protDbName;               /* Name of proteome database for this genome. */
#define MAX_CONTROL_COLUMNS 5
#define LOW 1
#define MEDIUM 2
#define BRIGHT 3
#define MAXCHAINS 50000000
boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
int imagePixelHeight = 0;
struct hash *oldVars = NULL;

boolean hideControls = FALSE;		/* Hide all controls? */

/* Structure returned from findGenomePos. 
 * We use this to to expand any tracks to full
 * that were found to contain the searched-upon
 * position string */
struct hgPositions *hgp = NULL;


/* Other global variables. */
struct group *groupList = NULL;    /* List of all tracks. */
char *browserName;              /* Test or public browser */
char *organization;             /* UCSC or MGC */


struct track *trackFindByName(struct track *tracks, char *trackName)
/* find a track in tracks by name, recursively searching subtracks */
{
struct track *track;
for (track = tracks; track != NULL; track = track->next)
    {
    if (sameString(track->mapName, trackName))
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
   return -1 * strcasecmp(a->shortLabel, b->shortLabel);
else
   return 1;
}

int trackRefCmpPriority(const void *va, const void *vb)
/* Compare based on priority. */
{
const struct trackRef *a = *((struct trackRef **)va);
const struct trackRef *b = *((struct trackRef **)vb);
float dif = a->track->group->priority - b->track->group->priority;

if (dif == 0)
    dif = a->track->priority - b->track->priority;
if (dif < 0)
    return -1;
if (dif == 0.0)
    {
    /* assure super tracks appear ahead of their members if same pri */
    if (a->track->tdb && a->track->tdb->isSuper)
        return 0;
    if (b->track->tdb && b->track->tdb->isSuper)
        return 1;
   return 0;
   }
else
   return 1;
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
	    if (changeVis == -1)
                {
                /* restore defaults */
                if (tdb->parentName)
                    {
                    /* removing the supertrack parent's cart variables
                     * restores defaults */
                    cartRemove(cart, tdb->parentName);
                    if (withPriorityOverride)
                        {
                        safef(pname, sizeof(pname), "%s.priority",
                                    tdb->parentName);
                        cartRemove(cart, pname);
                        }
                    }
                track->visibility = tdb->visibility;
                cartRemove(cart, track->mapName);

                /* set the track priority back to the default value */
                if (withPriorityOverride)
                    {
                    safef(pname, sizeof(pname), "%s.priority",track->mapName);
                    cartRemove(cart, pname);
                    track->priority = track->defaultPriority;
                    }
                }
            else
                {
                /* change to specified visibility */
                if (tdb->parent)
                    {
                    /* Leave supertrack members alone -- only change parent */
                    struct trackDb *parentTdb = tdb->parent;
                    if ((changeVis == tvHide && !parentTdb->isShow) ||
                        (changeVis != tvHide && parentTdb->isShow))
                        {
                        /* remove if setting to default vis */
                        cartRemove(cart, parentTdb->tableName);
                        }
                    else
                        cartSetString(cart, parentTdb->tableName, 
                                    changeVis == tvHide ? "hide" : "show");
                    }
                else 
                    {
                    /* regular track */
                    if (changeVis == tdb->visibility)
                        /* remove if setting to default vis */
                        cartRemove(cart, track->mapName);
                    else
                        cartSetString(cart, track->mapName, 
                                                hStringFromTv(changeVis));
                    track->visibility = changeVis;
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
			  int height, char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
x = hvGfxAdjXW(hvg, x, width);
char *encodedName = cgiEncode(name);

hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s\"", hgTrackUiName(), cartSessionVarName(),
                         cartSessionId(cart), chromName, encodedName);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
freeMem(encodedName);
}

static void mapBoxToggleComplement(struct hvGfx *hvg, int x, int y, int width, int height, 
	struct track *toggleGroup, char *chrom,
	int start, int end, char *message)
/*print out a box along the DNA bases that toggles a cart variable
 * "complement" to complement the DNA bases at the top by the ruler*/
{
struct dyString *ui = uiStateUrlPart(toggleGroup);
x = hvGfxAdjXW(hvg, x, width);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?complement_%s=%d",
	hgTracksName(), database, !cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE));
hPrintf("&%s\"", ui->string);
freeDyString(&ui);
if (message != NULL)
    mapStatusMessage("%s", message);
hPrintf(">\n");
}

void smallBreak()
/* Draw small horizontal break */
{
hPrintf("<FONT SIZE=1><BR></FONT>\n");
}

int trackPlusLabelHeight(struct track *track, int fontHeight)
/* Return the sum of heights of items in this track (or subtrack as it may be) 
 * and the center label(s) above the items (if any). */
{
int y = track->totalHeight(track, track->limitedVis);
if (isWithCenterLabels(track))
    y += fontHeight;
if (isCompositeTrack(track))
    {
    struct track *subtrack;
    for (subtrack = track->subtracks;  subtrack != NULL;
	 subtrack = subtrack->next)
	{
	if (isSubtrackVisible(subtrack) && isWithCenterLabels(subtrack))
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
else				/* try to make the button look as if
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
   seq = hDnaFromSeq(chromName, winStart, winEnd, dnaUpper);
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
    if(sameString(track->mapName, "cytoBandIdeo"))
	{
	if (hTableExists(track->mapName))
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

void makeChromIdeoImage(struct track **pTrackList, char *psOutput)
/* Make an ideogram image of the chromsome and our position in
   it. */
{
struct track *ideoTrack = NULL;
MgFont *font = tl.font;
char *mapName = "ideoMap";
struct hvGfx *hvg;
struct tempName gifTn;
boolean doIdeo = TRUE;
int ideoWidth = round(.8 *tl.picWidth);
int ideoHeight = 0;
int textWidth = 0;

ideoTrack = chromIdeoTrack(*pTrackList);

/* If no ideogram don't draw. */
if(ideoTrack == NULL)
    doIdeo = FALSE;
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

    /* If doing postscript, skip ideogram. */
    if(psOutput)
	doIdeo = FALSE;
    }
if(doIdeo)
    {
    char startBand[16];
    char endBand[16];
    char title[32];
    startBand[0] = endBand[0] = '\0';
    fillInStartEndBands(ideoTrack, startBand, endBand, sizeof(startBand)); 
    /* Draw the ideogram. */
    trashDirFile(&gifTn, "hgtIdeo", "hgtIdeo", ".gif");
    /* Start up client side map. */
    hPrintf("<MAP Name=%s>\n", mapName);
    ideoHeight = gfxBorder + ideoTrack->height;
    hvg = hvGfxOpenGif(ideoWidth, ideoHeight, gifTn.forCgi);
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
    hPrintf("</MAP>\n");
    }
hPrintf("<TABLE BORDER=0 CELLPADDING=0>");
if(doIdeo)
    {
    hPrintf("<TR><TD HEIGHT=5></TD></TR>");
    hPrintf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
	    gifTn.forHtml, ideoWidth, ideoHeight, mapName);
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD HEIGHT=5></TD></TR></TABLE>");
    }
else
    hPrintf("<TR><TD HEIGHT=10></TD></TR></TABLE>");
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
if (! pcrResultParseCart(cart, &pslFileName, &primerFileName, &target))
    return;

struct psl *pslList = pslLoadAll(pslFileName), *psl;
struct linkedFeatures *itemList = NULL;
if (target != NULL)
    {
    int rowOffset = hOffsetPastBin(chromName, target->pslTable);
    struct sqlConnection *conn = hAllocConn();
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
	    if (sameString(gpsl->tName, chromName) && gpsl->tStart < winEnd &&
		gpsl->tEnd > winStart)
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
		safecpy(lf->name, sizeof(lf->name), itemAcc);
		lf->extra = cloneString(itemName);
		slAddHead(&itemList, lf);
		pslFree(&trimmed);
		}
	    pslFree(&gpsl);
	    }
	}
    hFreeConn(&conn);
    }
else
    for (psl = pslList;  psl != NULL;  psl = psl->next)
	if (sameString(psl->tName, chromName) && psl->tStart < winEnd &&
	    psl->tEnd > winStart)
	    {
	    /* Collapse the 2-block PSL into one block for display: */
	    psl->blockCount = 1;
	    psl->blockSizes[0] = psl->tEnd - psl->tStart;
	    struct linkedFeatures *lf = 
		lfFromPslx(psl, 1, FALSE, FALSE, tg);
	    safecpy(lf->name, sizeof(lf->name), "");
	    lf->extra = cloneString("");
	    slAddHead(&itemList, lf);
	    }
pslFreeList(&pslList);
slSort(&itemList, linkedFeaturesCmp);
tg->items = itemList;
}

struct track *pcrResultTg()
/* Make track of hgPcr results (alignments of user's submitted primers). */
{
struct trackDb *tdb = pcrResultFakeTdb();
struct track *tg = trackFromTrackDb(tdb);
tg->loadItems = pcrResultLoad;
tg->itemName = lfMapNameFromExtra;
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
/* Load up rnas from table into track items. */
{
char *ss = userSeqString;
char buf2[3*512];
char *faFileName, *pslFileName;
struct lineFile *f;
struct psl *psl;
struct linkedFeatures *lfList = NULL, *lf;
enum gfType qt, tt;
int sizeMul = 1;
enum baseColorDrawOpt drawOpt = baseColorGetDrawOpt(tg);
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;

indelEnabled(cart, (tg ? tg->tdb : NULL), basesPerPixel,
	     &indelShowDoubleInsert, &indelShowQueryInsert, &indelShowPolyA);

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
	if (drawOpt > baseColorDrawOff ||
	    indelShowQueryInsert || indelShowPolyA)
	    lf->original = psl;
	else
	    pslFree(&psl);
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
tg->mapName = "hgUserPsl";
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
tdb->tableName = cloneString(tg->mapName);
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
    seq = hDnaFromSeq(chromName, winStart, winEnd, dnaLower);
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
        warn("More than %d items in %s, suppressing display",
	    maxCount, tg->shortLabel);
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
tg->mapName = "oligoMatch";
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
tdb->tableName = cloneString(tg->mapName);
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
char o4[128];
char o5[128];
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

/*	Wiggle tracks depend upon clipping.  They are reporting
 *	totalHeight artifically high by 1 so this will leave a
 *	blank area one pixel high below the track.
 */
if (sameString("wig",track->tdb->type))
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight-1);
else
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);

minRange = 0.0;
safef( o4, sizeof(o4),"%s.min.cutoff", track->mapName);
safef( o5, sizeof(o5),"%s.max.cutoff", track->mapName);
minRangeCutoff = max( atof(cartUsualString(cart,o4,"0.0"))-0.1, 
                                track->minRange );
maxRangeCutoff = min( atof(cartUsualString(cart,o5,"1000.0"))+0.1, 
                                track->maxRange);
if( sameString( track->mapName, "humMusL" ) ||
    sameString( track->mapName, "musHumL" ) ||
    sameString( track->mapName, "mm3Rn2L" ) ||		
    sameString( track->mapName, "hg15Mm3L" ) ||		
    sameString( track->mapName, "mm3Hg15L" ) ||
    sameString( track->mapName, "regpotent" ) ||
    sameString( track->mapName, "HMRConservation" )  )
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
        break;	/* Do nothing; */
    case tvPack:
    case tvSquish:
	y += tHeight;
        break;
    case tvFull:
        if (isWithCenterLabels(track))
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
            if(track->subType == lfSubSample )
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
                if( sameString( track->mapName, "humMusL" ) || 
                         sameString( track->mapName, "hg15Mm3L" ))
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1,
                             itemHeight, track->ixColor, font, "Mouse Cons");
                else if( sameString( track->mapName, "musHumL" ) ||
                         sameString( track->mapName, "mm3Hg15L"))
                    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1, 
                                itemHeight, track->ixColor, font, "Human Cons");
                else if( sameString( track->mapName, "mm3Rn2L" ))
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
        
        if (isWithCenterLabels(track))
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
int arrowWidth = insideHeight;
int arrowButtonWidth = arrowWidth + 2 * NEXT_ITEM_ARROW_BUFFER;
int rightButtonX = insideX + insideWidth - arrowButtonWidth - 1;
char buttonText[100];
Color fillColor = lightGrayIndex();
labelColor = blackIndex();
hvGfxNextItemButton(hvg, rightButtonX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, TRUE);
hvGfxNextItemButton(hvg, insideX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, FALSE);
safef(buttonText, ArraySize(buttonText), "hgt.prevItem=%s", track->mapName);
mapBoxReinvoke(hvg, insideX, y + 1, arrowButtonWidth, insideHeight, NULL,
	       NULL, 0, 0, (revCmplDisp ? "Next item" : "Prev item"), buttonText);
mapBoxToggleVis(hvg, insideX + arrowButtonWidth, y + 1, insideWidth - (2 * arrowButtonWidth), insideHeight, parentTrack);
safef(buttonText, ArraySize(buttonText), "hgt.nextItem=%s", track->mapName);
mapBoxReinvoke(hvg, insideX + insideWidth - arrowButtonWidth, y + 1, arrowButtonWidth, insideHeight, NULL,
	       NULL, 0, 0, (revCmplDisp ? "Prev item" : "Next item"), buttonText);
}

static int doCenterLabels(struct track *track, struct track *parentTrack,
                                struct hvGfx *hvg, MgFont *font, int y)
/* Draw center labels.  Return y coord */
{
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
if (track->limitedVis != tvHide)
    {
    Color labelColor = (track->labelColor ? 
                        track->labelColor : track->ixColor);
    hvGfxTextCentered(hvg, insideX, y+1, insideWidth, insideHeight, 
                        labelColor, font, track->longLabel);
    if (withNextItemArrows && track->labelNextItemButtonable && track->labelNextPrevItem)
	doLabelNextItemButtons(track, parentTrack, hvg, font, y, trackPastTabX,
			  trackPastTabWidth, fontHeight, insideHeight, labelColor);
    else
	mapBoxToggleVis(hvg, trackPastTabX, y+1, 
			trackPastTabWidth, insideHeight, parentTrack);
    mapBoxToggleVis(hvg, trackPastTabX, y+1, 
                    trackPastTabWidth, insideHeight, parentTrack);
    y += fontHeight;
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
if (isWithCenterLabels(track))
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

if (isWithCenterLabels(track))
    y += fontHeight;
if (track->mapsSelf)
    {
    return y+track->height;
    }
if (track->subType == lfSubSample && track->items == NULL)
     y += track->lineHeight;

/* override doMapItems for hapmapLd track */
/* does not scale with subtracks right now, so this is commented out until it can be fixed
if (startsWith("hapmapLd",track->mapName))
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
int pixWidth = tl.picWidth;
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
Color labelColor = (track->labelColor ? track->labelColor : track->ixColor);
if (track->limitedVis == tvPack)
    { /*XXX This needs to be looked at, no example yet*/
    hvGfxSetClip(hvg, gfxBorder+trackTabWidth+1, y, 
              pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
    track->drawLeftLabels(track, winStart, winEnd,
                          hvg, leftLabelX, y, leftLabelWidth, tHeight,
                          isWithCenterLabels(track), font, labelColor, 
                          track->limitedVis);
    }
else
    {
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);
    
    /* when the limitedVis == tvPack is correct above,
     *	this should be outside this else clause
     */
    track->drawLeftLabels(track, winStart, winEnd,
                          hvg, leftLabelX, y, leftLabelWidth, tHeight,
                          isWithCenterLabels(track), font, labelColor, 
                          track->limitedVis);
    }
hvGfxUnclip(hvg);
y += tHeight;
return y;
}

int doTrackMap(struct track *track, struct hvGfx *hvg, int y, int fontHeight, 
	       int trackPastTabX, int trackPastTabWidth)
/* Write out the map for this track. Return the new offset. */
{
int mapHeight = 0;
switch (track->limitedVis)
    {
    case tvHide:
	break;	/* Do nothing; */
    case tvPack:
    case tvSquish:
	y += trackPlusLabelHeight(track, fontHeight);
	break;
    case tvFull:
	if (!nextItemCompatible(track))
	    {
	    if (isCompositeTrack(track))
		{
		struct track *subtrack;
		for (subtrack = track->subtracks;  subtrack != NULL;
		     subtrack = subtrack->next)
		    if (isSubtrackVisible(subtrack))
                        y = doMapItems(subtrack, hvg, fontHeight, y);
		}
	    else
		y = doMapItems(track, hvg, fontHeight, y);
	    }
	else
	    y += trackPlusLabelHeight(track, fontHeight);
	break;
    case tvDense:
	if (isWithCenterLabels(track))
	    y += fontHeight;
	if (isCompositeTrack(track))
	    mapHeight = track->height;
	else
	    mapHeight = track->lineHeight;
	mapBoxToggleVis(hvg, trackPastTabX, y, trackPastTabWidth, mapHeight, track);
	y += mapHeight;
	break;
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
int y;
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
int relNumOff;
boolean rulerCds = zoomedToCdsColorLevel;

/* Start a global track hash. */
trackHash = newHash(8);
/* Figure out dimensions and allocate drawing space. */
pixWidth = tl.picWidth;

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
/* firefox on Linux worked almost up to 34,000 at the default 620 width	*/
#define maxSafeHeight	32000
/* Hash tracks/subtracks, limit visibility and calculate total image height: */
for (track = trackList; track != NULL; track = track->next)
    {
    hashAddUnique(trackHash, track->mapName, track);
    limitVisibility(track);
    if (!safeHeight)
	{
	track->limitedVis = tvHide;
	track->limitedVisSet = TRUE;
	continue;
	}
    if (track->limitedVis != tvHide)
	{
        if (isCompositeTrack(track))
            {
            struct track *subtrack;
            for (subtrack = track->subtracks; subtrack != NULL;
                         subtrack = subtrack->next)
                {
		hashAddUnique(trackHash, subtrack->mapName, subtrack);
                if (!isSubtrackVisible(subtrack))
                    continue;
		if (!subtrack->limitedVisSet)
		    {
		    subtrack->visibility = track->visibility;
		    subtrack->limitedVis = track->limitedVis;
		    subtrack->limitedVisSet = TRUE;
		    }
                }
	    }
	if (maxSafeHeight < (pixHeight+trackPlusLabelHeight(track,fontHeight)))
	    {
	    char numBuf[64];
	    sprintLongWithCommas(numBuf, maxSafeHeight);
	    printf("warning: image is over %s pixels high at "
		"track '%s',<BR>remaining tracks set to hide "
		"for this view.<BR>\n", numBuf, track->tdb->shortLabel);
	    safeHeight = FALSE;
	    track->limitedVis = tvHide;
	    track->limitedVisSet = TRUE;
	    }
	else
	    pixHeight += trackPlusLabelHeight(track, fontHeight);
	}
    }

imagePixelHeight = pixHeight;
if (psOutput)
    hvg = hvGfxOpenPostScript(pixWidth, pixHeight, psOutput);
else
    {
    trashDirFile(&gifTn, "hgt", "hgt", ".gif");
    hvg = hvGfxOpenGif(pixWidth, pixHeight, gifTn.forCgi);
    }
hvg->rc = revCmplDisp;
initColors(hvg);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

/* Find colors to draw in. */
findTrackColors(hvg, trackList);

leftLabelX = gfxBorder;
leftLabelWidth = insideX - gfxBorder*3;

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
        drawGrayButtonBox(hvg, trackTabX, y, trackTabWidth, height, TRUE);
        mapBoxTrackUi(hvg, trackTabX, y, trackTabWidth, height,
		      RULER_TRACK_NAME, RULER_TRACK_LABEL);
        y += height + 1;
        }
    for (track = trackList; track != NULL; track = track->next)
        {
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
            if (grayButtonGroup) 
                drawGrayButtonBox(hvg, trackTabX, yStart, trackTabWidth, 
	    	                h, track->hasUi); 
            else
                drawBlueButtonBox(hvg, trackTabX, yStart, trackTabWidth, 
	    	                h, track->hasUi); 
	    if (track->hasUi)
                mapBoxTrackUi(hvg, trackTabX, yStart, trackTabWidth, h,
			      track->mapName, track->shortLabel);
	    }
	}
    butOff = trackTabX + trackTabWidth;
    leftLabelX += butOff;
    leftLabelWidth -= butOff;
    }

if (withLeftLabels)
    {
    Color lightRed = hvGfxFindColorIx(hvg, 255, 180, 180);

    hvGfxBox(hvg, leftLabelX + leftLabelWidth, 0,
    	gfxBorder, pixHeight, lightRed);
    y = gfxBorder;
    if (rulerMode != tvHide)
	{
	if (baseTitle)
	    {
	    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, titleHeight, 
			MG_BLACK, font, WIN_TITLE_LABEL);
	    y += titleHeight;
	    }
	if (baseShowPos||baseShowAsm)
	    {
	    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, showPosHeight, 
			MG_BLACK, font, WIN_POS_LABEL);
	    y += showPosHeight;
	    }
	if (baseShowScaleBar)
	    {
	    y += scaleBarPad;
	    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, scaleBarHeight, 
			MG_BLACK, font, SCALE_BAR_LABEL);
	    y += scaleBarHeight + scaleBarPad;
	    }
	if (baseShowRuler)
	    {
	    char rulerLabel[64];
	    if (hvg->rc)
		safef(rulerLabel,ArraySize(rulerLabel),":%s",chromName);
	    else
		safef(rulerLabel,ArraySize(rulerLabel),"%s:",chromName);
	    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, rulerHeight, 
			   MG_BLACK, font, rulerLabel);
	    y += rulerHeight;
	    }
	if (zoomedToBaseLevel || rulerCds)
	    {		    
	    /* disable complement toggle for HIV because HIV is single stranded RNA */
	    if (!hIsGsidServer())
                drawComplementArrow(hvg,leftLabelX, y,
                                    leftLabelWidth-1, baseHeight, font);
	    if (zoomedToBaseLevel)				    
    		y += baseHeight;
	    }
        if (rulerCds)
            y += rulerTranslationHeight;
	}
    for (track = trackList; track != NULL; track = track->next)
        {
	if (track->limitedVis == tvHide)
	    continue;
        if (isCompositeTrack(track))
            {
	    struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
            for (subtrack = track->subtracks; subtrack != NULL;
		 subtrack = subtrack->next)
                if (isSubtrackVisible(subtrack))
                    y = doLeftLabels(subtrack, hvg, font, y);
            }
        else
            y = doLeftLabels(track, hvg, font, y);
        }
    }
else
    {
    leftLabelX = leftLabelWidth = 0;
    }

/* Draw guidelines. */
if (withGuidelines)
    {
    int height = pixHeight - 2*gfxBorder;
    int x;
    Color lightBlue = hvGfxFindRgb(hvg, &guidelineColor);

    hvGfxSetClip(hvg, insideX, gfxBorder, insideWidth, height);
    y = gfxBorder;

    for (x = insideX+guidelineSpacing-1; x<pixWidth; x += guidelineSpacing)
	hvGfxBox(hvg, x, y, 1, height, lightBlue);
    hvGfxUnclip(hvg);
    }

/* Show ruler at top. */
if (rulerMode != tvHide)
    {
    struct dnaSeq *seq = NULL;
    int rulerClickY = 0;
    int rulerClickHeight = rulerHeight;

    y = rulerClickY;
    hvGfxSetClip(hvg, insideX, y, insideWidth, yAfterRuler-y+1);
    relNumOff = winStart;
    
    if (baseTitle)
	{
	hvGfxTextCentered(hvg, insideX, y, insideWidth, titleHeight, 
			    MG_BLACK, font, baseTitle);
	rulerClickHeight += titleHeight;
	y += titleHeight;
	}
    if (baseShowPos||baseShowAsm)
	{
	char txt[256];
	char numBuf[64];
	char *freezeName = NULL;
	freezeName = hFreezeFromDb(database);
	sprintLongWithCommas(numBuf, winEnd-winStart);
	if(freezeName == NULL)
	    freezeName = "Unknown";
	if (baseShowPos&&baseShowAsm)
    	    safef(txt,sizeof(txt),"%s %s   %s (%s bp)",organism,freezeName,addCommasToPos(position),numBuf);
	else if (baseShowPos)
    	    safef(txt,sizeof(txt),"%s (%s bp)",addCommasToPos(position),numBuf);
	else
    	    safef(txt,sizeof(txt),"%s %s",organism,freezeName);
	hvGfxTextCentered(hvg, insideX, y, insideWidth, showPosHeight, 
			    MG_BLACK, font, txt);
	rulerClickHeight += showPosHeight;
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
	rulerClickHeight += scaleBarTotalHeight;
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
    {
    /* Make hit boxes that will zoom program around ruler. */
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
	mapBoxJumpTo(hvg, ps+insideX,rulerClickY,pe-ps,rulerClickHeight,
		        chromName, ns, ne, message);
	}
    }
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
        chromSize = hChromSize(chromName);
        end = min(winEnd + 3, chromSize);
        extraSeq = hDnaFromSeq(chromName, start, end, dnaUpper);
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
            mapBoxReinvoke(hvg, insideX, y+rulerHeight, insideWidth,baseHeight,
			   NULL, NULL, 0, 0, "", newRulerVis);
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
    }

/* Draw center labels. */
if (withCenterLabels)
    {
    hvGfxSetClip(hvg, insideX, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
        {
        struct track *subtrack;
	if (track->limitedVis == tvHide)
	    continue;
        if (isCompositeTrack(track))
            {
	    if (isWithCenterLabels(track))
		y = doCenterLabels(track, track, hvg, font, y)
		    - track->height; /* subtrack heights tallied below: */
	    for (subtrack = track->subtracks; subtrack != NULL;
		 subtrack = subtrack->next)
		if (isSubtrackVisible(subtrack))
		    {
		    if (isWithCenterLabels(subtrack))
			y = doCenterLabels(subtrack, track, hvg, font, y);
		    else
			y += subtrack->totalHeight(subtrack,
						   subtrack->limitedVis);
		    }
            }
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
    for (track = trackList; track != NULL; track = track->next)
	{
	if (track->limitedVis == tvHide)
            continue;
        if (isCompositeTrack(track))
            {
            struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
            for (subtrack = track->subtracks; subtrack != NULL;
                         subtrack = subtrack->next)
                if (isSubtrackVisible(subtrack))
                    y = doDrawItems(subtrack, hvg, font, y, &lastTime);
            }
        else
            y = doDrawItems(track, hvg, font, y, &lastTime);
        }
}
/* if a track can draw its left labels, now is the time since it
 *	knows what exactly happened during drawItems
 */
if (withLeftLabels)
    {
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
	{
	if (track->limitedVis == tvHide)
            continue;
	if (isCompositeTrack(track))
	    {
	    struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
	    for (subtrack = track->subtracks; subtrack != NULL;
		 subtrack = subtrack->next)
		if (isSubtrackVisible(subtrack))
		    {
		    if (subtrack->drawLeftLabels != NULL)
			y = doOwnLeftLabels(subtrack, hvg, font, y);
		    else
			y += trackPlusLabelHeight(subtrack, fontHeight);
		    }
	    }
        else if (track->drawLeftLabels != NULL)
	    {
	    y = doOwnLeftLabels(track, hvg, font, y);
	    }
        else
            {
	    y += trackPlusLabelHeight(track, fontHeight);
            }
        }
    }


/* Make map background. */
y = yAfterRuler;
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->limitedVis != tvHide)
        y = doTrackMap(track, hvg, y, fontHeight, trackPastTabX, trackPastTabWidth);
    }

/* Finish map. */
hPrintf("</MAP>\n");

/* Save out picture and tell html file about it. */
hvGfxClose(&hvg);
hPrintf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s ",
    gifTn.forHtml, pixWidth, pixHeight, mapName);
hPrintf("><BR>\n");
}


static void printEnsemblAnchor(char *database, char* archive)
/* Print anchor to Ensembl display on same window. */
{
char *scientificName = hScientificName(database);
char *dir = ensOrgNameFromScientificName(scientificName);
struct dyString *ensUrl;
char *name;
int start, end;

if (sameWord(scientificName, "Takifugu rubripes"))
    {
    /* for Fugu, must give scaffold, not chr coordinates */
    /* Also, must give "chrom" as "scaffold_N", name below. */
    if (!hScaffoldPos(chromName, winStart, winEnd,
                        &name, &start, &end))
        /* position doesn't appear on Ensembl browser.
         * Ensembl doesn't show scaffolds < 2K */
        return;
    }
else
    {
    name = chromName;
    start = winStart;
    end = winEnd;
    }
start += 1;
ensUrl = ensContigViewUrl(dir, name, seqBaseCount, start, end, archive);
hPrintf("<A HREF=\"%s\" TARGET=_blank class=\"topbar\">", ensUrl->string);
/* NOTE: probably should free mem from dir and scientificName ?*/
dyStringFree(&ensUrl);
}

void makeHgGenomeTrackVisible(struct track *track)
/* This turns on a track clicked from hgGenome, even if it was previously */
/* hidden manually and there are cart vars to support that. */
{
struct hashEl *hels;
struct hashEl *hel;
char prefix[64];
/* First check if the click was from hgGenome.  If not, leave. */
if (!cgiVarExists("hgGenomeClick"))
    return;
/* get the names of the tracks in the cart */
safef(prefix, sizeof(prefix), "%s_", hggGraphPrefix);
hels = cartFindPrefix(cart, prefix);
/* loop through them and compare them to the track passed into this */
/* function. */
for (hel = hels; hel != NULL; hel = hel->next)
    {
    struct trackDb *subtrack;
    char *table = hel->val;
    /* check non-subtrack. */
    if (sameString(track->tdb->tableName, table))
	{
	track->visibility = tvFull;
	track->tdb->visibility = tvFull;
	cartSetString(cart, track->tdb->tableName, "full");
	}
    else if (track->tdb->subtracks != NULL)
	{
	for (subtrack = track->tdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
	    {
	    if (sameString(subtrack->tableName, table))
		{
		char selName[64];
		char selVal[2];
		track->visibility = tvFull;
		track->tdb->visibility = tvFull;
		cartSetString(cart, track->tdb->tableName, "full");
		subtrack->visibility = tvFull;
		safef(selName, sizeof(selName), "%s_sel", table);
		safef(selVal, sizeof(selVal), "%d", tvFull);
		cartSetString(cart, selName, selVal);
		}
	    }
	}
    }
hashElFreeList(&hels);
} 

void loadFromTrackDb(struct track **pTrackList)
/* Load tracks from database, consulting handler list. */
{
struct trackDb *tdb, *tdbList = NULL;
struct track *track;
TrackHandler handler;

tdbList = hTrackDb(chromName);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    track = trackFromTrackDb(tdb);
    track->hasUi = TRUE;
    if (slCount(tdb->subtracks) != 0)
        makeCompositeTrack(track, tdb);
    else
        {
        handler = lookupTrackHandler(tdb->tableName);
        if (handler != NULL)
            handler(track);
        }
    makeHgGenomeTrackVisible(track);
    if (track->loadItems == NULL)
        warn("No load handler for %s; possible missing trackDb `type' or `subTrack' attribute", tdb->tableName);
    else if (track->drawItems == NULL)
        warn("No draw handler for %s", tdb->tableName);
    else
        slAddHead(pTrackList, track);
    }
}

static int getScoreFilter(char *tableName)
/* check for score filter configuration setting */
{
char optionScoreStr[128];

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tableName);
return cartUsualInt(cart, optionScoreStr, 0);
}

void ctLoadSimpleBed(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct customTrack *ct = tg->customPt;
struct bed *bed, *nextBed, *list = NULL;
int scoreFilter = getScoreFilter(ct->tdb->tableName);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
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
    hFreeOrDisconnect(&conn);
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
int scoreFilter = getScoreFilter(ct->tdb->tableName);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
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
	    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeOrDisconnect(&conn);
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
		lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
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
int scoreFilter = getScoreFilter(ct->tdb->tableName);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
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
    hFreeOrDisconnect(&conn);
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
int scoreFilter = getScoreFilter(ct->tdb->tableName);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
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
	    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeOrDisconnect(&conn);
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
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;
    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	lfs = lfsFromColoredExonBed(bed);
	slAddHead(&lfsList, lfs);
	}
    hFreeOrDisconnect(&conn);
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
  static char buf[256];
  if (strlen(itemName) > 0)
      sprintf(buf, "%s %s", ctFileName, itemName);
  else
      sprintf(buf, "%s NoItemName", ctFileName);
  return buf;
}


void coloredExonMethodsFromCt(struct track *tg)
/* same as coloredExonMethods but different loader. */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = ctLoadColoredExon;
tg->canPack = TRUE;
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
    struct sqlConnection *conn = sqlCtConn(FALSE);
    if ((struct sqlConnection *)NULL == conn)
	errAbort("can not connect to customTracks DB");
    else
	hFreeOrDisconnect(&conn);
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
    }
else if (sameString(type, "wig"))
    {
    tg = trackFromTrackDb(tdb);
    if (ct->dbTrack)
	tg->loadItems = wigLoadItems;
    else
	tg->loadItems = ctWigLoadItems;
    tg->customPt = ct;
    }
else if (sameString(type, "bedGraph"))
    {
    tg = trackFromTrackDb(tdb);
    tg->canPack = FALSE;
    tg->customPt = ct;
    ct->wigFile = ctFileName;
    tg->mapItemName = ctMapItemName;
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
    tg->customPt = ct;
    }
else if (sameString(type, "chromGraph"))
    {
    tdb->type = NULL;	/* Swap out type for the moment. */
    tg = trackFromTrackDb(tdb);
    chromGraphMethodsCt(tg);
    tdb->type = typeOrig;
    }
else if (sameString(type, "array"))
    {
    tg = trackFromTrackDb(tdb);
    expRatioMethodsFromCt(tg);
    tg->customPt = ct;
    }
else if (sameString(type, "coloredExon"))
    {
    tg = trackFromTrackDb(tdb);
    coloredExonMethodsFromCt(tg);
    tg->customPt = ct;
    }
else
    {
    errAbort("Unrecognized custom graph type %s", type);
    }
tg->labelNextItemButtonable = FALSE;
tg->hasUi = TRUE;
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

ctList = customTracksParseCart(cart, &browserLines, &ctFileName);

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

void loadCustomTracks(struct track **pGroupList)
/* Load up custom tracks and append to list. */
{
struct customTrack *ct;
struct track *tg;
struct slName *bl;

/* build up browser lines from cart variables set by hgCustom */
char *visAll = cartCgiUsualString(cart, "hgt.visAllFromCt", NULL);
if (visAll)
    {
    char buf[64];
    safef(buf, sizeof buf, "browser %s %s", visAll, "all");
    slAddTail(&browserLines, slNameNew(buf));
    }
struct hashEl *visEl;
struct hashEl *visList = cartFindPrefix(cart, "hgtct.");
for (visEl = visList; visEl != NULL; visEl = visEl->next)
    {
    char buf[128];
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
		    for (tg = *pGroupList; tg != NULL; tg = tg->next)
		        {
			if (toAll || sameString(s, tg->mapName))
                            {
                            if (hTvFromString(command) == tg->tdb->visibility)
                                /* remove if setting to default vis */
                                cartRemove(cart, tg->mapName);
                            else
                                cartSetString(cart, tg->mapName, command);
                            /* hide or show supertrack enclosing this track */
                            if (tg->tdb->parentName)
                                cartSetString(cart, tg->tdb->parentName,
                                            (sameString(command, "hide") ? 
                                                "hide" : "show"));
                            }
                        }
		    }
		}
	    }
	else if (sameString(command, "position"))
	    {
	    if (wordCount < 3)
	        errAbort("Expecting 3 words in browser position line");
	    if (!hgIsChromRange(words[2])) 
	        errAbort("browser position needs to be in chrN:123-456 format");
	    hgParseChromRange(words[2], &chromName, &winStart, &winEnd);

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
    slAddHead(pGroupList, tg);
    }
}

boolean restrictionEnzymesOk()
/* Check to see if it's OK to do restriction enzymes. */
{
return (hTableExistsDb("hgFixed", "cutters") &&
	hTableExistsDb("hgFixed", "rebaseRefs") &&
	hTableExistsDb("hgFixed", "rebaseCompanies"));
}

void hotLinks()
/* Put up the hot links bar. */
{
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);
char *orgEnc = cgiEncode(organism);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#2636D1\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"../index.html?org=%s\" class=\"topbar\">Home</A></TD>", orgEnc);

if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Sequence View Gateway</A></TD>", orgEnc, database);
    hPrintf(
    "<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidTable?gsidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks\" class=\"topbar\">%s</A></TD>",
    '%', '%', "Select Subjects");
    } 
else
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Genomes</A></TD>", orgEnc, database);
    }
if (gotBlat)
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?%s\" class=\"topbar\">Blat</A></TD>", uiVars->string);
    }
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidTable?db=%s\" class=\"topbar\">%s</A></TD>",
       database, "Table View");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTables?db=%s&position=%s:%d-%d&%s=%u\" class=\"topbar\">%s</A></TD>",
       database, chromName, winStart+1, winEnd, cartSessionVarName(),
       cartSessionId(cart), "Tables");
    }
if (hgNearOk(database))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgNear?%s\" class=\"topbar\">%s</A></TD>",
                 uiVars->string, "Gene Sorter");
    }
if (hgPcrOk(database))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgPcr?%s\" class=\"topbar\">PCR</A></TD>", uiVars->string);
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\" class=\"topbar\">"
      " %s </A></TD>",  hgcNameAndSettings(),
      winStart, chromName, winStart, winEnd, database, uiVars->string, "DNA");

if (liftOverChainForDb(database) != NULL)
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"");
    hPrintf("../cgi-bin/hgConvert?%s&db=%s&position=%s:%d-%d", 
    	uiVars->string, database, chromName, winStart+1, winEnd);
    hPrintf("\" class=\"topbar\">Convert</A></TD>");
    }

/* see if hgFixed.trackVersion exists */
boolean trackVersionExists = hTableExistsDb("hgFixed", "trackVersion");
char ensVersionString[256];
char ensDateReference[256];
ensVersionString[0] = 0;
ensDateReference[0] = 0;
if (trackVersionExists)
    {
    struct sqlConnection *conn = hAllocConn();
    char query[256];
    safef(query, sizeof(query), "select version,dateReference from hgFixed.trackVersion where db = '%s' order by updateTime DESC limit 1", database);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;

    while ((row = sqlNextRow(sr)) != NULL)
        {
        safef(ensVersionString, sizeof(ensVersionString), "Ensembl %s",
                row[0]);
        safef(ensDateReference, sizeof(ensDateReference), "%s",
                row[1]);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

/* Print Ensembl anchor for latest assembly of organisms we have
 * supported by Ensembl (human, mouse, rat, fugu) */
if (ensVersionString[0] && !isUnknownChrom(database, chromName))
    {
    char *archive = NULL;
    if (ensDateReference[0] && differentWord("current", ensDateReference))
            archive = cloneString(ensDateReference);
    hPuts("<TD ALIGN=CENTER>");
    printEnsemblAnchor(database, archive);
    hPrintf("%s</A></TD>", "Ensembl");
    }

/* Print NCBI MapView anchor */
if (sameString(database, "hg18"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "mm8"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=10090&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "danRer2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7955&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "galGal3"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9031&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "canFam2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9615&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "rheMac2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9544&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "panTro2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9598&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "anoGam1"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7165&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "oryLat1"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://medaka.utgenome.org/browser_ens_jump.php?revision=version1.0&chr=chromosome%s&start=%d&end=%d\" TARGET=_blank class=\"topbar\">%s</A></TD>",
        skipChr(chromName), winStart+1, winEnd, "UTGB");
    }
if (sameString(database, "cb3"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://www.wormbase.org/db/seq/gbrowse/briggsae?name=%s:%d-%d\" TARGET=_blank class=\"topbar\">%s</A></TD>", 
        skipChr(chromName), winStart+1, winEnd, "WormBase");
    }
if (sameString(database, "ce4"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://ws170.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d\" TARGET=_blank class=\"topbar\">%s</A></TD>", 
        skipChr(chromName), winStart+1, winEnd, "WormBase");
    }
if (sameString(database, "ce2"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d\" TARGET=_blank class=\"topbar\">%s</A></TD>", 
        skipChr(chromName), winStart+1, winEnd, "WormBase");
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\" class=\"topbar\">%s</A></TD>\n",cartSessionVarName(),
       cartSessionId(cart), "PDF/PS");
if (wikiLinkEnabled())
    {
    printf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgSession?%s=%u"
	   "&hgS_doMainPage=1\" class=\"topbar\">Session</A></TD>",
	   cartSessionVarName(), cartSessionId(cart));
    }
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"/goldenPath/help/gsidTutorial.html#SequenceView\" TARGET=_blank class=\"topbar\">%s</A></TD>\n", "Help");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank class=\"topbar\">%s</A></TD>\n", "Help");
    }
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}

static void setSuperTrackHasVisibleMembers(struct trackDb *tdb)
/* Determine if any member tracks are visible -- currently 
 * recording this in the parent's visibility setting */
{
tdb->visibility = tvDense;
}

static boolean superTrackHasVisibleMembers(struct trackDb *tdb)
/* Determine if any member tracks are visible -- currently 
 * recording this in the parent's visibility setting */
{
if (!tdb->isSuper)
    return FALSE;
return (tdb->visibility != tvHide);
}

static void groupTracks(struct track **pTrackList, struct group **pGroupList,
                                int vis)
/* Make up groups and assign tracks to groups.
 * If vis is -1, restore default groups to tracks. */
{
struct group *unknown = NULL;
struct group *group, *list = NULL;
struct hash *hash = newHash(8);
struct track *track;
struct trackRef *tr;
struct grp* grps = hLoadGrps();
struct grp *grp;
char cartVar[512];

/* build group objects from database. */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    /* deal with group reordering */
    float priority = grp->priority;
    if (withPriorityOverride)
        {
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
    slAddHead(&list, group);
    hashAdd(hash, grp->name, group);
    }
grpFreeList(&grps);

/* Loop through tracks and fill in their groups. 
 * If necessary make up an unknown group. */
for (track = *pTrackList; track != NULL; track = track->next)
    {
    /* handle track reordering feature -- change group assigned to track */
    if (withPriorityOverride)
        {
        char *groupName = NULL;
        char cartVar[128];

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
        if (track->tdb->parent)
            {
            /* supertrack member must be in same group as its super */
            /* determine supertrack group */
            safef(cartVar, sizeof(cartVar), "%s.group",track->tdb->parentName);
            groupName = cloneString(
                    cartUsualString(cart, cartVar, track->tdb->parent->grp));
            track->tdb->parent->grp = cloneString(groupName);
            }
        else
            {
            /* get group */
            safef(cartVar, sizeof(cartVar), "%s.group",track->mapName);
            groupName = cloneString(
                    cartUsualString(cart, cartVar, track->defaultGroupName));
            }
        if (vis == -1)
            groupName = track->defaultGroupName;
        track->groupName = cloneString(groupName);
        if (sameString(groupName, track->defaultGroupName))
            cartRemove(cart, cartVar);

        /* get priority */
        safef(cartVar, sizeof(cartVar), "%s.priority",track->mapName);
        float priority = (float)cartUsualDouble(cart, cartVar, 
                                                    track->defaultPriority);
        /* remove cart variables that are the same as the trackDb settings */
        if (priority == track->defaultPriority)
            cartRemove(cart, cartVar);
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
    if (track->tdb->parent)
        {
        if (hTvFromString(cartUsualString(cart, track->mapName,
                        hStringFromTv(track->tdb->visibility))) != tvHide)
            setSuperTrackHasVisibleMembers(track->tdb->parent);
        if (hashLookup(superHash, track->tdb->parentName))
            /* ignore supertrack if it's already been handled */
            continue;
        /* create track and reference for the supertrack */
        struct track *superTrack = trackFromTrackDb(track->tdb->parent);
        superTrack->hasUi = TRUE;
        superTrack->group = group;
        superTrack->groupName = cloneString(group->name);
        superTrack->defaultGroupName = cloneString(group->name);

        /* handle track reordering */
        char cartVar[128];
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
        hashAdd(superHash, track->tdb->parentName, track->tdb->parent);
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
hButton(var, paddedLabel);
}

void limitSuperTrackVis(struct track *track)
/* Limit track visibility by supertrack parent */
{
struct trackDb *parent = track->tdb->parent;
if (!parent)
    return;
if (sameString("hide", cartUsualString(cart, parent->tableName,
                                parent->isShow ? "show" : "hide")))
    track->visibility = tvHide;
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
if (pcrResultParseCart(cart, NULL, NULL, NULL))
    slSafeAddHead(&trackList, pcrResultTg());
if (userSeqString != NULL) slSafeAddHead(&trackList, userPslTg());
slSafeAddHead(&trackList, oligoMatchTg());
if (restrictionEnzymesOk())
    {
    slSafeAddHead(&trackList, cuttersTg());
    }
if (wikiTrackEnabled(database, NULL))
    addWikiTrack(&trackList);
loadCustomTracks(&trackList);
groupTracks(&trackList, pGroupList, vis);

if (cgiOptionalString( "hideTracks"))
    changeTrackVis(groupList, NULL, tvHide);

/* Get visibility values if any from ui. */
for (track = trackList; track != NULL; track = track->next)
    {
    char *s = cartOptionalString(cart, track->mapName);
    if (cgiOptionalString("hideTracks"))
	{
	s = cgiOptionalString(track->mapName);
	if (s != NULL && (hTvFromString(s) != track->tdb->visibility))
            {
            cartSetString(cart, track->mapName, s);
            }
	}
    if (s != NULL)
	track->visibility = hTvFromString(s);
    if (isCompositeTrack(track) && track->visibility != tvHide)
	{
	struct trackDb *parent = track->tdb->parent;
	char *parentShow = NULL;
	if (parent)
	    parentShow = cartUsualString(cart, parent->tableName,
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
if (track->labelNextPrevItem != NULL)
    track->labelNextPrevItem(track, goNext);
}

char *collapseGroupVar(char *name)
/* Construct cart variable name for collapsing group */
{
static char varName[128];
safef(varName, sizeof(varName), 
        "%s%s_%s_%s", "hgt", "group", name, "close");
return (cloneString(varName));
}

boolean isCollapsedGroup(char *name)
/* Determine if group is collapsed */
{
return cartUsualInt(cart, collapseGroupVar(name), 0);
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
    printf("<tr style='display: %s' id='%s-%d'>", isOpen ? "" : "none", id, counter++);
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
if (measureTiming)
    uglyTime("Done with trackForm");
for (track = trackList; track != NULL; track = track->next)
    {
    char *cartVis = cartOptionalString(cart, track->mapName);
    if (cartVis != NULL && hTvFromString(cartVis) == track->tdb->visibility)
	cartRemove(cart, track->mapName);
    }
if (measureTiming)
    {
    uglyTime("Pruned redundant vis from cart");
    }
}

void doTrackForm(char *psOutput)
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
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
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackHeaderForm\" METHOD=GET>\n\n", hgTracksName());
cartSaveSession(cart);
clearButtonJavascript = "document.TrackHeaderForm.position.value=''";

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
    uglyTime("Time before getTrackList");
trackList = getTrackList(&groupList, defaultTracks ? -1 : -2);
if (measureTiming)
    uglyTime("getTrackList");

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

/* Tell tracks to load their items. */
for (track = trackList; track != NULL; track = track->next)
    {
    /* adjust track visibility based on supertrack just before load loop */
    if (track->tdb->parent)
        limitSuperTrackVis(track);

    /* remove cart priority variables if they are set  
       to the default values in the trackDb */
    if(!hTrackOnChrom(track->tdb, chromName)) 
	{
	track->limitedVis = tvHide;
	track->limitedVisSet = TRUE;
	}
    else if (track->visibility != tvHide)
	{
	if (measureTiming)
	    lastTime = clock1000();
	track->loadItems(track); 
	
	if (measureTiming)
	    {
	    thisTime = clock1000();
	    track->loadTime = thisTime - lastTime;
	    }
	}
    }

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
            boolean isOpen = !isCollapsedGroup(group->name);
            char buf[1000];
            safef(buf, sizeof(buf), "<input type='hidden' name=\"%s\" id=\"%s_%d\" value=\"%s\">\n", collapseGroupVar(group->name), collapseGroupVar(group->name), looper, isOpen ? "0" : "1");
            dyStringAppend(looper == 1 ? trackGroupsHidden1 : trackGroupsHidden2, buf);
            }
        }
    }

/* Center everything from now on. */
hPrintf("<CENTER>\n");


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
    hPrintf("<FONT SIZE=5><B>");
    if (startsWith("zoo",database) )
	{
	hPrintf("%s %s on %s June 2002 Assembly %s target1",
		organization, browserName, organism, freezeName); 
	}
    else
	{

	if (sameString(organism, "Archaea"))
	    hPrintf("%s %s on Archaeon %s Assembly", 
	    	organization, browserName, freezeName);
	else
	    hPrintf("%s %s on %s %s Assembly", 
	    	organization, browserName, organism, freezeName); 
	}
    hPrintf("</B></FONT><BR>\n");

    /* This is a clear submit button that browsers will use by default when enter is pressed in position box. */
    hPrintf("<INPUT TYPE=IMAGE BORDER=0 NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\">");
    /* Put up scroll and zoom controls. */
    hWrites("move ");
    hButton("hgt.left3", "<<<");
    hButton("hgt.left2", " <<");
    hButton("hgt.left1", " < ");
    hButton("hgt.right1", " > ");
    hButton("hgt.right2", ">> ");
    hButton("hgt.right3", ">>>");
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
    hWrites("<BR>\n");

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
	hPrintf("<INPUT TYPE=HIDDEN NAME=\"position\" "
		"VALUE=\"%s:%d-%d\">", chromName, winStart+1, winEnd);
        hPrintf("\n%s", trackGroupsHidden1->string);
	hPrintf("</CENTER></FORM>\n");
	hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" METHOD=POST>\n\n", hgTracksName());
        hPrintf(trackGroupsHidden2->string);
        freeDyString(&trackGroupsHidden1);
        freeDyString(&trackGroupsHidden2);
	cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
	clearButtonJavascript = "document.TrackForm.position.value=''";
	hPrintf("<CENTER>");
	}


    /* Make line that says position. */
	{
	char buf[256];
	char *survey = getCfgValue("HGDB_SURVEY", "survey");
        char *javascript = "onchange=\"document.location = '/cgi-bin/hgTracks?db=' + document.TrackForm.db.options[document.TrackForm.db.selectedIndex].value;\"";
        if (containsStringNoCase(database, "zoo"))
            {
            hPuts("Organism ");
            printAssemblyListHtmlExtra(database, javascript);
            }

	sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);
	position = cloneString(buf);
	hWrites("position/search ");
	hTextVar("position", addCommasToPos(position), 30);
	sprintLongWithCommas(buf, winEnd - winStart);
	hWrites(" ");
	hButton("submit", "jump");
	hOnClickButton(clearButtonJavascript,"clear");
	hPrintf(" size %s bp. ", buf);
        hButton("hgTracksConfigPage", "configure");
#define SURVEY 1
#ifdef SURVEY
        //hPrintf("&nbsp;&nbsp;<FONT SIZE=3><A STYLE=\"text-decoration:none; padding:2px; background-color:yellow; border:solid 1px\" HREF=\"http://www.surveymonkey.com/s.asp?u=881163743177\" TARGET=_BLANK><EM><B>Your feedback</EM></B></A></FONT>\n");
	if (survey && sameWord(survey, "on"))
	    hPrintf("&nbsp;&nbsp;<FONT SIZE=3><A STYLE=\"background-color:yellow;\" HREF=\"http://www.surveymonkey.com/s.asp?u=881163743177\" TARGET=_BLANK><EM><B>Take survey</EM></B></A></FONT>\n");
#endif
	hPutc('\n');

	}
    }

/* Make chromsome ideogram gif and map. */
makeChromIdeoImage(&trackList, psOutput);

/* Make clickable image and map. */
makeActiveImage(trackList, psOutput);
fflush(stdout);
if (!hideControls)
    {
    struct controlGrid *cg = NULL;

    hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1 WIDTH=%d COLS=%d><TR>\n", 
    	tl.picWidth, 27);
    hPrintf("<TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move start<BR>");
    hButton("hgt.dinkLL", " < ");
    hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
    hButton("hgt.dinkLR", " > ");
    hPrintf("</TD><TD COLSPAN=15 style=\"white-space:normal\">"); // allow this text to wrap
    hWrites("Click on a feature for details."
            "Click on base position to zoom in around cursor."
            "Click gray/blue bars on left for track options and descriptions." );
    hPrintf("</TD><TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move end<BR>");
    hButton("hgt.dinkRL", " < ");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButton("hgt.dinkRR", " > ");
    hPrintf("</TD></TR></TABLE>\n");
    // smallBreak();

    /* Display bottom control panel. */
    hButton("hgt.reset", "default tracks");
    if (showTrackControls)
	{
	hPrintf(" ");
	hButton("hgt.hideAll", "hide all");
	}

    hPrintf(" ");
    hOnClickButton("document.customTrackForm.submit();return false;",
                        hasCustomTracks ? 
                            CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
    
    hPrintf(" ");
    hButton("hgTracksConfigPage", "configure");
    hPrintf(" ");

    if (!hIsGsidServer())
	{
        hButton("hgt.toggleRevCmplDisp", "reverse");
        hPrintf(" ");
	}
    hButton("submit", "refresh");

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
    jsIncludeFile("hgTracks.js", NULL);
    hPrintf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
    hPrintf("<tr><td colspan='5' align='CENTER' nowrap>"
	   "Use drop-down controls below and press refresh to alter tracks "
	   "displayed.<BR>"
	   "Tracks with lots of items will automatically be displayed in "
	   "more compact modes.</td></tr>\n");
    if (!hIsGsidServer())
    	{
	cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
	}
    else
	{
	/* 4 cols fit GSID's display better */ 
    	cg = startControlGrid(MAX_CONTROL_COLUMNS-1, "left");
	}
    boolean isFirstNotCtGroup = TRUE;
    for (group = groupList; group != NULL; group = group->next)
        {
	if (group->trackList == NULL)
	    continue;

	struct trackRef *tr;

        /* check if group section should be displayed */
        char *otherState;
        char *indicator;
        char *indicatorImg;
        boolean isOpen = !isCollapsedGroup(group->name);
        collapseGroupGoodies(isOpen, TRUE, &indicatorImg, 
                                &indicator, &otherState);
	hPrintf("<TR>");
	cg->rowOpen = TRUE;
	if (!hIsGsidServer())
	    {
            hPrintf("<th align=\"left\" colspan=%d BGCOLOR=#536ED3>", 
	    	    MAX_CONTROL_COLUMNS);
	    }
	else
	    {
            hPrintf("<th align=\"left\" colspan=%d BGCOLOR=#536ED3>", 
	    	    MAX_CONTROL_COLUMNS-1);
	    }
	hPrintf("<table width='100%'><tr><td align='left'>");
	hPrintf("\n<A NAME=\"%sGroup\"></A>",group->name);
        hPrintf("<A HREF=\"%s?%s&%s=%s#%sGroup\" class='bigBlue'><IMG height='18' width='18' onclick=\"return toggleTrackGroupVisibility(this, '%s');\" id=\"%s_button\" src=\"%s\" alt=\"%s\" class='bigBlue'></A>&nbsp;&nbsp;",
                hgTracksName(), cartSidUrlString(cart), 
                collapseGroupVar(group->name),
                otherState, group->name, 
                group->name, group->name, indicatorImg, indicator);
	hPrintf("</td><td align='center' width='100%'>\n");
	hPrintf("<B>%s</B>", wrapWhiteFont(group->label));
	hPrintf("</td><td align='right'>\n");
	hPrintf("<input type='submit' name='submit' value='refresh'>\n");
	hPrintf("</td></tr></table></th>\n");
	controlGridEndRow(cg);

	/* First track group that is not custom track group gets ruler, 
         * unless it's collapsed. */
	if (!showedRuler && isFirstNotCtGroup && 
                differentString(group->name, "user"))
	    {
	    showedRuler = TRUE;
	    myControlGridStartCell(cg, isOpen, group->name);
            hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		    cartSessionVarName(), cartSessionId(cart),
		    chromName, RULER_TRACK_NAME);
	    hPrintf(" %s<BR> ", RULER_TRACK_LABEL);
            hPrintf("</A>");
	    hDropListClassWithStyle("ruler", rulerMenu, 
                    sizeof(rulerMenu)/sizeof(char *), rulerMenu[rulerMode],
                    rulerMode == tvHide ? "hiddenText" : "normalText",
                    TV_DROPDOWN_STYLE);
	    controlGridEndCell(cg);
	    }
        if (differentString(group->name, "user"))
            isFirstNotCtGroup = FALSE;

        /* Add supertracks to  track list, sort by priority and
         * determine if they have visible member tracks */
        groupTrackListAddSuper(cart, group);

        /* Display track controls */
	for (tr = group->trackList; tr != NULL; tr = tr->next)
	    {
            struct track *track = tr->track;
            if (track->tdb->parentName)
                /* don't display supertrack members */
                continue;
	    myControlGridStartCell(cg, isOpen, group->name);
	    if (track->hasUi)
		{
		char *encodedMapName = cgiEncode(track->mapName);
		hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		    cartSessionVarName(), cartSessionId(cart),
		    chromName, encodedMapName);
		freeMem(encodedMapName);
		}
            hPrintf(" %s", track->shortLabel);
            if (track->tdb->isSuper)
                hPrintf("...");
	    hPrintf("<BR> ");
	    if (track->hasUi)
		hPrintf("</A>");

	    if (hTrackOnChrom(track->tdb, chromName)) 
		{
                if (track->tdb->isSuper)
                    superTrackDropDown(cart, track->tdb,
                                        superTrackHasVisibleMembers(track->tdb));
                else
                    {
                    /* check for option of limiting visibility to one mode */
                    hTvDropDownClassVisOnly(track->mapName, track->visibility,
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
        {
	hPrintf("track, load time, draw time, total<BR>\n");
	for (track = trackList; track != NULL; track = track->next)
	    {
	    if (track->visibility == tvHide)
                continue;
            if (isCompositeTrack(track))
                {
                struct track *subtrack;
                for (subtrack = track->subtracks; subtrack != NULL; 
                                                    subtrack = subtrack->next)
                    if (isSubtrackVisible(subtrack))
                        hPrintf("%s, %d, %d, %d<BR>\n", subtrack->shortLabel, 
                                subtrack->loadTime, subtrack->drawTime,
				subtrack->loadTime + subtrack->drawTime);
                }
            else 
                {
	        hPrintf("%s, %d, %d, %d<BR>\n", 
			track->shortLabel, track->loadTime, track->drawTime,
			track->loadTime + track->drawTime);
                if (startsWith("wigMaf", track->tdb->type))
                  if (track->subtracks)
                      if (track->subtracks->loadTime)
                         hPrintf("&nbsp; &nbsp; %s wiggle, load %d<BR>\n", 
                            track->shortLabel, track->subtracks->loadTime);
                }
	    }
	}
    hPrintf("</DIV>\n");
    }
if (showTrackControls)
    hButton("submit", "refresh");
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

pruneRedundantCartVis(trackList);
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
return round(x*guideBases);
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
trashDirFile(&psTn, "hgt", "hgt", ".eps");
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");
doTrackForm(psTn.forCgi);
printf("<A HREF=\"%s\">Click here</A> "
       "to download the current browser graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
    printf("<A TARGET=_blank HREF=\"%s\">Click here</A> "
	   "to download the current browser graphic in PDF.", pdfFile);
    }
else
    printf("<BR><BR>PDF format not available");
freez(&pdfFile);
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
    errAbort("Please go back and enter a coordinate range in the \"position\" field.<br>For example: chr22:20100000-20200000.\n");
    }

chromName = NULL;
winStart = 0;
if (isGenome(position) || NULL ==
    (hgp = findGenomePos(position, &chromName, &winStart, &winEnd, cart)))
    {
    if (winStart == 0)	/* number of positions found */
	hgp = findGenomePos(defaultPosition, &chromName, &winStart, &winEnd,
			    cart);
    }

if (NULL != hgp && NULL != hgp->tableList && NULL != hgp->tableList->name)
    {
    char *trackName = hgp->tableList->name;
    char *parent = hGetParent(trackName);
    if (parent)
        trackName = cloneString(parent);
    char *vis = cartOptionalString(cart, trackName);
    if (vis == NULL || differentString(vis, "full"))
	cartSetString(cart, trackName, hTrackOpenVis(trackName));
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

seqBaseCount = hChromSize(chromName);
winBaseCount = winEnd - winStart;

/* Figure out basic dimensions of display.  This
 * needs to be done early for the sake of the
 * zooming and dinking routines. */
withIdeogram = cartUsualBoolean(cart, "ideogram", TRUE);
withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);
withCenterLabels = cartUsualBoolean(cart, "centerLabels", TRUE);
withGuidelines = cartUsualBoolean(cart, "guidelines", TRUE);
withNextItemArrows = cartUsualBoolean(cart, "nextItemArrows", FALSE);
withNextExonArrows = cartUsualBoolean(cart, "nextExonArrows", FALSE);
if (!hIsGsidServer())
    {
    revCmplDisp = cartUsualBooleanDb(cart, database, REV_CMPL_DISP, FALSE);
    }
withPriorityOverride = cartUsualBoolean(cart, configPriorityOverride, FALSE);
insideX = trackOffsetX();
insideWidth = tl.picWidth-gfxBorder-insideX;


baseShowPos = cartUsualBoolean(cart, BASE_SHOWPOS, FALSE);
baseShowAsm = cartUsualBoolean(cart, BASE_SHOWASM, FALSE);
baseShowScaleBar = cartUsualBoolean(cart, BASE_SCALE_BAR, FALSE);
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
    winStart -= 1000;
    winEnd += 1000;
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
    errAbort("Window out of range on %s", chromName);
/* Save computed position in cart. */
sprintf(newPos, "%s:%d-%d", chromName, winStart+1, winEnd);
cartSetString(cart, "org", organism);
cartSetString(cart, "db", database);
cartSetString(cart, "position", newPos);
if (cgiVarExists("hgt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL);
}

void chromInfoTotalRow(long long total)
/* Make table row with total size from chromInfo. */
{
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
printf("Total");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
printLongWithCommas(stdout, total);
cgiTableFieldEnd();
cgiTableRowEnd();
}

void chromInfoRowsChrom()
/* Make table rows of chromosomal chromInfo name & size, sorted by name. */
{
struct slName *chromList = hAllChromNames();
struct slName *chromPtr = NULL;
long long total = 0;

slSort(&chromList, chrSlNameCmp);
for (chromPtr = chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    unsigned size = hChromSize(chromPtr->name);
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
chromInfoTotalRow(total);
slFreeList(&chromList);
}

void chromInfoRowsNonChrom(int limit)
/* Make table rows of non-chromosomal chromInfo name & size, sorted by size. */
{
struct sqlConnection *conn = hAllocConn();
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
    chromInfoTotalRow(total);
    }
else
    {
    safef(msg1, sizeof(msg1), "Limit reached");
    safef(msg2, sizeof(msg2), "%d rows displayed", limit);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf(msg1);
    cgiTableFieldEnd();
    cgiSimpleTableFieldStart();
    printf(msg2);
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
	printf(msg1);
	cgiTableFieldEnd();
	cgiSimpleTableFieldStart();
	printf(msg2);
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
char *defaultChrom = hDefaultChrom();
struct dyString *title = dyStringNew(512);
dyStringPrintf(title, "%s %s (%s) Browser Sequences",
	       hOrganism(database), hFreezeFromDb(database), database);
webStartWrapperDetailedNoArgs(cart, "", title->string, FALSE, FALSE, FALSE, FALSE);
printf("<FORM ACTION=\"%s\" NAME=\"posForm\" METHOD=GET>\n", hgTracksName());
cartSaveSession(cart);

puts("Enter a position, or click on a sequence name to view the entire "
     "sequence in the genome browser.<P>");
puts("position ");
hTextVar("position", addCommasToPos(position), 30);
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
    hChromCount() < 100)
    chromInfoRowsChrom();
else
    chromInfoRowsNonChrom(1000);

hTableEnd();

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

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char *debugTmp = NULL;
/* Uncomment this to see parameters for debugging. */
/* struct dyString *state = NULL; */
/* Initialize layout and database. */
cart = theCart;

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

hSetDb(database);

hDefaultConnect();
initTl();

char *configPageCall = cartCgiUsualString(cart, "hgTracksConfigPage", "notSet");
/* Do main display. */
if (cartVarExists(cart, "chromInfoPage"))
    {
    cartRemove(cart, "chromInfoPage");
    chromInfoPage();
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
    struct grp *grp = NULL, *grps = hLoadGrps();
    for (grp = grps; grp != NULL; grp = grp->next)
        collapseGroup(grp->name, TRUE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configShowAllGroups))
    {
    cartRemove(cart, configShowAllGroups);
    struct grp *grp = NULL, *grps = hLoadGrps();
    for (grp = grps; grp != NULL; grp = grp->next)
        collapseGroup(grp->name, FALSE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configHideEncodeGroups))
    {
    /* currently not used */
    cartRemove(cart, configHideEncodeGroups);
    struct grp *grp = NULL, *grps = hLoadGrps();
    for (grp = grps; grp != NULL; grp = grp->next)
        if (startsWith("encode", grp->name))
            collapseGroup(grp->name, TRUE);
    configPageSetTrackVis(-2);
    }
else if (cartVarExists(cart, configShowEncodeGroups))
    {
    /* currently not used */
    cartRemove(cart, configShowEncodeGroups);
    struct grp *grp = NULL, *grps = hLoadGrps();
    for (grp = grps; grp != NULL; grp = grp->next)
        if (startsWith("encode", grp->name))
            collapseGroup(grp->name, FALSE);
    configPageSetTrackVis(-2);
    }
else
    {
    tracksDisplay();
    }
}

void doDown(struct cart *cart)
{
printf("<H2>The Browser is Being Updated</H2>\n");
printf("The browser is currently unavailable.  We are in the process of\n");
printf("updating the database and the display software with a number of\n");
printf("new tracks, including some gene predictions.  Please try again tomorrow.\n");
}

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart 
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "hgt.reset",
			"hgt.in1", "hgt.in2", "hgt.in3", "hgt.inBase",
			"hgt.out1", "hgt.out2", "hgt.out3",
			"hgt.left1", "hgt.left2", "hgt.left3", 
			"hgt.right1", "hgt.right2", "hgt.right3", 
			"hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
			"hgt.tui", "hgt.hideAll", "hgt.visAllFromCt", 
                        "hgt.psOutput", "hideControls", "hgt.toggleRevCmplDisp",
			NULL };

int main(int argc, char *argv[])
{
enteredMainTime = clock1000();
uglyTime(NULL);
browserName = (hIsPrivateHost() ? "Test Browser" : "Genome Browser");
organization = (hIsMgcServer() ? "MGC/ORFeome" : "UCSC");

/* change title if this is for GSID */
browserName = (hIsGsidServer() ? "Sequence View" : browserName);
organization = (hIsGsidServer() ? "GSID" : organization);

/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which 
 * somehow can't be moved effectively into doMiddle. */
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
htmlSetStyle("<LINK REL=\"STYLESHEET\" HREF=\"../style/HGStyle.css\" TYPE=\"text/css\">\n"); 
oldVars = hashNew(10);
if (hIsGsidServer())
	cartHtmlShell("GSID Sequence View", doMiddle, hUserCookie(), excludeVars, oldVars);
else
	cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);
if (measureTiming)
    {
    fprintf(stdout, "Overall total time: %ld millis<BR>\n",
	clock1000() - enteredMainTime);
    }
return 0;
}
