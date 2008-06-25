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

static char const rcsid[] = "$Id: wiggleCart.c,v 1.15 2008/06/25 16:13:14 tdreszer Exp $";

extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */

#define correctOrder(min,max) if (max < min) \
	{ double d; d = max; max = min; min = d; }
/* check a min,max pair (doubles) and keep them properly in order */

#if defined(DEBUG)	/*	dbg	*/

#include "portable.h"

static long wigProfileEnterTime = 0;

void wigProfileEnter()
{
wigProfileEnterTime = clock1000();
}

long wigProfileLeave()
{
long deltaTime = 0;
deltaTime = clock1000() - wigProfileEnterTime;
return deltaTime;
}

/****           some simple debug output during development	*/
static char dbgFile[] = "trash/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void wigDebugOpen(char * name) {
if (debugOpened) return;
dF = fopen( dbgFile, "w");
if ((FILE *)NULL == dF)
	errAbort("wigDebugOpen: can not open %s", dbgFile);
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

#ifdef NOT
/*	example usage:	*/
snprintf(dbgMsg, DBGMSGSZ, "%s pixels: min,default,max: %d:%d:%d", tdb->tableName, wigCart->minHeight, wigCart->defaultHeight, wigCart->maxHeight);
wigDebugPrint("wigFetch");
#endif

#endif

/*****************************************************************************
 *	Min, Max Y viewing limits
 *	Absolute limits are defined on the trackDb type line
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits declaration from trackDb
 *		or viewLimits from custom tracks
 *		(both identifiers work from either custom or trackDb)
 *****************************************************************************/
void wigFetchMinMaxYWithCart(struct cart *theCart, struct trackDb *tdb, char *name, 
    double *min, double *max, double *tDbMin, double *tDbMax, int wordCount, char **words)
{
char o4[MAX_OPT_STRLEN]; /* Option 4 - minimum Y axis value: .minY	*/
char o5[MAX_OPT_STRLEN]; /* Option 5 - maximum Y axis value: .minY	*/
char *minY_str = NULL;  /*	string from cart	*/
char *maxY_str = NULL;  /*	string from cart	*/
double minYc;   /*	value from cart */
double maxYc;   /*	value from cart */
double minY;    /*	from trackDb.ra words, the absolute minimum */
double maxY;    /*	from trackDb.ra words, the absolute maximum */
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, DEFAULTVIEWLIMITS, "NONE") );
double defaultViewMinY = 0.0;	/* optional default viewing window	*/
double defaultViewMaxY = 0.0;	/* can be different than absolute min,max */
char * viewLimits = (char *)NULL;
boolean optionalViewLimitsExist = FALSE;	/* to decide if using these */


/*	Allow the word "viewLimits" to be recognized too */
if (sameWord("NONE",tdbDefault))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(trackDbSettingOrDefault(tdb, VIEWLIMITS, "NONE"));
    }

/*	And check for either viewLimits in custom track settings */
if ((tdb->settings != (char *)NULL) &&
    (tdb->settingsHash != (struct hash *)NULL))
    {
    struct hashEl *hel;
    if ((hel = hashLookup(tdb->settingsHash, VIEWLIMITS)) != NULL)
	viewLimits = cloneString((char *)hel->val);
    else if ((hel = hashLookup(tdb->settingsHash, DEFAULTVIEWLIMITS)) != NULL)
	viewLimits = cloneString((char *)hel->val);
    }

/*	If no viewLimits from trackDb, if in settings, use them.  */
if (sameWord("NONE",tdbDefault) && (viewLimits != (char *)NULL))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(viewLimits);
    }

/*	Assume last resort defaults, these should never be used
 *	The only case they would be used is if trackDb settings are not
 *	there or can not be parsed properly
 *	A trackDb wiggle track entry should read with three words:
 *	type wig min max
 *	where min and max are floating point numbers (integers OK)
 */
*min = minY = DEFAULT_MIN_Yv;
*max = maxY = DEFAULT_MAX_Yv;

/*	Let's see what trackDb has to say about these things,
 *	these words come from the trackDb.ra type wig line:
 *	type wig <min> <max>
 */
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

