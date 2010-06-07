#include "common.h"
#include "dMatrix.h"
#include "options.h"
#include "linefile.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics_double.h>


struct rnaBinder 
/* Name and correlation of rna binding proteins. */
{
    struct rnaBinder *next; /* Next in list. */
    char *psName;           /* Name of probe set. Something like
		 	     * E@NM_XXXXXX_a_at or GXXXXXXX_a_at.*/
    char *geneName;         /* Name of gene probed by probe
			     * set. Hopefully something readable. */
    char *pfamAcc;          /* Name of pfam accession. */
    char *pfamName;         /* Name of pfam domain. */
    double corr;           /* Correlation between rnaBinder and cluster of interest. */
};

struct clusterMember
/* Member of the cluster of interest. */
{
    struct clusterMember *next; /* Next in list. */
    char *geneId;               /* Gene id in cluster. */
    char *psName;               /* Name of the probe set. */
    char *desc;                 /* Description of the probe set. */
};

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"sjIndexFile", OPTION_STRING},
    {"psFile", OPTION_STRING},
    {"clusterFile", OPTION_STRING},
    {"rnaBindingFile", OPTION_STRING},
    {"outputFile", OPTION_STRING},
    {"antiCorrelation", OPTION_BOOLEAN},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this messge.",
    "File with splice junction indexes.",
    "File with probe set intensities.",
    "File with probe sets in cluster of interest.",
    "File with rna binding gene sets.",
    "File to output correlation results to.",
    "Sort by anti-correlation rather than correlation."
};

void usage()
/* Print usage and quit. */
{
int i=0;
warn("rnaBindingClusterCorr - Program to calculate the correlation\n"
     "between rna binding protein expression data and splice junctions\n"
     "co-regulated as determined by clustering splice junction indexes.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n"
	 "   rnaBindingClusterCorr -sjIndexFile=input/sjIndexTest.tab -psFile=input/psIntenTest.tab \\ \n"
         "      -clusterFile=input/clusterTest.tab -rnaBindingFile=input/rnaBinderTest.tab \\ \n"
         "      -outputFile=output/test.out.tab\n");
}


double correlation(double *X, double *Y, int count)
/* Compute the correlation between X and Y 
   correlation(X,Y) = cov(X,Y)/ squareRt(var(X)var(Y))
   page 332 Sheldon Ross "A First Course in Probability" 1998
*/
{
double varX = gsl_stats_variance(X, 1, count);
double varY = gsl_stats_variance(Y, 1, count);
double covXY = gsl_stats_covariance(X, 1, Y, 1, count);

double correlation = covXY / sqrt(varX *varY);
return correlation;
}

double calcDistanceFromCluster(struct rnaBinder *rb, struct clusterMember *cmList,
			       struct dMatrix *sjIndex, struct dMatrix *psInten)
/* Calculate the distance from the rnaBinder intensity measurement to
   the sjIndexes of the cluster members. If no intensity present use
   0 as it will fall in the middle of [-1,1]. */
{
double sum = 0;
int count = 0;
int sjIx = 0, gsIx = 0;
struct clusterMember *cm = NULL;
double corr = 0;
if(sjIndex->colCount != psInten->colCount)
    errAbort("Splice Junction and Intensity files must have same number of columns.");

/* Get the index of the gene set in the intensity file. */
gsIx = hashIntValDefault(psInten->nameIndex, rb->psName, -1);
if(gsIx == -1)
    {
/*     warn("Probe Set %s not found in intensitiy file."); */
    return 0;
    }
for(cm = cmList; cm != NULL; cm = cm->next)
    {
    /* For each member get the index in the splice junction file. */
    sjIx = hashIntValDefault(sjIndex->nameIndex, cm->psName, -1);
    if(sjIx == -1)
	errAbort("Probe Set %s not found in SJ index file.");
    corr = correlation(psInten->matrix[gsIx], sjIndex->matrix[sjIx], sjIndex->colCount);
    sum += corr;
    count++;
    }
if(count == 0)
    errAbort("No junctions in cluster.");
sum = sum / (double) count;
return sum;
}

