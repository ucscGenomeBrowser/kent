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


static void chainDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chained features. This loads up the simple features from 
 * the chainLink table, calls linkedFeaturesDraw, and then
 * frees the simple features again. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct hash *hash = newHash(0);	/* Hash of chain ids. */
struct dyString *query = newDyString(1024);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct lm *lm = lmInit(1024*4);
char **row;
double scale = ((double)(winEnd - winStart))/width;

/* Make up a hash of all linked features keyed by
 * id, which is held in the extras field.  To
 * avoid burning memory on full chromosome views
 * we'll just make a single simple feature and
 * exclude from hash chains less than three pixels wide, 
 * since these would always appear solid. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    double pixelWidth = scale * (lf->end - lf->start);
    if (pixelWidth >= 2.5)
	{
	hashAdd(hash, lf->extra, lf);
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

/* Make up range query. */
dyStringPrintf(query, "select chainId,tStart,tEnd from %s_%sLink where ",
	chromName, tg->mapName);
hAddBinToQuery(seqStart, seqEnd, query);
dyStringPrintf(query, "tStart<%u and tEnd>%u", seqEnd, seqStart);
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
	sf->grayIx = maxShade;
	slAddHead(&lf->components, sf);
	}
    }

/* Someday we may need to put in a sort on the simple features
 * here.  For now though nobody cares. */

linkedFeaturesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width,
	font, color, vis);

/* Cleanup time. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    lf->components = NULL;
lmCleanup(&lm);
freeHash(&hash);
dyStringFree(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void chainLoadItems(struct trackGroup *tg)
/* Load up all of the chains from correct table into tg->items 
 * item list.  At this stage to conserve memory for other tracks
 * we don't load the links into the components list until draw time. */
{
char *track = tg->mapName;
struct chain chain;
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, track, chromName, winStart, winEnd,
	NULL, &rowOffset);
struct linkedFeatures *list = NULL, *lf;
int qs;

while ((row = sqlNextRow(sr)) != NULL)
    {
    char buf[16];
    chainStaticLoad(row + rowOffset, &chain);
    AllocVar(lf);
    lf->start = lf->tallStart = chain.tStart;
    lf->end = lf->tallEnd = chain.tEnd;
    lf->grayIx = maxShade;
    lf->filterColor = -1;
    lf->score = chain.score;
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

/* Make sure this is sorted if in full mode. */
if (tg->visibility == tvFull && limitVisibility(tg, tg->items) == tvFull)
    slSort(&list, linkedFeaturesCmpStart);
else
    slReverse(&list);
tg->items = list;

/* Clean up. */
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void chainMethods(struct trackGroup *tg)
/* Fill in custom parts of alignment chains. */
{
char option[128]; /* Option -  rainbow chromosome color */
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr; 
char *optionStr ;
linkedFeaturesMethods(tg);
snprintf(option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = lfChromColor;
else
    tg->itemColor = NULL;
tg->loadItems = chainLoadItems;
tg->drawItems = chainDraw;
tg->mapItemName = lfMapNameFromExtra;
}

