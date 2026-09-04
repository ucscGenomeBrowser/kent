/* chainTrack - stuff to load and display chain type tracks in
 * browser.   Chains are typically from cross-species genomic
 * alignments. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "chainBlock.h"
#include "chainLink.h"
#include "chainDb.h"
#include "chainCart.h"
#include "hgColors.h"
#include "hubConnect.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "snake.h"
#include "chainNetDbLoad.h"
#include "bigChain.h"
#include "quickLift.h"
#include "chainToPsl.h"
#include "psl.h"

struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

struct sqlClosure
{
struct sqlConnection *conn;
};

struct bbClosure
{
struct bbiFile *bbi;
};

typedef void  (*linkRetrieveFunc)(void *closure,  char *fullName,
			struct lm *lm, struct hash *hash,
			int start, int end, char * chainId, boolean isSplit);

static void doBbQuery(void *closure, char *fullName,
			struct lm *lm, struct hash *hash,
			int start, int end, char * chainId, boolean isSplit)
/* doBbQuery- check a bigBed for chain elements between
 * 	start and end.  Use the passed hash to resolve chain
 * 	id's and place the elements into the right
 * 	linkedFeatures structure
 */
{
struct bbClosure *bbClosure = (struct bbClosure *)closure;
struct bbiFile *bbi = bbClosure->bbi;
struct linkedFeatures *lf;
struct simpleFeature *sf;

struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, start, end, 0, lm);
char *bedRow[5];
char startBuf[16], endBuf[16];

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    lf = hashFindVal(hash, bedRow[3]);

    if (lf != NULL)
	{
	lmAllocVar(lm, sf);
	sf->start = sqlUnsigned(bedRow[1]);
	sf->end = sqlUnsigned(bedRow[2]);
	sf->qStart = sqlUnsigned(bedRow[4]);
	sf->qEnd = sf->qStart + (sf->end - sf->start);
	slAddHead(&lf->components, sf);
	}
    }
}

static void doQuery(void *closure, char *fullName,
			struct lm *lm, struct hash *hash,
			int start, int end, char * chainId, boolean isSplit)
