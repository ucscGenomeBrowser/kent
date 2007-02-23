/* cgapSageTrack - module for CGAP SAGE track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "hCommon.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"

static int grayIxForCgap(double tpm)
/* Return a grayIx based on the score. */
{
int val = (int)ceil(tpm);
return grayInRange(val, 0, 150);
}

static struct hash *libTissueHash(struct sqlConnection *conn)
/* Read two columns of a table and hash em up. */
{
struct hash *ret = newHash(9);
struct sqlResult *sr = NULL;
char query[40] = "select libId,tissue from cgapSageLib";
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(ret, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
return ret;
}

struct cgapSageTpmHashEl 
/* A convenience struct for computing means. */
    {
    double total;
    int count;
    };

static boolean keepThisLib(char *tissue, char *libId)
{
char *tissueHl = cartUsualString(cart, "cgapSage.tissueHl", "All");
char *libHl = cartUsualString(cart, "cgapSage.libHl", "All");
if (!tissue || !libId)
    errAbort("NULL tissue or libId passed into keepThisLib()");
if (sameString(tissueHl, "All") && sameString(libHl, "All"))
    return TRUE;
if (sameString(tissue, tissueHl) || sameString(libId, libHl))
    return TRUE;
return FALSE;
}

static struct hash *combineCgapSages(struct cgapSage *tag, struct hash *libHash)
/* Go through the each lib for a tag and combine it's score using a hash for */
/* repeated tissues. */
{
struct hash *tpmHash = newHash(5);
int i;
for (i = 0; i < tag->numLibs; i++)
    {
    char libId[16];
    char *libName;
    struct cgapSageTpmHashEl *tpm;
    safef(libId, sizeof(libId), "%d", tag->libIds[i]);
    libName = hashMustFindVal(libHash, libId);
    tpm = hashFindVal(tpmHash, libName);
    if (keepThisLib(libName, libId))
	{
	if (tpm)
	    {
	    tpm->count++;
	    tpm->total += tag->tagTpms[i];
	    }
	else
	    {
	    AllocVar(tpm);
	    tpm->count = 1;
	    tpm->total = tag->tagTpms[i];
	    hashAdd(tpmHash, libName, tpm);
	    }
	}
    }
return tpmHash;
}

static struct linkedFeatures *skeletonLf(struct cgapSage *tag)
/* Fill in everything except the name and score. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
AllocVar(lf);
AllocVar(sf);
lf->start = sf->start = tag->chromStart;
lf->end = sf->end = tag->chromEnd;
lf->tallStart = tag->thickStart;
lf->tallEnd = tag->thickEnd;
lf->orientation = orientFromChar(tag->strand[0]);
lf->components = sf;
return lf;
}

static void addSimpleFeature(struct linkedFeatures *lf)
/* Make a dumb simpleFeature out of a single linkedFeatures. */
{
struct simpleFeature *sf;
AllocVar(sf);
sf->start = lf->start;
sf->end = lf->end;
sf->grayIx = lf->grayIx;
lf->components = sf;
}

static char *cgapSageMapItemName(struct track *tg, void *item)
/* Use what's in the ->extra for the name. */
{
struct linkedFeatures *lf = item;
return (char *)lf->extra;
}

static struct linkedFeatures *cgapSageToLinkedFeatures(struct cgapSage *tag, 
                  struct hash *libHash, enum trackVisibility vis)
/* Convert a single CGAP tag to a list of linkedFeatures. */
{
struct linkedFeatures *libList = NULL;
struct linkedFeatures *skel = skeletonLf(tag);
int i;
if (vis == tvPack)
    {
    /* If it's pack mode, average tissues into one linkedFeature. */
    struct hash *tpmHash = combineCgapSages(tag, libHash);
    struct hashEl *tpmList = hashElListHash(tpmHash);
    struct hashEl *tpmEl;
    slSort(&tpmList, slNameCmp);
    for (tpmEl = tpmList; tpmEl != NULL; tpmEl = tpmEl->next)
	{
	struct linkedFeatures *tiss = CloneVar(skel);
	struct cgapSageTpmHashEl *tpm = (struct cgapSageTpmHashEl *)tpmEl->val;
	safef(tiss->name, sizeof(tiss->name), "%s (%d)", tpmEl->name, tpm->count);
	tiss->grayIx = grayIxForCgap(tpm->total/(double)tpm->count);
	tiss->extra = cloneString(tpmEl->name);
	addSimpleFeature(tiss);
	slAddHead(&libList, tiss);
	}
    hashElFreeList(&tpmList);
    freeHashAndVals(&tpmHash);
    }
else 
    {
    for (i = 0; i < tag->numLibs; i++)
	{
	struct linkedFeatures *lf;
	char libId[16];
	char extra[32];
	char *libName;
	safef(libId, sizeof(libId), "%d", tag->libIds[i]);
	libName = hashMustFindVal(libHash, libId);
	if (keepThisLib(libName, libId))
	    {
	    lf = CloneVar(skel);
	    safef(lf->name, sizeof(lf->name), "%s", libName);
	    safef(extra, sizeof(extra), "libId.%s", libId);
	    lf->grayIx = grayIxForCgap(tag->tagTpms[i]);
	    lf->extra = cloneString(extra);
	    addSimpleFeature(lf);	
	    slAddHead(&libList, lf);
	    }
	}
    }
slReverse(&libList);
return libList;
}

void cgapSageLoadItems(struct track *tg)
/* This function loads the beds in the current window into a linkedFeatures list. */
/* Each bed entry may turn into multiple linkedFeatures because one is made for */
/* each library at a given tag (bed). */
{
struct linkedFeatures *itemList = NULL;
struct sqlConnection *conn = hAllocConn();
struct hash *libHash = libTissueHash(conn);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
sr = hOrderedRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
			NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cgapSage *tag = cgapSageLoad(row+rowOffset);
    struct linkedFeatures *oneLfList = cgapSageToLinkedFeatures(tag, libHash, tg->visibility);
    itemList = slCat(itemList, oneLfList);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = itemList;
}

/***** All the drawing/color-related stuff. ******/

static Color cgapShadesOfRed[10+1];
/* This will go from white to darker red. */

static Color cgapSageItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw CGAP SAGE item */
{
struct linkedFeatures *thisItem = item;
return cgapShadesOfRed[thisItem->grayIx];
}

void cgapSageDrawItems(struct track *tg, 
        int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Initialize the colors, then do the normal drawing. */
{
static struct rgbColor lowerColor = {205, 191, 191};
static struct rgbColor cgapRed = {205, 0, 0};
vgMakeColorGradient(vg, &lowerColor, &cgapRed, 10, cgapShadesOfRed);
genericDrawItems(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}

void cgapSageMethods(struct track *tg)
/* Make track for simple repeats. */
{
linkedFeaturesMethods(tg);
tg->loadItems = cgapSageLoadItems;
tg->itemColor = cgapSageItemColor;
tg->drawItems = cgapSageDrawItems;
tg->mapItemName = cgapSageMapItemName;
}
