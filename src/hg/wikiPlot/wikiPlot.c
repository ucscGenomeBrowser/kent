/* wikiPlot - Quick plots of maps vs. each other. */ 
#include "common.h" 
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "portable.h"
#include "memgfx.h"


/* Variables that can be overridden by CGI. */
char *contigDir = "/projects/hg3/gs.7/oo.29/19/ctg18433";
char *mapX = "info.noNt";
char *mapY = "gold.99";
int pix = 500;
double xOff = -0.05, yOff = -0.05, zoom = 0.9;

/* Span range of values in x and y. */
int xStart, xEnd, yStart, yEnd;
boolean plotName = TRUE;
MgFont *font;

struct clonePos
    {
    struct clonePos *next;
    char *name;	/* Name of clone. */
    int pos;	/* Position in map. */
    double totSize;	/* Total fragment size (just for gold) */
    double weightedPos;	/* Total weighted position (just for gold) */
    };

double scaleOne(double raw, double rangeStart, double rangeEnd, int pix,
	double offset, double zoom)
/* Scale one coordinate. */
{
double range = rangeEnd - rangeStart + 1;
double pos = ((raw - rangeStart) / range - offset) * zoom;
return pix*pos;
}

void zoomScale(double x, double y, int *retX, int *retY)
/* Scale both coordinates. */
{
*retX = round(scaleOne(x, xStart, xEnd, pix, xOff, zoom));
*retY = round(scaleOne(y, yStart, yEnd, pix, yOff, zoom));
}

void readInfo(char *fileName, struct clonePos **retList, struct hash **retHash)
/* Read info formatted files */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[3];
struct clonePos *cpList = NULL, *cp;
struct hash *hash = newHash(0);

if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
while (lineFileRow(lf, row))
    {
    if (hashLookup(hash, row[0]))
        warn("%s duplicated in %s, ignoring all but first", row[0], fileName);
    else 
        {
	AllocVar(cp);
	hashAddSaveName(hash, row[0], cp, &cp->name);
	cp->pos = lineFileNeedNum(lf, row, 1);
	slAddHead(&cpList, cp);
	}
    }
lineFileClose(&lf);
slReverse(&cpList);
*retList = cpList;
*retHash = hash;
}

void readGold(char *fileName, struct clonePos **retList, struct hash **retHash)
/* Read .agp/gold formatted file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[12];
struct clonePos *cpList = NULL, *cp;
struct hash *hash = newHash(0);
int wordCount;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *type = words[4];
    char *clone = words[5];
    int fragStart, fragEnd;
    double fragSize;
    if (type[0] == 'N')
        continue;
    lineFileExpectWords(lf, 9, wordCount);
    chopSuffix(clone);
    fragStart = lineFileNeedNum(lf, words, 1)-1;
    fragEnd = lineFileNeedNum(lf, words, 2);
    fragSize = fragEnd - fragStart;
    if ((cp = hashFindVal(hash, clone)) == NULL)
        {
	AllocVar(cp);
	hashAddSaveName(hash, clone, cp, &cp->name);
	slAddHead(&cpList, cp);
	}
    cp->weightedPos += fragSize * fragStart;
    cp->totSize += fragSize;
    }
lineFileClose(&lf);
slReverse(&cpList);
for (cp = cpList; cp != NULL; cp = cp->next)
    cp->pos = cp->weightedPos/cp->totSize;
*retList = cpList;
*retHash = hash;
}


void readGl(char *fileName, struct clonePos **retList, struct hash **retHash)
/* Read gl formatted files. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
struct clonePos *cpList = NULL, *cp;
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    char *clone = row[0];
    int fragStart, fragEnd;
    double fragSize;
    chopSuffix(clone);
    fragStart = lineFileNeedNum(lf, row, 1);
    fragEnd = lineFileNeedNum(lf, row, 2);
    fragSize = fragEnd - fragStart;
    if ((cp = hashFindVal(hash, clone)) == NULL)
        {
	AllocVar(cp);
	hashAddSaveName(hash, clone, cp, &cp->name);
	slAddHead(&cpList, cp);
	}
    cp->weightedPos += fragSize * fragStart;
    cp->totSize += fragSize;
    }
lineFileClose(&lf);
slReverse(&cpList);
for (cp = cpList; cp != NULL; cp = cp->next)
    cp->pos = cp->weightedPos/cp->totSize;
*retList = cpList;
*retHash = hash;
}

void readMmBarge(char *fileName, struct clonePos **retList, struct hash **retHash)
/* Read mmBarge formatted files. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[3];
struct clonePos *cpList = NULL, *cp;
struct hash *hash = newHash(0);

/* Open file and verify header. */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
if (!startsWith("Barge", line))
    errAbort("%s is not an mmBarge file", fileName);
