/* heatmap - Routines for taking heatmap data in a bigBed and displaying it
 * in the browser.  The format has its own .as file, just not a distinct track
 * type, and a number of extra fields are required.  Columns in the heatmap
 * correspond to exons in the bigBed.
 */

struct heatmap
    {
    struct heatmap *next;
    char *label;
    int rows, cols;  // number of rows and columns
    double rowHeight, colWidth; // extent of a row or column in pixels; might be fractional
    int *rowStarts, *rowSizes;  // dimensions in pixels
    int *colStarts, *colSizes;  // dimensions in pixels
    char **rowLabels;
    char **colLabels; // not used at present, maybe later
    char **scores;
    char *special;
    char **itemLabels;
    boolean *isNA; // override for the scores table when a value was set to NA.  Consider defaulting to True
    double highCap; // scores at or above this level are saturated in the heatmap
    double lowCap; // scores at or below this level are saturated in the heatmap
    double midPoint; // the score where we transition from the low spectrum to the high spectrum
    struct rgbColor *lowColor, *midColor, *highColor;
    Color *lowSpectrum;
    Color *highSpectrum;
    };


struct heatmap *heatmapNew(int rows, int cols);
/* Allocate a new heatmap structure and return it */

void heatmapSetDrawSizes(struct heatmap *h, double rowHeight, double colWidth);
/* Update the expected cell height and width in pixels for the heatmap */

void heatmapAssignRowLabels(struct heatmap *h, char **rowLabels);
/* Planned abstraction for assigning row labels in a heatmap */

void heatmapAssignColumnLabels(struct heatmap *h, char **rowLabels);
/* Planned abstraction for assigning column labels in a heatmap */

struct heatmap *heatmapFromLF(struct track *tg, struct linkedFeatures *lf);
/* This takes a linkedFeatures structure from a bigBed and builds a heatmap out
 * of it.  In the current setup, the heatmap values and labels are each stored
 * in a 1-D comma-separated matrix that we convert to a 2-D matrix using
 * row-major ordering.
 * The column count is the number of exons in the LF structure; the number of
 * rows is in an extra field.  The full type definition is in hg/lib/heatmap.as.
 */

void heatmapDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg,
                         int x, int y, double scale, MgFont *font,
                         Color color, enum trackVisibility vis);

void heatmapMakeSpectrum(struct heatmap *h, struct hvGfx *hvg, double lowScore, struct rgbColor *lowColor,
        double midPoint, struct rgbColor *midColor, double highScore, struct rgbColor *highColor);
/* Take high, low, and midpoints and associated colors, and create two spectrums (one for low scores, one for high)
 * that can be used to shade cells in the heatmap.
 */

int heatmapItemRowCount(struct track *tg, void *item);
/* This returns the number of rows in a spaceSaver structure that this heatmap will
 * occupy.  This is expected to be the number of rows in the heatmap plus 1.  The
 * extra row is for the item label for the heatmap, since the space to the left is
 * already occupied by the row labels.
 */

void heatmapMethods(struct track *tg);
/* This is intended to be invoked after commonBigBedMethods has already been applied to a track (really,
 * it's only expected to be invoked as part of bigBedMethods).
 * Cribbed from messageLineMethods, which also needs to check vis early on to decide what to do.
 * Here, we only want to set up to draw heatmaps if we're not in dense mode.
 */
