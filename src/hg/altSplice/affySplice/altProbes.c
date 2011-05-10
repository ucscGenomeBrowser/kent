/* altProbes.c - Match probes to splicing events and do anaylysis.

   This program origianlly was written to calculate which probe
   sets were being expressed and matching up the probe sets with
   the correct splice junctions. Since then it has expanded to fill
   a number of roles as it is one of the few times where the probe
   sets, data, and splicing information are all accesible at the
   same time. Now the program is almost run in an iterative fashion,
   the first time it generates a number of matrices that are used
   in R to do calculations. The pvalues and scores that come out
   of R can then be fed back in to be used as pre-calculated
   p-vals and scores for outputting new stats.

   Currently altProbes actually:
   - Matches probe sets to splicing paths.
   - Calculates average intensities and pvalues for a given path.
   - Looks for known binding sites of splicing factors.
   - Lifts splicing factors to current assembly and looks for overlaps
     with conserved elements.
   - Compiles and outputs matrices for R.
   - Outputs a number of cassette exon specific sequence files.
*/

#include "common.h"
#include "bed.h"
#include "hash.h"
#include "options.h"
#include "chromKeeper.h"
#include "binRange.h"
#include "obscure.h"
#include "linefile.h"
#include "dystring.h"
#include "altGraphX.h"
#include "splice.h"
#include "hdb.h"
#include "dMatrix.h"
#include "liftOver.h"
#include "dnautil.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_statistics_double.h>

struct altPath
/* Information about a path and associated bed probes. */
{
    struct altPath *next; /* Next in list. */
    int probeCount;        /* Number of probes on this path. */
    struct bed **beds;     /* Array of beds that are unique to this path. */
    int *bedTypes;         /* Are beds exons or splice junction probes. */
    double **expVals;      /* Expression values. */
    double **pVals;        /* Probability of expression. */
    double *avgExpVals;    /* Average value for each exp. */
    struct path *path;     /* Path that this altPath represents. */
    double score;               /* Score of being expressed overall. */
    double exonCorr;            /* Correlation of exon probe set with include junction if appropriate. */
    double exonSkipCorr;        /* Correlation of exon probe set with skip junction if appropriate. */
    double probCorr;            /* Correlation of exon probe set prob and include probs. */
    double probSkipCorr;        /* Correlation of exon probe set prob skip junction if appropriate. */
    double exonSjPercent;       /* Percentage of includes confirmed by exon probe set. */
    double sjExonPercent;       /* Percentage of exons confirmed by sj probe set. */
    int exonAgree;              /* Number of times exon agrees with matched splice junction */
    int exonDisagree;           /* Number of times exon disagrees with matched splice junction. */
    int sjAgree;                /* Number of times sj agrees with matched exon */
    int sjDisagree;             /* Number of times sj disagrees with matched exon. */
    int sjExp;                  /* Number of times sj expressed. */
    int exonExp;                /* Number of times exon expressed. */
    int *motifUpCounts;         /* Number of motifs counted upstream for each motif. */
    int *motifDownCounts;        /* Number of motifs counted downstream for each motif. */
    int *motifInsideCounts;        /* Number of motifs counted inside for each motif. */
};

struct valuesMat
/* Matrix of slNames. */
{
    struct valuesMat *next; /* Next in list. */
    struct hash *rowIndex;   /* Hash of row names idexes. */
    int colCount;            /* Number of columns. */
    int maxCols;             /* Number of columns we have memory for. */
    char **colNames; /* Column Names. */
    int rowCount;            /* Number of rows. */
    int maxRows;             /* Number of rows we have memory for. */
    char **rowNames;         /* Names of rows. */
    char ***vals;            /* Values. */
};

struct altEvent
/* Information about a particular alt-splicing event. Specifically a
 * collection of paths and the associated data. */
{
    struct altEvent *next;       /* Next in list. */
    struct splice *splice;       /* Splice event. */
    struct altPath *altPathList; /* Matching of paths to probe sets. */
    int altPathProbeCount;       /* Paths with associated probes. */
    int geneProbeCount;              /* Number of probes on this gene. */
    struct bed **geneBeds;           /* Array of beds that are unique to this gene. */
    double **geneExpVals;            /* Expression values. */
    double **genePVals;              /* Probability of expression. */
    double *avgExpVals;          /* Average value for each exp. */
    double flipScore;            /* Number of times on version - number of times other / sum of both. */
    double percentUltra;           /* Percentage of bases that overlap very conserved elements. */
    double pVal;                   /* PVal of being differentially expressed. */
    boolean isExpressed;         /* Is this altEvent Expressed. */
    boolean isAltExpressed;      /* Is this altEvent alt-expressed? */
};

struct bindSite
/* Binding sites for a protein. */
{
    struct bindSite *next; /* Next in list. */
    char **motifs;     /* List of valid motifs. */
    int motifCount;    /* Number of moitfs to look for. */
    char *rnaBinder;   /* Rna binding protein associated with these sites. */
};


struct unitTest
{
    struct unitTest *next; /* Next in list. */
    boolean (*test)(struct unitTest *test);     /* Function to call for test. */
    char *description;     /* Description of tests. */
    char *errorMsg;        /* Error message returned by test. */
};

/* Create a funtion pointer. */
typedef boolean (*testFunction)(struct unitTest *test);

struct unitTest *tests = NULL;

static struct optionSpec optionSpecs[] =
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"junctionProbes", OPTION_STRING},
    {"spliceFile", OPTION_STRING},
    {"probFile", OPTION_STRING},
    {"intensityFile", OPTION_STRING},
    {"db", OPTION_STRING},
    {"doPathRatios", OPTION_STRING},
    {"brainSpecific", OPTION_STRING},
    {"tissueExpThresh", OPTION_INT},
    {"otherTissExpThresh", OPTION_INT},
    {"useMaxProbeSet", OPTION_BOOLEAN},
    {"browser", OPTION_STRING},
    {"brainSpecificStrict", OPTION_BOOLEAN},
    {"tissueSpecific", OPTION_STRING},
    {"brainTissues", OPTION_STRING},
    {"combinePSets", OPTION_BOOLEAN},
    {"useExonBeds", OPTION_BOOLEAN},
    {"skipMotifControls", OPTION_BOOLEAN},
    {"selectedPValues", OPTION_STRING},
    {"selectedPValFile", OPTION_STRING},
    {"outputProbeMatch", OPTION_STRING},
    {"outputProbePvals", OPTION_STRING},
    {"outputRatioStats", OPTION_STRING},
    {"psToRefGene", OPTION_STRING},
    {"psToKnownGene", OPTION_STRING},
    {"splicePVals", OPTION_STRING},
    {"cassetteBeds", OPTION_STRING},
    {"newDb", OPTION_STRING},
    {"flipScoreSort", OPTION_BOOLEAN},
    {"conservedCassettes", OPTION_STRING},
    {"notHumanCassettes", OPTION_STRING},
    {"pValThresh", OPTION_FLOAT},
    {"controlStats", OPTION_STRING},
    {"sortScore", OPTION_STRING},
    {"plotDir", OPTION_STRING},
    {"minFlip", OPTION_FLOAT},
    {"maxFlip", OPTION_FLOAT},
    {"minIntCons", OPTION_FLOAT},
    {"phastConsScores", OPTION_STRING},
    {"muscleOn", OPTION_BOOLEAN},
    {"presThresh", OPTION_FLOAT},
    {"doTests", OPTION_BOOLEAN},
    {NULL, 0}
};


static char *optionDescripts[] =
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Bed format junction probe sets.",
    "Splice format file containing alternative splicing events.",
    "Data matrix with probability of transcription for each probe set.",
    "Data matrix with intensity estimates for each probe set.",
    "Database freeze of splices.",
    "Generate ratios of probe sets of splice events to each other.",
    "Output brain specific isoforms and dna.",
    "How many tissues have to be seen before called expressed [default=1].",
    "How many other tissues have to be seen before called expressed [default=1].",
    "Instead of using best correlated probe set use all probe sets.",
    "Browser to point at when creating web page.",
    "Require each non-brain tissue to be below absThresh.",
    "Output tissue specific counts for various events to this file.",
    "Comma separated list of tissues to consider brain.",
    "Combine the p-values for a path using Fisher's method.",
    "Use exon beds, not just splice junction beds.",
    "Skip looking at the motifs in the controls, can be quite slow.",
    "File containing list of AFFY G###### ids and gene names.",
    "File containing list of AFFY G###### ids and known gene ids.",
    "File to output pvalue vectors for selected genes to.",
    "Output the splicing events and the probes that were used.",
    "Output the probe level pvals for each alternative event.",
    "Output the ratio for altCassette, alt5, alt3, and altMutExclusive to file.",
    "File with mapping from probe set to gene name.",
    "File with pvalues for specific splices of interest.",
    "Prefix for files to output not-expressed, expressed not alt, and alt cassette beds.",
    "Genome to try and lift coordinates to.",
    "Sort brain specific genes by flip score.",
    "Cassettes that are conserved (in new genome).",
    "Cassettes that are not in human (in new genome).",
    "Threshold to filter pvalue at, default .001",
    "Output some stats for control exons to this file.",
    "Score to replace flipScore for sorting, etc.",
    "Directory plots are in.",
    "Minimum Flip Score to output fasta record for.",
    "Maximum Flip Score to output fasta record for.",
    "Minimum intron conservation percent to output fasta records for.",
    "Conservation scores table where first column is skipping probeset.",
    "Do muscle rather than brain, currently only effects some urls.",
    "Threshold to be declared expressed.",
    "Do some unit tests."
};


int isBrainTissue[256]; /* Global brain identifier. */
struct hash *liftOverHash = NULL; /* Hash holding chains for lifting. */
struct hash *bedHash = NULL; /* Access bed probe sets by hash. */
double presThresh = .9;      /* Threshold for being called present. */
int tissueExpThresh = 1;     /* Number of tissues something must be seen in to
				be considered expressed. */
double absThresh = .1;       /* Threshold for being called not present. */
boolean brainSpecificStrict = FALSE; /* Do we require that all non-brain tissues be below absThresh? */
char *browserName = NULL;    /* Name of browser that we are going to html link to. */
char *db = NULL;             /* Database version used here. */
char *newDb = NULL;             /* Database version lifting to. */
boolean useMaxProbeSet;      /* Instead of using the best correlated probe set, use any
				probe set. */
boolean useComboProbes = FALSE; /* Combine probe sets on a given path using fisher's method. */
boolean useExonBedsToo = FALSE; /* Use exon beds too, not just splice junction beds. */
int intronsConserved = 0;
int intronsConsCounted = 0;
struct slRef *brainSpEvents = NULL; /* List of brain specific events. */
FILE *constMotifCounts = NULL; /* Counts for each motif in constitutive exons. */
FILE *probeMappingsOut = NULL;
FILE *ratioStatOut = NULL;  /* File to output a matrix of ratios of skip/includes. */
FILE *ratioSkipIntenOut = NULL; /* Intensity file for skip paths. */
FILE *ratioIncIntenOut = NULL; /* Intensity file for skip paths. */
FILE *ratioGeneIntenOut = NULL; /* Intensity file for gene probe set. */
FILE *ratioProbOut = NULL; /* Probability gene is expressed. */
FILE *incProbOut = NULL;   /* Include maximium probability values. */
FILE *skipProbOut = NULL;   /* Skip maximum probability values. */
FILE *incProbCombOut = NULL;   /* Include combined probability values. */
FILE *skipProbCombOut = NULL;   /* Skip combined probability values. */

FILE *pathProbabilitiesOut = NULL; /* File to print out path probability vectors. */
FILE *pathExpressionOut = NULL; /* File to print out path probability vectors. */
FILE *pathProbabilitiesBedOut = NULL; /* File to print out corresponding beds for paths. */

double pvalThresh = 2; //.05 / 1263; /* Threshold at which we believe pval is real. */
struct hash *notHumanHash = NULL;    /* Hash of exons that are not even aligned to human. */
struct hash *conservedHash = NULL;   /* Hash of conserved exon events. */
struct hash *splicePValsHash = NULL; /* Hash of pvals of interest. */
struct hash *phastConsHash = NULL; /* Hash of conservation scores of interest. */
struct hash *sortScoreHash = NULL; /* Hash of scores of interest. */
struct hash *outputPValHash = NULL; /* Hash of selected names. */
struct hash *psToRefGeneHash = NULL; /* Hash of probe set to affy genes. */
struct hash *psToKnownGeneHash = NULL; /* Hash of probe set to known genes. */

int otherTissueExpThres = 1; /* Number of other tissues something must be seen in to
				be considered expressed, used */
struct bindSite *bindSiteList = NULL; /* List of rna binding sites to look for. */
struct bindSite **bindSiteArray = NULL; /* array of rna binding sites to look for. */
int bindSiteCount = 0;             /* Number of binding sites to loop through. */
int cassetteExonCount = 0;         /* Number of cassette exons examined. */
int cassetteExonSkipCount = 0;         /* Number of cassette exons examined. */
int *cassetteUpMotifs = NULL;      /* Counts of motifs upstream cassette exons. */
int *cassetteDownMotifs = NULL;    /* Counts of moitfs downstream cassette exons. */
int *cassetteInsideMotifs = NULL;    /* Counts of moitfs downstream cassette exons. */
int *cassetteSkipUpMotifs = NULL;      /* Counts of motifs upstream cassette exons for skipping exons. */
int *cassetteSkipDownMotifs = NULL;    /* Counts of moitfs downstream cassette exon for skipping exons. */
int *cassetteSkipInsideMotifs = NULL;    /* Counts of moitfs downstream cassette exons for skipping exons. */
int cassetteSkipInsideBpCount = 0;    /* Counts of base pairs cassette exons for skipping exons. */
int controlExonCount = 0;         /* Number of control exons examined. */
int *controlUpMotifs = NULL;      /* Counts of motifs upstream cassette exons. */
int *controlDownMotifs = NULL;    /* Counts of moitfs downstream cassette exons. */
int *controlInsideMotifs = NULL;  /* Counts of moitfs inside cassette exons. */
int controlInsideBpCount = 0;  /* Counts of moitfs inside cassette exons. */
boolean skipMotifControls = FALSE; /* Skip looking for motifs in controls. */
FILE *controlValuesOut = NULL;     /* File to output control values to. */
struct valuesMat *controlValuesVm = NULL; /* ValuesMat to keep track of various stats. */
struct valuesMat *brainSpecificValues = NULL; /* Values to keep track of for values. */
int brainSpecificEventCount = 0;  /* How many brain specific events are there. */
int brainSpecificEventCassCount = 0;  /* How many brain specific events are there. */
int brainSpecificEventMutExCount = 0;  /* How many brain specific events are there. */
int brainSpecificPathCount = 0;  /* How many brain specific paths are there. */

FILE *brainSpValuesOut = NULL;  /* Value matrix outputted here. */
FILE *brainSpDnaUpOut = NULL;  /* File for dna around first exon on
			      * brain specific isoforms. */
FILE *brainSpConsDnaOut = NULL;
FILE *brainSpConsBedOut = NULL;
FILE *brainSpDnaUpDownOut = NULL;
FILE *brainSpDnaDownOut = NULL;
FILE *brainSpDnaMergedOut = NULL;
FILE *brainSpBedUpOut = NULL;  /* File for paths (in bed format) that
			      * are brain specific. */
FILE *brainSpBedDownOut = NULL;
FILE *brainSpPathBedOut = NULL;
FILE *brainSpPathEndsBedOut = NULL;
FILE *brainSpPSetOut = NULL;
FILE *brainSpPathBedExonUpOut = NULL;
FILE *brainSpPathBedExonDownOut = NULL;
FILE *brainSpTableHtmlOut = NULL; /* Html table for visualizing brain specific events. */
FILE *brainSpFrameHtmlOut = NULL; /* Html frame for visualizing brain specific events. */


static int *brainSpecificCounts;
FILE *tissueSpecificOut = NULL;    /* File to output tissue specific results to. */
boolean tissueSpecificStrict = FALSE; /* Do we require that all other tissues be below absThresh? */
static int **tissueSpecificCounts; /* Array of counts for different tissue specific splice
				      types per tissue. */
int *brainSpecificMotifUpCounts = NULL;
int *brainSpecificMotifDownCounts = NULL;
int *brainSpecificMotifInsideCounts = NULL;
int brainSpecificMotifInsideBpCount = 0;
FILE *expressedCassetteBedOut = NULL; /* Output beds for casssette exons that are expressed. */
FILE *altCassetteBedOut = NULL;       /* Output beds for cassettte exons that are alt. expressed. */
FILE *notExpressedCassetteBedOut = NULL; /* Output beds for cassette exons that don't look expressed. */

/* Keep track of how many of each event are occuring. */
static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altOtherCount = 0;
static int alt3PrimeSoftCount = 0;
static int alt5PrimeSoftCount = 0;
static int altMutExclusiveCount = 0;
static int altControlCount = 0;

/* Keep track of how many of each event with probes */
static int alt5PrimeWProbeCount = 0;
static int alt3PrimeWProbeCount = 0;
static int altCassetteWProbeCount = 0;
static int altRetIntWProbeCount = 0;
static int altOtherWProbeCount = 0;
static int alt3PrimeSoftWProbeCount = 0;
static int alt5PrimeSoftWProbeCount = 0;
static int altMutExclusiveWProbeCount = 0;
static int altControlWProbeCount = 0;

/* Keep track of how many of each event expressed. */
static int alt5PrimeExpCount = 0;
static int alt3PrimeExpCount = 0;
static int altCassetteExpCount = 0;
static int altRetIntExpCount = 0;
static int altOtherExpCount = 0;
static int alt3PrimeSoftExpCount = 0;
static int alt5PrimeSoftExpCount = 0;
static int altMutExclusiveExpCount = 0;
static int altControlExpCount = 0;

/* Keep track of how many of each event alt-expressed */
static int alt5PrimeAltExpCount = 0;
static int alt3PrimeAltExpCount = 0;
static int altCassetteAltExpCount = 0;
static int altRetIntAltExpCount = 0;
static int altOtherAltExpCount = 0;
static int alt3PrimeSoftAltExpCount = 0;
static int alt5PrimeSoftAltExpCount = 0;
static int altMutExclusiveAltExpCount = 0;
static int altControlAltExpCount = 0;

void usage()
/** Print usage and quit. */
{
int i=0;
warn("altProbes - Match probes to splicing paths and analyze.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   ");
}

struct splice *ndr2CassTest()
/* Create a splice for use in testsing. */
{
char *string = cloneString("chr14	43771400	43772080	chr14.7822-8.26	2	-	9319	38	43765895,43766921,43767127,43767179,43767304,43767340,43767563,43767611,43767860,43767912,43768199,43768244,43768682,43768786,43768944,43769001,43769182,43769269,43769488,43769549,43770672,43770735,43770929,43771050,43771294,43771356,43771400,43771404,43771700,43771742,43772080,43772161,43773937,43774090,43774093,43774180,43774538,43774609,	0,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,1,3,2,1,3,1,3,1,1,2,2,1,2,	2	{\"chr14\",43771400,43772080,0,2,2,{26,30,},24,31,0,},{\"chr14\",43771400,43772080,0,4,4,{26,28,29,30,},24,31,42,},");
struct splice *splice = NULL;
char *words[100];
chopByWhite(string, words, sizeof(words));
splice = spliceLoad(words);
}

struct bed *ndr2BedTest()
/* Create beds used in testing. */
{
char *inc1 = "chr14	43771385	43771715	G6912708@J918653_RC@j_at	0	-	43771385	43771715	0	2	15,15,	0,315,";
char *skip = "chr14	43771385	43772095	G6912708@J918654_RC@j_at	0	-	43771385	43772095	0	2	15,15,	0,695,";
char *inc2 = "chr14	43771727	43772095	G6912708@J918655_RC@j_at	0	-	43771727	43772095	0	2	15,15,	0,353,";
char *gene = "chr14	43765981	43772151	G6912708_RC_a_at	0	-	43765981	43772151	0	9	25,25,25,25,25,25,25,25,43,	0,149,1602,3222,3262,3516,5000,5392,6127,";
struct bed *bedList = NULL, *bed = NULL;
char *words[12];

chopByWhite(cloneString(inc1), words, ArraySize(words));
bed = bedLoadN(words,12);
slAddHead(&bedList, bed);

chopByWhite(cloneString(skip), words, ArraySize(words));
bed = bedLoadN(words,12);
slAddHead(&bedList, bed);

chopByWhite(cloneString(inc2), words, ArraySize(words));
bed = bedLoadN(words,12);
slAddHead(&bedList, bed);

chopByWhite(cloneString(gene), words, ArraySize(words));
bed = bedLoadN(words,12);
slAddHead(&bedList, bed);

slReverse(&bedList);
return bedList;
}

struct dMatrix *ndr2BedTestMat(boolean probability)
/* Create a fake matrix of probabilities. If probability is TRUE fill
 in a probability matrix, otherwise fill in an intensity matrix. */
{
struct dMatrix *probs = NULL;
static char *colNames[] = {"cerebellum", "cortex", "heart", "skeletal"};
static char *rowNames[] = {"G6912708@J918653_RC@j_at", "G6912708@J918654_RC@j_at",
		    "G6912708@J918655_RC@j_at", "G6912708_RC_a_at"};
double probM[4][4] = {{.8, 1, 0, .5},{0, 0, 0, 1}, {.8, 0, 0, .5}, {1, 0, .5, 1}};
double intenM[4][4] = {{1, 1, 1, 2.5}, {1, 1, 1, 1}, {1, 1, 0, 1.5}, {1, 1, 1, .5}};
double **matrix = NULL;
int count = 0;
AllocVar(probs);
AllocArray(matrix, ArraySize(rowNames));
probs->nameIndex = newHash(3);

for(count = 0; count < ArraySize(colNames); count++)
    {
    if(probability)
	matrix[count] = CloneArray(probM[count], ArraySize(probM[count]));
    else
	matrix[count] = CloneArray(intenM[count], ArraySize(intenM[count]));
    hashAddInt(probs->nameIndex, rowNames[count], count);
    }

probs->colNames = colNames;
probs->colCount = ArraySize(colNames);
probs->rowNames = rowNames;
probs->rowCount = ArraySize(rowNames);
probs->matrix = matrix;
return probs;
}