lineFileNext(lf, &line, NULL);
lineFileNext(lf, &line, NULL);
lineFileNext(lf, &line, NULL);
if (!startsWith("-----", line))
    errAbort("%s is not an mmBarge file", fileName);

while (lineFileRow(lf, row))
    {
    char *offset = row[0];
    if (startsWith("-----", offset))
        continue;
    if (hashLookup(hash, row[1]))
        warn("%s duplicated in %s, ignoring all but first", row[0], fileName);
    else
        {
	AllocVar(cp);
	hashAddSaveName(hash, row[1], cp, &cp->name);
	if (offset[0] == '(')
	    ++offset;
	if (!isdigit(offset[0]))
	    errAbort("Expecting number first field of line %d of %s", 
	    	lf->lineIx, lf->fileName);
	cp->pos = atoi(offset);
	slAddHead(&cpList, cp);
	}
    }
lineFileClose(&lf);
slReverse(&cpList);
*retList = cpList;
*retHash = hash;
}

void readDat(char *fileName, struct clonePos **retList, struct hash **retHash, 
   int column)
/* Read in one of Terry Furey's dat files. */ 
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
struct clonePos *cpList = NULL, *cp;
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    AllocVar(cp);
    hashAddSaveName(hash, row[2], cp, &cp->name);
    cp->pos = lineFileNeedNum(lf, row, column);
    slAddHead(&cpList, cp);
    }
lineFileClose(&lf);
slReverse(&cpList);
*retList = cpList;
*retHash = hash;
}


void loadMap(char *fileName, struct clonePos **retList, struct hash **retHash)
/* Figure out file type and load it. */
{
char file[128], ext[64];
splitPath(fileName, NULL, file, ext);
if (startsWith("info", file))
    return readInfo(fileName, retList, retHash);
else if (startsWith("mmBarge", file))
    return readMmBarge(fileName, retList, retHash);
else if (startsWith("gold", file) || sameString(".agp", ext))
    return readGold(fileName, retList, retHash);
else if (sameString(".gl", ext) || sameString("gl", file))
    return readGl(fileName, retList, retHash);
else
    errAbort("Unrecognized file type %s%s", file, ext);
}

void loadMaps(char *mapA, char *mapB, struct clonePos **retListA, 
    struct clonePos **retListB, struct hash **retHashA, struct hash **retHashB)
/* Load some maps. */
{
char file[128], ext[64];
splitPath(mapA, NULL, file, ext);
if (sameString(".dat", ext))
    {
    readDat(mapA, retListA, retHashA, 0);
    readDat(mapA, retListB, retHashB, 1);
    }
else
    {
    loadMap(mapA, retListA, retHashA);
    loadMap(mapB, retListB, retHashB);
    }
}

void posSpan(struct clonePos *cpList, int *retStart, int *retEnd)
/* Figure out min/max span of clone list. */
{
struct clonePos *cp;
int start, end;
start = end = cpList->pos;
for (cp = cpList->next; cp != NULL; cp = cp->next)
    {
    if (start > cp->pos) start = cp->pos;
    if (end < cp->pos) end = cp->pos;
    }
*retStart = start;
*retEnd = end;
}

