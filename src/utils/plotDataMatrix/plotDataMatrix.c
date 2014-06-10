/* plotDataMatrix - Create an image showing a matrix of data values.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "vGfx.h"
#include "memgfx.h"

struct vGfx *vgOpenGif(int width, int height, char *fileName, boolean useTransparency);


int cellWidth = 25;
int cellHeight = 25;

boolean clCorrelate = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "plotDataMatrix - Create an image showing a matrix of data values.\n"
  "usage:\n"
  "   plotDataMatrix inPairs.tab xLabels.tab yLabels.tab output.gif\n"
  "where the three inputs are all tab-separated text files:\n"
  "   inPairs.tab has 3 columns:  xId, yId, and a floating point number\n"
  "   xLabels.tab has 2 columns: xId and xLabel\n"
  "   yLabels.tab has 2 column: yId and yLabel\n"
  "The label files also determine the order (top to bottom, right to left)\n"
  "of the display.\n"
  "options:\n"
  "   -correlate Treat as a correlation matrix.  Fill in the diagonal with\n"
  "              perfect score and reflect other squares\n"
  );
}

static struct optionSpec options[] = {
   {"correlate", OPTION_BOOLEAN},
   {NULL, 0},
};

struct sparseCell
/* Two id's and a value - a representation of a sparse matrix cell. */
    {
    struct sparseCell *next;
    char *xId;		/* ID for columns.  Allocated in matrix hash.  */
    char *yId;		/* ID for rows.  Allocated in matrix hash.  */
    double val;		/* Value at this position in matrix. */
    };

struct sparseMatrix
/* A matrix stored as a list of cells with values. */
    {
    struct sparseMatrix *next;
    struct sparseCell *cellList;	/* List of all cells */
    struct hash *xIdHash;	/* Hash of column IDs. */
    struct hash *yIdHash;	/* Hash of row IDs. */
    };

struct sparseMatrix *sparseMatrixRead(char *fileName)
/* Read in a sparse matrix file and return it in memory structure. */
{
/* Open up file */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

/* Start up matrix data structure. */
struct sparseMatrix *mat;
AllocVar(mat);
mat->xIdHash = hashNew(0);
mat->yIdHash = hashNew(0);

/* Loop through file adding cells. */
while (lineFileRow(lf, row))
    {
    struct sparseCell *cell;
    AllocVar(cell);
    cell->xId = hashStoreName(mat->xIdHash, row[0]);
    cell->yId = hashStoreName(mat->yIdHash, row[1]);
    cell->val = sqlDouble(row[2]);
    slAddHead(&mat->cellList, cell);
    }
slReverse(&mat->cellList);

/* Clean up and go home. */
lineFileClose(&lf);
return mat;
}

struct sparseCell *sparseMatrixFindCell(struct sparseMatrix *mat, char *xId, char *yId)
/* Rummage through cell list looking for match. */
{
struct sparseCell *cell;
for (cell = mat->cellList; cell != NULL; cell = cell->next)
    {
    if (sameString(cell->xId, xId) && sameString(cell->yId, yId))
        return cell;
    }
return NULL;
}

struct slPair *labelsRead(char *fileName, struct hash *idHash, char *inPairsFile)
/* Read in file into a list of pairs where name is id, val is label. */
{
/* Open up file */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];

/* Loop through file adding pairs to list. */
struct slPair *list = NULL;
while (lineFileRowTab(lf, row))
    {
    char *id = row[0];
    char *label = row[1];
    if (!hashLookup(idHash, id))
        verbose(2, "id %s is in line %d of %s, but not in %s", id, 
		lf->lineIx, lf->fileName, inPairsFile);
    struct slPair *el = slPairNew(id, cloneString(label));
    slAddHead(&list, el);
    }
slReverse(&list);

lineFileClose(&lf);
return list;
}

int widestLabelWidth(MgFont *font, struct slPair *labelList)
/* Return width in pixels of widest label in list. */
{
int widest = 0;
struct slPair *label;
for (label = labelList; label != NULL; label = label->next)
    {
    int width = mgFontStringWidth(font, label->val);
    if (widest < width)
        widest = width;
    }
return widest;
}

void makeImage(struct sparseMatrix *mat, struct slPair *xLabelList, struct slPair *yLabelList, char *fileName)
/* Make image file from matrix and labels. */
{
/* Figure out dimensions */
int xCount = slCount(xLabelList);
int yCount = slCount(yLabelList);
MgFont *font = mgTimes12Font();
int spaceWidth = mgFontCharWidth(font, ' ');
int lineHeight = mgFontLineHeight(font);
int labelWidth = 2*spaceWidth + widestLabelWidth(font, yLabelList);
int totalWidth = labelWidth + cellWidth * xCount;
int totalHeight = lineHeight + cellHeight * yCount;
int matrixX = labelWidth;
int matrixY = lineHeight;

/* Start up image. */
struct vGfx *vg = vgOpenGif(totalWidth, totalHeight, fileName, FALSE);

/* Print row labels */
struct slPair *yLabel;
int y = matrixY;
for (yLabel = yLabelList; yLabel != NULL; yLabel = yLabel->next)
    {
    vgTextCentered(vg, 0, y, labelWidth, cellHeight, MG_BLACK, font, yLabel->val);
    y += cellHeight;
    }

/* Print x labels */
struct slPair *xLabel;
int x = matrixX;
for (xLabel = xLabelList; xLabel != NULL; xLabel = xLabel->next)
    {
    vgTextCentered(vg, x, 0, cellWidth, lineHeight, MG_BLACK, font, xLabel->val);
    x += cellWidth;
    }

/* Print matrix. */
y = matrixY;
for (yLabel = yLabelList; yLabel != NULL; yLabel = yLabel->next)
    {
    x = matrixX;
    for (xLabel = xLabelList; xLabel != NULL; xLabel = xLabel->next)
        {
	boolean gotData = FALSE;
	struct sparseCell *cell = sparseMatrixFindCell(mat, xLabel->name, yLabel->name);
	if (cell == NULL && clCorrelate)
	    cell = sparseMatrixFindCell(mat, yLabel->name, xLabel->name);
	int grayLevel;
	if (cell != NULL)
	    {
	    grayLevel = 255 - 255 * cell->val;
	    gotData = TRUE;
	    }
	else	/* No data - maybe try and fake it? */
	    {
	    if (clCorrelate)  
	        {
		if (sameString(xLabel->name, yLabel->name))
		    {
		    grayLevel = 0;
		    gotData = TRUE;
		    }
		}
	    }
	Color color;
	if (gotData)
	    color = vgFindColorIx(vg, grayLevel, grayLevel, grayLevel);
	else
	    color = MG_YELLOW;
	vgBox(vg, x, y, cellWidth, cellHeight, color);
	x += cellWidth;
	}
    y += cellHeight;
    }

/* Clean up */
vgClose(&vg);
}

void plotDataMatrix(char *inPairsFile, char *xLabelsFile, char *yLabelsFile, char *outFile)
/* plotDataMatrix - Create an image showing a matrix of data values.. */
{
struct sparseMatrix *mat = sparseMatrixRead(inPairsFile);
struct slPair *xLabelList = labelsRead(xLabelsFile, mat->xIdHash, inPairsFile);
struct slPair *yLabelList = labelsRead(yLabelsFile, mat->yIdHash, inPairsFile);
verbose(2, "got %d cells, %d x dims, %d y dims\n", slCount(mat->cellList), slCount(xLabelList), slCount(yLabelList));
makeImage(mat, xLabelList, yLabelList, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
clCorrelate = optionExists("correlate");
plotDataMatrix(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