struct valuesMat *newValuesMat(int numCol, int numRow)
/* Create a new values matrix. */
{
struct valuesMat *vm = NULL;
int i = 0;
AllocVar(vm);
vm->maxCols = numCol;
vm->maxRows = numRow;
AllocArray(vm->colNames, vm->maxCols);
AllocArray(vm->rowNames, vm->maxRows);
AllocArray(vm->vals, vm->maxRows);
for(i = 0; i < vm->maxRows; i++)
    AllocArray(vm->vals[i], vm->maxCols);
vm->rowIndex = newHash(round(logBase2(vm->maxCols) + .5));
return vm;
}

void vmAddVal(struct valuesMat *vm, char *rowName, char *colName, char *val)
/* Add item val to rowName in correct column. */
{
int i = 0;
int colIx = -1;
int rowIx = -1;
assert(vm);
assert(vm->rowIndex);
assert(rowName);
assert(val);
assert(colName);
for( i = 0; i < vm->colCount; i++)
    if(sameString(vm->colNames[i], colName))
	{
	colIx = i;
	break;
	}

/* If column not existent add it. */
if(colIx == -1)
    {
    if(vm->colCount + 1 >= vm->maxCols)
	{
	ExpandArray(vm->colNames, vm->maxCols, vm->maxCols+1);
	for(i = 0; i < vm->rowCount; i++)
	    ExpandArray(vm->vals[i], vm->maxCols, vm->maxCols+1);
	vm->maxCols++;
	}
    colIx = vm->colCount;
    vm->colNames[colIx] = cloneString(colName);
    vm->colCount++;
    }

/* Get the row for that identifier. */
rowIx = hashIntValDefault(vm->rowIndex, rowName, -1);
if(rowIx == -1)
    {
    if(vm->rowCount + 1 >= vm->maxRows)
	{
	ExpandArray(vm->rowNames, vm->maxRows, vm->maxRows+1);
	ExpandArray(vm->vals, vm->maxRows, vm->maxRows+1);
	AllocArray(vm->vals[vm->maxRows], vm->maxCols);
	vm->maxRows++;
	}
    rowIx = vm->rowCount;
    vm->rowNames[rowIx] = cloneString(rowName);
    hashAddInt(vm->rowIndex, rowName, rowIx);
    vm->rowCount++;
    }
vm->vals[rowIx][colIx] = cloneString(val);
}

void vmWriteVals(struct valuesMat *vm, FILE *out)
/* Writ out the vm. */
{
int colIx = 0, rowIx = 0;
for(colIx = 0; colIx < vm->colCount - 1; colIx++)
    fprintf(out, "%s\t", vm->colNames[colIx]);
fprintf(out, "%s\n", vm->colNames[colIx]);

for(rowIx = 0; rowIx < vm->rowCount; rowIx++)
    {
    fprintf(out, "%s\t", vm->rowNames[rowIx]);
    for(colIx = 0; colIx < vm->colCount -1; colIx++)
	fprintf(out, "%s\t", vm->vals[rowIx][colIx]);
    fprintf(out, "%s\n", vm->vals[rowIx][colIx]);
    }
}

void logSpliceType(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type)
    {
    case alt5Prime:
	alt5PrimeCount++;
	break;
    case alt3Prime:
	alt3PrimeCount++;
	break;
    case altCassette:
	altCassetteCount++;
	break;
    case altRetInt:
	altRetIntCount++;
	break;
    case altOther:
	altOtherCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftCount++;
	break;
    case altMutExclusive:
	altMutExclusiveCount++;
	break;
    case altControl:
	altControlCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}


void logSpliceTypeWProbe(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type)
    {
    case alt5Prime:
	alt5PrimeWProbeCount++;
	break;
    case alt3Prime:
	alt3PrimeWProbeCount++;
	break;
    case altCassette:
	altCassetteWProbeCount++;
	break;
    case altRetInt:
	altRetIntWProbeCount++;
	break;
    case altOther:
	altOtherWProbeCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftWProbeCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftWProbeCount++;
	break;
    case altMutExclusive:
	altMutExclusiveWProbeCount++;
	break;
    case altControl:
	altControlWProbeCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void logSpliceTypeExp(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type)
    {
    case alt5Prime:
	alt5PrimeExpCount++;
	break;
    case alt3Prime:
	alt3PrimeExpCount++;
	break;
    case altCassette:
	altCassetteExpCount++;
	break;
    case altRetInt:
	altRetIntExpCount++;
	break;
    case altOther:
	altOtherExpCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftExpCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftExpCount++;
	break;
    case altMutExclusive:
	altMutExclusiveExpCount++;
	break;
    case altControl:
	altControlExpCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void logSpliceTypeAltExp(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type)
    {
    case alt5Prime:
	alt5PrimeAltExpCount++;
	break;
    case alt3Prime:
	alt3PrimeAltExpCount++;
	break;
    case altCassette:
	altCassetteAltExpCount++;
	break;
    case altRetInt:
	altRetIntAltExpCount++;
	break;
    case altOther:
	altOtherAltExpCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftAltExpCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftAltExpCount++;
	break;
    case altMutExclusive:
	altMutExclusiveAltExpCount++;
	break;
    case altControl:
	altControlAltExpCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void initSplicePVals()
/* Load up the pvals of interest generated in a separate file. */
{
struct lineFile *lf = NULL;
char *fileName = optionVal("splicePVals", NULL);
char *words[2];
struct slDouble *sd = NULL;
assert(fileName);
splicePValsHash = newHash(10);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    char *mark = NULL;
    hashAdd(splicePValsHash, words[0], slDoubleNew(sqlDouble((words[1]))));
    }
lineFileClose(&lf);
}

double pvalForSplice(char *name)
/* Return a pval as specified in the splicePVal file or 2 if not there. */
{
struct slDouble *d = NULL;
if(splicePValsHash == NULL)
    errAbort("Need to specify -splicePVal file if using pvalForSplice");
assert(name);
d = hashFindVal(splicePValsHash, name);
if(d == NULL)
    return 2.0;
return d->val;
}

void initPhastConsScores()
/* Load up the conservation scores of interest generated in a separate file. */
{
struct lineFile *lf = NULL;
char *fileName = optionVal("phastConsScores", NULL);
char *words[2];
struct slDouble *sd = NULL;
assert(fileName);
phastConsHash = newHash(10);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    hashAdd(phastConsHash, words[0], slDoubleNew(sqlDouble((words[1]))));
    }
lineFileClose(&lf);
}

double phastConsForSplice(char *name)
/* Return a phast cons score associated with the splicing event. */
{
struct slDouble *d = NULL;
if(phastConsHash == NULL)
    errAbort("Need to specify -phastConsScores file if using phastConsForSplice");
assert(name);
d = hashFindVal(phastConsHash, name);
if(d == NULL)
    return -1;
return d->val;
}

void initSortScore()
/* Load up the pvals of interest generated in a separate file. */
{
struct lineFile *lf = NULL;
char *fileName = optionVal("sortScore", NULL);
char *words[2];
struct slDouble *sd = NULL;
assert(fileName);
sortScoreHash = newHash(10);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    char *mark = NULL;
    double d = 0;
    d = sqlDouble(words[1]);
    sd = slDoubleNew(d);
    hashAddUnique(sortScoreHash, words[0], sd);
    }
lineFileClose(&lf);
}

double sortScoreForSplice(char *name)
/* Return a pval as specified in the splicePVal file or 2 if not there. */
{
struct slDouble *d = NULL;
if(sortScoreHash == NULL)
    errAbort("Need to specify -sortScore file if using sortScoreForSplice");
assert(name);
d = hashFindVal(sortScoreHash, name);
if(d == NULL)
    return 0;
return d->val;
}

void initPsToRefGene()
/* Load up a file that converts from affy psName to gene names. */
{
struct lineFile *lf = NULL;
char *fileName = optionVal("psToRefGene", NULL);
char *words[2];
assert(fileName);
psToRefGeneHash = newHash(10);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    hashAdd(psToRefGeneHash, words[1], cloneString(words[0]));
    }
lineFileClose(&lf);
}

char *refSeqForPSet(char *pSet)
/* Look up the refseq name for a probe set. */
{
static boolean warned = FALSE;
char *gene = NULL;
if(psToRefGeneHash == NULL)
    {
    if(warned != TRUE)
	warn("Can't convert psName to refSeq name, have to specify psToRefGene flag.");
    warned = TRUE;
    return pSet;
    }
gene = hashFindVal(psToRefGeneHash, pSet);
if(gene == NULL)
    return pSet;
return gene;
}

void initPsToKnownGene()
/* Load up a file that converts from affy psName to gene names. */
{
struct lineFile *lf = NULL;
char *fileName = optionVal("psToKnownGene", NULL);
char *words[2];
assert(fileName);
psToKnownGeneHash = newHash(10);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    hashAdd(psToKnownGeneHash, words[1], cloneString(words[0]));
    }
lineFileClose(&lf);
}

char *knownGeneForPSet(char *pSet)
/* Look up the refseq name for a probe set. */
{
static boolean warned = FALSE;
char *gene = NULL;
if(psToKnownGeneHash == NULL)
    {
    if(warned != TRUE)
	errAbort("Can't convert psName to known name, have to specify psToKnowngene flag.");
    warned = TRUE;
    return pSet;
    }
gene = hashFindVal(psToKnownGeneHash, pSet);
if(gene == NULL)
    return pSet;
return gene;
}


void initBindSiteList()
/* Create a little hand curated knowledge about binding sites. */
{
struct bindSite *bsList = NULL, *bs = NULL;
int i = 0;

/* Nova-1 */
AllocVar(bs);
bs->motifCount = 2;
AllocArray(bs->motifs, bs->motifCount);
/* Expand TCATY */
bs->motifs[i++] = cloneString("TCATT");
bs->motifs[i++] = cloneString("TCATC");
bs->rnaBinder = cloneString("Nova-1");
slAddHead(&bsList, bs);

/* Fox-1 */
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[0] = cloneString("GCATG");
bs->rnaBinder = cloneString("Fox-1");
slAddHead(&bsList, bs);

/* PTB */
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[0] = cloneString("CTCTCT");
bs->rnaBinder = cloneString("PTB/nPTB");
slAddHead(&bsList, bs);

/* hnRNPs */
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[0] = cloneString("GGGG");
bs->rnaBinder = cloneString("hnRNP-H/F");
slAddHead(&bsList, bs);

/* /\* First motif... *\/ */
/* i = 0; */
/* AllocVar(bs); */
/* bs->motifCount = 16; */
/* AllocArray(bs->motifs, bs->motifCount); */
/* /\* (YNCURY; Y, pyrimidine; R, purine; N, any nucleotide; the branch point adenosine is underlined*\/ */
/* bs->motifs[i++] = cloneString("TACTAAT"); */
/* bs->motifs[i++] = cloneString("TGCTAAT"); */
/* bs->motifs[i++] = cloneString("TTCTAAT"); */
/* bs->motifs[i++] = cloneString("TCCTAAT"); */

/* bs->motifs[i++] = cloneString("TACTAAC"); */
/* bs->motifs[i++] = cloneString("TGCTAAC"); */
/* bs->motifs[i++] = cloneString("TTCTAAC"); */
/* bs->motifs[i++] = cloneString("TCCTAAC"); */

/* bs->motifs[i++] = cloneString("TATTAAT"); */
/* bs->motifs[i++] = cloneString("TGTTAAT"); */
/* bs->motifs[i++] = cloneString("TTTTAAT"); */
/* bs->motifs[i++] = cloneString("TCTTAAT"); */

/* bs->motifs[i++] = cloneString("TATTAAC"); */
/* bs->motifs[i++] = cloneString("TGTTAAC"); */
/* bs->motifs[i++] = cloneString("TTTTAAC"); */
/* bs->motifs[i++] = cloneString("TCTTAAC"); */
/* bs->rnaBinder = cloneString("TNCTAAC/T"); */
/* slAddHead(&bsList, bs); */

i = 0;
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[i++] = cloneString("CTAAC");
bs->rnaBinder = cloneString("CTAAC");
slAddHead(&bsList, bs);

i = 0;
AllocVar(bs);
bs->motifCount = 2;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[i++] = cloneString("TGCTTTC");
bs->motifs[i++] = cloneString("TGTTTTC");
bs->rnaBinder = cloneString("TGYTTTC");
slAddHead(&bsList, bs);

i = 0;
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[i++] = cloneString("TAGGG");
bs->rnaBinder = cloneString("hnRNP-A1");
slAddHead(&bsList, bs);

i = 0;
AllocVar(bs);
bs->motifCount = 2;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[i++] = cloneString("TCCTTT");
bs->motifs[i++] = cloneString("TGCTTT");
bs->rnaBinder = cloneString("TG/CCTTT");
slAddHead(&bsList, bs);

/* i = 0; */
/* AllocVar(bs); */
/* bs->motifCount = 2; */
/* AllocArray(bs->motifs, bs->motifCount); */
/* bs->motifs[i++] = cloneString("TGCTTTCC"); */
/* bs->motifs[i++] = cloneString("TGTTTTCC"); */
/* bs->rnaBinder = cloneString("TGC/TTTTCC"); */
/* slAddHead(&bsList, bs); */

/* First discovered motif... */
AllocVar(bs);
bs->motifCount = 1;
AllocArray(bs->motifs, bs->motifCount);
bs->motifs[0] = cloneString("TCCTT");
bs->rnaBinder = cloneString("TCCTT");
slAddHead(&bsList, bs);

slReverse(&bsList);
bindSiteList = bsList;
AllocArray(bindSiteArray, slCount(bsList));
for(bs = bsList; bs != NULL; bs = bs->next)
    bindSiteArray[bindSiteCount++] = bs;

AllocArray(cassetteUpMotifs, bindSiteCount);
AllocArray(cassetteDownMotifs, bindSiteCount);
AllocArray(cassetteInsideMotifs, bindSiteCount);
AllocArray(cassetteSkipUpMotifs, bindSiteCount);
AllocArray(cassetteSkipDownMotifs, bindSiteCount);
AllocArray(cassetteSkipInsideMotifs, bindSiteCount);
AllocArray(controlUpMotifs, bindSiteCount);
AllocArray(controlDownMotifs, bindSiteCount);
AllocArray(controlInsideMotifs, bindSiteCount);
}

boolean isJunctionBed(struct bed *bed)
/* Return TRUE if bed looks like a junction probe. Specifically
   two blocks, each 15bp. FALSE otherwise. */
{
if(bed->blockCount == 2 && bed->blockSizes[0] == 15 && bed->blockSizes[1] == 15)
    return TRUE;
return FALSE;
}

boolean validVertex(struct splice *splice, int v1, int v2)
/* Check to see if a given vertex is the sink or source of the
   graph and should be ignored. */
{
return (v1 >= 0 && v1 < splice->vCount && v2 >= 0 && v2 < splice->vCount);
}

enum ggEdgeType pathEdgeTypeValid(struct splice *s, int v1, int v2)
{
assert(validVertex(s, v1,v2));
return pathEdgeType(s->vTypes, v1, v2);
}

boolean pathContainsIntron(struct splice *splice, struct path *path, char *chrom,
			   int chromStart, int chromEnd, char *strand)
/* Return TRUE if this path contains an intron (splice junction) that
   starts at chromStart and ends at chromEnd. */
{
int i = 0;
int *vPos = splice->vPositions;
int *verts = path->vertices;
if(differentString(splice->tName, chrom) ||
   differentString(splice->strand, strand))
    return FALSE;

/* Check the edges on the path. */
for(i = 0; i < path->vCount - 1; i++)
    {
    if(validVertex(splice, verts[i], verts[i+1]) &&
       pathEdgeTypeValid(splice, verts[i], verts[i+1]) == ggSJ)
	{
	if(chromStart == vPos[verts[i]] && chromEnd == vPos[verts[i+1]])
	    return TRUE;
	}
    }
return FALSE;
}

boolean pathContainsBlock(struct splice *splice, struct path *path, char *chrom,
			  int chromStart, int chromEnd, char *strand,
			  boolean allBases)
/* Return TRUE if this block is contained in the path, FALSE otherwise.
   If allBases then every base in chromStart-chromEnd must be covered
   by path. */
{
int i = 0;
int *vPos = splice->vPositions;
int *verts = path->vertices;
if(differentString(splice->tName, chrom) ||
   differentString(splice->strand, strand))
    return FALSE;

/* Check the first edge. */
if(validVertex(splice, path->upV, verts[0]) &&
   pathEdgeTypeValid(splice, path->upV,verts[0]) == ggExon)
    {
    if(chromStart >= vPos[path->upV] && chromEnd <= vPos[verts[0]])
	return TRUE;
    else if(!allBases && rangeIntersection(chromStart, chromEnd,
					   vPos[path->upV], vPos[verts[0]]) > 0)
	return TRUE;
    }

/* Check the last edge. */
if(validVertex(splice, verts[path->vCount -1], path->downV) &&
   pathEdgeTypeValid(splice, verts[path->vCount - 1], path->downV) == ggExon)
    {
    if(chromStart >= vPos[verts[path->vCount - 1]] &&
       chromEnd <= vPos[path->downV])
	return TRUE;
    else if(!allBases &&
	    rangeIntersection(chromStart, chromEnd,
			      vPos[verts[path->vCount - 1]], vPos[path->downV])  > 0)
	return TRUE;
    }

/* Check the edges on the path. */
for(i = 0; i < path->vCount - 1; i++)
    {
    if(validVertex(splice, verts[i], verts[i+1]) &&
       pathEdgeTypeValid(splice, verts[i], verts[i+1]) == ggExon)
	{
	if(chromStart >= vPos[verts[i]] && chromEnd <= vPos[verts[i+1]])
	    return TRUE;
	else if(!allBases && rangeIntersection(chromStart, chromEnd,
					       vPos[verts[i]], vPos[verts[i+1]]) > 0)
	    return TRUE;
	}
    }
return FALSE;
}

boolean pathContainsBed(struct splice *splice, struct path *path,
			struct bed *bed,  boolean intronsToo)
/* Return TRUE if this path contains this bed, FALSE otherwise. If
   allBases, the bed must be completely subsumed by the path. */
{
boolean containsBed = TRUE;
int i = 0;
assert(bed);
for(i = 0; i < bed->blockCount; i++)
    {
    int chromStart = bed->chromStart + bed->chromStarts[i];
    int chromEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];

    /* Check the exon. */
    containsBed &= pathContainsBlock(splice, path, bed->chrom, chromStart,
				     chromEnd, bed->strand, TRUE);
    if(containsBed == FALSE)
	break;

    /* Check the next intron. */
    if(intronsToo && i+1 != bed->blockCount)
	containsBed &= pathContainsIntron(splice, path, bed->chrom, chromEnd,
					  bed->chromStarts[i+1]+chromStart, bed->strand);
    }
return containsBed;
}

void insertBedIntoPath(struct altEvent *altEvent, struct altPath *altPath,
		       struct bed *bed)
/* Add the cbed to this path. */
{
int pCount = 0;
assert(altPath);
pCount = altPath->probeCount;
ExpandArray(altPath->beds, pCount, pCount+1);
ExpandArray(altPath->bedTypes, pCount, pCount+1);
altPath->beds[altPath->probeCount++] = bed;
}

int findProbeSetsForEvents(struct altEvent *altEvent)
/* Look for all of the beds that overlap a splice and match
   them the ones that are uniq to a path. Returns the number
   of probes that map to this splice. */
{
struct binElement *be = NULL, *beList = NULL;
struct altPath *altPath = NULL, *altMatchPath = NULL;
struct bed *bed = NULL;

assert(altEvent);
struct splice *splice = altEvent->splice;
/* Load all the beds that span this range. */
beList = chromKeeperFind(splice->tName, splice->tStart, splice->tEnd);
for(be = beList; be != NULL; be = be->next)
    {
    bed = be->val;
    if(isJunctionBed(bed) || (useExonBedsToo && strchr(bed->name, '@') != NULL))
	{
	altMatchPath = NULL; /* Indicates that we haven't found a match for this bed. */
	for(altPath = altEvent->altPathList; altPath != NULL; altPath = altPath->next)
	    {
	    /* If it is a junction pass the intronsToo flag == TRUE, otherwise
	       set to false. */
	    if((isJunctionBed(bed) && pathContainsBed(splice, altPath->path, bed, TRUE)) ||
	       (!isJunctionBed(bed) && pathContainsBed(splice, altPath->path, bed, FALSE)))
		{
		if(altMatchPath == NULL)
		    altMatchPath = altPath; /* So far unique match. */
		else
		    { /* Not a unique match. */
		    altMatchPath = NULL;
		    break;
		    }
		}
	    }
	}
    else
	continue;
    /* If we've uniquely found a path for a bed insert it. */
    if(altMatchPath != NULL)
	insertBedIntoPath(altEvent, altMatchPath, bed);
    }

/* Count up how many paths have probes. */
for(altPath = altEvent->altPathList; altPath != NULL; altPath = altPath->next)
    if(altPath->probeCount > 0)
	altEvent->altPathProbeCount++;
}

