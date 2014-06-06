/* binTest - test bin functionality */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "binRange.h"


static boolean newStyle = FALSE; /* check new style range*/
static boolean oldStyle = FALSE; /* check old style range*/
static boolean sqlOnly = FALSE; /* only check SQL statements */
static boolean binOnly = FALSE; /* check only binRange results */
static int testStart = 0;	/* for a single test of one coordinate range */
static int testEnd = 0;	/*	for a single test of one coordinate range */

static int failureCount = 0;	/*	cumulative count of all errors */

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
"\t-binOnly  - exercise only the bin scheme\n"
"\t-start=N -end=M - single check of coordinates N-M\n"
"\t-verbose=2 - display errors if any\n"
"\t-verbose=3 - more information on tests being performed");
}

static char *expectedOldSql[] = {
"(bin>=585 and bin<=4680 or bin>=73 and bin<=584 or bin>=9 and bin<=72 or bin>=1 and bin<=8 or bin=0 or bin=4681 ) and ",
"(bin>=585 and bin<=1096 or bin>=73 and bin<=136 or bin>=9 and bin<=16 or bin=1 or bin=0 or bin=4681 ) and ",
"(bin>=1097 and bin<=1608 or bin>=137 and bin<=200 or bin>=17 and bin<=24 or bin=2 or bin=0 or bin=4681 ) and ",
"(bin>=1609 and bin<=2120 or bin>=201 and bin<=264 or bin>=25 and bin<=32 or bin=3 or bin=0 or bin=4681 ) and ",
"(bin>=2121 and bin<=2632 or bin>=265 and bin<=328 or bin>=33 and bin<=40 or bin=4 or bin=0 or bin=4681 ) and ",
"(bin>=2633 and bin<=3144 or bin>=329 and bin<=392 or bin>=41 and bin<=48 or bin=5 or bin=0 or bin=4681 ) and ",
"(bin>=3145 and bin<=3656 or bin>=393 and bin<=456 or bin>=49 and bin<=56 or bin=6 or bin=0 or bin=4681 ) and ",
"(bin>=3657 and bin<=4168 or bin>=457 and bin<=520 or bin>=57 and bin<=64 or bin=7 or bin=0 or bin=4681 ) and ",
"(bin>=4169 and bin<=4680 or bin>=521 and bin<=584 or bin>=65 and bin<=72 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin>=585 and bin<=648 or bin>=73 and bin<=80 or bin=9 or bin=1 or bin=0 or bin=4681 ) and ",
"(bin>=1097 and bin<=1160 or bin>=137 and bin<=144 or bin=17 or bin=2 or bin=0 or bin=4681 ) and ",
"(bin>=1609 and bin<=1672 or bin>=201 and bin<=208 or bin=25 or bin=3 or bin=0 or bin=4681 ) and ",
"(bin>=2121 and bin<=2184 or bin>=265 and bin<=272 or bin=33 or bin=4 or bin=0 or bin=4681 ) and ",
"(bin>=2633 and bin<=2696 or bin>=329 and bin<=336 or bin=41 or bin=5 or bin=0 or bin=4681 ) and ",
"(bin>=3145 and bin<=3208 or bin>=393 and bin<=400 or bin=49 or bin=6 or bin=0 or bin=4681 ) and ",
"(bin>=3657 and bin<=3720 or bin>=457 and bin<=464 or bin=57 or bin=7 or bin=0 or bin=4681 ) and ",
"(bin>=4169 and bin<=4232 or bin>=521 and bin<=528 or bin=65 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin>=4617 and bin<=4680 or bin>=577 and bin<=584 or bin=72 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin>=585 and bin<=592 or bin=73 or bin=9 or bin=1 or bin=0 or bin=4681 ) and ",
"(bin>=1097 and bin<=1104 or bin=137 or bin=17 or bin=2 or bin=0 or bin=4681 ) and ",
"(bin>=1609 and bin<=1616 or bin=201 or bin=25 or bin=3 or bin=0 or bin=4681 ) and ",
"(bin>=2121 and bin<=2128 or bin=265 or bin=33 or bin=4 or bin=0 or bin=4681 ) and ",
"(bin>=2633 and bin<=2640 or bin=329 or bin=41 or bin=5 or bin=0 or bin=4681 ) and ",
"(bin>=3145 and bin<=3152 or bin=393 or bin=49 or bin=6 or bin=0 or bin=4681 ) and ",
"(bin>=3657 and bin<=3664 or bin=457 or bin=57 or bin=7 or bin=0 or bin=4681 ) and ",
"(bin>=4169 and bin<=4176 or bin=521 or bin=65 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin>=4673 and bin<=4680 or bin=584 or bin=72 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin=585 or bin=73 or bin=9 or bin=1 or bin=0 or bin=4681 ) and ",
"(bin=1097 or bin=137 or bin=17 or bin=2 or bin=0 or bin=4681 ) and ",
"(bin=1609 or bin=201 or bin=25 or bin=3 or bin=0 or bin=4681 ) and ",
"(bin=2121 or bin=265 or bin=33 or bin=4 or bin=0 or bin=4681 ) and ",
"(bin=2633 or bin=329 or bin=41 or bin=5 or bin=0 or bin=4681 ) and ",
"(bin=3145 or bin=393 or bin=49 or bin=6 or bin=0 or bin=4681 ) and ",
"(bin=3657 or bin=457 or bin=57 or bin=7 or bin=0 or bin=4681 ) and ",
"(bin=4169 or bin=521 or bin=65 or bin=8 or bin=0 or bin=4681 ) and ",
"(bin=4680 or bin=584 or bin=72 or bin=8 or bin=0 or bin=4681 ) and "
};

