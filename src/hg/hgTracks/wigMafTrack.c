/* wigMafTrack - display multiple alignment files with score wiggle
 * and base-level alignment, or else density plot of pairwise alignments
 * (zoomed out) */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"
#include "wiggle.h"
#include "hCommon.h"
#include "hgMaf.h"
#include "mafTrack.h"
#ifndef GBROWSE
#include "customTrack.h"
#endif /* GBROWSE */
#include "mafSummary.h"
#include "mafFrames.h"
#include "phyloTree.h"

static char const rcsid[] = "$Id: wigMafTrack.c,v 1.129.2.1 2008/06/07 01:45:42 angie Exp $";

#ifdef GBROWSE
boolean isCustomTrack(char *in)
/* Stub routine for GBrowse -- custom track code is excluded. */
{
return FALSE;
}
#endif /* GBROWSE */

#define GAP_ITEM_LABEL  "Gaps"
#define MAX_SP_SIZE 2000

struct wigMafItem
/* A maf track item -- 
 * a line of bases (base level) or pairwise density gradient (zoomed out). */
    {
    struct wigMafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int group;          /* number of species group/clade */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    int inserts[128];
    int insertsSize;
    int seqEnds[128];
    int seqEndsSize;
    int brackStarts[128];
    int brackStartsSize;
    int brackEnds[128];
    int brackEndsSize;
    };

static void wigMafItemFree(struct wigMafItem **pEl)
/* Free up a wigMafItem. */
{
struct wigMafItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void wigMafItemFreeList(struct wigMafItem **pList)
/* Free a list of dynamically allocated wigMafItem's */
{
struct wigMafItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigMafItemFree(&el);
    }
*pList = NULL;
}

Color wigMafItemLabelColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw a maf item based on the species group it is in */
{
struct wigMafItem *mi = (struct wigMafItem *)item;
if (sameString(mi->name, GAP_ITEM_LABEL))
    return getOrangeColor();
return (((struct wigMafItem *)item)->group % 2 ? 
                        tg->ixAltColor : tg->ixColor);
}

static struct mafAli *wigMafLoadInRegion(struct sqlConnection *conn, 
    struct sqlConnection *conn2, char *table, char *chrom, int start, int end)
/* Load mafs from region */
{
    return mafLoadInRegion2(conn, conn2, table, chrom, start, end);
}

static struct wigMafItem *newMafItem(char *s, int g, boolean lowerFirstChar)
/* Allocate and initialize a maf item. Species param can be a db or name */
{
struct wigMafItem *mi;
char *val;

AllocVar(mi);
if ((val = hGenome(s)) != NULL)
    {
    /* it's a database name */
    mi->db = cloneString(s);
    mi->name = val;
    }
else
    {
    mi->db = cloneString(s);
    mi->name = cloneString(s);
    }
mi->name = hgDirForOrg(mi->name);
if (lowerFirstChar)
    *mi->name = tolower(*mi->name);
mi->height = tl.fontHeight;
mi->group = g;
return mi;
}

struct wigMafItem *getSpeciesFromMaf(struct track *track, int height)
{
struct wigMafItem *mi = NULL, *miList = NULL;
struct hash *hash = newHash(8);	/* keyed by database. */
struct mafPriv *mp = getMafPriv(track);
struct mafAli *maf;
char buf[64];
char *otherOrganism;

for (maf = mp->list; maf != NULL; maf = maf->next)
    {
    struct mafComp *mc;
    for (mc = maf->components; mc != NULL; mc = mc->next)
	{
	mafSrcDb(mc->src, buf, sizeof(buf));
	if (sameString(buf, database))
	    continue;
	if (hashLookup(hash, buf) == NULL)
	    {
	    AllocVar(mi);
	    mi->db = cloneString(buf);
	    otherOrganism = hOrganism(mi->db);
	    mi->name = 
		(otherOrganism == NULL ? cloneString(buf) : otherOrganism);

	    mi->height = tl.fontHeight;
	    slAddHead(&miList, mi);
	    hashAdd(hash, mi->db, mi);
	    }
	}
    }
hashFree(&hash);

return miList;
}

struct wigMafItem *newSpeciesItems(struct track *track, int height)
/* Make up item list for all species configured in track settings */
{
struct dyString *order = dyStringNew(256);
char option[MAX_SP_SIZE];
char *species[MAX_SP_SIZE];
char *groups[20];
char *defaultOff[MAX_SP_SIZE];
char sGroup[24];
struct wigMafItem *mi = NULL, *miList = NULL;
int group;
int i;
int speciesCt = 0, groupCt = 1;
int speciesOffCt = 0;
struct hash *speciesOffHash = newHash(0);
char buffer[128];
char *speciesTarget = trackDbSetting(track->tdb, SPECIES_TARGET_VAR);
char *speciesTree = trackDbSetting(track->tdb, SPECIES_TREE_VAR);
bool useTarg;	/* use phyloTree to find shortest path */
struct phyloTree *tree = NULL;
char *speciesUseFile = trackDbSetting(track->tdb, SPECIES_USE_FILE);
char *msaTable = NULL;

/* either speciesOrder or speciesGroup is specified in trackDb */
char *speciesOrder = trackDbSetting(track->tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(track->tdb, SPECIES_GROUP_VAR);
char *speciesOff = trackDbSetting(track->tdb, SPECIES_DEFAULT_OFF_VAR);

bool lowerFirstChar = TRUE;
char *firstCase;

firstCase = trackDbSetting(track->tdb, ITEM_FIRST_CHAR_CASE);
if (firstCase != NULL)
    {
    if (sameWord(firstCase, "noChange")) lowerFirstChar = FALSE;
    }

safef(buffer, sizeof(buffer), "%s.vis",track->mapName);
if (!cartVarExists(cart, buffer) && (speciesTarget != NULL))
    useTarg = TRUE;
else
    {
    char *val;

    val = cartUsualString(cart, buffer, "useCheck");
    useTarg = sameString("useTarg",val);
    }

if (useTarg && (tree = phyloParseString(speciesTree)) == NULL)
    useTarg = FALSE;

if (speciesOrder == NULL && speciesGroup == NULL && speciesUseFile == NULL)
    return getSpeciesFromMaf(track, height);

if (speciesGroup)
    groupCt = chopLine(cloneString(speciesGroup), groups);

if (speciesUseFile)
    {
    if ((speciesGroup != NULL) || (speciesOrder != NULL))
	errAbort("Can't specify speciesUseFile and speciesGroup or speciesOrder");
    if (hIsGsidServer())
	{
	msaTable = trackDbSetting(track->tdb, "msaTable");
    	if (msaTable != NULL)
	    {
	    speciesOrder = cartGetOrderFromFileAndMsaTable(cart, speciesUseFile, msaTable);
    	    }
	else
	    {
    	    speciesOrder = cartGetOrderFromFile(cart, speciesUseFile);
	    }
	}
    else
	{
    	speciesOrder = cartGetOrderFromFile(cart, speciesUseFile);
    	}
    speciesOff = NULL;
    }

/* keep track of species configured off initially for track */
if (speciesOff)
    {
    speciesOffCt = chopLine(cloneString(speciesOff), defaultOff);
    for (i = 0; i < speciesOffCt; i++)
        hashAdd(speciesOffHash, defaultOff[i], NULL);
    }

/* Make up items for other organisms by scanning through group & species 
   track settings */
for (group = 0; group < groupCt; group++)
    {
    if (groupCt != 1 || !speciesOrder)
        {
        safef(sGroup, sizeof sGroup, "%s%s", 
                                SPECIES_GROUP_PREFIX, groups[group]);
        speciesOrder = trackDbRequiredSetting(track->tdb, sGroup);
        }
    if (useTarg)
	{
	char *ptr, *path;
	struct hash *orgHash = newHash(0);
	int numNodes, ii;
	char *nodeNames[512];
	char *species = NULL;
	char *lowerString;

	path = phyloNodeNames(tree);
	numNodes = chopLine(path, nodeNames);
	for(ii=0; ii < numNodes; ii++)
	    {
	    if ((ptr = hOrganism(nodeNames[ii])) != NULL)
		{
		ptr[0] = tolower(ptr[0]);
		hashAdd(orgHash, ptr, nodeNames[ii]);
		}
	    else
		{
		hashAdd(orgHash, nodeNames[ii], nodeNames[ii]);
		}
	    }

	lowerString = cartUsualString(cart, SPECIES_HTML_TARGET,speciesTarget);
	lowerString[0] = tolower(lowerString[0]);
	species = hashFindVal(orgHash, lowerString);
	if ((ptr = phyloFindPath(tree, database, species)) != NULL)
	    speciesOrder = ptr;
	}


    safef(option, sizeof(option), "%s.speciesOrder", track->mapName);
    speciesCt = chopLine(cloneString(speciesOrder), species);
    for (i = 0; i < speciesCt; i++)
        {
	if (!useTarg)
	    {
	    /* skip this species if UI checkbox was unchecked */
	    safef(option, sizeof(option), "%s.%s", track->mapName, species[i]);
	    if (!cartVarExists(cart, option))
		if (hashLookup(speciesOffHash, species[i]))
		    cartSetBoolean(cart, option, FALSE);
	    if (!cartUsualBoolean(cart, option, TRUE))
		continue;
	    }
        mi = newMafItem(species[i], group, lowerFirstChar);
        slAddHead(&miList, mi);
        }
    }

slReverse(&miList);
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->height = height;
    dyStringPrintf(order, "%s ",mi->db);
    }
safef(option, sizeof(option), "%s.speciesOrder", track->mapName);
cartSetString(cart, option, order->string);
slReverse(&miList);

return miList;
}

static struct wigMafItem *scoreItem(int scoreHeight, char *label)
/* Make up item that will show the score */
{
struct wigMafItem *mi;

AllocVar(mi);
mi->name = cloneString(label);
mi->height = scoreHeight;
return mi;
}