void bedLoadChromKeeper()
/* Load the beds's in the file into the chromKeeper. */
{
char *db = optionVal("db", NULL);
char *bedFile = optionVal("junctionProbes", NULL);
struct bed *bed = NULL, *bedList = NULL;
bedHash = newHash(12);
if(db == NULL)
    errAbort("Must specify a database when loading beds.");
if(bedFile == NULL)
    errAbort("Must specify a file for loading beds.");
chromKeeperInit(db);
bedList = bedLoadAll(bedFile);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    chromKeeperAdd(bed->chrom, bed->chromStart, bed->chromEnd, bed);
    hashAdd(bedHash, bed->name, bed);
    }
}

struct altEvent *altEventsFromSplices(struct splice *spliceList)
/* Create the altEvents and altPaths from the spliceList. */
{
struct splice *splice = NULL;
struct path *path = NULL;
struct altEvent *event = NULL, *eventList = NULL;
struct altPath *altPath = NULL, *altPathList = NULL;
for(splice = spliceList; splice != NULL; splice = splice->next)
    {
    AllocVar(event);
    event->splice = splice;
    for(path = splice->paths; path != NULL; path = path->next)
	{
	AllocVar(altPath);
	altPath->score = 1;
	altPath->path = path;
	slAddHead(&event->altPathList, altPath);
	}
    slReverse(&event->altPathList);
    slAddHead(&eventList, event);
    }
slReverse(&eventList);
return eventList;
}

void fillInAltPathData(struct altPath *altPath, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Fill in the data for the probes that map to this
   altPath. */
{
int i = 0;
int pCount = altPath->probeCount;

if(pCount == 0)
    return;

AllocArray(altPath->expVals, pCount);
AllocArray(altPath->pVals, pCount);
AllocArray(altPath->avgExpVals, pCount);
for(i = 0; i < pCount; i++)
    {
    int index = hashIntValDefault(intenM->nameIndex, altPath->beds[i]->name, -1);
    if(index != -1)
	altPath->expVals[i] = intenM->matrix[index];
    index = hashIntValDefault(probM->nameIndex, altPath->beds[i]->name, -1);
    if(index != -1)
	altPath->pVals[i] = probM->matrix[index];
    }
}

char *altGeneNameForPSet(struct bed *bed)
/* Do some parsing of the bed name and assemble
   a gene name. */
{
static char altGeneName[256]; /* static so memory char * can be returned. */
char buff[256];
char *mark = NULL;
char *extra = "EX";
*altGeneName = '\0';
safef(buff, sizeof(buff), "%s", bed->name);
mark = strchr(buff, '@');
if(mark != NULL)
    {
    *mark = '\0';

    /* If not looking for altSet skip the extra string. */
    if(stringIn("RC", bed->name))
	safef(altGeneName, sizeof(altGeneName), "%s%s_RC_a_at", buff, extra);
    else
	safef(altGeneName, sizeof(altGeneName), "%s%s_a_at", buff, extra);
    return altGeneName;
    }
return NULL;

}

char *geneNameForPSet(struct bed *bed)
/* Do some parsing of the bed name and assemble
   a gene name. */
{
static char geneName[256]; /* static so memory char * can be returned. */
char buff[256];
char *mark = NULL;
char *extra = "EX";
*geneName = '\0';
safef(buff, sizeof(buff), "%s", bed->name);
mark = strchr(buff, '@');
if(mark != NULL)
    {
    *mark = '\0';

    /* If not looking for altSet skip the extra string. */
    extra = "";
    if(stringIn("RC", bed->name))
	safef(geneName, sizeof(geneName), "%s%s_RC_a_at", buff, extra);
    else
	safef(geneName, sizeof(geneName), "%s%s_a_at", buff, extra);
    return geneName;
    }
return NULL;

}

void fillInGeneData(struct altEvent *altEvent, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Fill in the gene probe set data for this event. */
{
int i = 0;
struct altPath *altPath = NULL;

for(altPath = altEvent->altPathList; altPath != NULL; altPath = altPath->next)
    {
    if(altPath->probeCount > 0)
	{
	int index = -1;
	int altIndex = -1;
	char *geneName = NULL, *altGeneName = NULL;
	int geneCount = 0;
	/* Find the indexes of the possible gene sets. */
	geneName = geneNameForPSet(altPath->beds[0]);
	index = hashIntValDefault(intenM->nameIndex, geneName, -1);
	altGeneName = altGeneNameForPSet(altPath->beds[0]);
	altIndex = hashIntValDefault(intenM->nameIndex, altGeneName, -1);

	/* Count how many probes we found. */
	if(index != -1)
	    geneCount++;
	if(altIndex != -1)
	    geneCount++;

	if(geneCount > 0)
	    {
	    /* Allocate some memory. */
	    AllocArray(altEvent->geneBeds, geneCount);
	    AllocArray(altEvent->geneExpVals, geneCount);
	    AllocArray(altEvent->genePVals, geneCount);
	    AllocArray(altEvent->avgExpVals, geneCount);
	    altEvent->geneProbeCount = geneCount;

	    /* If we found the main name look it up. */
	    if(index != -1)
		{
		altEvent->geneBeds[0] = hashFindVal(bedHash, geneName);
		altEvent->geneExpVals[0] = intenM->matrix[index];
		index =  hashIntValDefault(probM->nameIndex, geneName, -1);
		if(index != -1)
		    altEvent->genePVals[0] = probM->matrix[index];
		}
	    /* Try the alternative gene probe set. */
	    if(altIndex != -1)
		{
		int offSet = geneCount - 1;
		altEvent->geneBeds[offSet] = hashFindVal(bedHash, altGeneName);
		altEvent->geneExpVals[offSet] = intenM->matrix[altIndex];
		index =  hashIntValDefault(probM->nameIndex, altGeneName, -1);
		if(index != -1)
		    altEvent->genePVals[offSet] = probM->matrix[index];
		}
	    }
	}
    }
}

void fillInEventData(struct altEvent *eventList, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Loop through the eventList and fill in the altPaths with
   data in intenM and probM. */
{
struct altEvent *event = NULL;
struct altPath *altPath = NULL;
for(event = eventList; event != NULL; event = event->next)
    {
    for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
	{
	fillInAltPathData(altPath, intenM, probM);
	}
    fillInGeneData(event, intenM, probM);
    }
}

double geneExpression(struct altEvent *event, int tissueIx)
/* Return maximum pval for all the gene sets. */
{
double maxInten = -1;
int geneIx = 0;
for(geneIx = 0; geneIx < event->geneProbeCount; geneIx++)
    {
    if(event->geneExpVals[geneIx] != NULL &&
       (event->geneExpVals[geneIx][tissueIx] >= maxInten))
	maxInten = event->geneExpVals[geneIx][tissueIx];
    }
return maxInten;
}


double genePVal(struct altEvent *event, int tissueIx)
/* Return maximum pval for all the gene sets. */
{
double maxPval = -1;
int geneIx = 0;
for(geneIx = 0; geneIx < event->geneProbeCount; geneIx++)
    {
    if(event->genePVals[geneIx] != NULL &&
       (event->genePVals[geneIx][tissueIx] >= maxPval))
	maxPval = event->genePVals[geneIx][tissueIx];
    }
return maxPval;
}


boolean geneExpressed(struct altEvent *event, int tissueIx)
/* Return TRUE if any of the gene probe sets are expressed in
   this tissue. */
{
if(genePVal(event, tissueIx) >= presThresh)
    return TRUE;
return FALSE;
}

void printVars(double *preLog, double *log)
{
printf("Pre-Log %.20f, Log() %.20f\n", preLog, log);
}

double pvalAltPathMaxExpressed(struct altPath *altPath, int tissueIx)
/* Return the maximum pval attained for this path. */
{
double max = 0;
int i = 0;
for(i = 0; i < altPath->probeCount; i++)
    {
    if(altPath->pVals[i] && altPath->pVals[i][tissueIx] >= max)
	{
	max = altPath->pVals[i][tissueIx];
	}
    }
return max;
}

double pvalAltPathCombExpressed(struct altPath *altPath, int tissueIx)
/* Combine pvals that path expressed using fisher's method and return
   overall prob that expressed. */
{
double probProduct = 0;
int probCount = 0;
int i = 0;
double combination = 0;
double x = 0;
for(i = 0; i < altPath->probeCount; i++)
    {
    if(altPath->pVals[i])
	{
	/* Check for log 0, make it just log very small number. */
	if(altPath->pVals[i][tissueIx] == 0)
	    x = log(.00000001);
	else
	    x = log(altPath->pVals[i][tissueIx]);
	probProduct += x;
	probCount++;
	}
    }
/* If no probes return 0. */
if(probCount == 0)
    return 0;
combination = gsl_cdf_chisq_P(-2.0*probProduct,2.0*probCount);
return combination;
}

boolean altPathProbesCombExpressed(struct altEvent *event, struct altPath *altPath,
			       int probeIx, int tissueIx)
/* Return TRUE if the path is expressed, FALSE otherwise. */
{
double combination = 1;
assert(altPath);
combination = pvalAltPathCombExpressed(altPath, tissueIx);
if(combination <= altPath->score && event->genePVals && geneExpressed(event, tissueIx))
    altPath->score = combination;
if(combination <= 1 - presThresh)
    return TRUE;
return FALSE;
}

boolean altPathProbesExpressed(struct altEvent *event, struct altPath *altPath,
			       int probeIx, int tissueIx, double *expression)
/* Return TRUE if the path is expressed, FALSE otherwise. */
{
int i = 0;
int probeCount = 0;
double **expVals = altPath->expVals;
boolean expressed = FALSE;
if(useMaxProbeSet)
    {
    for(i = 0; i < altPath->probeCount; i++)
	{
	if(altPath->pVals[i] && altPath->pVals[i][tissueIx] >= presThresh)
	    {
	    expressed = TRUE;
	    *expression = max(*expression, expVals[i][tissueIx]);
	    }
	}
    }
else if(useComboProbes)
    {
    if(altPathProbesCombExpressed(event, altPath, probeIx, tissueIx))
	expressed = TRUE;
    if(expVals[i] != NULL)
	{
	for(i = 0; i < altPath->probeCount; i++)
	    {
	    *expression += expVals[i][tissueIx];
	    probeCount++;
	    }
	}
    if(probeCount > 0)
	*expression = *expression / probeCount;
    }
else
    {
    if(altPath->pVals[probeIx] && altPath->pVals[probeIx][tissueIx] >= presThresh)
	{
	expressed = TRUE;
	*expression = altPath->expVals[probeIx][tissueIx];
	}
    }
return expressed;
}

boolean tissueExpressed(struct altEvent *event, struct altPath *altPath,
			int probeIx, int tissueIx, double *expression)
/* Return TRUE if the probeIx in tissueIx is above minimum
   pVal, FALSE otherwise. */
{
int geneIx = 0;
if(!altPathProbesExpressed(event, altPath, probeIx, tissueIx, expression))
    return FALSE;
if(event->genePVals && geneExpressed(event, tissueIx))
    return TRUE;
return FALSE;
}

boolean altPathProbesNotExpressed(struct altEvent *event, struct altPath *altPath,
			       int probeIx, int tissueIx)
/* Return TRUE if the path is notExpressed, FALSE otherwise. */
{
boolean notExpressed = FALSE;
int i = 0;
if(useMaxProbeSet)
    {
    for(i = 0; i < altPath->probeCount; i++)
	{
	if(altPath->pVals[i] && altPath->pVals[i][tissueIx] <= absThresh)
	    notExpressed = TRUE;
	}
    }
else
    {
    if(altPath->pVals[probeIx] && altPath->pVals[probeIx][tissueIx] <= absThresh)
	notExpressed = TRUE;
    }
return notExpressed;
}

boolean tissueNotExpressed(struct altEvent *event, struct altPath *altPath,
			int probeIx, int tissueIx)
/* Return TRUE if the probeIx in tissueIx is above minimum
   pVal, FALSE otherwise. */
{
int geneIx = 0;
if(!altPathProbesNotExpressed(event, altPath, probeIx, tissueIx))
    return FALSE;
return TRUE;
}

double covariance(double *X, double *Y, int count)
/* Compute the covariance for two vectors.
   cov(X,Y) = E[XY] - E[X]E[Y]
   page 326 Sheldon Ross "A First Course in Probability" 1998
*/
{
double cov = gsl_stats_covariance(X, 1, Y, 1, count);
return cov;
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

int determineBestProbe(struct altEvent *event, struct altPath *altPath,
		       struct dMatrix *intenM, struct dMatrix *probM,
		       int pathIx)
/* Find the probe with the best correlation to the gene probe sets. */
{
double corr = 0, bestCorr = -2;
int bestIx = -1;
int bedIx = 0, geneIx = 0;

/* Check the easy case. */
if(altPath->probeCount == 1)
    return 0;

/* Loop through and find the probe set with the best
   correlation to the gene sets. */
for(bedIx = 0; bedIx < altPath->probeCount; bedIx++)
    {
    if(altPath->expVals[bedIx] == NULL)
	continue;
    for(geneIx = 0; geneIx < event->geneProbeCount; geneIx++)
	{
	if(event->geneExpVals[geneIx] == NULL)
	    continue;
	corr = correlation(altPath->expVals[bedIx],
				  event->geneExpVals[geneIx], intenM->colCount);
	if(corr >= bestCorr)
	    {
	    bestIx = bedIx;
	    bestCorr = corr;
	    }
	}
    }
return bestIx;
}

void readSelectedList(char *fileName)
/* Read in a list of gene identifiers that we want to
   output. */
{
struct lineFile *lf = NULL;
struct hash *hash = newHash(12);
char *words[2];
int wordCount = ArraySize(words);
assert(fileName);
lf = lineFileOpen(fileName, TRUE);
while(lineFileChopNext(lf, words, wordCount))
    {
    hashAdd(hash, words[0], cloneString(words[1]));
    }
lineFileClose(&lf);
outputPValHash = hash;
}

char *isSelectedProbeSet(struct altEvent *event, struct altPath *altPath)
/* Return gene name if this probe set is select for outputing
   probability vector else return NULL. */
{
char *mark;
char buff[256];
struct altPath *path = NULL;
int i = 0;
for(i = 0; i < altPath->probeCount; i++)
    {
    if(altPath->beds[i] != NULL)
	{
	char *mark = strchr(altPath->beds[i]->name, '@');
	if(mark != NULL)
	    {
	    char *geneName = NULL;
	    *mark = '\0';
	    if((geneName = hashFindVal(outputPValHash, altPath->beds[i]->name)) != NULL)
		{
		*mark = '@';
		return geneName;
		}
	    }
	}
    }
return NULL;
}

char * nameForType(int type)
/* Log the different types of splicing. */
{
switch (type)
    {
    case alt5Prime:
	return "alt5Prime";
	break;
    case alt3Prime:
	return "alt3Prime";
	break;
    case altCassette:
	return "altCassette";
	break;
    case altRetInt:
	return "altRetInt";
	break;
    case altOther:
	return "altOther";
	break;
    case altControl:
	return "altControl";
	break;
    case altMutExclusive:
	return "altMutEx";
	break;
    case alt5PrimeSoft:
	return "altTxStart";
	break;
    case alt3PrimeSoft:
	return "altTxEnd";
	break;
    case altIdentity:
	return "altIdentity";
	break;
    default:
	errAbort("nameForType() - Don't recognize type %d", type);
    }
return "error";
}

void outputPathProbabilities(struct altEvent* event, struct altPath *altPath,
			     struct dMatrix *intenM, struct dMatrix *probM,
			     int** expressed, int **notExpressed, double **expression,
			     int pathIx)
/* Print out a thresholded probablilty vector for clustering and visualization
   in treeview. */
{
int i = 0;
FILE *out = pathProbabilitiesOut;
FILE *eOut = pathExpressionOut;
struct splice *splice = event->splice;
struct path *path = altPath->path;
char *geneName = NULL;
assert(out);
if((geneName = isSelectedProbeSet(event, altPath)) != NULL)
    {
    char *psName = "Error";
    assert(altPath->beds[0] != NULL);
    psName = altPath->beds[0]->name;
    fprintf(out, "%s\t%s.%s.%s.%d", psName, geneName, nameForType(splice->type), psName, path->bpCount);
    fprintf(eOut, "%s\t%s.%s.%s.%d", psName, geneName, nameForType(splice->type), psName, path->bpCount);
    for(i = 0; i < probM->colCount; i++)
	{
	fprintf(eOut, "\t%f", expression[pathIx][i]);
	if(expressed[pathIx][i])
	    fprintf(out, "\t2");
	else if(notExpressed[pathIx][i])
	    fprintf(out, "\t-2");
	else
	    fprintf(out, "\t0");
	}
    fprintf(out, "\n");
    fprintf(eOut, "\n");
    }
}

boolean altPathExpressed(struct altEvent *event, struct altPath *altPath,
			 struct dMatrix *intenM, struct dMatrix *probM,
			 int **expressed, int **notExpressed, double **expression,
			 int pathIx)
/* Fill in the expression matrix for this path. */
{
int bestProbeIx = 0;
int tissueIx = 0;
int total = 0;

/* Quick check to see if there are any
      probes at all. */
if(altPath->probeCount == 0)
    return FALSE;

bestProbeIx = determineBestProbe(event, altPath, intenM, probM, pathIx);
/* Loop through the tissues to see if they are expressed. */
if(bestProbeIx == -1)
    return FALSE;

for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    double e = -1;
    if(tissueExpressed(event, altPath, bestProbeIx, tissueIx, &e))
	{
	expressed[pathIx][tissueIx]++;
	total++;
	}
    else if(tissueNotExpressed(event, altPath, bestProbeIx, tissueIx))
	{
	notExpressed[pathIx][tissueIx]++;
	}
    expression[pathIx][tissueIx] = e;
    }
return total > 0;
}

double expressionAverage(struct altPath *altPath, int tissueIx)
/* Return the average expression for all probe sets matched
   to this path in this tissue. */
{
double pathExp = 0.0;
int pCount = 0.0;
int probeIx = 0;
for(probeIx = 0; probeIx < altPath->probeCount; probeIx++)
    {
    if(altPath->expVals[probeIx])
	{
	pathExp += altPath->expVals[probeIx][tissueIx];
	pCount++;
	}
    }
if(pCount == 0)
    return -1;
pathExp = pathExp / pCount;
return pathExp;
}

char *nameForSplice(struct splice *splice, struct altPath *namePath)
/* Return a unique name for this splicing event. Memory is owned by
 this function and will be overwritten each time this function is
 called. */
{
static char buff[256];
safef(buff, sizeof(buff), "%s:%s:%s:%s:%s:%s:%d:%d",
      splice->name, refSeqForPSet(namePath->beds[0]->name), nameForType(splice->type), namePath->beds[0]->name,
      splice->strand, splice->tName, splice->tStart, splice->tEnd);
return buff;
}

double calculateFlipScore(struct altEvent *event, struct altPath *altPath,
			  char *name, struct dMatrix *probM,
			  boolean *isBrain, boolean doAll, int *incBrain, int *skipBrain)

/* Calculate |(#times skip > include) - (#times skip < include)|/totalTissues */
{
struct altPath *skip = NULL, *include = NULL;
int tissueIx = 0;
int brainCount = 0;
int notBrainCount = 0;
int tissueCount = 0;
*incBrain = 0;
*skipBrain = 0;
assert(event);
skip = event->altPathList;
include = event->altPathList->next;
assert(probM);
assert(probM->colCount > 0);

/* Use the sortScore hash if it is setup. */
if(sortScoreHash != NULL)
    {
    double result = sortScoreForSplice(nameForSplice(event->splice, altPath));
    if(result > 0)
	*incBrain = 1;
    else if(result < 0)
	*skipBrain = 1;
    return result;
    }

/* Else do some calculations. */
for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    if(geneExpressed(event, tissueIx) || doAll)
	{
	double skipI = expressionAverage(skip, tissueIx);
	double incI = expressionAverage(include, tissueIx);
	if(incI >= skipI && skipI >= 0 && incI >= 0)
	    {
	    if(isBrain[tissueIx])
		{
		brainCount++;
		(*incBrain)++;
		}
	    else
		notBrainCount++;
	    tissueCount++;
	    }
	else if(skipI >= 0 && incI >= 0)
	    {
	    if(isBrain[tissueIx])
		{
		brainCount--;
		(*skipBrain)++;
		}
	    else
		notBrainCount--;
	    tissueCount++;
	    }
	}
    }
/* If no data, then set score to worst val: 0 */
if(tissueCount == 0)
    return 0;
else
    {
    double brainScore = 0;
    double nonBrainScore = 0;
    brainScore = (double) brainCount / tissueCount;
    nonBrainScore = (double) notBrainCount / tissueCount;
    return fabs(brainScore - nonBrainScore);
    }
return 0;
}



boolean brainSpecific(struct altEvent *event, struct altPath *path, int pathIx,
		      int **expressed, int **notExpressed,
		      int pathCount, struct dMatrix *probM)
/* Output the event if it is alt-expressed and brain
   specific. */
{
int tissueIx = 0;
int brainTissues = 0;
int otherTissues = 0;
int geneBrainTissues = 0;
int geneOtherTissues = 0;
int brainIx = 0;
int incBrain = 0, skipBrain =0;
static int tissueCount = 0;
static char *brainTissuesToChop = NULL;
static char *brainTissuesStrings[256];
static boolean initDone = FALSE;

/* Setup array of what is brain and what isn't. */
if(!initDone)
    {
    brainTissuesToChop = cloneString(optionVal("brainTissues","cerebellum,cerebral_hemisphere,cortex,hind_brain,medial_eminence,olfactory_bulb,pinealgland,thalamus"));
    if(ArraySize(isBrainTissue) < probM->colCount)
	errAbort("Can only handle up to %d columns, %d is too many",
		 ArraySize(isBrainTissue), probM->colCount);
    /* Set everthing to FALSE. */
    for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
	isBrainTissue[tissueIx] = FALSE;

    /* Set brain tissue to TRUE. */
    tissueCount = chopByChar(brainTissuesToChop, ',', brainTissuesStrings, ArraySize(brainTissuesStrings));
    for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
	for(brainIx = 0; brainIx < tissueCount; brainIx++)
	    if(stringIn(brainTissuesStrings[brainIx], probM->colNames[tissueIx]))
		isBrainTissue[tissueIx] = TRUE;

    initDone = TRUE;
    }

/* If we have pvals specified by the user use them instead. */
if(splicePValsHash != NULL)
    {
    double d = 0;
    if (path->probeCount > 0)
	d = pvalForSplice(nameForSplice(event->splice, path));
    else
	return FALSE;

    if(d <= pvalThresh)
	return TRUE;
    else
	return FALSE;
    }

/* Otherwise use the routines here. */
for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    if(expressed[pathIx] != NULL &&
       expressed[pathIx][tissueIx])
	{
	if(isBrainTissue[tissueIx])
	    brainTissues++;
	else
	    otherTissues++;
	}
    else if(brainSpecificStrict &&
	    notExpressed[pathIx] != NULL &&
	    !notExpressed[pathIx][tissueIx])
	{
	if(!isBrainTissue[tissueIx])
	    otherTissues++;
	}
    if(geneExpressed(event, tissueIx))
	{
	if(isBrainTissue[tissueIx])
	    geneBrainTissues++;
	else
	    geneOtherTissues++;
	}
    }

if(otherTissues == 0 &&
   brainTissues >= tissueExpThresh &&
   geneOtherTissues >= tissueExpThresh)
    return TRUE;
return FALSE;
}

boolean tissueSpecific(struct altEvent *event, int pathIx, int targetTissue,
		      int **expressed, int **notExpressed,
		      int pathCount, struct dMatrix *probM)
/* Output the event if it is alt-expressed and brain
   specific. */
{
int tissueIx = 0;
int specificTissues = 0;
int otherTissues = 0;
int geneSpecificTissues = 0;
int geneOtherTissues = 0;

for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    if(expressed[pathIx] != NULL &&
       expressed[pathIx][tissueIx])
	{
	if(tissueIx == targetTissue)
	    specificTissues++;
	else
	    otherTissues++;
	}
    else if(tissueSpecificStrict &&
	    notExpressed[pathIx] != NULL &&
	    !notExpressed[pathIx][tissueIx])
	{
	if(tissueIx != targetTissue)
	    otherTissues++;
	}
    if(geneExpressed(event, tissueIx))
	{
	if(tissueIx == targetTissue)
	    geneSpecificTissues++;
	else
	    geneOtherTissues++;
	}
    }

if(otherTissues == 0 &&
   specificTissues >= tissueExpThresh &&
   geneOtherTissues >= tissueExpThresh)
    return TRUE;
return FALSE;
}

