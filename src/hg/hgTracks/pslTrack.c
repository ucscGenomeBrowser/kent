/* pslTrack - stuff to handle loading and display of
 * psl (blat format) based tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "psl.h"

static int pslGrayIx(struct psl *psl, boolean isXeno)
/* Figure out gray level for an RNA block. */
{
double misFactor;
double hitFactor;
int res;

if (isXeno)
    {
    misFactor = (psl->misMatch + psl->qNumInsert + psl->tNumInsert)*2.5;
    }
else
    {
    misFactor = (psl->misMatch + psl->qNumInsert)*5;
    }
misFactor /= (psl->match + psl->misMatch + psl->repMatch);
hitFactor = 1.0 - misFactor;
res = round(hitFactor * maxShade);
if (res < 1) res = 1;
if (res >= maxShade) res = maxShade-1;
return res;
}

static void filterMrna(struct track *tg, struct linkedFeatures **pLfList)
/* Apply filters if any to mRNA linked features. */
{
struct linkedFeatures *lf, *next, *newList = NULL, *oldList = NULL;
struct mrnaUiData *mud = tg->extraUiData;
struct mrnaFilter *fil;
char *type;
int i = 0;
boolean anyFilter = FALSE;
boolean colorIx = 0;
boolean isExclude = FALSE;
boolean andLogic = TRUE;
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = NULL;
boolean isDense;

if (*pLfList == NULL || mud == NULL)
    return;

/* First make a quick pass through to see if we actually have
 * to do the filter. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
        anyFilter = TRUE;
    }
if (!anyFilter)
    return;

type = cartUsualString(cart, mud->filterTypeVar, "red");
if (sameString(type, "exclude"))
    isExclude = TRUE;
else if (sameString(type, "include"))
    isExclude = FALSE;
else
    colorIx = getFilterColor(type, MG_BLACK);
type = cartUsualString(cart, mud->logicTypeVar, "and");
andLogic = sameString(type, "and");

/* Make a pass though each filter, and start setting up search for
 * those that have some text. */
conn = hAllocConn();
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
	{
	fil->hash = newHash(10);
	if ((fil->mrnaTableIx = sqlFieldIndex(conn, "mrna", fil->table)) < 0)
	    internalErr();
	}
    }

/* Scan tables id/name tables to build up hash of matching id's. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    struct hash *hash = fil->hash;
    int wordIx, wordCount;
    char *words[128];

    if (hash != NULL)
	{
	boolean anyWild;
	char *dupPat = cloneString(fil->pattern);
	wordCount = chopLine(dupPat, words);
	for (wordIx=0; wordIx <wordCount; ++wordIx)
	    {
	    char *pattern = cloneString(words[wordIx]);
	    if (lastChar(pattern) != '*')
		{
		int len = strlen(pattern)+1;
		pattern = needMoreMem(pattern, len, len+1);
		pattern[len-1] = '*';
		}
	    anyWild = (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL);
	    sprintf(query, "select id,name from %s", fil->table);
	    touppers(pattern);
	    sr = sqlGetResult(conn, query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		boolean gotMatch;
		touppers(row[1]);
		if (anyWild)
		    gotMatch = wildMatch(pattern, row[1]);
		else
		    gotMatch = sameString(pattern, row[1]);
		if (gotMatch)
		    {
		    hashAdd(hash, row[0], NULL);
		    }
		}
	    sqlFreeResult(&sr);
	    freez(&pattern);
	    }
	freez(&dupPat);
	}
    }

/* Scan through linked features coloring and or including/excluding ones that 
 * match filter. */
for (lf = *pLfList; lf != NULL; lf = next)
    {
    boolean passed = andLogic;
    next = lf->next;
    sprintf(query, "select * from mrna where acc = '%s'", lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	for (fil = mud->filterList; fil != NULL; fil = fil->next)
	    {
	    if (fil->hash != NULL)
		{
		if (hashLookup(fil->hash, row[fil->mrnaTableIx]) == NULL)
		    {
		    if (andLogic)    
			passed = FALSE;
		    }
		else
		    {
		    if (!andLogic)
		        passed = TRUE;
		    }
		}
	    }
	}
    sqlFreeResult(&sr);
    if (passed ^ isExclude)
	{
	slAddHead(&newList, lf);
	if (colorIx > 0)
	    lf->filterColor = colorIx;
	}
    else
        {
	slAddHead(&oldList, lf);
	}
    }

slReverse(&newList);
slReverse(&oldList);
if (colorIx > 0)
   {
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (tg->visibility == tvDense)
       {
       newList = slCat(oldList, newList);
       }
   else
       {
       newList = slCat(newList, oldList);
       }
   }
*pLfList = newList;
tg->limitedVisSet = FALSE;	/* Need to recalculate this after filtering. */