/*	Check to see if custom track viewLimits will override the
 *	track type wig limits.  When viewLimits are greater than the
 *	type wig limits, use the viewLimits.  This is necessary because
 *	the custom track type wig limits are exactly at the limits of
 *	the data input and thus it is better if viewLimits are set
 *	outside the limits of the data so it will look better in the
 *	graph.
 */
if (viewLimits)
    {
    char *words[2];
    char *sep = ":";
    int wordCount = chopString(viewLimits,sep,words,ArraySize(words));
    double viewMin;
    double viewMax;
    if (wordCount == 2)
	{
	viewMin = atof(words[0]);
	viewMax = atof(words[1]);
	/*	make sure they are in order	*/
	correctOrder(viewMin,viewMax);

	/*	and they had better be different	*/
	if (! ((viewMax - viewMin) > 0.0))
	    {
	    viewMax = viewMin = 0.0;	/* failed the test */
	    }
	else
	    {
	    minY = min(minY, viewMin);
	    maxY = max(maxY, viewMax);
	    }
	}
    }

/*	return if OK to do that	*/
if (tDbMin)
    *tDbMin = minY;
if (tDbMax)
    *tDbMax = maxY;

/*	See if a default viewing window is specified in the trackDb.ra file
 *	Yes, it is true, this parsing is paranoid and verifies that the
 *	input values are valid in order to be used.  If they are no
 *	good, they are as good as not there and the result is a pair of
 *	zeros and they are not used.
 */
if (differentWord("NONE",tdbDefault))
    {
    char *words[2];
    char *sep = ":";
    int wordCount = chopString(tdbDefault,sep,words,ArraySize(words));
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
snprintf( o4, sizeof(o4), "%s.%s", name, MIN_Y);
snprintf( o5, sizeof(o5), "%s.%s", name, MAX_Y);
minY_str = cartOptionalString(theCart, o4);
maxY_str = cartOptionalString(theCart, o5);

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

freeMem(tdbDefault);
freeMem(viewLimits);
}	/*	void wigFetchMinMaxYWithCart()	*/

/*	Min, Max, Default Pixel height of track
 *	Limits may be defined in trackDb with the maxHeightPixels string,
 *	Or user requested limits are defined in the cart.
 *	And default opening display limits may optionally be defined with the
 *		maxHeightPixels declaration from trackDb
 *****************************************************************************/
void wigFetchMinMaxPixelsWithCart(struct cart *theCart, struct trackDb *tdb, char *name,int *Min, int *Max, int *Default)
{
char option[MAX_OPT_STRLEN]; /* Option 1 - track pixel height:  .heightPer  */
char *heightPer = NULL; /*	string from cart	*/
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int defaultHeightPixels = maxHeightPixels;
int defaultHeight;      /*      truncated by limits     */
int minHeightPixels = MIN_HEIGHT_PER;
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, MAXHEIGHTPIXELS, DEFAULT_HEIGHT_PER) );

if (sameWord(DEFAULT_HEIGHT_PER,tdbDefault))
    {
    struct hashEl *hel;
    /*	no maxHeightPixels from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, MAXHEIGHTPIXELS)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

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
if (differentWord(DEFAULT_HEIGHT_PER,tdbDefault))
    {
    char *words[3];
    char *sep = ":";
    int wordCount;
    wordCount=chopString(tdbDefault,sep,words,ArraySize(words));
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
snprintf( option, sizeof(option), "%s.%s", name, HEIGHTPER);
heightPer = cartOptionalString(theCart, option);
/*      Clip the cart value to range [minHeightPixels:maxHeightPixels] */
if (heightPer) defaultHeight = min( maxHeightPixels, atoi(heightPer));
else defaultHeight = defaultHeightPixels;
defaultHeight = max(minHeightPixels, defaultHeight);

*Max = maxHeightPixels;
*Default = defaultHeight;
*Min = minHeightPixels;

freeMem(tdbDefault);
}	/* void wigFetchMinMaxPixelsWithCart()	*/

/*	A common operation for binary options (two values possible)
 *	check for trackDb.ra, then tdb->settings values
 *	return one of the two possibilities if found
 *	(the tdbString and secondTdbString are a result of
 *		early naming conventions changing over time resulting in
 *		two possible names for the same thing ...)
 */
