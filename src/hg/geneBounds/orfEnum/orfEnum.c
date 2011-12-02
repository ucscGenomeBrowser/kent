/* orfEnum - Enumerate all orfs in a cDNA according to a variety of ORF criteria. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "fa.h"


int minSize=24;
boolean doAll = TRUE;
boolean doBiggest = FALSE;
boolean doBiggestAtg = FALSE;
boolean doFirst = FALSE;
boolean firstKozak = FALSE;
boolean biggestKozak = FALSE;
boolean firstAboveMin = FALSE;
boolean firstKozakAboveMin = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfEnum - Enumerate all orfs in a cDNA.\n"
  "usage:\n"
  "   orfEnum in.fa out.orf\n"
  "options:\n"
  "   -minSize=N - minimum size to output (in bases).  Default %d\n"
  "The following options will make it so not all, but only the ones\n"
  "that meet the criteria, are output.\n"
  "   -biggest - output biggest ORF\n"
  "   -biggestAtg - output biggest ORF that starts with a proper ATG\n"
  "   -first - output first ORF\n"
  "   -firstKozak - output first with good Kozak sequence\n"
  "   -biggestKozak - output biggest with good Kozak sequence\n"
  "   -firstAboveMin - output first orf bigger than minSize\n"
  "   -firstKozakAboveMin - output first Kozak orf bigger than minSize\n"
  "This option is the same as doing the above seven option\n"
  "   -various\n"
  , minSize
  );
}

static struct optionSpec options[] = {
   {"minSize", OPTION_INT},
   {"biggest", OPTION_BOOLEAN},
   {"biggestAtg", OPTION_BOOLEAN},
   {"first", OPTION_BOOLEAN},
   {"firstKozak", OPTION_BOOLEAN},
   {"biggestKozak", OPTION_BOOLEAN},
   {"firstAboveMin", OPTION_BOOLEAN},
   {"firstKozakAboveMin", OPTION_BOOLEAN},
   {"various", OPTION_BOOLEAN},
   {NULL, 0},
};


struct orfInfo
/* Information about ORF. */
    {
    struct orfInfo *next;
    char *name;		/* Name of associated cDNA. */
    int start,end;	/* Half open interval covering ORF. */
    int size;		/* Just end - start. */
    bool gotAtg;	/* TRUE if has initial ATG. */
    bool kozak;		/* Has a Kozak consensus. */
    };

struct orfInfo *orfInfoNew(DNA *dna, int dnaSize, char *name, int start, int end)
/* Return new orfInfo. */
{
struct orfInfo *orf;
AllocVar(orf);
orf->name = cloneString(name);
orf->start = start;
orf->end = end;
orf->size = end - start;
orf->gotAtg = startsWith("atg", dna+start);
if (orf->gotAtg)
    orf->kozak = isKozak(dna, dnaSize, start);
return orf;
}

void orfInfoFree(struct orfInfo **pEl)
/* Free up an orfInfo. */
{
struct orfInfo *el;
if ((el = *pEl) == NULL) return;
freeMem(el->name);
freez(pEl);
}

void orfInfoFreeList(struct orfInfo **pList)
/* Free a list of dynamically allocated orfInfo's */
{
struct orfInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    orfInfoFree(&el);
    }
*pList = NULL;
}

void outputIfBigEnough(struct orfInfo *orf, FILE *f)
/* Output if orf is big enough. */
{
if (orf->size >= minSize)
    fprintf(f, "%s\t%d\t%d\n", orf->name, orf->start, orf->end);
}

int orfInfoCmpStart(const void *va, const void *vb)
/* Compare to sort based on start . */
{
const struct orfInfo *a = *((struct orfInfo **)va);
const struct orfInfo *b = *((struct orfInfo **)vb);
return a->start - b->start;
}

struct orfInfo *orfStartingAt(char *name, DNA *dna, int start, int dnaSize)
/* Save ORF starting at start, and extending to stop codon
 * or end, whatever comes first. */
{
int stop;
int lastPos = dnaSize-3;
for (stop = start; stop<=lastPos; stop += 3)
    {
    if (isStopCodon(dna+stop))
	 return orfInfoNew(dna, dnaSize, name, start, stop+3);
    }
int aaSize = (stop-start)/3;
return orfInfoNew(dna, dnaSize, name, start, start+3*aaSize);
}