/* doQuery- check the database for chain elements between
 * 	start and end.  Use the passed hash to resolve chain
 * 	id's and place the elements into the right
 * 	linkedFeatures structure
 */
{
struct sqlClosure *sqlClosure = (struct sqlClosure *)closure;
struct sqlConnection *conn = sqlClosure->conn;
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct dyString *query = dyStringNew(1024);

if (chainId == NULL)
    {
    sqlDyStringPrintf(query,
	"select chainId,tStart,tEnd,qStart from %sLink ",
	fullName);
    if (isSplit)
	sqlDyStringPrintf(query,"force index (bin) ");
    sqlDyStringPrintf(query,"where "); 
    }
else
    sqlDyStringPrintf(query,
	"select chainId, tStart,tEnd,qStart from %sLink where chainId=%s and ",
	fullName, chainId);
if (!isSplit)
    sqlDyStringPrintf(query, "tName='%s' and ", chromName);
hAddBinToQuery(start, end, query);
sqlDyStringPrintf(query, "tStart<%u and tEnd>%u", end, start);
sr = sqlGetResult(conn, query->string);

/* Loop through making up simple features and adding them
 * to the corresponding linkedFeature. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    lf = hashFindVal(hash, row[0]);
    if (lf != NULL)
	{
	lmAllocVar(lm, sf);
	sf->start = sqlUnsigned(row[1]);
	sf->end = sqlUnsigned(row[2]);
	sf->qStart = sqlUnsigned(row[3]);
	sf->qEnd = sf->qStart + (sf->end - sf->start);
	slAddHead(&lf->components, sf);
	}
    }
sqlFreeResult(&sr);
dyStringFree(&query);
}

static void loadLinks(struct track *tg, int seqStart, int seqEnd,
        enum trackVisibility vis)
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct lm *lm;
struct hash *hash;	/* Hash of chain ids. */
#ifdef OLD
double scale = ((double)(winEnd - winStart))/width;
#endif /* OLD */

char fullName[64];
int start, end, extra;
struct simpleFeature *lastSf = NULL;
int maxOverLeft = 0, maxOverRight = 0;
int overLeft, overRight;
void *closure;
struct sqlClosure sqlClosure;
struct bbClosure bbClosure;
linkRetrieveFunc queryFunc;
if (tg->isBigBed)
    {
    closure = &bbClosure;
    queryFunc = doBbQuery;
    char *fileName = trackDbSetting(tg->tdb, "linkDataUrl");
    if (fileName == NULL)
        {
        char buffer[4096];

        char *bigDataUrl = cloneString(trackDbSetting(tg->tdb, "bigDataUrl"));
        char *dot = strrchr(bigDataUrl, '.');
        if (dot == NULL)
            errAbort("No linkDataUrl in track %s", tg->track);

        *dot = 0;
        safef(buffer, sizeof buffer, "%s.link.bb", bigDataUrl);
        fileName = buffer;
        }
    struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
    if (bbi == NULL)
        errAbort("cannot open linkDataUrl %s", fileName);
    bbClosure.bbi =  bbi;
    }
else
    {
    closure = &sqlClosure;
    queryFunc = doQuery;
    sqlClosure.conn = hAllocConn(database);
    }

lm = lmInit(1024*4);
hash = newHash(0);

/* Make up a hash of all linked features keyed by
 * id, which is held in the extras field.  To
 * avoid burning memory on full chromosome views
 * we'll just make a single simple feature and
 * exclude from hash chains less than three pixels wide,
 * since these would always appear solid. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
#ifdef OLD
    double pixelWidth = (lf->end - lf->start) / scale;
    if (pixelWidth >= 2.5)
#endif /* OLD */
	{
	hashAdd(hash, lf->extra, lf);
	overRight = lf->end - seqEnd;
	if (overRight > maxOverRight)
	    maxOverRight = overRight;
	overLeft = seqStart - lf->start ;
	if (overLeft > maxOverLeft)
	    maxOverLeft = overLeft;
	}
#ifdef OLD
    else
	{
	lmAllocVar(lm, sf);
	sf->start = lf->start;
	sf->end = lf->end;
	sf->grayIx = lf->grayIx;
	lf->components = sf;
	}
#endif /* OLD */
    }