struct mafPriv *getMafPriv(struct track *track)
{
struct mafPriv *mp = track->customPt;

if (mp == NULL)
    {
    AllocVar(mp);
    track->customPt = mp;
    }

return mp;
}

static void loadMafsToTrack(struct track *track)
/* load mafs in region to track custom pointer */
{
struct sqlConnection *conn;
struct sqlConnection *conn2;
struct mafPriv *mp = getMafPriv(track);

if (winBaseCount > MAF_SUMMARY_VIEW)
    return;

/* we open two connections to the database
 * that has the maf track in it.  One is
 * for the scoredRefs, the other to access
 * the extFile database.  We could get away
 * with just one connection, but then we'd
 * have to allocate more memory to hold
 * the scoredRefs (whereas now we just use
 * one statically loaded scoredRef).
 */
#ifndef GBROWSE
if (mp->ct)
    {
    conn = sqlCtConn(TRUE);
    conn2 = sqlCtConn(TRUE);
    mp->list = wigMafLoadInRegion(conn, conn2, mp->ct->dbTableName,
				chromName, winStart - 2 , winEnd + 2);
    sqlDisconnect(&conn);
    sqlDisconnect(&conn2);
    }
else
#endif /* GBROWSE */
    {
    conn = hAllocConn();
    conn2 = hAllocConn();
    mp->list = wigMafLoadInRegion(conn, conn2, track->mapName, 
				chromName, winStart - 2 , winEnd + 2);
    hFreeConn(&conn);
    hFreeConn(&conn2);
    }
}

static struct wigMafItem *loadBaseByBaseItems(struct track *track)
/* Make up base-by-base track items. */
{
struct wigMafItem *miList = NULL, *speciesList = NULL, *mi;
int scoreHeight = 0;

bool lowerFirstChar = TRUE;
char *firstCase;

firstCase = trackDbSetting(track->tdb, ITEM_FIRST_CHAR_CASE);
if (firstCase != NULL)
    {
    if (sameWord(firstCase, "noChange")) lowerFirstChar = FALSE;
    }

loadMafsToTrack(track);

/* NOTE: we are building up the item list backwards */

/* Add items for conservation wiggles */
struct track *wigTrack = track->subtracks;
if (wigTrack)
    {
    enum trackVisibility wigVis = (wigTrack->visibility == tvDense ? tvDense : tvFull);
    while (wigTrack !=  NULL)
        {
        scoreHeight = wigTotalHeight(wigTrack, wigVis);
        mi = scoreItem(scoreHeight, wigTrack->shortLabel);
        slAddHead(&miList, mi);
        wigTrack = wigTrack->next;
        }
    }

/* Make up item that will show gaps in this organism. */
AllocVar(mi);

mi->name = GAP_ITEM_LABEL;
mi->height = tl.fontHeight;
slAddHead(&miList, mi);

/* Make up item for this organism. */
mi = newMafItem(database, 0, lowerFirstChar);
slAddHead(&miList, mi);
speciesList = newSpeciesItems(track, tl.fontHeight);
/* Make items for other species */
miList = slCat(speciesList, miList);

slReverse(&miList);
return miList;
}

static char *summarySetting(struct track *track)
/* Return the setting for the MAF summary table
 * or NULL if none set  */
{
return trackDbSetting(track->tdb, SUMMARY_VAR);
}

static char *pairwiseSuffix(struct track *track)
/* Return the suffix for the wiggle tables for the pairwise alignments,
 * or NULL if none set  */
{
char *suffix = trackDbSetting(track->tdb, PAIRWISE_VAR);
if (suffix != NULL)
    suffix = firstWordInLine(cloneString(suffix));
return suffix;
}

static int pairwiseWigHeight(struct track *track)
/* Return the height of a pairwise wiggle for this track, or 0 if n/a
 * NOTE: set one pixel smaller than we actually want, to 
 * leave a border at the bottom of the wiggle.
 */
{
char *words[2];
int wordCount;
char *settings;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = tl.fontHeight;
int consWigHeight = 0;

settings = cloneString(trackDbSetting(track->tdb, PAIRWISE_HEIGHT));
if (settings != NULL)
    return(atoi(firstWordInLine(settings)));

if (wigTrack)
    {
    consWigHeight = wigTotalHeight(wigTrack, tvFull);
    pairwiseHeight = max(consWigHeight/3 - 1, pairwiseHeight);
    }

settings = cloneString(trackDbSetting(track->tdb, PAIRWISE_VAR));
if (settings == NULL)
    return pairwiseHeight;

/* get height for pairwise wiggles */
if ((wordCount = chopLine(settings, words)) > 1)
    {
    int settingsHeight = atoi(words[1]);
    if (settingsHeight < tl.fontHeight)
       pairwiseHeight = tl.fontHeight;
    else if (settingsHeight > consWigHeight && consWigHeight > 0)
        pairwiseHeight = consWigHeight;
    else
        pairwiseHeight = settingsHeight;
    }
freez(&settings);
return pairwiseHeight;
}

static char *getWigTablename(char *species, char *suffix)
/* generate tablename for wiggle pairwise: "<species>_<table>_wig" */
{
char table[64];

safef(table, sizeof(table), "%s_%s_wig", species, suffix);
return cloneString(table);
}

static char *getMafTablename(char *species, char *suffix)
/* generate tablename for wiggle maf:  "<species>_<table>" */
{
char table[64];

safef(table, sizeof(table), "%s_%s", species, suffix);
return cloneString(table);
}

static boolean displayPairwise(struct track *track)
/* determine if tables are present for pairwise display */
{
return winBaseCount < MAF_SUMMARY_VIEW || isCustomTrack(track->mapName) || 
	pairwiseSuffix(track) || summarySetting(track);
}

static boolean displayZoomedIn(struct track *track)
/* determine if mafs are loaded -- zoomed in display */
{
struct mafPriv *mp = getMafPriv(track);

return mp->list != (char *)-1;
}

static void markNotPairwiseItem(struct wigMafItem *mi)
{
    mi->ix = -1;
}
static boolean isPairwiseItem(struct wigMafItem *mi)
{
    return mi->ix != -1;
}

static struct wigMafItem *loadPairwiseItems(struct track *track)
/* Make up items for modes where pairwise data are shown.
   First an item for the score wiggle, then a pairwise item
   for each "other species" in the multiple alignment.
   These may be density plots (pack) or wiggles (full).
   Return item list.  Also set customPt with mafList if
   zoomed in */
{
struct wigMafItem *miList = NULL, *speciesItems = NULL, *mi;
struct track *wigTrack = track->subtracks;
int scoreHeight = tl.fontHeight * 4;

if (winBaseCount < MAF_SUMMARY_VIEW)
    {
    /* "close in" display uses actual alignments from file */
    struct mafPriv *mp = getMafPriv(track);
    struct sqlConnection *conn, *conn2;

#ifndef GBROWSE
    if (mp->ct)
	{
	conn = sqlCtConn(TRUE);
	conn2 = sqlCtConn(TRUE);
	mp->list = wigMafLoadInRegion(conn, conn2, mp->ct->dbTableName,
					    chromName, winStart, winEnd);
	sqlDisconnect(&conn);
	sqlDisconnect(&conn2);
	}
    else
#endif /* GBROWSE */
	{
	conn = hAllocConn();
	conn2 = hAllocConn();
	mp->list = wigMafLoadInRegion(conn, conn2, track->mapName, 
					    chromName, winStart, winEnd);
	hFreeConn(&conn);
	hFreeConn(&conn2);
	}
#ifdef DEBUG
    slSort(&mp->list, mafCmp);
#endif
    }
while (wigTrack != NULL)
    {
    /* display score graph along with pairs only if a wiggle
     * is provided */
    scoreHeight = wigTotalHeight(wigTrack, tvFull);
    mi = scoreItem(scoreHeight, wigTrack->shortLabel);
    /* mark this as not a pairwise item */
    markNotPairwiseItem(mi);
    slAddHead(&miList, mi);
    wigTrack = wigTrack->next;
    }
if (displayPairwise(track))
    /* make up items for other organisms by scanning through
     * all mafs and looking at database prefix to source. */
    {
    speciesItems = newSpeciesItems(track, track->visibility == tvFull ?
                                                pairwiseWigHeight(track) :
                                                tl.fontHeight);
    miList = slCat(speciesItems, miList);
    }
slReverse(&miList);
return miList;
}

static struct wigMafItem *loadWigMafItems(struct track *track,
                                                 boolean isBaseLevel)
