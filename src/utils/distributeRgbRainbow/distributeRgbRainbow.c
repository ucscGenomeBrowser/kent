/* distributeRgbRainbow - Associate colors with items on a list where distance between colors 
 * is proportional to distance between items. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "pairDistance.h"
#include "rainbow.h"

double gStart = 0, gEnd = 0.85;   /* Global start/end and defaults */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "distributeRgbRainbow - Associate colors with items on a list where distance between colors \n"
  "is proportional to distance between items.\n"
  "usage:\n"
  "   distributeRgbRainbow in.lst in.pairs out.tab\n"
  "where:\n"
  "   in.lst has one identifier per line\n"
  "   in.pairs is whitespace separated with the columns <a> <b> <distance>\n"
  "      where a and b are items from in.lst\n"
  "   out.tab is tab-separated with columns <identifier> <pos> <r> <g> <b>\n"
  "      where identifier is from in.lst, pos is a number between 0 and 1, and\n"
  "      r,g,b are color components between 0 and 255\n"
  "options:\n"
  "   -start=0.N - a number between 0 and 1 that is rainbow start (default %g - red)\n"
  "   -end=0.N - a number between 0 and 1 that is rainbow end (default %g - purple)\n"
  "   -invert - deal with cases such as correlation for in.pairs where high numbers are proximite\n"
  , gStart, gEnd
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"start", OPTION_DOUBLE},
   {"end", OPTION_DOUBLE},
   {"invert", OPTION_BOOLEAN},
   {NULL, 0},
};

void outputItem(FILE *f, char *name, double colorPos)
/* Print out one item */
{
struct rgbColor c = saturatedRainbowAtPos(colorPos);
fprintf(f, "%s\t%g\t%d\t%d\t%d\n", name, colorPos, c.r, c.g, c.b);
}

void distributeRgbRainbow(char *inList, char *inPairs, char *outTab)
/* distributeRgbRainbow - Associate colors with items on a list where distance between colors 
 * is proportional to distance between items. */
{
/* Read in inputs */
struct slName *item, *itemList = readAllLines(inList);
verbose(1, "%d items in %s\n", slCount(itemList), inList);
struct pairDistance *pairList= pairDistanceReadAll(inPairs);
verbose(1, "%d pairs in %s\n", slCount(pairList), inPairs);
struct hash *pairHash = pairDistanceHashList(pairList);

/* Open output, and just finish early if it should be empty */
FILE *f = mustOpen(outTab, "w");
if (itemList == NULL)
    return;

/* Cope with invert option. */
if (optionExists("invert"))
    pairDistanceInvert(pairList);

/* Add up total distance between all items */
double totalDistance = 0.0;
for (item = itemList; item != NULL; item = item->next)
    {
    struct slName *nextItem = item->next;
    if (nextItem == NULL)
         break;
    totalDistance += pairDistanceHashLookup(pairHash, item->name, nextItem->name);
    }

double colorRange = gEnd - gStart;
outputItem(f, itemList->name, gStart);

double soFar = 0.0;
for (item = itemList; item != NULL; item = item->next)
    {
    struct slName *nextItem = item->next;
    if (nextItem == NULL)
         break;
    double distance = pairDistanceHashLookup(pairHash, item->name, nextItem->name);
    soFar += distance;
    outputItem(f, nextItem->name, gStart + (soFar/totalDistance * colorRange));
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gStart = optionDouble("start", gStart);
gEnd = optionDouble("end", gEnd);
distributeRgbRainbow(argv[1], argv[2], argv[3]);
return 0;
}
