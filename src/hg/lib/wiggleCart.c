/*	wiggleCart - take care of parsing and combining values from the
 *	wiggle trackDb optional settings and the same values that may be
 *	in the cart.
 */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "hui.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wiggleCart.c,v 1.1 2004/01/20 19:39:56 hiram Exp $";

extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */

#define correctOrder(min,max) if (max < min) \
	{ double d; d = max; max = min; min = d; }
/* check a min,max pair (doubles) and keep them properly in order */

#if ! defined(DEBUG)
/****           some simple debug output during development	*/
static char dbgFile[] = "trash/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void wigDebugOpen(char * name) {
if (debugOpened) return;
dF = fopen( dbgFile, "w");
fprintf( dF, "opened by %s\n", name);
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH);
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
void wigDebugPrint(char * name) {
wigDebugOpen(name);
if (debugOpened)
    {
    if (dbgMsg[0])
	fprintf( dF, "%s: %s\n", name, dbgMsg);
    else
	fprintf( dF, "%s:\n", name);
    }
    dbgMsg[0] = (char) NULL;
    fflush(dF);
}
#endif

/*	Min, Max Y viewing limits
 *	Absolute limits are defined on the trackDb type line
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits declaration from trackDb
 *****************************************************************************/
void wigFetchMinMaxY(struct trackDb *tdb, double *min, double *max,
    double *tDbMin, double *tDbMax, int wordCount, char **words)
{
char o4[MAX_OPT_STRLEN]; /* Option 4 - minimum Y axis value: .minY	*/
char o5[MAX_OPT_STRLEN]; /* Option 5 - maximum Y axis value: .minY	*/
char *minY_str = NULL;  /*	string from cart	*/
char *maxY_str = NULL;  /*	string from cart	*/
double minYc;   /*	value from cart */
double maxYc;   /*	value from cart */
double minY;    /*	from trackDb.ra words, the absolute minimum */
double maxY;    /*	from trackDb.ra words, the absolute maximum */
char * defaultViewLimits =
    trackDbSettingOrDefault(tdb, "defaultViewLimits", "NONE");
double defaultViewMinY = 0.0;	/* optional default viewing window	*/
double defaultViewMaxY = 0.0;	/* can be different than absolute min,max */
boolean optionalViewLimitsExist = FALSE;	/* to decide if using these */
char cartStr[64];	/*	to set cart strings	*/


/*	Assume last resort defaults, these should never be used
 *	The only case they would be used is if trackDb settings are not
 *	there or can not be parsed properly
 *	A trackDb wiggle track entry should read with three words:
 *	type wig min max
 *	where min and max are floating point numbers (integers OK)
 */
*min = minY = DEFAULT_MIN_Yv;
*max = maxY = DEFAULT_MAX_Yv;

/*	Let's see what trackDb has to say about these things	*/
switch (wordCount)
    {
	case 3:
	    maxY = atof(words[2]);
	    minY = atof(words[1]);
	    break;
	case 2:
	    minY = atof(words[1]);
	    break;
	default:
	    break;
    } 
correctOrder(minY,maxY);
*tDbMin = minY;
*tDbMax = maxY;

/*	See if a default viewing window is specified in the trackDb.ra file
 *	Yes, it is true, this parsing is paranoid and verifies that the
 *	input values are valid in order to be used.  If they are no
 *	good, they are as good as not there and the result is a pair of
 *	zeros and they are not used.
 */
if (differentWord("NONE",defaultViewLimits))
    {
    char *words[2];
    char sep = ':';
    int wordCount = chopString(defaultViewLimits,&sep,words,ArraySize(words));
    if (wordCount == 2)
	{
	defaultViewMinY = atof(words[0]);
	defaultViewMaxY = atof(words[1]);
	/*	make sure they are in order	*/
	correctOrder(defaultViewMinY,defaultViewMaxY);

	/*	and they had better be different	*/
	if (! ((defaultViewMaxY - defaultViewMinY) > 0.0))
	    {
	    defaultViewMaxY = defaultViewMinY = 0.0;	/* failed the test */
	    }
	else
	    optionalViewLimitsExist = TRUE;
	}
    }

/*	And finally, let's see if values are available in the cart */
snprintf( o4, sizeof(o4), "%s.minY", tdb->tableName);
snprintf( o5, sizeof(o5), "%s.maxY", tdb->tableName);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);

if (minY_str && maxY_str)	/*	if specified in the cart */
    {
    minYc = atof(minY_str);	/*	try to use them	*/
    maxYc = atof(maxY_str);
    }
else
    {
    if (optionalViewLimitsExist)	/* if specified in trackDb	*/
	{
	minYc = defaultViewMinY;	/*	try to use these	*/
	maxYc = defaultViewMaxY;
	}
    else
	{
	minYc = minY;		/* no cart, no other settings, use the	*/
	maxYc = maxY;		/* values from the trackDb type line	*/
	}
    }


/*	Finally, clip the possible user requested settings to those
 *	limits within the range of the trackDb type line
 */
*min = max( minY, minYc);
*max = min( maxY, maxYc);
/*      And ensure their order is correct     */
correctOrder(*min,*max);

return;
}	/*	void wigFetchMinMaxY()	*/

/*	Min, Max, Default Pixel height of track
 *	Limits may be defined in trackDb with the maxHeightPixels string,
 *	Or user requested limits are defined in the cart.
 *	And default opening display limits may optionally be defined with the
 *		maxHeightPixels declaration from trackDb
 *****************************************************************************/