/* if some chains are actually loaded */
if (hash->elCount) 
    {
    boolean isSplit = TRUE;
    /* Make up range query. */
    safef(fullName, sizeof fullName, "%s_%s", chromName, tg->table);
    if (tg->isBigBed || !hTableExists(database, fullName))
	{
	strcpy(fullName, tg->table);
	isSplit = FALSE;
	}

    /* in dense mode we don't draw the lines
     * so we don't need items off the screen
     */
    if (vis == tvDense)
	queryFunc(closure, fullName, lm,  hash, seqStart, seqEnd, NULL, isSplit);
    else
	{
	/* if chains extend beyond edge of window we need to get
	 * elements that are off the screen
	 * in both directions so we know whether to draw
	 * one or two lines to the edge of the screen.
	 */
#define STARTSLOP	10000
#define MULTIPLIER	10
#define MAXLOOK		100000
	extra = (STARTSLOP < maxOverLeft) ? STARTSLOP : maxOverLeft;
	start = seqStart - extra;
	extra = (STARTSLOP < maxOverRight) ? STARTSLOP : maxOverRight;
	end = seqEnd + extra;
	queryFunc(closure, fullName, lm,  hash, start, end, NULL, isSplit);

	for (lf = tg->items; lf != NULL; lf = lf->next)
	    {
	    if (lf->components != NULL)
		slSort(&lf->components, linkedFeaturesCmpStart);
	    extra = (STARTSLOP < maxOverRight)?STARTSLOP:maxOverRight;
	    end = seqEnd + extra;
	    for (lastSf=sf=lf->components;  sf;  lastSf=sf, sf=sf->next)
		;
	    while (lf->end > end )
		{
		/* get out if we have an element off right side */
		if (( (lastSf != NULL) &&(lastSf->end > seqEnd)) || (extra > MAXLOOK))
		    break;

		extra *= MULTIPLIER;
		start = end;
		end = start + extra;
                queryFunc(closure, fullName, lm,  hash, start, end, lf->extra, isSplit);
		if (lf->components != NULL)
		    slSort(&lf->components, linkedFeaturesCmpStart);
		for (sf=lastSf;  sf != NULL;  lastSf=sf, sf=sf->next)
		    ;
		}

	    /* if we didn't find an element off to the right , add one */
	    if ((lf->end > seqEnd) && ((lastSf == NULL) ||(lastSf->end < seqEnd)))
		{
		lmAllocVar(lm, sf);
		sf->start = seqEnd;
		sf->end = seqEnd+1;
		sf->grayIx = lf->grayIx;
		sf->qStart = 0;
		sf->qEnd = sf->qStart + (sf->end - sf->start);
		sf->next = lf->components;
		lf->components = sf;
		slSort(&lf->components, linkedFeaturesCmpStart);
		}

	    /* we know we have a least one component off right
	     * now look for one off left
	     */
	    extra = (STARTSLOP < maxOverLeft) ? STARTSLOP:maxOverLeft;
	    start = seqStart - extra;
	    while((extra < MAXLOOK) && (lf->start < seqStart) &&
		(lf->components != NULL) && (lf->components->start > seqStart))
		{
		extra *= MULTIPLIER;
		end = start;
		start = end - extra;
                if (start < 0)
                    start = 0;
		queryFunc(closure, fullName, lm,  hash, start, end, lf->extra, isSplit);
		slSort(&lf->components, linkedFeaturesCmpStart);
		}
	    if ((lf->components != NULL) && (lf->components->start > seqStart) && (lf->start < lf->components->start))
		{
		lmAllocVar(lm, sf);
		sf->start = 0;
		sf->end = 1;
		sf->grayIx = lf->grayIx;
		sf->qStart = lf->components->qStart;
		sf->qEnd = sf->qStart + (sf->end - sf->start);
		sf->next = lf->components;
		lf->components = sf;
		slSort(&lf->components, linkedFeaturesCmpStart);
		}
	    }
	}
    }
for (lf = tg->items; lf != NULL; lf = lf->next)
        {
	if ((lf) && lf->orientation == -1)
	    {
            struct simpleFeature *sf = lf->components;

            for(; sf; sf = sf->next)
                {
                int temp;

                temp = sf->qStart;
                sf->qStart = lf->qSize - sf->qEnd;
                sf->qEnd = lf->qSize - temp;
                }
	    }
        }
if (tg->isBigBed)
    bbiFileClose(&bbClosure.bbi);
else
    hFreeConn(&sqlClosure.conn);
}

void chainDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chained features. This loads up the simple features from
 * the chainLink table, calls linkedFeaturesDraw, and then
 * frees the simple features again. */
{

if (tg->items == NULL)		/*Exit Early if nothing to do */
    return;

linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
    font, color, vis);
}


static struct linkedFeatures *lfFromLiftedChain(struct chain *chain, struct chain *lifted,
    int qs, boolean doSnake)
