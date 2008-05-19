/* Put up clone/coverage track */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "obscure.h"
#include "spaceSaver.h" 
#include "glDbRep.h"
#include "clonePos.h"

struct cloneFrag
/* A fragment of a clone. */
    {
    struct cloneFrag *next;	/* Next in list. */
    char *name;			/* Name of fragment.  Not allocated here. */
    };

struct cloneFragPos
/* An alignment involving a clone frag. */
    {
    struct cloneFragPos *next;	/* Next in list. */
    struct cloneFrag *frag;     /* Fragment info. */
    struct psl *psl;            /* Alignment info. Memory owned here. Possibly NULL. */
    struct gl *gl;              /* Golden path position info. */
    int start, end;             /* Start end in chromosome. */
    };

struct cloneInfo
/* Info about a clone and where it aligns. */
    {
    struct cloneInfo *next; 	/* Next in list */
    char *name;                 /* Name of clone. (Not allocated here) */
    short phase;			/* Htg phase - 1 2 or 3. */
    char stage;                 /* Stage - (P)redraft, (D)raft, (F)inished. */
    struct cloneFrag *fragList; /* List of fragments. */
    int fragCount;              /* Count of fragments. */
    struct cloneFragPos *cfaList;   /* List of alignments. */
    struct spaceSaver *ss;      /* How this is layed out. */
    int cloneStart, cloneEnd;       /* Min/Max position of alignments. */
    };

int cmpCloneInfo(const void *va, const void *vb)
/* Compare two cloneInfos by cloneStart. */
{
const struct cloneInfo *a = *((struct cloneInfo **)va);
const struct cloneInfo *b = *((struct cloneInfo **)vb);
return a->cloneStart - b->cloneStart;
}


void cloneInfoFree(struct cloneInfo **pCi)
/* free up a clone info. */
{
struct cloneInfo *ci;
if ((ci = *pCi) != NULL)
    {
    struct cloneFragPos *cfa;
    for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
	{
	pslFree(&cfa->psl);
	glFree(&cfa->gl);
	}
    slFreeList(&ci->cfaList);
    slFreeList(&ci->fragList);
    freez(pCi);
    }
}

void cloneInfoFreeList(struct cloneInfo **pList)
/* Free up a list of cloneInfo's. */
{
struct cloneInfo *el,*next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cloneInfoFree(&el);
    }
*pList = NULL;
}


struct cloneFrag *findCloneFrag(struct cloneInfo *ci, char *fragName)
/* Search for named frag and return it, or NULL if not found. */
{
struct cloneFrag *frag;
for (frag = ci->fragList; frag != NULL; frag = frag->next)
    {
    if (sameString(frag->name, fragName))
	return frag;
    }
return NULL;
}

void layoutCloneAli(struct cloneInfo *ci)
/* Figure out space saver layout for alignments in clone. */
{
struct spaceSaver *ss;
struct cloneFragPos *cfa;

ss = ci->ss = spaceSaverNew(winStart, winEnd, 100);
for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
    {
    spaceSaverAdd(ss, cfa->start, cfa->end, cfa);
    }
spaceSaverFinish(ss);
}

char *cloneName(struct track *tg, void *item)
/* Return name of gold track item. */
{
struct cloneInfo *ci = item;
return ci->name;
}

int cloneFragMaxWin = 1500000;

int oneOrRowCount(struct cloneInfo *ci)
/* Return row count, but at least one. */
{
int rowCount = ci->ss->rowCount;
if (rowCount < 1) rowCount = 1;
return rowCount;
}

static int cloneItemHeight(struct track *tg, void *item)
/* Return item height for fixed height track. */
{
struct cloneInfo *ci = item;
int height1 = tl.fontHeight+1;
if (winBaseCount <= cloneFragMaxWin)
    return height1*oneOrRowCount(ci)+2;
else
    return height1;
}

static int cloneItemStart(struct track *tg, void *item)
/* Return start of item on clone track. */
{
struct cloneInfo *ci = item;
return ci->cloneStart;
}