void wigFetchMinMaxPixels(struct trackDb *tdb, int *Min, int *Max,
    int *Default, int wordCount, char **words)
{
char option[MAX_OPT_STRLEN]; /* Option 1 - track pixel height:  .heightPer  */
char *heightPer = NULL; /*	string from cart	*/
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int defaultHeightPixels = maxHeightPixels;
int defaultHeight;      /*      truncated by limits     */
int minHeightPixels = MIN_HEIGHT_PER;
char * maxHeightPixelString =
    trackDbSettingOrDefault(tdb, "maxHeightPixels", DEFAULT_HEIGHT_PER);
char cartStr[64];	/*	to set cart strings	*/

/*	the maxHeightPixels string can be one, two, or three words
 *	separated by :
 *	All three would be: 	max:default:min
 *	When only two: 		max:default
 *	When only one: 		max
 *	(this works too:	min:default:max)
 *	Where min is minimum allowed, default is initial default setting
 *	and max is the maximum allowed
 *	If it isn't available, these three have already been set
 *	in their declarations above
 */
if (differentWord(DEFAULT_HEIGHT_PER,maxHeightPixelString))
    {
    char *words[3];
    char sep = ':';
    int wordCount=chopString(maxHeightPixelString,&sep,words,ArraySize(words));
    switch (wordCount)
	{
	case 3:
	    minHeightPixels = atoi(words[2]);
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);
	    correctOrder(minHeightPixels,maxHeightPixels);
	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 2:
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);
	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 1:
	    maxHeightPixels = atoi(words[0]);
	    defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	default:
	    break;
	}
    }

snprintf( option, sizeof(option), "%s.heightPer", tdb->tableName);
heightPer = cartOptionalString(cart, option);
/*      Clip the cart value to range [minHeightPixels:maxHeightPixels] */
if (heightPer) defaultHeight = min( maxHeightPixels, atoi(heightPer));
else defaultHeight = defaultHeightPixels;
defaultHeight = max(minHeightPixels, defaultHeight);

*Max = maxHeightPixels;
*Default = defaultHeight;
*Min = minHeightPixels;

}	/* void wigFetchMinMaxPixels()	*/

/*	These last three items are all looking pretty identical in their
 *	processing.  They could be turned into one generic set of code
 *	except their defaults are all different.  You would need to pass
 *	in a default as an argument and I'm not sure we want to do that.
 *	Right now all defaults are specified here and can be overridden
 *	by trackDb strings.
 */
/*	horizontalGrid - off by default **********************************/
enum wiggleGridOptEnum wigFetchHorizontalGrid(struct trackDb *tdb,
	char **optString, int wordCount, char **words)
{
char *Default = wiggleGridEnumToString(wiggleHorizontalGridOff);
char *notDefault = wiggleGridEnumToString(wiggleHorizontalGridOn);
char option[MAX_OPT_STRLEN]; /* .horizGrid  */
char *horizontalGrid = NULL;

snprintf( option, sizeof(option), "%s.horizGrid", tdb->tableName );
horizontalGrid = cartOptionalString(cart, option);

/*	If horizontalGrid is a string, it came from the cart, otherwise
 *	return the default for this option.
 */
if (!horizontalGrid)
    {
    char * gridDefault = 
	trackDbSettingOrDefault(tdb, "gridDefault", Default);
    if (differentWord(Default,gridDefault))
	horizontalGrid = notDefault;
    else
	horizontalGrid = Default;
    }

if (optString)
    *optString = horizontalGrid;

return wiggleGridStringToEnum(horizontalGrid);
}	/*	enum wiggleGridOptEnum wigFetchHorizontalGrid()	*/

/******	autoScale - on by default ***************************************/
enum wiggleScaleOptEnum wigFetchAutoScale(struct trackDb *tdb,
	char **optString, int wordCount, char **words)
{
char * Default = wiggleScaleEnumToString(wiggleScaleAuto);
char * notDefault = wiggleScaleEnumToString(wiggleScaleManual);
char option[MAX_OPT_STRLEN]; /* .autoScale  */
char * autoScale = NULL;

snprintf( option, sizeof(option), "%s.autoScale", tdb->tableName );
autoScale = cartOptionalString(cart, option);

/*	If autoScale is a string, it came from the cart, otherwise
 *	see if it is specified in the trackDb option, finally
 *	return the default.
 */
if (!autoScale)
    {
    char * autoScaleDefault = 
	trackDbSettingOrDefault(tdb, "autoScaleDefault", Default);
    if (differentWord(Default,autoScaleDefault))
	autoScale = notDefault;
    else
	autoScale = Default;
    }

if (optString)
    *optString = autoScale;

return wiggleScaleStringToEnum(autoScale);
}	/*	enum wiggleScaleOptEnum wigFetchAutoScale()	*/

/******	graphType - line(points) or bar graph *****************************/
enum wiggleGraphOptEnum wigFetchGraphType(struct trackDb *tdb,
	char **optString, int wordCount, char **words)
{
char *Default = wiggleGraphEnumToString(wiggleGraphBar);
char *notDefault = wiggleGraphEnumToString(wiggleGraphPoints);
char option[MAX_OPT_STRLEN]; /* .horizGrid  */
char *graphType = NULL;

snprintf( option, sizeof(option), "%s.lineBar", tdb->tableName );
graphType = cartOptionalString(cart, option);

/*	If graphType is a string, it came from the cart, otherwise
 *	return the default for this option.
 */
if (!graphType)
    {
    char * graphTypeDefault = 
	trackDbSettingOrDefault(tdb, "graphTypeDefault", Default);
    if (differentWord(Default,graphTypeDefault))
	graphType = notDefault;
    else
	graphType = Default;
    }

if (optString)
    *optString = graphType;

return wiggleGraphStringToEnum(graphType);
}	/*	enum wiggleGraphOptEnum wigFetchGraphType()	*/
