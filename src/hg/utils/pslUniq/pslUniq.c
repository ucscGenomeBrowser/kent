/* pslUniq - strip out all but first record found */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslUniq.c,v 1.3 2004/02/07 20:28:23 braney Exp $";

int numAllow = 1;

void usage()
/* Explain usage and exit. */
{
errAbort("usage: pslUniq in.psl out.psl\n"
"   -numAllow=N  how many of each identifier to keep (default 1)\n"
);
}

void pslUniq( char *pslName, char *outName, int numAllow)
{
int size;
char *name;
struct psl *psl;
struct hash *pslHash = newHash(0);
struct hashEl *hel;
char *start;
FILE *out = mustOpen(outName, "w");
struct lineFile *list;
struct lineFile *pslF = pslFileOpen(pslName);

while ( psl = pslNext(pslF))
    {
    if ( (hel = hashLookup(pslHash, psl->qName)) == NULL)
	{
	hashAdd(pslHash, psl->qName, 1);
	pslTabOut(psl, out); 
	}
    else
	{
	hel->val++;
	if (hel->val <= numAllow)
	    pslTabOut(psl, out); 
	}

    }
lineFileClose(&pslF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
numAllow = optionInt("numAllow", 1);
if (argc != 3)
    usage();
pslUniq(argv[1], argv[2], numAllow);
return 0;
}