static int cloneItemEnd(struct track *tg, void *item)
/* Return end of item on clone track. */
{
struct cloneInfo *ci = item;
return ci->cloneEnd;
}



static int cloneTotalHeight(struct track *tg, enum trackVisibility vis)
/* Height of a clone track. */
{
switch (vis)
    {
    case tvFull:
	{
	int total = 0;
	struct cloneInfo *ci;
	for (ci = tg->items; ci != NULL; ci = ci->next)
	    {
	    total += tg->itemHeight(tg, ci);
	    }
	tg->height = total+2;
	break;
	}
    case tvDense:
	tg->lineHeight = tl.fontHeight+1;
	tg->heightPer = tg->lineHeight - 1;
	tg->height = tg->lineHeight;
	break;
    case tvHide:
    case tvPack:
    case tvSquish:
        break;
    }
return tg->height;
}

static void drawOneClone(struct cloneInfo *ci, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width,
    MgFont *font, int lineHeight, Color color, boolean stagger, 
    boolean hiliteDupes, boolean nofrag)
/* Draw a single clone item - using space saver layout on fragments. */
{
struct cloneFragPos *cfa;
struct psl *psl;
int y;
int heightPer = lineHeight-1;
struct spaceSaver *ss = ci->ss;
int baseWidth = seqEnd - seqStart;
struct spaceNode *sn;
int x1, x2, w;
char *s;
int textWidth;
char fullPos[256];
struct hash *dupeHash = NULL;
Color col;
struct hashEl *hel;

if (hiliteDupes)
    {
    struct hash *uniqHash = newHash(7);
    dupeHash = newHash(6);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
	{
	cfa = sn->val;
	if (cfa->end - cfa->start > 1000)
	    {
	    s = strchr(cfa->frag->name, '_');
	    if (s != NULL)
		{
		s += 1;
		if (hashLookup(uniqHash, s) == NULL)
		    {
		    hashAdd(uniqHash, s, NULL);
		    }
		else	/* Got a dupe! */
		    {
		    if ((hel = hashLookup(dupeHash, s)) == NULL)
			hashAdd(dupeHash, s, sn);
		    }
		}
	    }
	}
    freeHash(&uniqHash);
    }

for (sn = ss->nodeList; sn != NULL; sn = sn->next)
    {
    if (stagger)
	y = yOff + sn->row*lineHeight;
    else
	y = yOff;
    cfa = sn->val;
    x1 = roundingScale(cfa->start-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(cfa->end-winStart, width, baseWidth)+xOff;

    /* Clip here so that text will tend to be more visible... */
    if (x1 < xOff)
	x1 = xOff;
    if (x2 > xOff + width)
	x2 = xOff + width;
    w = x2-x1;
    if (w < 1)
	w = 1;
    s = strchr(cfa->frag->name, '_');
    if (s == NULL)
	s = "";
    else
	s += 1;
    col = color;
    if (hiliteDupes)
	{
	if ((hel = hashLookup(dupeHash, s)) != NULL)
	    {
	    if (hel->val == sn)
		col = MG_RED;
	    else
		col = MG_BLUE;
	    }
	}
    hvGfxBox(hvg, x1, y, w, heightPer, col);
    textWidth = mgFontStringWidth(font, s);
    if ((textWidth <= w) && (!nofrag))
	hvGfxTextCentered(hvg, x1, y, w, heightPer, MG_WHITE, font, s); 
    if (baseWidth <= 2000000)
	{
	psl = cfa->psl;
	if (psl != NULL)
	    {
	    sprintf(fullPos, "%s %d to %d of %d, strand %s, hits %d to %d", 
	    	psl->qName, psl->qStart, 
		psl->qEnd, psl->qSize, psl->strand,
		psl->tStart, psl->tEnd);
	    mapBoxHc(hvg, cfa->start, cfa->end, x1,y,w,heightPer, "hgClone", cfa->frag->name, fullPos);
	    }
	else
	    mapBoxHc(hvg, cfa->start, cfa->end, x1,y,w,heightPer, "hgClone", cfa->frag->name, cfa->frag->name);
	}
    }
freeHash(&dupeHash);
}

/* These tables are for combining sequence scores. 
 *    0 = no coverage
 *    1 = predraft
 *    2 = draft
 *    3 = deep draft
 *    4 = finished  */
static UBYTE predraftInc[5] = {1, 1, 2, 3, 4};  
static UBYTE draftInc[5] = {2, 2, 3, 3, 4};
static UBYTE finishedInc[5] = {4, 4, 4, 4, 4};

void incStage(UBYTE *b, int width, char stage)
/* Increment b from 0 to width-1 according to stage. */
{
UBYTE *inc = NULL;
int i;

if (stage == 'P')
    inc = predraftInc;
else if (stage == 'D')
    inc = draftInc;
else if (stage == 'F')
    inc = finishedInc;
else
    {
    warn("Unknown stage %c (%d)", stage, stage);
    inc = draftInc;
    }
for (i=0; i<width; ++i)
   b[i] = inc[b[i]];
}

void resampleBytes(UBYTE *s, int sct, UBYTE *d, int dct)
/* Shrink or stretch an line of bytes. */
{
#define WHOLESCALE 256
if (sct > dct)	/* going to do some averaging */
	{
	int i;
	int j, jend, lj;
	long lasts, ldiv;
	long acc, div;
	long t1,t2;

	ldiv = WHOLESCALE;
	lasts = s[0];
	lj = 0;
	for (i=0; i<dct; i++)
		{
		acc = lasts*ldiv;
		div = ldiv;
		t1 = (i+1)*(long)sct;
		jend = t1/dct;
		for (j = lj+1; j<jend; j++)
			{
			acc += s[j]*WHOLESCALE;
			div += WHOLESCALE;
			}
		t2 = t1 - jend*(long)dct;
		lj = jend;
		lasts = s[lj];
		if (t2 == 0)
			{
			ldiv = WHOLESCALE;
			}
		else
			{
			ldiv = WHOLESCALE*t2/dct;
			div += ldiv;
			acc += lasts*ldiv;
			ldiv = WHOLESCALE-ldiv;
			}
		*d++ = acc/div;
		}
	}
else if (dct == sct)	/* they's the same */
	{
	while (--dct >= 0)
		*d++ = *s++;
	}
else if (sct == 1)
	{
	while (--dct >= 0)
		*d++ = *s;
	}
else/* going to do some interpolation */
	{
	int i;
	long t1;
	long p1;
	long err;
	int dct2;

	dct -= 1;
	sct -= 1;
	dct2 = dct/2;
	t1 = 0;
	for (i=0; i<=dct; i++)
		{
		p1 = t1/dct;
		err =  t1 - p1*dct;
		if (err == 0)
			*d++ = s[p1];
		else
			*d++ = (s[p1]*(dct-err)+s[p1+1]*err+dct2)/dct;
		t1 += sct;
		}
	}
}

static void cloneDenseDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
UBYTE *aveCounts;
int lineHeight = mgFontLineHeight(font);
struct cloneInfo *ci;
struct cloneFragPos *cfa;   /* List of alignments. */
int s, e, w;
int log2 = digitsBaseTwo(baseWidth);
int shiftFactor = log2 - 17;
int sampleWidth;

if (shiftFactor < 0)
    shiftFactor = 0;
sampleWidth = (baseWidth>>shiftFactor);
AllocArray(useCounts, sampleWidth);
AllocArray(aveCounts, width);
memset(useCounts, 0, sampleWidth * sizeof(useCounts[0]));
for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    char stage = ci->stage;
    for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
	{
	s = ((cfa->start - seqStart)>>shiftFactor);
	e = ((cfa->end - seqStart)>>shiftFactor);
	if (s < 0) s = 0;
	if (e > sampleWidth) e = sampleWidth;
	w = e - s;
	if (w > 0)
	    incStage(useCounts+s, w, stage);
	}
    }