void mapZoomIn(int x, int y, int width, int height, double zx, double zy, double zScale)
/* Print out image map rectangle that calls self with zoom info. */
{
printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/wikiPlot?contigDir=%s&mapX=%s&mapY=%s&pix=%d&xOff=%f&yOff=%f&zoom=%f\" ",
	cgiEncode(contigDir), cgiEncode(mapX), cgiEncode(mapY), pix, zx, zy, zScale);
printf(">\n");
}

void plot(struct memGfx *mg, int x, int y, char *name, Color color)
/* Plot a point. */
{
// y = mg->height - 1 - y;
if (plotName)
    {
    mgTextCentered(mg, x-1, y-1, 3, 3, color, font, name);
    }
else
    mgDrawBox(mg, x-1, y-1, 3, 3, color);
}

void makePlot(struct clonePos *xList, struct clonePos *yList, struct hash *yHash)
/* Write out graphics for plot. */
{
struct memGfx *mg = NULL;
struct tempName gifTn;
char *mapName = "map";
struct clonePos *xp, *yp;
int i, j, x, y, nextX, nextY;
int divisions = 10;
double invZoom = 1.0/zoom;
double magnify = 2.0;
double newZoom = zoom*magnify;
double invNewZoom = 1.0/newZoom;
int xCount = slCount(xList);
plotName = (xCount/zoom < 50);

if (xList == NULL || yList == NULL)
    return;
font = mgSmallFont();
posSpan(xList, &xStart, &xEnd);
posSpan(yList, &yStart, &yEnd);
if (pix < 50 || pix > 5000)
    errAbort("Pixels out of range - must be between 50 an 5000");
mg = mgNew(pix, pix);
mgClearPixels(mg);

/* Plot dots. */
for (xp = xList; xp != NULL; xp = xp->next)
    {
    if ((yp = hashFindVal(yHash, xp->name)) != NULL)
        {
	zoomScale(xp->pos, yp->pos, &x, &y);
	plot(mg, x, y, xp->name, MG_BLACK);
	}
    }


/* Make zooming image map. */
printf("<MAP Name=%s>\n", mapName);
for (i=0; i<divisions; ++i)
    {
    double cenX = xOff + (i + 0.5) * (invZoom / divisions);
    double sx = cenX - invNewZoom/2;
    x = i*pix/divisions;
    nextX = (i+1)*pix/divisions;
    for (j=0; j<divisions; ++j)
        {
	double cenY = yOff + (j + 0.5) * (invZoom / divisions);
	double sy = cenY - invNewZoom/2;
	y = j*pix/divisions;
	nextY = (j+1)*pix/divisions;
	mapZoomIn(x, y, nextX - x, nextY - y, sx, sy, zoom*magnify);
	}
    }
printf("</MAP>\n");

/* Save image in temp dir. */
makeTempName(&gifTn, "wikPic", ".gif");
mgSaveGif(mg, gifTn.forCgi, FALSE);
printf(
    "<P><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s><BR>\n",
    gifTn.forHtml, pix, pix, mapName);
mgFree(&mg);

/* Print some extra info. */
printf("X has %d elements ranging from %d to %d<BR>\n", slCount(xList), xStart, xEnd);
printf("Y has %d elements ranging from %d to %d<BR>\n", slCount(yList), yStart, yEnd);
}

void printBox()
/* Make little navigation box. */
{
puts(
"<TABLE BORDER=\"0\" WIDTH=\"100%\">\n"
"  <TR>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxUpLeft\" VALUE=\" \\ \"></TD>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxUp\" VALUE=\" ^ \"></TD>\n"
"    <TD WIDTH=\"34%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxUpRight\" VALUE=\" / \"></TD>\n"
"  </TR>\n"
"  <TR>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxLeft\" VALUE=\" < \"></TD>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxOut\" VALUE=\" Z \"></TD>\n"
"    <TD WIDTH=\"34%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxRight\" VALUE=\" > \"></TD>\n"
"  </TR>\n"
"  <TR>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxDownLeft\" VALUE=\" / \"></TD>\n"
"    <TD WIDTH=\"33%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxDown\" VALUE=\" v \"></TD>\n"
"    <TD WIDTH=\"34%\"><INPUT TYPE=\"SUBMIT\" NAME=\"boxDownRight\" VALUE=\" \\ \"></TD>\n"
"  </TR>\n"
"  <TR>\n"
"  </TR>\n"
"</TABLE>\n"
"\n");
cgiMakeButton("unzoom", "unzoom");
}