void fillInSequences(struct altEvent *event, struct path *path,
		     struct dnaSeq **upSeq, struct dnaSeq **downSeq, struct dnaSeq **exonSeq,
		     struct bed **upSeqBed, struct bed **downSeqBed)
/* Fill in the sequences from the path. 200 from intron, 5bp from
 exon. */
{
int firstSplice = -1;
int secondSplice = -1;
int i = 0;
struct splice *splice = event->splice;
int *vPos = splice->vPositions;
unsigned char*vTypes = splice->vTypes;
int vC = path->vCount;
int *verts = path->vertices;
int exonOffset = 0;
int intronOffset = 100;
/* Want first exon in order of transcription. */
if(sameString(splice->strand,"+"))
    {
    for(i = 0; i < vC -1; i++)
	if(validVertex(splice, verts[i], verts[i+1]) &&
	   pathEdgeTypeValid(splice, verts[i], verts[i+1]) == ggExon)
	    {
	    firstSplice = vPos[verts[i]];
	    secondSplice = vPos[verts[i+1]];
	    break;
	    }
    }
else
    {
    for(i = vC-1; i > 0; i--)
	if(validVertex(splice, verts[i-1], verts[i]) &&
	   pathEdgeTypeValid(splice, verts[i-1], verts[i]) == ggExon)
	    {
	    firstSplice = vPos[verts[i-1]];
	    secondSplice = vPos[verts[i]];
	    break;
	    }
    }

if(firstSplice < 0 || secondSplice < 0)
    errAbort("Problem with event %s at %s:%d-%d doesn't have an exon",
	     splice->name, splice->tName, splice->tStart, splice->tEnd);

AllocVar(*upSeqBed);
AllocVar(*downSeqBed);
/* Construct the beds. */
(*upSeqBed)->name = cloneString(splice->name);
(*upSeqBed)->chrom = cloneString(splice->tName);
safef((*upSeqBed)->strand, sizeof((*upSeqBed)->strand), "%s", splice->strand);
(*upSeqBed)->chromStart = firstSplice - intronOffset;
(*upSeqBed)->chromEnd = firstSplice + exonOffset;
(*upSeqBed)->score = splice->type;

(*downSeqBed)->name = cloneString(splice->name);
(*downSeqBed)->chrom = cloneString(splice->tName);
safef((*downSeqBed)->strand, sizeof((*downSeqBed)->strand), "%s", splice->strand);
(*downSeqBed)->chromStart = secondSplice - exonOffset;
(*downSeqBed)->chromEnd = secondSplice + intronOffset;
(*downSeqBed)->score = splice->type;

/* Get the sequences. */
*upSeq = hSeqForBed((*upSeqBed));
*downSeq = hSeqForBed((*downSeqBed));
*exonSeq = hChromSeq(splice->tName , firstSplice, secondSplice);
if(sameString(splice->strand, "-"))
    reverseComplement((*exonSeq)->dna, (*exonSeq)->size);

/* If on negative strand swap up and down. */
if(sameString(splice->strand, "-"))
    {
    struct dnaSeq *tmpSeq = NULL;
    struct bed *tmpBed = NULL;
    /* Swap beds. */
    tmpBed = *upSeqBed;
    *upSeqBed = *downSeqBed;
    *downSeqBed = tmpBed;
    /* Swap sequences. */
    tmpSeq = *upSeq;
    *upSeq = *downSeq;
    *downSeq = tmpSeq;
    }
}

void makeJunctMdbGenericLink(struct splice *js, struct dyString *buff, char *name)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.cse.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on&mdbsg.toScale=on\">",
	       "mm2", js->strand[0], js->tName, js->tStart, js->tEnd,
	       js->tStart - offSet, js->tEnd + offSet, "AffyMouseSplice1-02-2004");
dyStringPrintf(buff, "%s", name);
dyStringPrintf(buff, "</a> ");
}

struct bed *phastConForRegion(char *table, char *chrom, int chromStart, int chromEnd)
/* Load the beds for the region specified in the table specified. */
{
struct sqlConnection *conn = hAllocConn2();
struct sqlResult *sr = NULL;
struct bed *conBed = NULL, *conBedList = NULL;
int rowOffset = 0;
char **row = NULL;
sr = hRangeQuery(conn, table, chrom, chromStart, chromEnd,
		 NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    conBed = bedLoad5(row+rowOffset);
    slAddHead(&conBedList, conBed);
    }
sqlFreeResult(&sr);
hFreeConn2(&conn);
slReverse(&conBedList);
return conBedList;
}

struct bed *bed12ForRegion(char *table, char *chrom, int chromStart, int chromEnd, char strand)
/* Load the beds for the region specified in the table specified. */
{
struct sqlConnection *conn = hAllocConn2();
struct sqlResult *sr = NULL;
struct bed *bed = NULL, *bedList = NULL;
int rowOffset = 0;
char **row = NULL;
sr = hRangeQuery(conn, table, chrom, chromStart, chromEnd,
		 NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad12(row+rowOffset);
    if(bed->strand[0] == strand)
	slAddHead(&bedList, bed);
    else
	bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn2(&conn);
slReverse(&bedList);
return bedList;
}

double gcPercentForRegion(char *chrom, int chromStart, int chromEnd, char *db)
/* Return gcPercent for region. */
{
struct dnaSeq *seq = NULL;
int hist[4];
double percent = 0;
if(newDb != NULL && sameString(db, newDb))
    seq = hChromSeq2(chrom, chromStart, chromEnd);
else
    seq = hChromSeq(chrom, chromStart, chromEnd);
dnaBaseHistogram(seq->dna, seq->size, hist);
percent =  (1.0 *hist[G_BASE_VAL]+hist[C_BASE_VAL])/seq->size;
dnaSeqFree(&seq);
return percent;
}

void findExonNumForRegion(char *chrom, int chromStart, int chromEnd, char strand,
			  int *exonNum, int *exonCount)
/* Find out which exon this region overlaps and fill in the exonNum and exonCount. */
{
struct bed *bed = NULL, *bedList = NULL, *goodBed = NULL;
int blockIx = 0;
int *starts = NULL, *sizes = NULL;
int cs = 0, ce = 0;
*exonNum = -1;
*exonCount = -1;
bedList = bed12ForRegion("agxBed", chrom, chromStart, chromEnd, strand);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(bed->strand[0] != strand)
	continue;
    cs = bed->chromStart;
    ce = bed->chromEnd;
    starts = bed->chromStarts;
    sizes = bed->blockSizes;
    for(blockIx = 0; blockIx < bed->blockCount; blockIx++)
	{
	int bStart = cs+starts[blockIx];
	int bEnd = cs+starts[blockIx]+sizes[blockIx];
	if(rangeIntersection(chromStart, chromEnd, bStart, bEnd ) > 0)
	    {
	    if(goodBed == NULL)
		{
		goodBed = bed;
		*exonNum = blockIx;
		*exonCount = bed->blockCount;
  		}
	    break;
	    }
	}
    }
if(strand == '-' && *exonNum != -1)
    {
    *exonNum = *exonCount - *exonNum;
    }
bedFreeList(&bedList);
}

double percentIntronBasesOverlappingConsSide(char *table, char *chrom, int chromStart, int chromEnd, boolean upStream)
/* Percentage of intron bases that overlap a phastCons element. */
{
int intronOffset = 100, overlap = 0;
double percent = 0;
if(newDb != NULL && hTableExists2(table))
    {
    struct bed *upstream = NULL;
    struct bed *bed = NULL;
    if(upStream == TRUE)
	upstream = phastConForRegion(table, chrom, chromStart-intronOffset, chromStart);
    else
	upstream = phastConForRegion(table, chrom, chromEnd, chromEnd + intronOffset);

    for(bed = upstream; bed != NULL; bed = bed->next)
	{
	if(upStream == TRUE)
	    {
	    overlap += positiveRangeIntersection(chromStart-intronOffset, chromStart,
						 bed->chromStart, bed->chromEnd);
	    }
	else
	    {
	    overlap += positiveRangeIntersection(chromEnd, chromEnd+intronOffset,
						 bed->chromStart, bed->chromEnd);
	    }
	}
    percent = (double)overlap / (intronOffset);
    bedFreeList(&upstream);
    }
return percent;
}

double percentIntronBasesOverlappingCons(char *table, char *chrom, int chromStart, int chromEnd)
/* Percentage of intron bases that overlap a phastCons element. */
{
int intronOffset = 100, overlap = 0;
double percent = 0;
if(newDb != NULL && hTableExists2(table))
    {
    struct bed *upstream = phastConForRegion(table, chrom, chromStart-intronOffset, chromStart);
    struct bed *downstream = phastConForRegion(table, chrom, chromEnd, chromEnd + intronOffset);
    struct bed *bed = NULL;
    for(bed = upstream; bed != NULL; bed = bed->next)
	{
	overlap += positiveRangeIntersection(chromStart-intronOffset, chromStart,
					     bed->chromStart, bed->chromEnd);
	}
    for(bed = downstream; bed != NULL; bed = bed->next)
	{
	overlap += positiveRangeIntersection(chromEnd, chromEnd+intronOffset,
					     bed->chromStart, bed->chromEnd);
	}
    percent = (double)overlap / (2 * intronOffset);
    bedFreeList(&upstream);
    bedFreeList(&downstream);
    }
return percent;
}

void printLinks(struct altEvent *event)
/* Loop through and print each splicing event to web page. */
{
struct splice *s = NULL;
struct path *lastPath = NULL;
struct splice *splice = event->splice;
struct altPath *altPath = NULL, *namePath = NULL;
char *brainDiffDir = optionVal("plotDir","brainDiffPlots.9");
char *fileSuffix = optionVal("fileSuffix", "png");
char *chrom = NULL;
char strand;
int chromStart = 0,  chromEnd = 0;
int diff = -1;
char *useDb = db;
char *newDb = optionVal("newDb", NULL);
boolean overlapsPhasCons = FALSE;
double overlapPercent = 0;
struct dyString *buff = NULL;
char *skipPSet = NULL;
int i = 0;
boolean muscleOn = optionExists("muscleOn");
/* struct altPath *incPath = NULL, *skipPath = NULL; */
if(splice->paths == NULL || splice->type == altControl)
    return;

for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
    {
    struct bed *bed = NULL;
    double score = event->flipScore;
    char upPos[256];
    char downPos[256];

    /* If we're dealing with a cassette or other limit to 2 paths
       use skip/include path model. */
    if(onlyTwoPaths(event))
	{
	namePath = event->altPathList;
	altPath = event->altPathList->next;
	diff = abs(splice->paths->bpCount - splice->paths->next->bpCount);
	}
    else
	{
	diff = altPath->path->bpCount;
	namePath = altPath;
	}

    /* Check to make sure that we have probes and that this
       path is brain specific. */
    if(namePath->probeCount < 1 || event->geneProbeCount < 1)
	continue;
    if(splicePValsHash != NULL)
	{
	double d = 0;
	d = pvalForSplice(nameForSplice(event->splice,namePath));
	if(d > pvalThresh)
	    continue;
	event->pVal = d;
	}
    if(sortScoreHash != NULL)
	score = sortScoreForSplice(nameForSplice(event->splice, namePath));
    bed = pathToBed(altPath->path, event->splice, -1, -1, FALSE);
    chrom = cloneString(bed->chrom);
    chromStart = bed->chromStart;
    chromEnd = bed->chromEnd;
    strand = bed->strand[0];

    if(newDb != NULL)
	{
	if(liftOverCoords(&chrom, &chromStart, &chromEnd, &strand))
	    useDb = newDb;
	}
/* overlapPercent = percentIntronBasesOverlappingCons(chrom, chromStart,chromEnd); */
    buff = newDyString(256);

/*     if(brainSpPSetOut != NULL)  */
/* 	{ */
/* 	skipPath = namePath; */
/* 	skipPSet = skipPath->beds[0]->name; */
/* 	if(onlyTwoPaths(event)) */
/* 	    incPath = event->altPathList->next; */
/* 	else */
/* 	    incPath = NULL; */
/* 	fprintf(brainSpPSetOut, "%s\t", refSeqForPSet(skipPSet)); */
/* 	fprintf(brainSpPSetOut, "%s\t", skipPSet); */
/* 	if(incPath != NULL) */
/* 	    { */
/* 	    fprintf(brainSpPSetOut, "%d\t", incPath->probeCount); */
/* 	    for(i = 0; i < incPath->probeCount; i++)  */
/* 		{ */
/* 		fprintf(brainSpPSetOut, "%s,", incPath->beds[i]->name); */
/* 		} */
/* 	    } */
/* 	else */
/* 	    { */
/* 	    fprintf(brainSpPSetOut, "%d\t", event->geneProbeCount); */
/* 	    for(i = 0; i < event->geneProbeCount; i++)  */
/* 		{ */
/* 		fprintf(brainSpPSetOut, "%s,", event->geneBeds[i]->name); */
/* 		} */
/* 	    } */
/* 	fprintf(brainSpPSetOut, "\t%d\t", event->geneProbeCount); */
/* 	for(i = 0; i < event->geneProbeCount; i++)  */
/* 	    { */
/* 	    fprintf(brainSpPSetOut, "%s,", event->geneBeds[i]->name); */
/* 	    } */
/* 	fprintf(brainSpPSetOut, "\n"); */
/* 	} */

    fprintf(brainSpTableHtmlOut, "<tr><td>");
    if(overlapsPhasCons)
	fprintf(brainSpTableHtmlOut, "<span style='color:red;'> * </span>");
    fprintf(brainSpTableHtmlOut, "<a target=\"browser\" "
	    "href=\"http://%s/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&hgt.motifs=TCATT%%2CTCATC%%2CGGGGG%%2CCTCTCT%%2CGCATG%%2CTCCTT\",>", browserName,
	    useDb, chrom, chromStart-100, chromEnd+100);
    fprintf(brainSpTableHtmlOut,"%s </a><font size=-1>%s</font>\n",
	    refSeqForPSet(event->geneBeds[0]->name), useDb);
    safef(upPos, sizeof(upPos), "%s:%d-%d", chrom, chromStart-95, chromStart+5);
    safef(downPos, sizeof(downPos), "%s:%d-%d", chrom, chromEnd-5, chromEnd+95);
    fprintf(brainSpTableHtmlOut, "<a target=\"browser\" "
	    "href=\"http://%s/cgi-bin/hgTracks?db=%s&position=%s&complement=%d&hgt.motifs=TCATT%%2CTCATC%%2CGGGGG%%2CCTCTCT%%2CGCATG%%2CTCCTT\">[u]</a>",
	    browserName, useDb, strand == '+' ? upPos : downPos,
	    strand == '-' ? 1 : 0);
    fprintf(brainSpTableHtmlOut, "<a target=\"browser\" "
	    "href=\"http://%s/cgi-bin/hgTracks?db=%s&position=%s&complement=%d&hgt.motifs=TCATT%%2CTCATC%%2CGGGGG%%2CCTCTCT%%2CGCATG%%2CTCCTT\">[d]</a>",
	    browserName, useDb, strand == '+' ? downPos : upPos,
	    strand == '-' ? 1 : 0);
    fprintf(brainSpTableHtmlOut,
	    "<a target=\"plots\" href=\"http://%s/cgi-bin/spliceProbeVis?skipPName=%s%s\">[p]</a>",
	    browserName, namePath->beds[0]->name, muscleOn ? "&muscle=on" : "");
    fprintf(brainSpTableHtmlOut,
	    "<a target=\"plots\" href=\"http://%s/cgi-bin/spliceProbeVis?skipPName=%s&pdf=on%s\">[pdf]</a>",
	    browserName, namePath->beds[0]->name, muscleOn ? "&muscle=on" : "");
    fprintf(brainSpTableHtmlOut, "<a target=\"plots\" href=\"./%s/%s:%s:%s:%s:%s:%s:%d:%d.%s\">[f]</a>",
	    brainDiffDir,
	    splice->name, refSeqForPSet(namePath->beds[0]->name),
	    nameForType(splice->type), namePath->beds[0]->name,
	    splice->strand, splice->tName, splice->tStart, splice->tEnd, fileSuffix);
/* makeJunctMdbGenericLink(splice, buff, "[p]"); */
/* fprintf(brainSpTableHtmlOut, "%s", buff->string); */
    fprintf(brainSpTableHtmlOut, "<font size=-1>(%5.2f, %s, %d, %d)</font>", event->percentUltra, nameForType(splice->type), diff, diff % 3);
    fprintf(brainSpTableHtmlOut, "<font size=-2>");
    if(altPath->motifUpCounts != NULL)
	{
	fprintf(brainSpTableHtmlOut, "[");
	for(i = 0; i < bindSiteCount; i++)
	    fprintf(brainSpTableHtmlOut, " %d", altPath->motifUpCounts[i]);
	fprintf(brainSpTableHtmlOut, "] ");

	fprintf(brainSpTableHtmlOut, "[");
	for(i = 0; i < bindSiteCount; i++)
	    fprintf(brainSpTableHtmlOut, " %d", altPath->motifInsideCounts[i]);
	fprintf(brainSpTableHtmlOut, "] ");

	fprintf(brainSpTableHtmlOut, "[");
	for(i = 0; i < bindSiteCount; i++)
	    fprintf(brainSpTableHtmlOut, " %d", altPath->motifDownCounts[i]);
	fprintf(brainSpTableHtmlOut, "] ");
	}
    fprintf(brainSpTableHtmlOut, "</font>\n");
    fprintf(brainSpTableHtmlOut, " </td>");
    fprintf(brainSpTableHtmlOut,"<td>%.4f</td></tr>\n", score);
    dyStringFree(&buff);
    }
}

double consForIntCoords(char *chrom, int intStart, int intEnd, boolean doLift)
/* Return the conservation for the intron specified by intStart, intEnd. */
{
char strand = '+';
char *startChrom = cloneString(chrom);
char *endChrom = cloneString(chrom);
double cons = 0;
int overlap = 0;
int chromStart = intStart - 10, chromEnd = intEnd + 10;
struct bed *bedList = NULL, *bed = NULL;
if(doLift && newDb != NULL)
    {
    /* Lift over each end seperately so as to ignore changes
       in assembly in the middle. */
    liftOverCoords(&startChrom, &chromStart, &intStart, &strand);
    liftOverCoords(&endChrom, &intEnd, &chromEnd, &strand);
    }
if(intStart < intEnd && newDb != NULL)
    {
    bedList = phastConForRegion("phastConsOrig90", startChrom, intStart, intEnd);
    for(bed = bedList; bed != NULL; bed = bed->next)
	{
	overlap += positiveRangeIntersection(intStart, intEnd,
					     bed->chromStart, bed->chromEnd);
	}
    cons = (double)overlap / (intEnd - intStart);
    bedFreeList(&bedList);
    }
else
    cons = 0;
return cons;
}

void fillInStatsForEvent(struct valuesMat *vm, char *name, char *chrom, int intStart,
			 int exonStart, int exonEnd, int intEnd, char strand, enum altSpliceType type)
