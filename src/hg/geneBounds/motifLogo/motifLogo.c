/* motifLogo - Make a sequence logo out of a motif.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dystring.h"
#include "dnaMotif.h"

static char const rcsid[] = "$Id: motifLogo.c,v 1.4 2004/09/12 21:30:02 kent Exp $";

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
  );
}

void motifLogo(char *motifFile, char *outFile)
/* motifLogo - Make a sequence logo out of a motif.. */
{
struct dnaMotif *motif = dnaMotifLoadAll(motifFile);
if (motif == NULL)
    errAbort("No motifs in %s", motifFile);
if (motif->next != NULL)
    warn("%s contains multiple motifs, only using first", motifFile);
dnaMotifMakeProbabilitic(motif);
// dnaMotifToLogoPs(motif, 40, 120, outFile);
dnaMotifToLogoPng(motif, 40, 120, gsExe, tempDir, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
motifLogo(argv[1], argv[2]);
return 0;
}

