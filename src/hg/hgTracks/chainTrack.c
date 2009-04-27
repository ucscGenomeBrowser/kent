/* chainTrack - stuff to load and display chain type tracks in
 * browser.   Chains are typically from cross-species genomic
 * alignments. */

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

static char const rcsid[] = "$Id: chainTrack.c,v 1.30 2008/09/03 19:19:01 markd Exp $";


struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

static void doQuery(struct sqlConnection *conn, char *fullName, 
			struct lm *lm, struct hash *hash, 
			int start, int end, char * chainId, boolean isSplit)
/* doQuery- check the database for chain elements between
 * 	start and end.  Use the passed hash to resolve chain
 * 	id's and place the elements into the right
 * 	linkedFeatures structure
 */
{
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct dyString *query = newDyString(1024);
char *force = "";

if (isSplit)
    force = "force index (bin)";

if (chainId == NULL)
    dyStringPrintf(query, 
	"select chainId,tStart,tEnd,qStart from %sLink %s where ",
	fullName, force);
else
    dyStringPrintf(query, 
	"select chainId, tStart,tEnd,qStart from %sLink where chainId=%s and ",
	fullName, chainId);
if (!isSplit)
    dyStringPrintf(query, "tName='%s' and ", chromName);
hAddBinToQuery(start, end, query);
dyStringPrintf(query, "tStart<%u and tEnd>%u", end, start);
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

static void chainDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chained features. This loads up the simple features from 
 * the chainLink table, calls linkedFeaturesDraw, and then
 * frees the simple features again. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct lm *lm;
struct hash *hash;	/* Hash of chain ids. */
struct sqlConnection *conn;
double scale = ((double)(winEnd - winStart))/width;
char fullName[64];
int start, end, extra;
struct simpleFeature *lastSf = NULL;
int maxOverLeft = 0, maxOverRight = 0;
int overLeft, overRight;

if (tg->items == NULL)		/*Exit Early if nothing to do */
    return;

lm = lmInit(1024*4);
hash = newHash(0);
conn = hAllocConn(database);

/* Make up a hash of all linked features keyed by
 * id, which is held in the extras field.  To
 * avoid burning memory on full chromosome views
 * we'll just make a single simple feature and
 * exclude from hash chains less than three pixels wide, 
 * since these would always appear solid. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    double pixelWidth = (lf->end - lf->start) / scale;
    if (pixelWidth >= 2.5)
	{
	hashAdd(hash, lf->extra, lf);
	overRight = lf->end - seqEnd;
	if (overRight > maxOverRight)
	    maxOverRight = overRight;
	overLeft = seqStart - lf->start ;
	if (overLeft > maxOverLeft)
	    maxOverLeft = overLeft;
	}
    else
	{
	lmAllocVar(lm, sf);
	sf->start = lf->start;
	sf->end = lf->end;
	sf->grayIx = lf->grayIx;
	lf->components = sf;
	}
    }

/* if some chains are bigger than 3 pixels */
if (hash->size)
    {
    boolean isSplit = TRUE;
    /* Make up range query. */
    sprintf(fullName, "%s_%s", chromName, tg->mapName);
    if (!hTableExists(database, fullName))
	{
	strcpy(fullName, tg->mapName);
	isSplit = FALSE;
	}

    /* in dense mode we don't draw the lines 
     * so we don't need items off the screen 
     */
    if (vis == tvDense)
	doQuery(conn, fullName, lm,  hash, seqStart, seqEnd, NULL, isSplit);
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
	doQuery(conn, fullName, lm,  hash, start, end, NULL, isSplit);

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
                doQuery(conn, fullName, lm,  hash, start, end, lf->extra, isSplit);
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
		(lf->components->start > seqStart))
		{
		extra *= MULTIPLIER;
		end = start;
		start = end - extra;
                if (start < 0)
                    start = 0;
		doQuery(conn, fullName, lm,  hash, start, end, lf->extra, isSplit);
		slSort(&lf->components, linkedFeaturesCmpStart);
		}
	    if ((lf->components->start > seqStart) && (lf->start < lf->components->start))
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
linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
/* Cleanup time. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    lf->components = NULL;

lmCleanup(&lm);
freeHash(&hash);
hFreeConn(&conn);
}

void chainLoadItems(struct track *tg)
/* Load up all of the chains from correct table into tg->items 
 * item list.  At this stage to conserve memory for other tracks
 * we don't load the links into the components list until draw time. */
{
char *track = tg->mapName;
struct chain chain;
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
struct linkedFeatures *list = NULL, *lf;
int qs;
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr;
char extraWhere[128] ;
struct cartOptions *chainCart;

chainCart = (struct cartOptions *) tg->extraUiData;

snprintf( optionChr, sizeof(optionChr), "%s.chromFilter", tg->mapName);
optionChrStr = cartUsualString(cart, optionChr, "All");
if (startsWith("chr",optionChrStr)) 
    {
    snprintf(extraWhere, sizeof(extraWhere), 
            "qName = \"%s\" and score > %d",optionChrStr, 
            chainCart->scoreFilter);
    sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
            extraWhere, &rowOffset);
    }
else
    {
    if (chainCart->scoreFilter > 0)
        {
        snprintf(extraWhere, sizeof(extraWhere), 
                "score > \"%d\"",chainCart->scoreFilter);
        sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
                extraWhere, &rowOffset);
        }
    else
        {
        snprintf(extraWhere, sizeof(extraWhere), " ");
        sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
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
    lf->grayIx = maxShade;
    if (chainCart->chainColor == chainColorScoreColors)
	{
	float normScore = sqlFloat((row+rowOffset)[11]);
	lf->grayIx = (int) ((float)maxShade * (normScore/100.0));
	if (lf->grayIx > (maxShade+1)) lf->grayIx = maxShade+1;
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
    snprintf(lf->name, sizeof(lf->name), "%s %c %dk", 
    	chain.qName, chain.qStrand, qs/1000);
    snprintf(lf->popUp, sizeof(lf->name), "%s %c start %d size %d",
    	chain.qName, chain.qStrand, qs, chain.qEnd - chain.qStart);
    snprintf(buf, sizeof(buf), "%d", chain.id);
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
tg->altColor.r = 127;
tg->altColor.g = 127;
tg->altColor.b = 127;
tg->ixColor = MG_BLACK;
tg->ixAltColor = MG_GRAY;
}

void chainMethods(struct track *tg, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Fill in custom parts of alignment chains. */
{

boolean normScoreAvailable = FALSE;
struct cartOptions *chainCart;
char scoreOption[256];

AllocVar(chainCart);

normScoreAvailable = chainDbNormScoreAvailable(database, chromName, tg->mapName, NULL);

/*	what does the cart say about coloring option	*/
chainCart->chainColor = chainFetchColorOption(tdb, (char **) NULL);

snprintf( scoreOption, sizeof(scoreOption), "%s.scoreFilter", tdb->tableName);
chainCart->scoreFilter = cartUsualInt(cart, scoreOption, 0);


linkedFeaturesMethods(tg);
tg->itemColor = lfChromColor;	/*	default coloring option */

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
    char option[128]; /* Option -  rainbow chromosome color */
    char *optionStr;	/* this old option was broken before */

    snprintf(option, sizeof(option), "%s.color", tg->mapName);
    optionStr = cartUsualString(cart, option, "on");
    if (differentWord("on",optionStr))
	{
	setNoColor(tg);
	chainCart->chainColor = chainColorNoColors;
	}
    else
	chainCart->chainColor = chainColorChromColors;
    }

tg->loadItems = chainLoadItems;
tg->drawItems = chainDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
tg->extraUiData = (void *) chainCart;
}
