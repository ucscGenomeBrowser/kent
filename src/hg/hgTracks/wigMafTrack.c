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
#include "hgMaf.h"
#include "mafTrack.h"

static char const rcsid[] = "$Id: wigMafTrack.c,v 1.49 2004/11/10 22:33:36 hiram Exp $";

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

Color wigMafItemLabelColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw a maf item based on the species group it is in */
{
return (((struct wigMafItem *)item)->group % 2 ? 
                        //lighterColor(vg, tg->ixColor) : tg->ixColor);
                        tg->ixAltColor : tg->ixColor);
}

struct mafAli *wigMafLoadInRegion(struct sqlConnection *conn, 
	char *table, char *chrom, int start, int end)
/* Load mafs from region */
{
    return mafLoadInRegion(conn, table, chrom, start, end);
}

static boolean dbPartOfName(char *name, char *retDb, int retDbSize)
/* Parse out just database part of name (up to but not including
 * first dot). If dot found, return entire name */
{
int len;
char *e = strchr(name, '.');
/* Put prefix up to dot into buf. */
len = (e == NULL ? strlen(name) : e - name);
if (len >= retDbSize)
     len = retDbSize-1;
memcpy(retDb, name, len);
retDb[len] = 0;
return TRUE;
}


static struct wigMafItem *newMafItem(char *s, int g)
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
*mi->name = tolower(*mi->name);
mi->height = tl.fontHeight;
mi->group = g;
return mi;
}

struct wigMafItem *newSpeciesItems(struct track *track, int height)
/* Make up item list for all species configured in track settings */
{
char option[64];
char *species[100];
char *groups[20];
char sGroup[24];
struct wigMafItem *mi = NULL, *miList = NULL;
int group;
int i;
int speciesCt = 0, groupCt = 1;
/* either speciesOrder or speciesGroup is specified in trackDb */
char *speciesOrder = trackDbSetting(track->tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(track->tdb, SPECIES_GROUP_VAR);

if (speciesOrder == NULL && speciesGroup == NULL)
    errAbort(
      "Track %s missing required trackDb setting: speciesOrder or speciesGroup",
                track->mapName);
if (speciesGroup)
    groupCt = chopLine(speciesGroup, groups);

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
    speciesCt = chopLine(speciesOrder, species);
    for (i = 0; i < speciesCt; i++)
        {
        /* skip this species if UI checkbox was unchecked */
        safef(option, sizeof(option), "%s.%s", track->mapName, species[i]);
        if (!cartUsualBoolean(cart, option, TRUE))
            continue;
        mi = newMafItem(species[i], group);
        slAddHead(&miList, mi);
        }
    }
for (mi = miList; mi != NULL; mi = mi->next)
    mi->height = height;
return miList;
}

static struct wigMafItem *scoreItem(scoreHeight)
/* Make up (wiggle) item that will show the score */
{
struct wigMafItem *mi;

AllocVar(mi);
mi->name = cloneString("Conservation");
mi->height = scoreHeight;
return mi;
}

