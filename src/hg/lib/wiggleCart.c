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

static char const rcsid[] = "$Id: wiggleCart.c,v 1.24 2009/11/10 05:41:56 kent Exp $";

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

static void wigMaxLimitsCompositeOverride(struct trackDb *tdb,char *name,double *min, double *max,double *tDbMin, double *tDbMax)
/* If aquiring min/max for composite level wig cfg, look for trackDb.ra "settingsByView" */
{
if(isNameAtCompositeLevel(tdb,name))
    {
    char *setting = trackDbSettingByView(tdb,VIEWLIMITSMAX);
    if(setting != NULL)
        {
        char *word = strSwapChar(setting,':',' ');
        if(tDbMin != NULL && word[0] != 0)
            *tDbMin = sqlDouble(nextWord(&word));
        if(tDbMax != NULL && word[0] != 0)
            *tDbMax = sqlDouble(word);
        if(tDbMin != NULL && tDbMax != NULL)
            correctOrder(*tDbMin,*tDbMax);
        freez(&setting);
        }
    setting = trackDbSettingByView(tdb,VIEWLIMITS);
    if(setting != NULL)
        {
        char *word = strSwapChar(setting,':',' ');
        assert(min != NULL && max != NULL);
        *min = sqlDouble(nextWord(&word));
        if(word[0] != 0)
            *max = sqlDouble(word);
        correctOrder(*min,*max);
        freez(&setting);
        }
    }
}

/*****************************************************************************
 *	Min, Max Y viewing limits
 *	Absolute limits are defined on the trackDb type line
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits declaration from trackDb
 *		or viewLimits from custom tracks
 *		(both identifiers work from either custom or trackDb)
 * [JK Comment - this is a horrible, horrible routine!  Should be maybe 10 lines, not 200, no?]
 *****************************************************************************/
void wigFetchMinMaxYWithCart(struct cart *theCart, struct trackDb *tdb, char *name,
    double *min, double *max, double *absMax, double *absMin, int wordCount, char **words)
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *minY_str = NULL;  /*	string from cart	*/
char *maxY_str = NULL;  /*	string from cart	*/
double minY, maxY;	/* Current min/max for view. */
double absMinY, absMaxY;	/* Absolute min/max allowed. */
char * tdbDefault = cloneString(trackDbSettingClosestToHomeOrDefault(tdb, DEFAULTVIEWLIMITS, "NONE"));
double defaultViewMinY = 0.0;	/* optional default viewing window	*/
double defaultViewMaxY = 0.0;	/* can be different than absolute min,max */
char * viewLimits = (char *)NULL;
char * settingsMin = (char *)NULL;
char * settingsMax = (char *)NULL;
boolean isBedGraph = (wordCount == 0 || sameString(words[0],"bedGraph"));

/*	Allow the word "viewLimits" to be recognized too */
if (sameWord("NONE",tdbDefault))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(trackDbSettingClosestToHomeOrDefault(tdb, VIEWLIMITS, "NONE"));
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
    if(isBedGraph)
        {
        if ((hel = hashLookup(tdb->settingsHash, MIN_LIMIT)) != NULL)
        settingsMin = cloneString((char *)hel->val);
        if ((hel = hashLookup(tdb->settingsHash, MAX_LIMIT)) != NULL)
        settingsMax = cloneString((char *)hel->val);
        }
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
if(isBedGraph)
    {
    *min = minY = DEFAULT_MIN_BED_GRAPH;
    *max = maxY = DEFAULT_MAX_BED_GRAPH;
    }
else
    {
    *min = minY = DEFAULT_MIN_Yv;
    *max = maxY = DEFAULT_MAX_Yv;
    }

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
            minY = viewMin;
            maxY = viewMax;
            }
        }
    }

*min = minY;
*max = maxY;

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
            {
            *min = defaultViewMinY;
            *max = defaultViewMaxY;
            }
        }
    }

/* Calculate absolute min/max */
if(isBedGraph)
    {
    char * tdbMin = cloneString(trackDbSettingClosestToHomeOrDefault(tdb, MIN_LIMIT, "NONE"));
    char * tdbMax = cloneString(trackDbSettingClosestToHomeOrDefault(tdb, MAX_LIMIT, "NONE"));
    if (sameWord("NONE",tdbMin))
        {
        if (settingsMin != (char *)NULL)
            absMinY = sqlDouble(settingsMin);
        }
    else
        absMinY = sqlDouble(tdbMin);

    if (sameWord("NONE",tdbMax))
        {
        if (settingsMax != (char *)NULL)
            absMaxY = sqlDouble(settingsMax);
        }
    else
        absMaxY = sqlDouble(tdbMax);
    freeMem(tdbMax);
    freeMem(tdbMin);
    freeMem(settingsMax);
    freeMem(settingsMin);
    }
else
    {
    /*	Let's see what trackDb has to say about these things,
    *	these words come from the trackDb.ra type wig line:
    *	type wig <min> <max>
    */
    double diff = abs(maxY - minY);
    absMinY = minY - diff*0.10;
    absMaxY = maxY + diff*0.10;
    switch (wordCount)
        {
        case 3:
            absMaxY = atof(words[2]);
	    // Fall through
        case 2:
            absMinY = atof(words[1]);
            break;
        default:
            break;
        }
    }