void wikiPlot()
/* wikiPlot - Quick plots of maps vs. each other. */
{
boolean gotDir = cgiVarExists("contigDir");
double step;

contigDir = cgiUsualString("contigDir", contigDir);
mapX = cgiUsualString("mapX", mapX);
mapY = cgiUsualString("mapY", mapY);
pix = cgiUsualInt("pix", pix);
xOff = cgiUsualDouble("xOff", xOff);
yOff = cgiUsualDouble("yOff", yOff);
zoom = cgiUsualDouble("zoom", zoom);
step = 0.1 * 1/zoom;

if (cgiVarExists("boxOut"))
    {
    double invZoom = 1.0/zoom;
    double xCen = xOff + invZoom*0.5;
    double yCen = yOff + invZoom*0.5;
    zoom /= 2;
    invZoom = 1.0/zoom;
    xOff = xCen - invZoom*0.5;
    yOff = yCen - invZoom*0.5;
    }
else if (cgiVarExists("boxUp"))
    yOff -= step;
else if (cgiVarExists("boxDown"))
    yOff += step;
else if (cgiVarExists("boxLeft"))
    xOff -= step;
else if (cgiVarExists("boxRight"))
    xOff += step;
else if (cgiVarExists("boxUpLeft"))
    {
    yOff -= step;
    xOff -= step;
    }
else if (cgiVarExists("boxUpRight"))
    {
    yOff -= step;
    xOff += step;
    }
else if (cgiVarExists("boxDownLeft"))
    {
    yOff += step;
    xOff -= step;
    }
else if (cgiVarExists("boxDownRight"))
    {
    yOff += step;
    xOff += step;
    }
else if (cgiVarExists("unzoom"))
    {
    xOff = yOff = -0.05;
    zoom = 0.9;
    }

printf("<FORM ACTION=\"../cgi-bin/wikiPlot\" METHOD=\"GET\">\n");
printf("<TABLE BORDER=0 WIDTH=\"100%%\">\n");
printf("<TR>\n");
printf("<TD WIDTH=\"78%%\">\n");
printf("<B>Wiki Plotter</B><BR>\n");
printf("<B>Contig: </B>");
cgiMakeTextVar("contigDir", contigDir, 0);
if (gotDir)
    {
    cgiMakeButton("refresh", "refresh");
    }
else
    cgiMakeButton("submit", "submit");
printf("<BR>\n");
printf("<B>Map X: </B>");
cgiMakeTextVar("mapX", mapX, 12);
printf("<B>Map Y: </B>");
cgiMakeTextVar("mapY", mapY, 12);
printf("<B>Pixels: </B>");
cgiMakeIntVar("pix", pix, 4);
printf("</TD>\n");
printf("<TD WIDTH=\"22%%\">\n");
if (gotDir)
    printBox();
printf("</TD>\n");
printf("</TR>\n");
printf("</TABLE>\n");

if (gotDir)
    {
    char xFile[512], yFile[512];
    struct hash *xHash = NULL, *yHash = NULL;
    struct clonePos *xList = NULL, *yList = NULL;
    sprintf(xFile, "%s/%s", contigDir, mapX);
    sprintf(yFile, "%s/%s", contigDir, mapY);
    loadMaps(xFile, yFile, &xList, &yList, &xHash, &yHash);
    makePlot(xList, yList, yHash);
    }

/* Save hidden vars. */
    {
    char buf[256];
    sprintf(buf, "%f", zoom);
    cgiMakeHiddenVar("zoom", buf);
    sprintf(buf, "%f", xOff);
    cgiMakeHiddenVar("xOff", buf);
    sprintf(buf, "%f", yOff);
    cgiMakeHiddenVar("yOff", buf);
    }
printf("</FORM>\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmShell("wikiPlot", wikiPlot, NULL);
return 0;
}
