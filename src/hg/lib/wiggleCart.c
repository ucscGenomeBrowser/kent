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

static char const rcsid[] = "$Id: wiggleCart.c,v 1.39 2010/05/11 01:43:30 kent Exp $";

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
snprintf(dbgMsg, DBGMSGSZ, "%s pixels: min,default,max: %d:%d:%d", tdb->track, wigCart->minHeight, wigCart->defaultHeight, wigCart->maxHeight);
wigDebugPrint("wigFetch");
#endif

#endif

static void parseColonRange(char *setting, double *retMin, double *retMax)
/* Parse setting's two colon-separated numbers into ret{Min,Max}, unless setting
 * is NULL or empty or retMin/retMax is NULL.  errAbort if invalid format. */
{
if (isNotEmpty(setting))
    {
    char tmp[64]; // Intentionally small -- should be only 2 floating point #s + ':'
    safecpy(tmp, sizeof(tmp), setting);
    char *words[3];
    if (chopByChar(tmp, ':', words, ArraySize(words)) == 2)
        {
	double low = sqlDouble(words[0]);
        double high = sqlDouble(words[1]);
        correctOrder(low, high);
	if (retMin)
	    *retMin = low;
	if (retMax)
	    *retMax = high;
        }
    else
	errAbort("Can't parse colon range '%s'", setting);
    }
}

static void viewLimitsCompositeOverride(struct trackDb *tdb,char *name,
                      double *retMin, double *retMax,double *absMin, double *absMax)
/* If aquiring min/max for composite level wig cfg. Look in parents as well as self. */
{
if(isNameAtCompositeLevel(tdb,name))
    {
    char *setting = NULL;
    if(absMin != NULL && absMax != NULL)
        {
        setting = trackDbSettingByView(tdb,MIN_LIMIT);
        if(setting != NULL)
            {
            if(setting[0] != '\0')
                *absMin = sqlDouble(setting);
            }
        setting = trackDbSettingByView(tdb,MAX_LIMIT);
        if(setting != NULL)
            {
            if(setting[0] != '\0')
                *absMax = sqlDouble(setting);
            }
        else
            {
            setting = trackDbSettingByView(tdb,VIEWLIMITSMAX);  // Legacy
            if(setting != NULL)
                {
                parseColonRange(setting, absMin, absMax);
                }
            }
        }

    setting = trackDbSettingByView(tdb, VIEWLIMITS);
    if(setting != NULL)
        {
        parseColonRange(setting, retMin, retMax);
        }
    }
}

void wigFetchMinMaxYWithCart(struct cart *theCart, struct trackDb *tdb, char *name,
			     double *retMin, double *retMax, double *retAbsMin, double *retAbsMax,
			     int wordCount, char **words)
/*****************************************************************************
 *	Min, Max Y viewing limits
 *	Absolute limits are defined on the trackDb type line for wiggle,
 *	or MIN_LIMIT / MAX_LIMIT trackDb settings for bedGraph
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits or viewLimits declaration from trackDb
 *****************************************************************************/
{
boolean isBedGraph = (wordCount == 0 || sameString(words[0],"bedGraph"));
// Determine absolute min and max.  Someday hgTrackDb will enforce inclusion of data
// range settings, but until then, there is some circular logic where either one
// can provide a default for the other if the other is missing.
double absMax = 0.0, absMin = 0.0;
boolean missingAbsMin = FALSE, missingAbsMax = FALSE;
if (isBedGraph)
    {
    char *tdbMin = trackDbSettingClosestToHomeOrDefault(tdb, MIN_LIMIT, NULL);
    char *tdbMax = trackDbSettingClosestToHomeOrDefault(tdb, MAX_LIMIT, NULL);
    if (tdbMin == NULL)
	missingAbsMin = TRUE;
    else
	absMin = sqlDouble(tdbMin);
    if (tdbMax == NULL)
	missingAbsMax = TRUE;
    else
	absMax = sqlDouble(tdbMax);
    }
else
    {
    // Wiggle: get min and max from type setting, which has been chopped into words and wordCount:
    // type wig <min> <max>
    if (wordCount >= 3)
	absMax = atof(words[2]);
    else
	missingAbsMax = TRUE;
    if (wordCount >= 2)
	absMin = atof(words[1]);
    else
	missingAbsMin = TRUE;
    }
correctOrder(absMin, absMax);

// Determine current minY,maxY.
// Precedence:  1. cart;  2. defaultViewLimits;  3. viewLimits;
//              4. absolute [which may need to default to #2 or #3!]
boolean compositeLevel = isNameAtCompositeLevel(tdb, name);
char *cartMinStr = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, MIN_Y);
char *cartMaxStr = cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, MAX_Y);
double cartMin = 0.0, cartMax = 0.0;