resampleBytes(useCounts, sampleWidth, aveCounts, width);
grayThreshold(aveCounts, width);
hvGfxVerticalSmear(hvg,xOff,yOff,width,lineHeight,aveCounts,TRUE);
freeMem(useCounts);
freeMem(aveCounts);
}

static void cloneFullDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw full  clone items. */
{
int y = yOff;
int lineHeight = mgFontLineHeight(font)+1;
struct cloneInfo *ci;
Color light = tg->ixAltColor;
int oneHeight;
int x1, x2, w;
int baseWidth = seqEnd - seqStart;
int tooBig = (winBaseCount > cloneFragMaxWin);
int hilight = MG_CYAN;
int unfinished = MG_GRAY;
Color standard = color;
boolean gotTiling = hTableExists("tilingPath");
struct sqlConnection *conn = NULL;
int bgColor;
char accOnly[64];
boolean nofrag = (strcmp("Clone Coverage/Fragment Position", tg->longLabel));

if (gotTiling)
    conn = hAllocConn();
for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    bgColor = light;
    if (gotTiling)
	{
	char query[256], buf[256];
	strcpy(accOnly, ci->name);
	chopSuffix(accOnly);
	sprintf(query, "select accession from tilingPath where accession = '%s'", accOnly);
        if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
	    bgColor = hilight;
	}
    /* Check if track no longer showing fragments (starting with hg15) */
    if ((nofrag) && (ci->phase < 3)) 
        color = unfinished;
    else
	color = standard;
	
    if (!tooBig)
	oneHeight = oneOrRowCount(ci)*lineHeight+2;
    else
	oneHeight = lineHeight;
    x1 = roundingScale(ci->cloneStart-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(ci->cloneEnd-winStart, width, baseWidth)+xOff;
    w = x2-x1;
    hvGfxBox(hvg, x1, y, w, oneHeight-1, bgColor);
    if (!tooBig)
	drawOneClone(ci, seqStart, seqEnd, hvg, xOff, y+1, width, font, lineHeight, 
		color, TRUE, tg->subType, nofrag);
    else
	drawOneClone(ci, seqStart, seqEnd, hvg, xOff, y, width, font, oneHeight-1, 
		color, FALSE, tg->subType, nofrag);
    y += oneHeight;
    }
hFreeConn(&conn);
}