/* Build the drawing item for one chain that has been mapped onto the reference.  The
 * blocks come from the lifted alignment, so unlike the native loaders this one does not
 * leave the components for loadLinks to fetch. */
{
struct linkedFeatures *lf;
char buf[16];

AllocVar(lf);
lf->start = lf->tallStart = lifted->tStart;
lf->end = lf->tallEnd = lifted->tEnd;
lf->qSize = lifted->qSize;
lf->grayIx = maxShade;
lf->score = chain->score;
lf->filterColor = -1;

lf->orientation = (lifted->qStrand == '-') ? -1 : 1;

int len = strlen(chain->qName) + 32;
lf->name = needMem(len);
if (!doSnake)
    safef(lf->name, len, "%s %c %dk", chain->qName, lifted->qStrand, qs/1000);
else
    safef(lf->name, len, "%s", chain->qName);
safef(buf, sizeof(buf), "%d", chain->id);
lf->extra = cloneString(buf);

struct simpleFeature *sfList = NULL, *sf;
struct cBlock *b;
for (b = lifted->blockList; b != NULL; b = b->next)
    {
    AllocVar(sf);
    sf->start = b->tStart;
    sf->end = b->tEnd;
    sf->grayIx = lf->grayIx;
    // A minus strand chain keeps its query coordinates reverse complemented, and the
    // drawing code wants them forward, which is the same turn loadLinks makes at the end.
    if (lf->orientation == -1)
        {
        sf->qStart = lifted->qSize - b->qEnd;
        sf->qEnd = lifted->qSize - b->qStart;
        }
    else
        {
        sf->qStart = b->qStart;
        sf->qEnd = b->qEnd;
        }
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
return lf;
}

static void quickLiftChainLoadItems(struct track *tg)
/* Load chains out of the assembly this track came from and map them onto the reference.
 * A chain is an alignment between that assembly and some other species, so lifting one
 * composes two alignments and leaves the user with chains between the assembly on screen
 * and that species. */
{
boolean doSnake = cartOrTdbBoolean(cart, tg->tdb, "doSnake" , FALSE);
struct cartOptions *chainCart = (struct cartOptions *) tg->extraUiData;
char *optionChrStr = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "chromFilter");
if (isNotEmpty(optionChrStr) && sameWord("All", skipLeadingSpaces(optionChrStr)))
    optionChrStr = NULL;

char *liftDb = trackDbSetting(tg->tdb, "quickLiftDb");
char *table = NULL;
quickLiftResolveTable(tg->tdb, tg->table, &table, &liftDb);
char *quickLiftFile = trackDbSetting(tg->tdb, "quickLiftUrl");

char *chainFile = NULL, *linkFile = NULL;
if (tg->isBigBed)
    {
    chainFile = trackDbSetting(tg->tdb, "bigDataUrl");
    // linkDataUrl when the track names its link file, which not every track does by the
    // name bigChainGetLinkFile would guess.  This is the choice loadLinks makes.
    linkFile = trackDbSetting(tg->tdb, "linkDataUrl");
    if (linkFile == NULL)
        linkFile = bigChainGetLinkFile(chainFile);
    }

struct hash *chainHash = newHash(8);
struct hash *mapPsls = NULL;
struct quickLiftRange *range, *rangeList = quickLiftSourceRanges(quickLiftFile, chromName,
    winStart, winEnd, chainHash);
struct linkedFeatures *list = NULL;

for (range = rangeList; range != NULL; range = range->next)
    {
    struct chain *chain, *chainList;
    if (tg->isBigBed)
        chainList = chainLoadIdRangeHub(NULL, chainFile, linkFile, range->chrom,
            range->start, range->end, -1);
    else
        chainList = chainLoadRange(liftDb, table, range->chrom, range->start, range->end);

    for (chain = chainList; chain != NULL; chain = chain->next)
        {
        if (chain->blockList == NULL)
            continue;
        if ((optionChrStr != NULL) && !startsWith(optionChrStr, chain->qName))
            continue;
        if ((chainCart->scoreFilter > 0) && (chain->score < chainCart->scoreFilter))
            continue;

        int qs = (chain->qStrand == '-') ? chain->qSize - chain->qEnd : chain->qStart;
        struct chain *lifted = quickLiftChain(chainHash, &mapPsls, chain);
        if (lifted == NULL)
            continue;

        slAddHead(&list, lfFromLiftedChain(chain, lifted, qs, doSnake));
        }
    }

// put the list back into the order the chains were loaded in, which is what decides
// between items the sort below sees as equal
slReverse(&list);
if (tg->visibility != tvDense)
    slSort(&list, linkedFeaturesCmpStart);
else if (chainCart->chainColor == chainColorScoreColors)
    slSort(&list, chainCmpScore);
tg->items = list;

maybeLoadSnake(tg);
}

void bigChainLoadItems(struct track *tg)
/* Load up all of the chains from correct table into tg->items
 * item list.  At this stage to conserve memory for other tracks
 * we don't load the links into the components list until draw time. */
{
if ((trackDbSetting(tg->tdb, "quickLiftDb") != NULL) && !quickLiftIsOwnChainTrack(tg->tdb))
    {
    quickLiftChainLoadItems(tg);
    return;
    }

boolean doSnake = cartOrTdbBoolean(cart, tg->tdb, "doSnake" , FALSE);
struct linkedFeatures *list = NULL, *lf;
int qs;
char *optionChrStr;
struct cartOptions *chainCart;

chainCart = (struct cartOptions *) tg->extraUiData;

optionChrStr = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE,
	"chromFilter");