void orfInFrame(char *name, DNA *dna, int start, int dnaSize, struct orfInfo **pList)
/* Output all ORFs in this frame. */
{
int endPos = dnaSize - 3;
/* Since we may be a fragment, we always begin with an ORF. */
struct orfInfo *orf = orfStartingAt(name, dna, start, dnaSize);
slAddHead(pList, orf);
int pos;
for (pos = start+3; pos <= endPos; pos += 3)
    {
    if (startsWith("atg", dna+pos))
	{
        orf = orfStartingAt(name, dna, pos, dnaSize);
	slAddHead(pList, orf);
	}
    }
}

void outputUnique(struct slRef **pUniq, struct orfInfo *orf, FILE *f)
/* Output ORF, taking care not to ouput it twice. */
{
if (refOnList(*pUniq, orf))
    return;
outputIfBigEnough(orf, f);
refAdd(pUniq, orf);
}

void orfEnum(char *inFa, char *outOrf)
/* orfEnum - Enumerate all orfs in a cDNA. */
{
struct lineFile *lf = lineFileOpen(inFa, TRUE);
FILE *f = mustOpen(outOrf, "w");
DNA *dna;
char *name;
int dnaSize;
while (faSpeedReadNext(lf, &dna, &dnaSize, &name))
    {
    struct orfInfo *orf, *orfList = NULL;
    orfInFrame(name, dna, 0, dnaSize, &orfList);
    orfInFrame(name, dna, 1, dnaSize, &orfList);
    orfInFrame(name, dna, 2, dnaSize, &orfList);
    if (orfList != NULL)
	{
	slSort(&orfList, orfInfoCmpStart);
	if (doAll)
	    {
	    for (orf = orfList; orf != NULL; orf = orf->next)
		outputIfBigEnough(orf, f);
	    }
	else
	    {
	    struct slRef *refList = NULL;
	    if (doFirst)
		{
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->gotAtg)
			{
			outputUnique(&refList, orf, f);
			break;
			}
		    }
		}
	    if (firstKozak)
		{
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->kozak)
			{
			outputUnique(&refList, orf, f);
			break;
			}
		    }
		}
	    if (firstAboveMin)
		{
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->gotAtg && orf->size >= minSize)
			{
			outputUnique(&refList, orf, f);
			break;
			}
		    }
		}
	    if (firstKozakAboveMin)
		{
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->kozak && orf->size >= minSize)
			{
			outputUnique(&refList, orf, f);
			break;
			}
		    }
		}
	    if (doBiggest)
		{
		int biggestSize = 0;
		struct orfInfo *biggestOrf = NULL;
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->size > biggestSize)
			{
			biggestSize = orf->size;
			biggestOrf = orf;
			}
		    }
		outputUnique(&refList, biggestOrf, f);
		}
	    if (doBiggestAtg)
		{
		int biggestSize = 0;
		struct orfInfo *biggestOrf = NULL;
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->gotAtg && orf->size > biggestSize)
			{
			biggestSize = orf->size;
			biggestOrf = orf;
			}
		    }
		if (biggestOrf != NULL)
		    {
		    outputUnique(&refList, biggestOrf, f);
		    }
		}
	    if (biggestKozak)
		{
		int biggestSize = 0;
		struct orfInfo *biggestOrf = NULL;
		for (orf = orfList; orf != NULL; orf = orf->next)
		    {
		    if (orf->kozak && orf->size > biggestSize)
			{
			biggestSize = orf->size;
			biggestOrf = orf;
			}
		    }
		if (biggestOrf != NULL)
		    {
		    outputUnique(&refList, biggestOrf, f);
		    }
		}
	    slFreeList(&refList);
	    }
	orfInfoFreeList(&orfList);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
minSize = optionInt("minSize", minSize);
doBiggest = optionExists("biggest");
doBiggestAtg = optionExists("biggestAtg");
doFirst = optionExists("first");
firstKozak = optionExists("firstKozak");
biggestKozak = optionExists("biggestKozak");
firstAboveMin = optionExists("firstAboveMin");
firstKozakAboveMin = optionExists("firstKozakAboveMin");
if (optionExists("various"))
    doBiggest = doBiggestAtg = doFirst = firstKozak 
    	= biggestKozak = firstAboveMin = firstKozakAboveMin = TRUE;
if (doBiggest || doBiggestAtg || doFirst || firstKozak || biggestKozak 
	|| firstAboveMin || firstKozakAboveMin )
   doAll = FALSE;
dnaUtilOpen();
orfEnum(argv[1], argv[2]);
return 0;
}
