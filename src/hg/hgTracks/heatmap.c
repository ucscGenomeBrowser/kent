#include "common.h"
#include "hgTracks.h"
#include "bigBed.h"
#include "spaceSaver.h" // going to need this eventually to pack them?  Or not?  Can maybe remove?
#include "mouseOver.h"  // probably want to handle custom mouseover
#include "vGfx.h"
#include "htmlColor.h"

#include "heatmap.h"

#define HEATMAP_COLOR_STEPS 20

#define BG_COLOR_TDB_SETTING "heatmap.background"
#define LOW_COLOR_TDB_SETTING "heatmap.lowColor"
#define MID_COLOR_TDB_SETTING "heatmap.midColor"
#define HIGH_COLOR_TDB_SETTING "heatmap.highColor"

#define LOW_SCORE_TDB_SETTING "heatmap.lowScore"
#define MID_SCORE_TDB_SETTING "heatmap.midScore"
#define HIGH_SCORE_TDB_SETTING "heatmap.highScore"


struct heatmap *heatmapNew(int rows, int cols)
/* Allocate a new heatmap structure and return it */
{
struct heatmap *h = NULL;
AllocVar(h);
h->rows = rows;
h->cols = cols;
AllocArray(h->rowLabels, rows);
AllocArray(h->scores, rows*cols);
AllocArray(h->special, rows*cols);
AllocArray(h->itemLabels, rows*cols);
AllocArray(h->isNA, rows*cols);
AllocArray(h->rowStarts, rows);
AllocArray(h->rowSizes, rows);
AllocArray(h->colStarts, cols);
AllocArray(h->colSizes, cols);
return h;
}


void heatmapSetDrawSizes(struct heatmap *h, double rowHeight, double colWidth)
/* Update the expected cell height and width in pixels for the heatmap */
{
h->rowHeight = rowHeight;
h->colWidth = colWidth;
}

int rowColToIndex(struct heatmap *h, int row, int col)
/* Scores and labels are actually stored in 1-D arrays, so we need to convert a set
 * of row/column indices into a 1-D index for those arrays.  The storage format is
 * in row-major order.
 */
{
// NB: column count here is the number of exons; rows is the height (literally a number of
// rows in the browser display).
if (row >= h->rows || col >= h->cols)
    errAbort("Out of range indices row %d, col %d in heatmap (max %d, %d)", row, col, h->rows, h->cols);
return row*h->cols + col;
}

void setCell(struct heatmap *h, int row, int col, double score, boolean isNA)
/* Presently unused, might just get deleted */
{
int index = rowColToIndex(h, row, col);
if (isNA)
    h->isNA[index] = TRUE;
else
    {
    char buffer[256];
    safef(buffer, sizeof(buffer), "%lf", score);
    h->scores[index] = cloneString(buffer);
    h->isNA[index] = FALSE;
    }
}

char* getItemLabel(struct heatmap *h, int row, int col)
/* Returns the mouseover label for a cell */
{
int index = rowColToIndex(h, row, col);
return h->itemLabels[index];
}

char* getScore(struct heatmap *h, int row, int col)
{
int index = rowColToIndex(h, row, col);
return h->scores[index];
}


boolean getNA(struct heatmap *h, int row, int col)
/* Return whether a particular cell is NA */
{
int index = rowColToIndex(h, row, col);
return h->isNA[index];
}


void heatmapAssignRowLabels(struct heatmap *h, char **rowLabels)
/* Planned abstraction for assigning row labels in a heatmap */
{
}

void heatmapAssignColumnLabels(struct heatmap *h, char **rowLabels)
/* Planned abstraction for assigning column labels in a heatmap */
{
}


