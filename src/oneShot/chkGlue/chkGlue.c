/* chkGlue - help check gluing by displaying where contigs
 * go against glued and finished versions of same sequence. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "patSpace.h"
#include "fuzzyFind.h"
#include "memgfx.h"
#include "cheapcgi.h"
#include "htmshell.h"

#ifdef WIN32
char *oocFile = "e:/biodata/human/htg.ooc";       
char *finDir = "e:/biodata/human/testSets/fin";       /* Where to find finished seq. */
char *fullMaskDir = "e:/biodata/human/testSets/fullMask";
char *longMaskDir = "e:/biodata/human/testSets/longMask";
char *shortMaskDir = "e:/biodata/human/testSets/shortMask";
char *unfinDir = "e:/biodata/human/testSets/unfin";   /* Where to find unfinished seq. */
char *gluedDir = "e:/biodata/human/testSets/glued";     /* Where to find glued seq. */
#else
char *oocFile = "/projects/compbio/experiments/hg/h/10.ooc";       
char *finDir = "/projects/compbio/experiments/hg/testSets/chkGlue/fin";       /* Where to find finished seq. */
char *fullMaskDir = "/projects/compbio/experiments/hg/testSets/chkGlue/finFullMask";
char *longMaskDir = "/projects/compbio/experiments/hg/testSets/chkGlue/finLongMask";
char *shortMaskDir = "/projects/compbio/experiments/hg/testSets/chkGlue/finShortMask";
char *unfinDir = "/projects/compbio/experiments/hg/testSets/chkGlue/unfin";   /* Where to find unfinished seq. */
char *gluedDir = "/projects/compbio/experiments/hg/testSets/chkGlue/glued";     /* Where to find glued seq. */
#endif

char finBac[512];      /* File with finished seq. */
char unfinBac[512];    /* File with unfinished seq. */
char gluedBac[512];    /* File with glued seq. */

int trackHeight;
int trackWidth;
int border = 2;
MgFont *font;

enum mapType
/* There's one of these for each type of area in the graphic display
 * that the user can click on. */
    {
    mtEnd,
    mtNone,
    mtBlock,
    };

void mapReadString(FILE *f, char **ps)
/* Read string from map file. */
{
short len;
char *s;
mustReadOne(f, len);
if (len == 0)
    s = NULL;
else
    {
    s = needMem(len+1);
    mustRead(f, s, len);
    }
*ps = s;
}

void mapWriteString(FILE *f, char *s)
/* Write string to map file. */
{
short len;

if (s == NULL)
    {
    len = 0;
    writeOne(f, len);
    }
else
    {
    len = strlen(s);
    writeOne(f, len);
    mustWrite(f, s, len);
    }
}

void mapWriteBox(FILE *f, bits16 mt, int x, int y, int width, int height, 
    char *bac, bits16 contig, int qStart, int qSize, int tStart, int tSize)
/* Write out one hit box. */
{
writeOne(f, mt);
writeOne(f, x);
writeOne(f, y);
writeOne(f, width);
writeOne(f, height);
mapWriteString(f, bac);
writeOne(f, contig);
writeOne(f, qStart);
writeOne(f, qSize);
writeOne(f, tStart);
writeOne(f, tSize);
}

void mapReadBox(FILE *f, bits16 *mt, int *x, int *y, int *width, int *height, 
    char **bac, bits16 *contig, int *qStart, int *qSize, int *tStart, int *tSize)
/* Read in one hit box. */
{
mustReadOne(f, *mt);
mustReadOne(f, *x);
mustReadOne(f, *y);
mustReadOne(f, *width);
mustReadOne(f, *height);
mapReadString(f, bac);
mustReadOne(f, *contig);
mustReadOne(f, *qStart);
mustReadOne(f, *qSize);
mustReadOne(f, *tStart);
mustReadOne(f, *tSize);
}

boolean mapScanForHit(FILE *f, int mouseX, int mouseY, 
    bits16 *mt, int *x, int *y, int *width, int *height, char **bac, 
    bits16 *contig, int *qStart, int *qSize, int *tStart, int *tSize)
/* Scan file for one that hits mouse.  Return TRUE if get one. */
{
for (;;)
    {
    mapReadBox(f, mt, x, y, width, height, bac, contig, qStart, qSize, tStart, tSize);
    if (*mt == mtEnd)
        return FALSE;
    if (mouseX >= *x && mouseX < *x + *width 
        && mouseY >= *y && mouseY < *y + *height)
        {
        return TRUE;
        }
    freez(bac);
    }
}

bits32 gluMapSig = 0x60424d96;