static char *expectedNewSql[] = {
"(bin>=585 and bin<=4680 or bin>=73 and bin<=584 or bin>=9 and bin<=72 or bin>=1 and bin<=8 or bin=0 or bin>=9362 and bin<=25745 or bin>=5266 and bin<=7313 or bin>=4754 and bin<=5009 or bin>=4690 and bin<=4721 or bin>=4682 and bin<=4685 or bin=4681) and ",
"(bin>=13458 and bin<=17553 or bin>=5778 and bin<=6289 or bin>=4818 and bin<=4881 or bin>=4698 and bin<=4705 or bin=4683 or bin=4681) and ",
"(bin>=17554 and bin<=21649 or bin>=6290 and bin<=6801 or bin>=4882 and bin<=4945 or bin>=4706 and bin<=4713 or bin=4684 or bin=4681) and ",
"(bin>=21650 and bin<=25745 or bin>=6802 and bin<=7313 or bin>=4946 and bin<=5009 or bin>=4714 and bin<=4721 or bin=4685 or bin=4681) and ",
"(bin>=13458 and bin<=13969 or bin>=5778 and bin<=5841 or bin>=4818 and bin<=4825 or bin=4698 or bin=4683 or bin=4681) and ",
"(bin>=17554 and bin<=18065 or bin>=6290 and bin<=6353 or bin>=4882 and bin<=4889 or bin=4706 or bin=4684 or bin=4681) and ",
"(bin>=21650 and bin<=22161 or bin>=6802 and bin<=6865 or bin>=4946 and bin<=4953 or bin=4714 or bin=4685 or bin=4681) and ",
"(bin>=25234 and bin<=25745 or bin>=7250 and bin<=7313 or bin>=5002 and bin<=5009 or bin=4721 or bin=4685 or bin=4681) and ",
"(bin>=13458 and bin<=13521 or bin>=5778 and bin<=5785 or bin=4818 or bin=4698 or bin=4683 or bin=4681) and ",
"(bin>=17554 and bin<=17617 or bin>=6290 and bin<=6297 or bin=4882 or bin=4706 or bin=4684 or bin=4681) and ",
"(bin>=21650 and bin<=21713 or bin>=6802 and bin<=6809 or bin=4946 or bin=4714 or bin=4685 or bin=4681) and ",
"(bin>=25682 and bin<=25745 or bin>=7306 and bin<=7313 or bin=5009 or bin=4721 or bin=4685 or bin=4681) and ",
"(bin>=13458 and bin<=13465 or bin=5778 or bin=4818 or bin=4698 or bin=4683 or bin=4681) and ",
"(bin>=17554 and bin<=17561 or bin=6290 or bin=4882 or bin=4706 or bin=4684 or bin=4681) and ",
"(bin>=21650 and bin<=21657 or bin=6802 or bin=4946 or bin=4714 or bin=4685 or bin=4681) and ",
"(bin>=25738 and bin<=25745 or bin=7313 or bin=5009 or bin=4721 or bin=4685 or bin=4681) and ",
"(bin=13458 or bin=5778 or bin=4818 or bin=4698 or bin=4683 or bin=4681) and ",
"(bin=17554 or bin=6290 or bin=4882 or bin=4706 or bin=4684 or bin=4681) and ",
"(bin=21650 or bin=6802 or bin=4946 or bin=4714 or bin=4685 or bin=4681) and ",
"(bin=25745 or bin=7313 or bin=5009 or bin=4721 or bin=4685 or bin=4681) and "
};