struct heatmap *heatmapFromLF(struct track *tg, struct linkedFeatures *lf)
/* This takes a linkedFeatures structure from a bigBed and builds a heatmap out
 * of it.  In the current setup, the heatmap values and labels are each stored
 * in a 1-D comma-separated matrix that we convert to a 2-D matrix using
 * row-major ordering.
 * The column count is the number of exons in the LF structure; the number of
 * rows is in an extra field.  The full type definition is in hg/lib/heatmap.as.
 */
{
struct bigBedInterval *bbi = lf->original;
if (bbi == NULL)
    {
    warn("unable to make heatmap from lf - no original bbi");
    return NULL;
    }

#define HEATMAP_COUNT 17
char *bedRow[HEATMAP_COUNT]; // hard-coded for now, should change this to accommodate extra .as fields
char startBuf[16], endBuf[16];
bigBedIntervalToRow(bbi, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

// Use this to figure out which field to pull.  For now, we're just doing 12, 13, 14, 15
// (right after the exons)
// 9: exonCount, 10: sizes, 11: starts, 12: rowcount, 13: rowlabels, 14: scores, 15: itemLabels
// 16: link to maveDb

struct heatmap *h = NULL;

int rowCount = atoi(bedRow[12]);
int colCount = atoi(bedRow[9]);

h = heatmapNew(rowCount, colCount);

// Get the row labels, which will be drawn off to the side of the heatmap
int labelsFound = chopByCharRespectDoubleQuotesKeepEmpty(cloneString(bedRow[13]), ',', h->rowLabels, h->rows);
if (labelsFound != rowCount)
    {
    errAbort("Field containing heatmap row labels had %d values, but %d were expected", labelsFound, rowCount);
    }

// Read in the scores
char *tmpScore[h->rows*h->cols];
int scoresFound = chopCommas(cloneString(bedRow[14]), tmpScore);
if (scoresFound != h->rows*h->cols)
    {
    errAbort("Field containing heatmap scores had %d values, but %d were expected", scoresFound, h->rows*h->cols);
    }

int i;
double max=atof(tmpScore[0]), min=atof(tmpScore[0]);
for (i=0; i<scoresFound; i++)
    {
    if (isNotEmpty(tmpScore[i]))
        {
        // Check if the "score" string is actually an HTML color code.  This allows users to override
        // and set specific cell colors that aren't part of the score spectrum.
        if (tmpScore[i][0] == '#')
        {
            if (strlen(tmpScore[i]) >= 7)
            {
                char buffer[8];
                safencpy(buffer, sizeof(buffer), tmpScore[i], 7);
                if (htmlColorForCode(buffer, NULL))
                    {
                    // It's a valid HTML color code - #NNNNNN
                    h->scores[i] = cloneString(buffer);
                    }
                else
                    {
                    warn("Invalid heatmap score: %s in track %s\n", tmpScore[i], tg->track);
                    }
            }
            else
                warn("Invalid heatmap score: %s in track %s\n", tmpScore[i], tg->track);
        }
        else
        {
            // The standard case - this score is assumed to be a floating-point number
            h->scores[i] = cloneString(tmpScore[i]);
            double value = atof(tmpScore[i]);
            h->isNA[i] = FALSE;
            if (value > max)
                max = value;
            if (value < min)
                min = value;
        }
        char *c = NULL;
        // Check for a | followed by a character.  That character will be displayed in the cell.
        // Usually this is to mark some weirdness, like multiple scores for that cell in the
        // data source.
        if ((c = strchr(tmpScore[i], '|')) != NULL)
            {
            c++;
            if (*c != 0 && isascii(*c))
                h->special[i] = *c;
            }
        }
    else
        h->isNA[i] = TRUE;
    }

// These are the mouseover texts for each cell of the heatmap
// Note that we're re-using tmpScore for convenience!
int itemLabelsFound = chopByCharRespectDoubleQuotesKeepEmpty(cloneString(bedRow[15]), ',', tmpScore, h->rows*h->cols);
if (itemLabelsFound != h->rows*h->cols)
    {
    errAbort("Field containing heatmap mouseover labels had %d values, but %d were expected", itemLabelsFound, h->rows*h->cols);
    }

for (i=0; i<itemLabelsFound; i++)
    {
    if (isNotEmpty(tmpScore[i]))
        {
        h->itemLabels[i] = cloneString(tmpScore[i]);
        }
    // else, the itemLabels[i] pointer was already zeroed by AllocArray
    }

// Keep track of the min, max, and midpoint of the scores we've read
h->highCap = max;
h->lowCap = min;
h->midPoint = (max+min)/2.0;

// The label for this heatmap
h->label = cloneString(bedRow[3]);

return h;
}



int heatmapTotalHeight(struct heatmap *h)
/*
 * Need to be able to say how high a heatmap is, including optional column labels
 * I think.  We'll see if and where we want to invoke this.  This is probably
 * dead code and can be removed.
 */
{
// for now, just draw the raw heatmap
// final row starts at int(rows*rowHeight).  each row ends one pixel before the next,
// but we'll need a sanity check for very zoomed out situations (a row might have a
// "height" of less than 1 pixel, and we can't end before we start).
// so max(int( (rows+1)*rowHeight)-1, int(rows*rowHeight))
int lastStart = (int)(h->rows * h->rowHeight);
int lastEnd = (int)( (h->rows+1) * h->rowHeight) - 1;
if (lastEnd > lastStart)
    return lastEnd;
else
    return lastStart;
}

int heatmapItemRowCount(struct track *tg, void *item)
/* This returns the number of rows in a spaceSaver structure that this heatmap will
 * occupy.  This is expected to be the number of rows in the heatmap plus 1.  The
 * extra row is for the item label for the heatmap, since the space to the left is
 * already occupied by the row labels.
 */
{
// The problem here is that I want to access the heatmap structure for an item, but to
// do so either the item has to include a link to the heatmap, be the heatmap, or else
// I have to dynamically generate it just to get a row count.
// We're doing that last right now, but because we're not freeing or storing it for later use,
// it's quite wasteful.  Still seems to be pretty fast though.
struct linkedFeatures *lf = (struct linkedFeatures *) item;
struct heatmap *h = heatmapFromLF(tg, lf);
if (h)
    return h->rows+1;  // +1 to allow for a title row.  column labels would be a further complication

// something went wrong.  This should maybe be an errAbort or a warn instead.
return 0;
}

int heatmapTotalWidth(struct heatmap *h)
// Need to be able to say how wide a heatmap is for packing, including optional row labels
// NB: this goes to hell if we do row labels to the left of the track image, so 
// have fun working that out.
//
// This is likely dead code - we just let spaceSaver pack the item using its normal width,
// with the caveat that we extend the left label width for packing if any of the row labels
// are longer than the item label.
{
int lastCol = h->cols-1;
int extent = h->colStarts[lastCol] + h->colSizes[lastCol] - h->colStarts[0];

return extent;

/*
int i, maxLabelWidth = 0;
for (i=0; i<h->rows; i++)
    {
    if (h->rowLabels[i] != NULL)
        {
        // this isn't quite right, but something like this below

        if (!tg->drawLabelInBox && !tg->drawName && withLabels && (!noLabel))
            // add item label
            {
            leftLabelSize = mgFontStringWidth(font, tg->itemName(tg, item)) + extraWidth;
            if (start - leftLabelSize + winOffset < 0)
                leftLabelSize = start + winOffset;
            start -= leftLabelSize;
            }
        }
    }
 */

/*
int lastStart = (int)(h->cols * h->colWidth);
int lastEnd = (int)( (h->cols+1) * h->colWidth) - 1;
if (lastEnd > lastStart)
    return lastEnd;
else
    return lastStart;
*/
}


void heatmapDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg,
                         int x, int y, double scale, MgFont *font,
                         Color color, enum trackVisibility vis)