static void cloneDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw clone items. */
{
if (vis == tvFull)
    cloneFullDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else
    cloneDenseDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}


#ifdef UNUSED
struct hash *realiCloneHash;
struct cloneInfo *realiCloneList;

void loadRealiClonesInWindow()
/* Load up realiCloneHash and realiCloneList with the clones in the window. */
{
if (realiCloneList == NULL)
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    char **row;
    struct cloneInfo *ci;
    struct psl *psl;
    char *fragName;
    struct cloneFrag *cf;
    char cloneName[128];
    struct hashEl *hel;
    struct cloneFragPos *cfa;
    char *s;
    struct clonePos cp;

    /* Load in clone extents from database. */
    realiCloneHash = newHash(12);
    sprintf(query, 
    	"select * from cloneAliPos where chrom='%s'and chromStart<%u and chromEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	clonePosStaticLoad(row, &cp);
	AllocVar(ci);
	hel = hashAdd(realiCloneHash, cp.name, ci);
	ci->name = hel->name;
	ci->cloneStart = cp.chromStart;
	ci->cloneEnd = cp.chromEnd;
	ci->phase = cp.phase;
	slAddHead(&realiCloneList, ci);
	}
    sqlFreeResult(&sr);

    /* Load in alignments from database and sort them by clone. */
    sprintf(query, "select * from %s_frags where tStart<%u and tEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row);
	fragName = psl->qName;
	strcpy(cloneName, fragName);
	s = strchr(cloneName, '_');
	if (s != NULL)
	    *s = 0;
	if ((hel = hashLookup(realiCloneHash, cloneName)) == NULL)
	    {
	    warn("%s not in range in cloneAliPos", cloneName);
	    continue;
	    }
	ci = hel->val;
	if ((cf = findCloneFrag(ci, fragName)) == NULL)
	    {
	    AllocVar(cf);
	    cf->name = fragName;
	    slAddHead(&ci->fragList, cf);
	    }
	AllocVar(cfa);
	cfa->frag = cf;
	cfa->psl = psl;
	cfa->start = psl->tStart;
	cfa->end = psl->tEnd;
	slAddHead(&ci->cfaList, cfa);
	}
    slSort(&realiCloneList, cmpCloneInfo);
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    /* Do preliminary layout processing for each clone. */
    for (ci = realiCloneList; ci != NULL; ci = ci->next)
	{
	slReverse(&ci->cfaList);
	layoutCloneAli(ci);
	}
    }
}
#endif /* UNUSED */

