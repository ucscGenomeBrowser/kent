/* pslFilter - thin out psl file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "psl.h"

void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslFilter - filter out psl file\n"
    "    pslFilter in.psl out.psl \n");
}

boolean filterOk(struct psl *psl)
/* Return TRUE if psl passes filter. */
{
if (psl->match + psl->repMatch < 100)
    return FALSE;
//if (sameString(psl->tName, psl->qName))
 //   return FALSE;
if (pslCalcMilliBad(psl, FALSE) > 50)
    return FALSE;
return TRUE;
}

void pslFilter(char *inName, char *outName)
/* Filter inName into outName. */
{
struct lineFile *in = pslFileOpen(inName);
FILE *out = mustOpen(outName, "w");
struct psl *psl;
int passCount = 0;
int totalCount = 0;

printf("Filtering %s into %s\n", inName, outName);
pslWriteHead(out);
while ((psl = pslNext(in)) != NULL)
    {
    ++totalCount;
    if (filterOk(psl))
	{
	++passCount;
	pslTabOut(psl, out);
	}
    pslFree(&psl);
    }
lineFileClose(&in);
fclose(out);
printf("%d of %d passed filter\n", passCount, totalCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
pslFilter(argv[1], argv[2]);
return 0;
}
