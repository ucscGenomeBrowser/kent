/* netSyntenic - Prune net of non-syntenic elements and gather stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainNet.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netSyntenic - Prune net of non-syntenic elements and gather stats\n"
  "usage:\n"
  "   netSyntenic in.net out.net\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* A little stack of fills to help keep track of parents and
 * grandparents. */
enum {stackSize=128};
struct cnFill fillStackBuf[stackSize];
struct cnFill *fillStack = fillStackBuf + stackSize - 1;

boolean rangeNear(int s1, int e1, int s2, int e2, int maxDistance)
/* Return true if ranges are near each other. */
{
return rangeIntersection(s1, e1, s2, e2) >= -maxDistance;
}

void rNetSyn(struct chainNet *net, struct cnFill *fillList, 
	struct cnFill *parent, int depth)
/* Recursively classify syntenically. */
{
struct cnFill *fill;
char *type;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        {
	if (parent == NULL)
	    type = "top";
	else if (!sameString(fill->qName, parent->qName))
	    type = "nonSyn";
	else
	    {
	    int fStart = fill->qStart;
	    int fEnd = fStart + fill->qSize;
	    int pStart = parent->qStart;
	    int pEnd = pStart + parent->qSize;
	    int intersection = rangeIntersection(fStart, fEnd, pStart, pEnd);
	    if (intersection > 0)
	        {
		fill->qOver = intersection;
		fill->qFar = 0;
		}
	    else
	        {
		fill->qOver = 0;
		fill->qFar = -intersection;
		}
	    if (parent->qStrand == fill->qStrand)
	        type = "syn";
	    else
	        type = "inv";
	    }
	fill->type = type;
	}
    if (fill->children)
        rNetSyn(net, fill->children, fill, depth+1);
    }
}

void netSyntenic(char *inFile, char *outFile)
/* netSyntenic - Prune net of non-syntenic elements and gather stats. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct chainNet *net;

while ((net = chainNetRead(lf)) != NULL)
    {
    rNetSyn(net, net->fillList, NULL, 0);
    chainNetWrite(net, f);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
netSyntenic(argv[1], argv[2]);
return 0;
}