/* Fill in stats for the exon specified by intStart->exonStart->exonEnd->intEnd. */
{
char *skipPSet = NULL;
char buff[256];
double percentPhastCons = 0;
double upIntCons = 0;
double downIntCons = 0;
double ultraCons = 0;
int exonCount = -1, exonNum = -1;
double cons = 0;

/* Add event to our list for later sorting, etc. */
ultraCons = cons = percentIntronBasesOverlappingConsSide("phastConsOrig90", chrom, exonStart,exonEnd, TRUE);

/* Percent ultra. */
safef(buff, sizeof(buff), "%.4f", cons);
vmAddVal(vm, name, "intronUltraConsUpStream", buff);

cons = percentIntronBasesOverlappingConsSide("phastConsOrig90", chrom, exonStart,exonEnd, FALSE);
ultraCons += cons;

/* Percent ultra. */
safef(buff, sizeof(buff), "%.4f", cons);
vmAddVal(vm, name, "intronUltraConsDownStream", buff);

ultraCons = ultraCons / 2;

/* Percent ultra. */
safef(buff, sizeof(buff), "%.4f", ultraCons);
vmAddVal(vm, name, "intronUltraCons", buff);

/* Percent conserved. */
percentPhastCons = percentIntronBasesOverlappingCons("phastConsElements", chrom, exonStart, exonEnd);
safef(buff, sizeof(buff), "%.4f", percentPhastCons);
vmAddVal(vm, name, "intronCons", buff);

/* Name of splicing event. */
vmAddVal(vm, name, "type", nameForType(type));

/* Size in bp of splicing event. */
safef(buff, sizeof(buff), "%d", exonEnd - exonStart);
vmAddVal(vm, name, "bpDiff", buff);

/* Intron size. */
safef(buff, sizeof(buff), "%d", intEnd - intStart);
vmAddVal(vm, name, "intronSize", buff);

/* Genome Position. */
safef(buff, sizeof(buff), "%s:%d-%d", chrom, intStart, intEnd);
vmAddVal(vm, name, "mm2.genomePos", buff);

/* Genome Position. */
safef(buff, sizeof(buff), "%s:%d-%d", chrom, exonStart, exonEnd);
vmAddVal(vm, name, "mm5.exonPos", buff);

/* Strand. */
safef(buff, sizeof(buff), "%c", strand);
vmAddVal(vm, name, "strand", buff);

/* GC Percent. */
safef(buff, sizeof(buff), "%.4f", gcPercentForRegion(chrom, exonStart, exonEnd, newDb));
vmAddVal(vm, name, "gcPercent", buff);

/* Upstream intron conservation. */
upIntCons = consForIntCoords(chrom, intStart, exonStart, FALSE );
downIntCons = consForIntCoords(chrom, exonEnd, intEnd, FALSE);
if(strand == '-')
    {
    double dTmp = upIntCons;
    upIntCons = downIntCons;
    downIntCons = dTmp;
    }
safef(buff, sizeof(buff), "%g", upIntCons);
vmAddVal(vm, name, "upFullIntronCons", buff);

safef(buff, sizeof(buff), "%g", downIntCons);
vmAddVal(vm, name, "downFullIntronCons", buff);

/* Add some values about expresion. */
findExonNumForRegion(chrom, exonStart, exonEnd, strand, &exonNum, &exonCount);

safef(buff, sizeof(buff), "%d", exonNum);
vmAddVal(vm, name, "exonNum", buff);

safef(buff, sizeof(buff), "%d", exonCount);
vmAddVal(vm, name, "exonCount", buff);
}

void writeOutOverlappingUltra(FILE *out, FILE *bedOut, char *chrom,
			      int chromStart, int chromEnd, char strand)
/* Write out the sequences for phastCons elements that overlap the region. */
{
struct bed *bedList = NULL, *bed = NULL;
struct dnaSeq *seq = NULL;
char buff[256];
bedList = phastConForRegion("phastConsOrig90", chrom, chromStart, chromEnd);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    seq = hChromSeq2(bed->chrom, bed->chromStart, bed->chromEnd);
    if(strand == '-')
	reverseComplement(seq->dna, seq->size);
    safef(buff, sizeof(buff), "%s.%s:%d-%d", bed->name, chrom, chromStart, chromEnd);
    safef(bed->strand, sizeof(bed->strand), "%c", strand);
    faWriteNext(out, buff, seq->dna, seq->size);
    dnaSeqFree(&seq);
    bedTabOutN(bed, 6, bedOut);
    }
bedFreeList(&bedList);
}


void outputBrainSpecificEvents(struct altEvent *event, int **expressed, int **notExpressed,
			       int pathCount, struct dMatrix *probM)
/* Output the event if it is alt-expressed and brain
   specific. */
{
struct dnaSeq *upSeq = NULL;
struct dnaSeq *downSeq = NULL;
struct dnaSeq *exonSeq = NULL;
struct bed *pathBed = NULL;
struct bed *upSeqBed = NULL;
struct bed *downSeqBed = NULL;

struct slRef *brainEvent = NULL;
boolean lifted = FALSE;
int i = 0;
int pathIx = 0;
struct altPath *altPath = NULL;
boolean brainSpecificEvent = FALSE;
struct splice *splice = event->splice;
boolean eventAdded = FALSE;
for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next, pathIx++)
    {
    char *chrom = NULL;
    int chromStart = 0;
    int chromEnd = 0;
    int intronEnd = 0;
    int intronStart = 0;
    int intronDummy = 0;
    char strand;
    char *skipPSet = NULL;
    struct altPath *skipPath = NULL;
    struct altPath *incPath = NULL;
    struct altPath *namePath = NULL;
    char buff[256];
    double percentPhastCons = 0;
    double upIntCons = 0;
    double downIntCons = 0;
    double cons = 0, ultraCons = 0;
    double flipScore = 0;
    double maxProb = 0;
    int incBrain = 0, skipBrain =0;
    int exonCount = -1, exonNum = -1;
    if(onlyTwoPaths(event) && altPath == event->altPathList->next)
	namePath = event->altPathList;
    else
	namePath = altPath;
    if(!brainSpecific(event, namePath, pathIx, expressed, notExpressed, pathCount, probM) ||
       splice->type == altOther)
	{
	continue;
	}

    /* check to make sure this path includes some sequence. */
    pathBed = pathToBed(altPath->path, event->splice, -1, -1, FALSE);
    if(pathBed == NULL && splice->type != alt5PrimeSoft && splice->type != alt3PrimeSoft)
	continue;

    if(!eventAdded)
	{
	/* Add event to our list for later sorting, etc. */
	AllocVar(brainEvent);
	brainEvent->val = event;
	slAddHead(&brainSpEvents, brainEvent);
	eventAdded = TRUE;
	}


    if(sortScoreHash != NULL && namePath->probeCount > 0)
	event->flipScore = sortScoreForSplice(nameForSplice(event->splice, namePath));

    /* Rest of the logging functions are only for cassettes with probes. */
    if(splice->type != altCassette || event->altPathProbeCount < 2)
	{
	continue;
	}



    /* Print out the cassette connected to the proximal exons. */
    if(brainSpPathEndsBedOut)
	{
	struct bed *longBed = pathToBed(altPath->path, event->splice, -1, -1, TRUE);
	assert(longBed);
	bedTabOutN(longBed, 12, brainSpPathEndsBedOut);
	bedFree(&longBed);
	}

    /* Lets do some stat recording. */
    skipPath = event->altPathList;
    skipPSet = skipPath->beds[0]->name;
    incPath = event->altPathList->next;

/*     if(brainSpPSetOut != NULL)  */
/* 	{ */
/* 	fprintf(brainSpPSetOut, "%s\t", refSeqForPSet(skipPSet)); */
/* 	fprintf(brainSpPSetOut, "%s\t", skipPSet); */
/* 	fprintf(brainSpPSetOut, "%d\t", incPath->probeCount); */
/* 	for(i = 0; i < incPath->probeCount; i++)  */
/* 	    { */
/* 	    fprintf(brainSpPSetOut, "%s,", incPath->beds[i]->name); */
/* 	    } */
/* 	fprintf(brainSpPSetOut, "\t%d\t", event->geneProbeCount); */
/* 	for(i = 0; i < event->geneProbeCount; i++)  */
/* 	    { */
/* 	    fprintf(brainSpPSetOut, "%s,", event->geneBeds[i]->name); */
/* 	    } */
/* 	fprintf(brainSpPSetOut, "\n"); */
/* 	} */

    chrom = cloneString(pathBed->chrom);
    chromStart = pathBed->chromStart;
    chromEnd = pathBed->chromEnd;
    intronStart = splice->tStart;
    intronEnd = splice->tEnd;
    strand = pathBed->strand[0];

    if(newDb != NULL)
	{
	lifted = liftOverCoords(&chrom, &chromStart, &chromEnd, &strand);
	}
    /* Add event to our list for later sorting, etc. */
    ultraCons = cons = percentIntronBasesOverlappingConsSide("phastConsOrig90", chrom, chromStart,chromEnd, TRUE);

    /* Percent ultra. */
    safef(buff, sizeof(buff), "%.4f", cons);
    vmAddVal(brainSpecificValues, skipPSet, "intronUltraConsUpStream", buff);

    cons = percentIntronBasesOverlappingConsSide("phastConsOrig90", chrom, chromStart,chromEnd, FALSE);
    ultraCons += cons;

    /* Percent ultra. */
    safef(buff, sizeof(buff), "%.4f", cons);
    vmAddVal(brainSpecificValues, skipPSet, "intronUltraConsDownStream", buff);

    ultraCons = ultraCons / 2;
    event->percentUltra = ultraCons;
/*     event->percentUltra = percentIntronBasesOverlappingCons("phastConsOrig90", chrom, chromStart,chromEnd); */
    if(event->percentUltra >= .20)
	intronsConserved++;
    intronsConsCounted++;


    /* Percent ultra. */
    safef(buff, sizeof(buff), "%.4f", event->percentUltra);
    vmAddVal(brainSpecificValues, skipPSet, "intronUltraCons", buff);

    /* If we have phastCons data we want it to end up being printed. */
    if(phastConsHash != NULL)
	event->percentUltra = phastConsForSplice(skipPSet);

    /* Percent conserved. */
    percentPhastCons = percentIntronBasesOverlappingCons("phastConsElements", chrom, chromStart,chromEnd);
    safef(buff, sizeof(buff), "%.4f", percentPhastCons);
    vmAddVal(brainSpecificValues, skipPSet, "intronCons", buff);

    /* Name of splicing event. */
    vmAddVal(brainSpecificValues, skipPSet, "type", nameForType(splice->type));

    /* Size in bp of splicing event. */
    safef(buff, sizeof(buff), "%d", incPath->path->bpCount - skipPath->path->bpCount);
    vmAddVal(brainSpecificValues, skipPSet, "bpDiff", buff);

    /* Modulous 3. */
    safef(buff, sizeof(buff), "%d", (incPath->path->bpCount - skipPath->path->bpCount) % 3);
    vmAddVal(brainSpecificValues, skipPSet, "mod3", buff);

    /* Intron size. */
    safef(buff, sizeof(buff), "%d", splice->tEnd - splice->tStart);
    vmAddVal(brainSpecificValues, skipPSet, "intronSize", buff);

    /* Gene Name. */
    if(psToRefGeneHash != NULL)
	vmAddVal(brainSpecificValues, skipPSet, "geneName", refSeqForPSet(skipPSet));

    /* Known Gene. */
    if(psToKnownGeneHash != NULL)
	vmAddVal(brainSpecificValues, skipPSet, "knownGene", knownGeneForPSet(skipPSet));


    /* Chrom. */
    safef(buff, sizeof(buff), "%s", splice->tName);
    vmAddVal(brainSpecificValues, skipPSet, "mm2.chrom", buff);

    /* ChromStart */
    safef(buff, sizeof(buff), "%d",pathBed->chromStart);
    vmAddVal(brainSpecificValues, skipPSet, "mm2.exonChromStart", buff);

    /* ChromEnd */
    safef(buff, sizeof(buff), "%d",pathBed->chromEnd);
    vmAddVal(brainSpecificValues, skipPSet, "mm2.exonChromEnd", buff);

    /* Strand. */
    vmAddVal(brainSpecificValues, skipPSet, "strand", splice->strand);


    /* Genome Position. */
    safef(buff, sizeof(buff), "%s:%d-%d", splice->tName, splice->tStart, splice->tEnd);
   vmAddVal(brainSpecificValues, skipPSet, "mm2.genomePos", buff);


    /* Genome Position. */
    safef(buff, sizeof(buff), "%s:%d-%d", chrom, chromStart, chromEnd);
    vmAddVal(brainSpecificValues, skipPSet, "mm5.exonPos", buff);

    /* Pvalue. */
    if(splicePValsHash != NULL)
	{
	safef(buff, sizeof(buff), "%g", pvalForSplice(nameForSplice(event->splice, skipPath)));
	vmAddVal(brainSpecificValues, skipPSet, "bsPVal", buff);
	event->pVal = atof(buff);
	}

    /* GC Percent. */
    safef(buff, sizeof(buff), "%.4f", gcPercentForRegion(pathBed->chrom, pathBed->chromStart, pathBed->chromEnd, db));
    vmAddVal(brainSpecificValues, skipPSet, "gcPercent", buff);

    /* Upstream intron conservation. */
    upIntCons = consForIntCoords(splice->tName, splice->tStart, pathBed->chromStart, TRUE);
    downIntCons = consForIntCoords(splice->tName, pathBed->chromEnd, splice->tEnd, TRUE);
    if(pathBed->strand[0] == '-')
	{
	double dTmp = upIntCons;
	upIntCons = downIntCons;
	downIntCons = dTmp;
	}
    safef(buff, sizeof(buff), "%g", upIntCons);
    vmAddVal(brainSpecificValues, skipPSet, "upFullIntronCons", buff);

    safef(buff, sizeof(buff), "%g", downIntCons);
    vmAddVal(brainSpecificValues, skipPSet, "downFullIntronCons", buff);

    /* Calculate a flip score using all tissues even if they
       aren't expressed. */
    event->flipScore = flipScore = calculateFlipScore(event, skipPath, skipPSet, probM, isBrainTissue, TRUE, &incBrain, &skipBrain);
    safef(buff, sizeof(buff), "%.4f", flipScore);
    vmAddVal(brainSpecificValues, skipPSet, "flipScoreAllTissues", buff);

    /* flip. */
    safef(buff, sizeof(buff), "%.4f", event->flipScore);
    vmAddVal(brainSpecificValues, skipPSet, "flipScore", buff);

    /* Calculate times brain includes and brain excludes. */
    flipScore = calculateFlipScore(event, skipPath, skipPSet, probM, isBrainTissue, FALSE, &incBrain, &skipBrain);
    safef(buff, sizeof(buff), "%d", incBrain);
    vmAddVal(brainSpecificValues, skipPSet, "incBrain", buff);
    safef(buff, sizeof(buff), "%d", skipBrain);
    vmAddVal(brainSpecificValues, skipPSet, "skipBrain", buff);

/* Conserved in human transcriptome? */
    safef(buff, sizeof(buff), "%s:%d-%d:%c", chrom,
	  chromStart, chromEnd, strand);

    if(!lifted || conservedHash == NULL)
	vmAddVal(brainSpecificValues, skipPSet, "humanAltToo", "UNKNOWN");
    else if(hashFindVal(conservedHash, buff) != NULL)
	vmAddVal(brainSpecificValues, skipPSet, "humanAltToo", "TRUE");
    else
	vmAddVal(brainSpecificValues, skipPSet, "humanAltToo", "FALSE");


    /* Add some values about expresion. */
    if(lifted)
	findExonNumForRegion(chrom, chromStart, chromEnd, strand, &exonNum, &exonCount);

    safef(buff, sizeof(buff), "%d", exonNum);
    vmAddVal(brainSpecificValues, skipPSet, "exonNum", buff);

    safef(buff, sizeof(buff), "%d", exonCount);
    vmAddVal(brainSpecificValues, skipPSet, "exonCount", buff);

    if(event->altPathProbeCount >= 2)
	vmAddVal(brainSpecificValues, skipPSet, "wProbes", "TRUE");
    else
	vmAddVal(brainSpecificValues, skipPSet, "wProbes", "FALSE");

    if(event->isExpressed)
	vmAddVal(brainSpecificValues, skipPSet, "expressed", "TRUE");
    else
	vmAddVal(brainSpecificValues, skipPSet, "expressed", "FALSE");

    if(event->isAltExpressed)
	vmAddVal(brainSpecificValues, skipPSet, "altExpressed", "TRUE");
    else
	vmAddVal(brainSpecificValues, skipPSet, "altExpressed", "FALSE");

    maxProb = max(skipPath->score, incPath->score);
    safef(buff, sizeof(buff), "%.10f", maxProb);
    vmAddVal(brainSpecificValues, skipPSet, "pExpressed", buff);

    /* Splice event name. */
    safef(buff, sizeof(buff), "%s", event->splice->name);
    vmAddVal(brainSpecificValues, skipPSet, "spliceName", buff);

    /* Count up some motifs. */
    if(splice->type == altCassette && altPath->path->bpCount > 0)
	{
	if(event->flipScore >= 0)
	    cassetteSkipInsideBpCount += incPath->path->bpCount - skipPath->path->bpCount;
	else
	    brainSpecificMotifInsideBpCount += incPath->path->bpCount - skipPath->path->bpCount;

	for(i = 0; i < bindSiteCount; i++)
	    {
	    int total = altPath->motifUpCounts[i] + altPath->motifDownCounts[i] + altPath->motifInsideCounts[i];
	    char colName[256];
	    /* Add the total up. */
	    safef(buff, sizeof(buff), "%d", total);
	    vmAddVal(brainSpecificValues, skipPSet, bindSiteArray[i]->rnaBinder, buff);

	    /* Add the upstream counts */
	    safef(colName, sizeof(colName), "%s-up", bindSiteArray[i]->rnaBinder);
	    safef(buff, sizeof(buff), "%d", altPath->motifUpCounts[i]);
	    vmAddVal(brainSpecificValues, skipPSet, colName, buff);

	    /* Add the downstream counts. */
	    safef(colName, sizeof(colName), "%s-down", bindSiteArray[i]->rnaBinder);
	    safef(buff, sizeof(colName), "%d", altPath->motifDownCounts[i]);
	    vmAddVal(brainSpecificValues, skipPSet, colName, buff);

	    if(event->flipScore >=0)
		{
		cassetteSkipUpMotifs[i] += event->altPathList->next->motifUpCounts[i];
		cassetteSkipDownMotifs[i] += event->altPathList->next->motifDownCounts[i];
		cassetteSkipInsideMotifs[i] += event->altPathList->next->motifInsideCounts[i];

		}
	    else
		{
		brainSpecificMotifUpCounts[i] += altPath->motifUpCounts[i];
		brainSpecificMotifDownCounts[i] += altPath->motifDownCounts[i];
		brainSpecificMotifInsideCounts[i] += altPath->motifInsideCounts[i];
		}

	    }
	if(event->flipScore >=0)
	    cassetteExonSkipCount++;
	}
    brainSpecificPathCount++;
    brainSpecificEvent = TRUE;
    fillInSequences(event, slLastEl(event->splice->paths),
		    &upSeq, &downSeq, &exonSeq, &upSeqBed, &downSeqBed);


    /* Write out the data. */
    if(event->flipScore >= optionFloat("minFlip", -200000) &&
       event->flipScore <= optionFloat("maxFlip", 200000) &&
       event->percentUltra >= optionFloat("minIntCons", 0.0))
	{
	struct bed *endsBed = NULL;
	struct bed *upExonBed = NULL, *downExonBed = NULL;
	struct dyString *dna = newDyString(2*upSeq->size+5);
	char *tmp = NULL;
	dyStringPrintf(dna, "%sNNN%s", upSeq->dna, downSeq->dna);
	if(lifted)
	    {
	    writeOutOverlappingUltra(brainSpConsDnaOut, brainSpConsBedOut,
				     chrom, chromStart, chromEnd, strand);
	    }
	faWriteNext(brainSpDnaUpOut, upSeq->name, upSeq->dna, upSeq->size);
	bedTabOutN(upSeqBed, 6, brainSpBedUpOut);
	vmAddVal(brainSpecificValues, skipPSet, "upSeq", upSeq->dna);

	faWriteNext(brainSpDnaDownOut, downSeq->name, downSeq->dna, downSeq->size);
	bedTabOutN(downSeqBed, 6, brainSpBedDownOut);
	vmAddVal(brainSpecificValues, skipPSet, "downSeq", downSeq->dna);

	vmAddVal(brainSpecificValues, skipPSet, "exonSeq", exonSeq->dna);
	faWriteNext(brainSpDnaMergedOut, upSeqBed->name, dna->string, dna->stringSize);
	pathBed->score = 100*event->flipScore;
	tmp = pathBed->name;
	pathBed->name = skipPSet;
	bedTabOutN(pathBed, 6, brainSpPathBedOut);
	pathBed->name = tmp;

	/* Find the ends of the intron and make beds for the
	   portions of the upstream and downstream exons that connect
	   to this exon bed. */
	endsBed = pathToBed(altPath->path, event->splice, -1, -1, TRUE);
	assert(endsBed);
	AllocVar(upExonBed);
	AllocVar(downExonBed);
	upExonBed->strand[0] = downExonBed->strand[0] = endsBed->strand[0];
	upExonBed->score = downExonBed->score = pathBed->score;
	upExonBed->chrom = cloneString(endsBed->chrom);
	downExonBed->chrom = cloneString(endsBed->chrom);
	upExonBed->name = cloneString(endsBed->name);
	downExonBed->name = cloneString(endsBed->name);

	downExonBed->chromStart = endsBed->chromEnd;
	downExonBed->chromEnd = downExonBed->chromStart + 10;
	upExonBed->chromEnd = endsBed->chromStart;
	upExonBed->chromStart = upExonBed->chromEnd - 10;
	if(endsBed->strand[0] == '-')
	    {
	    struct bed *tmp = upExonBed;
	    upExonBed = downExonBed;
	    downExonBed = tmp;
	    }
	bedTabOutN(upExonBed, 6, brainSpPathBedExonUpOut);
	bedTabOutN(downExonBed, 6, brainSpPathBedExonDownOut);
	bedFree(&upExonBed);
	bedFree(&downExonBed);
	bedFree(&endsBed);
	}

    bedFree(&upSeqBed);
    bedFree(&downSeqBed);
    dnaSeqFree(&upSeq);
    dnaSeqFree(&downSeq);
    dnaSeqFree(&exonSeq);
    }

