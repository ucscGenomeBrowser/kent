/* pslUnpile - Removes huge piles of alignments from sorted psl files 
 * (due to unmasked repeats presumably).. */
#include "common.h"
#include "cheapcgi.h"
#include "psl.h"

/* Constants that can be over-ridden by command line. */
boolean doTarget = TRUE;
boolean noHead = FALSE;
int maxPile = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslUnpile - Removes huge piles of alignments from sorted \n"
  "psl files (due to unmasked repeats presumably).\n"
  "usage:\n"
  "   pslUnpile in.psl out.psl\n"
  "options:\n"
  "   -query - removes piles on query side\n"
  "   -target - removes piles on target side (default)\n"
  "   -nohead - suppress psl header in output.\n"
  "   -maxPile - maximum number of hits allowed in a pile before filtering\n"
  "              (default 100)\n"
  "in.psl should be sorted by query or target as appropriate");
}

boolean pslOverlap(struct psl *a, struct psl *b)
/* Returns TRUE if two psl's overlap. */
{
if (doTarget)
    {
    if (!sameString(a->tName, b->tName))
        return FALSE;
    return rangeIntersection(a->tStart, a->tEnd, b->tStart, b->tEnd) > 0;
    }
else
    {
    if (!sameString(a->qName, b->qName))
        return FALSE;
    return rangeIntersection(a->qStart, a->qEnd, b->qStart, b->qEnd) > 0;
    }
}

void pslUnpile(char *inName, char *outName)
/* pslUnpile - Removes huge piles of alignments from sorted 
 * psl files (due to unmasked repeats presumably).. */
{
FILE *f = mustOpen(outName, "w");
enum gfType qType, tType;
struct lineFile *lf;
struct psl *list = NULL, *psl, *el;

pslxFileOpen(inName, &qType, &tType, &lf);
if (!noHead)
    pslxWriteHead(f, qType, tType);
for (;;)
    {
    psl = pslNext(lf);
    if (list != NULL && (psl == NULL || !pslOverlap(psl, list)))
        {
	if (list != NULL)
	    {
	    slReverse(&list);
	    if (slCount(list) <= maxPile)
	        {
		for (el = list; el != NULL; el = el->next)
		    {
		    pslTabOut(el, f);
		    }
		}
	    else
	        {
		for (el = list; el != NULL; el = el->next)
		    {
		    if (psl == NULL)
			pslTabOut(el, f);
		    else if (psl->tEnd - psl->tStart > 4000)
			pslTabOut(el, f);
		    }
		}
	    pslFreeList(&list);
	    }
	}
    if (psl == NULL)
        break;
    slAddHead(&list, psl);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
doTarget = !cgiBoolean("query");
noHead = (cgiBoolean("nohead") || cgiBoolean("noHead"));
maxPile = cgiOptionalInt("maxPile", maxPile);
pslUnpile(argv[1], argv[2]);
return 0;
}