void mapWriteHead(FILE *f, bits32 width, bits32 height, char *bac, int trim, char *repeatMask)
/* Write start of map file. */
{
bits32 sig = gluMapSig;
writeOne(f, sig);
writeOne(f,width);
writeOne(f,height);
mapWriteString(f, bac);
writeOne(f, trim);
mapWriteString(f, repeatMask);
}

void mapReadHead(FILE *f, bits32 *width, bits32 *height, char **bac, int *trim, char **repeatMask)
/* Read start of map file and abort if it isn't right. */
{
bits32 sig;
mustReadOne(f, sig);
if (sig != gluMapSig)
    errAbort("Bad map file");
mustReadOne(f,*width);
mustReadOne(f,*height);
mapReadString(f, bac);
mustReadOne(f, *trim);
mapReadString(f, repeatMask);
}

void freeAllSeq(struct dnaSeq **pList)
/* Free all sequences on list. */
{
struct dnaSeq *seq, *next;
if (*pList != NULL)
    {
    for (seq = *pList; seq != NULL; seq = next)
        {
        next = seq->next;
        freeDnaSeq(&seq);
        }
    *pList = NULL;
    }
}


Color blockColors[8];
struct rgbColor blockRgbColors[8] =
    {
        {0,0,0},
        {128, 0, 0},
        {0, 96, 0},
        {0, 0, 150},
        {80, 80, 80},
        {0, 80, 80},
        {100, 0, 100},
        {80, 80, 0},
    };

void makeBlockColors(struct memGfx *mg)
/* Get some defined colors to play with. */
{
int i;
struct rgbColor *c;
for (i=0; i<ArraySize(blockColors); ++i)
    {
    c = blockRgbColors+i;
    blockColors[i] = mgFindColor(mg, c->r, c->g, c->b);
    }
}

struct aliList
/* A list of scored alignments. */
    {
    struct aliList *next;
    struct ffAli *ali;
    boolean isRc;
    int score;
    };

int cmpAliList(const void *va, const void *vb)
/* Compare two aliLists by whatVal. */
{
const struct aliList *a = *((struct aliList **)va);
const struct aliList *b = *((struct aliList **)vb);
return b->score - a->score;
}


boolean fastFind(DNA *needle, int needleSize, 
    struct patSpace *ps, struct ffAli **retAli, boolean *retRc, int *retScore)
/* Do fast alignment. */
{
struct patClump *clumpList, *clump;
boolean isRc;
struct aliList *aliList = NULL, *ali;

for (isRc = 0; isRc <= 1; ++isRc)
    {
    if (isRc)
        reverseComplement(needle, needleSize);
    if ((clumpList = patSpaceFindOne(ps, needle, needleSize)) != NULL)
        {
        for (clump = clumpList; clump != NULL; clump = clump->next)
            {
            struct dnaSeq *haySeq = clump->seq;
            DNA *haystack = haySeq->dna;
            int start = clump->start;
            struct ffAli *ffAli = ffFind(needle, needle+needleSize, 
                haystack+start, haystack+start+clump->size, ffCdna);
            if (ffAli != NULL)
                {
                AllocVar(ali);
                ali->ali = ffAli;
                ali->score = ffScoreCdna(ffAli);
                ali->isRc = isRc;
                slAddHead(&aliList, ali);
                }
            }
        slFreeList(&clumpList);
        }
    if (isRc)
        reverseComplement(needle, needleSize);
    }
if (aliList != NULL)
    {
    slSort(&aliList, cmpAliList);
    *retAli = aliList->ali;
    aliList->ali = NULL;
    *retRc = aliList->isRc;
    *retScore = aliList->score;
    for (ali = aliList->next; ali != NULL; ali = ali->next)
        ffFreeAli(&ali->ali);
    slFreeList(&aliList);
    return TRUE;
    }
else
    return FALSE;
}

void aliTrack(char *bacAcc, char *wholeName, char *partsName, 
    struct memGfx *mg, int x, int y, FILE *mapFile, int trim, char *repeatMask)