static struct wigMafItem *loadBaseByBaseItems(struct track *track)
/* Make up base-by-base track items. */
{
struct wigMafItem *miList = NULL, *speciesList = NULL, *mi;
struct sqlConnection *conn = hAllocConn();
int i;

/* load up mafs */
track->customPt = wigMafLoadInRegion(conn, track->mapName, 
                                        chromName, winStart, winEnd);
hFreeConn(&conn);

/* Add item for score wiggle base alignment */
if (track->subtracks != NULL)
    {
    enum trackVisibility wigVis = 
    	(track->visibility == tvDense ? tvDense : tvFull);
    mi = scoreItem(wigTotalHeight(track->subtracks, wigVis));
    slAddHead(&miList, mi);
    }
/* Make up item that will show gaps in this organism. */
AllocVar(mi);
mi->name = "Gaps";
mi->height = tl.fontHeight;
slAddHead(&miList, mi);

/* Make up item for this organism. */
mi = newMafItem(database, 0);
slAddHead(&miList, mi);

/* Make items for other species */
speciesList = newSpeciesItems(track, tl.fontHeight);
miList = slCat(speciesList, miList);

slReverse(&miList);
return miList;
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
int pairwiseHeight;

if (wigTrack == NULL)
    return 0;
settings = cloneString(trackDbSetting(track->tdb, PAIRWISE_VAR));
if (settings == NULL)
    return 0;

/* get height for pairwise wiggles */
/* default height */
pairwiseHeight = max(wigTotalHeight(wigTrack, tvFull)/3 - 1, tl.fontHeight);
wordCount = chopLine(settings, words);
if (wordCount > 1)
    {
    int height = atoi(words[1]);
    if (height < tl.fontHeight)
       pairwiseHeight = tl.fontHeight;
    else if (height > wigTotalHeight(wigTrack, tvFull))
        pairwiseHeight = wigTotalHeight(wigTrack, tvFull);
    else
        pairwiseHeight = height;
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

static struct wigMafItem *loadPairwiseItems(struct track *track)
/* Make up items for modes where pairwise data are shown.
   First an item for the score wiggle, then a pairwise item
   for each "other species" in the multiple alignment.
   These may be density plots (pack) or wiggles (full). */
{
struct wigMafItem *miList = NULL, *speciesItems = NULL, *mi;
struct track *wigTrack = track->subtracks;
char *suffix;

if (wigTrack != NULL)
    {
    mi = scoreItem(wigTotalHeight(wigTrack, tvFull));
    /* mark this as not a pairwise item */
    mi->ix = -1;
    slAddHead(&miList, mi);
    }
suffix = pairwiseSuffix(track);
if (suffix != NULL)
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
struct wigMafItem *miList = NULL;

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
/* Make up tracks for display. */
if (isBaseLevel)
    {
    miList = loadBaseByBaseItems(track);
    }
/* zoomed out */
else if (track->visibility == tvFull || track->visibility == tvPack)
    {
    miList = loadPairwiseItems(track);
    }
else if (track->visibility == tvSquish)
    {
    miList = scoreItem(wigTotalHeight(track->subtracks, tvFull));
    }
else 
    {
    /* dense mode, zoomed out - show density plot, with track label */
    AllocVar(miList);
    miList->name = cloneString(track->shortLabel);
    miList->height = tl.fontHeight;
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

if (wigTrack != NULL) 
    // load wiggle subtrack items
    {
    /* update track visibility from parent track,
     * since hgTracks will update parent vis before loadItems */
    wigTrack->visibility = track->visibility;
    wigTrack->loadItems(wigTrack);
    }
}

static int wigMafTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct wigMafItem *mi;
int total = 0;
for (mi = track->items; mi != NULL; mi = mi->next)
    total += mi->height;
track->height =  total;
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
if (track->customPt != NULL)
    mafAliFreeList((struct mafAli **)&track->customPt);
if (track->items != NULL)
    wigMafItemFreeList((struct wigMafItem **)&track->items);
}

static char *wigMafItemName(struct track *track, void *item)
/* Return name of maf level track. */
{
struct wigMafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, int textSize, 
	                        int insertCounts[], int baseCount)
/* Make up insert line from sequence of reference species.  
   It has a gap count at each displayed base position, and is generated by
   counting up '-' chars in the sequence  */
{
int i, baseIx = 0;
char c;
for (i=0; i < textSize && baseIx < baseCount; i++)
    {
    c = text[i];
    if (c == '-')
	insertCounts[baseIx]++;
    else
        baseIx++;
    }
}

static void charifyInserts(char *insertLine, int insertCounts[], int size)
/* Create insert line from insert counts */
{
int i;
char c;
for (i=0; i<size; ++i)
    {
    int b = insertCounts[i];
    if (b == 0)
       c = ' ';
    else if (b <= 9)
       c = b + '0';
    else if (b % 3)
        /* multiple of 3 gap */
       c = '+';
    else
       c = '*';
    insertLine[i] = c;
    }
}

static void processSeq(char *text, char *masterText, int textSize,
                            char *outLine, int offset, int outSize)
/* Add text to outLine, suppressing copy where there are dashes
 * in masterText.  This effectively projects the alignment onto
 * the master genome.
 * If no dash exists in this sequence, count up size
 * of the insert and save in the line.
 */
{
int i, outIx = 0, outPositions = 0;
int insertSize = 0, previousInserts = 0;

/* count up insert counts in the existing line -- need to 
   add these to determine where to start this sequence in the line */
for (i=0; outLine[i]; i++)
    if (outLine[i] == '|')
        {
        previousInserts++;
        i++;    /* skip count after escape char */
        }
outLine = outLine + offset + previousInserts * 2;
for (i=0; i < textSize && outPositions < outSize;  i++)
    {
    if (masterText[i] != '-')
        {
        if (insertSize != 0)
            {
            outLine[outIx++] = '|';// escape to indicate following is count
            outLine[outIx++] = (unsigned char) max(255, insertSize);
            insertSize = 0;
            }
	outLine[outIx++] = text[i];
        outPositions++;
	}
    else
        {
        /* gap in master (reference) sequence */
        if (text[i] != '-')
            insertSize++;
        }
    }
}

static void drawScoreOverview(char *tableName, int height,
                             int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, 
                                        seqStart, seqEnd, NULL, &rowOffset);
double scale = scaleForPixels(width);
int x1,x2,y,w;
int height1 = height - 2;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    x1 = round((ref.chromStart - seqStart)*scale);
    x2 = round((ref.chromEnd - seqStart)*scale);
    w = x2-x1;
    if (w < 1) w = 1;
    if (vis == tvFull)
        {
        y = ref.score * height1;
        vgBox(vg, x1 + xOff, yOff + height1 - y, w, y+1, color);
        }
    else
        {
        Color c;
        int shade = ref.score * maxShade;
        if ((shade < 0) || (shade >= maxShade))
            shade = 0;
        c = shadesOfGray[shade];
        vgBox(vg, x1 + xOff, yOff, w, height-1, c);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void drawMafScore(char *tableName, int height,
                            int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, enum trackVisibility vis)
/* display alignments using on-the-fly scoring */
{
struct mafAli *mafList;
struct sqlConnection *conn;

if (seqEnd - seqStart > 1000000)
    drawScoreOverview(tableName, height, seqStart, seqEnd, vg, xOff, yOff,
                          width, font, color, color, vis);
else
    {
    /* load mafs */
    conn = hAllocConn();
    mafList = wigMafLoadInRegion(conn, tableName, chromName, 
                                    seqStart, seqEnd);
    drawMafRegionDetails(mafList, height, seqStart, seqEnd, 
                             vg, xOff, yOff, width, font,
                             color, color, vis, FALSE);
    hFreeConn(&conn);
    }
}

static boolean wigMafDrawPairwise(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise tables for this multiple alignment, 
 *  if "pairwise" setting is on.  For full mode, display wiggles,
 *  for pack mode, display density plot of mafs.
 */
{
boolean ret = FALSE;
char *suffix;
char *tableName;
Color newColor;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = pairwiseWigHeight(track);
struct wigMafItem *miList = track->items, *mi = miList;

if (miList == NULL)
    return FALSE;

/* get pairwise wiggle suffix from trackDb */
if ((suffix = pairwiseSuffix(track)) == NULL)
    return FALSE;


/* we will display pairwise, either a graph (wiggle or on-the-fly),
   or density plot */
ret = TRUE;
if (vis == tvFull)
    {
    double minY = 50.0;
    double maxY = 100.0;

    if (wigTrack == NULL)
        return FALSE;
    /* swap colors for pairwise wiggles */
    newColor = wigTrack->ixColor;
    wigTrack->ixColor = wigTrack->ixAltColor;
    wigTrack->ixAltColor = newColor;
    wigSetCart(wigTrack, MIN_Y, (void *)&minY);
    wigSetCart(wigTrack, MAX_Y, (void *)&maxY);
    }

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
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
            vgSetClip(vg, xOff, yOff, width, wigTrack->height);
            wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                             width, font, color, tvFull);
            vgUnclip(vg);
            }
        else
            {
            /* no wiggle table for this -- compute a graph on-the-fly 
               from mafs */
            vgSetClip(vg, xOff, yOff, width, mi->height);
            tableName = getMafTablename(mi->name, suffix);
            if (!hTableExists(tableName))
                tableName = getMafTablename(mi->db, suffix);
            if (hTableExists(tableName))
                drawMafScore(tableName, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 track->ixAltColor, tvFull);
            vgUnclip(vg);
            }
        /* need to add extra space between wiggles (for now) */
        mi->height = pairwiseHeight;
        }
    else 
        {
        /* pack */
        /* get maf table, containing pairwise alignments for this organism */
        /* display pairwise alignments in this region in dense format */
        vgSetClip(vg, xOff, yOff, width, mi->height);
        tableName = getMafTablename(mi->name, suffix);
	if (!hTableExists(tableName))
	    tableName = getMafTablename(mi->db, suffix);
        if (hTableExists(tableName))
            drawMafScore(tableName, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 color, tvDense);
        vgUnclip(vg);
        }
    yOff += mi->height;
    }
return ret;
}