static char *wigCheckBinaryOption(struct trackDb *tdb, char *Default,
    char *notDefault, char *tdbString, char *secondTdbString)
{
char *tdbDefault = trackDbSettingOrDefault(tdb, tdbString, "NONE");
char *ret;

ret = Default;	/* the answer, unless found to be otherwise	*/

if (sameWord("NONE",tdbDefault) && (secondTdbString != (char *)NULL))
    tdbDefault = trackDbSettingOrDefault(tdb, secondTdbString, "NONE");

if (differentWord("NONE",tdbDefault))
    {
    if (differentWord(Default,tdbDefault))
    	ret = notDefault;
    }
else
    {
    struct hashEl *hel;
    /*	no setting from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */

    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, tdbString)) != NULL)
	    {
	    if (differentWord(Default,(char *)hel->val))
		ret = notDefault;
	    }
	else if (secondTdbString != (char *)NULL)
	    {
	    if ((hel = hashLookup(tdb->settingsHash, secondTdbString)) != NULL)
		{
		if (differentWord(Default,(char *)hel->val))
		    ret = notDefault;
		}
	    }
	}
    }
return(cloneString(ret));
}

/*	horizontalGrid - off by default **********************************/
enum wiggleGridOptEnum wigFetchHorizontalGridWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name,char **optString)
{
char *Default = wiggleGridEnumToString(wiggleHorizontalGridOff);
char *notDefault = wiggleGridEnumToString(wiggleHorizontalGridOn);
char option[MAX_OPT_STRLEN]; /* .horizGrid  */
char *horizontalGrid = NULL;
enum wiggleGridOptEnum ret;

snprintf( option, sizeof(option), "%s.%s", name, HORIZGRID );
horizontalGrid = cloneString(cartOptionalString(theCart, option));

if (!horizontalGrid)	/*	if it is NULL	*/
    horizontalGrid = wigCheckBinaryOption(tdb,Default,notDefault,GRIDDEFAULT,
	HORIZGRID);

if (optString)
    *optString = cloneString(horizontalGrid);

ret = wiggleGridStringToEnum(horizontalGrid);
freeMem(horizontalGrid);
return(ret);
}	/*	enum wiggleGridOptEnum wigFetchHorizontalGridWithCart()	*/

/******	autoScale - on by default ***************************************/
enum wiggleScaleOptEnum wigFetchAutoScaleWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name, char **optString)
{
char *Default = wiggleScaleEnumToString(wiggleScaleAuto);
char *notDefault = wiggleScaleEnumToString(wiggleScaleManual);
char option[MAX_OPT_STRLEN]; /* .autoScale  */
char *autoScale = NULL;
enum wiggleScaleOptEnum ret;


snprintf( option, sizeof(option), "%s.%s", name, AUTOSCALE );
autoScale = cloneString(cartOptionalString(theCart, option));

if (!autoScale)	/*	if nothing from the Cart, check trackDb/settings */
    {
    /*	It may be the autoScale=on/off situation from custom tracks */
    char * tdbDefault = trackDbSettingOrDefault(tdb, AUTOSCALE, "NONE");
    if (sameWord(tdbDefault,"on"))
	autoScale = cloneString(Default);
    else if (sameWord(tdbDefault,"off"))
	autoScale = cloneString(notDefault);
    else
	autoScale = wigCheckBinaryOption(tdb,Default,notDefault,
	    AUTOSCALEDEFAULT, AUTOSCALE);
    }

if (optString)
    *optString = cloneString(autoScale);

ret = wiggleScaleStringToEnum(autoScale);
freeMem(autoScale);
return(ret);
}	/*	enum wiggleScaleOptEnum wigFetchAutoScaleWithCart()	*/

/******	graphType - line(points) or bar graph *****************************/
enum wiggleGraphOptEnum wigFetchGraphTypeWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name, char **optString)
{
char *Default = wiggleGraphEnumToString(wiggleGraphBar);
char *notDefault = wiggleGraphEnumToString(wiggleGraphPoints);
char option[MAX_OPT_STRLEN]; /* .lineBar  */
char *graphType = NULL;
enum wiggleGraphOptEnum ret;

snprintf( option, sizeof(option), "%s.%s", name, LINEBAR );
graphType = cloneString(cartOptionalString(theCart, option));

if (!graphType)	/*	if nothing from the Cart, check trackDb/settings */
    graphType = wigCheckBinaryOption(tdb,Default,notDefault,GRAPHTYPEDEFAULT,
	GRAPHTYPE);

if (optString)
    *optString = cloneString(graphType);

ret = wiggleGraphStringToEnum(graphType);
freeMem(graphType);
return(ret);
}	/*	enum wiggleGraphOptEnum wigFetchGraphTypeWithCart()	*/

