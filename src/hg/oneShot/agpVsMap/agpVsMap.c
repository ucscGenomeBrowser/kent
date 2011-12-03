/* agpVsMap - Plot clones in agp file vs. map coordinates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memgfx.h"
#include "agpFrag.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpVsMap - Plot clones in agp file vs. map coordinates\n"
  "usage:\n"
  "   agpVsMap XXX\n");
}

struct cloneInfo
/* Info on one clone. */
    {
    struct cloneInfo *next;     /* next in list. */
    char *name;                 /* Name, allocated in hash. */
    struct agpFrag *bp;        /* Position in agp. */
    struct mapPos *mp;          /* Position in map. */
    };

struct agpFrag *readAgpFile(char *agpName)
/* Read agps from file. */
{
struct lineFile *lf = lineFileOpen(agpName, TRUE);
int wordCount;
char *words[16];
struct agpFrag *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (words[4][0] != 'N')
        {
	lineFileExpectWords(lf, 9, wordCount);
	el = agpFragLoad(words);
	slAddHead(&list, el);
	}
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

void drawPlus(struct memGfx *mg, int x, int y, Color color)
/* Draw a little tiny plus symbol centered at x/y. */
{
int plusSize = 5;
int plusOff = 2;

mgDrawBox(mg, x-plusOff, y, plusSize, 1, color);
mgDrawBox(mg, x, y-plusOff, 1, plusSize, color);
}

void agpVsMap(char *agpName, char *infoName, char *gifName)
/* agpVsMap - Plot clones in agp vs. map coordinates. */
{
struct mapPos *mapList, *mp;
struct agpFrag *agpList, *bp;
struct hash *cloneHash = newHash(14);
struct hashEl *hel;
struct cloneInfo *cloneList = NULL, *clone;
struct memGfx *mg = NULL;
int pixWidth = 600;
int pixHeight = 600;
int rulerHeight = 20;
int maxMapPos = 0, maxAgpPos = 0;
double scaleMap, scaleAgp;
Color orange, green;

mapList = readInfoFile(infoName);
agpList = readAgpFile(agpName);

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

for (bp = agpList; bp != NULL; bp = bp->next)
    {
    if (bp->chromStart > maxAgpPos) maxAgpPos = bp->chromStart;
    }

/* Draw scatterplot on bitmap. */
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
orange = mgFindColor(mg, 210, 150, 0);
green = mgFindColor(mg, 0, 200, 0);
mgDrawRuler(mg, 0, pixHeight-rulerHeight, rulerHeight, pixWidth, MG_BLACK,
       mgSmallFont(), 0, maxMapPos+1);
scaleMap = (double)pixWidth/(double)(maxMapPos+1.0);
scaleAgp = (double)(pixHeight)/(double)(maxAgpPos+1.0);
for (bp = agpList; bp != NULL; bp = bp->next)
    {
    char cloneName[128];
    fragToCloneName(bp->frag, cloneName);
    clone = hashFindVal(cloneHash, cloneName);
    if (clone == NULL)
        warn("%s is in %s but not %s", cloneName, 
	    agpName, infoName);
    else
	{
	int x = round(scaleMap*clone->mp->pos);
	int y = pixHeight - round(scaleAgp*bp->chromStart);
	int phase = clone->mp->phase;
	int back;
	if (phase <= 1) back = green;
	else if (phase == 2) back = orange;
	else back = MG_RED;
	drawPlus(mg, x, y, back);
	}
    }

mgSaveGif(mg, gifName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
agpVsMap(argv[1], argv[2], argv[3]);
return 0;
}