/* Load up items */
{
struct wigMafItem *miList = NULL, *mi;
int scoreHeight = tl.fontHeight * 4;
struct track *wigTrack = track->subtracks;

struct mafPriv *mp = getMafPriv(track);
mp->list = (char *)-1;   /* no maf's loaded or attempted to load */

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
/* Make up tracks for display. */
if (track->visibility == tvFull || track->visibility == tvPack)
    {
    if (isBaseLevel)
	{
	miList = loadBaseByBaseItems(track);
	}
    /* zoomed out */
    else 
	{
	miList = loadPairwiseItems(track);
	}
    }
else if (track->visibility == tvSquish)
    {
    if (!wigTrack)
        {
        scoreHeight = tl.fontHeight * 4;
        if (winBaseCount < MAF_SUMMARY_VIEW)
            loadMafsToTrack(track);
        /* not a real meausre of conservation, so don't label it so */
        miList = scoreItem(scoreHeight, "");
        }
    else while (wigTrack)
        {
        /* have a wiggle */
        scoreHeight = wigTotalHeight(wigTrack, tvFull);
        mi = scoreItem(scoreHeight, wigTrack->shortLabel);
        slAddTail(&miList, mi);
        wigTrack = wigTrack->next;
        }
    }
else 
    {
    /* dense mode, zoomed out - show density plot, with track label */
    if (!wigTrack)
        {
        /* no wiggle -- use mafs if close in */
        if (winBaseCount < MAF_SUMMARY_VIEW)
#ifndef NOTYET
           loadMafsToTrack(track);
	AllocVar(miList);
	miList->name = cloneString(track->shortLabel);
	miList->height = tl.fontHeight;
#else
	    miList =  loadPairwiseItems(track);
	
	/* remove components that aren't selected */
	struct wigMafItem *mil = miList;
	struct hash *nameHash = newHash(5);
	for(;mil; mil = mil->next)
	    hashStore(nameHash, mil->name);

	struct mafPriv *mp = getMafPriv(track);
	struct mafAli *mafList = mp->list;
	struct mafComp *mc;

	for(; mafList; mafList = mafList->next)
	    {
	    struct mafComp *prev = mafList->components;

	    /* start after the master component */
	    for(mc = mafList->components->next; mc; mc = mc->next)
		{
		char *ptr = strchr(mc->src, '.');
		if (ptr)
		    *ptr = 0;
		char *ptr2 = hOrganism(mc->src);
		*ptr2 = tolower(*ptr2);
		if (!hashLookup(nameHash, ptr2))
		    /* delete this component */
		    prev->next = mc->next;
		else
		    {
		    if (ptr)
			*ptr = '.';
		    prev = mc;
		    }
		}
	    }

	/* label with the track name */
        miList->name = cloneString(track->shortLabel);
	miList->next = NULL;
#endif /* NOTYET */
        }
    else while (wigTrack)
        {
        AllocVar(mi);
        mi->name = cloneString(wigTrack->shortLabel);
        mi->height = tl.fontHeight + 1;
        slAddTail(&miList, mi);
        wigTrack = wigTrack->next;
        }
    }
return miList;
}

static void wigMafLoad(struct track *track)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct wigMafItem *miList = NULL;
struct track *wigTrack = track->subtracks;

miList = loadWigMafItems(track, zoomedToBaseLevel);
track->items = miList;

while (wigTrack != NULL) 
    // load wiggle subtrack items
    {
    /* update track visibility from parent track,
     * since hgTracks will update parent vis before loadItems */
    wigTrack->visibility = track->visibility;
    wigTrack->loadItems(wigTrack);
    wigTrack = wigTrack->next;
    }
}

static int wigMafTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct wigMafItem *mi;
int total = 0;
for (mi = track->items; mi != NULL; mi = mi->next)
    total += mi->height;

track->height = total;
return track->height;
}

static int wigMafItemHeight(struct track *track, void *item)
/* Return total height of maf track.  */
{
struct wigMafItem *mi = item;
return mi->height;
}


static void wigMafFree(struct track *track)
/* Free up maf items. */
{
struct mafPriv *mp = getMafPriv(track);
if (mp->list != NULL && mp->list != (char *)-1)
    mafAliFreeList((struct mafAli **)&mp->list);
if (track->items != NULL)
    wigMafItemFreeList((struct wigMafItem **)&track->items);
}

static char *wigMafItemName(struct track *track, void *item)
/* Return name of maf level track. */
{
struct wigMafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, struct mafAli *maf, 
                                struct hash *itemHash,
	                        int insertCounts[], int baseCount)
/* Make up insert line from sequence of reference species.  
   It has a gap count at each displayed base position, and is generated by
   counting up '-' chars in the sequence, where  */
{
int i, baseIx = 0;
struct mafComp *mc;
char c;

for (i=0; i < maf->textSize && baseIx < baseCount - 1; i++)
    {
    c = text[i];
    if (c == '-')
        {
        for (mc = maf->components; mc != NULL; mc = mc->next)
            {
            char buf[64];
            mafSrcDb(mc->src, buf, sizeof buf);
            if (hashLookup(itemHash, buf) == NULL)
                continue;
            if (mc->text == NULL)
                /* empty row annotation */
                continue;
            if (mc->text[i] != '-')
                {
                insertCounts[baseIx]++;
                break;
                }
            }
        }
    else
        baseIx++;
    }
}


static int processSeq(char *text, char *masterText, int textSize,
                            char *outLine, int offset, int outSize,
			    int *inserts, int insertCounts)
/* Add text to outLine, suppressing copy where there are dashes
 * in masterText.  This effectively projects the alignment onto
 * the master genome.
 * If no dash exists in this sequence, count up size
 * of the insert and save in the line.
 */
{
int i, outIx = 0, outPositions = 0;
int insertSize = 0, previousInserts = 0;
int previousBreaks = 0;

outLine = outLine + offset + previousBreaks + (previousInserts * 2);
for (i=0; i < textSize /*&& outPositions < outSize*/;  i++)
    {
    if (masterText[i] != '-')
        {
        if (insertSize != 0)
            {
	    inserts[insertCounts++] = offset + outIx + 1;
            insertSize = 0;
            }
	outLine[outIx++] = text[i];
        outPositions++;
	}
    else
        {
        /* gap in master (reference) sequence but not in this species */
	switch(text[i])
	    {
	    case 'N':
	    case '-':
	    case '=':
	    case ' ':
		break;
	    default:
		insertSize++;
	    }
        }
    }
    if (insertSize != 0)
	{
	inserts[insertCounts++] = offset + outIx + 1;
	}
return insertCounts;
}

static int drawScore(float score, int chromStart, int chromEnd, int seqStart,
                        double scale, struct hvGfx *hvg, int xOff, int yOff,
                        int height, Color color, enum trackVisibility vis)
/* Draw density plot or graph based on score. Return last X drawn  */
{
int x1,x2,y,w;
int height1 = height-1;

x1 = round((chromStart - seqStart)*scale);
x2 = round((chromEnd - seqStart)*scale);
w = x2-x1+1;
if (vis == tvFull)
    {
    y = score * height1;
    hvGfxBox(hvg, x1 + xOff, yOff + height1 - y, w, y+1, color);
    }
else
    {
    Color c;
    int shade = (score * maxShade) + 1;
    if (shade < 0)
        shade = 0;
    else if (shade > maxShade)
        shade = maxShade;
    c = shadesOfGray[shade];
    hvGfxBox(hvg, x1 + xOff, yOff, w, height1, c);
    }
return xOff + x1 + w - 1;
}

static void drawScoreSummary(struct mafSummary *summaryList, int height,
                             int seqStart, int seqEnd, 
                            struct hvGfx *hvg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor,
                            enum trackVisibility vis, boolean chainBreaks)
/* Draw density plot or graph for summary maf scores */
{
struct mafSummary *ms;
double scale = scaleForPixels(width);
boolean isDouble = FALSE;
int x1, x2;
int w = 0;
int chromStart = seqStart;
int lastX;

/* draw chain before first alignment */
ms = summaryList;
if (chainBreaks && ms->chromStart > chromStart)
    {
    if ((ms->leftStatus[0] == MAF_CONTIG_STATUS || 
     ms->leftStatus[0] == MAF_INSERT_STATUS))
	{
	isDouble = (ms->leftStatus[0] == MAF_INSERT_STATUS);
	x1 = xOff;
	x2 = round((double)((int)ms->chromStart-1 - seqStart) * scale) + xOff;
	w = x2 - x1;
	if (w > 0)
	    drawMafChain(hvg, x1, yOff, w, height, isDouble);
	}
    else if (ms->leftStatus[0] == MAF_MISSING_STATUS )
	{
	Color fuzz = shadesOfGray[1];

	x1 = xOff;
	x2 = round((double)((int)ms->chromStart-1 - seqStart) * scale) + xOff;
	w = x2 - x1;
	hvGfxBox(hvg, x1, yOff, w, height, fuzz);
	}
    }
for (ms = summaryList; ms != NULL; ms = ms->next)
    {
    lastX = drawScore(ms->score, ms->chromStart, ms->chromEnd, seqStart,
                        scale, hvg, xOff, yOff, height, color, vis);

    /* draw chain after alignment */
    if (chainBreaks && ms->chromEnd < seqEnd && ms->next != NULL)
	{
	if ((ms->rightStatus[0] == MAF_CONTIG_STATUS || 
         ms->rightStatus[0] == MAF_INSERT_STATUS))
	    {
	    isDouble = (ms->rightStatus[0] == MAF_INSERT_STATUS);
	    x1 = round((double)((int)ms->chromEnd+1 - seqStart) * scale) + xOff;
	    x2 = round((double)((int)ms->next->chromStart-1 - seqStart) * scale) 
		    + xOff;
	    w = x2 - x1;
	    if (w == 1 && x1 == lastX)
		continue;
	    if (w > 0);
		drawMafChain(hvg, x1, yOff, w, height, isDouble);
	    }
	else if (ms->rightStatus[0] == MAF_MISSING_STATUS )
	    {
	    Color fuzz = shadesOfGray[2];

	    x1 = round((double)((int)ms->chromEnd+1 - seqStart) * scale) + xOff;
	    x2 = round((double)((int)ms->next->chromStart-1 - seqStart) * scale) + xOff ;
	    w = x2 - x1;
	    if (w == 1 && x1 == lastX)
		continue;
	    if (w > 0)
		hvGfxBox(hvg, x1, yOff, w, height, fuzz);
	    }
	}
    }
}

static void drawScoreOverviewC(struct sqlConnection *conn,
			    char *tableName, int height,
                             int seqStart, int seqEnd, 
                            struct hvGfx *hvg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor,
                            enum trackVisibility vis)
/* Draw density plot or graph for overall maf scores rather than computing
 * by sections, for speed.  Don't actually load the mafs -- just
 * the scored refs from the table.
 * TODO: reuse code in mafTrack.c 
 */
{
char **row;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, 
			    seqStart, seqEnd, NULL, &rowOffset);

double scale = scaleForPixels(width);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    drawScore(ref.score, ref.chromStart, ref.chromEnd, seqStart, scale,
                hvg, xOff, yOff, height, color, vis);
    }
sqlFreeResult(&sr);
}

