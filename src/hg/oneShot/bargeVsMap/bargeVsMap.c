/* bargeVsMap - Plot clones in barge vs. map coordinates. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "memgfx.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bargeVsMap - Plot clones in barge vs. map coordinates\n"
  "usage:\n"
  "   bargeVsMap bargeFile infoFile output.gif\n");
}

struct cloneInfo
/* Info on one clone. */
    {
    struct cloneInfo *next;     /* next in list. */
    char *name;                 /* Name, allocated in hash. */
    struct bargePos *bp;        /* Position in barge. */
    struct mapPos *mp;          /* Position in map. */
    };

struct bargePos
/* Position of clone in barge. */
    {
    struct bargePos *next;	/* Next in list. */
    char *cloneName;            /* Name of clone. */
    int pos;                    /* Contig position in barge file. */
    int size;                   /* Base size. */
    int overlapNext;            /* Overlap with next. */
    char *maxClone;             /* Name of clone overlaps with most. */
    int maxOverlap;             /* Size of overlaps with most. */
    int bargeId;                /* Which barge. */
    };

struct bargePos *readBargeFile(char *bargeName)
/* Read barges from file. */
{
struct lineFile *lf = lineFileOpen(bargeName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
struct bargePos *list = NULL, *el;
int bargeId = 0;

lineFileNeedNext(lf, &line, &lineSize);
if (!startsWith("Barge", line))
    errAbort("%s is not a barge file", bargeName);
lineFileSkip(lf, 3);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '-')
	{
	++bargeId;
        continue;
	}
    wordCount = chopLine(line, words);
    lineFileExpectWords(lf, 6, wordCount);
    AllocVar(el);
    el->pos = atoi(words[0]);
    el->cloneName = cloneString(words[1]);
    el->size = atoi(words[2]);
    el->overlapNext = atoi(words[3]);
    el->maxClone = cloneString(words[4]);
    el->maxOverlap = atoi(words[5]);
    el->bargeId = bargeId;
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct mapPos
/* Position of clone in map. */
    {
    struct mapPos *next;	/* Next in list. */
    char *cloneName;            /* Name of clone. */
    int pos;                    /* Position in info file. */
    int phase;                  /* HTG phase. */
    };

struct mapPos *readInfoFile(char *mapName)
/* Read maps from file. */
{
struct lineFile *lf = lineFileOpen(mapName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
struct mapPos *list = NULL, *el;

lineFileNeedNext(lf, &line, &lineSize);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    lineFileExpectWords(lf, 3, wordCount);
    AllocVar(el);
    el->cloneName = cloneString(words[0]);
    el->pos = atoi(words[1]);
    el->phase = atoi(words[2]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void drawPlus(struct memGfx *mg, int x, int y, Color color, Color back)
/* Draw a little tiny plus symbol centered at x/y. */
{
int plusSize = 5;
int plusOff = 2;

mgDrawBox(mg, x-plusOff, y-plusOff, plusSize, plusSize, back);
mgDrawBox(mg, x-plusOff, y, plusSize, 1, color);
mgDrawBox(mg, x, y-plusOff, 1, plusSize, color);
}

void bargeVsMap(char *bargeName, char *infoName, char *gifName)
/* bargeVsMap - Plot clones in barge vs. map coordinates. */
{
struct mapPos *mapList, *mp;
struct bargePos *bargeList, *bp;
struct hash *cloneHash = newHash(14);
struct hashEl *hel;
struct cloneInfo *cloneList = NULL, *clone;
struct memGfx *mg = NULL;
int pixWidth = 600;
int pixHeight = 600;
int rulerHeight = 20;
int maxMapPos = 0, maxBargePos = 0;
double scaleMap, scaleBarge;
Color green;

mapList = readInfoFile(infoName);
bargeList = readBargeFile(bargeName);

for (mp = mapList; mp != NULL; mp = mp->next)
    {
    if (mp->phase > 0)
        {
	AllocVar(clone);
	hel = hashAddUnique(cloneHash, mp->cloneName, clone);
	clone->name = hel->name;
	clone->mp = mp;
	slAddHead(&cloneList, clone);
	if (mp->pos > maxMapPos) maxMapPos = mp->pos;
	}
    }
slReverse(&cloneList);

for (bp = bargeList; bp != NULL; bp = bp->next)
    {
    clone = hashFindVal(cloneHash, bp->cloneName);
    if (clone == NULL)
        warn("%s is in %s but not %s", bp->cloneName, 
	    bargeName, infoName);
    else
	{
	clone->bp = bp;
	if (bp->pos > maxBargePos) maxBargePos = bp->pos;
	}
    }

/* Draw scatterplot on bitmap. */
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
green = mgFindColor(mg, 0, 150, 0);
mgDrawRuler(mg, 0, pixHeight-rulerHeight, rulerHeight, pixWidth, MG_BLACK,
       mgSmallFont(), 0, maxMapPos+1);
scaleMap = (double)pixWidth/(double)(maxMapPos+1.0);
scaleBarge = (double)(pixHeight)/(double)(maxBargePos+1.0);
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->bp && clone->mp)
        {
	int x = round(scaleMap*clone->mp->pos);
	int y = pixHeight - round(scaleBarge*clone->bp->pos);
	int colMod = clone->bp->bargeId%3;
	int phase = clone->mp->phase;
	int col, back;
	if (colMod == 0) col = MG_BLUE;
	else if (colMod == 1) col = MG_RED;
	else if (colMod == 2) col = green;
	if (phase <= 1) back = MG_WHITE;
	else if (phase == 2) back = MG_GRAY;
	else back = MG_BLACK;
	drawPlus(mg, x, y, back, col);
	}
    }

mgSaveGif(mg, gifName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
bargeVsMap(argv[1], argv[2], argv[3]);
return 0;
}
