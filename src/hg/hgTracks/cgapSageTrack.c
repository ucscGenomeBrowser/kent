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
    long freqTotal;
    long libTotals;
    int count;
    };

static boolean keepThisLib(char *tissue, char *libId)
/* Tissue filtering code checks cart. */
{
char *tissueHl = cartUsualString(cart, "cgapSage.tissueHl", "All");
if (!tissue)
    errAbort("NULL tissue or libId passed into keepThisLib()");
if (sameString(tissueHl, "All") || sameString(tissue, tissueHl))
    return TRUE;
return FALSE;
}

static struct hash *combineCgapSages(struct cgapSage *tag, struct hash *libHash, struct hash *libTotHash)
/* Go through the each lib for a tag and combine it's score using a hash for */
/* repeated tissues. */
{
struct hash *tpmHash = newHash(5);
int i;
for (i = 0; i < tag->numLibs; i++)
    {
    char libId[16];
    char *libName;
    int libTotTags = 0;
    struct cgapSageTpmHashEl *tpm;
    safef(libId, sizeof(libId), "%d", tag->libIds[i]);
    libName = hashMustFindVal(libHash, libId);
    tpm = hashFindVal(tpmHash, libName);
    libTotTags = hashIntVal(libTotHash, libId);
    if (keepThisLib(libName, libId))
	{
	if (tpm)
	    {
	    tpm->count++;
	    tpm->freqTotal += tag->freqs[i];
	    tpm->total += tag->tagTpms[i];
	    tpm->libTotals += libTotTags;
	    }
	else
	    {
	    AllocVar(tpm);
	    tpm->count = 1;
	    tpm->total = tag->tagTpms[i];
	    tpm->freqTotal = tag->freqs[i];
	    tpm->libTotals = libTotTags;
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

static void cgapSageMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, char *mapItemName, int start, int end,
			    int x, int y, int width, int height)
{
struct linkedFeatures *lf = item;
mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, 
                  tg->mapName, itemName, NULL, NULL, TRUE,
                  (char *)lf->extra);
}

int cgapLinkedFeaturesCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return (int)b->score - (int)a->score;
}

static struct linkedFeatures *cgapSageToLinkedFeatures(struct cgapSage *tag, 
                  struct hash *libHash, struct hash *libTotHash, enum trackVisibility vis)
/* Convert a single CGAP tag to a list of linkedFeatures. */
{
struct linkedFeatures *libList = NULL;
struct linkedFeatures *skel = skeletonLf(tag);
int i;
if (vis == tvDense)
    /* Just use the skeleton one. */
    {
    int tagTotal = 0;
    int freqTotal = 0;
    int libsUsed = 0;
    for (i = 0; i < tag->numLibs; i++)
	{
	char libId[16];
	char *libName;
	safef(libId, sizeof(libId), "%d", tag->libIds[i]);
	libName = hashMustFindVal(libHash, libId);
	if (keepThisLib(libName, libId))
	    {
	    int libTotal = hashIntVal(libTotHash, libId);
	    tagTotal += libTotal;
	    freqTotal += tag->freqs[i];
	    libsUsed++;
	    }
	}
    if (libsUsed > 0)
	{
	safef(skel->name, sizeof(skel->name), "whatever");
	skel->score = (float)((double)freqTotal * (1000000/tagTotal));
	skel->grayIx = grayIxForCgap(skel->score);
	addSimpleFeature(skel);
	libList = skel;
	}
    }
else if (vis == tvPack)
    {
    /* If it's pack mode, average tissues into one linkedFeature. */
    struct hash *tpmHash = combineCgapSages(tag, libHash, libTotHash);
    struct hashEl *tpmList = hashElListHash(tpmHash);
    struct hashEl *tpmEl;
    slSort(&tpmList, slNameCmp);
    for (tpmEl = tpmList; tpmEl != NULL; tpmEl = tpmEl->next)
	{
	struct linkedFeatures *tiss = CloneVar(skel);
	struct cgapSageTpmHashEl *tpm = (struct cgapSageTpmHashEl *)tpmEl->val;
	char link[256];
	char *encTissName = NULL;
	double score = 0;
	safef(tiss->name, sizeof(tiss->name), "%s (%d)", tpmEl->name, tpm->count);
	encTissName = cgiEncode(tpmEl->name);
	safef(link, sizeof(link), "i=%s&tiss=%s", tag->name, encTissName);
	score = (double)tpm->freqTotal*(1000000/(double)tpm->libTotals);
	tiss->score = (float)score;
	tiss->grayIx = grayIxForCgap(score);
	tiss->extra = cloneString(link);
	freeMem(encTissName);
	addSimpleFeature(tiss);
	slAddHead(&libList, tiss);
	}
    hashElFreeList(&tpmList);
    freeHashAndVals(&tpmHash);
    }
else 
    /* full mode */
    {
    for (i = 0; i < tag->numLibs; i++)
	{
	char libId[16];
	char *libName;
	char link[256];
	struct linkedFeatures *lf;
	safef(libId, sizeof(libId), "%d", tag->libIds[i]);
	libName = hashMustFindVal(libHash, libId);
	if (keepThisLib(libName, libId))
	    {	    
	    lf = CloneVar(skel);
	    safef(lf->name, sizeof(lf->name), "%s", libName);
	    safef(link, sizeof(link), "i=%s&lib=%s", tag->name, libId);
	    lf->score = (float)tag->tagTpms[i];
	    lf->grayIx = grayIxForCgap(tag->tagTpms[i]);
	    lf->extra = cloneString(link);
	    addSimpleFeature(lf);	
	    slAddHead(&libList, lf);
	    }
	}
    }
slSort(&libList, cgapLinkedFeaturesCmp);
slReverse(&libList);
return libList;
}

struct hash *getTotTagsHashFromTable(struct sqlConnection *conn)
/* Load the cgapSageLib table for the db then call getTotTagsHash. */
{
struct cgapSageLib *libs = cgapSageLibLoadByQuery(conn, "select * from cgapSageLib");
struct hash *libTotHash = getTotTagsHash(libs);
cgapSageLibFreeList(&libs);
return libTotHash;
}

void cgapSageLoadItems(struct track *tg)
/* This function loads the beds in the current window into a linkedFeatures list. */
/* Each bed entry may turn into multiple linkedFeatures because one is made for */
/* each library at a given tag (bed). */
{
struct linkedFeatures *itemList = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct hash *libHash = libTissueHash(conn);
struct hash *libTotHash = getTotTagsHashFromTable(conn);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
sr = hOrderedRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
			NULL, &rowOffset);
if ((winEnd - winStart) > CGAP_SAGE_DENSE_GOVERNOR)
    tg->visibility = tvDense;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cgapSage *tag = cgapSageLoad(row+rowOffset);
    struct linkedFeatures *oneLfList = cgapSageToLinkedFeatures(tag, libHash, libTotHash, tg->visibility);
    itemList = slCat(oneLfList, itemList);
    }
slReverse(&itemList);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = itemList;
}

/***** All the drawing/color-related stuff. ******/

static Color cgapShadesOfRed[10+1];
/* This will go from white to darker red. */

static Color cgapSageItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw CGAP SAGE item */
{
struct linkedFeatures *thisItem = item;
return cgapShadesOfRed[thisItem->grayIx];
}

void cgapSageDrawItems(struct track *tg, 
        int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Initialize the colors, then do the normal drawing. */
{
static struct rgbColor lowerColor = {205, 191, 191};
static struct rgbColor cgapRed = {205, 0, 0};
hvGfxMakeColorGradient(hvg, &lowerColor, &cgapRed, 10, cgapShadesOfRed);
genericDrawItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void cgapSageMethods(struct track *tg)
/* Make track for simple repeats. */
{
linkedFeaturesMethods(tg);
tg->loadItems = cgapSageLoadItems;
tg->itemColor = cgapSageItemColor;
tg->drawItems = cgapSageDrawItems;
tg->mapItemName = cgapSageMapItemName;
tg->mapItem = cgapSageMapItem;
}