static void drawScoreOverview(char *tableName, 
	int height, int seqStart, int seqEnd, 
	struct hvGfx *hvg, int xOff, int yOff,
	int width, MgFont *font, 
	Color color, Color altColor,
	enum trackVisibility vis)
{
struct sqlConnection *conn = hAllocConn();

drawScoreOverviewC(conn, tableName, height, seqStart, seqEnd, 
	hvg, xOff, yOff, width, font, color, altColor, vis);

hFreeConn(&conn);
}

#ifndef GBROWSE
static void drawScoreOverviewCT(char *tableName, 
	int height, int seqStart, int seqEnd, 
	struct hvGfx *hvg, int xOff, int yOff,
	int width, MgFont *font, 
	Color color, Color altColor,
	enum trackVisibility vis)
{
struct sqlConnection *conn = sqlCtConn(TRUE);

drawScoreOverviewC(conn, tableName, height, seqStart, seqEnd, 
	hvg, xOff, yOff, width, font, color, altColor, vis);

sqlDisconnect(&conn);
}
#endif /* GBROWSE */

static boolean drawPairsFromSummary(struct track *track, 
        int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
{
/* Draw pairwise display for this multiple alignment */
char *summary;
struct wigMafItem *miList = track->items, *mi = miList;
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct mafSummary *ms, *summaryList;
struct hash *componentHash = newHash(6);
struct hashEl *hel;
struct hashCookie cookie;
struct dyString *where = dyStringNew(256);
char *whereClause = NULL;
boolean useIrowChains = TRUE;
char option[64];

if (miList == NULL)
    return FALSE;

/* get summary table name from trackDb */
if ((summary = summarySetting(track)) == NULL)
    return FALSE;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartVarExists(cart, option))
    useIrowChains = cartCgiUsualBoolean(cart, option, TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
    cartSetBoolean(cart, option, useIrowChains);
    }

/* Create SQL where clause that will load up just the
 * summaries for the species that we are including. */ 
conn = hAllocConn();
dyStringAppend(where, "src in (");
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (!isPairwiseItem(mi))
	/* exclude non-species items (e.g. conservation wiggle */
	continue;
    dyStringPrintf(where, "'%s'", mi->db);
    if (mi->next != NULL)
	dyStringAppend(where, ",");
    }
dyStringAppend(where, ")");
/* check for empty where clause */
if (!sameString(where->string,"src in ()"))
    whereClause = where->string;
sr = hOrderedRangeQuery(conn, summary, chromName, seqStart, seqEnd,
                        whereClause, &rowOffset);

/* Loop through result creating a hash of lists of maf summary blocks.
 * The hash is keyed by species. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hHasField(summary, "leftStatus"))
        ms = mafSummaryLoad(row + rowOffset);
    else
        /* previous table schema didn't have status fields */
        ms = mafSummaryMiniLoad(row + rowOffset);
    /* prune to fit in window bounds */
    if (ms->chromStart < seqStart)
        ms->chromStart = seqStart;
    if (ms->chromEnd > seqEnd)
        ms->chromEnd = seqEnd;
    if ((hel = hashLookup(componentHash, ms->src)) == NULL)
        hashAdd(componentHash, ms->src, ms);
    else
        slAddHead(&(hel->val), ms);
    }
sqlFreeResult(&sr);

/* reverse summary lists */
cookie = hashFirst(componentHash);
while ((hel = hashNext(&cookie)) != NULL)
    slReverse(&hel->val);
hFreeConn(&conn);

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
    summaryList = (struct mafSummary *)hashFindVal(componentHash, mi->db);
    if (summaryList == NULL)
        summaryList = (struct mafSummary *)hashFindVal(componentHash, mi->name);

    if (summaryList != NULL)
	{
	if (vis == tvFull)
	    {
	    hvGfxSetClip(hvg, xOff, yOff, width, 16);
	    drawScoreSummary(summaryList, mi->height, seqStart, seqEnd, hvg, 
				    xOff, yOff, width, font, track->ixAltColor,
				    track->ixAltColor, tvFull, useIrowChains);
	    hvGfxUnclip(hvg);
	    }
	else 
	    {
	    /* pack */
	    /* get maf table, containing pairwise alignments for this organism */
	    /* display pairwise alignments in this region in dense format */
	    hvGfxSetClip(hvg, xOff, yOff, width, mi->height);
	    drawScoreSummary(summaryList, mi->height, seqStart, seqEnd, hvg, 
				xOff, yOff, width, font, color, color, tvDense,
				useIrowChains);
	    hvGfxUnclip(hvg);
	    }
	}
    yOff += mi->height;
    }
return TRUE;
}

static boolean drawPairsFromPairwiseMafScores(struct track *track, 
        int seqStart, int seqEnd, struct hvGfx *hvg, int xOff, int yOff, 
        int width, MgFont *font, Color color, enum trackVisibility vis)
/* Draw pairwise display for this multiple alignment */
{
char *suffix;
char *tableName;
Color pairColor;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = pairwiseWigHeight(track);
struct wigMafItem *miList = track->items, *mi = miList;

if (miList == NULL)
    return FALSE;

/* get pairwise table suffix from trackDb */
suffix = pairwiseSuffix(track);

if (vis == tvFull && !isCustomTrack(track->mapName))
    {
    double minY = 50.0;
    double maxY = 100.0;

    /* NOTE: later, remove requirement for wiggle */
    if (wigTrack == NULL)
        return FALSE;
    /* swap colors for pairwise wiggles */
    pairColor = wigTrack->ixColor;
    wigTrack->ixColor = wigTrack->ixAltColor;
    wigTrack->ixAltColor = pairColor;
    wigSetCart(wigTrack, MIN_Y, (void *)&minY);
    wigSetCart(wigTrack, MAX_Y, (void *)&maxY);
    }

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
#ifndef GBROWSE
    if (isCustomTrack(track->mapName))
	{
	struct mafPriv *mp = getMafPriv(track);
	drawScoreOverviewCT(mp->ct->dbTableName, mi->height, 
		seqStart, seqEnd, 
		hvg, xOff, yOff, width, font, color, color, vis);
	hvGfxUnclip(hvg);
	}
    else 
#endif /* GBROWSE */
    if (vis == tvFull)
        {
        /* get wiggle table, of pairwise 
           for example, percent identity */
        tableName = getWigTablename(mi->name, suffix);
        if (!hTableExists(tableName))
            tableName = getWigTablename(mi->db, suffix);
        if (hTableExists(tableName))
            {
            /* reuse the wigTrack for pairwise tables */
            wigTrack->mapName = tableName;
            wigTrack->loadItems(wigTrack);
            wigTrack->height = wigTrack->lineHeight = wigTrack->heightPer =
                                                    pairwiseHeight - 1;
            /* clip, but leave 1 pixel border */
            hvGfxSetClip(hvg, xOff, yOff, width, wigTrack->height);
            wigTrack->drawItems(wigTrack, seqStart, seqEnd, hvg, xOff, yOff,
                             width, font, color, tvFull);
            hvGfxUnclip(hvg);
            }
        else
            {
            /* no wiggle table for this -- compute a graph on-the-fly 
               from mafs */
            hvGfxSetClip(hvg, xOff, yOff, width, mi->height);
            tableName = getMafTablename(mi->name, suffix);
            if (!hTableExists(tableName))
                tableName = getMafTablename(mi->db, suffix);
            if (hTableExists(tableName))
                drawScoreOverview(tableName, mi->height, seqStart, seqEnd, hvg, 
                                xOff, yOff, width, font, track->ixAltColor, 
                                track->ixAltColor, tvFull);
            hvGfxUnclip(hvg);
            }
        /* need to add extra space between wiggles (for now) */
        mi->height = pairwiseHeight;
        }
    else 
        {
	/* pack */
	/* get maf table, containing pairwise alignments for this organism */
	/* display pairwise alignments in this region in dense format */
	hvGfxSetClip(hvg, xOff, yOff, width, mi->height);
	tableName = getMafTablename(mi->name, suffix);
	if (!hTableExists(tableName))
	    tableName = getMafTablename(mi->db, suffix);
	if (hTableExists(tableName))
	    drawScoreOverview(tableName, mi->height, seqStart, seqEnd, hvg, 
			    xOff, yOff, width, font, color, color, tvDense);
        hvGfxUnclip(hvg);
        }
    yOff += mi->height;
    }
return TRUE;
}

