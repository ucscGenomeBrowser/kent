/* wigCommon.h - common items to the two graphing types of tracks
 *	type wig and type bedGraph
 */
#ifndef WIGCOMMON_H
#define WIGCOMMON_H

struct preDrawElement
    {
	double	max;	/*	maximum value seen for this point	*/
	double	min;	/*	minimum value seen for this point	*/
	unsigned long long	count;	/* number of datum at this point */
	double	sumData;	/*	sum of all values at this point	*/
	double  sumSquares;	/* sum of (values squared) at this point */
	double  plotValue;	/*	raw data to plot	*/
	double  smooth;	/*	smooth data values	*/
    };

/*	source to these routines is in wigTrack.c	*/

struct preDrawElement * initPreDraw(int width, int *preDrawSize,
	int *preDrawZero);
/*	initialize a preDraw array of size width	*/

void preDrawWindowFunction(struct preDrawElement *preDraw, int preDrawSize,
	enum wiggleWindowingEnum windowingFunction);
/*	apply windowing function to the values in preDraw array	*/

void preDrawSmoothing(struct preDrawElement *preDraw, int preDrawSize,
    enum wiggleSmoothingEnum smoothingWindow);
/*	apply smoothing function to preDraw array	*/

double preDrawLimits(struct preDrawElement *preDraw, int preDrawZero,
    int width, double *overallUpperLimit, double *overallLowerLimit);
/*	scan preDraw array and determine graph limits */

double preDrawAutoScale(struct preDrawElement *preDraw, int preDrawZero,
    int width, enum wiggleScaleOptEnum autoScale,
    double *overallUpperLimit, double *overallLowerLimit,
    double *graphUpperLimit, double *graphLowerLimit,
    double *overallRange, double *epsilon, int lineHeight,
    double maxY, double minY);
/*	if autoScaling, scan preDraw array and determine limits */

Color * allocColorArray(struct preDrawElement *preDraw, int width,
    int preDrawZero, char *colorTrack, struct track *tg, struct vGfx *vg);
/*	allocate and fill in a coloring array based on another track */

void graphPreDraw(struct preDrawElement *preDraw, int preDrawZero, int width,
    struct track *tg, struct vGfx *vg, int xOff, int yOff,
    double graphUpperLimit, double graphLowerLimit, double graphRange,
    double epsilon, Color *colorArray, enum trackVisibility vis,
    enum wiggleGraphOptEnum lineBar);
/*	graph the preDraw array */

void drawZeroLine(enum trackVisibility vis,
    enum wiggleGridOptEnum horizontalGrid,
    double graphUpperLimit, double graphLowerLimit,
    struct vGfx *vg, int xOff, int yOff, int width, int lineHeight);
/*	draw a line at y=0 on the graph	*/

void drawArbitraryYLine(enum trackVisibility vis,
    enum wiggleGridOptEnum horizontalGrid,
    double graphUpperLimit, double graphLowerLimit,
    struct vGfx *vg, int xOff, int yOff, int width, int lineHeight,
    double yLineMark, double graphRange, enum wiggleYLineMarkEnum yLineOnOff);
/*	draw a line at y=yLineMark on the graph	*/

void wigMapSelf(struct track *tg, int seqStart, int seqEnd,
    int xOff, int yOff, int width);
/*	if self mapping, create the mapping box	*/

#endif /* WIGCOMMON_H */