struct bbiFile *bbi =  fetchBbiForTrack(tg);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
int fieldCount = 11;
char *bedRow[fieldCount];
char startBuf[16], endBuf[16];

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    if ((optionChrStr != NULL) && !startsWith(optionChrStr, bedRow[7]))
        continue;

    if (chainCart->scoreFilter >0)
        {
        unsigned score = sqlUnsigned(bedRow[4]);
        if  (score < chainCart->scoreFilter)
            continue;
        }

    struct bed *bed = bedLoadN(bedRow, 6);
    lf = bedMungToLinkedFeatures(&bed, tg->tdb, fieldCount,
        0, 1000, FALSE);

    lf->qSize = sqlUnsigned(bedRow[8]);

    if (*bedRow[5] == '-')
	{
	lf->orientation = -1;
        qs = sqlUnsigned(bedRow[8]) - sqlUnsigned(bedRow[10]);
	}
    else
        {
	lf->orientation = 1;
	qs = sqlUnsigned(bedRow[9]);
	}

    int len = strlen(bedRow[7]) + 32;
    lf->name = needMem(len);
    if (!doSnake)
        safef(lf->name, len, "%s %c %dk", bedRow[7], *bedRow[5], qs/1000);
    else
        safef(lf->name, len, "%s", bedRow[7]);
    lf->extra = cloneString(bedRow[3]);
    lf->components = NULL;
    slAddHead(&list, lf);
    }

/* Make sure this is sorted if in full mode. Sort by score when
 * coloring by score and in dense */
if (tg->visibility != tvDense)
    slSort(&list, linkedFeaturesCmpStart);
else if ((tg->visibility == tvDense) &&
	(chainCart->chainColor == chainColorScoreColors))
    slSort(&list, chainCmpScore);
else
    slReverse(&list);
tg->items = list;

bbiFileClose(&bbi);

loadLinks(tg, winStart, winEnd, tvFull);
maybeLoadSnake(tg);
}

void chainLoadItems(struct track *tg)
/* Load up all of the chains from correct table into tg->items
 * item list.  At this stage to conserve memory for other tracks
 * we don't load the links into the components list until draw time. */
{
if ((trackDbSetting(tg->tdb, "quickLiftDb") != NULL) && !quickLiftIsOwnChainTrack(tg->tdb))
    {
    quickLiftChainLoadItems(tg);
    return;
    }

boolean doSnake = cartOrTdbBoolean(cart, tg->tdb, "doSnake" , FALSE);
char *table = tg->table;
struct chain chain;
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
struct linkedFeatures *list = NULL, *lf;
int qs;
char *optionChrStr;
char extraWhere[128] ;
struct cartOptions *chainCart;

chainCart = (struct cartOptions *) tg->extraUiData;

optionChrStr = skipLeadingSpaces(cartUsualStringClosestToHome(cart, tg->tdb,
    FALSE, "chromFilter", "All"));

if (strlen(optionChrStr) > 0 && differentWord("All",optionChrStr))
    {
    sqlSafef(extraWhere, sizeof(extraWhere),
            "qName = \"%s\" and score > %d",optionChrStr,
            chainCart->scoreFilter);
    sr = hRangeQuery(conn, table, chromName, winStart, winEnd,
            extraWhere, &rowOffset);
    }
else
    {
    if (chainCart->scoreFilter > 0)
        {
        sqlSafef(extraWhere, sizeof(extraWhere),
                "score > \"%d\"",chainCart->scoreFilter);
        sr = hRangeQuery(conn, table, chromName, winStart, winEnd,
                extraWhere, &rowOffset);
        }
    else
        {
        sr = hRangeQuery(conn, table, chromName, winStart, winEnd,
                NULL, &rowOffset);
        }
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    char buf[16];
    chainHeadStaticLoad(row + rowOffset, &chain);
    AllocVar(lf);
    lf->start = lf->tallStart = chain.tStart;
    lf->end = lf->tallEnd = chain.tEnd;
    lf->qSize = chain.qSize;
    lf->grayIx = maxShade;
    if (chainCart->chainColor == chainColorScoreColors)
	{
	float normScore = sqlFloat((row+rowOffset)[11]);
        lf->grayIx = hGrayInRange(normScore, 0, 100, maxShade+1);
	lf->score = normScore;
	}
    else
	lf->score = chain.score;

    lf->filterColor = -1;

    if (chain.qStrand == '-')
	{
	lf->orientation = -1;
        qs = chain.qSize - chain.qEnd;
	}
    else
        {
	lf->orientation = 1;
	qs = chain.qStart;
	}
    int len = strlen(chain.qName) + 32;
    lf->name = needMem(len);
    if (!doSnake)
        safef(lf->name, len, "%s %c %dk", chain.qName, chain.qStrand, qs/1000);
    else
        safef(lf->name, len, "%s", chain.qName);
    safef(buf, sizeof(buf), "%d", chain.id);
    lf->extra = cloneString(buf);
    slAddHead(&list, lf);
    }

/* Make sure this is sorted if in full mode. Sort by score when
 * coloring by score and in dense */
if (tg->visibility != tvDense)
    slSort(&list, linkedFeaturesCmpStart);
else if ((tg->visibility == tvDense) &&
	(chainCart->chainColor == chainColorScoreColors))
    slSort(&list, chainCmpScore);
else
    slReverse(&list);
tg->items = list;
loadLinks(tg, winStart, winEnd, tvFull);

maybeLoadSnake(tg);

/* Clean up. */
sqlFreeResult(&sr);
hFreeConn(&conn);
}	/*	chainLoadItems()	*/