if(cartMinStr)
    *retMin = atof(cartMinStr);
if(cartMaxStr)
    *retMax = atof(cartMaxStr);
if (cartMinStr && cartMaxStr)
    correctOrder(*retMin, *retMax);
// If it weren't for the the allowance for missing data range values,
// we could set retAbs* and be done here.
if(cartMinStr)
    cartMin = *retMin;
if(cartMaxStr)
    cartMax = *retMax;

// Get trackDb defaults, and resolve missing wiggle data range if necessary.
char *defaultViewLimits = trackDbSettingClosestToHomeOrDefault(tdb, DEFAULTVIEWLIMITS, NULL);
if (defaultViewLimits == NULL)
    defaultViewLimits = trackDbSettingClosestToHomeOrDefault(tdb, VIEWLIMITS, NULL);
if (defaultViewLimits != NULL)
    {
    double viewLimitMin = 0.0, viewLimitMax = 0.0;
    parseColonRange(defaultViewLimits, &viewLimitMin, &viewLimitMax);
    *retMin = viewLimitMin;
    *retMax = viewLimitMax;
    if (missingAbsMax)
	absMax = viewLimitMax;
    if (missingAbsMin)
	absMin = viewLimitMin;
    }
else if (missingAbsMin || missingAbsMax)
    {
    // I would like to make this an errAbort, but too many tracks are afflicted
    // to do that until hgTrackDb helps to enforce:
    if (hIsPrivateHost())
	warn("trackDb %s, and no default view limits are specified for track %s",
	     (isBedGraph ? MIN_LIMIT " and/or " MAX_LIMIT " is not specified" :
			   "'type wig' line is missing min and/or max data value"),
	     tdb->track);
    // When that becomes an errAbort, remove these defines from wiggle.h:
    if (isBedGraph)
	{
	absMin = DEFAULT_MIN_BED_GRAPH;
	absMax = DEFAULT_MAX_BED_GRAPH;
	}
    else
	{
	absMin = DEFAULT_MIN_Yv;
	absMax = DEFAULT_MAX_Yv;
	}
    *retMin = absMin;
    *retMax = absMax;
    }
else
    {
    *retMin = absMin;
    *retMax = absMax;
    }

if (retAbsMin)
    *retAbsMin = absMin;
if (retAbsMax)
    *retAbsMax = absMax;
// After the dust settles from tdb's trackDb settings, now see if composite view
// settings from tdb's parents override that stuff anyway:
viewLimitsCompositeOverride(tdb, name, retMin, retMax, retAbsMin, retAbsMax);

// And as the final word after composite override, reset retMin and retMax if from cart:
if (cartMinStr)
    *retMin = cartMin;
if (cartMaxStr)
    *retMax = cartMax;
}	/*	void wigFetchMinMaxYWithCart()	*/

void wigFetchMinMaxPixelsWithCart(struct cart *theCart, struct trackDb *tdb, char *name,int *Min, int *Max, int *Default)
/*	Min, Max, Default Pixel height of track
 *	Limits may be defined in trackDb with the maxHeightPixels string,
 *	Or user requested limits are defined in the cart.
 *	And default opening display limits may optionally be defined with the
 *		maxHeightPixels declaration from trackDb
 *****************************************************************************/
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

static char *wigCheckBinaryOption(struct trackDb *tdb, char *Default,
    char *notDefault, char *tdbString, char *secondTdbString)