if(brainSpecificEvent == TRUE)
    {
    brainSpecificEventCount++;

    switch(splice->type)
	{
	case altCassette :
	    brainSpecificCounts[altCassette]++;
	    break;
	case altMutExclusive :
	    brainSpecificCounts[altMutExclusive]++;
	    break;
	case alt5Prime :
	    brainSpecificCounts[alt5Prime]++;
	    break;
	case alt3Prime :
	    brainSpecificCounts[alt3Prime]++;
	    break;
	case alt3PrimeSoft :
	    brainSpecificCounts[alt3PrimeSoft]++;
	    break;
	case altOther :
	    brainSpecificCounts[altOther]++;
	    break;
	}
    }
}

void incrementCountByType(int *array, int type)
{
switch(type)
    {
    case altCassette :
	array[altCassette]++;
	break;
    case altMutExclusive :
	array[altMutExclusive]++;
	break;
    case alt5Prime :
	array[alt5Prime]++;
	break;
    case alt3Prime :
	array[alt3Prime]++;
	break;
    case alt3PrimeSoft :
	array[alt3PrimeSoft]++;
	break;
    case altOther :
	array[altOther]++;
	break;
    }
}

void outputTissueSpecificEvents(struct altEvent *event, int **expressed, int **notExpressed,
			       int pathCount, struct dMatrix *probM)
/* Output the event if it is tissue specific
   specific. */
{
int tissueIx = 0;
struct altPath *altPath = NULL;
struct splice *splice = event->splice;
for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    boolean tissueSpecificEvent = FALSE;
    int pathIx = 0;
    for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next, pathIx++)
	{
	if(altPath->path->bpCount > 0 &&
	   tissueSpecific(event, pathIx, tissueIx, expressed, notExpressed, pathCount, probM))
	    {
	    incrementCountByType(tissueSpecificCounts[tissueIx], splice->type);
	    break;
	    }
	}
    }
}


boolean onlyTwoPaths(struct altEvent *event)
/* Return TRUE if event is altCassette, alt3Prime, alt5Prime,
 * altMutEx. FALSE otherwise. */
{
int type = event->splice->type;
if(type == altCassette || type == alt3Prime || type == alt5Prime || type == altMutExclusive)
    return TRUE;
return FALSE;
}



double expressionRatio(struct altEvent *event, struct altPath *skipPath,
		       struct altPath *incPath, int tissueIx)
/* Calculate the expression ratio between the two paths.*/
{
double skipExp = 0.0, incExp = 0.0;
int probeIx = 0;
double ratio = 0.0;
/* Calculate average expression for inc and skip paths. */
skipExp = expressionAverage(skipPath, tissueIx);
incExp =  expressionAverage(incPath, tissueIx);
if(skipExp == -1 || incExp == -1)
    return -1;

/* Caclulate the ratio. */
if(incExp == 0)
    incExp = .00001;
ratio = skipExp / incExp;
return ratio;
}

struct altPath *altPathStubForGeneSet(struct altEvent *event, int geneIx)
/* Create a "fake" path from data for gene probe set. Return NULL if no
 probeSets for a given event. */
{
struct altPath *ap = NULL;
if(event->geneProbeCount < 1)
    return NULL;
AllocVar(ap);
ap->probeCount = 1;
ap->beds = &event->geneBeds[geneIx];
ap->expVals = &event->geneExpVals[geneIx];
ap->pVals = &event->genePVals[geneIx];
ap->avgExpVals = event->geneExpVals[geneIx];
return ap;
}



void outputRatioStatsForPaths(struct altEvent *event, struct altPath *incPath,
			      struct altPath *skipPath, struct altPath *namePath,
			      struct dMatrix *probM, struct dMatrix *intenM)
/* Write the stats for a pair of particular paths. */
{
struct splice *splice = event->splice;
int tissueIx = 0;
char *eventName = nameForSplice(splice, namePath);
int i = 0;
char *skipPSet = NULL;
assert(ratioStatOut != NULL);
assert(intenM->colCount = probM->colCount);

/* If there is no data don't output anything. */
if(skipPath->probeCount == 0 || incPath->probeCount == 0)
    return;

if(brainSpPSetOut != NULL)
    {
    skipPSet = namePath->beds[0]->name;
    fprintf(brainSpPSetOut, "%s\t", refSeqForPSet(skipPSet));
    fprintf(brainSpPSetOut, "%s\t", skipPSet);
    fprintf(brainSpPSetOut, "%d\t", incPath->probeCount);
    for(i = 0; i < incPath->probeCount; i++)
	{
	fprintf(brainSpPSetOut, "%s,", incPath->beds[i]->name);
	}
    fprintf(brainSpPSetOut, "\t%d\t", event->geneProbeCount);
    for(i = 0; i < event->geneProbeCount; i++)
	{
	fprintf(brainSpPSetOut, "%s,", event->geneBeds[i]->name);
	}
    fprintf(brainSpPSetOut, "\n");
    }

fprintf(ratioStatOut, "%s", eventName);
fprintf(ratioSkipIntenOut, "%s", eventName);
fprintf(ratioIncIntenOut, "%s", eventName);
fprintf(ratioGeneIntenOut, "%s", eventName);
fprintf(ratioProbOut, "%s", eventName);
fprintf(incProbOut, "%s", eventName);
fprintf(skipProbOut, "%s", eventName);
fprintf(incProbCombOut, "%s", eventName);
fprintf(skipProbCombOut, "%s", eventName);
/* Loop through tissues caclulating ratio. */
for(tissueIx = 0; tissueIx < probM->colCount; tissueIx++)
    {
    double ratio = expressionRatio(event, skipPath, incPath, tissueIx);
    int bestProbeIx = determineBestProbe(event, incPath, intenM, probM, BIGNUM);
    fprintf(ratioStatOut, "\t%.6f", ratio);
    fprintf(ratioSkipIntenOut, "\t%.6f", expressionAverage(skipPath, tissueIx));
    fprintf(ratioIncIntenOut, "\t%.6f", expressionAverage(incPath, tissueIx));
    fprintf(ratioGeneIntenOut, "\t%.6f", geneExpression(event, tissueIx));
    fprintf(ratioProbOut, "\t%.6f", genePVal(event, tissueIx));
    fprintf(skipProbOut, "\t%.6f", pvalAltPathMaxExpressed(skipPath, tissueIx));
    fprintf(incProbOut, "\t%.6f", pvalAltPathMaxExpressed(incPath, tissueIx));
    fprintf(skipProbCombOut, "\t%.6f", 1-pvalAltPathCombExpressed(skipPath, tissueIx));
/*     fprintf(incProbCombOut, "\t%.6f", incPath->pVals[bestProbeIx][tissueIx]); */
    fprintf(incProbCombOut, "\t%.6f", 1-pvalAltPathCombExpressed(incPath, tissueIx));
    }
fprintf(ratioStatOut, "\n");
fprintf(ratioSkipIntenOut, "\n");
fprintf(ratioIncIntenOut, "\n");
fprintf(ratioGeneIntenOut, "\n");
fprintf(ratioProbOut, "\n");
fprintf(incProbOut, "\n");
fprintf(skipProbOut, "\n");
fprintf(incProbCombOut, "\n");
fprintf(skipProbCombOut, "\n");


}

void outputRatioStats(struct altEvent *event, struct dMatrix *probM,
		      struct dMatrix *intenM, boolean doVsGeneSets)
/* Calculate the ratio of skip to include for tissues that are
   considered expressed. For tissues where event isn't expressed
   output -1 as all ratios will be positive this makes a good empty
   data place holder. */
{
struct altPath *skipPath = NULL, *incPath = NULL, *namePath = NULL;
double tmpThresh = presThresh;

/* Few sanity checks. */
assert(ratioStatOut != NULL);
assert(intenM->colCount = probM->colCount);

/* Going to include everything that we think might
   have a chance of being expressed. */
if(!doVsGeneSets)
    {
    assert(slCount(event->altPathList) == 2);
    skipPath = event->altPathList;
    incPath = event->altPathList->next;
    namePath = skipPath;
    /* If there is no data don't output anything. */
    if(skipPath->probeCount == 0 || incPath->probeCount == 0)
	return;
    presThresh = absThresh;
    outputRatioStatsForPaths(event, incPath, skipPath, namePath, probM, intenM);
    presThresh = tmpThresh;
    }
else if(event->geneProbeCount > 0)
    {
    struct altPath *gene1Path = NULL, *gene2Path = NULL;
    presThresh = absThresh;
    gene1Path = altPathStubForGeneSet(event, 0);
    for(incPath = event->altPathList; incPath != NULL; incPath = incPath->next)
	{
	if(incPath->probeCount == 0)
	    continue;
	skipPath = gene1Path;
	namePath = incPath;
	outputRatioStatsForPaths(event, incPath, skipPath, namePath, probM, intenM);
	}
    freez(&gene1Path);
    presThresh = tmpThresh;
    }
}

void doEventAnalysis(struct altEvent *event, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Analyze a given event. */
{
int i = 0;
struct splice *splice = event->splice;
int **expressed = NULL;
int **notExpressed = NULL;
double **expression = NULL;
struct altPath *altPath = NULL;
int pathCount = slCount(event->altPathList);
int pathExpCount = 0;
int pathIx = 0;
int withProbes = 0;

/* Allocate a 2D array for expression. */
AllocArray(expressed, pathCount);
AllocArray(notExpressed, pathCount);
AllocArray(expression, pathCount);
for(i = 0; i < pathCount; i++)
    {
    AllocArray(expressed[i], probM->colCount);
    AllocArray(notExpressed[i], probM->colCount);
    AllocArray(expression[i], intenM->colCount);
    }

/* Fill in the array. */
for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
    {
    if(altPath->probeCount > 0)
	withProbes++;
    if(altPathExpressed(event, altPath, intenM, probM,
			expressed, notExpressed, expression, pathIx))
	pathExpCount++;

    /* Output the probabilities. */
    if(pathProbabilitiesOut != NULL && event->splice->type != altOther)
	{
	outputPathProbabilities(event, altPath, intenM, probM,
				expressed, notExpressed, expression, pathIx);
	}
    pathIx++;
    }

/* Determine our expression, alternate or otherwise. */
if(pathExpCount >= tissueExpThresh && withProbes > 1)
    event->isExpressed = TRUE;
if(pathExpCount >= tissueExpThresh * 2 && withProbes > 1)
    event->isAltExpressed = TRUE;

/* If we are outputting the beds for cassettes do it now. */
if(altCassetteBedOut && event->splice->type == altCassette && event->altPathProbeCount >= 2)
    {
    struct bed *outBed = pathToBed(event->altPathList->next->path, event->splice, -1, -1, TRUE);
    if(event->isAltExpressed)
	bedTabOutN(outBed, 12, altCassetteBedOut);
    else if(event->isExpressed)
	bedTabOutN(outBed, 12, expressedCassetteBedOut);
    else
	bedTabOutN(outBed, 12, notExpressedCassetteBedOut);
    bedFree(&outBed);
    }

/* If we are outputting a ratio statistic do it now. */
if(onlyTwoPaths(event) && ratioStatOut)
    {
    outputRatioStats(event, probM, intenM, FALSE);
    }
else if(( splice->type == altMutExclusive ||
	  splice->type == alt3PrimeSoft ||
	  splice->type == alt5PrimeSoft) &&
	ratioStatOut)
    {
    outputRatioStats(event, probM, intenM, TRUE);
    }

/* Get a background count of motifs in the cassettes. */
if(brainSpBedUpOut != NULL)
    outputBrainSpecificEvents(event, expressed, notExpressed, pathCount, probM);

if(tissueSpecificOut != NULL && event->isExpressed == TRUE)
    outputTissueSpecificEvents(event, expressed, notExpressed, pathCount, probM);

if(event->isAltExpressed && event->splice->type == altCassette && event->pVal < pvalThresh)
    {
    for(i = 0; i < bindSiteCount; i++)
	{
	cassetteUpMotifs[i] += event->altPathList->next->motifUpCounts[i];
	cassetteDownMotifs[i] += event->altPathList->next->motifDownCounts[i];
	cassetteInsideMotifs[i] += event->altPathList->next->motifInsideCounts[i];
	}
    cassetteExonCount++;
    }

/* Cleanup some memory. */
for(i = 0; i < pathCount; i++)
    {
    freez(&expressed[i]);
    freez(&notExpressed[i]);
    freez(&expression[i]);
    }
freez(&notExpressed);
freez(&expressed);
freez(&expression);
}

int stringOccurance(char *needle, char *haystack)
/* How many times is needle in haystack? */
{
int count = 0;
char *rest = haystack;
touppers(needle);
touppers(haystack);
while((rest = stringIn(needle, rest)) != NULL)
    {
    count++;
    rest++;
    }
return count;
}

int countMotifs(char *seq, struct bindSite *bs)
/* Count how many times this binding site appears in this
   sequence. */
{
int i = 0, j = 0;
int count = 0;
touppers(seq);
for(i = 0; i < bs->motifCount; i++)
    {
    count += stringOccurance(bs->motifs[i],seq);
    }
return count;
}

void doMotifCassetteAnalysis(struct altEvent *event)
/* Look to see how many motifs are found around this event. */
{
struct dnaSeq *upStream = NULL, *downStream = NULL, *tmp = NULL, *inside = NULL;
struct altPath *altPath = event->altPathList->next;
struct path *path = altPath->path;
int *vPos = event->splice->vPositions;
int *v = path->vertices;
int chromStart = vPos[v[1]];
int chromEnd = vPos[v[2]];
int motifWin = optionInt("motifWin", 200);
int i = 0;
if(event->splice->strand[0] == '-')
    {
    upStream = hChromSeq(path->tName, chromEnd, chromEnd+motifWin);
    reverseComplement(upStream->dna, upStream->size);
    downStream = hChromSeq(path->tName, chromStart-motifWin, chromStart);
    reverseComplement(downStream->dna, downStream->size);
    inside = hChromSeq(path->tName, chromStart, chromEnd);
    reverseComplement(inside->dna, inside->size);
    }
else
    {
    upStream = hChromSeq(path->tName, chromStart-motifWin, chromStart);
    downStream = hChromSeq(path->tName, chromEnd, chromEnd+motifWin);
    inside = hChromSeq(path->tName, chromStart, chromEnd);
    }
AllocArray(altPath->motifUpCounts, bindSiteCount);
AllocArray(altPath->motifDownCounts, bindSiteCount);
AllocArray(altPath->motifInsideCounts, bindSiteCount);
for(i = 0; i < bindSiteCount; i++)
    {
    altPath->motifUpCounts[i] += countMotifs(upStream->dna, bindSiteArray[i]);
    altPath->motifDownCounts[i] += countMotifs(downStream->dna, bindSiteArray[i]);
    altPath->motifInsideCounts[i] += countMotifs(inside->dna, bindSiteArray[i]);
    }
dnaSeqFree(&upStream);
dnaSeqFree(&downStream);
dnaSeqFree(&inside);
}

void doMotifControlAnalysis(struct altEvent *event)
/* Look to see how many motifs are found around this event. */
{
struct dnaSeq *upStream = NULL, *downStream = NULL, *tmp = NULL, *inside = NULL;
struct altPath *altPath = event->altPathList;
struct splice *splice = event->splice;
struct path *path = altPath->path;
struct bed *bed = NULL;
int *vPos = event->splice->vPositions;
int *v = path->vertices;
int chromStart = 0;
int chromEnd = 0;
int motifWin = optionInt("motifWin", 200);
int i = 0;
int blockIx;
bed = pathToBed(path, splice, -1, -1, FALSE);
if(bed == NULL)
    return;

/* Only do internal exons. */
for(blockIx = 1; blockIx < bed->blockCount - 1; blockIx++)
    {
    struct bed *outBed = NULL;

    /* Print out a bed of this exon. */
    AllocVar(outBed);
    outBed->chromStart = chromStart = bed->chromStart + bed->chromStarts[blockIx];
    outBed->chromEnd = chromEnd = chromStart + bed->blockSizes[blockIx];
    safef(outBed->strand,sizeof(outBed->strand),"%s", bed->strand);
    outBed->score = bed->score;
    outBed->chrom = cloneString(bed->chrom);
    outBed->name = cloneString(bed->name);
    subChar(outBed->name, '-', '\0');
    bedTabOutN(outBed, 6, brainSpPathBedOut);

    if(!skipMotifControls)
	{
	assert(constMotifCounts);
	fprintf(constMotifCounts, "%s:%s:%d-%d",
		outBed->name, outBed->chrom, outBed->chromStart, outBed->chromEnd);
	/* Get the sequences and do motif analysis. */
	if(event->splice->strand[0] == '-')
	    {
	    upStream = hChromSeq(path->tName, chromEnd, chromEnd+motifWin);
	    reverseComplement(upStream->dna, upStream->size);
	    downStream = hChromSeq(path->tName, chromStart-motifWin, chromStart);
	    reverseComplement(downStream->dna, downStream->size);
	    inside = hChromSeq(path->tName, chromStart, chromEnd);
	    reverseComplement(inside->dna, inside->size);
	    }
	else
	    {
	    upStream = hChromSeq(path->tName, chromStart-motifWin, chromStart);
	    downStream = hChromSeq(path->tName, chromEnd, chromEnd+motifWin);
	    inside = hChromSeq(path->tName, chromStart, chromEnd);
	    }
	touppers(upStream->dna);
	touppers(downStream->dna);
	touppers(inside->dna);
	controlInsideBpCount += inside->size;
	if(altPath->motifUpCounts == NULL)
	    {
	    AllocArray(altPath->motifUpCounts, bindSiteCount);
	    AllocArray(altPath->motifDownCounts, bindSiteCount);
	    AllocArray(altPath->motifInsideCounts, bindSiteCount);
	    }
	for(i = 0; i < bindSiteCount; i++)
	    {
	    int upStreamCount = countMotifs(upStream->dna, bindSiteArray[i]);
	    int downStreamCount = countMotifs(downStream->dna, bindSiteArray[i]);
	    int insideCount = countMotifs(inside->dna, bindSiteArray[i]);

	    /* Report our counts. */
	    fprintf(constMotifCounts, "\t%d\t%d", upStreamCount, downStreamCount);

	    altPath->motifUpCounts[i] += upStreamCount;
	    controlUpMotifs[i] += upStreamCount;
	    altPath->motifDownCounts[i] += downStreamCount;
	    controlDownMotifs[i] += downStreamCount;
	    altPath->motifInsideCounts[i] += insideCount;
	    controlInsideMotifs[i] += insideCount;

	    }
	bedFree(&outBed);
	/* Finish control line. */
	fprintf(constMotifCounts, "\n");
	controlExonCount++;
	}
    }
dnaSeqFree(&upStream);
dnaSeqFree(&downStream);
dnaSeqFree(&inside);
}

int flipScoreCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct slRef *a = *((struct slRef**)va);
const struct slRef *b = *((struct slRef **)vb);
struct altEvent *aE = a->val;
struct altEvent *bE = b->val;
if(fabs(aE->flipScore) > fabs(bE->flipScore))
    return -1;
else if(fabs(aE->flipScore) < fabs(bE->flipScore))
    return 1;
return 0;
}

int percentUltraCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct slRef *a = *((struct slRef**)va);
const struct slRef *b = *((struct slRef **)vb);
struct altEvent *aE = a->val;
struct altEvent *bE = b->val;
if(aE->percentUltra < bE->percentUltra)
    return 1;
else if(aE->percentUltra > bE->percentUltra)
    return -1;
return 0;
}

boolean isHardExon(unsigned char *vTypes, int v1, int v2)
/* Return TRUE if this is a hard exon. */
{
if(ggExon == pathEdgeType(vTypes, v1, v2) &&
   (vTypes[v1] == ggHardStart || vTypes[v1] == ggHardEnd) &&
   (vTypes[v2] == ggHardStart || vTypes[v2] == ggHardEnd))
    return TRUE;
return FALSE;
}

void doControlStats(struct altEvent *event)
/* Calculate control stats for control events. */
{
struct splice *splice = event->splice;
struct path *path = event->altPathList->path;
int chromStart = 0, chromEnd = 0;
char strand;
char *chrom;
int vertIx = 0;
int *verts = NULL;
int vC = path->vCount;
int *vPos = splice->vPositions;
unsigned char *vTypes = splice->vTypes;
struct dyString *name = newDyString(128);
for(vertIx = 0; vertIx < vC - 1; vertIx++)
    {
    if(isHardExon(vTypes, vertIx, vertIx+1))
	{
	char *startChrom = cloneString(path->tName);
	char *endChrom = cloneString(path->tName);
	char *cassChrom = cloneString(path->tName);
	int intStart = vPos[vertIx == 0 ? path->upV : vertIx-1];
	int dummy = 0;
	int chromStart = vPos[vertIx];
	int chromEnd = vPos[vertIx+1];
	int intEnd = vPos[vertIx+1 == vC - 1 ? path->downV : vertIx+2];
	char strand = splice->strand[0];
	boolean lifted = TRUE;
	dyStringClear(name);
	/* Lift over each end seperately so as to ignore changes
	   in assembly in the middle. */
	dummy = intStart-10;
	lifted &= liftOverCoords(&startChrom, &dummy, &intStart, &strand);
	strand = splice->strand[0];
	dummy = intEnd+10;
	lifted &= liftOverCoords(&endChrom, &intEnd, &dummy, &strand);
	strand = splice->strand[0];
	lifted &= liftOverCoords(&cassChrom, &chromStart, &chromEnd, &strand);
	if(lifted && sameString(endChrom, cassChrom) && sameString (startChrom, cassChrom))
	    {
	    dyStringPrintf(name, "%s.%s:%d-%d", splice->name, startChrom, chromStart, chromEnd);
	    fillInStatsForEvent(controlValuesVm, name->string, startChrom,
				intStart, chromStart, chromEnd, intEnd, strand,
				altControl);
	    }
	}
    }
dyStringFree(&name);
}

