/* binTest - test bin functionality */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "binRange.h"

static char const rcsid[] = "$Id: binTest.c,v 1.1 2006/02/09 18:28:06 hiram Exp $";

boolean newStyle = FALSE; /* if not specified, will remain in old style range*/
boolean oldStyle = FALSE; /* if not specified, will remain in old style range*/

static struct optionSpec optionSpecs[] = {
    {"newStyle", OPTION_BOOLEAN},
    {"oldStyle", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort("usage: binTest [options]\n"
"options:\n"
"\t-newStyle - exercise expanded bin scheme over 512M\n"
"\t-oldStyle - exercise standard bin scheme up to 512M");
}

static void testOne(int start, int end, int expected)
{
int bin;

bin = binFromRange(start,end);
if (expected != bin)
    verbose(1,"#\tFAILED %d != %d = binFromRange(%d, %d)\n",
	expected, bin, start, end);
verbose(2,"#\t%5d = binFromRange(%d, %d)\n", bin, start, end);
}

static void standardBinScheme()
{
int maxEnd = 512*1024*1024;
int end;
int start;
int bin;
int binSize;
int expect;
int binCount = 0;
int level = 0;

/*	first check full range of scheme, should be bin 0	*/
verbose(2,"#\ttop level, whole chromosome\n");
start = 0; end = maxEnd; expect = 0;
testOne(start,end, expect);
binCount += 1;

for (level = 1; level < 5; ++level)
    {
    /*	first level of 8 bins	*/
    verbose(2,"#\tlevel %d of %d bins\n", level, 1<<(level*3));
    binSize = maxEnd >> (3*level);
    for (start = 0; start < maxEnd; start += binSize << ((level-1)*3) )
    {
	expect = (start / binSize) + binCount;
	end = start + binSize;
	testOne(start,end, expect);
    }
    if (level > 1)
	{
	/*	plus the last one at this level (when more than 8)	*/
	start = maxEnd - binSize;
	end = start + binSize;
	expect = (start / binSize) + binCount;
	testOne(start,end, expect);
	}
    binCount += 1<<(level*3);
    }

binCount = 9;

verbose(2,"#\tsecond level of sixty four bins\n");
/*	next level of 64 bins, test only eight of them	*/
binSize = maxEnd >> 6;
for (start = 0; start < maxEnd; start += (binSize << 3))
{
    expect = (start / binSize) + binCount;
    end = start + binSize;
    testOne(start,end, expect);
}
/*	plus the last one at this level	*/
start = maxEnd - binSize;
end = start + binSize;
expect = (start / binSize) + binCount;
testOne(start,end, expect);
binCount += 64;

verbose(2,"#\tthird level of 512 bins\n");
/*	next level of 512 bins, test only eight of them	*/
binSize = maxEnd >> 9;
for (start = 0; start < maxEnd; start += (binSize << 6))
{
    expect = (start / binSize) + binCount;
    end = start + binSize;
    testOne(start,end, expect);
}
/*	plus the last one at this level	*/
start = maxEnd - binSize;
end = start + binSize;
expect = (start / binSize) + binCount;
testOne(start,end, expect);
binCount += 512;

verbose(2,"#\tthird level of %d bins\n", 512*8);
/*	next level of 512*8 = 4096 bins, test only eight of them	*/
binSize = maxEnd >> 12;
for (start = 0; start < maxEnd; start += (binSize << 9))
{
    expect = (start / binSize) + binCount;
    end = start + binSize;
    testOne(start,end, expect);
}
/*	plus the last one at this level	*/
start = maxEnd - binSize;
end = start + binSize;
expect = (start / binSize) + binCount;
testOne(start,end, expect);
binCount += 4096;


}	/*	static void standardBinScheme()	*/

static void expandedBinScheme()
{
verbose(2,"#\texercise expanded bin scheme\n");
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);

newStyle = optionExists("newStyle");
oldStyle = optionExists("oldStyle");

if ( ! (newStyle || oldStyle) )
    usage();

if (oldStyle)
    standardBinScheme();
if (newStyle)
    expandedBinScheme();

return 0;
}
