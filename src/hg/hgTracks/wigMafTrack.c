/* wigMafTrack - display multiple alignment files with score wiggle
 * and base-level alignment, or else density plot of pairwise alignments
 * (zoomed out) */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"
#include "hCommon.h"
#include "hgMaf.h"
#include "mafTrack.h"
#include "customTrack.h"
#include "mafSummary.h"
#include "mafFrames.h"
#include "phyloTree.h"
#include "soTerm.h"
#include "bigBed.h"
#include "hubConnect.h"
#include "chromAlias.h"


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
    struct sqlConnection *conn2, char *table, char *chrom, int start, int end, char *file)
/* Load mafs from region */
{
    return mafLoadInRegion2(conn, conn2, table, chrom, start, end, file);
}

static struct wigMafItem *newMafItem(char *s, int g, boolean lowerFirstChar, struct hash *labelHash)
/* Allocate and initialize a maf item. Species param can be a db or name */
{
struct wigMafItem *mi;
char *val;

AllocVar(mi);
if (labelHash)
    {
    mi->db = cloneString(hubConnectSkipHubPrefix(s));
    mi->name = hashFindVal(labelHash, mi->db);
    }

if (mi->name == NULL)
    {
    if ((val = hGenome(s)) != NULL)
        {
        /* it's a database name */
        mi->db = cloneString(hubConnectSkipHubPrefix(s));
        mi->name = val;
        }
    else
        {
        mi->db = cloneString(s);
        mi->name = cloneString(s);
        }
    }
mi->name = hgDirForOrg(mi->name);
if (lowerFirstChar && labelHash == NULL )
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

if (mp->list == (struct mafAli *)-1)
    return NULL;
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
char *species[MAX_SP_SIZE];
char *groups[1000];
char *defaultOff[MAX_SP_SIZE];
char sGroup[MAX_SP_SIZE];
int group;
int i;
int speciesCt = 0, groupCt = 1;
int speciesOffCt = 0;
struct hash *speciesOffHash = newHash(0);
char *speciesUseFile = trackDbSetting(track->tdb, SPECIES_USE_FILE);

/* either speciesOrder or speciesGroup is specified in trackDb */
char *speciesOrder = trackDbSetting(track->tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(track->tdb, SPECIES_GROUP_VAR);
char *speciesOff = trackDbSetting(track->tdb, SPECIES_DEFAULT_OFF_VAR);
struct hash *labelHash = mafGetLabelHash(track->tdb);

bool lowerFirstChar = TRUE;
char *firstCase;

firstCase = trackDbSetting(track->tdb, ITEM_FIRST_CHAR_CASE);
if (firstCase != NULL)
    {
    if (sameWord(firstCase, "noChange")) lowerFirstChar = FALSE;
    }

if (speciesOrder == NULL && speciesGroup == NULL && speciesUseFile == NULL && labelHash == NULL)
    return getSpeciesFromMaf(track, height);

if (speciesGroup)
    groupCt = chopLine(cloneString(speciesGroup), groups);

if (speciesUseFile)
    {
    if ((speciesGroup != NULL) || (speciesOrder != NULL))
	errAbort("Can't specify speciesUseFile and speciesGroup or speciesOrder");
    speciesOrder = cartGetOrderFromFile(database, cart, speciesUseFile);
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
struct wigMafItem *mi = NULL, *miList = NULL;
for (group = 0; group < groupCt; group++)
    {
    if (groupCt != 1 || !speciesOrder)
        {
        safef(sGroup, sizeof sGroup, "%s%s",
                                SPECIES_GROUP_PREFIX, groups[group]);
        speciesOrder = trackDbRequiredSetting(track->tdb, sGroup);
        }
    speciesCt = chopLine(cloneString(speciesOrder), species);
    for (i = 0; i < speciesCt; i++)
        {
	boolean defaultOn = (hashLookup(speciesOffHash, species[i]) == NULL);

	if (cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, species[i],defaultOn))
	    {
	    mi = newMafItem(species[i], group, lowerFirstChar, labelHash);
	    mi->height = height;
	    slAddHead(&miList, mi);
	    }
        }
    }

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

char *getTrackMafFile(struct track *track)
/* look up MAF file name in track setting, return NULL if not found */
{
return hashFindVal(track->tdb->settingsHash, "mafFile");
}

char *getCustomMafFile(struct track *track)
{
char *fileName = getTrackMafFile(track);
if (fileName == NULL)
    errAbort("cannot find custom maf setting");
return fileName;
}


static void loadMafsToTrack(struct track *track)
/* load mafs in region to track custom pointer */
{
struct sqlConnection *conn;
struct sqlConnection *conn2;
struct mafPriv *mp = getMafPriv(track);

if (inSummaryMode(cart, track->tdb, winBaseCount))
    return;

int begin = winStart - 2;
if (begin < 0)
    begin = 0;

if (track->isBigBed)
    {
    struct bbiFile *bbi = fetchBbiForTrack(track);
    mp->list = bigMafLoadInRegion(bbi, chromName, begin, winEnd+2);
    bbiFileClose(&bbi);
    track->bbiFile = NULL;
    }
else if (mp->ct)
    {
/* we open two connections to the database
 * that has the maf track in it.  One is
 * for the scoredRefs, the other to access
 * the extFile database.  We could get away
 * with just one connection, but then we'd
 * have to allocate more memory to hold
 * the scoredRefs (whereas now we just use
 * one statically loaded scoredRef).
 */

    char *fileName = getCustomMafFile(track);

    conn = hAllocConn(CUSTOM_TRASH);
    conn2 = hAllocConn(CUSTOM_TRASH);
    mp->list = wigMafLoadInRegion(conn, conn2, mp->ct->dbTableName,
				chromName, begin, winEnd + 2, fileName);
    hFreeConn(&conn);
    hFreeConn(&conn2);
    }
else
    {
    char *fileName = getTrackMafFile(track);  // optional
    conn = hAllocConn(database);
    conn2 = hAllocConn(database);
    mp->list = wigMafLoadInRegion(conn, conn2, track->table,
				chromName, begin, winEnd + 2, fileName);
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

struct hash *labelHash = mafGetLabelHash(track->tdb);

loadMafsToTrack(track);

/* NOTE: we are building up the item list backwards */

/* Add items for conservation wiggles */
struct track *wigTrack = track->subtracks;
if (wigTrack)
    {
    enum trackVisibility wigVis = (wigTrack->limitedVis == tvDense ? tvDense : tvFull);
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
mi = newMafItem(database, 0, lowerFirstChar, labelHash);
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
return !inSummaryMode(cart, track->tdb, winBaseCount) || isCustomTrack(track->table) ||
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
char *snpTable = trackDbSetting(track->tdb, "snpTable");
boolean doSnpTable = FALSE;
if ( (track->limitedVis == tvPack) && (snpTable != NULL) && 
    cartOrTdbBoolean(cart, track->tdb, MAF_SHOW_SNP,FALSE))
    doSnpTable = TRUE;

// the maf's only get loaded if we're not in summary or snpTable views
if (!doSnpTable && !inSummaryMode(cart, track->tdb, winBaseCount))
    {
    /* "close in" display uses actual alignments from file */
    struct mafPriv *mp = getMafPriv(track);
    struct sqlConnection *conn, *conn2;

    if (track->isBigBed)
        {
        struct bbiFile *bbi = fetchBbiForTrack(track);
        mp->list = bigMafLoadInRegion(fetchBbiForTrack(track), chromName, winStart, winEnd);
        bbiFileClose(&bbi);
        track->bbiFile = NULL;
        }
    else if (mp->ct)
	{
	char *fileName = getCustomMafFile(track);
	conn = hAllocConn(CUSTOM_TRASH);
	conn2 = hAllocConn(CUSTOM_TRASH);
	mp->list = wigMafLoadInRegion(conn, conn2, mp->ct->dbTableName,
					chromName, winStart, winEnd, fileName);
	hFreeConn(&conn);
	hFreeConn(&conn2);
	}
    else
	{
        char *fileName = getTrackMafFile(track);  // optional
	conn = hAllocConn(database);
	conn2 = hAllocConn(database);
	mp->list = wigMafLoadInRegion(conn, conn2, track->table,
					chromName, winStart, winEnd, fileName);
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
    speciesItems = newSpeciesItems(track, track->limitedVis == tvFull ?
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

/* if we're out in summary view and rendering a custom
 * track we force dense mode since we don't have
 * a summary table (yet). */
if (inSummaryMode(cart, track->tdb, winBaseCount) && isCustomTrack(track->table))
    track->limitedVis = tvDense;

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
char *defaultOn = trackDbSetting(track->tdb, "speciesDefaultOn");
char *defaultMaf = trackDbSetting(track->tdb, "defaultMaf");

// check to see if all species that are on are in defaultMaf
if (defaultOn && defaultMaf && hTableExists(database, defaultMaf))
    {
    struct wigMafItem *speciesList = newSpeciesItems(track, tl.fontHeight), *wmi;
    struct slName *defaultNames = slNameListFromString(defaultOn, ' ');
    
    boolean allOnInDefault = TRUE;
    for(wmi = speciesList; wmi; wmi = wmi->next)
        {
        if (!slNameInList(defaultNames, wmi->db))
            allOnInDefault = FALSE;
        }

    if (allOnInDefault)
        track->table = defaultMaf;
    }

/* Make up tracks for display. */
if (track->limitedVis == tvFull || track->limitedVis == tvPack)
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
else if (track->limitedVis == tvSquish)
    {
    if (!wigTrack)
        {
        scoreHeight = tl.fontHeight * 4;
        if (!inSummaryMode(cart, track->tdb, winBaseCount))
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
        if (!inSummaryMode(cart, track->tdb, winBaseCount))
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

// Make sure visibility takes into account any composite track with multi-views
track->limitedVis = tvMin(track->visibility,limitedVisFromComposite(track));
track->limitedVisSet = (track->limitedVis != tvHide && track->visibility != track->limitedVis);

miList = loadWigMafItems(track, zoomedToBaseLevel);
track->items = miList;

while (wigTrack != NULL)
    // load wiggle subtrack items
    {
    /* update track visibility from parent track,
     * since hgTracks will update parent vis before loadItems */
    wigTrack->visibility = track->visibility;
    wigTrack->limitedVis = track->limitedVis;
    wigTrack->loadItems(wigTrack);
    wigTrack = wigTrack->next;
    }
}

static int wigMafTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct wigMafItem *mi;
int total = 0,count=0;
for (mi = track->items; mi != NULL; mi = mi->next)
    {
    total += mi->height;
    count++;
    }

track->height = total;
if (track->limitedVis == tvDense)
    track->height=(tl.fontHeight+1)*count; // Evidence that track->height is 9 but should be 10!
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
return hubConnectSkipHubPrefix(mi->name);
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
    if (isContigOrTandem(ms->leftStatus[0]) ||
	 ms->leftStatus[0] == MAF_INSERT_STATUS)
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
	if (isContigOrTandem(ms->rightStatus[0]) ||
	     ms->rightStatus[0] == MAF_INSERT_STATUS)
	    {
	    isDouble = (ms->rightStatus[0] == MAF_INSERT_STATUS);
	    x1 = round((double)((int)ms->chromEnd+1 - seqStart) * scale) + xOff;
	    x2 = round((double)((int)ms->next->chromStart-1 - seqStart) * scale)
		    + xOff;
	    w = x2 - x1;
	    if (w == 1 && x1 == lastX)
		continue;
	    if (w > 0)
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

static void drawScoreOverviewBig( struct track *track,  int height,
                               int seqStart, int seqEnd,
                               struct hvGfx *hvg, int xOff, int yOff,
                               int width, MgFont *font,
                               Color color, Color altColor,
                               enum trackVisibility vis)
/* Draw density plot or graph for overall maf scores rather than computing
 * by sections, for speed.  Don't actually load the mafs -- just
 * the scored refs from the table.
 */
{
struct lm *lm = lmInit(0);
char *fileName = trackDbSetting(track->tdb, "summary");
if (fileName == NULL)
    {
    warn("cannot find summary information in trackDb for track '%s'", track->track);
    return;
    }
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, seqStart, seqEnd, 0, lm);
char *bedRow[7];
char startBuf[16], endBuf[16];
double scale = scaleForPixels(width);

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct mafSummary *ms;
    ms = mafSummaryLoad(bedRow);
    drawScore(ms->score, ms->chromStart, ms->chromEnd, seqStart, scale,
                    hvg, xOff, yOff, height, color, vis);

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
struct sqlConnection *conn = hAllocConn(database);

drawScoreOverviewC(conn, tableName, height, seqStart, seqEnd,
	hvg, xOff, yOff, width, font, color, altColor, vis);

hFreeConn(&conn);
}

static void drawScoreOverviewCT(char *tableName,
                                int height, int seqStart, int seqEnd,
                                struct hvGfx *hvg, int xOff, int yOff,
                                int width, MgFont *font,
                                Color color, Color altColor,
                                enum trackVisibility vis)
{
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);

drawScoreOverviewC(conn, tableName, height, seqStart, seqEnd,
	hvg, xOff, yOff, width, font, color, altColor, vis);

hFreeConn(&conn);
}


static boolean drawPairsFromSnpTable(struct track *track,
	char *snpTable,
        int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* use trackDb variable "snpTable" bed to draw this mode */
{
struct wigMafItem *miList = track->items, *mi = miList;
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct hash *componentHash = newHash(6);
struct hashEl *hel;
struct dyString *where = dyStringNew(256);
char *whereClause = NULL;

if (miList == NULL)
    return FALSE;

if (snpTable == NULL)
    return FALSE;

/* Create SQL where clause that will load up just the
 * beds for the species that we are including. */
conn = hAllocConn(database);
sqlDyStringPrintf(where, "name in (");
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (!isPairwiseItem(mi))
	/* exclude non-species items (e.g. conservation wiggle */
	continue;
    sqlDyStringPrintf(where, "'%s'", mi->db);
    if (mi->next != NULL)
	sqlDyStringPrintf(where, ",");
    }
sqlDyStringPrintf(where, ")");
/* check for empty where clause */
if (!sameString(where->string,"name in ()"))
    whereClause = where->string;
sr = hOrderedRangeQuery(conn, snpTable, chromName, seqStart, seqEnd,
                        whereClause, &rowOffset);

/* Loop through result creating a hash of lists of beds .
 * The hash is keyed by species. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoadN(&row[1], 5);
    /* prune to fit in window bounds */
    if (bed->chromStart < seqStart)
        bed->chromStart = seqStart;
    if (bed->chromEnd > seqEnd)
        bed->chromEnd = seqEnd;
    if ((hel = hashLookup(componentHash, bed->name)) == NULL)
        hashAdd(componentHash, bed->name, bed);
    else
        slAddHead(&(hel->val), bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

/* display pairwise items */
Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
	/* ignore item for the score */
	continue;
    struct bed *bedList = (struct bed *)hashFindVal(componentHash, mi->db);
    if (bedList == NULL)
	bedList = (struct bed *)hashFindVal(componentHash, mi->name);

    if (bedList != NULL)
	{
	hvGfxSetClip(hvg, xOff, yOff, width, mi->height);

	struct bed *bed = bedList;

	double scale = scaleForPixels(width);
	for(; bed; bed = bed->next)
	    {
	    int x1 = round((bed->chromStart - seqStart)*scale);
	    int x2 = round((bed->chromEnd - seqStart)*scale);
	    int w = x2-x1+1;
	    int height1 = mi->height-1;
	    int color = MG_BLACK;
	    switch(bed->score)
		{
		case 222:  // place holder for heterozygous nonsynonymous
		    color = 0xff000000;
		    break;
		case 111:  // place holder for heterozygous synonymous
		    color = 0xffd0e040;
		    break;
		case 0:
		    color = yellow;
		    break;
		case _5_prime_UTR_variant:
		case _3_prime_UTR_variant:
		case coding_sequence_variant:
		case intron_variant:
		case intergenic_variant:
		    color = MG_BLUE;
		    break;
		case synonymous_variant:
		    color = MG_GREEN;
		    break;
		case missense_variant:
		    color = MG_RED;
		    break;
                // non-syn BLOSUM62 > 1
                case 1579:
                    color = 0xffe0e0ff;
		    break;
                // non-syn BLOSUM62 > -1  but less than 1
                case 1581:
                    color = 0xffb0b0ff;
		    break;
		}
	    hvGfxBox(hvg, x1 + xOff, yOff, w, height1, color);

	    }
	hvGfxUnclip(hvg);
	}
    yOff += mi->height;
    }
return TRUE;
}

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

if (miList == NULL)
    return FALSE;

/* get summary table name from trackDb */
if ((summary = summarySetting(track)) == NULL)
    return FALSE;

if (cartVarExistsAnyLevel(cart, track->tdb,FALSE,MAF_CHAIN_VAR))
    useIrowChains = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, MAF_CHAIN_VAR,TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
    }

if (track->isBigBed)
    {
    struct lm *lm = lmInit(0);
    char *fileName = trackDbSetting(track->tdb, "summary");
    struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
    struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, seqStart, seqEnd, 0, lm);
    char *bedRow[7];
    char startBuf[16], endBuf[16];

    for (bb = bbList; bb != NULL; bb = bb->next)
        {
        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
        ms = mafSummaryLoad(bedRow);
        if ((hel = hashLookup(componentHash, ms->src)) == NULL)
            hashAdd(componentHash, ms->src, ms);
        else
            slAddHead(&(hel->val), ms);
        }
    bbiFileClose(&bbi);
    }
else 
    {
    /* Create SQL where clause that will load up just the
     * summaries for the species that we are including. */
    conn = hAllocConn(database);
    sqlDyStringPrintf(where, "src in (");
    for (mi = miList; mi != NULL; mi = mi->next)
        {
        if (!isPairwiseItem(mi))
            /* exclude non-species items (e.g. conservation wiggle */
            continue;
        sqlDyStringPrintf(where, "'%s'", mi->db);
        if (mi->next != NULL)
            sqlDyStringPrintf(where, ",");
        }
    sqlDyStringPrintf(where, ")");
    /* check for empty where clause */
    if (!sameString(where->string,"src in ()"))
        whereClause = where->string;
    sr = hOrderedRangeQuery(conn, summary, chromName, seqStart, seqEnd,
                            whereClause, &rowOffset);

    boolean hasFieldLeftStatus = hHasField(database, summary, "leftStatus");

    /* Loop through result creating a hash of lists of maf summary blocks.
     * The hash is keyed by species. */
    while ((row = sqlNextRow(sr)) != NULL)
        {
        if (hasFieldLeftStatus)
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
    }

/* reverse summary lists */
cookie = hashFirst(componentHash);
while ((hel = hashNext(&cookie)) != NULL)
    slReverse(&hel->val);
if (!track->isBigBed)
    hFreeConn(&conn);

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
    summaryList = (struct mafSummary *)hashFindVal(componentHash, mi->db);
    if (summaryList == NULL)
        {
        // sometimes the summary table has the other database without a dot even if it has one
        char *tryNoDot = cloneString(mi->db);
        char *dot = strchr(tryNoDot, '.');
        if (dot != NULL)
            {
            *dot = 0;
            summaryList = (struct mafSummary *)hashFindVal(componentHash, tryNoDot);
            }
        }
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

if (vis == tvFull && !isCustomTrack(track->table))
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
    if (isCustomTrack(track->table))
	{
	struct mafPriv *mp = getMafPriv(track);
	drawScoreOverviewCT(mp->ct->dbTableName, mi->height,
                            seqStart, seqEnd,
                            hvg, xOff, yOff, width, font, color, color, vis);
	hvGfxUnclip(hvg);
	}
    else if (vis == tvFull)
        {
        /* get wiggle table, of pairwise
           for example, percent identity */
        tableName = getWigTablename(mi->name, suffix);
        if (!hTableExists(database, tableName))
            tableName = getWigTablename(mi->db, suffix);
        if (hTableExists(database, tableName))
            {
            /* reuse the wigTrack for pairwise tables */
            wigTrack->track = tableName;
            wigTrack->table = tableName;
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
            if (!hTableExists(database, tableName))
                tableName = getMafTablename(mi->db, suffix);
            if (hTableExists(database, tableName))
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
	if (!hTableExists(database, tableName))
	    tableName = getMafTablename(mi->db, suffix);
	if (hTableExists(database, tableName))
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
boolean doSnpMode = (vis == tvPack) &&(trackDbSetting(track->tdb, "snpMode") != NULL);

struct mafPriv *mp = getMafPriv(track);
if (miList == NULL || mp->list == NULL)
    return FALSE;

if (cartVarExistsAnyLevel(cart, track->tdb,FALSE,MAF_CHAIN_VAR))
    useIrowChains = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, MAF_CHAIN_VAR,TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
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

        mcThis = mafFindCompSpecies(maf, hubConnectSkipHubPrefix(database), '.');
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
                         useIrowChains, doSnpMode);
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
    if (isCustomTrack(track->table) || pairwiseSuffix(track))
        return drawPairsFromPairwiseMafScores(track, seqStart, seqEnd, hvg,
                                        xOff, yOff, width, font, color, vis);
    if (inSummaryMode(cart, track->tdb, winBaseCount) && summarySetting(track))
        return drawPairsFromSummary(track, seqStart, seqEnd, hvg,
                                        xOff, yOff, width, font, color, vis);
    char *snpTable = trackDbSetting(track->tdb, "snpTable");
    if ( (track->limitedVis == tvPack) && (snpTable != NULL))
        return drawPairsFromSnpTable(track, snpTable, seqStart, seqEnd, hvg,
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

struct sqlClosure
{
char *mafFile;
struct sqlConnection *conn2;
struct sqlConnection *conn3;
char *tableName;
};

struct bbClosure
{
struct bbiFile *bbi;
};

typedef struct mafAli * (*mafRetrieveFunc)(void *closure,  char *chrom, int start, int end);

static struct mafAli *mafLoadFromBb(void *closure, char *chrom, int start, int end)
/* Retrieve maf blocks from a bigBed file. */
{
struct bbClosure *bbClosure = closure;

return bigMafLoadInRegion( bbClosure->bbi, chrom, start, end);
}

static struct mafAli *mafLoadFromSql(void *closure, char *chrom, int start, int end)
/* Retrieve maf blocks from an SQL table. */
{
struct sqlClosure *sqlClosure = closure;

return mafLoadInRegion2(sqlClosure->conn2, sqlClosure->conn3, sqlClosure->tableName, chrom, start, end, sqlClosure->mafFile  );
}

static void translateCodons(mafRetrieveFunc func, void *closure,
                            char *compName,
                            DNA *dna, int start, int length, int frame, char strand,
                            int prevEnd, int nextStart, bool alreadyComplemented,
                            int x, int y, int width, int height, struct hvGfx *hvg)
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

safef(masterChrom, sizeof(masterChrom), "%s.%s", hubConnectSkipHubPrefix(database), chromName);

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
	    ali = func(closure,  chromName, prevEnd , prevEnd + 1);
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
                ali = func(closure,  chromName, prevEnd , prevEnd + 2);
		if (ali != NULL)
		    sub = mafSubset(ali, masterChrom, prevEnd, prevEnd + 2  );
		}
	    else
		{
                ali = func(closure,  chromName, prevEnd - 1 , prevEnd + 1);
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
        ali = func(closure,  chromName, nextStart - 2 + length, nextStart + 1);
	if (ali != NULL)
	    sub = mafSubset(ali, masterChrom,
		nextStart - 2 + length, nextStart + 1);
	}
    else
	{
        ali = func(closure,  chromName, nextStart , nextStart + 2);
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

static struct mafFrames *getFramesFromSql(  char *framesTable,
    char *chromName, int seqStart, int seqEnd, char *extra, boolean newTableType)
{
struct mafFrames *mfList = NULL;
int rowOffset;
struct sqlResult *sr;
struct mafFrames *mf;
char **row;

struct sqlConnection *conn = hAllocConn(database);
sr = hRangeQuery(conn, framesTable, chromName, seqStart, seqEnd, extra, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (newTableType)
        mf = mafFramesLoad(row + rowOffset);
    else
        mf = mafFramesLoadOld(row + rowOffset);
    slAddHead(&mfList, mf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return mfList;
}

static struct mafFrames *getFramesFromBb(  char *framesTable, char *chromName, int seqStart, int seqEnd, char *component)
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  bigBedFileOpenAlias(framesTable, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, seqStart, seqEnd, 0, lm);
char *bedRow[11];
char startBuf[16], endBuf[16];
struct mafFrames *mfList = NULL, *mf;

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    if (sameString(bedRow[3], component))
        {
        mf = mafFramesLoad( bedRow );
        slAddHead(&mfList, mf);
        }
    }

bbiFileClose(&bbi);
slReverse(&mfList);
return mfList;
}

static char * extraPrevious = NULL;

static void resetCache()
// reset the frames cache
{
extraPrevious = NULL;
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
char **lines = NULL, *selfLine;
int *insertCounts;
int i, x = xOff, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
struct hash *srcHash = newHash(0);
char dbChrom[64];
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

int mafOrigOffset = 0;
struct mafPriv *mp = getMafPriv(track);
char *mafFile = NULL;
struct sqlConnection *conn2 = NULL;
struct sqlConnection *conn3 = NULL;
char *tableName = NULL;

if (mp->ct != NULL)
    {
    conn2 = hAllocConn(CUSTOM_TRASH);
    conn3 = hAllocConn(CUSTOM_TRASH);
    tableName = mp->ct->dbTableName;
    mafFile = getCustomMafFile(track);
    }
else if (!track->isBigBed)
    {
    conn2 = hAllocConn(database);
    conn3 = hAllocConn(database);
    tableName = track->table;
    mafFile = getTrackMafFile(track);  // optional
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

if (cartVarExistsAnyLevel(cart, track->tdb,FALSE,MAF_DOT_VAR))
    dots = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, MAF_DOT_VAR,TRUE);
else
    {
    char *dotString = trackDbSetting(track->tdb, MAF_DOT_VAR);
    if (dotString && sameString(dotString, "on"))
	{
	dots = TRUE;
	}
    }

if (cartVarExistsAnyLevel(cart, track->tdb,FALSE,"frames"))
    framesTable = cartOptionalStringClosestToHome(cart, track->tdb,FALSE,"frames");
else
    framesTable = trackDbSetting(track->tdb, "frames");

if (framesTable)
    {
    codonTransMode = cartUsualStringClosestToHome(cart, track->tdb,FALSE,"codons", "codonDefault");
    if (sameString("codonNone", codonTransMode))
	framesTable = NULL;
    }

boolean newTableType = FALSE;

if (framesTable != NULL)
    {
    if (track->isBigBed)
        newTableType = TRUE;
    else
        newTableType = hHasField(database, framesTable, "isExonStart");
    }

/* initialize "no alignment" string to o's */
for (i = 0; i < sizeof noAlignment - 1; i++)
    noAlignment[i] = UNALIGNED_SEQ;


if (cartVarExistsAnyLevel(cart, track->tdb,FALSE,MAF_CHAIN_VAR))
    useIrowChains = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, MAF_CHAIN_VAR,TRUE);
else
    {
    char *irowString = trackDbSetting(track->tdb, "irows");
    if (irowString && sameString(irowString, "off"))
	useIrowChains = FALSE;
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
selfLine = lines[1];

/* Allocate a line for recording gap sizes in reference */
AllocArray(insertCounts, alignLineLength);

/* Load up self-line with DNA */
seq = hChromSeqMixed(database, chromName, seqStart , seqEnd);
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
safef(dbChrom, sizeof(dbChrom), "%s.%s", hubConnectSkipHubPrefix(database), chromName);

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
                if (isContigOrTandem(mc->leftStatus) &&
                    isContigOrTandem(mc->rightStatus))
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
	struct dyString *label = dyStringNew(20);
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
    alternateColorBaseCount =
            cartUsualIntClosestToHome(cart, track->tdb, FALSE, BASE_COLORS_VAR, 0);
    alternateColorBaseOffset =
            cartUsualIntClosestToHome(cart, track->tdb, FALSE, BASE_COLORS_OFFSET_VAR, 0);
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
    /* HAVE DONE:  David doesn't like lower case by default
     * TODO: casing based on quality values ?? */
    alignSeqToUpperN(line);
    if (complementBases)
        {
	complement(line, strlen(line));
        }
    if (genomeIsRna)
        toRna(line);
    /* draw sequence letters for alignment */
    hvGfxSetClip(hvg, x, y-1, width, mi->height);

    /* having these static here will allow the option 'codonDefault'
     * to run rapidly through the display without extra calls to SQL
     * for the same answer over and over.  For other codon modes, it
     * will still need to query the Frames table repeatedly.
     * This was found to speed up dramatically by adding an index to
     * the table:
     * hgsql staAur2 -e 'CREATE INDEX src on multiz369wayFrames (src, bin);'
     */
    static struct mafFrames *mfList = NULL, *mf;
    static boolean found = FALSE;
    if (framesTable != NULL)
	{
	char extra[512];

	if (sameString("codonDefault", codonTransMode))
	    {
	    sqlSafef(extra, sizeof(extra), "src='%s'",defaultCodonSpecies);

	    found = TRUE;
	    }
	else if (sameString("codonFrameDef", codonTransMode))
	    {
	    sqlSafef(extra, sizeof(extra), "src='%s'",mi->db);

	    found = FALSE;
	    }
	else if (sameString("codonFrameNone", codonTransMode))
	    {
	    sqlSafef(extra, sizeof(extra), "src='%s'",mi->db);

	    found = TRUE;
	    }
	else
	    errAbort("unknown codon translation mode %s",codonTransMode);
tryagain:
        if (differentStringNullOk(extraPrevious, extra))
            {
            if (track->isBigBed)
                {
                char *species = mi->db;
                if (sameString("codonDefault", codonTransMode))
                    species = defaultCodonSpecies;
                mfList = getFramesFromBb(  framesTable, chromName, seqStart, seqEnd, species);
                }
            else
                mfList = getFramesFromSql(  framesTable, chromName, seqStart, seqEnd, extra, newTableType);
            }
        extraPrevious = cloneString(extra);

        if (mfList != NULL)
            found = TRUE;

        for(mf = mfList; mf; mf = mf->next)
            {
            int start, end, w;
            int frame;
            if (mf->chromStart < seqStart)
                start = 0;
            else
                start = mf->chromStart-seqStart;
            frame = mf->frame;
            if (mf->strand[0] == '-')
                {
                if (mf->chromEnd > seqEnd)
                    frame = (frame + mf->chromEnd-seqEnd  ) % 3;
                }
            else
                {
                if (mf->chromStart < seqStart)
                    frame = (frame + seqStart-mf->chromStart  ) % 3;
                }

            end = mf->chromEnd > seqEnd ? seqEnd - seqStart  : mf->chromEnd - seqStart;
            w= end - start;

            void *closure;
            mafRetrieveFunc func;
            struct sqlClosure sqlClosure;
            struct bbClosure bbClosure;

            if (track->isBigBed)
                {
                func =  mafLoadFromBb;
                closure = &bbClosure;
                bbClosure.bbi = fetchBbiForTrack(track);
                }
            else
                {
                func =  mafLoadFromSql;
                closure = &sqlClosure;
                sqlClosure.conn2 = conn2;
                sqlClosure.conn3 = conn3;
                sqlClosure.mafFile = mafFile;
                sqlClosure.tableName = tableName;
                }
            translateCodons(func, closure, mi->db, line, start,
                            w, frame, mf->strand[0],mf->prevFramePos,mf->nextFramePos,
                            complementBases, x, y, width, mi->height,  hvg);
            }

	if (!found)
	    {
	    /* try the default species */
	    sqlSafef(extra, sizeof(extra), "src='%s'",defaultCodonSpecies);

	    found = TRUE; /* don't try again */
	    goto tryagain;
	    }
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
hFreeConn(&conn2);
hFreeConn(&conn3);
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
    boolean doSnpMode = (vis == tvPack) &&(trackDbSetting(track->tdb, "snpMode") != NULL);
    /* no wiggle */
    int height = tl.fontHeight * 4;
    if (vis == tvFull || vis == tvPack)
        /* suppress graph if other items displayed (bases or pairs) */
        return yOff;
    else if (vis == tvDense)
        height = track->height; // Evidence that track-height comes in as 9 but should be 10!
    /* draw some kind of graph from multiple alignment */
    struct mafPriv *mp = getMafPriv(track);
    if (mp->list != (char *)-1 && mp->list != NULL)
        {
        /* use mafs */
        drawMafRegionDetails(mp->list, height, seqStart, seqEnd,
                                hvg, xOff, yOff, width, font,
                                color, color, scoreVis, FALSE, FALSE,
				doSnpMode);
        }
    else if (mp->ct != NULL)
        {
        /* use or scored refs from maf table*/
        drawScoreOverviewCT(mp->ct->dbTableName, height, seqStart, seqEnd,
		hvg, xOff, yOff, width, font, color, color, scoreVis);
        yOff++;
        }
    else
        {
        /* use or scored refs from maf table*/
        if (differentString(track->tdb->type, "bigMaf"))
            drawScoreOverview(track->table, height, seqStart, seqEnd, hvg,
                            xOff, yOff, width, font, color, color, scoreVis);
        else
            drawScoreOverviewBig(track, height, seqStart, seqEnd, hvg,
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
        resetCache();

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
mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width, track->height, track->track,
            track->track, NULL);
}

void wigMafMethods(struct track *track, struct trackDb *tdb,
                                        int wordCount, char *words[])
/* Make track for maf multiple alignment. */
{
struct track *wigTrack;
int i;
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
consWigList = wigMafWiggles(database, tdb);
if (consWigList == NULL)
    return;

/* determine which conservation wiggles to use -- from cart,
 or if none there, use first entry in trackDb setting */
boolean first = TRUE;
for (consWig = consWigList; consWig != NULL; consWig = consWig->next)
    {
    if (differentString(consWig->leftLabel, DEFAULT_CONS_LABEL))
        {
        char *wigVarSuffix = NULL;
        (void)wigMafWiggleVar(tdb->track, consWig,&wigVarSuffix);
        if (!cartUsualBooleanClosestToHome(cart, tdb, FALSE, wigVarSuffix, first))
            continue;
        }
    first = FALSE;

    //  Manufacture and initialize wiggle subtrack, both tdb and track
    struct trackDb *wigTdb = CloneVar(tdb);
    wigType = dyStringNew(64);
    dyStringPrintf(wigType, "type wig ");
    for (i = 1; i < wordCount; i++)
        dyStringPrintf(wigType, "%s ", words[i]);
    wigTdb->type = cloneString(wigType->string);
    wigTdb->track = consWig->table;
    wigTdb->table= consWig->table;

    /* Tweak wiggle left labels: replace underscore with space and
     * append 'Cons' */
    struct dyString *ds = dyStringNew(0);
    dyStringAppend(ds, consWig->leftLabel);
    if (differentString(consWig->leftLabel, DEFAULT_CONS_LABEL))
        dyStringAppend(ds, " Cons");
    wigTdb->shortLabel = dyStringCannibalize(&ds);
    subChar(wigTdb->shortLabel, '_', ' ');

    wigTrack = trackFromTrackDb(wigTdb);


    /* setup wiggle methods in subtrack */
    wigMethods(wigTrack, tdb, wordCount, words);

    wigTrack->mapsSelf = FALSE;
    wigTrack->drawLeftLabels = NULL;
    wigTrack->next = NULL;
    slAddTail(&track->subtracks, wigTrack);
    dyStringFree(&wigType);
    }
}