int rnaBinderCmp(const void *va, const void *vb)
/* Compare to sort based correlation. */
{
const struct rnaBinder *a = *((struct rnaBinder **)va);
const struct rnaBinder *b = *((struct rnaBinder **)vb);
return a->corr < b->corr;
}

struct rnaBinder *loadRnaBinders()
/* Load the probe sets that encode genes thought to 
   bind rnas. Expected order is probeSet, geneName, pfamAcc, pfamName */
{
struct rnaBinder *rbList = NULL, *rb = NULL;
char *words[4];
struct lineFile *lf = NULL;
char *inputFile = optionVal("rnaBindingFile", NULL);

assert(inputFile);
lf = lineFileOpen(inputFile, TRUE);
while(lineFileChopCharNext(lf, '\t', words, ArraySize(words)))
    {
    AllocVar(rb);
    rb->psName = cloneString(words[0]);
    rb->geneName = cloneString(words[1]);
    rb->pfamAcc = cloneString(words[2]);
    rb->pfamName = cloneString(words[3]);
    slAddHead(&rbList, rb);
    }
lineFileClose(&lf);
slReverse(&rbList);
return rbList; 
}

struct clusterMember *loadClusterMembers()
/* Load the probe sets that are in our cluster of interest. */
{
struct clusterMember *cmList = NULL, *cm = NULL;
char *words[3];
struct lineFile *lf = NULL;
char *inputFile = optionVal("clusterFile", NULL);
int wordCount = 0;
assert(inputFile);
lf = lineFileOpen(inputFile, TRUE);
while((wordCount = lineFileChopCharNext(lf, '\t', words, ArraySize(words))) != 0)
    {
    AllocVar(cm);
    if(wordCount == 3) 
	{
	cm->geneId = cloneString(words[0]);
	cm->psName = cloneString(words[1]);
	cm->desc = cloneString(words[2]);
	}
    else if(wordCount == 2)
	{
	cm->psName = cloneString(words[0]);
	cm->desc = cloneString(words[1]);
	}
    else
	errAbort("Got %d words at line %d", wordCount, lf->lineIx);
    slAddHead(&cmList, cm);
    }
lineFileClose(&lf);
slReverse(&cmList);
return cmList; 
}

void reportRnaBinders(struct rnaBinder *rbList)
/* Report the rnaBinder distances. */
{
struct rnaBinder *rb = NULL;
char *fileName = optionVal("outputFile", NULL);
FILE *out = NULL;

if(fileName == NULL)
    errAbort("Must specify an outputFile");
out = mustOpen(fileName, "w");
for(rb = rbList; rb != NULL; rb = rb->next)
    {
    fprintf(out, "%s\t%s\t%s\t%s\t%.4f\n", 
	    rb->psName, rb->geneName, rb->pfamAcc, rb->pfamName, rb->corr);
    }
carefulClose(&out);
}

void rnaBindingClusterCorr()
/* Top level function to calculate correlations. */
{
char *sjIndexFile = optionVal("sjIndexFile", NULL);
char *psFile = optionVal("psFile", NULL);
struct dMatrix *sjIndex = NULL, *psInten = NULL;
struct rnaBinder *rbList = NULL, *rb = NULL;
struct clusterMember *cmList = NULL, *cm = NULL;

warn("Loading Files.");
sjIndex = dMatrixLoad(sjIndexFile); 
psInten = dMatrixLoad(psFile); 
rbList = loadRnaBinders();
cmList = loadClusterMembers();

warn("Calculating Distance.");
/* Calculate distance. */
for(rb = rbList; rb != NULL; rb = rb->next)
    rb->corr = calcDistanceFromCluster(rb, cmList, sjIndex, psInten);

/* Sort by distance (correlati2n) */
slSort(&rbList, rnaBinderCmp);

/* If we want anti-correlation reverse. */
if(optionExists("antiCorrelation"))
    slReverse(&rbList);

warn("Writing out results.");
/* Do some outputting. */
reportRnaBinders(rbList);

/* Cleanup. */
dMatrixFree(&sjIndex);
dMatrixFree(&psInten);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
rnaBindingClusterCorr();
return 0;
}