/* Free up hashes, etc. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    hashFree(&fil->hash);
    }
hFreeConn(&conn);
}

struct linkedFeatures *lfFromPslx(struct psl *psl, 
	int sizeMul, boolean isXeno, boolean nameGetsPos)
/* Create a linked feature item from pslx.  Pass in sizeMul=1 for DNA, 
 * sizeMul=3 for protein. */
{
unsigned *starts = psl->tStarts;
unsigned *sizes = psl->blockSizes;
int i, blockCount = psl->blockCount;
int grayIx = pslGrayIx(psl, isXeno);
struct simpleFeature *sfList = NULL, *sf;
struct linkedFeatures *lf;
boolean rcTarget = (psl->strand[1] == '-');

AllocVar(lf);
lf->score = (psl->match - psl->misMatch - psl->repMatch);
lf->grayIx = grayIx;
if (nameGetsPos)
    {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s:%d-%d %s:%d-%d", psl->qName, psl->qStart, psl->qEnd,
    	psl->tName, psl->tStart, psl->tEnd);
    lf->extra = cloneString(buf);
    snprintf(lf->name, sizeof(lf->name), "%s %s %dk", psl->qName, psl->strand, psl->qStart/1000);
    snprintf(lf->popUp, sizeof(lf->popUp), "%s:%d-%d score %9.0f", psl->qName, psl->qStart, psl->qEnd, lf->score);
    }
else
    strncpy(lf->name, psl->qName, sizeof(lf->name));
lf->orientation = orientFromChar(psl->strand[0]);
if (rcTarget)
    lf->orientation = -lf->orientation;
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    sf->start = sf->end = starts[i];
    sf->end += sizes[i]*sizeMul;
    if (rcTarget)
        {
	int s, e;
	s = psl->tSize - sf->end;
	e = psl->tSize - sf->start;
	sf->start = s;
	sf->end = e;
	}
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->start = psl->tStart;	/* Correct for rounding errors... */
lf->end = psl->tEnd;
return lf;
}

struct linkedFeatures *lfFromPsl(struct psl *psl, boolean isXeno)
/* Create a linked feature item from psl. */
{
return lfFromPslx(psl, 1, isXeno, FALSE);
}

static void connectedLfFromPslsInRange(struct sqlConnection *conn,
    struct track *tg, int start, int end, char *chromName,
    boolean isXeno, boolean nameGetsPos, int sizeMul)
/* Return linked features from range of table after have
 * already connected to database.. */
{
char *track = tg->mapName;
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
char *optionChrStr;
struct linkedFeatures *lfList = NULL, *lf;
char optionChr[128]; /* Option -  chromosome filter */
char extraWhere[128] ;

snprintf( optionChr, sizeof(optionChr), "%s.chromFilter", tg->mapName);
optionChrStr = cartUsualString(cart, optionChr, "All");
if (startsWith("chr",optionChrStr)) 
    {
    snprintf(extraWhere, sizeof(extraWhere), "qName = \"%s\"",optionChrStr);
    sr = hRangeQuery(conn, track, chromName, start, end, extraWhere, &rowOffset);
    }
else
    {
    snprintf(extraWhere, sizeof(extraWhere), " ");
    sr = hRangeQuery(conn, track, chromName, start, end, NULL, &rowOffset);
    }

if (sqlCountColumns(sr) < 21+rowOffset)
    errAbort("trackDb has incorrect table type for table \"%s\"",
	     tg->mapName);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+rowOffset);
    lf = lfFromPslx(psl, sizeMul, isXeno, nameGetsPos);
    slAddHead(&lfList, lf);
    pslFree(&psl);
    }
slReverse(&lfList);
tg->items = lfList;	/* Do this twice for benefit of limit visibility */
if (limitVisibility(tg) != tvDense)
    slSort(&lfList, linkedFeaturesCmpStart);
if (tg->extraUiData)
    filterMrna(tg, &lfList);
tg->items = lfList;
sqlFreeResult(&sr);
}


static void lfFromPslsInRange(struct track *tg, int start, int end, 
	char *chromName, boolean isXeno, boolean nameGetsPos, int sizeMul)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
connectedLfFromPslsInRange(conn, tg, start, end, chromName, 
	isXeno, nameGetsPos, sizeMul);
hFreeConn(&conn);
}

static void lfFromPslsInRangeAndFilter(struct track *tg, int start, int end, 
	char *chromName, boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
connectedLfFromPslsInRange(conn, tg, start, end, chromName, 
	isXeno, nameGetsPos, 1);
hFreeConn(&conn);
}

static void loadXenoPslWithPos(struct track *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, TRUE, TRUE, 1);
}

void pslChromMethods(struct track *tg)
/* Fill in custom parts of blatMus - assembled mouse genome blat vs. human. */
{
char option[128]; /* Option -  rainbow chromosome color */
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr; 
char *optionStr ;
snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
tg->mapItemName = lfMapNameFromExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = lfChromColor;
else
    tg->itemColor = NULL;
tg->loadItems = loadXenoPslWithPos;
}

void loadPsl(struct track *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, FALSE, FALSE, 1);
}

static void loadProteinPsl(struct track *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, TRUE, FALSE, 3);
}

static void loadXenoPsl(struct track *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, TRUE, FALSE, 1);
}

void pslMethods(struct track *track, struct trackDb *tdb, 
	int argc, char *argv[])
/* Load up psl type methods. */
{
char *subType = ".";
if (argc >= 2)
   subType = argv[1];
linkedFeaturesMethods(track);
if (!tdb->useScore)
    track->colorShades = NULL;
if (sameString(subType, "protein"))
    {
    track->subType = lfSubXeno;
    track->loadItems = loadProteinPsl;
    }
else if (sameString(subType, "xeno"))
    {
    track->subType = lfSubXeno;
    track->loadItems = loadXenoPsl;
    if (argc >= 3)
	{
	char *otherDb = argv[2];
	pslChromMethods(track);
	}
    }
else
    track->loadItems = loadPsl;
if (sameString(subType, "est"))
    track->drawItems = linkedFeaturesAverageDense;
}
