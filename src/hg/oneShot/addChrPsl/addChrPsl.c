/* addChrPsl - add chr to chromosome in psl file. */
#include "common.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"addChrPsl - add chr to chromosome in psl file.\n"
"usage:\n"
"   addChrPsl old.psl new.psl\n");
}

void addChrPsl(char *oldName, char *newName)
/* addChrPsl - add chr to chromosome in psl file. */
{
char chrName[64];
struct lineFile *lf = pslFileOpen(oldName);
FILE *f = mustOpen(newName, "w");
struct psl *pslList = NULL, *psl;
char oName[64];
bool firstTime = TRUE;

while ((psl = pslNext(lf)) != NULL)
    {
    if (firstTime)
	{
	strcpy(oName, psl->tName);
	sprintf(chrName, "chr%s", oName);
	firstTime = FALSE;
	}
    else
	{
	if (!sameString(psl->tName, oName))
	    errAbort("Expecting all targets to be the same in %s, but got %s and %s",
		oldName, oName, psl->tName);
	}
    psl->tName = chrName;
    slAddHead(&pslList, psl);
    }
slSort(&pslList, pslCmpTarget);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    pslTabOut(psl, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
addChrPsl(argv[1], argv[2]);
}
