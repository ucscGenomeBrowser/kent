/* motifLogo - Make a sequence logo out of a motif.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dystring.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"


char *tempDir = ".";
char *gsExe = "gs";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifLogo - Make a sequence logo out of a motif.\n"
  "usage:\n"
  "   motifLogo input.motif output.ps\n"
  "where input.motif contains a single line with the following space-separated\n"
  "fields:\n"
  "   name - name of the motif\n"
  "   baseCount - number of bases in the motif\n"
  "   aProb - comma-separated list of probabilities for base A\n"
  "   cProb - probabilities for C\n"
  "   gProb - probabilities for G\n"
  "   tProb - probabilities for T\n"
  "\n"
  "Options\n"
  "   -minHeight=n  the minimum height that is excluded from information\n"
  "    content scaling.  This allows something to show up in columns with very\n"
  "    little information content. maximum value is 120\n"
  "   -freq - create a letter frequency-based logo, ignoring information content.\n"
  );
}
static struct optionSpec options[] = {
   {"minHeight", OPTION_INT},
   {"freq", OPTION_BOOLEAN},
   {NULL, 0}
};
static int optMinHeight = 0;
static boolean optFreq = FALSE;

// size to allow for letters
static int widthPerBase = 40;
static int height = 120;


void motifLogo(char *motifFile, char *outFile)
/* motifLogo - Make a sequence logo out of a motif.. */
{
struct dnaMotif *motif = dnaMotifLoadAll(motifFile);
if (motif == NULL)
    errAbort("No motifs in %s", motifFile);
if (motif->next != NULL)
    warn("%s contains multiple motifs, only using first", motifFile);
dnaMotifMakeProbabalistic(motif);
dnaMotifToLogoPs2(motif, widthPerBase, height, optMinHeight, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
optFreq = optionExists("freq");
if (optFreq)
    {
    if (optionExists("minHeight"))
        errAbort("can't specify both -minHeight and -freq");
    optMinHeight = height; // no information content scaling
    }
else
    {
    optMinHeight = optionInt("minHeight", optMinHeight);
    if ((optMinHeight < 0) || (optMinHeight > height))
        errAbort("-minHeight must be in the range 0..%d", height);
    }


motifLogo(argv[1], argv[2]);
return 0;
}