void doAnalysis(struct altEvent *eventList, struct dMatrix *intenM,
		struct dMatrix *probM)
/* How many of the alt events are expressed. */
{
struct altEvent *event = NULL;
int i = 0;
struct altPath *altPath = NULL;
int expressed = 0, altExpressed = 0;
struct slRef *ref = NULL;
int contCount = 0;
char *prefix = optionVal("brainSpecific", NULL);
char *useDb = newDb;
for(event = eventList; event != NULL; event = event->next)
    {
    if(event->splice->type == altCassette && bindSiteCount > 0)
	doMotifCassetteAnalysis(event);
    else if(event->splice->type == altControl && bindSiteCount > 0)
	doMotifControlAnalysis(event);

    if(controlValuesVm != NULL && event->splice->type == altControl)
	{
	if(++contCount % 100 == 0)
	    {
	    fputc('*',stderr);
	    fflush(stderr);
	    contCount = 0;
	    }
	doControlStats(event);
	}
    doEventAnalysis(event, intenM, probM);

    if(event->isExpressed)
	logSpliceTypeExp(event->splice->type);
    if(event->isAltExpressed)
	logSpliceTypeAltExp(event->splice->type);
    }
if(brainSpBedUpOut != NULL)
    {
    char *chrom = NULL;
    int chromStart = 0, chromEnd = 0;
    struct bed *bed = NULL;

    if(optionExists("flipScoreSort"))
	slSort(&brainSpEvents, flipScoreCmp);
    else
	slSort(&brainSpEvents, percentUltraCmp);
    vmWriteVals(brainSpecificValues, brainSpValuesOut);
    event = brainSpEvents->val;
    altPath = event->altPathList->next;
    bed = pathToBed(altPath->path, event->splice, -1, -1, FALSE);
    chrom = cloneString(bed->chrom);
    chromStart = bed->chromStart;
    chromEnd = bed->chromEnd;
    fprintf(brainSpFrameHtmlOut, "<html><head><title>%s Specific Events.</title></head>\n"
	    "<frameset cols=\"30%,70%\">\n"
	    "   <frame name=\"_list\" src=\"./%s%s\">\n"
	    "   <frameset rows=\"50%,50%\">\n"
	    "      <frame name=\"browser\" src=\"http://%s/cgi-bin/hgTracks?db=%s&hgt.motifs=GCATG,CTCTCT,GGGG&position=%s:%d-%d\">\n"
	    "      <frame name=\"plots\" src=\"http://%s/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">\n"
	    "   </frameset>\n"
	    "</frameset>\n"
	    "</html>\n", optionVal("brainTissues","Brain"), prefix, ".table.html",
	    browserName, useDb,
	    chrom, chromStart, chromEnd,
	    browserName, useDb,
	    chrom, chromStart, chromEnd
	    );
    carefulClose(&brainSpFrameHtmlOut);
    for(ref = brainSpEvents; ref != NULL; ref = ref->next)
	{
	struct altEvent *bEvent = ref->val;
	printLinks(bEvent);
	}
    }
}

double calcPercent(double numerator, double denominator)
/* Calculate a percent checking for zero. */
{
if(denominator != 0)
    return (100.0 * numerator / denominator);
return 0;
}

double cph(double numerator, double denominator)
/* Calculate a ratio checking for zero. */
{
if(denominator != 0)
    return (numerator / denominator);
return 0;
}

void reportTissueSpecificCounts(char **tissueNames, int tissueCount)
/* Writ out a matrix of tissues and their counts. */
{
int i = 0, j = 0;

assert(tissueSpecificOut);

fprintf(tissueSpecificOut, "\taltCassette\taltMutExclusive\talt5Prime\talt3Prime\taltOther\n");
for(i = 0; i < tissueCount; i++)
    {
    fprintf(tissueSpecificOut, "%-22s\t%4d\t%4d\t%4d\t%4d\t%4d\n", tissueNames[i],
	    tissueSpecificCounts[i][altCassette], tissueSpecificCounts[i][altMutExclusive],
	    tissueSpecificCounts[i][alt5Prime],tissueSpecificCounts[i][alt3Prime],
	    tissueSpecificCounts[i][altOther]);
    }
}

void reportMotifCounts(int *up, int *down, int *in, int total, int bpTotal)
{
int *u = up;
int *d = down;
int *i = in;
int t = total;
int bt = bpTotal;
struct bindSite **b = bindSiteArray;
assert(up);
assert(down);
fprintf(stderr, "+----------+--------------+----------------+----------------+-----------------+----------------+----------------+\n");
fprintf(stderr, "| Position | %12s | %14s | %14s | %15s | %14s | %14s |\n",
	b[0]->rnaBinder, b[1]->rnaBinder, b[2]->rnaBinder, b[3]->rnaBinder, b[4]->rnaBinder, b[5]->rnaBinder);
fprintf(stderr, "+----------+--------------+----------------+----------------+-----------------+----------------+----------------+\n");
//fprintf(stderr, "+----------+------------+--------------+--------------+---------------+--------------+--------------+\n");
fprintf(stderr, "| %-8s | %3d (%4.4f) | %5d (%4.4f) | %5d (%4.4f) | %6d (%4.4f) | %5d (%4.4f) | %5d (%4.4f) |\n",
	"UpStream", u[0], cph(u[0],t), u[1], cph(u[1],t), u[2], cph(u[2],t), u[3], cph(u[3],t), u[4],cph(u[4],t), u[5],cph(u[5],t));
fprintf(stderr, "| %-8s | %3d (%4.4f) | %5d (%4.4f) | %5d (%4.4f) | %6d (%4.4f) | %5d (%4.4f) | %5d (%4.4f) |\n",
	"DnStream", d[0], cph(d[0],t), d[1], cph(d[1],t), d[2], cph(d[2],t), d[3], cph(d[3],t), d[4],cph(d[4],t), d[5],cph(d[5],t));
fprintf(stderr, "| %-8s | %3d (%1.4f) | %5d (%1.4f) | %5d (%1.4f) | %6d (%1.4f) | %5d (%1.4f) | %5d (%1.4f) |\n",
	"Inside  ", i[0], cph(i[0],bt), i[1], cph(i[1],bt), i[2], cph(i[2],bt), i[3], cph(i[3],bt), i[4],cph(i[4],bt), i[5],cph(i[5],bt));
fprintf(stderr, "+----------+--------------+----------------+----------------+-----------------+----------------+----------------+\n");

}


void reportBrainSpCounts()
/* Print brain specific results. */
{
if(brainSpDnaUpOut != NULL)
    {
    int *bC = brainSpecificCounts;
    char *tissue = optionVal("brainTissues","Brain");
    fprintf(stderr, "%d %s Specific Events. %d %s Specific paths\n",
	     brainSpecificEventCount, tissue, brainSpecificPathCount, tissue);
    fprintf(stderr, "+----------+---------+-------+-------+-------+-------+\n");
    fprintf(stderr, "| Cassette | MutExcl | Alt3' | Alt5' | TxEnd | Other |\n");
    fprintf(stderr, "+----------+---------+-------+-------+-------+-------+\n");
    fprintf(stderr, "| %8d | %7d | %5d | %5d | %5d | %5d |\n",
	    bC[altCassette], bC[altMutExclusive], bC[alt3Prime], bC[alt5Prime],
	    bC[alt3PrimeSoft], bC[altOther]);
    fprintf(stderr, "+----------+---------+-------+-------+-------+-------+\n");
    if(bindSiteCount != 0)
	{
	int *u = brainSpecificMotifUpCounts;
	int *d = brainSpecificMotifDownCounts;
	int *i = brainSpecificMotifInsideCounts;
	int bp = brainSpecificMotifInsideBpCount;
	struct bindSite **b = bindSiteArray;
	fprintf(stderr,"Cassette binding sites in %d include exons:\n", bC[altCassette]-cassetteExonSkipCount);
	reportMotifCounts(u,d,i,bC[altCassette] - cassetteExonSkipCount, bp);
	u = cassetteSkipUpMotifs;
	d = cassetteSkipDownMotifs;
	i = cassetteSkipInsideMotifs;
	bp = cassetteSkipInsideBpCount;
	fprintf(stderr,"Cassette binding sites in %d skip exons:\n", cassetteExonSkipCount);
	reportMotifCounts(u,d,i,cassetteExonSkipCount, bp);
	}
    }
}

void reportEventCounts()
/* Print some stats about splices and probe with counts. */
{
fprintf(stderr, "+----------------------+-------+-----------+-----------+------+---------+\n");
fprintf(stderr, "| Alt Event.           | Count | w/ Probes | Expressed | Alt. | Percent |\n");
fprintf(stderr, "+----------------------+-------+-----------+-----------+------+---------+\n");
fprintf(stderr, "| alt 5'               | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	alt5PrimeCount, alt5PrimeWProbeCount, alt5PrimeExpCount, alt5PrimeAltExpCount,
	calcPercent(alt5PrimeAltExpCount, alt5PrimeExpCount));
fprintf(stderr, "| alt 3'               | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	alt3PrimeCount, alt3PrimeWProbeCount, alt3PrimeExpCount, alt3PrimeAltExpCount,
	calcPercent(alt3PrimeAltExpCount, alt3PrimeExpCount));
fprintf(stderr, "| alt Cass             | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	altCassetteCount, altCassetteWProbeCount, altCassetteExpCount, altCassetteAltExpCount,
	calcPercent(altCassetteAltExpCount, altCassetteExpCount));
fprintf(stderr, "| alt Ret. Int.        | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	altRetIntCount, altRetIntWProbeCount, altRetIntExpCount, altRetIntAltExpCount,
	calcPercent(altRetIntAltExpCount, altRetIntExpCount));
fprintf(stderr, "| alt Mutual Exclusive | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	altMutExclusiveCount, altMutExclusiveWProbeCount, altMutExclusiveExpCount, altMutExclusiveAltExpCount,
	calcPercent(altMutExclusiveAltExpCount, altMutExclusiveExpCount));
fprintf(stderr, "| alt Txn Start        | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	alt5PrimeSoftCount, alt5PrimeSoftWProbeCount, alt5PrimeSoftExpCount, alt5PrimeSoftAltExpCount,
	calcPercent(alt5PrimeSoftAltExpCount, alt5PrimeSoftExpCount));
fprintf(stderr, "| alt Txn End          | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	alt3PrimeSoftCount, alt3PrimeSoftWProbeCount, alt3PrimeSoftExpCount, alt3PrimeSoftAltExpCount,
	calcPercent(alt3PrimeSoftAltExpCount, alt3PrimeSoftExpCount));
fprintf(stderr, "| alt Other            | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	altOtherCount, altOtherWProbeCount, altOtherExpCount, altOtherAltExpCount,
	calcPercent(altOtherAltExpCount, altOtherExpCount));
fprintf(stderr, "| alt Control          | %5d |      %4d |      %4d |  %3d |  %5.1f%% |\n",
	altControlCount, altControlWProbeCount, altControlExpCount, altControlAltExpCount,
	calcPercent(altControlAltExpCount, altControlExpCount));
fprintf(stderr, "+----------------------+-------+-----------+-----------+------+---------+\n");

/* if(bindSiteCount != 0) */
/*     { */
/*     int *u = cassetteUpMotifs; */
/*     int *d = cassetteDownMotifs; */
/*     int *i = cassetteInsideMotifs; */
/*     struct bindSite **b = bindSiteArray; */
/*     fprintf(stderr,"Cassette binding sites in included %d include exons:\n", cassetteExonCount); */
/*     reportMotifCounts(u,d,i,cassetteExonCount); */

/*     } */

if(bindSiteCount != 0)
    {
    int *u = controlUpMotifs;
    int *d = controlDownMotifs;
    int *i = controlInsideMotifs;
    struct bindSite **b = bindSiteArray;
    fprintf(stderr,"Control binding sites in %d exons:\n", controlExonCount);
    reportMotifCounts(u,d,i,controlExonCount, controlInsideBpCount);
    }
}

void initLiftOver()
/* Initialize the liftOver process. */
{
char *from = optionVal("db", NULL);
char *to = optionVal("newDb", NULL);
char *chainFile = NULL;

assert(from);
hSetDb2(to);
if(to == NULL)
    errAbort("Must specify a -newDb to do a lift.");
chainFile = liftOverChainFile(from, to);
if(chainFile == NULL)
    errAbort("Couldn't load a lift chainFile for %s->%s", from, to);
liftOverHash = newHash(12);
readLiftOverMap(chainFile, liftOverHash);
}

boolean liftOverCoords(char **chrom, int *chromStart, int *chromEnd, char *strand)
/* Return TRUE and new values if values passed in can be lifted. */
{
char *newChrom = NULL;
int newStart = 0, newEnd = 0;
char newStrand;
char *msg = NULL;
boolean success = FALSE;
if(liftOverHash != NULL)
    msg = liftOverRemapRange(liftOverHash, .95, *chrom, *chromStart, *chromEnd, *strand,
			     1.0, &newChrom, &newStart, &newEnd, &newStrand);
if(msg == NULL)
    {
    freez(chrom);
    *chrom = newChrom;
    *chromStart = newStart;
    *chromEnd = newEnd;
    *strand = newStrand;
    success = TRUE;
    }
return success;
}

void initTissueSpecific(int tissueCount)
/* Open up the files for tissue specific isoforms and dna. */
{
char *name = optionVal("tissueSpecific", NULL);
int i = 0;
AllocArray(tissueSpecificCounts, tissueCount);
for(i = 0; i < tissueCount; i++)
    {
    AllocArray(tissueSpecificCounts[i], 20);
    }
tissueSpecificOut = mustOpen(name, "w");
}

void initBrainSpecific()
/* Open up the files for brain specific isoforms and dna. */
{
struct bindSite *bs = NULL;
char *prefix = optionVal("brainSpecific", NULL);
struct dyString *file = newDyString(strlen(prefix)+10);
char *useDb = newDb;
brainSpecificStrict = optionExists("brainSpecificStrict");
brainSpecificValues = newValuesMat(400,15);
AllocArray(brainSpecificCounts, altMutExclusive + 1);

if(bindSiteCount > 0)
    {
    AllocArray(brainSpecificMotifUpCounts, bindSiteCount);
    AllocArray(brainSpecificMotifDownCounts, bindSiteCount);
    AllocArray(brainSpecificMotifInsideCounts, bindSiteCount);
    }

dyStringClear(file);
dyStringPrintf(file, "%s.path.bed", prefix);
brainSpPathBedOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.probeSets.tab", prefix);
brainSpPSetOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.path.ends.bed", prefix);
brainSpPathEndsBedOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.path.exonUp.bed", prefix);
brainSpPathBedExonUpOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.path.exonDown.bed", prefix);
brainSpPathBedExonDownOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.up.fa", prefix);
brainSpDnaUpOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.merged.fa", prefix);
brainSpDnaMergedOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.cons.fa", prefix);
brainSpConsDnaOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.cons.bed", prefix);
brainSpConsBedOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.down.fa", prefix);
brainSpDnaDownOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.up.bed", prefix);
brainSpBedUpOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.down.bed", prefix);
brainSpBedDownOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.values.tab", prefix);
brainSpValuesOut = mustOpen(file->string, "w");

dyStringClear(file);
dyStringPrintf(file, "%s.table.html", prefix);
brainSpTableHtmlOut = mustOpen(file->string, "w");
fprintf(brainSpTableHtmlOut, "<html>\n<body bgcolor=\"#FFF9D2\"><b>Alt-Splice List</b>\n");

fprintf(brainSpTableHtmlOut, "<tr><td><b>Motif Order:</b>");
for(bs = bindSiteList; bs != NULL; bs = bs->next)
    fprintf(brainSpTableHtmlOut, "%s, ", bs->rnaBinder);
fprintf(brainSpTableHtmlOut, "</td></tr>");
fprintf(brainSpTableHtmlOut, 	"<table border=1><tr><th>Name</th><th>Sep</th></tr>\n");
dyStringClear(file);
dyStringPrintf(file, "%s.frame.html", prefix);
brainSpFrameHtmlOut = mustOpen(file->string, "w");


dyStringFree(&file);
}

void printCdtHeader(FILE *out, struct dMatrix *dM)
{
int i = 0;
fprintf(out, "YORF\tNAME");
for(i = 0; i < dM->colCount; i++)
    {
    fprintf(out, "\t%s", dM->colNames[i]);
    }
fprintf(out, "\n");
}