static void alternateBlocksBehindChars(struct vGfx *vg, int x, int y, 
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
    vgBox(vg, x1+x, y, x2-x1, height, color);
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
    if (*line == '|')
        /* escape char, indicating insert count */
        i += 2;
    else
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
    else if (*dna == '|')
        {
        /* escape indicates skip next char -- it is an insert count */
        dna++;
        i++;
        }
    }
}
    
static int wigMafDrawBases(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis,
	struct wigMafItem *miList)
/* Draw base-by-base view, return new Y offset. */
{
struct wigMafItem *mi;
struct mafAli *mafList, *maf, *sub, *infoMaf;
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
int alignLineLength = winBaseCount*2 * 1;
        /* doubled to allow space for insert counts */
boolean complementBases = cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE);
bool dots, chainBreaks;         /* configuration options */
/* this line must be longer than the longest base-level display */
char noAlignment[2000];

/* initialize "no alignment" string to o's */
for (i = 0; i < sizeof noAlignment - 1; i++)
    noAlignment[i] = UNALIGNED_SEQ;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_DOT_VAR);
dots = cartCgiUsualBoolean(cart, option, FALSE);
safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
chainBreaks = cartCgiUsualBoolean(cart, option, FALSE);

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
seq = hChromSeq(chromName, seqStart, seqEnd);
memcpy(selfLine, seq->dna, winBaseCount);
toUpperN(selfLine, winBaseCount);
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
mafList = track->customPt;
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);

