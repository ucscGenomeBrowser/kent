/*	wigDataStream - an object to access wiggle data values from
 *	either a DB access or from a .wig text file (==custom track)
 */
#include "common.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wigDataStream.c,v 1.2 2004/08/04 22:01:34 hiram Exp $";

static void addConstraint(struct wiggleDataStream *wDS, char *left, char *right)
{
struct dyString *constrain = dyStringNew(256);
if (wDS->sqlConstraint)
    dyStringPrintf(constrain, "%s AND ", wDS->sqlConstraint);

dyStringPrintf(constrain, "%s \"%s\"", left, right);

freeMem(wDS->sqlConstraint);
wDS->sqlConstraint = cloneString(constrain->string);
dyStringFree(&constrain);
}

static void setPositionConstraint(struct wiggleDataStream *wDS,
	int winStart, int winEnd)
{
/*	keep them in proper order	*/
if (winStart > winEnd)
    {
    wDS->winStart = winEnd;
    wDS->winEnd = winStart;
    }
else
    {
    wDS->winStart = winStart;
    wDS->winEnd = winEnd;
    }
}

static void setChromConstraint(struct wiggleDataStream *wDS, char *chr)
{
addConstraint(wDS, "chrom =", chr);
freeMem(wDS->chrName);
wDS->chrName = cloneString(chr);
}

static void setSpanConstraint(struct wiggleDataStream *wDS, unsigned span)
{
struct dyString *dyTmp = dyStringNew(256);
dyStringPrintf(dyTmp, "%u", span);
addConstraint(wDS, "span =", dyTmp->string);
wDS->spanLimit = span;
dyStringFree(&dyTmp);
}

/*	the double comparison functions
 *	used to check the wiggle SQL rows which are a bucket of values
 *	between *lower and *upper.  Therefore, the value to be checked
 *	which is in wDS->limit_0 (and wDS->limit_1 in the case of
 *	a range) needs to be compared to the bucket of values.  If it
 *	falls within the specified range, then it is considered to be in
 *	that bucket.
 */
/* InRange means the SQL row begins before the limit_1 (lower<=limit_1)
 *	 and the row ends after the limit_0 (upper>=limit_0)
 *	i.e. there is at least some overlap of the range
 */
static boolean wigInRange_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((lower <= wDS->limit_1) && (upper >= wDS->limit_0));
}
/* LessThan means:  the row begins before the limit_0 (value<limit_0)
 *	i.e. there are data values below the specified limit_0
 */
static boolean wigLessThan_D(struct wiggleDataStream *wDS, double value,
	double dummy)
{
return (value < wDS->limit_0);
}
/* LessEqual means:  the row begins at or before the limit_0 (value<=limit_0)
 *	i.e. there are data values at or below the specified limit_0
 */
static boolean wigLessEqual_D(struct wiggleDataStream *wDS, double value,
	double dummy)
{
return (value <= wDS->limit_0);
}
/* Equal means:  similar to InRange, the test value limit_0 can be found
 * in the SQL row, i.e. lower <= limit_0 <= upper
 */
static boolean wigEqual_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((lower <= wDS->limit_0) && (wDS->limit_0 <= upper));
}
/* NotEqual means:  the opposite of Equal, the test value limit_0 can not
 *	be found in the SQL row, i.e. (limit_0 < lower) or (upper < limit_0)
 */
static boolean wigNotEqual_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((wDS->limit_0 < lower) || (upper < wDS->limit_0));
}
/* GreaterEqual means:  the row ends at or after the limit_0 (limit_0<=upper)
 *	i.e. there are data values at or above the specified limit_0
 */
static boolean wigGreaterEqual_D(struct wiggleDataStream *wDS, double dummy,
	double upper)
{
return (wDS->limit_0 <= upper);
}
/* GreaterEqual means:  the row ends after the limit_0 (limit_0<upper)
 *	i.e. there are data values above the specified limit_0
 */
static boolean wigGreaterThan_D(struct wiggleDataStream *wDS, double dummy,
	double upper)
{
return (wDS->limit_0 < upper);
}
/*	the unsigned char comparison functions
 *	Unlike the above, these are straighforward, just compare the
 *	byte values
 */
static boolean wigInRange(struct wiggleDataStream *wDS, unsigned char *value)
{
return ((*value <= wDS->ucUpperLimit) && (*value >= wDS->ucLowerLimit));
}
static boolean wigLessThan(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value < wDS->ucLowerLimit);
}
static boolean wigLessEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value <= wDS->ucLowerLimit);
}
static boolean wigEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value == wDS->ucLowerLimit);
}
static boolean wigNotEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value != wDS->ucLowerLimit);
}
static boolean wigGreaterEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value >= wDS->ucLowerLimit);
}
static boolean wigGreaterThan(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value > wDS->ucLowerLimit);
}