/* Write out one alignment track. */
{
struct dnaSeq *whole, *partList, *part;
bits16 contig;
int maxBlockSize = 5000;
int wholeSize;
struct patSpace *ps;
DNA *wholeDna;

whole = faReadAllDna(wholeName);
if (slCount(whole) > 1)
    warn("%d sequences in %s, only using first", slCount(whole), wholeName);
wholeDna = whole->dna;
wholeSize = whole->size;
ps = makePatSpace(&whole, 1, oocFile, 5, 500);
partList = faReadAllDna(partsName);
printf("%d contigs in %s\n\n", slCount(partList), partsName);

for (part = partList, contig = 0; part != NULL; part = part->next, ++contig)
    {
    DNA *dna = part->dna;
    int dnaSize = part->size;
    int start, size;
    int subIx = 0;
    char numText[12];

    Color color = blockColors[contig%ArraySize(blockColors)];
    sprintf(numText, "%d", contig+1);
    for (start = trim; start < dnaSize-trim; start += size)
        {
        struct ffAli *left, *right;
        boolean rc;
        int score;

        size = dnaSize - start-trim;
        if (size > maxBlockSize)
            size = maxBlockSize;
        if (!fastFind(dna+start, size, ps, &left, &rc, &score) )
            {
            printf("Contig %d.%d:%d-%d of %d UNALIGNED\n",
                contig+1, subIx, start, start+size, dnaSize);
            }
        else
            {
            int x1, x2;
            int xo, w;
            double quality;
            int qStart, qSize, tStart,tSize;
            char qualityString[40];

            right = left;
            while (right->right != NULL)
                right = right->right;
            qStart = left->nStart - dna;
            qSize = right->nEnd - left->nStart;
	    if (rc)
		{
		int rcEnd = right->nEnd - (dna+start) - 1;
		qStart = reverseOffset(rcEnd, size) + start;
		}
            tStart = left->hStart - wholeDna;
            tSize = right->hEnd - left->hStart;
            quality = 100.0 * score / qSize;
            if (quality >= 25.0)
                sprintf(qualityString, "%4.1f%%", quality);
            else
                sprintf(qualityString, "<50%%");

            printf("<A HREF=\"../cgi-bin/chkGlue.exe?bacAcc=%s&contig=%d&qStart=%d&qSize=%d&tStart=%d&tSize=%d&repeatMask=%s\">",
                bacAcc, contig, qStart, qSize, tStart, tSize, repeatMask);

            printf("Contig %d.%d:%d-%d %c of %d aligned %d-%d of %d aliSize %d quality %s</A>\n",
                contig+1, subIx, qStart, qStart+qSize, 
                (rc ? '-' : '+'), dnaSize, 
                tStart, tStart + tSize,
                wholeSize,
                qSize, qualityString);
            x1 = roundingScale(trackWidth, left->hStart - wholeDna, wholeSize);
            x2 = roundingScale(trackWidth, right->hEnd - wholeDna, wholeSize);
            xo = x1+x;
            w = x2-x1;
            mapWriteBox(mapFile, mtBlock, xo, y, w, trackHeight,
                bacAcc, contig, qStart, qSize, tStart, tSize);
            mgDrawBox(mg, xo, y, w, trackHeight, color);
            mgTextCentered(mg, xo, y, w, trackHeight, MG_WHITE, font, numText);
            ffFreeAli(&left);
            }
        ++subIx;
        }
    }
freePatSpace(&ps);
freeAllSeq(&whole);
freeAllSeq(&partList);
}