static void testOneSql(int start, int end, char *expected)
{
struct dyString *sqlQuery;
sqlQuery = newDyString(1024);
hAddBinToQuery(start, end, sqlQuery);
if (NULL != expected)
    if (differentString(sqlQuery->string,expected))
	{
	verbose(2,"#\tERROR: SQL incorrect at (%d, %d)\n",
	    start, end);
	++failureCount;
	}

verbose(3,"# (%d, %d):\nsql:\"%s\",\n", start, end, sqlQuery->string);
freeDyString(&sqlQuery);
}

static void testOneBin(int start, int end, int expected)
/*	expected < 0 == do not check, do not know the answer */
{
int bin;

bin = binFromRange(start,end);
if ((expected >= 0) && (expected != bin))
    {
    verbose(2,"#\tERROR: expected: %d got: %d = binFromRange(%d, %d)\n",
	expected, bin, start, end);
    ++failureCount;
    }
verbose(3,"#\t%5d = binFromRange(%d, %d)\n", bin, start, end);
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
    verbose(3,"#\ttop level, whole chromosome\n");
    start = 0; end = maxEnd; expect = 0;
    testOneBin(start,end, expect);
    binCount += 1;

    for (level = 1; level < 5; ++level)
	{
	verbose(3,"#\tlevel %d of %d bins\n", level, 1 << (level*3));
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
    int testCount = 0;

    /*	first check full range of scheme, should be bin 0	*/
    verbose(3,"#\ttop level, whole chromosome\n");
    start = 0; end = maxEnd;
    testOneSql(start,end, expectedOldSql[testCount++]);

    for (level = 1; level < 5; ++level)
	{
	verbose(3,"#\tlevel %d of %d bins\n", level, 1 << (level*3));
	binSize = maxEnd >> (level*3);
	for (start = 0; start < maxEnd; start += binSize << ((level-1)*3) )
	{
	    end = start + binSize;
	    testOneSql(start,end, expectedOldSql[testCount++]);
	}
	if (level > 1)
	    {
	    /*	plus the last one at this level (when more than 8)	*/
	    start = maxEnd - binSize;
	    end = start + binSize;
	    testOneSql(start,end, expectedOldSql[testCount++]);
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
verbose(3,"#\tpracticalMaxEnd: %d\n", practicalMaxEnd);
binSize = actualMaxEnd;

if (!sqlOnly)
    {
    /*	first check full range of new scheme, should be bin extendedOffset */
    verbose(3,"#\ttop level, whole chromosome, binSize: %lld\n", binSize);
    start = 0; end = practicalMaxEnd;
    expect = extendedOffset;
    testOneBin(start,end, expect);
    binCount += 1;

    for (level = 1; level < 6; ++level)
	{
	binSize = actualMaxEnd >> (level*3);
	verbose(3,"#\tlevel %d of %d bins, binSize: %lld\n",
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
    int testCount = 0;

    binCount = 0;
    binSize = actualMaxEnd;
    /*	first check full range of new scheme, should be bin extendedOffset */
    verbose(3,"#\ttop level, whole chromosome, binSize: %lld\n", binSize);
    start = 0; end = practicalMaxEnd;
    expect = extendedOffset;
    testOneSql(start,end, expectedNewSql[testCount++]);
    binCount += 1;

    for (level = 1; level < 6; ++level)
	{
	binSize = actualMaxEnd >> (level*3);
	verbose(3,"#\tlevel %d of %d bins, binSize: %lld\n",
	    level, 1 << (level*3), binSize);
	for (start = standardMaxEnd; (start > 0) &&
	    (start < (int)((long long)practicalMaxEnd-binSize));
		start += binSize << ((level-1)*3) )
	{
	    expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	    end = start + binSize;
	    if (end >= practicalMaxEnd) break;
	    testOneSql(start,end, expectedNewSql[testCount++]);
	}
	/*	plus the last one at this level */
	start = practicalMaxEnd - binSize + 1;
	end = practicalMaxEnd;
	expect = (int)((long long)start / binSize) + binCount + extendedOffset;
	testOneSql(start,end, expectedNewSql[testCount++]);
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
    testOneSql(testStart, testEnd, NULL);
}

if (oldStyle)
    standardBinScheme();
if (newStyle)
    expandedBinScheme();

if (failureCount > 0)
    {
    printf("ERROR: hg/lib/tests/binTest failed %d tests\n", failureCount);
    return 255;
    }
else
    return 0;
}
