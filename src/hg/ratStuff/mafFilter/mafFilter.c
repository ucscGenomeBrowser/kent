/* mafFilter - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

static char const rcsid[] = "$Id: mafFilter.c,v 1.3 2003/05/06 07:22:34 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFilter - Filter out maf files. Output goes to standard out\n"
  "usage:\n"
  "   mafFilter file(s).maf\n"
  "options:\n"
  "   -tolerate - Just ignore bad input rather than aborting.\n"
  "   -minScore=N - Minimum allowed score\n"
  );
}

boolean gotMinScore = FALSE;
double minScore;

static jmp_buf recover;    /* Catch errors in load maf the hard way in C. */

static void abortHandler()
/* Abort fuzzy finding. */
{
if (optionExists("tolerate"))
    {
    longjmp(recover, -1);
    }
else
    exit(-1);
}

boolean filterOne(struct mafAli *maf)
/* Return TRUE if maf passes filters. */
{
if (gotMinScore && maf->score < minScore)
    return FALSE;
return TRUE;
}

void mafFilter(int fileCount, char *files[])
/* mafFilter - Filter out maf files. */
{
FILE *f = stdout;
int i;
int status;

for (i=0; i<fileCount; ++i)
    {
    char *fileName = files[i];
    struct mafFile *mf = mafOpen(fileName);
    struct mafAli *maf = NULL;
    if (i == 0)
        mafWriteStart(f, mf->scoring);
    for (;;)
	{
	/* Effectively wrap try/catch around mafNext() */
	status = setjmp(recover);
	if (status == 0)    /* Always true except after long jump. */
	    {
	    pushAbortHandler(abortHandler);
	    maf = mafNext(mf);
	    if (maf != NULL && filterOne(maf))
	        mafWrite(f, maf);
	    }
	popAbortHandler();
	if (maf == NULL)
	    break;
	}
    mafFileFree(&mf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (optionExists("minScore"))
    {
    gotMinScore = TRUE;
    minScore = optionFloat("minScore", 0);
    }
if (argc < 2)
    usage();
mafFilter(argc-1, argv+1);
return 0;
}