static void wigSetCompareFunctions(struct wiggleDataStream *wDS)
{
if (!wDS->dataConstraint)
    return;

if (sameWord(wDS->dataConstraint,"<"))
    {
    wDS->cmpDouble = wigLessThan_D;
    wDS->cmpByte = wigLessThan;
    }
else if (sameWord(wDS->dataConstraint,"<="))
    {
    wDS->cmpDouble = wigLessEqual_D;
    wDS->cmpByte = wigLessEqual;
    }
else if (sameWord(wDS->dataConstraint,"="))
    {
    wDS->cmpDouble = wigEqual_D;
    wDS->cmpByte = wigEqual;
    }
else if (sameWord(wDS->dataConstraint,"!="))
    {
    wDS->cmpDouble = wigNotEqual_D;
    wDS->cmpByte = wigNotEqual;
    }
else if (sameWord(wDS->dataConstraint,">="))
    {
    wDS->cmpDouble = wigGreaterEqual_D;
    wDS->cmpByte = wigGreaterEqual;
    }
else if (sameWord(wDS->dataConstraint,">"))
    {
    wDS->cmpDouble = wigGreaterThan_D;
    wDS->cmpByte = wigGreaterThan;
    }
else if (sameWord(wDS->dataConstraint,"in range"))
    {
    wDS->cmpDouble = wigInRange_D;
    wDS->cmpByte = wigInRange;
    }
else
    errAbort("wigSetCompareFunctions: unknown constraint: '%s'",
	wDS->dataConstraint);
verbose(2, "#\twigSetCompareFunctions: set to '%s'\n", wDS->dataConstraint);
}

static void setDataConstraint(struct wiggleDataStream *wDS,
	char *dataConstraint, double lowerLimit, double upperLimit)
{
wDS->dataConstraint = cloneString(dataConstraint);
if (lowerLimit < upperLimit)
    {
    wDS->limit_0 = lowerLimit;
    wDS->limit_1 = upperLimit;
    }
else if (!(upperLimit < lowerLimit))
  errAbort("wigSetDataConstraint: upper and lower limits are equal: %.g == %.g",
	lowerLimit, upperLimit);
else
    {
    wDS->limit_0 = upperLimit;
    wDS->limit_1 = lowerLimit;
    }
wigSetCompareFunctions(wDS);
wDS->useDataConstraint = TRUE;
}

static void setCompareByte(struct wiggleDataStream *wDS,
	double lower, double range)
{
if (wDS->limit_0 < lower)
    wDS->ucLowerLimit = 0;
else
    wDS->ucLowerLimit = MAX_WIG_VALUE * ((wDS->limit_0 - lower)/range);
if (wDS->limit_1 > (lower+range))
    wDS->ucUpperLimit = MAX_WIG_VALUE;
else
    wDS->ucUpperLimit = MAX_WIG_VALUE * ((wDS->limit_1 - lower)/range);
verbose(2, "#\twigSetCompareByte: [%g : %g] becomes [%d : %d]\n",
	lower, lower+range, wDS->ucLowerLimit, wDS->ucUpperLimit);
}

static void closeWibFile(struct wiggleDataStream *wDS)
/*	if there is a Wib file open, close it	*/
{
if (wDS->wibFH > 0)
    close(wDS->wibFH);
wDS->wibFH = -1;
freez(&wDS->wibFile);
}

static void openWibFile(struct wiggleDataStream *wDS, char *file)
{
if (wDS->wibFile)
    {		/*	close and open only if different */
    if (differentString(wDS->wibFile,file))
	{
	if (wDS->wibFH > 0)
	    close(wDS->wibFH);
	freeMem(wDS->wibFile);
	wDS->wibFile = cloneString(file);
	wDS->wibFH = open(wDS->wibFile, O_RDONLY);
	if (wDS->wibFH == -1)
	    errAbort("openWibFile: failed to open %s", wDS->wibFile);
	}
    }
else
    {
    wDS->wibFile = cloneString(file);	/* first time */
    wDS->wibFH = open(wDS->wibFile, O_RDONLY);
    if (wDS->wibFH == -1)
	errAbort("openWibFile: failed to open %s", wDS->wibFile);
    }
}

static void closeWigConn(struct wiggleDataStream *wDS)
{
closeWibFile(wDS);	/*	closes only if it is open	*/
if (wDS->conn)
    {
    sqlFreeResult(&wDS->sr);
    sqlDisconnect(&wDS->conn);
    }
}

static void openWigConn(struct wiggleDataStream *wDS, char *tableName)
/*	open connection to db or to a file, prepare SQL result for db */
{
if (wDS->isFile)
    {
    struct dyString *fileName = dyStringNew(256);
    lineFileClose(&wDS->lf);	/*	possibly a previous file */
    dyStringPrintf(fileName, "%s.wig", tableName);
    wDS->lf = lineFileOpen(fileName->string, TRUE);
    dyStringFree(&fileName);
    }
else
    {
    struct dyString *query = dyStringNew(256);
    dyStringPrintf(query, "select * from %s", tableName);
    if (wDS->sqlConstraint)
	dyStringPrintf(query, " where (%s) order by chromStart",
	    wDS->sqlConstraint);
    verbose(2, "#\t%s\n", query->string);
    if (!wDS->conn)
	wDS->conn = sqlConnect(wDS->db);
    wDS->sr = sqlGetResult(wDS->conn,query->string);
    }
}

struct wiggleDataStream *newWigDataStream()
{
struct wiggleDataStream *wds;
AllocVar(wds);
/*	everything is zero which is good since that is NULL for all the
 *	strings and lists.  A few items should have some initial values
 *	which are not necessarily NULL
 */
wds->isFile = FALSE;
wds->useDataConstraint = FALSE;
wds->wibFH = -1;
wds->limit_0 = -1 * INFINITY;
wds->limit_1 = INFINITY;
/*	Set method pointers	*/
wds->setPositionConstraint = setPositionConstraint;
wds->setChromConstraint = setChromConstraint;
wds->setSpanConstraint = setSpanConstraint;
wds->setDataConstraint = setDataConstraint;
wds->setCompareByte = setCompareByte;
wds->openWibFile = openWibFile;
wds->closeWigConn = closeWigConn;
wds->openWigConn = openWigConn;
return wds;
}