/******	windowingFunction - Maximum by default **************************/
enum wiggleWindowingEnum wigFetchWindowingFunctionWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name, char **optString)
{
char *Default = wiggleWindowingEnumToString(wiggleWindowingMax);
char option[MAX_OPT_STRLEN]; /* .windowingFunction  */
char *windowingFunction = NULL;
enum wiggleWindowingEnum ret;

snprintf( option, sizeof(option), "%s.%s", name, WINDOWINGFUNCTION );
windowingFunction = cloneString(cartOptionalString(theCart, option));

/*	If windowingFunction is a string, it came from the cart, otherwise
 *	see if it is specified in the trackDb option, finally
 *	return the default.
 */
if (!windowingFunction)
    {
    char * tdbDefault = 
	trackDbSettingOrDefault(tdb, WINDOWINGFUNCTION, Default);

    freeMem(windowingFunction);
    if (differentWord(Default,tdbDefault))
	windowingFunction = cloneString(tdbDefault);
    else
	{
	struct hashEl *hel;
	/*	no windowingFunction from trackDb, maybe it is in tdb->settings
	 *	(custom tracks keep settings here)
	 */
	windowingFunction = cloneString(Default);
	if ((tdb->settings != (char *)NULL) &&
	    (tdb->settingsHash != (struct hash *)NULL))
	    {
	    if ((hel =hashLookup(tdb->settingsHash, WINDOWINGFUNCTION)) !=NULL)
		if (differentWord(Default,(char *)hel->val))
		    {
		    freeMem(windowingFunction);
		    windowingFunction = cloneString((char *)hel->val);
		    }
	    }
	}
    }

if (optString)
    *optString = cloneString(windowingFunction);

ret = wiggleWindowingStringToEnum(windowingFunction);
freeMem(windowingFunction);
return(ret);
}	/*	enum wiggleWindowingEnum wigFetchWindowingFunctionWithCart() */

/******	smoothingWindow - OFF by default **************************/
enum wiggleSmoothingEnum wigFetchSmoothingWindowWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name, char **optString)
{
char * Default = wiggleSmoothingEnumToString(wiggleSmoothingOff);
char option[MAX_OPT_STRLEN]; /* .smoothingWindow  */
char * smoothingWindow = NULL;
enum wiggleSmoothingEnum ret;

snprintf( option, sizeof(option), "%s.%s", name, SMOOTHINGWINDOW );
smoothingWindow = cloneString(cartOptionalString(theCart, option));

if (!smoothingWindow) /* if nothing from the Cart, check trackDb/settings */
    {
    char * tdbDefault = 
	trackDbSettingOrDefault(tdb, SMOOTHINGWINDOW, Default);


    if (differentWord(Default,tdbDefault))
	smoothingWindow = cloneString(tdbDefault);
    else
	{
	struct hashEl *hel;
	/*	no smoothingWindow from trackDb, maybe it is in tdb->settings
	 *	(custom tracks keep settings here)
	 */
	smoothingWindow = cloneString(Default);
	if ((tdb->settings != (char *)NULL) &&
	    (tdb->settingsHash != (struct hash *)NULL))
	    {
	    if ((hel = hashLookup(tdb->settingsHash, SMOOTHINGWINDOW)) != NULL)
		if (differentWord(Default,(char *)hel->val))
		    {
		    freeMem(smoothingWindow);
		    smoothingWindow = cloneString((char *)hel->val);
		    }
	    }
	}
    }

if (optString)
    *optString = cloneString(smoothingWindow);

ret = wiggleSmoothingStringToEnum(smoothingWindow);
freeMem(smoothingWindow);
return(ret);
}	/*	enum wiggleSmoothingEnum wigFetchSmoothingWindowWithCart()	*/