static boolean drawPairsFromMultipleMaf(struct track *track, 
        int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise display from maf of multiple alignment.
 * Extract pairwise alignments from maf and rescore.
 * This is used only when zoomed-in.
 */
{
struct wigMafItem *miList = track->items, *mi = miList;
int graphHeight = 0;
Color pairColor = (vis == tvFull ? track->ixAltColor : color);
boolean useIrowChains = TRUE;
char option[64];

struct mafPriv *mp = getMafPriv(track);
if (miList == NULL || mp->list == NULL)
    return FALSE;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartVarExists(cart, option))
    useIrowChains = cartCgiUsualBoolean(cart, option, TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
    cartSetBoolean(cart, option, useIrowChains);
    }

if (vis == tvFull)
    graphHeight = pairwiseWigHeight(track);

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    struct mafAli *mafList = NULL, *maf, *pairMaf;
    struct mafComp *mcThis, *mcPair = NULL, *mcMaster = NULL;

    if (mi->ix < 0)
        /* ignore item for the score */
        continue;

    /* using maf sequences from file */
    /* create pairwise maf list from the multiple maf */
    struct mafPriv *mp = getMafPriv(track);
    for (maf = mp->list; maf != NULL; maf = maf->next)
        {
        if ((mcThis = mafMayFindCompSpecies(maf, mi->db, '.')) == NULL)
            continue;
	//if (mcPair->srcSize != 0)
        // TODO: replace with a cloneMafComp()
        AllocVar(mcPair);
        mcPair->src = cloneString(mcThis->src);
        mcPair->srcSize = mcThis->srcSize;
        mcPair->strand = mcThis->strand;
        mcPair->start = mcThis->start;
        mcPair->size = mcThis->size;
        mcPair->text = cloneString(mcThis->text);
        mcPair->leftStatus = mcThis->leftStatus;
        mcPair->leftLen = mcThis->leftLen;
        mcPair->rightStatus = mcThis->rightStatus;
        mcPair->rightLen = mcThis->rightLen;

        mcThis = mafFindCompSpecies(maf, database, '.');
        AllocVar(mcMaster);
        mcMaster->src = cloneString(mcThis->src);
        mcMaster->srcSize = mcThis->srcSize;
        mcMaster->strand = mcThis->strand;
        mcMaster->start = mcThis->start;
        mcMaster->size = mcThis->size;
        mcMaster->text = cloneString(mcThis->text);
        mcMaster->next = mcPair;

        AllocVar(pairMaf);
        pairMaf->components = mcMaster;
        pairMaf->textSize = maf->textSize;
        slAddHead(&mafList, pairMaf);
        }
    slReverse(&mafList);

    /* compute a graph or density on-the-fly from mafs */
    hvGfxSetClip(hvg, xOff, yOff, width, mi->height);
    drawMafRegionDetails(mafList, mi->height, seqStart, seqEnd, hvg, xOff, yOff,
                         width, font, pairColor, pairColor, vis, FALSE, 
                         useIrowChains);
    hvGfxUnclip(hvg);

    /* need to add extra space between graphs ?? (for now) */
    if (vis == tvFull)
        mi->height = graphHeight;

    yOff += mi->height;
    mafAliFreeList(&mafList);
    }
return TRUE;
}

static boolean wigMafDrawPairwise(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise display for this multiple alignment
 * When zoomed in, use on-the-fly scoring of alignments extracted from multiple
 * When zoomed out:
 *  if "pairwise" setting is on, use pairwise tables (maf or wiggle)
 *      <species>_<suffix> for maf, <species>_<suffix>_wig for wiggle
 *              For full mode, display graph.
 *              for pack mode, display density plot.
 *  if "summary" setting is on, use maf summary table
 *      (saves space, and performs better) */
{
    if (displayZoomedIn(track))
        return drawPairsFromMultipleMaf(track, seqStart, seqEnd, hvg,
                                        xOff, yOff, width, font, color, vis);
    if (isCustomTrack(track->mapName) || pairwiseSuffix(track))
        return drawPairsFromPairwiseMafScores(track, seqStart, seqEnd, hvg,
                                        xOff, yOff, width, font, color, vis);
    if (summarySetting(track))
        return drawPairsFromSummary(track, seqStart, seqEnd, hvg,
                                        xOff, yOff, width, font, color, vis);
    return FALSE;
}

static void alternateBlocksBehindChars(struct hvGfx *hvg, int x, int y, 
	int width, int height, int charWidth, int charCount, 
	int stripeCharWidth, Color a, Color b)
/* Draw blocks that alternate between color a and b. */
{
int x1,x2 = x + width;
int color = a;
int i;
for (i=0; i<charCount; i += stripeCharWidth)
    {
    x1 = i * width / charCount;
    x2 = (i+stripeCharWidth) * width/charCount;
    hvGfxBox(hvg, x1+x, y, x2-x1, height, color);
    if (color == a)
        color = b;
    else
        color = a;
    }
}

void alignSeqToUpperN(char *line)
/* force base chars to upper, ignoring insert counts */
{
int i;
for (i=0; line[i] != 0; i++)
    line[i] = toupper(line[i]);
}

void complementUpperAlignSeq(DNA *dna, int size)
/* Complement DNA (not reverse), ignoring insert counts.
 * Assumed to be all upper case bases */
{
int i;
for (i = 0; i < size; i++, dna++)
    {
    if (*dna == 'A')
        *dna = 'T';
    else if (*dna == 'C')
        *dna = 'G';
    else if (*dna == 'G')
        *dna = 'C';
    else if (*dna == 'T')
        *dna = 'A';
    }
}

void reverseForStrand(DNA *dna, int length, char strand, bool alreadyComplemented)
{
if (strand == '-')
    {
    if (alreadyComplemented)
	reverseBytes(dna, length);
    else
	reverseComplement(dna, length);
    }
else
    {
    if (alreadyComplemented)
	complement(dna, length);
    }
}

#define ISGAP(x)  (((x) == '=') || (((x) == '-')))
#define ISN(x)  ((x) == 'N') 
#define ISSPACE(x)  ((x) == ' ') 
#define ISGAPSPACEORN(x)  (ISSPACE(x) || ISGAP(x) || ISN(x))

static AA lookupAndCheckCodon(char *codon)
{
AA retValue;

if ((retValue = lookupCodon(codon)) == 0)
    return '*';

return retValue;
}

static void translateCodons(struct sqlConnection *conn, struct sqlConnection *conn2, char *tableName, char *compName, DNA *dna, int start, int length, int frame, 
				char strand,int prevEnd, int nextStart,
				bool alreadyComplemented,
				int x, int y, int width, int height, 
				struct hvGfx *hvg)
{
int size = length;
DNA *ptr;
int color;
int end = start + length;
int x1;
char masterChrom[128];
struct mafAli *ali, *sub = NULL;
struct mafComp *comp = NULL;
int mult = 1;
char codon[4];
int fillBox = FALSE;

safef(masterChrom, sizeof(masterChrom), "%s.%s", database, chromName);

dna += start;

reverseForStrand(dna, length, strand, alreadyComplemented);

ptr = dna;
color = shadesOfSea[0];

mult = 0;
if (frame && (prevEnd == -1))
    {
    switch(frame)
	{
	case 1:
	    if (0)//!( ISGAPSPACEORN(ptr[0]) ||ISGAPSPACEORN(ptr[1])))
		{
		fillBox = TRUE;
		*ptr++ = 'X';
		*ptr++ = 'X';
		mult = 2;
		}
	    else
		{
		reverseForStrand(ptr, 2, strand, alreadyComplemented);
		ptr +=2;
		}
	    length -=2;
	    break;
	case 2:
	    if (0)//!( ISGAPSPACEORN(ptr[0])))
		{
		fillBox = TRUE;
		*ptr++ = 'X';
		length -=1;
		mult = 1;
		}
	    else
		{
		reverseForStrand(ptr, 1, strand, alreadyComplemented);
		ptr++;
		}
	    length -=1;
	    break;
	}
    }
else if (frame && (prevEnd != -1))
    {
    memset(codon, 0, sizeof(codon));
    switch(frame)
	{
	case 1:
	    ali = mafLoadInRegion2(conn, conn2, tableName, chromName, prevEnd , prevEnd + 1  );
	    if (ali != NULL)
		{
		sub = mafSubset(ali, masterChrom, prevEnd , prevEnd + 1  );
		comp = mafMayFindCompSpecies(sub, compName, '.');
		}
	    if (comp && comp->text && (!(ISGAPSPACEORN(comp->text[0]) ||ISGAPSPACEORN(ptr[0]) ||ISGAPSPACEORN(ptr[1]))))
		{
		if (strand == '-')
		    complement(comp->text, 1);
		codon[0] = comp->text[0];
		codon[1] = ptr[0];
		codon[2] = ptr[1];
		fillBox = TRUE;
		mult = 2;
		*ptr++ = ' ';
		*ptr++ = lookupAndCheckCodon(codon);
		}
	    else
		ptr+=2;
	    length -= 2;
	    break;
	case 2:
	    if (strand == '-')
		{
		ali = mafLoadInRegion2(conn, conn2, tableName, chromName, 
			    prevEnd, prevEnd + 2);
		if (ali != NULL)
		    sub = mafSubset(ali, masterChrom, prevEnd, prevEnd + 2  );
		}
	    else
		{
		ali = mafLoadInRegion2(conn, conn2, tableName, chromName, 
			    prevEnd - 1, prevEnd + 1);
		if (ali != NULL)
		    sub = mafSubset(ali, masterChrom, prevEnd - 1, prevEnd + 1);
		}
	    if (sub != NULL)
		comp = mafMayFindCompSpecies(sub, compName, '.');
	    if (comp && comp->text && (!(ISGAPSPACEORN(comp->text[0])||ISGAPSPACEORN(comp->text[1]) ||ISGAPSPACEORN(*ptr))))
		{
		if (strand == '-')
		    reverseComplement(comp->text, 2);
		codon[0] = comp->text[0];
		codon[1] = comp->text[1];
		codon[2] = *ptr;
		fillBox = TRUE;
		mult = 1;
		*ptr++ = lookupAndCheckCodon(codon);
		}
	    else
		ptr++;
	    length -= 1;
	    break;
	}
    }
if (fillBox)
    {
    if (strand == '-')
	{
	x1 = x + ( end - ( 5 - frame)  ) * width / winBaseCount;
	}
    else
	{
	x1 = x + (start - 2) * width / winBaseCount;
	}
    hvGfxBox(hvg, x1, y, mult*width/winBaseCount + 1 , height, color);
    }

for (;length > 2; ptr +=3 , length -=3)
    {
    if (!(ISGAPSPACEORN(ptr[0]) || ISGAPSPACEORN(ptr[1]) || ISGAPSPACEORN(ptr[2]) ))
	{
	ptr[1] = lookupAndCheckCodon(ptr);
	ptr[0] = ' ';
	ptr[2] = ' ';

	if (strand == '-')
	    {
	    x1 = x + ( start + length - 3 - 2) * width / winBaseCount;
	    }
	else
	    {
	    x1 = x + (end - length - 2) * width / winBaseCount;
	    }

	if (color == shadesOfSea[0])
	    color = shadesOfSea[1];
	else
	    color = shadesOfSea[0];
	hvGfxBox(hvg, x1, y, 3*width/winBaseCount + 1 , height, color);
	}
    }

if (length && (nextStart != -1))
    {
    char codon[4];
    int mult = 1;
    boolean fillBox = FALSE;

    memset(codon, 0, sizeof(codon));
    sub = NULL;
	
    if (strand == '-')
	{
	ali = mafLoadInRegion2(conn, conn2, tableName, chromName, 
		    nextStart - 2 + length, nextStart + 1 );
	if (ali != NULL)
	    sub = mafSubset(ali, masterChrom, 
		nextStart - 2 + length, nextStart + 1);
	}
    else
	{
	ali = mafLoadInRegion2(conn, conn2, tableName, chromName, 
				nextStart , nextStart + 2 );
	if (ali != NULL)
	    sub = mafSubset(ali, masterChrom, nextStart , nextStart + 2);
	}
    if (sub != NULL)
	comp = mafMayFindCompSpecies(sub, compName, '.');
    if (sub && comp && comp->text)
	{
	switch(length)
	    {
	    case 2:
		if (strand == '-')
		    complement(comp->text, 1);
		codon[0] = *ptr;
		codon[1] = *(1 + ptr);
		codon[2] = *comp->text;

		if (!(ISGAPSPACEORN(codon[0]) ||ISGAPSPACEORN(codon[1]) ||ISGAPSPACEORN(codon[2])))
		    {
		    fillBox = TRUE;
		    *ptr++ = ' ';
		    *ptr++ = lookupAndCheckCodon(codon);
		    mult = 2;
		    }
		break;
	    case 1:
		if (strand == '-')
		    reverseComplement(comp->text, 2);
		codon[0] = *ptr;
		codon[1] = comp->text[0];
		codon[2] = comp->text[1];
		if (!(ISGAPSPACEORN(codon[0]) ||ISGAPSPACEORN(codon[1]) ||ISGAPSPACEORN(codon[2])))
		    {
		    *ptr++ = lookupAndCheckCodon(codon);
		    fillBox = TRUE;
		    }
		break;
	    }
	if (fillBox)
	    {
	    if (strand == '-')
		{
		x1 = x + ( start  - 2   ) * width / winBaseCount;
		}
	    else
		{
		x1 = x + (end - length - 2) * width / winBaseCount;
		}
	    if (color == shadesOfSea[0])
		color = shadesOfSea[1];
	    else
		color = shadesOfSea[0];
	    hvGfxBox(hvg, x1, y, mult*width/winBaseCount + 1 , height, color);
	    }
	}
    }
else if (length)	 /* broken frame, just show nucs */
    {
    if (strand == '-')
	{
	if (!alreadyComplemented)
	    complement(ptr, length);
	}
    else
	{
	if (alreadyComplemented)
	    complement(ptr, length);
	}
    }

if (strand == '-')
    reverseBytes(dna, size);
}

static int wigMafDrawBases(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis,
	struct wigMafItem *miList)
/* Draw base-by-base view, return new Y offset. */
{
struct wigMafItem *mi;
struct mafAli *mafList, *maf, *sub;
struct mafComp *mc, *mcMaster;
int lineCount = slCount(miList);
char **lines = NULL, *selfLine, *insertLine;
int *insertCounts;
int i, x = xOff, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
struct hash *srcHash = newHash(0);
char dbChrom[64];
char buf[1024];
char option[64];
int alignLineLength = winBaseCount * 2;
        /* doubled to allow space for insert counts */
boolean complementBases = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
bool dots = FALSE;         /* configuration option */
/* this line must be longer than the longest base-level display */
char noAlignment[2000];
boolean useIrowChains = TRUE;
int offset;
char *framesTable = NULL;
char *defaultCodonSpecies = cartUsualString(cart, SPECIES_CODON_DEFAULT, NULL);
char *codonTransMode = NULL;
boolean startSub2 = FALSE;

int mafOrig = 0;
int mafOrigOffset = 0;
char query[256];
struct mafPriv *mp = getMafPriv(track);
struct sqlConnection *conn2 = NULL;
struct sqlConnection *conn3 = NULL;
char *tableName = NULL;

#ifndef GBROWSE
if (mp->ct != NULL)
    {
    conn2 = sqlCtConn(TRUE);
    conn3 = sqlCtConn(TRUE);
    tableName = mp->ct->dbTableName;
    }
else
#endif /* GBROWSE */
    {
    conn2 = hAllocConn();
    conn3 = hAllocConn();
    tableName = track->mapName;
    }

if (hIsGsidServer())
    {
    /* decide the value of mafOrigOffset to be used to display xxAaMaf tracks. */
    struct sqlConnection *conn = hAllocConn();
    safef(query, sizeof(query), "select chromStart from %s", track->mapName);
    mafOrig = atoi(sqlNeedQuickString(conn, query));
    mafOrigOffset = (mafOrig % 3) - 1;
    /* offset has to be non-negative */
    if (mafOrigOffset < 0) mafOrigOffset = mafOrigOffset +3;

    hFreeConn(&conn);
    }

if (defaultCodonSpecies == NULL)
    defaultCodonSpecies = trackDbSetting(track->tdb, "speciesCodonDefault");

if (defaultCodonSpecies == NULL)
    defaultCodonSpecies = database;

if (seqStart > 2)
    {
    startSub2 = TRUE;
    seqStart -=2;
    }
seqEnd +=2;
if (seqEnd > seqBaseCount)
    seqEnd = seqBaseCount;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_DOT_VAR);
if (cartVarExists(cart, option))
    {
    dots = cartCgiUsualBoolean(cart, option, FALSE);
    }
else
    {
    char *dotString = trackDbSetting(track->tdb, MAF_DOT_VAR);
    if (dotString && sameString(dotString, "on"))
	{
	dots = TRUE;
	cartSetBoolean(cart, option, TRUE);
	}
    }

safef(buf, sizeof(buf), "%s.frames",track->mapName);
if (cartVarExists(cart, buf))
    framesTable = cartUsualString(cart, buf, NULL);
else
    framesTable = trackDbSetting(track->tdb, "frames");

if (framesTable)
    {
    safef(buf, sizeof(buf), "%s.codons",track->mapName);
    codonTransMode = cartUsualString(cart, buf, "codonDefault");
    if (sameString("codonNone", codonTransMode))
	framesTable = NULL;
    }

boolean newTableType = FALSE;

if (framesTable != NULL)
    newTableType = hHasField(framesTable, "isExonStart");

/* initialize "no alignment" string to o's */
for (i = 0; i < sizeof noAlignment - 1; i++)
    noAlignment[i] = UNALIGNED_SEQ;


safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartVarExists(cart, option))
    useIrowChains = cartCgiUsualBoolean(cart, option, TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
    cartSetBoolean(cart, option, useIrowChains);
    }

