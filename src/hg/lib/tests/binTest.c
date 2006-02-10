/* binTest - test bin functionality */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "binRange.h"

static char const rcsid[] = "$Id: binTest.c,v 1.3 2006/02/10 18:15:05 hiram Exp $";

boolean newStyle = FALSE; /* if not specified, will remain in old style range*/
boolean oldStyle = FALSE; /* if not specified, will remain in old style range*/
boolean sqlOnly = FALSE; /* if not specified, will remain in old style range*/
boolean binOnly = FALSE; /* if not specified, will remain in old style range*/
int testStart = 0;	/*	for a single test of one coordinate range */
int testEnd = 0;	/*	for a single test of one coordinate range */

static struct optionSpec optionSpecs[] = {
    {"newStyle", OPTION_BOOLEAN},
    {"oldStyle", OPTION_BOOLEAN},
    {"sqlOnly", OPTION_BOOLEAN},
    {"binOnly", OPTION_BOOLEAN},
    {"start", OPTION_INT},
    {"end", OPTION_INT},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort("usage: binTest [options]\n"
"options:\n"
"\t-newStyle - exercise expanded bin scheme over 512M\n"
"\t-oldStyle - exercise standard bin scheme up to 512M\n"
"\t-sqlOnly  - exercise only the sql statements\n"
"\t-binOnly  - exercise only the bin scheme");
}

static void testOneSql(int start, int end)
{
struct dyString *sqlQuery;
sqlQuery = newDyString(1024);
hAddBinToQuery(start, end, sqlQuery);
verbose(2,"# (%d, %d):'%s'\n", start, end, sqlQuery->string);
freeDyString(&sqlQuery);
}

static void testOneBin(int start, int end, int expected)
/*	expected < 0 == do not check, do not know the answer */
{
int bin;

bin = binFromRange(start,end);
if ((expected >= 0) && (expected != bin))
    verbose(1,"#\tFAILED %d != %d = binFromRange(%d, %d)\n",
	expected, bin, start, end);
verbose(2,"#\t%5d = binFromRange(%d, %d)\n", bin, start, end);
}

static void standardBinScheme()
{
int maxEnd = 512*1024*1024;
int end;
int start;
int binSize;
int expect;
int binCount = 0;
int level = 0;

if (! sqlOnly)
    {
    /*	first check full range of scheme, should be bin 0	*/
    verbose(2,"#\ttop level, whole chromosome\n");
    start = 0; end = maxEnd; expect = 0;
    testOneBin(start,end, expect);
    binCount += 1;

    for (level = 1; level < 5; ++level)
	{
	verbose(2,"#\tlevel %d of %d bins\n", level, 1 << (level*3));
	binSize = maxEnd >> (level*3);
	for (start = 0; start < maxEnd; start += binSize << ((level-1)*3) )
	{
	    expect = (start / binSize) + binCount;
	    end = start + binSize;
	    testOneBin(start,end, expect);
	}
	if (level > 1)
	    {
	    /*	plus the last one at this level (when more than 8)	*/
	    start = maxEnd - binSize;
	    end = start + binSize;
	    expect = (start / binSize) + binCount;
	    testOneBin(start,end, expect);
	    }
	binCount += 1 << (level*3);
	}
    }

if (! binOnly)
    {
    /*	first check full range of scheme, should be bin 0	*/
    verbose(2,"#\ttop level, whole chromosome\n");
    start = 0; end = maxEnd;
    testOneSql(start,end);

    for (level = 1; level < 5; ++level)
	{
	verbose(2,"#\tlevel %d of %d bins\n", level, 1 << (level*3));
	binSize = maxEnd >> (level*3);
	for (start = 0; start < maxEnd; start += binSize << ((level-1)*3) )
	{
	    end = start + binSize;
	    testOneSql(start,end);
	}
	if (level > 1)
	    {
	    /*	plus the last one at this level (when more than 8)	*/
	    start = maxEnd - binSize;
	    end = start + binSize;
	    testOneSql(start,end);
	    }
	}
    }

}	/*	static void standardBinScheme()	*/

static void expandedBinScheme()
{
int standardMaxEnd = 512*1024*1024;
long long actualMaxEnd = 8LL*512LL*1024LL*1024LL;
/* this is 2Gb,	can not actually do 4 Gb */
int practicalMaxEnd = 0;
int end;
int start;
long long binSize;
int expect;
int binCount = 0;
int level = 0;
int extendedOffset = 4681;

practicalMaxEnd = (int)((actualMaxEnd >> 1) - 1L);
verbose(2,"#\tpracticalMaxEnd: %d\n", practicalMaxEnd);
binSize = actualMaxEnd;

if (!sqlOnly)
    {
    /*	first check full range of new scheme, should be bin extendedOffset */
    verbose(2,"#\ttop level, whole chromosome, binSize: %lld\n", binSize);
    start = 0; end = practicalMaxEnd;
    expect = extendedOffset;
    testOneBin(start,end, expect);
    binCount += 1;

    for (level = 1; level < 6; ++level)
	{
	binSize = actualMaxEnd >> (level*3);
	verbose(2,"#\tlevel %d of %d bins, binSize: %lld\n",
	    level, 1 << (level*3), binSize);
	for (start = standardMaxEnd; (start > 0) &&
	    (start < (int)((long long)practicalMaxEnd-binSize));
		start += binSize << ((level-1)*3) )
	{
	    expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	    end = start + binSize;
	    if (end >= practicalMaxEnd) break;
	    testOneBin(start,end, expect);
	}
	/*	plus the last one at this level */
	start = practicalMaxEnd - binSize + 1;
	end = practicalMaxEnd;
	expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	testOneBin(start,end, expect);
	binCount += 1 << (level*3);
	}
    }

if (!binOnly)
    {
    binCount = 0;
    binSize = actualMaxEnd;
    /*	first check full range of new scheme, should be bin extendedOffset */
    verbose(2,"#\ttop level, whole chromosome, binSize: %lld\n", binSize);
    start = 0; end = practicalMaxEnd;
    expect = extendedOffset;
    testOneSql(start,end);
    binCount += 1;

    for (level = 1; level < 3; ++level)
	{
	binSize = actualMaxEnd >> (level*3);
	verbose(2,"#\tlevel %d of %d bins, binSize: %lld\n",
	    level, 1 << (level*3), binSize);
	for (start = standardMaxEnd; (start > 0) &&
	    (start < (int)((long long)practicalMaxEnd-binSize));
		start += binSize << ((level-1)*3) )
	{
	    expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	    end = start + binSize;
	    if (end >= practicalMaxEnd) break;
	    testOneSql(start,end);
	}
	/*	plus the last one at this level */
	start = practicalMaxEnd - binSize + 1;
	end = practicalMaxEnd;
	expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	testOneSql(start,end);
	binCount += 1 << (level*3);
	}
    }

}	/*	static void expandedBinScheme()	*/

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);

newStyle = optionExists("newStyle");
oldStyle = optionExists("oldStyle");
sqlOnly = optionExists("sqlOnly");
binOnly = optionExists("binOnly");

testStart = optionInt("start",0);
testEnd = optionInt("end",0);

if ( ! (newStyle || oldStyle || (testEnd > 0)) )
    usage();

if (testEnd > 0)
{
if (!sqlOnly)
    testOneBin(testStart, testEnd, -1);
if (!binOnly)
    testOneSql(testStart, testEnd);
}

if (oldStyle)
    standardBinScheme();
if (newStyle)
    expandedBinScheme();

return 0;
}
