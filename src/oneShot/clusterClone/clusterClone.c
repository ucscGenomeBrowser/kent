/*	clusterClone - cluster together clone psl alignment results */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: clusterClone.c,v 1.1 2004/06/16 22:27:20 hiram Exp $";

char *chr = (char *)NULL;	/*	process the one chromosome listed */
static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterClone - cluster together clone psl alignment results\n"
  "usage:\n"
  "   clusterClone [options] <file.psl>\n"
  "options:\n"
  "   -chr=<chrN> - process only this one chrN from the file\n"
  "   -verbose=N - set verbose level N"
  );
}

struct coordEl
/* a pair of coordinates in a list */
    {
    struct coordEl *next;
    unsigned start;
    unsigned end;
    };

static void processResult(struct hash *chrHash, struct hash *coordHash
)
{
struct hashCookie cookie;
struct hashEl *hashEl;
int highCount = 0;
char *highName = (char *)NULL;
struct hashEl *coords;
int i;
unsigned lowMark = BIGNUM;
unsigned highMark = 0;
struct coordEl *coord;
struct coordEl **coordListPt = (struct coordEl **) NULL;

cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    int count = ptToInt(hashEl->val);
verbose(1,"# chr%s %d\n", hashEl->name, count);
    if (count > highCount)
	{
	highCount = count;
	if (highName)
	    freeMem(highName);
	highName = cloneString(hashEl->name);
	}
    }
verbose(1,"# chr%s %d highest count\n", highName, highCount);
coords = hashLookup(coordHash, highName);
coordListPt = coords->val;
coord = *coordListPt;
i = 0;
while (coord != NULL)
    {
    verbose(1,"# %d %d - %d\n", ++i, coord->start, coord->end);
    if (coord->start < lowMark) lowMark = coord->start;
    if (coord->end > highMark) highMark = coord->end;
    coord = coord->next;
    }
verbose(1,"# range: %u:%u = %u\n", lowMark, highMark, highMark-lowMark);

cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    coords = hashLookup(coordHash, hashEl->name);
    coordListPt = coords->val;
    slFreeList(coordListPt);
    }
freeMem(highName);
}

static void clusterClone(int argc, char *argv[])
{
int i;

for (i=1; i < argc; ++i)
    {
    struct lineFile *lf;
    struct psl *psl;
    unsigned tSize;
    char *prevAccName = (char *)NULL;
    struct hashEl *el;
    struct hash *chrHash = newHash(0);
    struct hash *coordHash = newHash(0);
    struct coordEl *coord;
    struct coordEl **coordListPt = (struct coordEl **) NULL;

    verbose(2,"#\tprocess: %s\n", argv[i]);
    lf=pslFileOpen(argv[i]);
    while ((struct psl *)NULL != (psl = pslNext(lf)) )
	{
	char *accName = (char *)NULL;
	char *chrName = (char *)NULL;
	int chrCount;

	accName = cloneString(psl->qName);
	chopSuffixAt(accName,'_');
	if ((char *)NULL == prevAccName)
		prevAccName = cloneString(accName);

	if (differentWord(accName, prevAccName))
	    {
	    processResult(chrHash, coordHash);
	    freeMem(prevAccName);
	    prevAccName = cloneString(accName);
	    freeHash(&chrHash);
	    freeHash(&coordHash);
	    chrHash = newHash(0);
	    coordHash = newHash(0);
	    }

	chrName = cloneString(psl->tName);
	stripString(chrName, "chr");
	el = hashLookup(chrHash, chrName);
	if (el == NULL)
	    {
	    hashAddInt(chrHash, chrName, 1);
	    chrCount = 1;
	    }
	else
	    {
	    chrCount = ptToInt(el->val) + 1;
	    el->val=intToPt(chrCount);
	    }
	AllocVar(coord);
	coord->start = psl->tStart;
	coord->end = psl->tEnd;
	el = hashLookup(coordHash, chrName);
	if (el == NULL)
	    {
	    AllocVar(coordListPt);
	    hashAdd(coordHash, chrName, coordListPt);
	    }
	else
	    {
	    coordListPt = el->val;
	    }
	slAddHead(coordListPt,coord);
	tSize = psl->tEnd - psl->tStart;
	verbose(1,"# %s\t%u\t%u\t%u\t%.4f\t%d %s:%d-%d\n",
	    psl->qName, psl->qSize,
	    tSize, tSize - psl->qSize,
	    100.0*((double)(tSize+1)/(psl->qSize + 1)), chrCount, psl->tName,
		psl->tStart, psl->tEnd);
	freeMem(accName);
	freeMem(chrName);
	pslFree(&psl);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{   
optionInit(&argc, argv, options);

if (argc < 2)
    usage();

chr = optionVal("chr", NULL);

if (chr)
    verbose(1,"#\tprocess chrom: %s\n", chr);

clusterClone(argc, argv);

return(0);
}