/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount);
lines[0] = needMem(alignLineLength);
for (i=1; i<lineCount; ++i)
    {
    lines[i] = needMem(alignLineLength);
    memset(lines[i], ' ', alignLineLength - 1);
    }

/* Give nice names to first two. */
insertLine = lines[0];
selfLine = lines[1];

/* Allocate a line for recording gap sizes in reference */
AllocArray(insertCounts, alignLineLength);

/* Load up self-line with DNA */
seq = hChromSeqMixed(chromName, seqStart , seqEnd);
memcpy(selfLine, seq->dna, winBaseCount + 4);
//toUpperN(selfLine, winBaseCount);
freeDnaSeq(&seq);

/* Make hash of species items keyed by database. */
i = 0;
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->ix = i++;
    if (mi->db != NULL)
	hashAdd(miHash, mi->db, mi);
    }

/* Go through the mafs saving relevant info in lines. */
mafList = mp->list;
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);

for (maf = mafList; maf != NULL; maf = maf->next)
    {
    int mafStart;
    /* get info about sequences from full alignment,
       for use later, when determining if sequence is unaligned or missing */
    for (mc = maf->components; mc != NULL; mc = mc->next)
        if (!hashFindVal(srcHash, mc->src))
            hashAdd(srcHash, mc->src, maf);

    mcMaster = mafFindComponent(maf, dbChrom);
    mafStart = mcMaster->start;
    /* get portion of maf in this window */
    if (startSub2)
	sub = mafSubset(maf, dbChrom, winStart - 2, winEnd + 2);
    else
	sub = mafSubset(maf, dbChrom, winStart , winEnd + 2);
    if (sub != NULL)
        {
	int subStart,subEnd;
	int lineOffset, subSize;
	int startInserts = 0;
	char *ptr;

        /* process alignment for reference ("master") species */
	for(ptr = mcMaster->text; *ptr == '-'; ptr++)
	    startInserts++;

	mcMaster = mafFindComponent(sub, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(sub);
	subStart = mcMaster->start;
	subEnd = subStart + mcMaster->size;
	subSize = subEnd - subStart;
	lineOffset = subStart - seqStart;
        processInserts(mcMaster->text, sub, miHash,
                                &insertCounts[lineOffset], subSize);
	insertCounts[lineOffset] = max(insertCounts[lineOffset],startInserts);

        /* fill in bases for each species */
        for (mi = miList; mi != NULL; mi = mi->next)
            {
            char *seq;
            bool needToFree = FALSE;
            int size = sub->textSize;
            if (mi->ix == 1)
                /* reference */
                continue;
            if (mi->db == NULL)
                /* not a species line -- it's the gaps line, or... */
                continue;
            if ((mc = mafMayFindCompSpecies(sub, mi->db, '.')) == NULL)
                continue;
	    if (mafStart == subStart)
		{
		if (mc->size && mc->leftStatus == MAF_INSERT_STATUS && (*mc->text != '-') &&
		 !((lineOffset) && (((lines[mi->ix][lineOffset-1]) == '=') || (lines[mi->ix][lineOffset-1]) == '-')))
		    {
		    insertCounts[lineOffset] = max(insertCounts[lineOffset],mc->leftLen);
		    mi->inserts[mi->insertsSize] =  (subStart - seqStart)+ 1;
		    mi->insertsSize++;
		    }
		}
	    if (startInserts)
		{
		struct mafComp *mc1;
		if (((mc1 = mafMayFindCompSpecies(maf, mi->db, '.')) != NULL) && 
		    ((mc1->text) && (countNonDash(mc1->text, startInserts) > 0)))
			{
			mi->inserts[mi->insertsSize] =  (subStart - seqStart)+ 1;
			mi->insertsSize++;
			}
		}
            seq = mc->text;
            if ( mc->text == NULL )
                {
                /* if no alignment here, but MAF annotation indicates continuity
                 * of flanking alignments, fill with dashes or ='s */
               if (!useIrowChains)
                   continue;
                if ((mc->leftStatus == MAF_CONTIG_STATUS &&
                    mc->rightStatus == MAF_CONTIG_STATUS))
                    {
                    char fill = '-';
                    seq = needMem(size+1);
                    needToFree = TRUE;
                    memset(seq, fill, size);
                    }
		else if (mc->leftStatus == MAF_INSERT_STATUS && mc->rightStatus == MAF_INSERT_STATUS)
		    {
		    char fill = MAF_DOUBLE_GAP;
		    seq = needMem(size+1);
		    needToFree = TRUE;
		    memset(seq, fill, size);
		    }
		else if (mc->leftStatus == MAF_MISSING_STATUS && mc->rightStatus == MAF_MISSING_STATUS)
		    {
                    char fill = 'N';
                    seq = needMem(size+1);
                    needToFree = TRUE;
                    memset(seq, fill, size);
		    }
                else
                    continue;
                }

            if (((mc->leftStatus == MAF_NEW_STATUS ||
                mc->rightStatus == MAF_NEW_STATUS )
            || (mc->leftStatus == MAF_NEW_NESTED_STATUS ||
                mc->rightStatus == MAF_NEW_NESTED_STATUS )))
                {
                int i;
                char *p;
                seq = needMem(size+1);
                needToFree = TRUE;
                for (p = seq, i = 0; i < size; p++, i++)
                    *p = ' ';
                p = seq;
                if (mc->text != NULL)
                    strcpy(p, mc->text);
                if (mc->leftStatus == MAF_NEW_STATUS)
                    {
		    char *m = mcMaster->text;
		    if (mafStart == subStart)
			mi->seqEnds[mi->seqEndsSize] = (subStart - seqStart) + 1;
		    while(*p == '-')
			{
			if ((*m++ != '-') && (mafStart == subStart))
			    mi->seqEnds[mi->seqEndsSize]++;
			*p++ = ' ';
			}

		    if (mafStart == subStart)
			mi->seqEndsSize++;
                    }
                if (mc->leftStatus == MAF_NEW_NESTED_STATUS)
		    {
		    char *m = mcMaster->text;
		    if (mafStart == subStart)
			mi->brackStarts[mi->brackStartsSize] =  (subStart - seqStart)+ 1;
		    while(*p == '-')
			{
			*p++ = '=';
			if ((*m++ != '-') && (mafStart == subStart))
			    mi->brackStarts[mi->brackStartsSize]++;
			}
		    if (mafStart == subStart)
			mi->brackStartsSize++;
		    }
                if (mc->rightStatus == MAF_NEW_NESTED_STATUS)
		    {
		    char *p = seq + size - 1;
		    char *m = mcMaster->text + size - 1;
		    if (subEnd <= seqEnd)
			mi->brackEnds[mi->brackEndsSize] = (subStart - seqStart)+ subSize + 1;
		    while(*p == '-')
			{
			if ((*m-- != '-') && (subEnd <= seqEnd))
			    mi->brackEnds[mi->brackEndsSize]--;
			*p-- = '=';
			}
		    if (subEnd <= seqEnd)
		    	mi->brackEndsSize++;
		    }
                if (mc->rightStatus == MAF_NEW_STATUS)
		    {
		    char *p = seq + size - 1;
		    char *m = mcMaster->text + size - 1;

		    if (mc->size && ((subEnd <= seqEnd)))
			mi->seqEnds[mi->seqEndsSize] = (subStart - seqStart)+ subSize + 1;
		    while(*p == '-')
			{
			if ((*m-- != '-') && (mc->size && (subEnd <= seqEnd)))
			    mi->seqEnds[mi->seqEndsSize]--;
			*p-- = ' ';
			}
		    if (mc->size && ((subEnd <= seqEnd)))
		    	mi->seqEndsSize++;
		    }
		}
            if (mc->text && ((mc->leftStatus == MAF_MISSING_STATUS))) 
		{
                char *p = seq;
		while(*p == '-')
		    *p++ = 'N';
		}
            if (mc->text && ((mc->leftStatus == MAF_INSERT_STATUS))) 
		{
                char *p = seq;
		while(*p == '-')
		    *p++ = '=';
		}
            if (mc->text && ((mc->rightStatus == MAF_MISSING_STATUS)))
		{
                char *p = seq + size - 1;
		while(*p == '-')
		    *p-- = 'N';
		}
            if (mc->text && ((mc->rightStatus == MAF_INSERT_STATUS)))
		{
		char *m = mcMaster->text + size - 1;
                char *p = seq + size - 1;
		while((*p == '-') || (*m == '-'))
		    {
		    *p-- = '=';
		    m--;
		    }
		}
	    
            mi->insertsSize = processSeq(seq, mcMaster->text, size, lines[mi->ix], lineOffset, subSize,
	    	mi->inserts, mi->insertsSize);
            if (needToFree)
                freeMem(seq);
	    }
	}
    mafAliFree(&sub);
    }
/* draw inserts line */
mi = miList;

for(offset=startSub2*2; (offset < alignLineLength) && (offset < winBaseCount + startSub2 * 2); offset++)
    {
    int  x1, x2;

    x1 = (offset - startSub2 * 2) * width/winBaseCount;
    x2 = ((offset - startSub2 * 2)+1) * width/winBaseCount - 1;
    if (insertCounts[offset] != 0)
	{
	struct dyString *label = newDyString(20);
	int haveRoomFor = (width/winBaseCount)/tl.mWidth;

	/* calculate number of AAs instead of bases if it is wigMafProt */
	if (strstr(track->tdb->type, "wigMafProt"))
	    {
	    dyStringPrintf(label, "%d",insertCounts[offset]/3);
	    }
	else
	    {
	    dyStringPrintf(label, "%d",insertCounts[offset]);
	    }
	    
	if (label->stringSize > haveRoomFor)
	    {
	    dyStringClear(label);
	    dyStringPrintf(label, "%c",(insertCounts[offset] % 3) == 0 ? '*' : '+');
	    }
	hvGfxTextCentered(hvg, x1+x - (width/winBaseCount)/2, y, x2-x1, mi->height, 
		getOrangeColor(), font, label->string);
	dyStringFree(&label);
	}
    }
y += mi->height;

/* draw alternating colors behind base-level alignments */
    {
    int alternateColorBaseCount, alternateColorBaseOffset;
    safef(buf, sizeof(buf), "%s.%s", track->mapName, BASE_COLORS_VAR);
    alternateColorBaseCount = cartCgiUsualInt(cart, buf, 0);
    safef(buf, sizeof(buf), "%s.%s", track->mapName, BASE_COLORS_OFFSET_VAR);
    alternateColorBaseOffset = cartCgiUsualInt(cart, buf, 0);
    if (alternateColorBaseCount != 0)
        {
        int baseWidth = spreadStringCharWidth(width, winBaseCount);
        int colorX = x + alternateColorBaseOffset * baseWidth;
        alternateBlocksBehindChars(hvg, colorX, y-1, width, 
                mi->height*(lineCount-1), tl.mWidth, winBaseCount, 
                alternateColorBaseCount, shadesOfSea[0], MG_WHITE);
        }
    }

/* draw base-level alignments */
for (mi = miList->next, i=1; mi != NULL && mi->db != NULL; mi = mi->next, i++)
    {
    char *line;

    line  = lines[i];
    /* TODO: leave lower case in to indicate masking ?
       * NOTE: want to make sure that all sequences are soft-masked
       * if we do this */
    //alignSeqToUpperN(line);
    if (complementBases)
        {
	complement(line, strlen(line));
        }
    /* draw sequence letters for alignment */
    hvGfxSetClip(hvg, x, y-1, width, mi->height);

    if (framesTable != NULL)
	{
	int rowOffset;
	char **row;
	struct sqlConnection *conn = hAllocConn();
	struct sqlResult *sr;
	char extra[512];
	boolean found = FALSE;

	if (sameString("codonDefault", codonTransMode))
	    {
	    safef(extra, sizeof(extra), "src='%s'",defaultCodonSpecies); 

	    found = TRUE;
	    }
	else if (sameString("codonFrameDef", codonTransMode))
	    {
	    safef(extra, sizeof(extra), "src='%s'",mi->db); 

	    found = FALSE;
	    }
	else if (sameString("codonFrameNone", codonTransMode))
	    {
	    safef(extra, sizeof(extra), "src='%s'",mi->db); 

	    found = TRUE;
	    }
	else
	    errAbort("unknown codon translation mode %s",codonTransMode);
tryagain:
	sr = hRangeQuery(conn, framesTable, chromName, seqStart, seqEnd, extra, &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct mafFrames mf;
	    int start, end, w;
	    int frame; 

	    found = TRUE;
	    if (newTableType)
		mafFramesStaticLoad(row + rowOffset, &mf);
	    else
		mafFramesStaticLoadOld(row + rowOffset, &mf);

	    if (mf.chromStart < seqStart)
		start = 0;
	    else
		start = mf.chromStart-seqStart;
	    frame = mf.frame;
	    if (mf.strand[0] == '-')
		{
		if (mf.chromEnd > seqEnd)
		    frame = (frame + mf.chromEnd-seqEnd  ) % 3;
		}
	    else
		{
		if (mf.chromStart < seqStart)
		    frame = (frame + seqStart-mf.chromStart  ) % 3;
		}

	    end = mf.chromEnd > seqEnd ? seqEnd - seqStart  : mf.chromEnd - seqStart; 
	    w= end - start;

	    translateCodons(conn2, conn3, tableName, mi->db, line, start , 
		w, frame, mf.strand[0],mf.prevFramePos,mf.nextFramePos,
		complementBases, x, y, width, mi->height,  hvg);
	    
	    }
	sqlFreeResult(&sr);

	if (!found)
	    {
	    /* try the default species */
	    safef(extra, sizeof(extra), "src='%s'",defaultCodonSpecies); 

	    found = TRUE; /* don't try again */
	    goto tryagain;
	    }

	hFreeConn(&conn);
	}

    if (startSub2)
	{
        if (strstr(track->tdb->type, "wigMafProt"))
            {
            spreadAlignStringProt(hvg, x, y, width, mi->height-1, color,
                        font, &line[2], &selfLine[2], winBaseCount, dots, FALSE, seqStart, mafOrigOffset);
            }
	else
	    {
	    /* make sure we have bases to display before printing them */
	    if (strlen(line) > 2)
		spreadAlignString(hvg, x, y, width, mi->height-1, color,
		    font, &line[2], &selfLine[2], winBaseCount, dots, FALSE);
	    }
	}
    else
	spreadAlignString(hvg, x, y, width, mi->height-1, color,
                        font, line, selfLine, winBaseCount, dots, FALSE);
    for(offset = 0; offset < mi->seqEndsSize; offset++)
	{
	int x1;

	x1 = x + (mi->seqEnds[offset] -1 - startSub2 * 2) * width/winBaseCount;
	hvGfxBox(hvg, x1, y-1, 1, mi->height-1, getChromBreakBlueColor());
	}
    for(offset = 0; offset < mi->insertsSize; offset++)
	{
	int x1;

	x1 = x + (mi->inserts[offset] -1 - startSub2 * 2) * width/winBaseCount;
	hvGfxBox(hvg, x1, y-1, 1, mi->height-1, getOrangeColor());
	}
    for(offset = 0; offset < mi->brackStartsSize; offset++)
	{
	int x1;

	x1 = x + (mi->brackStarts[offset] -1- startSub2 * 2) * width/winBaseCount;
	hvGfxBox(hvg, x1, y-1, 2, 1, getChromBreakGreenColor());
	hvGfxBox(hvg, x1, y-1, 1, mi->height-1, getChromBreakGreenColor());
	hvGfxBox(hvg, x1, y + mi->height-3, 2, 1, getChromBreakGreenColor());
	}
    for(offset = 0; offset < mi->brackEndsSize; offset++)
	{
	int x1;

	x1 = x + (mi->brackEnds[offset] -1- startSub2 * 2) * width/winBaseCount;
	hvGfxBox(hvg, x1-1, y-1, 2, 1, getChromBreakGreenColor());
	hvGfxBox(hvg, x1, y-1, 1, mi->height-1, getChromBreakGreenColor());
	hvGfxBox(hvg, x1-1, y + mi->height-3, 2, 1, getChromBreakGreenColor());
	}

    hvGfxUnclip(hvg);
    y += mi->height;
    }

/* Clean up */
/*
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
*/
#ifndef GBROWSE
if (mp->ct != NULL)
    {
    sqlDisconnect(&conn2);
    sqlDisconnect(&conn3);
    }
else
#endif /* GBROWSE */
    {
    hFreeConn(&conn2);
    hFreeConn(&conn3);
    }
hashFree(&miHash);
return y;
}