for (maf = mafList; maf != NULL; maf = maf->next)
    {
    /* get info about sequences from full alignment,
       for use later, when determining if sequence is unaligned or missing */
    for (mc = maf->components; mc != NULL; mc = mc->next)
        if (!hashFindVal(srcHash, mc->src))
            hashAdd(srcHash, mc->src, maf);

    /* get portion of maf in this window */
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	char db[64];
	int subStart,subEnd;
	int lineOffset, subSize;
        struct sqlConnection *conn;
        char query[128];

        /* process alignment for reference ("master") species */
	mcMaster = mafFindComponent(sub, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(sub);
	subStart = mcMaster->start;
	subEnd = subStart + mcMaster->size;
	subSize = subEnd - subStart;
	lineOffset = subStart - seqStart;
        processInserts(mcMaster->text, sub->textSize, 
                                &insertCounts[lineOffset], subSize);

        /* fill in bases for each species */
        for (mi = miList; mi != NULL; mi = mi->next)
            {
            if (mi->db == NULL)
                /* not a species line -- it's the gaps line, or... */
                continue;
            if ((mc = mafMayFindCompPrefix(sub, mi->db, "")) == NULL)
                {
                char chainTable[64];
                char *dbUpper;

                /* no alignment for this species */
                if (chainBreaks)
                    {
                    /* if user has requested this option, see if a 
                     * chain spans this region; use it to "extend" alignment */
                    dbUpper = cloneString(mi->db);
                    dbUpper[0] = toupper(dbUpper[0]);
                    safef(chainTable, sizeof chainTable, "%s_chain%s", 
                                                chromName, dbUpper);
                    if (hTableExistsDb(database, chainTable))
                        {
                        conn = hAllocConn();
                        safef(query, sizeof query,
                            "SELECT count(*) from %s WHERE tStart < %d AND tEnd > %d",
                                        chainTable, subStart, subEnd);
                        if (sqlQuickNum(conn, query) > 0)
                            processSeq(noAlignment, noAlignment,
                                        sub->textSize, lines[mi->ix],
                                        lineOffset, subSize);
                        hFreeConn(&conn);
                        continue;
                        }
                    }
                /* try extending coordinates from another part of alignment */
#ifdef BIGEXTEND
                if ((infoMaf = 
                        (struct mafAli *)hashFindVal(srcHash, mi->db)) != NULL)
                    {
                    struct mafComp *master, *src;
                    int masterOffset, srcStart;
                    char extendAlignment[2000] = {'^'};
                    int i;
                    master = mafFindComponent(infoMaf, dbChrom);
                    src = mafFindComponent(infoMaf, mi->db);
                    if (master->strand == '-')
                        mafFlipStrand(infoMaf);
                    masterOffset = mcMaster->start - master->start;
                    srcStart = src->start + masterOffset;
                    if (srcStart < src->srcSize)
                        {
                        /* can extend */
                        for (i = 1; i < subSize && srcStart+i < src->srcSize;
                                                 i++)
                            extendAlignment[i] = '-';
                        processSeq(extendAlignment, mcMaster->text, 
                                        sub->textSize, lines[mi->ix],
                                        lineOffset, subSize);
                        }
                    }
#endif
                }
            else
                processSeq(mc->text, mcMaster->text, sub->textSize, 
                                    lines[mi->ix], lineOffset, subSize);
	    }
	}
    mafAliFree(&sub);
    }