void chkGlue(char *bacAcc, char *finBac, char *unfinBac, char *gluedBac, int trim, char *repeatMask)
/* Display glued and unglued form of BAC. */
{
int trackCount = 1;
int pixWidth, pixHeight;
int x, y;
struct memGfx *mg;
struct tempName gifTn, mapTn;
FILE *mapFile;


printf("See picture at bottom for overview of where contigs align.\n\n");

/* Figure out basic dimensions and allocate picture. */
font = mgSmallFont();
trackWidth = 700;
trackHeight = mgFontPixelHeight(font) + 4;
pixWidth = trackWidth + 2*border;
pixHeight = trackCount * (trackHeight+border) + border;
x = y = border;
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
makeBlockColors(mg);

/* Create map file. */
makeTempName(&mapTn, "glu", ".map");
mapFile = mustOpen(mapTn.forCgi, "wb");
mapWriteHead(mapFile, pixWidth, pixHeight, bacAcc, trim, repeatMask);


/* Write out tracks onto picture. */
aliTrack(bacAcc, finBac, unfinBac, mg, x, y, mapFile, trim, repeatMask);

/* Save pic and tell html file about it. */
makeTempName(&gifTn, "glu", ".gif");
mgSaveGif(mg, gifTn.forCgi);
printf("<INPUT TYPE=HIDDEN NAME=map VALUE=\"%s\">\n", mapTn.forCgi);
printf(
    "<P><INPUT TYPE=IMAGE SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME = \"clickMe\" ALIGN=BOTTOM><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight);

printf("Click on contig for detailed alignment\n");

/* Write end of map */
mapWriteBox(mapFile, mtNone, 0, 0, pixWidth, pixHeight, NULL, 0, 0, 0, 0, 0);
mapWriteBox(mapFile, mtEnd, 0, 0, pixWidth, pixHeight, NULL, 0, 0, 0, 0, 0);

/* Clean up. */
fclose(mapFile);
mgFree(&mg);
}

void makeFileNames(char *bacAcc, char *repeatMask)
/* Combine directory and bac names to make file names. */
{
if (sameWord(repeatMask, "none"))
    sprintf(finBac, "%s/%s.fa", finDir, bacAcc);
else if (sameWord(repeatMask, "full"))
    sprintf(finBac, "%s/%s.fa", fullMaskDir, bacAcc);
else if (sameWord(repeatMask, "short"))
    sprintf(finBac, "%s/%s.fa", shortMaskDir, bacAcc);
else if (sameWord(repeatMask, "long"))
    sprintf(finBac, "%s/%s.fa", longMaskDir, bacAcc);
else
    errAbort("Unknown repeatMask value %s", repeatMask);

sprintf(unfinBac, "%s/%s.fa", unfinDir, bacAcc);
sprintf(gluedBac, "%s/%s.fa", gluedDir, bacAcc);
}

void bacTrack(char *bacAcc, int trim, char *repeatMask)
/* Do a little bac track display */
{
printf("<H1>chkGlue %s</H1>\n", bacAcc);
printf("<TT><PRE>\n");
makeFileNames(bacAcc, repeatMask);

/* Tell browser where to go when they click on image. */
printf("<FORM ACTION=\"%schkGlue.exe\">\n\n", cgiDir());
chkGlue(bacAcc, finBac, unfinBac, gluedBac, trim, repeatMask);
}

void showDetailedMatch(char *bacAcc, int contig, int qStart, int qSize, int tStart, int tSize, char *repeatMask)
/* Process a click on active area. */
{
struct dnaSeq *queryList, *query, *target;
struct ffAli *ali;
boolean rc;
char needleName[128], hayName[128];

makeFileNames(bacAcc, repeatMask);
queryList = faReadAllDna(unfinBac);
target = faReadDna(finBac);
query = slElementFromIx(queryList, contig);

if (!ffFindEitherStrandN(query->dna + qStart, qSize, target->dna + tStart, tSize,
    ffCdna, &ali, &rc))
    {
    errAbort("Couldn't align %s and %s", unfinBac, finBac);
    }
sprintf(needleName, "%s contig %d %d-%d", bacAcc, contig, qStart, qStart+qSize);
sprintf(hayName, "%s finished %d-%d", bacAcc, tStart, tStart+tSize);
query->dna[qStart+qSize] = 0;
target->dna[tStart+tSize] = 0;
ffShowAli(ali, needleName, query->dna + qStart, 0,
               hayName, target->dna + tStart, 0, rc);
}

void doMap(char *mapName)
/* Interpret click on map. */
{
FILE *f = mustOpen(mapName, "rb");
int x, y, width, height;
char *mapBac, *mouseBac;
char *repeatMask;
int mouseX, mouseY;
bits16 mt, contig;
int trim;
int qStart, qSize, tStart, tSize;
boolean ok;

/* Figure out where clicked from map file. */
mapReadHead(f, &width, &height, &mapBac, &trim, &repeatMask);
mouseX = cgiInt("clickMe.x");
mouseY = cgiInt("clickMe.y");
ok = mapScanForHit(f, mouseX, mouseY, 
    &mt, &x, &y, &width, &height, 
    &mouseBac, &contig, &qStart, &qSize, &tStart, &tSize);
if (!ok)
    mt = mtNone;
fclose(f);

if (mt == mtNone)
    {
    bacTrack(mapBac, trim, repeatMask);
    }
else if (mt == mtBlock)
    {
    showDetailedMatch(mouseBac, contig, qStart, qSize, tStart, tSize, repeatMask);
    }
}

void doFuzzyFind()
/* Do fuzzy-finder alignment. */
{
char *bacAcc = cgiString("bacAcc");
char *repeatMask = cgiString("repeatMask");
int contig = cgiInt("contig");
int qStart = cgiInt("qStart");
int qSize = cgiInt("qSize");
int tStart = cgiInt("tStart");
int tSize = cgiInt("tSize");
showDetailedMatch(bacAcc, contig, qStart, qSize, tStart, tSize, repeatMask);
}

void doMiddle()
/* Write HTML file to stdout. */
{
char *mapName;
if ((mapName = cgiOptionalString("map")) != NULL)
    {
    printf("<TT>\n");
    doMap(mapName);
    }
else if (cgiVarExists("contig"))
    {
    doFuzzyFind();
    }
else
    {
    char *bacAcc = cgiString("bacAcc");
    char *repeatMask = cgiString("repeatMask");
    int trim = cgiInt("trim");
    if (trim < 0)
        trim = 0;
    bacTrack(bacAcc, trim, repeatMask);
    }
}

int main(int argc, char *argv[])
{
if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
    putenv("QUERY_STRING=bacAcc=AC005057");
    }

htmShell("Check Glue Display", doMiddle, NULL);
return 0;
}
