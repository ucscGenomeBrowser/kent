/* blatFilter - filter blat alignments somewhat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "psl.h"


int dotEvery = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blatFilter - filter blat alignments somewhat\n"
  "usage:\n"
  "   blatFilter output.psl infile(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dotOut()
/* Put out a dot every now and then if user want's to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}

int pslBlockTotalSize(struct psl *psl)
/* Return size of all blocks. */
{
int i;
return psl->match + psl->repMatch + psl->misMatch;
}


void writePslFrags(struct psl *psl, FILE *f)
/* Look into psl and figure out if we want to write out
 * all or part of it. */
{
int i;
int totalSize = 0;
int size = pslBlockTotalSize(psl);

for (i=0; i<psl->blockCount; ++i)
    {
    if ((size = psl->blockSizes[i]) >= 30)
        {
	static struct psl p;
	unsigned blockSizes, qStarts, tStarts;
	p.match = roundingScale(psl->match, size, totalSize);
	p.misMatch = roundingScale(psl->misMatch, size, totalSize);
	p.repMatch = roundingScale(psl->repMatch, size, totalSize);
	p.nCount = roundingScale(psl->nCount, size, totalSize);
	p.strand[0] = psl->strand[0];
	p.strand[1] = psl->strand[1];
	p.qName = psl->qName;
	p.qSize = psl->qSize;
	p.tName =psl->tName;
	p.tSize = psl->tSize;
	p.blockCount = 1;
	p.blockSizes = &blockSizes;
	blockSizes = size;
	p.qStarts = &qStarts;
	qStarts = psl->qStarts[i];
	p.tStarts = &tStarts;
	tStarts = psl->tStarts[i];
	if (p.strand[0] == '-')
	    p.qStart = psl->qSize - (qStarts + size);
	else
	    p.qStart = qStarts;
	if (p.strand[1] == '-')
	    p.tStart = psl->tSize - (tStarts + size);
	else
	    p.tStart = tStarts;
	p.qEnd = p.qStart + size;
	p.tEnd = p.tStart + size;
	pslTabOut(&p, f);
	}
    }
}

void blatFlekFilter(char *outName, int inCount, char *inNames[])
/* blatFilter - filter blat alignments somewhat. */
{
int i;
FILE *f = mustOpen(outName, "w");

for (i=0; i<inCount; ++i)
    {
    char *inName = inNames[i];
    struct lineFile *lf = pslFileOpen(inName);
    struct psl *psl;
    while ((psl = pslNext(lf)) != NULL)
        {
	dotOut();
	if (psl->tEnd - psl->tStart < (psl->qEnd + psl->qStart) * 3)
	    pslTabOut(psl, f);
	else
	    writePslFrags(psl, f);
	pslFree(&psl);
	}
    }
printf("\n");
}

boolean detailTest(struct psl *psl)
/* Detailed pass/fail test. */
{
int size = pslBlockTotalSize(psl);
int badFactor = psl->misMatch + psl->tNumInsert + psl->qNumInsert + 2*log(1+psl->tBaseInsert + psl->qBaseInsert);
int milliBad = roundingScale(1000, badFactor, size);

if (sameString(psl->qName, "ti|18649044"))
    {
    static int maxc = 10;
    uglyf("%s: size %d, badFactor %d, milliBad %d\n", psl->qName, size, badFactor, milliBad);
    if (--maxc == 0)
	uglyAbort("All for now");
    }
#ifdef NEVER
#endif /* NEVER */

if (milliBad < 85)
     return FALSE;
return TRUE;
}


void blatFilter(char *outName, int inCount, char *inNames[])
/* blatFilter - filter blat alignments somewhat. */
{
int i;
FILE *f = mustOpen(outName, "w");

for (i=0; i<inCount; ++i)
    {
    char *inName = inNames[i];
    struct lineFile *lf = pslFileOpen(inName);
    struct psl *psl;
    while ((psl = pslNext(lf)) != NULL)
        {
	dotOut();
	if (psl->match + psl->repMatch + psl->nCount < 260 || detailTest(psl))
	    pslTabOut(psl, f);
	pslFree(&psl);
	}
    }
printf("\n");
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 3)
    usage();
blatFilter(argv[1], argc-2, argv+2);
return 0;
}
