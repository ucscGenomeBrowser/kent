/* motifLogo - Make a sequence logo out of a motif.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaMotif.h"

static char const rcsid[] = "$Id: motifLogo.c,v 1.3 2004/09/12 18:00:31 kent Exp $";

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

void dnaMotifNormalize(struct dnaMotif *motif)
/* Make all columns of motif sum to one. */
{
int i;
for (i=0; i<motif->columnCount; ++i)
    {
    float sum = motif->aProb[i] + motif->cProb[i] + motif->gProb[i] + motif->tProb[i];
    if (sum < 0)
        errAbort("%s has negative numbers, perhaps it's score not probability based", 
		motif->name);
    if (sum == 0)
         motif->aProb[i] = motif->cProb[i] = motif->gProb[i] = motif->tProb[i] = 0.25;
    motif->aProb[i] /= sum;
    motif->cProb[i] /= sum;
    motif->gProb[i] /= sum;
    motif->tProb[i] /= sum;
    }
}


double u1(double prob)
/* Calculate partial uncertainty for one base. */
{
if (prob == 0)
    return 0;
return prob * logBase2(prob);
}

static double uncertainty(struct dnaMotif *motif, int pos)
/* Return the uncertainty at pos of motif.  This corresponds
 * to the H function in logo.pm */
{
return -( u1(motif->aProb[pos]) + u1(motif->cProb[pos])
	+ u1(motif->gProb[pos]) +u1(motif->gProb[pos]) );
}

static double bitsOfInfo(struct dnaMotif *motif, int pos)
/* Return bits of information at position. */
{
return 2 - uncertainty(motif, pos);
}

struct letterProb
/* A letter tied to a probability. */
    {
    struct letterProb *next;
    double prob;	/* Probability for this letter. */
    char letter;	/* The letter (upper case) */
    };

static struct letterProb *letterProbNew(char letter, double prob)
/* Make a new letterProb. */
{
struct letterProb *lp;
AllocVar(lp);
lp->letter = letter;
lp->prob = prob;
return lp;
}

static int letterProbCmp(const void *va, const void *vb)
/* Compare to sort highest probability first. */
{
const struct letterProb *a = *((struct letterProb **)va);
const struct letterProb *b = *((struct letterProb **)vb);
double dif = a->prob - b->prob;
if (dif < 0)
   return -1;
else if (dif > 0)
   return 1;
else
   return 0;
}

static void addBaseProb(struct letterProb **pList, char letter, double prob)
/* If prob > 0 add letterProb to list. */
{
if (prob > 0)
    {
    struct letterProb *lp = letterProbNew(letter, prob);
    slAddHead(pList, lp);
    }
}

static struct letterProb *letterProbFromMotifColumn(struct dnaMotif *motif, int pos)
/* Return letterProb list corresponding to column of motif. */
{
struct letterProb *lpList = NULL, *lp;
addBaseProb(&lpList, 'A', motif->aProb[pos]);
addBaseProb(&lpList, 'C', motif->cProb[pos]);
addBaseProb(&lpList, 'G', motif->gProb[pos]);
addBaseProb(&lpList, 'T', motif->tProb[pos]);
slSort(&lpList, letterProbCmp);
return lpList;
}

static void psOneColumn(struct dnaMotif *motif, int pos,
    double xStart, double yStart, double width, double totalHeight,
    FILE *f)
/* Write one column of logo to postScript. */
{
struct letterProb *lp, *lpList = letterProbFromMotifColumn(motif, pos);
double x = xStart, y = yStart, w = width, h;
for (lp = lpList; lp != NULL; lp = lp->next)
    {
    h = totalHeight * lp->prob;
    fprintf(f, "%cColor ", tolower(lp->letter));
    fprintf(f, "%3.2f ", x);
    fprintf(f, "%3.2f ", y);
    fprintf(f, "%3.2f ", x + w);
    fprintf(f, "%3.2f ", y + h);
    fprintf(f, "(%c) textInBox\n", lp->letter);
    y += h;
    }
fprintf(f, "\n");
}

void dnaMotifToLogoPs(struct dnaMotif *motif, double widthPerBase, double height, FILE *f)
/* Write logo corresponding to motif to postScript file. */
{
int i;
int xStart = 0;
char *s = 
#include "dnaSeqLogo.pss"
;

fprintf(f, "%%!PS-Adobe-3.1 EPSF-3.0\n");
fprintf(f, "%%%%BoundingBox: 0 0 %d %d\n\n", 
	(int)ceil(widthPerBase*motif->columnCount), (int)ceil(height));
fprintf(f, "%s", s);

fprintf(f, "%s", "% Start of code for this specific logo\n");

for (i=0; i<motif->columnCount; ++i)
    {
    double infoScale = bitsOfInfo(motif, i)/2.0;
    psOneColumn(motif, i, xStart, 0, widthPerBase, infoScale * height, f);
    xStart += widthPerBase;
    }
}

void motifLogo(char *motifFile, char *outFile)
/* motifLogo - Make a sequence logo out of a motif.. */
{
FILE *f = mustOpen(outFile, "w");
struct dnaMotif *motif = dnaMotifLoadAll(motifFile);
if (motif == NULL)
    errAbort("No motifs in %s", motifFile);
if (motif->next != NULL)
    warn("%s contains multiple motifs, only using first", motifFile);
dnaMotifNormalize(motif);
dnaMotifToLogoPs(motif, 20, 100, f);
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