/*	A common operation for binary options (two values possible)
 *	check for trackDb.ra, then tdb->settings values
 *	return one of the two possibilities if found
 *	(the tdbString and secondTdbString are a result of
 *		early naming conventions changing over time resulting in
 *		two possible names for the same thing ...)
 */
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

enum wiggleGridOptEnum wigFetchTransformFuncWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
/*	transformFunc - none by default **********************************/
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

enum wiggleGridOptEnum wigFetchAlwaysZeroWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
/*	alwaysZero - off by default **********************************/
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

enum wiggleGridOptEnum wigFetchHorizontalGridWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name,char **optString)
/*	horizontalGrid - off by default **********************************/
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

enum wiggleScaleOptEnum wigFetchAutoScaleWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name, char **optString)
/******	autoScale - off by default ***************************************/
{
char *autoString = wiggleScaleEnumToString(wiggleScaleAuto);
char *manualString = wiggleScaleEnumToString(wiggleScaleManual);
char *Default = manualString;
char *notDefault = autoString;
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *autoScale = NULL;
enum wiggleScaleOptEnum ret;


autoScale = cloneString(cartOptionalStringClosestToHome(theCart, tdb, compositeLevel, AUTOSCALE));

if (!autoScale)	/*	if nothing from the Cart, check trackDb/settings */
    {
    /*	It may be the autoScale=on/off situation from custom tracks */
    char * tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, AUTOSCALE, "NONE");
    if (sameWord(tdbDefault,"on"))
	autoScale = cloneString(autoString);
    else if (sameWord(tdbDefault,"off"))
	autoScale = cloneString(manualString);
    else
	{
	if (isCustomTrack(tdb->track))
	    // backwards defaults for custom tracks, autoScale on
	    autoScale = wigCheckBinaryOption(tdb,notDefault,Default,
		AUTOSCALEDEFAULT, AUTOSCALE);
	else
	    autoScale = wigCheckBinaryOption(tdb,Default,notDefault,
		AUTOSCALEDEFAULT, AUTOSCALE);
	}
    }

if (optString)
    *optString = cloneString(autoScale);

ret = wiggleScaleStringToEnum(autoScale);
freeMem(autoScale);
return(ret);
}	/*	enum wiggleScaleOptEnum wigFetchAutoScaleWithCart()	*/

enum wiggleGraphOptEnum wigFetchGraphTypeWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name, char **optString)
/******	graphType - line(points) or bar graph *****************************/
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

enum wiggleWindowingEnum wigFetchWindowingFunctionWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name, char **optString)
/******	windowingFunction - Whiskers by default **************************/
{
char *Default = wiggleWindowingEnumToString(wiggleWindowingWhiskers);
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

enum wiggleSmoothingEnum wigFetchSmoothingWindowWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name, char **optString)
/******	smoothingWindow - OFF by default **************************/
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

enum wiggleYLineMarkEnum wigFetchYLineMarkWithCart(struct cart *theCart,
    struct trackDb *tdb, char *name, char **optString)
/*	yLineMark - off by default **********************************/
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

void wigFetchYLineMarkValueWithCart(struct cart *theCart,struct trackDb *tdb, char *name, double *tDbYMark )
/*	y= marker line value
 *	User requested value is defined in the cart
 *	A Default value can be defined as
 *		yLineMark declaration from trackDb
 *****************************************************************************/
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

void wigFetchMinMaxLimitsWithCart(struct cart *theCart, struct trackDb *tdb, char *name,
    double *min, double *max,double *tDbMin, double *tDbMax)
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
{
wigFetchMinMaxYWithCart(theCart,tdb,name,min,max,tDbMin,tDbMax,0,NULL);
}	/*	void wigFetchMinMaxYWithCart()	*/

char *wigFetchAggregateValWithCart(struct cart *cart, struct trackDb *tdb)
/* Return aggregate value for track. */
{
return cartOrTdbString(cart, tdb, "aggregate", WIG_AGGREGATE_TRANSPARENT);
}

boolean wigIsOverlayTypeAggregate(char *aggregate)
/* Return TRUE if aggregater type is one of the overlay ones. */
{
if (aggregate == NULL)
    return FALSE;
return differentString(aggregate, WIG_AGGREGATE_NONE);
}