static int wigMafDrawScoreGraph(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
/* Draw routine for score graph, returns new Y offset */
struct track *wigTrack = track->subtracks;
enum trackVisibility scoreVis;

scoreVis = (vis == tvDense ? tvDense : tvFull);

if (wigTrack == NULL)
    {
    /* no wiggle */
    int height = tl.fontHeight * 4;
    if (vis == tvFull || vis == tvPack)
        /* suppress graph if other items displayed (bases or pairs) */
        return yOff;
    /* draw some kind of graph from multiple alignment */
    struct mafPriv *mp = getMafPriv(track);
    if (mp->list != (char *)-1 && mp->list != NULL)
        {
        /* use mafs */
        drawMafRegionDetails(mp->list, height, seqStart, seqEnd,
                                hvg, xOff, yOff, width, font,
                                color, color, scoreVis, FALSE, FALSE);
        }
#ifndef GBROWSE
    else if (mp->ct != NULL)
        {
        /* use or scored refs from maf table*/
        drawScoreOverviewCT(mp->ct->dbTableName, height, seqStart, seqEnd, 
		hvg, xOff, yOff, width, font, color, color, scoreVis);
        yOff++;
        }
#endif /* GBROWSE */
    else
        {
        /* use or scored refs from maf table*/
        drawScoreOverview(track->mapName, height, seqStart, seqEnd, hvg, 
                            xOff, yOff, width, font, color, color, scoreVis);
        yOff++;
        }
    yOff += height;
    }
else
    {
    Color wigColor = 0;
    while (wigTrack != NULL)
        {
        /* draw conservation wiggles */
        if (!wigColor)
            wigColor = hvGfxFindRgb(hvg, &wigTrack->color);
        else
            wigColor = slightlyLighterColor(hvg, wigColor);
        wigTrack->ixColor = wigColor;
        wigTrack->ixAltColor = hvGfxFindRgb(hvg, &wigTrack->altColor);
        hvGfxSetClip(hvg, xOff, yOff, width, wigTotalHeight(wigTrack, scoreVis) - 1);
        wigTrack->drawItems(wigTrack, seqStart, seqEnd, hvg, xOff, yOff,
                             width, font, color, scoreVis);
        hvGfxUnclip(hvg);
        yOff += wigTotalHeight(wigTrack, scoreVis);
        wigTrack = wigTrack->next;
        }
    }
return yOff;
}