/* Draw the cells for a single heatmap; this is intended to be a drop-in replacement
 * for linkedFeaturesDrawAt.  This doesn't pay attention to visibility because it is
 * only used as a replacement for linkedFeaturesDrawAt if we're in squish/pack/full.
 */
{
struct linkedFeatures *lf = (struct linkedFeatures *) item;

struct heatmap *h = heatmapFromLF(tg, lf);

/* Set the height and width of heatmap cells in pixels */
heatmapSetDrawSizes(h, tg->heightPer, (lf->end - lf->start)*scale/2.0); // 2.0 for now b/c hard-coded 2 columns

// Retrieve the colors that will be used in the heatmap
char *lowColorString = trackDbSettingClosestToHomeOrDefault(tg->tdb, LOW_COLOR_TDB_SETTING, "green");
char *midColorString = trackDbSettingClosestToHomeOrDefault(tg->tdb, MID_COLOR_TDB_SETTING, "black");
char *highColorString = trackDbSettingClosestToHomeOrDefault(tg->tdb, HIGH_COLOR_TDB_SETTING, "red");
struct rgbColor lowColor = bedColorToRgb(bedParseColor(lowColorString));
struct rgbColor midColor = bedColorToRgb(bedParseColor(midColorString));
struct rgbColor highColor = bedColorToRgb(bedParseColor(highColorString));

// Get heatmap value bounds, maybe from trackDb
double low = trackDbFloatSettingOrDefault(tg->tdb, LOW_SCORE_TDB_SETTING, h->lowCap);
double mid = trackDbFloatSettingOrDefault(tg->tdb, MID_SCORE_TDB_SETTING, 0);
double high = trackDbFloatSettingOrDefault(tg->tdb, HIGH_SCORE_TDB_SETTING, h->highCap);

heatmapMakeSpectrum(h, hvg, low, &lowColor, mid, &midColor, high, &highColor);

struct simpleFeature *exon = NULL;
int i = 0;

// Loop over the heatmap columns and set up the coordinates that each cell starts at
for (exon = lf->components; exon != NULL; exon = exon->next)
    {
    int r = (h->cols-1) - i;  // quick hack - exons are stored in reverse order!
    h->colStarts[r] = round((double)((int)exon->start - winStart)*scale) + x;
    h->colSizes[r] = round((double)((int)exon->end - exon->start)*scale);
    i++;
    }

// If a background color is specified for the heatmap, lay that down first.  Otherwise cells
// will be on a transparent background (with the thin vertical blue lines, etc.).
char *bgColorString = trackDbSettingClosestToHomeOrDefault(tg->tdb, BG_COLOR_TDB_SETTING, "");
if (isNotEmpty(bgColorString))
    {
    Color bgColor = bedColorToGfxColor(bedParseColor(bgColorString));

    int bigBoxxOffset = (int)(h->colStarts[h->cols-1]);
    if (h->colSizes[h->cols-1] > 1)
        bigBoxxOffset += h->colSizes[h->cols-1];
    else
        bigBoxxOffset += 1;
    int bigBoxyOffset = y + (int)(h->rowHeight * (h->rows + 1));
    hvGfxBox(hvg, h->colStarts[0], y + h->rowHeight,  // + rowHeight so the enclosing box doesn't include the track label
        bigBoxxOffset- h->colStarts[0], bigBoxyOffset - (y + h->rowHeight), bgColor);
    }

// Now loop over the cells and draw them
int row_i, column_j;
for (row_i = 0; row_i < h->rows; row_i++)
    {
    for (column_j = 0; column_j < h->cols; column_j++)
        {
        if (getNA(h, row_i, column_j))
            continue; // We draw nothing if there's no value here

        // At this point, we know it's not NA, so there must be something to display
        Color color = MG_BLACK;
        char *scoreStr = getScore(h, row_i, column_j);
        if (scoreStr[0] == '#')
        {
            htmlColorForCode(scoreStr, &color);
            color = bedColorToGfxColor(color);
        }
        else
        {
            double score = atof(scoreStr);

            if (score > h->midPoint)
                {
                double cappedScore = score > h->highCap ? h->highCap : score;
                double fraction = 0;
                if (h->highCap > h->midPoint)
                    fraction = (cappedScore - h->midPoint)/(h->highCap - h->midPoint);
                int index = (int) (HEATMAP_COLOR_STEPS-1)*fraction;
                color = h->highSpectrum[index];
                }
            else
                {
                double cappedScore = score < h->lowCap ? h->lowCap : score;
                double fraction = 0;
                if (h->midPoint > h->lowCap)
                    fraction = (h->midPoint - cappedScore)/(h->midPoint - h->lowCap);
                int index = (int) (HEATMAP_COLOR_STEPS-1)*fraction;
                color = h->lowSpectrum[index];
                }
            }

        // Now we have the color for the cell.  Time to set up the coordinates to draw it at.

        int yOffset = (int)((row_i+1) * h->rowHeight);  // +1 to allow for the heatmap label
        int yEnd = (int)((row_i+1+1)*h->rowHeight)-1;   // +1 to allow for the heatmap label
        if (yEnd < yOffset)
            yEnd = yOffset;
        int height = yEnd - yOffset;
        int xOffset = (int)(h->colStarts[column_j]);

        int width = h->colSizes[column_j];

        // We set width to 1 if it's less than 1.  Maybe a bad idea, but it will ensure SOMETHING
        // is visible when you're zoomed out.
        if (width < 1)
            width = 1;
        hvGfxBox(hvg, xOffset, y + yOffset, width, height, color);

        // Now draw the special character on top if there's space for it
        int specialIx = rowColToIndex(h, row_i, column_j);
        if (h->special[specialIx] != 0)
            {
                char text[2];
                safef(text, sizeof(text), "%c", h->special[specialIx]);
 
                if (width >= vgGetFontStringWidth(hvg->vg, font, text) ) // &&
                    //height >= vgGetFontPixelHeight(hvg->vg, font))
                    // commented out because row height is always fontheight?  Unless it's not?
                    // Squish mode ... Ideally we'd resize the font for that; we'll see.
                    hvGfxTextCentered(hvg, xOffset, y + yOffset, width, height,
                                    MG_WHITE, font, text);
                // TODO - replace MG_WHITE with that function that generates a contrasting
                // text color based on the background at that position
            }
        }
    }

    // Put a black outline around the periphery of the heatmap for clarity
    int xOffset = (int)(h->colStarts[h->cols-1]);
    if (h->colSizes[h->cols-1] > 1)
        xOffset += h->colSizes[h->cols-1];
    else
        xOffset += 1;
    int yOffset = y + (int)(h->rowHeight * (h->rows + 1));
    hvGfxOutlinedBox(hvg, h->colStarts[0], y + h->rowHeight,  // + rowHeight to avoid enclosing the track label
        xOffset- h->colStarts[0], yOffset - (y + h->rowHeight), hvGfxFindAlphaColorIx(hvg, 0,0,0,0), MG_BLACK);
}