struct hash *glCloneHash;
struct cloneInfo *glCloneList = NULL;

void glLoadInWindow()
/* Load up glCloneHash and glCloneList with the clones in the window. */
{
if (glCloneList == NULL)
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    char **row;
    struct cloneInfo *ci;
    struct gl *gl;
    char *fragName;
    struct cloneFrag *cf;
    char cloneName[128];
    struct hashEl *hel;
    struct cloneFragPos *cfa;
    struct clonePos cp;
    char *s;
    int rowOffset;

    /* Load in clone extents from database. */
    glCloneHash = newHash(12);
    sr = hRangeQuery(conn, "clonePos", chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	clonePosStaticLoad(row+rowOffset, &cp);
	AllocVar(ci);
	hel = hashAdd(glCloneHash, cp.name, ci);
	ci->name = hel->name;
	ci->cloneStart = cp.chromStart;
	ci->cloneEnd = cp.chromEnd;
	ci->phase = cp.phase;
	ci->stage = cp.stage[0];
	slAddHead(&glCloneList, ci);
	}
    sqlFreeResult(&sr);

    /* Load in gl from database and sort them by clone. */
    sr = hRangeQuery(conn, "gl", chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gl = glLoad(row+rowOffset);
	fragName = gl->frag;
	strcpy(cloneName, fragName);
	s = strchr(cloneName, '.');
	if (s != NULL)
	    s = strchr(s, '_');
	if (s != NULL)
	    *s = 0;
	if ((hel = hashLookup(glCloneHash, cloneName)) == NULL)
	    {
	    if (!sameString(database, "hg4") && !sameString(database, "hg5")
	    	&& !sameString(database, "hg6") && !sameString(database, "hg7"))
		/* Honestly, Asif and Jim will fix this someday! */
		warn("%s not in range in clonePos", cloneName);
	    continue;
	    }
	ci = hel->val;
	if ((cf = findCloneFrag(ci, fragName)) == NULL)
	    {
	    AllocVar(cf);
	    cf->name = fragName;
	    slAddHead(&ci->fragList, cf);
	    }
	AllocVar(cfa);
	cfa->frag = cf;
	cfa->gl = gl;
	cfa->start = gl->start;
	cfa->end = gl->end;
	slAddHead(&ci->cfaList, cfa);
	}
    slSort(&glCloneList, cmpCloneInfo);
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    /* Do preliminary layout processing for each clone. */
    for (ci = glCloneList; ci != NULL; ci = ci->next)
	{
	slReverse(&ci->cfaList);
	layoutCloneAli(ci);
	}
    }
}

void coverageLoad(struct track *tg)
/* Load up clone alignments from database tables and organize. */
{
glLoadInWindow();
tg->items = glCloneList;
}

void coverageFree(struct track *tg)
/* Free up clone track items. */
{
cloneInfoFreeList(&glCloneList);
freeHash(&glCloneHash);
}

void coverageMethods(struct track *tg)
/* Make track for golden path positions of all frags. */
{
tg->loadItems = coverageLoad;
tg->freeItems = coverageFree;
tg->drawItems = cloneDraw;
tg->itemName = cloneName;
tg->mapItemName = cloneName;
tg->totalHeight = cloneTotalHeight;
tg->itemHeight = cloneItemHeight;
tg->itemStart = cloneItemStart;
tg->itemEnd = cloneItemEnd;
}