static void wigMafDraw(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for wigmaf type tracks */
{
int y = yOff;
y = wigMafDrawScoreGraph(track, seqStart, seqEnd, hvg, xOff, y, width,
                                font, color, vis);
if (vis == tvFull || vis == tvPack)
    {
    if (zoomedToBaseLevel)
	{
	struct wigMafItem *wiList = track->items;
        /* skip over cons wiggles */
        struct track *wigTrack = track->subtracks;
        while (wigTrack)
            {
            wiList = wiList->next;
            wigTrack = wigTrack->next;
            }
	y = wigMafDrawBases(track, seqStart, seqEnd, hvg, xOff, y, width, font,
				    color, vis, wiList);
				    //MG_RED, vis, wiList);
	}
    else 
	wigMafDrawPairwise(track, seqStart, seqEnd, hvg, xOff, y, 
				width, font, color, vis);
    }
mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width, track->height, track->mapName, 
            track->mapName, NULL);
}

void wigMafMethods(struct track *track, struct trackDb *tdb,
                                        int wordCount, char *words[])
/* Make track for maf multiple alignment. */
{
struct track *wigTrack;
int i;
char *savedType;
char option[64];
struct dyString *wigType;
struct consWiggle *consWig, *consWigList = NULL;

track->loadItems = wigMafLoad;
track->freeItems = wigMafFree;
track->drawItems = wigMafDraw;
track->itemName = wigMafItemName;
track->mapItemName = wigMafItemName;
track->totalHeight = wigMafTotalHeight;
track->itemHeight = wigMafItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->itemLabelColor = wigMafItemLabelColor;
track->mapsSelf = TRUE;
//track->canPack = TRUE;

/* deal with conservation wiggle(s) */
consWigList = wigMafWiggles(tdb);
if (consWigList == NULL)
    return;

/* determine which conservation wiggles to use -- from cart, 
 or if none there, use first entry in trackDb setting */
boolean first = TRUE;
for (consWig = consWigList; consWig != NULL; consWig = consWig->next)
    {
    if (differentString(consWig->leftLabel, DEFAULT_CONS_LABEL))
        {
        safef(option, sizeof option, "%s.cons.%s", 
                tdb->tableName, consWig->leftLabel);
        if (!cartCgiUsualBoolean(cart, option, first))
            continue;
        }
    first = FALSE;
    //  manufacture and initialize wiggle subtrack
    /* CAUTION: this code is very interdependent with
       hgTracks.c:fillInFromType()
       Also, both the main track and subtrack share the same tdb */
    // restore "type" line, but change type to "wig"
    savedType = tdb->type;
    wigType = newDyString(64);
    dyStringClear(wigType);
    dyStringPrintf(wigType, "type wig ");
    for (i = 1; i < wordCount; i++)
        {
        dyStringPrintf(wigType, "%s ", words[i]);
        }
    dyStringPrintf(wigType, "\n");
    tdb->type = cloneString(wigType->string);
    wigTrack = trackFromTrackDb(tdb);
    tdb->type = savedType;

    /* replace tablename with wiggle table from "wiggle" setting */
    wigTrack->mapName = cloneString(consWig->table);

    /* Tweak wiggle left labels: replace underscore with space and
     * append 'Cons' */
    struct dyString *ds = dyStringNew(0);
    dyStringAppend(ds, consWig->leftLabel);
    if (differentString(consWig->leftLabel, DEFAULT_CONS_LABEL))
        dyStringAppend(ds, " Cons");
    wigTrack->shortLabel = dyStringCannibalize(&ds);
    subChar(wigTrack->shortLabel, '_', ' ');

    /* setup wiggle methods in subtrack */
    wigMethods(wigTrack, tdb, wordCount, words);

    wigTrack->mapsSelf = FALSE;
    wigTrack->drawLeftLabels = NULL;
    wigTrack->next = NULL;
    slAddTail(&track->subtracks, wigTrack);
    dyStringFree(&wigType);
    }
}