/*	yLineMark - off by default **********************************/
enum wiggleYLineMarkEnum wigFetchYLineMarkWithCart(struct cart *theCart, 
    struct trackDb *tdb, char *name, char **optString)
{
char *Default = wiggleYLineMarkEnumToString(wiggleYLineMarkOff);
char *notDefault = wiggleYLineMarkEnumToString(wiggleYLineMarkOn);
char option[MAX_OPT_STRLEN]; /* .yLineMark  */
char *yLineMark = NULL;
enum wiggleYLineMarkEnum ret;

snprintf( option, sizeof(option), "%s.%s", name, YLINEONOFF );
yLineMark = cloneString(cartOptionalString(theCart, option));

if (!yLineMark)	/*	if nothing from the Cart, check trackDb/settings */
    yLineMark = wigCheckBinaryOption(tdb,Default,notDefault,YLINEONOFF,
	(char *)NULL);

if (optString)
    *optString = cloneString(yLineMark);

ret = wiggleYLineMarkStringToEnum(yLineMark);
freeMem(yLineMark);
return(ret);
}	/*	enum wiggleYLineMarkEnum wigFetchYLineMarkWithCart()	*/

/*	y= marker line value
 *	User requested value is defined in the cart
 *	A Default value can be defined as
 *		yLineMark declaration from trackDb
 *****************************************************************************/
void wigFetchYLineMarkValueWithCart(struct cart *theCart,struct trackDb *tdb, char *name, double *tDbYMark )
{
char option[MAX_OPT_STRLEN]; /* Option 11 - value from: .yLineMark */
char *yLineMarkValue = NULL;  /*	string from cart	*/
double yLineValue;   /*	value from cart or trackDb */
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, YLINEMARK, "NONE") );

if (sameWord("NONE",tdbDefault))
    {
    struct hashEl *hel;
    /*	no yLineMark from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, YLINEMARK)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

/*	If nothing else, it is zero	*/
yLineValue = 0.0;

/*	Let's see if a value is available in the cart */
snprintf( option, sizeof(option), "%s.%s", name, YLINEMARK);
yLineMarkValue = cartOptionalString(theCart, option);

/*	if yLineMarkValue is non-Null, it is the requested value 	*/
if (yLineMarkValue)
    yLineValue = atof(yLineMarkValue);
else /*    See if a default line is specified in the trackDb.ra file */
    if (differentWord("NONE",tdbDefault))
	yLineValue = atof(tdbDefault);

/*	If possible to return	*/
if (tDbYMark)
	*tDbYMark = yLineValue;

freeMem(tdbDefault);
}	/*	void wigFetchYLineMarkValueWithCart()	*/

/*****************************************************************************
 *	Min, Max Y viewing limits
 *	Absolute limits are defined by minLimit, maxLimit in the trackDb
 *		default to 0 and 1000 if not present
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits declaration from trackDb
 *		or viewLimits from custom tracks
 *		(both identifiers work from either custom or trackDb)
 *****************************************************************************/
void wigFetchMinMaxLimitsWithCart(struct cart *theCart, struct trackDb *tdb, char *name, 
    double *min, double *max,double *tDbMin, double *tDbMax)
{
char o4[MAX_OPT_STRLEN]; /* Option 4 - minimum Y axis value: .minY	*/
char o5[MAX_OPT_STRLEN]; /* Option 5 - maximum Y axis value: .minY	*/
char *minY_str = NULL;  /*	string from cart	*/
char *maxY_str = NULL;  /*	string from cart	*/
double minYc;   /*	value from cart */
double maxYc;   /*	value from cart */
double minY;    /*	from trackDb.ra minLimit, the absolute minimum */
double maxY;    /*	from trackDb.ra maxLimit, the absolute maximum */
char * tdbMin = cloneString(
    trackDbSettingOrDefault(tdb, MIN_LIMIT, "NONE") );
char * tdbMax = cloneString(
    trackDbSettingOrDefault(tdb, MAX_LIMIT, "NONE") );
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, DEFAULTVIEWLIMITS, "NONE") );
double defaultViewMinY = 0.0;	/* optional default viewing window	*/
double defaultViewMaxY = 0.0;	/* can be different than absolute min,max */
char * viewLimits = (char *)NULL;
char * settingsMin = (char *)NULL;
char * settingsMax = (char *)NULL;
boolean optionalViewLimitsExist = FALSE;	/* to decide if using these */