correctOrder(absMinY,absMaxY);
/*	return absolut min/max if OK to do that	*/
if (absMin)
    *absMin = absMinY;
if (absMax)
    *absMax = absMaxY;


// One more step: If we are loading settings for a composite view, then look for overrides
wigMaxLimitsCompositeOverride(tdb,name,min,max,absMin,absMax);

/*	And finally, let's see if values are available in the cart */
minY_str = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, MIN_Y);
maxY_str = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, MAX_Y);

if (minY_str && maxY_str)	/*	if specified in the cart */
    {
    *min = max( minY, atof(minY_str));
    *max = min( maxY, atof(maxY_str));
    correctOrder(*min,*max);
    }

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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *heightPer = NULL; /*	string from cart	*/
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int defaultHeightPixels = maxHeightPixels;
int defaultHeight;      /*      truncated by limits     */
int minHeightPixels = MIN_HEIGHT_PER;
char * tdbDefault = cloneString(
    trackDbSettingClosestToHomeOrDefault(tdb, MAXHEIGHTPIXELS, DEFAULT_HEIGHT_PER) );

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
heightPer = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, HEIGHTPER);
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
char *tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, tdbString, "NONE");
char *ret;

ret = Default;	/* the answer, unless found to be otherwise	*/

if (sameWord("NONE",tdbDefault) && (secondTdbString != (char *)NULL))
    tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, secondTdbString, "NONE");

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

/*	transformFunc - none by default **********************************/
enum wiggleGridOptEnum wigFetchTransformFuncWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *transformFunc;
enum wiggleTransformFuncEnum ret = wiggleTransformFuncNone;
char * tdbDefault = trackDbSettingClosestToHome(tdb, TRANSFORMFUNC);

transformFunc = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, TRANSFORMFUNC));

if ((transformFunc == NULL) && (tdbDefault != NULL))
    transformFunc = cloneString(tdbDefault);

if (optString && transformFunc)
    *optString = cloneString(transformFunc);

if (transformFunc)
    {
    ret = wiggleTransformFuncToEnum(transformFunc);
    freeMem(transformFunc);
    }
return(ret);
}

/*	alwaysZero - off by default **********************************/
enum wiggleGridOptEnum wigFetchAlwaysZeroWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *alwaysZero;
enum wiggleAlwaysZeroEnum ret = wiggleAlwaysZeroOff;
char * tdbDefault = trackDbSettingClosestToHome(tdb, ALWAYSZERO);

alwaysZero = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, ALWAYSZERO));

if ((alwaysZero == NULL) && (tdbDefault != NULL))
    alwaysZero = cloneString(tdbDefault);

if (optString && alwaysZero)
    *optString = cloneString(alwaysZero);

if (alwaysZero)
    {
    ret = wiggleAlwaysZeroToEnum(alwaysZero);
    freeMem(alwaysZero);
    }
return(ret);
}

/*	horizontalGrid - off by default **********************************/
enum wiggleGridOptEnum wigFetchHorizontalGridWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
{
char *Default = wiggleGridEnumToString(wiggleHorizontalGridOff);
char *notDefault = wiggleGridEnumToString(wiggleHorizontalGridOn);
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *horizontalGrid = NULL;
enum wiggleGridOptEnum ret;

horizontalGrid = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, HORIZGRID));

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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *autoScale = NULL;
enum wiggleScaleOptEnum ret;


autoScale = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, AUTOSCALE));

if (!autoScale)	/*	if nothing from the Cart, check trackDb/settings */
    {
    /*	It may be the autoScale=on/off situation from custom tracks */
    char * tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, AUTOSCALE, "NONE");
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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *graphType = NULL;
enum wiggleGraphOptEnum ret;

graphType = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, LINEBAR));

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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *windowingFunction = NULL;
enum wiggleWindowingEnum ret;

windowingFunction = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, WINDOWINGFUNCTION));

/*	If windowingFunction is a string, it came from the cart, otherwise
 *	see if it is specified in the trackDb option, finally
 *	return the default.
 */
if (!windowingFunction)
    {
    char * tdbDefault =
	trackDbSettingClosestToHomeOrDefault(tdb, WINDOWINGFUNCTION, Default);

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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char * smoothingWindow = NULL;
enum wiggleSmoothingEnum ret;

smoothingWindow = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, SMOOTHINGWINDOW));

if (!smoothingWindow) /* if nothing from the Cart, check trackDb/settings */
    {
    char * tdbDefault =
	trackDbSettingClosestToHomeOrDefault(tdb, SMOOTHINGWINDOW, Default);


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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *yLineMark = NULL;
enum wiggleYLineMarkEnum ret;

yLineMark = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, YLINEONOFF));

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
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *yLineMarkValue = NULL;  /*	string from cart	*/
double yLineValue;   /*	value from cart or trackDb */
char * tdbDefault = cloneString(
    trackDbSettingClosestToHomeOrDefault(tdb, YLINEMARK, "NONE") );

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
yLineMarkValue = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, YLINEMARK);

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
wigFetchMinMaxYWithCart(theCart,tdb,name,min,max,tDbMin,tDbMax,0,NULL);
}	/*	void wigFetchMinMaxYWithCart()	*/