void heatmapSetBounds(struct heatmap *h, double lowScore, struct rgbColor *lowColor,
        double midScore, struct rgbColor *midColor, double highScore, struct rgbColor *highColor)
/* Also dead code, and may well be removed soon */
{
if (lowScore > midScore || midScore > highScore)
    errAbort("attempting to make heatmap spectrum with invalid scores (low %lf, mid %lf, high %lf)",
            lowScore, midScore, highScore);

}

void heatmapMakeSpectrum(struct heatmap *h, struct hvGfx *hvg, double lowScore, struct rgbColor *lowColor,
        double midPoint, struct rgbColor *midColor, double highScore, struct rgbColor *highColor)
/* Take high, low, and midpoints and associated colors, and create two spectrums (one for low scores, one for high)
 * that can be used to shade cells in the heatmap.
 */
{
if (h->lowSpectrum)
    freeMem(h->lowSpectrum);
AllocArray(h->lowSpectrum, HEATMAP_COLOR_STEPS);
if (h->highSpectrum)
    freeMem(h->highSpectrum);
AllocArray(h->highSpectrum, HEATMAP_COLOR_STEPS);

hvGfxMakeColorGradient(hvg, midColor, lowColor, HEATMAP_COLOR_STEPS, h->lowSpectrum);
hvGfxMakeColorGradient(hvg, midColor, highColor, HEATMAP_COLOR_STEPS, h->highSpectrum);
}