void initRatioStats(struct dMatrix *intenM)
/* Setupt a file to output ratio statistics to. */
{
int i = 0;
char *outputRatioStats = optionVal("outputRatioStats", NULL);
struct dyString *buff = NULL;
assert(outputRatioStats);
assert(intenM);
buff = newDyString(strlen(outputRatioStats)+30);
ratioStatOut = mustOpen(outputRatioStats, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.skip.intensity", outputRatioStats);
ratioSkipIntenOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.inc.intensity", outputRatioStats);
ratioIncIntenOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.gene.prob", outputRatioStats);
ratioProbOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.gene.intensity", outputRatioStats);
ratioGeneIntenOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.inc.prob", outputRatioStats);
incProbOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.skip.prob", outputRatioStats);
skipProbOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.inc.comb.prob", outputRatioStats);
incProbCombOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.skip.comb.prob", outputRatioStats);
skipProbCombOut = mustOpen(buff->string, "w");

for(i = 0; i < intenM->colCount - 1; i++)
    {
    fprintf(ratioStatOut, "%s\t", intenM->colNames[i]);
    fprintf(ratioSkipIntenOut, "%s\t", intenM->colNames[i]);
    fprintf(ratioIncIntenOut, "%s\t", intenM->colNames[i]);
    fprintf(ratioGeneIntenOut, "%s\t", intenM->colNames[i]);
    fprintf(ratioProbOut, "%s\t", intenM->colNames[i]);
    fprintf(skipProbOut, "%s\t", intenM->colNames[i]);
    fprintf(incProbOut, "%s\t", intenM->colNames[i]);
    fprintf(skipProbCombOut, "%s\t", intenM->colNames[i]);
    fprintf(incProbCombOut, "%s\t", intenM->colNames[i]);
    }
fprintf(ratioStatOut, "%s\n", intenM->colNames[i]);
fprintf(ratioSkipIntenOut, "%s\n", intenM->colNames[i]);
fprintf(ratioIncIntenOut, "%s\n", intenM->colNames[i]);
fprintf(ratioGeneIntenOut, "%s\n", intenM->colNames[i]);
fprintf(ratioProbOut, "%s\n", intenM->colNames[i]);
fprintf(skipProbOut, "%s\n", intenM->colNames[i]);
fprintf(incProbOut, "%s\n", intenM->colNames[i]);
fprintf(skipProbCombOut, "%s\n", intenM->colNames[i]);
fprintf(incProbCombOut, "%s\n", intenM->colNames[i]);
}

void outputProbeMatches(struct altEvent *eventList)
/* Loop through the event list and output the different probes for
   each path. Only do events that have two paths. */
{
struct altEvent *event = NULL;
FILE *out = NULL;
char *fileName = optionVal("outputProbeMatch", NULL);
struct altPath *altPath = NULL;
int i = 0;
assert(fileName);
out = mustOpen(fileName, "w");
for(event = eventList; event != NULL; event = event->next)
    {
    if(slCount(event->altPathList) != 2 || event->altPathProbeCount != 2)
	continue;
    fprintf(out, "%s", event->splice->name);

    /* Loop through the alternative isoforms and print comma separated probes for each path. */
    for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
	{
	fputc('\t', out);
	for(i = 0; i < altPath->probeCount; i++)
	    {
	    struct bed *bed = altPath->beds[i];
	    fprintf(out, "%s,", bed->name);
	    }
	}
    fputc('\n', out);
    }
carefulClose(&out);
}

void outputProbePvals(struct altEvent *eventList, struct dMatrix *probM)
/* Loop through the events and output the probe probabilities. Only do events
 that have two paths. */
{
struct altEvent *event = NULL;
struct altPath *altPath = NULL;
char *fileName = optionVal("outputProbePvals", NULL);
int probeIx = 0;
int expIx = 0;
int expCount = 0;
FILE *out = NULL;
assert(fileName);
assert(probM);
out = mustOpen(fileName, "w");
expCount = probM->colCount;

/* Output the header of column names. */
for(expIx = 0; expIx < expCount-1; expIx++)
    fprintf(out, "%s\t", probM->colNames[expIx]);
fprintf(out, "%s\n", probM->colNames[expIx]);

for(event = eventList; event != NULL; event = event->next)
    {
    if(slCount(event->altPathList) != 2 || event->altPathProbeCount != 2)
	continue;
    for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
	{
	for(probeIx = 0; probeIx < altPath->probeCount; probeIx++)
	    {
	    fprintf(out, "%s", altPath->beds[probeIx]->name);
	    for(expIx = 0; expIx < expCount; expIx++)
		{
		fprintf(out, "\t%.5f", altPath->pVals[probeIx][expIx]);
		}
	    fputc('\n',out);
	    }
	}
    }
carefulClose(&out);
}

void altProbes()
/* Top level function to map probes to paths and analyze. */
{
struct splice *spliceList = NULL;
struct altEvent *eventList = NULL, *event = NULL;
struct dMatrix *intenM = NULL, *probM = NULL;
char *intenIn = optionVal("intensityFile", NULL);
char *probIn = optionVal("probFile", NULL);
char *splicesIn = optionVal("spliceFile", NULL);
if(splicesIn == NULL)
    errAbort("Must specify a apliceFile");

warn("Loading splices from %s", splicesIn);
spliceList = spliceLoadAll(splicesIn);
eventList = altEventsFromSplices(spliceList);
warn("Loading beds.");
bedLoadChromKeeper();
dotForUserInit(max(slCount(eventList)/20, 1));
for(event = eventList; event != NULL; event = event->next)
    {
    dotForUser();
    findProbeSetsForEvents(event);
    logSpliceType(event->splice->type);
    if(event->altPathProbeCount >= 2)
	{
	logSpliceTypeWProbe(event->splice->type);
	}
    }
warn("");
if(optionExists("outputProbeMatch"))
    outputProbeMatches(eventList);
warn("Reading Data Matrixes.");
if(probIn == NULL || intenIn == NULL)
    errAbort("Must specify intensityFile and probeFile");
intenM = dMatrixLoad(intenIn);
probM = dMatrixLoad(probIn);

/* If we're doing path probabilities print some headers. */
if(pathProbabilitiesOut != NULL)
    {
    printCdtHeader(pathProbabilitiesOut, probM);
    printCdtHeader(pathExpressionOut, intenM);
    }

if(optionExists("tissueSpecific"))
    initTissueSpecific(probM->colCount);

if(optionExists("outputRatioStats"))
   initRatioStats(intenM);

warn("Filling in event data.");
fillInEventData(eventList, intenM, probM);
warn("Doing analysis");
if(optionExists("outputProbePvals"))
    outputProbePvals(eventList, probM);
doAnalysis(eventList, intenM, probM);
reportEventCounts();
reportBrainSpCounts();
if(intronsConsCounted > 0)
    warn("%d of %d introns are heavily conserved %.2f%%",
	 intronsConserved, intronsConsCounted, (100.0 * intronsConserved)/intronsConsCounted);
if(tissueSpecificOut)
    reportTissueSpecificCounts(probM->colNames, probM->colCount);
if(brainSpTableHtmlOut)
    fprintf(brainSpTableHtmlOut, "</table></body></html>\n");
if(controlValuesVm != NULL)
    vmWriteVals(controlValuesVm, controlValuesOut);
carefulClose(&brainSpTableHtmlOut);
carefulClose(&brainSpDnaUpOut);
carefulClose(&brainSpDnaDownOut);
carefulClose(&brainSpBedUpOut);
carefulClose(&brainSpBedDownOut);
carefulClose(&brainSpPathBedOut);
}

boolean unitTestFunct(struct unitTest *test, boolean passed,
			 struct dyString *error, char *errorMsg)
/* Wrapper around printing things to the error. */
{
assert(error);
if(!passed)
    dyStringPrintf(error, "%s, ", errorMsg);
return passed;
}


boolean testStringOccurance(struct unitTest *test)
/* Test how many times a motif is counted. */
{
int count = 0;

if(stringOccurance(cloneString("ggGG"), cloneString("nnnnnggggg")) != 2)
    return FALSE;

if(stringOccurance(cloneString("ggGG"), cloneString("nnnnngggggnnnn")) != 2)
    return FALSE;

if(stringOccurance(cloneString("ggGG"), cloneString("gggggnnnn")) != 2)
    return FALSE;

if(stringOccurance(cloneString("gcATG"), cloneString("GcatGcattg")) != 1)
    return FALSE;

return TRUE;
}

boolean testCdfChiSqP(struct unitTest *test)
/* Test the gsl results for the chisq function. Gold values from R.*/
{
double result = 0;
char buff[256];

/* Run the function for som test values, print to
   a buffer to get the precision / rounding right. */
result = gsl_cdf_chisq_P(-2*log(.9),2);
safef(buff, sizeof(buff), "%.7f", result);
if(differentString(buff, "0.1000000"))
    return FALSE;

result = gsl_cdf_chisq_P(-2*log(.1),2);
safef(buff, sizeof(buff), "%.7f", result);
if(differentString(buff, "0.9000000"))
    return FALSE;

result = gsl_cdf_chisq_P(-2*(log(.1)+log(.9)+log(.5)),6);
safef(buff, sizeof(buff), "%.7f", result);
if(differentString(buff, "0.5990734"))
    return FALSE;

result = gsl_cdf_chisq_P(-2*(log(.8)+log(.8)),4);
safef(buff, sizeof(buff), "%.8f", result);
if(differentString(buff, "0.07437625"))
    return FALSE;
return TRUE;
}

boolean testPathContains(struct unitTest *test)
/* Test the path contains block functionality. */
{
struct splice *ndr2 = ndr2CassTest();
struct path *skipPath = NULL, *incPath = NULL;
struct dyString *error = newDyString(128);
boolean result = TRUE, currentResult = TRUE;
skipPath = ndr2->paths;
incPath = ndr2->paths->next;

if(pathContainsIntron(ndr2, skipPath, "chr14", 43771385+15, 43772095-15, "-") != TRUE)
    currentResult = FALSE;
if(pathContainsIntron(ndr2, skipPath, "chr14", 43771385+15, 43771715-15, "-") != FALSE)
    currentResult = FALSE;
if(pathContainsIntron(ndr2, skipPath, "chr14", 43771727+15, 43772095-15, "-") != FALSE)
    currentResult = FALSE;
if(currentResult == FALSE)
    {
    dyStringPrintf(error, "Problem with placing introns on skip path, ");
    result = FALSE;
    }

currentResult = TRUE;
if(pathContainsIntron(ndr2, incPath, "chr14", 43771385+15, 43772095-15, "-") != FALSE)
    currentResult = FALSE;
if(pathContainsIntron(ndr2, incPath, "chr14", 43771385+15, 43771715-15, "-") != TRUE)
    currentResult = FALSE;
if(pathContainsIntron(ndr2, incPath, "chr14", 43771727+15, 43772095-15, "-") != TRUE)
    currentResult = FALSE;
if(currentResult == FALSE)
    {
    dyStringPrintf(error, "Problem with placing introns on skip path, ");
    result = FALSE;
    }

currentResult = TRUE;
if(pathContainsBlock(ndr2, incPath,  "chr14", 43771727, 43771727+15, "-", TRUE) != TRUE)
    currentResult = FALSE;
if(pathContainsBlock(ndr2, incPath,  "chr14", 43771703, 43771735, "-", TRUE) != TRUE)
    currentResult = FALSE;
if(pathContainsBlock(ndr2, skipPath,  "chr14", 43771703, 43771735, "-", TRUE) != FALSE)
    currentResult = FALSE;
if(currentResult == FALSE)
    {
    dyStringPrintf(error, "Problem placing blocks with all bases, ");
    result = FALSE;
    }

currentResult = TRUE;
if(pathContainsBlock(ndr2, incPath,  "chr14", 43771727, 43771727+16, "-", TRUE) != FALSE)
    currentResult = FALSE;
if(pathContainsBlock(ndr2, incPath,  "chr14", 43771727, 43771727+16, "-", FALSE) != TRUE)
    currentResult = FALSE;
if(currentResult == FALSE)
    {
    dyStringPrintf(error, "Problem placing blocks with not all bases, ");
    result = FALSE;
    }

test->errorMsg = cloneString(error->string);
dyStringFree(&error);
return result;
}

boolean testPathContainsBed(struct unitTest *test)
/* Test to see if a path contains the right stuff. */
{
struct splice *ndr2 = ndr2CassTest();
struct bed *bedList = ndr2BedTest(), *bed = NULL;
/* 1 include, 2 skip, 3 include, 4 gene. */
struct path *skipPath = NULL, *incPath = NULL;
boolean result = TRUE;
struct dyString *error = newDyString(128);
skipPath = ndr2->paths;
incPath = ndr2->paths->next;

/* First include. */
bed = bedList;
if(pathContainsBed(ndr2, skipPath, bed, TRUE) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Matching include bed to skip, ");
    }
if(pathContainsBed(ndr2, incPath, bed, TRUE) != TRUE)
    {
    result = FALSE;
    dyStringPrintf(error, "Not matching include bed to include, ");
    }

/* Skip probe set. */
bed = bed->next;
if(pathContainsBed(ndr2, skipPath, bed, TRUE) != TRUE)
    {
    result = FALSE;
    dyStringPrintf(error, "Not matching skip bed to skip, ");
    }
if(pathContainsBed(ndr2, incPath, bed, TRUE) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Matching skip bed to include, ");
    }

/* Second include. */
bed = bed->next;
if(pathContainsBed(ndr2, skipPath, bed, TRUE) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Matching 2nd include bed to skip, ");
    }
if(pathContainsBed(ndr2, incPath, bed, TRUE) != TRUE)
    {
    result = FALSE;
    dyStringPrintf(error, "Not matching 2nd include bed to include, ");
    }

/* Gene probe set. */
bed = bed->next;
if(pathContainsBed(ndr2, skipPath, bed, FALSE) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Matching gene set to skip path,  ");
    }

if(pathContainsBed(ndr2, incPath, bed, FALSE) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Matching gene set to include path,  ");
    }
test->errorMsg = cloneString(error->string);
dyStringFree(&error);
return result;
}

struct altEvent *setupNdr2AltEvent()
/* Setup an ndr2 fake event for testing. */
{
struct splice *splice = ndr2CassTest();
struct altEvent *event = NULL;
struct path *path = NULL;
struct altPath *altPath = NULL;
struct dMatrix *probM = ndr2BedTestMat(TRUE);
struct dMatrix *intenM = ndr2BedTestMat(FALSE);
double expression = 0;

AllocVar(event);
event->splice = splice;
AllocArray(event->geneExpVals, 1);
AllocArray(event->genePVals, 1);
event->geneProbeCount = 1;
event->altPathProbeCount = 2;
event->geneExpVals[0] = intenM->matrix[3];
event->genePVals[0] = probM->matrix[3];

AllocVar(altPath);
altPath->path = splice->paths;
altPath->probeCount = 1;
AllocArray(altPath->expVals, altPath->probeCount);
altPath->expVals[0] =  intenM->matrix[1];
AllocArray(altPath->pVals, altPath->probeCount);
altPath->pVals[0] = probM->matrix[1];
slAddHead(&event->altPathList, altPath);

AllocVar(altPath);
altPath->path = splice->paths->next;
altPath->probeCount = 2;
AllocArray(altPath->expVals, altPath->probeCount);
altPath->expVals[0] = intenM->matrix[0];
altPath->expVals[1] = intenM->matrix[2];
AllocArray(altPath->pVals, altPath->probeCount);
altPath->pVals[0] = probM->matrix[0];
altPath->pVals[1] = probM->matrix[2];
slAddHead(&event->altPathList, altPath);

slReverse(&event->altPathList);
return event;
}

boolean testAltPathProbesExpressed(struct unitTest *test)
/* Test to see if presense abs software is calling. */
{
struct altEvent *event = setupNdr2AltEvent();
struct dyString *error = newDyString(128);
double expression = 0;
boolean result = TRUE;
useMaxProbeSet = TRUE;
if(altPathProbesExpressed(event, event->altPathList->next, 1, 0, &expression) != FALSE)
    {
    result = FALSE;
    dyStringPrintf(error, "Max probe set calling false positive, ");
    }
if(altPathProbesExpressed(event, event->altPathList, 0, 3, &expression) != TRUE)
    {
    result = FALSE;
    dyStringPrintf(error, "Max probe set calling positive false, ");
    }

useMaxProbeSet = FALSE;
useComboProbes = TRUE;
if(altPathProbesExpressed(event, event->altPathList->next, 1, 0, &expression) != TRUE)
    {
    result = FALSE;
    dyStringPrintf(error, "Combo probes not combining correctly, ");
    }
test->errorMsg = cloneString(error->string);
dyStringFree(&error);
return result;
}

boolean sameDouble(double d1, double d2)
/* Check to see if these are the same doubles to 6 sig digits. */
{
char buff1[128], buff2[128];
safef(buff1, sizeof(buff1), "%.6f", d1);
safef(buff2, sizeof(buff2), "%.6f", d2);
return sameString(buff1, buff2);
}

boolean testExpressionRatio(struct unitTest *test)
/* Test to see if the ratios are calculated properly for expresionRatio() */
{
struct altEvent *event = setupNdr2AltEvent();
struct dyString *error = newDyString(128);
double expression = 0;
boolean result = TRUE;
boolean current = FALSE;
struct altPath *skipPath = event->altPathList;
struct altPath *incPath = event->altPathList->next;
double tmpThresh = presThresh;
presThresh = absThresh;

current = sameDouble(expressionRatio(event, skipPath, incPath, 0), 1);
result &= unitTestFunct(test, current, error, "Averaging");

current = sameDouble(expressionRatio(event, skipPath, incPath, 2), 2);
result &= unitTestFunct(test, current, error, "Low expressed");

current = sameDouble(expressionRatio(event, skipPath, incPath, 3), .5);
result &= unitTestFunct(test, current, error, "Averaging");

presThresh = tmpThresh;
return result;
}

boolean testBrainSpecific(struct unitTest *test)
/* Test to see if ndr2 is brain specific (which it should be). */
{
struct altEvent *event = setupNdr2AltEvent();
boolean result = FALSE;
int **expressed = NULL;
int **notExpressed = NULL;
double **expression = NULL;
struct altPath *altPath = NULL;
int pathCount = slCount(event->altPathList);
int pathIx = 0;
int pathExpCount = 0;
int i = 0;
struct dMatrix *probM = ndr2BedTestMat(TRUE);
struct dMatrix *intenM = ndr2BedTestMat(FALSE);
/* Allocate a 2D array for expression. */
AllocArray(expressed, pathCount);
AllocArray(notExpressed, pathCount);
AllocArray(expression, pathCount);
for(i = 0; i < pathCount; i++)
    {
    AllocArray(expressed[i], probM->colCount);
    AllocArray(notExpressed[i], probM->colCount);
    AllocArray(expression[i], intenM->colCount);
    }
for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
    {
    if(altPathExpressed(event, altPath, intenM, probM,
			expressed, notExpressed, expression, pathIx))
	pathExpCount++;
    pathIx++;
    }
result = brainSpecific(event, altPath, 1, expressed, notExpressed, 2, probM);
if(result != TRUE)
    test->errorMsg = cloneString("Not showing brain expressed.");
/* Cleanup some memory. */
for(i = 0; i < pathCount; i++)
    {
    freez(&expressed[i]);
    freez(&notExpressed[i]);
    freez(&expression[i]);
    }
freez(&notExpressed);
freez(&expressed);
freez(&expression);
return result;
}

void initTests()
{
struct unitTest *test = NULL;

/* Testing stat routines. */
AllocVar(test);
test->test = testCdfChiSqP;
test->description = "GSL ChiSq function";
slAddHead(&tests, test);

/* Testing motif matching. */
AllocVar(test);
test->test = testStringOccurance;
test->description = "Motif searching";
slAddHead(&tests, test);

/* Testing finding blocks on paths. */
AllocVar(test);
test->test = testPathContains;
test->description = "Match introns and exon blocks to paths";
slAddHead(&tests, test);

/* Testing finding blocks on paths. */
AllocVar(test);
test->test = testPathContainsBed;
test->description = "Match beds to paths";
slAddHead(&tests, test);

/* Testing finding blocks on paths. */
AllocVar(test);
test->test = testAltPathProbesExpressed;
test->description = "Call paths as expressed or not.";
slAddHead(&tests, test);

/* Testing expression ratio generation. */
AllocVar(test);
test->test = testExpressionRatio;
test->description = "Generate expression ratios.";
slAddHead(&tests, test);

/* Testing brain specificity. */
AllocVar(test);
test->test = testBrainSpecific;
test->description = "Brain Specific detection.";
slAddHead(&tests, test);

slReverse(&tests);
}

void runTests()
{
struct unitTest *test, *testNext, *passed = NULL, *failed = NULL;
boolean passedAll = TRUE;
int passCount = 0, *failCount = 0;
for(test = tests; test != NULL; test = testNext)
    {
    testNext = test->next;
    if(test->test(test) != FALSE)
	{
	passCount++;
	slAddHead(&passed, test);
	}
    else
	{
	slAddHead(&failed, test);
	passedAll = FALSE;
	failCount++;
	}
    }
if(passedAll)
    {
    fprintf(stdout, "Passed all %d tests.\n", passCount);
    exit(0);
    }
else
    {
    fprintf(stdout, "Failed tests:\n");
    for(test = failed; test != NULL; test = test->next)
	{
	fprintf(stdout, "%-40s\tFAILED\t%s\n", test->description,
		test->errorMsg == NULL ? "" : test->errorMsg);
	}
    exit(1);
    }
}

void initCassetteBedsOut()
{
struct dyString *buff = NULL;
char *prefix = optionVal("cassetteBeds", NULL);
assert(prefix);
buff = newDyString(strlen(prefix)+20);

dyStringClear(buff);
dyStringPrintf(buff, "%s.altExpressed.bed", prefix);
altCassetteBedOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.expressed.bed", prefix);
expressedCassetteBedOut = mustOpen(buff->string, "w");

dyStringClear(buff);
dyStringPrintf(buff, "%s.notExpressed.bed", prefix);
notExpressedCassetteBedOut = mustOpen(buff->string, "w");
}

void initNotHumanCassettes()
/* Load up cassettes that we think are notHuman. */
{
char *file = optionVal("notHumanCassettes", NULL);
struct bed *bed = NULL, *bedList = NULL;
char buff[256];
notHumanHash = newHash(10);
assert(file);
bedList = bedLoadAll(file);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    safef(buff, sizeof(buff), "%s:%d-%d:%s", bed->chrom, bed->chromStart,bed->chromEnd, bed->strand);
    hashAdd(notHumanHash, buff, bed);
    }
}

void initConservedCassettes()
/* Load up cassettes that we think are conserved. */
{
char *file = optionVal("conservedCassettes", NULL);
struct bed *bed = NULL, *bedList = NULL;
char buff[256];
conservedHash = newHash(10);
assert(file);
bedList = bedLoadAll(file);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    safef(buff, sizeof(buff), "%s:%d-%d:%s", bed->chrom, bed->chromStart,bed->chromEnd, bed->strand);
    hashAdd(conservedHash, buff, bed);
    }
}

void initConstMotifCounts()
/* Initialize file for reporting motif counts in constitutive exons. */
{
int i = 0;
constMotifCounts = mustOpen("constitutiveMotifCounts.tab", "w");
/* Do the first n-1 with tabs. */
for(i=0; i<bindSiteCount-1; i++)
    {
    fprintf(constMotifCounts, "%s-up\t%s-down\t",
	    bindSiteArray[i]->rnaBinder, bindSiteArray[i]->rnaBinder);
    }
/* Last one with newline. */
fprintf(constMotifCounts, "%s-up\t%s-down\n",
	bindSiteArray[i]->rnaBinder, bindSiteArray[i]->rnaBinder);
}

void setOptions()
/* Set up some options. */
{
char *selectedPValues = optionVal("selectedPValues", NULL);
if(optionExists("doTests"))
    {
    initTests();
    runTests();
    }
presThresh = optionFloat("presThresh", .9);
pvalThresh = optionFloat("pValThresh", .001);
tissueExpThresh = optionInt("tissueExpThresh", 1);
otherTissueExpThres = optionInt("otherTissueExpThres", 1);
useMaxProbeSet = optionExists("useMaxProbeSet");
db = optionVal("db", NULL);
browserName = optionVal("browser", "hgwdev-sugnet.cse.ucsc.edu");
useComboProbes = optionExists("combinePSets");
useExonBedsToo = optionExists("useExonBeds");
skipMotifControls = optionExists("skipMotifControls");


newDb = optionVal("newDb", NULL);
if(optionExists("newDb"))
 initLiftOver();

if(optionExists("psToRefGene"))
    initPsToRefGene();

if(optionExists("psToKnownGene"))
    initPsToKnownGene();

if(optionExists("phastConsScores"))
    initPhastConsScores();

if(optionExists("splicePVals"))
    initSplicePVals();

if(optionExists("cassetteBeds"))
    initCassetteBedsOut();

if(optionExists("conservedCassettes"))
   initConservedCassettes();

if(optionExists("notHumanCassettes"))
   initNotHumanCassettes();

if(optionExists("controlStats"))
    {
    controlValuesVm = newValuesMat(1000, 15);
    controlValuesOut = mustOpen(optionVal("controlStats", NULL), "w");
    }

if(optionExists("sortScore"))
    {
    initSortScore();
    }

if(selectedPValues != NULL)
    {
    char *selectedPValFile = optionVal("selectedPValFile", NULL);
    char buff[2048];

    if(selectedPValFile == NULL)
	errAbort("Must specify a selectedPValFile when enabling selectedPValues");
    readSelectedList(selectedPValues);
    pathProbabilitiesOut = mustOpen(selectedPValFile, "w");
    safef(buff, sizeof(buff), "%s.bed", selectedPValFile);
    pathProbabilitiesBedOut = mustOpen(buff, "w");
    safef(buff, sizeof(buff), "%s.intensity", selectedPValFile);
    pathExpressionOut = mustOpen(buff, "w");
    }

if(useMaxProbeSet)
    warn("Using max value from all probe sets in a path.");
/* Set up the datbase. */
db = optionVal("db", NULL);
if(db != NULL)
    hSetDb(db);
else
    errAbort("Must specify database.");
initBindSiteList();

if(!skipMotifControls)
    initConstMotifCounts();
if(optionVal("brainSpecific", NULL) != NULL)
    initBrainSpecific();
}

int main(int argc, char *argv[])
/* Everybody's favorite function... */
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
setOptions();
altProbes();
return 0;
}