/* draw inserts line */
charifyInserts(insertLine, insertCounts, winBaseCount);
mi = miList;
spreadBasesString(vg, x - (width/winBaseCount)/2, y, width, mi->height-1, 
                alignInsertsColor(), font, insertLine, winBaseCount);
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
        alternateBlocksBehindChars(vg, colorX, y-1, width, 
                mi->height*(lineCount-2), tl.mWidth, winBaseCount, 
                alternateColorBaseCount, shadesOfSea[0], MG_WHITE);
        }
    }

/* draw base-level alignments */
for (mi = miList->next, i=1; mi != NULL && mi->db != NULL; mi = mi->next, ++i)
    {
    char *line;
    line  = lines[i];
    /* TODO: leave lower case in to indicate masking ?
       * NOTE: want to make sure that all sequences are soft-masked
       * if we do this */
    alignSeqToUpperN(line);
    if (complementBases)
        {
        seq = newDnaSeq(line, strlen(line), "");
        complementUpperAlignSeq(seq->dna, seq->size);
        line = seq->dna;
        }
    /* draw sequence letters for alignment */
    vgSetClip(vg, x, y, width, mi->height-1);
    spreadAlignString(vg, x, y, width, mi->height-1, color,
                        font, line, selfLine, winBaseCount, dots);
    vgUnclip(vg);
    y += mi->height;
    }

/* Clean up */
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
hashFree(&miHash);
return y;
}

static int wigMafDrawScoreGraph(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis, 
        boolean zoomedToBaseLevel)
{
/* Draw routine for score graph, returns new Y offset */
struct track *wigTrack = track->subtracks;
enum trackVisibility wigVis;

wigVis = (vis == tvDense ? tvDense : tvFull);
if (wigTrack != NULL)
    {
    wigTrack->ixColor = vgFindRgb(vg, &wigTrack->color);
    wigTrack->ixAltColor = vgFindRgb(vg, &wigTrack->altColor);
    vgSetClip(vg, xOff, yOff, width, wigTotalHeight(wigTrack, wigVis) - 1);
    wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                         width, font, color, wigVis);
    vgUnclip(vg);
    yOff += wigTotalHeight(wigTrack, wigVis);
    }
return yOff;
}


static void wigMafDraw(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for maf type tracks */
{
int y = yOff;
if (zoomedToBaseLevel)
    {
    struct wigMafItem *wiList = track->items;
    if (track->subtracks != NULL)
        wiList = wiList->next;
    y = wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis, zoomedToBaseLevel);
    y = wigMafDrawBases(track, seqStart, seqEnd, vg, xOff, y, width, font,
                                color, vis, wiList);
    }
else 
    {
    y = wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis, zoomedToBaseLevel);
    if (vis == tvFull || vis == tvPack)
        wigMafDrawPairwise(track, seqStart, seqEnd, vg, xOff, y, 
                                width, font, color, vis);
    }
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, track->height, track->mapName, 
            track->mapName, NULL);
}

void wigMafMethods(struct track *track, struct trackDb *tdb,
                                        int wordCount, char *words[])
/* Make track for maf multiple alignment. */
{
char *wigTable;
struct track *wigTrack;
int i;
char *savedType;
struct dyString *wigType = newDyString(64);
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

if ((wigTable = trackDbSetting(tdb, "wiggle")) != NULL)
    if (hTableExists(wigTable))
        {
        //  manufacture and initialize wiggle subtrack
        /* CAUTION: this code is very interdependent with
           hgTracks.c:fillInFromType()
           Also, both the main track and subtrack share the same tdb */
        // restore "type" line, but change type to "wig"
        savedType = tdb->type;
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

        // replace tablename with wiggle table from "wiggle" setting
        wigTrack->mapName = cloneString(wigTable);

        // setup wiggle methods in subtrack
        wigMethods(wigTrack, tdb, wordCount, words);

        wigTrack->mapsSelf = FALSE;
        wigTrack->drawLeftLabels = NULL;
        track->subtracks = wigTrack;
        track->subtracks->next = NULL;
        }
dyStringFree(&wigType);
}