static boolean isTooLightForTextOnWhite(struct hvGfx *hvg, Color color)
/* Return TRUE if text in this color would probably be invisible on a white background. */
/* Speculatively included as part of thoughts on adapting in-cell labels to be visible,
 * but this probably isn't the function we'll use */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
int colorTotal = rgbColor.r + 2*rgbColor.g + rgbColor.b;
return colorTotal >= 512;
}

void heatmapDrawItemLabel(struct track *tg, struct spaceNode *sn,
                          struct hvGfx *hvg, int xOff, int y, int width,
                          MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                          double scale, boolean withLeftLabels)
/* Draw the labels for a heatmap.  This includes the full heatmap label as well as the labels
 * for each row (and potentially column, though we're not doing column labels right now)
 * This is cribbed and adapted from the more generic DrawItemLabel function, and could stand to
 * be cleaned up.
 */
{
struct slList *item = sn->val;
int s = tg->itemStart(tg, item);
int sClp = (s < winStart) ? winStart : s;
int x1 = round((sClp - winStart)*scale) + xOff;
int textX = x1;

// can't draw item labels inside of a heatmap; withLeftLabels is always true
withLeftLabels = TRUE;

if (tg->itemNameColor != NULL)
    {
    color = tg->itemNameColor(tg, item, hvg);
    labelColor = color;
    if (withLeftLabels && isTooLightForTextOnWhite(hvg, color))
	labelColor = somewhatDarkerColor(hvg, color);
    }

/* pgSnpDrawAt may change withIndividualLabels between items */
boolean withLabels = (withLeftLabels && ((vis == tvPack) || (vis == tvFull && isTypeBedLike(tg))) && (!sn->noLabel) && !tg->drawName);
if (withLabels)
    {
    struct linkedFeatures *lf = (struct linkedFeatures *) item;
    struct heatmap *h = heatmapFromLF(tg, lf);
    heatmapSetDrawSizes(h, tg->heightPer, (lf->end - lf->start)*scale/2.0); // 2.0 for now b/c hard-coded 2 columns

    // heatmap title
    if (isNotEmpty(h->label))
        {
        char *name = h->label;
        textX = x1;
        int nameWidth = mgFontStringWidth(font, name);
        int dotWidth = tl.nWidth/2;
        boolean snapLeft = FALSE;
        boolean drawNameInverted = highlightItem(tg, item);
        textX -= nameWidth + dotWidth;
        snapLeft = (textX < fullInsideX);
        snapLeft |= (vis == tvFull && isTypeBedLike(tg));

        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox == NULL && snapLeft)
        #else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (snapLeft)        /* Snap label to the left. */
        #endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
                {
                textX = leftLabelX;
                assert(hvgSide != NULL);
                int prevX, prevY, prevWidth, prevHeight;
                hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
                hvGfxUnclip(hvgSide);
                hvGfxSetClip(hvgSide, leftLabelX, y, fullInsideX - leftLabelX, tg->height);
                if(drawNameInverted)
                    {
                    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
                    hvGfxBox(hvgSide, boxStart, y, nameWidth+1, tg->heightPer - 1, color);
                    hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                                MG_WHITE, font, name);
                    }
                else
                    hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                                labelColor, font, name);
                hvGfxUnclip(hvgSide);
                hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
                }
            else
                {
                // shift clipping to allow drawing label to left of currentWindow
                int pdfSlop=nameWidth/5;
                int prevX, prevY, prevWidth, prevHeight;
                hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
                hvGfxUnclip(hvg);
                hvGfxSetClip(hvg, textX-1-pdfSlop, y, nameWidth+1+pdfSlop, tg->heightPer);
                if(drawNameInverted)
                    {
                    hvGfxBox(hvg, textX - 1, y, nameWidth+1, tg->heightPer-1, color);
                    hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, MG_WHITE, font, name);
                    }
                else
                    // usual labeling
                    hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, labelColor, font, name);
                hvGfxUnclip(hvg);
                hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
                }
            y += h->rowHeight;
            
        }

    int i = 0;
    for (i=0; i < h->rows; i++)
        {
        char *name = h->rowLabels[i];
        textX = x1;
//        char *name = tg->itemName(tg, item);
        int nameWidth = mgFontStringWidth(font, name);
        int dotWidth = tl.nWidth/2;
        boolean snapLeft = FALSE;
        boolean drawNameInverted = highlightItem(tg, item);
        textX -= nameWidth + dotWidth;
        snapLeft = (textX < fullInsideX);
        snapLeft |= (vis == tvFull && isTypeBedLike(tg));

        /* Special tweak for expRatio in pack mode: force all labels
         * left to prevent only a subset from being placed right: */
        snapLeft |= (startsWith("expRatio", tg->tdb->type));

    #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
        if (theImgBox == NULL && snapLeft)
    #else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
        if (snapLeft)        /* Snap label to the left. */
    #endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
            {
            textX = leftLabelX;
            assert(hvgSide != NULL);
            int prevX, prevY, prevWidth, prevHeight;
            hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
            hvGfxUnclip(hvgSide);
            hvGfxSetClip(hvgSide, leftLabelX, y, fullInsideX - leftLabelX, tg->height);
            if(drawNameInverted)
                {
                int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
                hvGfxBox(hvgSide, boxStart, y, nameWidth+1, tg->heightPer - 1, color);
                hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                            MG_WHITE, font, name);
                }
            else
                hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                            labelColor, font, name);
            hvGfxUnclip(hvgSide);
            hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
            }
        else
            {
            // shift clipping to allow drawing label to left of currentWindow
            int pdfSlop=nameWidth/5;
            int prevX, prevY, prevWidth, prevHeight;
            hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
            hvGfxUnclip(hvg);
            hvGfxSetClip(hvg, textX-1-pdfSlop, y, nameWidth+1+pdfSlop, tg->heightPer);
            if(drawNameInverted)
                {
                hvGfxBox(hvg, textX - 1, y, nameWidth+1, tg->heightPer-1, color);
                hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, MG_WHITE, font, name);
                }
            else
                // usual labeling
                hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, labelColor, font, name);
            hvGfxUnclip(hvg);
            hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
            }
        y += h->rowHeight;
        }
    }
}