static Color chainScoreColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;

return(tg->colorShades[lf->grayIx]);
}

static Color chainNoColor(struct track *tg, void *item, struct hvGfx *hvg)
{
return(tg->ixColor);
}

static void setNoColor(struct track *tg)
{
tg->itemColor = chainNoColor;
tg->color.r = 0;
tg->color.g = 0;
tg->color.b = 0;
tg->color.a = 255;
tg->altColor.r = 127;
tg->altColor.g = 127;
tg->altColor.b = 127;
tg->altColor.a = 255;
tg->ixColor = MG_BLACK;
tg->ixAltColor = MG_GRAY;
}

void chainMethods(struct track *tg, struct trackDb *tdb,
	int wordCount, char *words[])
/* Fill in custom parts of alignment chains. */
{
struct cartOptions *chainCart;

AllocVar(chainCart);

boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);

/*	what does the cart say about coloring option	*/
chainCart->chainColor = chainFetchColorOption(cart, tdb, FALSE);
int scoreFilterDefault = atoi(trackDbSettingOrDefault(tdb, "scoreFilter", "0"));
chainCart->scoreFilter = cartUsualIntClosestToHome(cart, tdb,
	FALSE, SCORE_FILTER, scoreFilterDefault);

linkedFeaturesMethods(tg);
tg->itemColor = lfChromColor;	/*	default coloring option */
tg->exonArrowsAlways = TRUE;

/*	if normScore column is available, then allow coloring	*/
if (normScoreAvailable)
    {
    switch (chainCart->chainColor)
	{
	case (chainColorScoreColors):
	    tg->itemColor = chainScoreColor;
	    tg->colorShades = shadesOfGray;
	    break;
	case (chainColorNoColors):
	    setNoColor(tg);
	    break;
	default:
	case (chainColorChromColors):
	    break;
	}
    }
else
    {
    char *optionStr;	/* this old option was broken before */

    optionStr = cartUsualStringClosestToHome(cart, tdb, FALSE, "color", "on");
    if (differentWord("on",optionStr))
	{
	setNoColor(tg);
	chainCart->chainColor = chainColorNoColors;
	}
    else
	chainCart->chainColor = chainColorChromColors;
    }


if (tg->isBigBed)
    tg->loadItems = bigChainLoadItems;
else
    tg->loadItems = chainLoadItems;
tg->drawItems = chainDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
tg->extraUiData = (void *) chainCart;
}
