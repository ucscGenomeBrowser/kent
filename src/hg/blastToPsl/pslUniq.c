/* pslUniq - strip out all but first record found */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslUniq.c,v 1.2 2003/11/05 05:03:50 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort("usage: pslUniq in.psl out.psl\n");
}

void pslUniq( char *pslName, char *outName)
{
int size;
char *name;
struct psl *psl;
struct hash *pslHash = newHash(0);
char *start;
FILE *out = mustOpen(outName, "w");
struct lineFile *list;
struct lineFile *pslF = pslFileOpen(pslName);

while ( psl = pslNext(pslF))
    {
    if ( !hashLookup(pslHash, psl->qName))
	{
	hashAdd(pslHash, psl->qName, psl);
	pslTabOut(psl, out); 
	}
    }
lineFileClose(&pslF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
pslUniq(argv[1], argv[2]);
return 0;
}