void heatmapMapItem(struct track *tg, struct hvGfx *hvg, void *item,
                    char *itemName, char *mapItemName, int start, int end,
                    int x, int y, int width, int height)
/* Draw all the heatmap boxes for a single heatmap, one per occupied cell (i.e. where
 * isNA = FALSE).  Mouseover text is draw from the itemLabels field if populated, otherwise
 * it's just the score value.
 * Duplicates some code from other places about coordinate calculations, but for the moment this
 * is sufficient to get the job done.
 */
{
double scale = (double)currentWindow->insideWidth/(currentWindow->winEnd - currentWindow->winStart);
struct linkedFeatures *lf = (struct linkedFeatures *) item;

// Yet another reallocation of a heatmap structure
struct heatmap *h = heatmapFromLF(tg, lf);

heatmapSetDrawSizes(h, tg->heightPer, (lf->end - lf->start)*scale/2.0); // 2.0 for now b/c hard-coded 2 columns

struct simpleFeature *exon = NULL;
int i = 0;
int sClp = (lf->start < winStart) ? winStart : lf->start;
for (exon = lf->components; exon != NULL; exon = exon->next)
    {
    int r = (h->cols-1) - i;  // quick hack - exons are stored in reverse order!
    h->colStarts[r] = round((double)((int)exon->start - sClp)*scale) + x;
    h->colSizes[r] = round((double)((int)exon->end - exon->start)*scale);
    i++;
    }

int row_i, column_j;
for (row_i = 0; row_i < h->rows; row_i++)
    {
    for (column_j = 0; column_j < h->cols; column_j++)
        {
        char *label = getItemLabel(h, row_i, column_j);
        if (isNotEmpty(label))
            {
            int yOffset = (int)((row_i+1) * h->rowHeight);  // +1 to allow for the heatmap label
            int yEnd = (int)((row_i+1+1)*h->rowHeight)-1;   // +1 to allow for the heatmap label
            if (yEnd < yOffset)
                yEnd = yOffset;
            int height = yEnd - yOffset;
            int xOffset = (int)(h->colStarts[column_j]);

            int width = h->colSizes[column_j];

            mapBoxHgcOrHgGene(hvg, lf->start, lf->end, xOffset, y+yOffset, width, height,
                       tg->track, lf->name, label, NULL, TRUE,
                       NULL);
            }
        }
    }
}