/*	Allow the word "viewLimits" to be recognized too */
if (sameWord("NONE",tdbDefault))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(trackDbSettingOrDefault(tdb, VIEWLIMITS, "NONE"));
    }

/*	And check for either viewLimits in custom track settings */
if ((tdb->settings != (char *)NULL) &&
    (tdb->settingsHash != (struct hash *)NULL))
    {
    struct hashEl *hel;
    if ((hel = hashLookup(tdb->settingsHash, VIEWLIMITS)) != NULL)
	viewLimits = cloneString((char *)hel->val);
    else if ((hel = hashLookup(tdb->settingsHash, DEFAULTVIEWLIMITS)) != NULL)
	viewLimits = cloneString((char *)hel->val);
    if ((hel = hashLookup(tdb->settingsHash, MIN_LIMIT)) != NULL)
	settingsMin = cloneString((char *)hel->val);
    if ((hel = hashLookup(tdb->settingsHash, MAX_LIMIT)) != NULL)
	settingsMax = cloneString((char *)hel->val);
    }

/*	If no viewLimits from trackDb, if in settings, use them.  */
if (sameWord("NONE",tdbDefault) && (viewLimits != (char *)NULL))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(viewLimits);
    }

/*	Assume last resort defaults, these should never be used
 *	The only case they would be used is if trackDb settings are not
 *	there or can not be parsed properly
 */
*min = minY = DEFAULT_MIN_BED_GRAPH;
*max = maxY = DEFAULT_MAX_BED_GRAPH;

/*	Let's see if trackDb, or settings from custom track,
 *	specifies a min,max
 */
if (sameWord("NONE",tdbMin))
    {
    if (settingsMin != (char *)NULL)
	minY = sqlDouble(settingsMin);
    }
else
    minY = sqlDouble(tdbMin);

if (sameWord("NONE",tdbMax))
    {
    if (settingsMax != (char *)NULL)
	maxY = sqlDouble(settingsMax);
    }
else
    maxY = sqlDouble(tdbMax);

correctOrder(minY,maxY);

/*	Check to see if custom track viewLimits will override the
 *	track type wig limits.  When viewLimits are greater than the
 *	type wig limits, use the viewLimits.  This is necessary because
 *	the custom track type wig limits are exactly at the limits of
 *	the data input and thus it is better if viewLimits are set
 *	outside the limits of the data so it will look better in the
 *	graph.
 */
if (viewLimits)
    {
    char *words[2];
    char *sep = ":";
    int wordCount = chopString(viewLimits,sep,words,ArraySize(words));
    double viewMin;
    double viewMax;
    if (wordCount == 2)
	{
	viewMin = atof(words[0]);
	viewMax = atof(words[1]);
	/*	make sure they are in order	*/
	correctOrder(viewMin,viewMax);

	/*	and they had better be different	*/
	if (! ((viewMax - viewMin) > 0.0))
	    {
	    viewMax = viewMin = 0.0;	/* failed the test */
	    }
	else
	    {
	    minY = min(minY, viewMin);
	    maxY = max(maxY, viewMax);
	    }
	}
    }

/*	return if OK to do that	*/
if (tDbMin)
    *tDbMin = minY;
if (tDbMax)
    *tDbMax = maxY;

/*	See if a default viewing window is specified in the trackDb.ra file
 *	Yes, it is true, this parsing is paranoid and verifies that the
 *	input values are valid in order to be used.  If they are no
 *	good, they are as good as not there and the result is a pair of
 *	zeros and they are not used.
 */
if (differentWord("NONE",tdbDefault))
    {
    char *words[2];
    char *sep = ":";
    int wordCount = chopString(tdbDefault,sep,words,ArraySize(words));
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
snprintf( o4, sizeof(o4), "%s.%s", name, MIN_Y);
snprintf( o5, sizeof(o5), "%s.%s", name, MAX_Y);
minY_str = cartOptionalString(theCart, o4);
maxY_str = cartOptionalString(theCart, o5);

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

/*	possibly cloned strings, free when non null	*/
freeMem(tdbDefault);
freeMem(tdbMax);
freeMem(tdbMin);
freeMem(viewLimits);
freeMem(settingsMax);
freeMem(settingsMin);
}	/*	void wigFetchMinMaxYWithCart()	*/