void heatmapItemMapAndArrows(struct track *tg, struct spaceNode *sn,
                                    struct hvGfx *hvg, int xOff, int y, int width,
                                    MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                                    double scale, boolean withLeftLabels)
/* Heatmap-specific function for putting down a mapbox with a label */
{
struct slList *item = sn->val;
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
int sClp = (s < winStart) ? winStart : s;
int eClp = (e > winEnd)   ? winEnd   : e;
int x1 = round((sClp - winStart)*scale) + xOff;
int x2 = round((eClp - winStart)*scale) + xOff;

int w = x2-x1;
tg->mapItem(tg, hvg, item, tg->itemName(tg, item),
            tg->mapItemName(tg, item), s, e, x1, y, w, tg->heightPer);
}


void heatmapMethods(struct track *tg)
/* This is intended to be invoked after commonBigBedMethods has already been applied to a track (really,
 * it's only expected to be invoked as part of bigBedMethods).
 * Cribbed from messageLineMethods, which also needs to check vis early on to decide what to do.
 * Here, we only want to set up to draw heatmaps if we're not in dense mode.
 */
{
//tg->mapsSelf = TRUE;  We're relying for now on the standard bigBed mapping, but might need to swap to mapsSelf
char *s = cartOptionalString(cart, tg->track);
if (cgiOptionalString("hideTracks"))
    {
    s = cgiOptionalString(tg->track);
    if (s != NULL && (hTvFromString(s) != tg->tdb->visibility))
        {
        cartSetString(cart, tg->track, s);
        }
    }
enum trackVisibility trackVis = tg->tdb->visibility;
if (s != NULL)
    trackVis = hTvFromString(s);
if (trackVis != tvDense)
    {
    tg->itemHeightRowsForPack = heatmapItemRowCount;
    tg->drawItemAt = heatmapDrawItemAt;
    tg->drawItemLabel = heatmapDrawItemLabel;
    tg->mapItem = heatmapMapItem;
    tg->doItemMapAndArrows = heatmapItemMapAndArrows;
    }
}
