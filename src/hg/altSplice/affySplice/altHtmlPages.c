/* altHtmlPages.c - Program to create a web based browser
   of altsplice predicitions. */

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
#include "altSpliceSite.h"
#include "hdb.h"
#include "dnaseq.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics_double.h>

struct junctSet 
/* A set of beds with a common start or end. */
{
    struct junctSet *next;   /* Next in list. */
    char *chrom;                /* Chromosome. */
    int chromStart;             /* Smallest start. */
    int chromEnd;               /* Largest end. */
    char *name;                 /* Name of junction. */
    int junctCount;             /* Number of junctions in set. */
    char strand[2];             /* + or - depending on which strand. */
    int genePSetCount;          /* Number of gene probe sets. */
    char **genePSets;           /* Gene probe set name. */
    char *hName;                /* Human name for locus or affy id if not present. */
    int maxJunctCount;          /* Maximum size of bedArray. */
    char **junctPSets;          /* Beds in the cluster. */
    int junctDupCount;          /* Number of redundant junctions in set. */
    int maxJunctDupCount;       /* Maximum size of bedDupArray. */
    char **dupJunctPSets;       /* Redundant beds in the cluster. */
    int junctUsedCount;         /* Number of juction sets used. */
    char **junctUsed;           /* Names of actual junction probe sets used. */
    boolean cassette;           /* Is cassette exon set? */
    char *exonPsName;           /* Cassette exon probe set if available. */
    double **junctProbs;        /* Matrix of junction probabilities, rows are junctUsed. */ 
    double **geneProbs;         /* Matrix of gene set probabilities, rows are genePSets. */
    double *exonPsProbs;        /* Exon probe set probabilities if cassette. */
    double **junctIntens;       /* Matrix of junction intensities, rows are junctUsed. */ 
    double **geneIntens;        /* Matrix of gene set intensities, rows are genePSets. */
    double *exonPsIntens;       /* Exon probe set intensities if cassette. */
    boolean expressed;          /* TRUE if one form of this junction set is expressed. */
    boolean altExpressed;       /* TRUE if more than one form of this junction set is expressed. */
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
    int spliceType;             /* Type of splicing event. */
};

struct resultM 
/* Wrapper for a matrix of results. */
{
    struct hash *nameIndex;  /* Hash with integer index for each row name. */
    int colCount;            /* Number of columns in matrix. */
    char **colNames;         /* Column Names for matrix. */
    int rowCount;            /* Number of rows in matrix. */
    char **rowNames;         /* Row names for matrix. */
    double **matrix;         /* The actual data for the resultM. */
};

struct altCounts
{
    char *gene; /*Name of gene. */
    int count; /* Number of alternative sites possible. */
    int jsExpressed; /* Number of alternative sites where one isoform expressed. */
    int jsAltExpressed; /* Number of alternative sites that are alternatively expressed. */
};


static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"junctFile", OPTION_STRING},
    {"probFile", OPTION_STRING},
    {"intensityFile", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"sortByExonCorr", OPTION_BOOLEAN},
    {"sortByExonPercent", OPTION_BOOLEAN},
    {"htmlPrefix", OPTION_STRING},
    {"hybeFile", OPTION_STRING},
    {"presThresh", OPTION_FLOAT},
    {"absThresh", OPTION_FLOAT},
    {"strictDisagree", OPTION_BOOLEAN},
    {"exonStats", OPTION_STRING},
    {"spreadSheet", OPTION_STRING},
    {"doSjRatios", OPTION_BOOLEAN},
    {"log2Ratio", OPTION_BOOLEAN},
    {"ratioFile", OPTION_STRING},
    {"junctPsMap", OPTION_STRING},
    {"agxFile", OPTION_STRING},
    {"db", OPTION_STRING},
    {"spliceTypes", OPTION_STRING},
    {"tissueSpecific", OPTION_STRING},
    {"outputDna", OPTION_STRING},
    {"cassetteBed", OPTION_STRING},
    {"geneStats", OPTION_STRING},
    {"tissueExpThresh", OPTION_INT},
    {"geneJsStats", OPTION_STRING},
    {"brainSpecific", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File containing junctionSets from plotAndCountAltsMultGs.R log.",
    "File containing summarized probability of expression for each probe set.",
    "File containing expression values for each probe set.",
    "File containing beds for each probe set.",
    "Flag to sort by exon correlation when present.",
    "Flag to sort by exon percent agreement with splice junction when present.",
    "Prefix for html files, i.e. <prefix>lists.html and <prefix>frame.html",
    "File giving names and ids for hybridizations.",
    "Optional threshold at which to call expressed.",
    "Optional threshold at which to call not expressed.",
    "Call disagreements only if below absense threshold instead of below present thresh.",
    "File to output confirming exon stats to.",
    "File to output spreadsheet format picks to.",
    "[optional] Calculate ratios of sj to other sj in same junction set.",
    "[optional] Transform ratios with log base 2.",
    "[optional] File to store sjRatios in.",
    "[optional] Print out the probe sets in a junction set and quit.",
    "[optional] File with graphs to determine types.",
    "[optional] Database that graphs are from.",
    "[optional] Try to determine types of splicing in junction sets.",
    "[optional] Output tissues specific isoforms to file specified.",
    "[optional] Output dna associated with a junction to this file.",
    "[optional] Output splice sites for cassette exons.",
    "[optional] Output stats about genes, junctions expressed, junctions alt to file name.",
    "[optional] Number of tissues that must express a probe set before considred expressed.",
    "[optional] Print out junction sets per gene stats.",
    "[optional] Output brain specific isoforms to file specified.",
};


int tissueExpThresh = 0;   /* Number of tissues that must express probe set. */
double presThresh = 0;     /* Probability above which we consider
			    * something expressed. */
double absThresh = 0;      /* Probability below which we consider
			    * something not expressed. */
double disagreeThresh = 0; /* Probability below which we consider
			    * something to disagree. */
FILE *exonPsStats = NULL;  /* File to output exon probe set 
			      confirmation stats into. */
FILE *geneCounts = NULL;   /* Print out the gene junction counts. */
boolean doJunctionTypes = FALSE;  /* Should we try to determine what type of splice type is out there? */ 
FILE *bedJunctTypeOut =   NULL;   /* File to write out bed types to. */
struct hash *junctBedHash = NULL;
FILE *cassetteBedOut = NULL;      /* File to write cassette bed records to. */
/* Counts of different alternative splicing 
   events. */
int alt3Count = 0;
int alt3SoftCount = 0;
int alt5Count = 0;
int alt5SoftCount = 0;
int altCassetteCount = 0;
int otherCount = 0;

/* Counts of different alternative splicing 
   events that are expressed. */
int alt3ExpCount = 0;
int alt3SoftExpCount = 0;
int alt5ExpCount = 0;
int alt5SoftExpCount = 0;
int altCassetteExpCount = 0;
int otherExpCount = 0;

/* Counts of different alternative splicing 
   events that are expressed. */
int alt3ExpAltCount = 0;
int alt3SoftExpAltCount = 0;
int alt5ExpAltCount = 0;
int alt5SoftExpAltCount = 0;
int altCassetteExpAltCount = 0;
int otherExpAltCount = 0;

int noDna = 0;
int withDna = 0;
void usage()
/** Print usage and quit. */
{
int i=0;
warn("altHtmlPages - Program to create a web based browser\n"
     "of altsplice predicitions. Now extended to do some intitial\n"
     "analysis as well.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   ");
}

void printJunctSetProbes(struct junctSet *jsList)
/* Print out a mapping from junctionSets to probe sets. */
{
char *fileName = optionVal("junctPsMap",NULL);
FILE *out = NULL;
int i = 0;
struct junctSet *js = NULL;

assert(fileName);
out = mustOpen(fileName, "w");
for(js = jsList; js != NULL; js = js->next)
    {
    for(i = 0; i < js->maxJunctCount; i++)
	fprintf(out, "%s\t%s\n", js->name, js->junctPSets[i]);
    for(i = 0; i < js->junctDupCount; i++)
	fprintf(out, "%s\t%s\n", js->name, js->dupJunctPSets[i]);
    for(i = 0; i < js->genePSetCount; i++)
	fprintf(out, "%s\t%s\n", js->name, js->genePSets[i]);
    }
carefulClose(&out);
}

struct resultM *readResultMatrix(char *fileName)
/* Read in an R style result table. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct resultM *rM = NULL;
char **words = NULL;
int colCount=0, i=0;
char *line = NULL;
double **M = NULL;
int rowCount =0;
int rowMax = 1000;
char **rowNames = NULL;
char **colNames = NULL;
struct hash *iHash = newHash(12);
char buff[256];
char *tmp;
/* Get the headers. */
lineFileNextReal(lf, &line);
while(line[0] == '\t')
    line++;
colCount = chopString(line, "\t", NULL, 0);
AllocArray(colNames, colCount);
AllocArray(words, colCount+1);
AllocArray(rowNames, rowMax);
AllocArray(M, rowMax);
tmp = cloneString(line);
chopByChar(tmp, '\t', colNames, colCount);
while(lineFileNextRow(lf, words, colCount+1))
    {
    if(rowCount+1 >=rowMax)
	{
	ExpandArray(rowNames, rowMax, 2*rowMax);
	ExpandArray(M, rowMax, 2*rowMax);
	rowMax = rowMax*2;
	}
    safef(buff, sizeof(buff), "%s", words[0]);
    tmp=strchr(buff,':');
    if(tmp != NULL)
	{
	assert(tmp);
	tmp=strchr(tmp+1,':');
	assert(tmp);
	tmp[0]='\0';
	}
    hashAddInt(iHash, buff, rowCount);
    rowNames[rowCount] = cloneString(words[0]);
    AllocArray(M[rowCount], colCount);
    for(i=1; i<=colCount; i++) /* Starting at 1 here as name is 0. */
	{
	M[rowCount][i-1] = atof(words[i]);
	}
    rowCount++;
    }
AllocVar(rM);
rM->nameIndex = iHash;
rM->colCount = colCount;
rM->colNames = colNames;
rM->rowCount = rowCount;
rM->rowNames = rowNames;
rM->matrix = M;
return rM;
}

struct junctSet *junctSetLoad(char *row[], int colCount)
/* Load up a junct set. */
{
struct junctSet *js = NULL;
int index = 1;
int count = 0;
AllocVar(js);
if(colCount > 14)
    index = 1;
else
    index = 0;
js->chrom = cloneString(row[index++]);
js->chromStart = sqlUnsigned(row[index++]);
js->chromEnd = sqlUnsigned(row[index++]);
js->name = cloneString(row[index++]);
js->junctCount = js->maxJunctDupCount = sqlUnsigned(row[index++]);
js->strand[0] = row[index++][0];
js->genePSetCount = sqlUnsigned(row[index++]);
sqlStringDynamicArray(row[index++], &js->genePSets, &count);
js->hName = cloneString(row[index++]);
sqlStringDynamicArray(row[index++], &js->junctPSets, &js->maxJunctCount);
js->junctDupCount = sqlUnsigned(row[index++]);
sqlStringDynamicArray(row[index++], &js->dupJunctPSets, &count);
js->cassette = atoi(row[index++]);
js->exonPsName = cloneString(row[index++]);
if(colCount != 14) /* If the junctions used fields not present fill them in later. */
    {
    js->junctUsedCount = sqlUnsigned(row[index++]);
    sqlStringDynamicArray(row[index++], &js->junctUsed, &count);
    }
js->spliceType = altOther;
return js;
}

struct junctSet *junctSetLoadAll(char *fileName)
/* Load all of the junct sets out of a fileName. */
{
struct junctSet *list = NULL, *el = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int numFields = 0;
char *line = NULL;
char **row = NULL;
lineFileNextReal(lf, &line);
numFields = chopByWhite(line, NULL, 0);
assert(numFields);
lineFileReuse(lf);
AllocArray(row, numFields);
while(lineFileNextRowTab(lf, row, numFields))
    {
    el = junctSetLoad(row, numFields);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&lf);
return list;
}

void fillInProbs(struct junctSet *jsList, struct resultM *probM)
/* Fill in the probability of expression for each junctSet's junction
   and gene probe sets. */
{
struct junctSet *js = NULL;
int i = 0, j = 0;
for(js = jsList; js != NULL; js = js->next)
    {
    /* Allocate junction probability matrix arrays. */
    AllocArray(js->junctProbs, js->junctUsedCount);
    for(i = 0; i < js->junctUsedCount; i++)
	AllocArray(js->junctProbs[i], probM->colCount);
    
    /* Fill in the values. */
    for(i = 0; i < js->junctUsedCount; i++)
	{
	int row = hashIntValDefault(probM->nameIndex, js->junctUsed[i], -1);
	if(row != -1)
	    {
	    for(j = 0; j < probM->colCount; j++) 
		js->junctProbs[i][j] = probM->matrix[row][j];
	    }
	else
	    {
	    for(j = 0; j < probM->colCount; j++) 
		js->junctProbs[i][j] = -1;
	    }
	}

    /* Allocate memory for gene sets. */
    AllocArray(js->geneProbs, js->genePSetCount);
    for(i = 0; i < js->genePSetCount; i++)
	AllocArray(js->geneProbs[i], probM->colCount);

    /* Fill in the values. */
    for(i = 0; i < js->genePSetCount; i++)
	{
	int row = hashIntValDefault(probM->nameIndex, js->genePSets[i], -1);
	if(row != -1)
	    {
	    for(j = 0; j < probM->colCount; j++) 
		js->geneProbs[i][j] = probM->matrix[row][j];
	    }
	else
	    {
	    for(j = 0; j < probM->colCount; j++) 
		js->geneProbs[i][j] = -1;
	    }
	}

    /* Allocate the cassette exon probabilities if present. */
    if(js->cassette && differentWord(js->exonPsName, "NA"))
	{
	int row = hashIntValDefault(probM->nameIndex, js->exonPsName, -1);
	if(row != -1)
	    {
	    AllocArray(js->exonPsProbs, probM->colCount);
	    for(j = 0; j < probM->colCount; j++)
		js->exonPsProbs[j] = probM->matrix[row][j];
	    }
	}
    }
}

void fillInIntens(struct junctSet *jsList, struct resultM *intenM)
/* Fill in the intenability of expression for each junctSet's junction
   and gene probe sets. */
{
struct junctSet *js = NULL;
int i = 0, j = 0;
for(js = jsList; js != NULL; js = js->next)
    {
    /* Allocate junction intenability matrix arrays. */
    AllocArray(js->junctIntens, js->junctUsedCount);
    for(i = 0; i < js->junctUsedCount; i++)
	AllocArray(js->junctIntens[i], intenM->colCount);
    
    /* Fill in the values. */
    for(i = 0; i < js->junctUsedCount; i++)
	{
	int row = hashIntValDefault(intenM->nameIndex, js->junctUsed[i], -1);
	if(row != -1)
	    {
	    for(j = 0; j < intenM->colCount; j++) 
		js->junctIntens[i][j] = intenM->matrix[row][j];
	    }
	else
	    {
	    for(j = 0; j < intenM->colCount; j++) 
		js->junctIntens[i][j] = -1;
	    }
	}

    /* Allocate memory for gene sets. */
    AllocArray(js->geneIntens, js->genePSetCount);
    for(i = 0; i < js->genePSetCount; i++)
	AllocArray(js->geneIntens[i], intenM->colCount);

    /* Fill in the values. */
    for(i = 0; i < js->genePSetCount; i++)
	{
	int row = hashIntValDefault(intenM->nameIndex, js->genePSets[i], -1);
	if(row != -1)
	    {
	    for(j = 0; j < intenM->colCount; j++) 
		js->geneIntens[i][j] = intenM->matrix[row][j];
	    }
	else
	    {
	    for(j = 0; j < intenM->colCount; j++) 
		js->geneIntens[i][j] = -1;
	    }
	}

    /* Allocate the cassette exon intenabilities if present. */
    if(js->cassette && differentWord(js->exonPsName, "NA"))
	{
	int row = hashIntValDefault(intenM->nameIndex, js->exonPsName, -1);
	if(row != -1)
	    {
	    AllocArray(js->exonPsIntens, intenM->colCount);
	    for(j = 0; j < intenM->colCount; j++)
		js->exonPsIntens[j] = intenM->matrix[row][j];
	    }
	}
    }
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

double calcMinCorrelation(struct junctSet *js, int *expressed, int tissueCount, int colCount)
/* Loop through all combinations of junction sets that are expressed
   and calculate the minimum correlation between the two in tissues
   if they are expressed in at least 1. */
{
double minCorr = 2; /* Correlation always <= 1, 2 is a good max. */
double corr = 0;
int i = 0, j = 0;
for(i = 0; i < js->junctUsedCount; i++)
    {
    /* If there isn't any expression for this junction don't bother. */
    if(expressed[i] == 0)
	continue;
    for(j = 0; j < js->junctUsedCount; j++)
	{
	/* Corralation when i == j is 1, don't bother. */
	if(i == j)
	    continue;

	/* If there isn't any expression for this junction don't bother. */
	if(expressed[j] == 0)
	    continue;

	/* Calculate correlation. */
	corr = correlation(js->junctIntens[i], js->junctIntens[j], colCount);
	if(minCorr > corr)
	    minCorr = corr;
	}
    }
return minCorr;
}

int includeJsIx(struct junctSet *js, struct hash *bedHash)
/* Return the index of the include junction for a cassette exon. */
{
int includeIx = 0;
struct bed *bed1 = NULL, *bed2 = NULL;

assert(js->cassette);
if(js->junctUsedCount < 2)
    return 0;

bed1 = hashMustFindVal(bedHash, js->junctUsed[0]);
bed2 = hashMustFindVal(bedHash, js->junctUsed[1]);
if(bed1->chromEnd - bed1->chromStart > bed2->chromEnd - bed2->chromStart)
    includeIx = 1;
else
    includeIx = 0;
return includeIx;
}

int skipJsIx(struct junctSet *js, struct hash *bedHash)
/* Return the index of the skip junction for a cassette exon. */
{
int skipIx = 0;
struct bed *bed1 = NULL, *bed2 = NULL;

assert(js->cassette);
if(js->junctUsedCount < 2)
    return 0;

bed1 = hashMustFindVal(bedHash, js->junctUsed[0]);
bed2 = hashMustFindVal(bedHash, js->junctUsed[1]);
if(bed1->chromEnd - bed1->chromStart > bed2->chromEnd - bed2->chromStart)
    skipIx = 0;
else
    skipIx = 1;
return skipIx;
}

boolean geneExpressed(struct junctSet *js, int colIx)
/* Is the gene at junctIx expressed above the 
   threshold in colIx? */
{
int i = 0;
for(i = 0; i < js->genePSetCount; i++)
    {
    if(js->geneProbs[i][colIx] >= presThresh)
	return TRUE;
    }
return FALSE;
}

boolean junctionExpressed(struct junctSet *js, int colIx, int junctIx)
/* Is the junction at junctIx expressed above the 
   thresnhold in colIx? */
{
boolean geneExp = FALSE;
boolean junctExp = FALSE;
int i = 0;
geneExp = geneExpressed(js, colIx);
if(geneExp && js->junctProbs[junctIx][colIx] >= presThresh)
    junctExp = TRUE;
return junctExp;
}

boolean exonExpressed(struct junctSet *js, int colIx)
/* Is the junction at junctIx expressed above the 
   threshold in colIx? */
{
boolean geneExp = FALSE;
boolean exonExp = FALSE;
int i = 0;
for(i = 0; i < js->genePSetCount; i++)
    {
    if(js->geneProbs[i][colIx] >= presThresh)
	geneExp = TRUE;
    }
if(geneExp && js->exonPsProbs[colIx] >= presThresh)
    exonExp = TRUE;
return exonExp;
}


double calcExonPercent(struct junctSet *js, struct hash *bedHash, struct resultM *probM)
/* Calculate how often the exon probe set is called expressed
   when probe set is. */
{
int includeIx = includeJsIx(js, bedHash);
int both = 0;
int jsOnly = 0;
double percent = 0;
int i = 0;
for(i = 0; i < probM->colCount; i++)
    {
    if(junctionExpressed(js, i, includeIx) &&
       js->exonPsProbs[i] >= presThresh)
	{
	both++;
	jsOnly++;
	js->exonAgree++;
	}
    else if(junctionExpressed(js, i, includeIx) &&
	    js->exonPsProbs[i] < disagreeThresh)
	{
	js->exonDisagree++;
	jsOnly++;
	}
    }
if(jsOnly > 0)
    percent = (double)both/jsOnly;
else 
    percent = -1;
return percent;
}

double calcSjPercent(struct junctSet *js, struct hash *bedHash, struct resultM *probM)
/* Calculate how often the sj probe set is called expressed
   when exon probe set is. */
{
int includeIx = includeJsIx(js, bedHash);
int both = 0;
int exonOnly = 0;
double percent = 0;
int i = 0;
for(i = 0; i < probM->colCount; i++)
    {
    if(junctionExpressed(js, i, includeIx) &&
       js->exonPsProbs[i] >= presThresh)
	{
	both++;
	exonOnly++;
	js->sjAgree++;
	}
    else if(exonExpressed(js, i) &&
	    js->junctProbs[includeIx][i] < disagreeThresh)
	{
	exonOnly++;
	js->sjDisagree++;
	}
    }
if(exonOnly > 0)
    percent = (double)both/exonOnly;
else 
    percent = -1;
return percent;
}

double calcExonSjCorrelation(struct junctSet *js, struct hash *bedHash, struct resultM *intenM)
/* Calcualate the correltation between the exon probe
   set and the appropriate splice junction set. */
{
double corr = -2;
int includeIx = includeJsIx(js, bedHash);
corr = correlation(js->junctIntens[includeIx], js->exonPsIntens, intenM->colCount);
return corr;
}

double calcExonSjSkipCorrelation(struct junctSet *js, struct hash *bedHash, struct resultM *intenM)
/* Calcualate the correltation between the exon probe
   set and the appropriate skip splice junction set. */
{
double corr = -2;
int skipIx = skipJsIx(js, bedHash);
corr = correlation(js->junctIntens[skipIx], js->exonPsIntens, intenM->colCount);
return corr;
}

double calcExonSjProbCorrelation(struct junctSet *js, struct hash *bedHash, struct resultM *probM)
/* Calcualate the correltation between the exon probe
   set and the appropriate splice junction set. */
{
double corr = -2;
int includeIx = includeJsIx(js, bedHash);
corr = correlation(js->junctProbs[includeIx], js->exonPsProbs, probM->colCount);
return corr;
}

double calcExonSjSkipProbCorrelation(struct junctSet *js, struct hash *bedHash, struct resultM *probM)
/* Calcualate the correltation between the exon probe
   set and the appropriate splice junction set. */
{
double corr = -2;
int skipIx = skipJsIx(js, bedHash);
corr = correlation(js->junctProbs[skipIx], js->exonPsProbs, probM->colCount);
return corr;
}

void calcExonCorrelation(struct junctSet *jsList, struct hash *bedHash, 
			 struct resultM *intenM, struct resultM *probM)
/* Calculate exon correlation if available. */
{
struct junctSet *js = NULL;
for(js = jsList; js != NULL; js = js->next)
    {
    if(js->junctUsedCount == 2 &&
       js->cassette == 1 && differentWord(js->exonPsName, "NA") && 
       js->exonPsIntens != NULL && js->exonPsProbs != NULL) 
	{
	js->exonCorr = calcExonSjCorrelation(js, bedHash, intenM);
	js->exonSkipCorr = calcExonSjSkipCorrelation(js, bedHash, intenM);
	js->probCorr = calcExonSjProbCorrelation(js, bedHash, probM);
	js->probSkipCorr = calcExonSjSkipProbCorrelation(js, bedHash, probM);
	js->exonSjPercent = calcExonPercent(js, bedHash, probM);
	js->sjExonPercent = calcSjPercent(js, bedHash, probM);
	if(js->altExpressed && exonPsStats)
	    fprintf(exonPsStats, "%s\t%.2f\t%.2f\t%.2f\t%d\t%d\t%d\t%d\t%.2f\t%.2f\t%.2f\n", 
		    js->exonPsName, js->exonCorr, js->exonSjPercent, js->sjExonPercent, 
		    js->exonAgree, js->exonDisagree, js->sjAgree, js->sjDisagree, js->probCorr,
		    js->exonSkipCorr, js->probSkipCorr);
	}
    }
}


void countExpSpliceType(int type)
/* Record the counts of different alt-splice types. */
{
switch(type)
    {
    case alt3Prime:
	alt3ExpCount++;
	break;
    case alt5Prime:
	alt5ExpCount++;
	break;
    case alt3PrimeSoft:
	alt3SoftExpCount++;
	break;
    case alt5PrimeSoft:
	alt5SoftExpCount++;
	break;
    case altCassette:
	altCassetteExpCount++;
	break;
    case altOther:
	otherExpCount++;
	break;
    deafult:
	warn("Don't recognize type: %d", type);
    }
}

void countExpAltSpliceType(int type)
/* Record the counts of different alt-splice types. */
{
switch(type)
    {
    case alt3Prime:
	alt3ExpAltCount++;
	break;
    case alt5Prime:
	alt5ExpAltCount++;
	break;
    case alt3PrimeSoft:
	alt3SoftExpAltCount++;
	break;
    case alt5PrimeSoft:
	alt5SoftExpAltCount++;
	break;
    case altCassette:
	altCassetteExpAltCount++;
	break;
    case altOther:
	otherExpAltCount++;
	break;
    deafult:
	warn("Don't recognize type: %d", type);
    }
}

void logOneJunctSet(struct hash *geneHash, struct junctSet *js)
/* Log that a gene is expressed. */
{
struct altCounts *ac = NULL;
ac = hashFindVal(geneHash, js->genePSets[0]);
if(ac == NULL)
    {
    AllocVar(ac);
    ac->gene = js->genePSets[0];
    ac->count++;
    hashAdd(geneHash, js->genePSets[0], ac);
    }
else
    ac->count++;
}


void logOneExpressed(struct hash *geneHash, struct junctSet *js)
/* Log an expressed event. */
{
struct altCounts *ac = NULL;
ac = hashMustFindVal(geneHash, js->genePSets[0]);
ac->jsExpressed++;
}

void logOneAltExpressed(struct hash *geneHash, struct junctSet *js)
/* Log an AltExpressed event. */
{
struct altCounts *ac = NULL;
ac = hashMustFindVal(geneHash, js->genePSets[0]);
ac->jsAltExpressed++;
}

void printAltCounts(void *val)
/* Print out the junction sets. */
{
struct altCounts *ac = (struct altCounts *)val;
if(geneCounts != NULL)
    fprintf(geneCounts, "%s\t%d\t%d\t%d\n", ac->gene, ac->count,
	    ac->jsExpressed, ac->jsAltExpressed);
}

int calcExpressed(struct junctSet *jsList, struct resultM *probMat)
/* Loop through and calculate expression and score where score is correlation. */
{
struct junctSet *js = NULL;
int i = 0, j = 0, k = 0;
double minCor = 2; /* Correlation is always between -1 and 1, 2 is
		    * good max. */
struct hash *geneHash = newHash(10);
char *geneJsOutName = optionVal("geneJsStats", NULL);
char *geneOutName = optionVal("geneStats", NULL);
FILE *geneOut = NULL;
double *X = NULL;
double *Y = NULL;
int elementCount;
int colCount = probMat->colCount;
int rowCount = probMat->rowCount;     
int colIx = 0, junctIx = 0;           /* Column and Junction indexes. */
int junctOneExp = 0, junctTwoExp = 0; /* What are the counts of
				       * expressed and alts. */
tissueExpThresh = optionInt("tissueExpThresh",1);
if(geneOutName != NULL)
    geneOut = mustOpen(geneOutName, "w");
if(geneJsOutName != NULL)
    geneCounts = mustOpen(geneJsOutName, "w");
for(js = jsList; js != NULL; js = js->next)
    {
    int **expressed = NULL;
    int totalTissues = 0;
    int junctExpressed = 0;
    int geneExpCount = 0;
    int altExpTissues = 0;
    int jsExpCount = 0;
    AllocArray(expressed, js->junctUsedCount);
    for(i = 0; i < js->junctUsedCount; i++)
	AllocArray(expressed[i], colCount);
    
    /* Loop through and see how many junctions
       are actually expressed and in how many tissues. */
    for(colIx = 0; colIx < colCount; colIx++)
	{
	int anyExpressed = 0;
	int jsExpressed = 0;
	if(geneExpressed(js, colIx))
	    geneExpCount++;
	for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	    {
	    if(junctionExpressed(js, colIx, junctIx) == TRUE)
		{
		expressed[junctIx][colIx]++;
		if(expressed[junctIx][colIx] >= tissueExpThresh)
		    anyExpressed++;
		}
	    if(js->junctProbs[junctIx][colIx] >= presThresh)
		jsExpressed++;
	    }
	if(anyExpressed > 0)
	    totalTissues++;
	if(jsExpressed >= tissueExpThresh)
	    jsExpCount++;

	if(jsExpressed >= tissueExpThresh * 2)
	    altExpTissues++;
	}
    
    /* Set number expressed. */
    for(i = 0; i < js->junctUsedCount; i++)
	{
	for(j = 0; j < colCount; j++)
	    {
	    int numTissueExpressed = 0;
	    if(expressed[i][j])
		{
		/* If this junction has been expressed more than
		   the threshold stop counting and move on to the 
		   next on. */
		if(++numTissueExpressed >= tissueExpThresh)
		    {
		    junctExpressed++;
		    break;
		    }
		}
	    }
	}
    
    /* Record that there is a junction set for this gene. */
    logOneJunctSet(geneHash, js);

    /* If one tissue expressed set is expressed. */
    if(junctExpressed >= 1)
	{
	js->expressed = TRUE;
	logOneExpressed(geneHash, js);
	junctOneExp++;
	}
    
    /* If two tissues are expressed set is alternative expressed. */
    if(junctExpressed >= 2)
	{
	js->altExpressed = TRUE;
	logOneAltExpressed(geneHash, js);
	junctTwoExp++;
	}
    if(js->altExpressed == TRUE)
	{
	js->score = calcMinCorrelation(js, expressed[0], totalTissues, colCount);
/*	countExpSpliceType(js->spliceType);*/
	}
    else
	js->score = BIGNUM;
    if(geneOut) 
	{
	char *tmpName = cloneString(js->junctUsed[0]);
	char *tmp = NULL;
	tmp = strchr(tmpName, '@');
	assert(tmp);
	*tmp = '\0';
	fprintf(geneOut, "%s\t%s\t%d\t%d\t%d\n", js->name, tmpName, geneExpCount, jsExpCount, altExpTissues);
	freez(&tmpName);
	}
    for(i = 0; i < js->junctUsedCount; i++)
	freez(&expressed[i]);
    freez(&expressed);
    }
carefulClose(&geneOut);
hashTraverseVals(geneHash, printAltCounts);
freeHashAndVals(&geneHash);
carefulClose(&geneCounts);
warn("%d junctions expressed above p-val %.2f, %d alternative (%.2f%%)",
     junctOneExp, presThresh, junctTwoExp, 100*((double)junctTwoExp/junctOneExp));
}

int junctSetScoreCmp(const void *va, const void *vb)
/* Compare to sort based on score, smallest to largest. */
{
const struct junctSet *a = *((struct junctSet **)va);
const struct junctSet *b = *((struct junctSet **)vb);
if( a->score > b->score)
    return 1;
else if(a->score < b->score)
    return -1;
return 0;	    
}

int junctSetExonCorrCmp(const void *va, const void *vb)
/* Compare to sort based on score, smallest to largest. */
{
const struct junctSet *a = *((struct junctSet **)va);
const struct junctSet *b = *((struct junctSet **)vb);
if( a->exonCorr > b->exonCorr)
    return -1;
else if(a->exonCorr < b->exonCorr)
    return 1;
return 0;	    
}

int junctSetExonPercentCmp(const void *va, const void *vb)
/* Compare to sort based on score, smallest to largest. */
{
const struct junctSet *a = *((struct junctSet **)va);
const struct junctSet *b = *((struct junctSet **)vb);
double dif = a->exonSjPercent > b->exonSjPercent;
if(dif == 0)
    {
    if( a->exonCorr > b->exonCorr)
	return -1;
    else if(a->exonCorr < b->exonCorr)
	return 1;
    }
if( a->exonSjPercent > b->exonSjPercent)
    return -1;
else if(a->exonSjPercent < b->exonSjPercent)
    return 1;
return 0;	    
}

void makePlotLinks(struct junctSet *js, struct dyString *buff)
{
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"./noAltTranStartJunctSet/altPlot/%s.%d-%d.%s.png\">[all]</a> ",
	       js->chrom, js->chromStart, js->chromEnd, js->hName);
dyStringPrintf(buff, "<a target=\"plots\" href=\"./noAltTranStartJunctSet/altPlot.median/%s.%d-%d.%s.png\">[median]</a> ",
	       js->chrom, js->chromStart, js->chromEnd, js->hName);
}

void makeJunctMdbGenericLink(struct junctSet *js, struct dyString *buff, char *name, struct resultM *probM, char *suffix)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.gi.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on&mdbsg.toScale=on%s\">", 
	       "mm2", js->strand[0], js->chrom, js->chromStart, js->chromEnd, 
	       js->chromStart - offSet, js->chromEnd + offSet, "AffyMouseSplice1-02-2004", suffix);
dyStringPrintf(buff, "%s", name);
dyStringPrintf(buff, "</a> ");
}


void makeJunctExpressedTissues(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
struct dyString *dy = newDyString(1048);
char anchor[128];
dyStringClear(buff);
for(i = 0; i < probM->colCount; i++)
    {
    if(js->junctProbs[junctIx][i] >= presThresh && 
       geneExpressed(js, i))
	{
	safef(anchor, sizeof(anchor), "#%s", probM->colNames[i]);
	makeJunctMdbGenericLink(js, dy, probM->colNames[i], probM, anchor);
	dyStringPrintf(buff, "%s, ", dy->string);
	}
    }
dyStringFree(&dy);
}

/* void makeJunctExpressedTissues(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM) */
/* { */
/* int offSet = 100; */
/* int i = 0; */
/* dyStringClear(buff); */
/* for(i = 0; i < probM->colCount; i++) */
/*     { */
/*     if(js->junctProbs[junctIx][i] >= presThresh &&  */
/*        geneExpressed(js, i)) */
/* 	dyStringPrintf(buff, "%s, ", probM->colNames[i]); */
/*     } */
/* } */

void makeJunctExpressedLink(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.gi.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on&mdbsg.toScale=on\">", 
	       "mm2", js->strand[0], js->chrom, js->chromStart, js->chromEnd, 
	       js->chromStart - offSet, js->chromEnd + offSet, "AffyMouseSplice1-02-2004");
for(i = 0; i < probM->colCount; i++)
    {
    if(js->junctProbs[junctIx][i] >= presThresh && 
       geneExpressed(js, i))
	dyStringPrintf(buff, "%s,", probM->colNames[i]);
    }
dyStringPrintf(buff, "</a> ");
}


void makeJunctNotExpressedTissues(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
struct dyString *dy = newDyString(1048);
char anchor[128];
dyStringClear(buff);

for(i = 0; i < probM->colCount; i++)
    {
    if(js->junctProbs[junctIx][i] <= absThresh)
	{
	safef(anchor, sizeof(anchor), "#%s", probM->colNames[i]);
	makeJunctMdbGenericLink(js, dy, probM->colNames[i], probM, anchor);
	dyStringPrintf(buff, "%s, ", dy->string);
	}
    }
dyStringFree(&dy);
}

void makeJunctMdbLink(struct junctSet *js, struct hash *bedHash,
		      struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.gi.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on&mdbsg.toScale=on\">", 
	       "mm2", js->strand[0], js->chrom, js->chromStart, js->chromEnd, 
	       js->chromStart - offSet, js->chromEnd + offSet, "AffyMouseSplice1-02-2004");
dyStringPrintf(buff, "%s", js->junctUsed[junctIx]);
dyStringPrintf(buff, "</a> ");
if(js->cassette == 1 && js->exonPsProbs != NULL) 
    {
    if(junctIx == includeJsIx(js, bedHash))
	{
	dyStringPrintf(buff, "<b> Inc.</b>");
	}
    }
}

void makeGbCoordLink(struct junctSet *js, int chromStart, int chromEnd, struct dyString *buff)
/* Make a link to the genome browser with links highlighted. */
{
int junctIx = 0;
int offSet = 100;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"browser\" href=\"http://hgwdev-sugnet.gi.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&splicesPFt=red&splicesPlt=or&splicesP_name=", js->chrom, chromStart - offSet, chromEnd + offSet);
for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
    dyStringPrintf(buff, "%s ", js->junctUsed[junctIx]);
if(js->exonPsName != NULL && differentWord(js->exonPsName, "NA"))
    dyStringPrintf(buff, "%s ", js->exonPsName);
dyStringPrintf(buff, "\">%s</a>", js->name);
}

void makeGbClusterLink(struct junctSet *js, struct dyString *buff)
{
makeGbCoordLink(js, js->chromStart, js->chromEnd, buff);
}

void makeGbLink(struct junctSet *js, struct dyString *buff)
{
int junctIx = 0;
int offSet = 100;
dyStringClear(buff);
if(js->hName[0] != 'G' || strlen(js->hName) != 8)
    {
    dyStringPrintf(buff, "<a target=\"browser\" href=\"http://hgwdev-sugnet.gi.ucsc.edu/cgi-bin/hgTracks?position=%s&splicesPFt=red&splicesPlt=or&splicesP_name=", js->hName);
    for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	dyStringPrintf(buff, "%s ", js->junctUsed[junctIx]);
    dyStringPrintf(buff, "\">%s</a>", js->hName);
    }
else
    dyStringPrintf(buff, "%s", js->hName);
}

char *spliceTypeName(int t)
/* Return the splice type names. */
{
switch(t) 
    {
    case alt5Prime:
	return "alt5";
    case alt3Prime:
	return "alt3";
    case altCassette:
	return "altCass";
    case altRetInt:
	return "altRetInt";
    case altOther:
	return "altOther";
    case alt3PrimeSoft:
	return "txEnd";
    case alt5PrimeSoft:
	return "txStart";
    default:
	return "unknown";
    }
return NULL;
}

void outputLinks(struct junctSet **jsList, struct hash *bedHash, struct resultM *probM, FILE *out)
/* Output a report for each junction set. */
{
struct junctSet *js = NULL;
int colIx = 0, junctIx = 0;
struct dyString *dy = newDyString(2048);
if(optionExists("sortByExonCorr")) 
    slSort(jsList, junctSetExonCorrCmp);
else if(optionExists("sortByExonPercent"))
    slSort(jsList, junctSetExonPercentCmp);
else
    slSort(jsList, junctSetScoreCmp);
fprintf(out, "<ol>\n");
for(js = *jsList; js != NULL; js = js->next)
    {
    if(js->score <= 1)
	{
	char affyName[256];
	boolean gray = FALSE;
	char *tmp = NULL;
	char *col = NULL;
	fprintf(out, "<li>");
	fprintf(out, "<table bgcolor=\"#000000\" border=0 cellspacing=0 cellpadding=1><tr><td>");
	fprintf(out, "<table bgcolor=\"#FFFFFF\" width=100%%>\n");
	fprintf(out, "<tr><td bgcolor=\"#7896be\">");
	makeGbClusterLink(js, dy);
	fprintf(out, "<b>Id:</b> %s ", dy->string);
	makeGbLink(js, dy);
	fprintf(out, "<b>GeneName:</b> %s ", dy->string);
	makePlotLinks(js, dy);
	fprintf(out, "<b>Plots:</b> %s ", dy->string);
	fprintf(out, "<b>Score:</b> %.2f", js->score);
	fprintf(out, "<b> Type:</b> %s", spliceTypeName(js->spliceType));
	safef(affyName, sizeof(affyName), "%s", js->junctUsed[0]);
	tmp = strchr(affyName, '@');
	assert(tmp);
	*tmp = '\0';
	fprintf(out, "<a target=_new href=\"https://bioinfo.affymetrix.com/Collab/"
		"Manuel_Ares/WebTools/asViewer.asp?gene=%s\"> <b> Affy </b></a>\n", affyName);
	fprintf(out, "</td></tr>\n<tr><td>");
	if(js->cassette == 1 && js->exonPsProbs != NULL)
	    {
	    fprintf(out, 
		    "<b>Exon Corr:</b> %.2f <b>Exon %%:</b> %.2f <b>Junct %%:</b> %.2f "
		    "<b>Expressed:</b> %d</td></tr><tr><td>\n",
		    js->exonCorr, js->exonSjPercent, js->sjExonPercent, js->exonAgree + js->exonDisagree);
	    fprintf(out, "<b>Exon ps:</b> %s</td></tr><tr><td>\n", js->exonPsName);
	    }
	fprintf(out, "<table width=100%%>\n");

	for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	    {
	    col = "#bebebe";
	    makeJunctMdbLink(js, bedHash, dy, junctIx, probM);
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Junct:</b> %s</td></tr>", col, dy->string);
	    makeJunctExpressedTissues(js, dy, junctIx, probM);
	    col = "#ffffff";
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Exp:</b> %s </td></tr>",col, dy->string);
	    makeJunctNotExpressedTissues(js, dy, junctIx, probM);
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Not Exp:</b> %s </td></tr>", col, dy->string);
	    }
	fprintf(out, "</table></td></tr>\n");
	fprintf(out, "</table></td></tr></table><br></li>\n");
	}
    }
}

char *findBestDuplicate(struct junctSet *js, struct resultM *intenM)
/* Loop through the junction duplicate set and see which one
   has the best correlation to a gene set. */
{
double bestCorr = -2;
double corr = 0;
int gsRow = 0;
int sjRow = 0;
char *bestJunct = NULL;
int junctIx = 0, geneIx = 0;
for(junctIx = 0; junctIx < js->junctDupCount; junctIx++)
    {
    for(geneIx = 0; geneIx < js->genePSetCount; geneIx++)
	{
	gsRow = hashIntValDefault(intenM->nameIndex, js->genePSets[geneIx], -1);
	sjRow = hashIntValDefault(intenM->nameIndex, js->dupJunctPSets[junctIx], -1);
	if(gsRow == -1 || sjRow == -1)
	    continue;
	corr = correlation(intenM->matrix[gsRow], intenM->matrix[sjRow], intenM->colCount);
	if(corr >= bestCorr)
	    {
	    bestCorr = corr;
	    bestJunct = js->dupJunctPSets[junctIx];
	    }
	}
    }
return bestJunct;
}

double identity(double d) 
/* Return d. Seems dumb but avoids a lot of if/else blocks... */
{
return d;
}

double log2(double d)
/* Return log base 2 of d. */
{
double ld = 0;
assert(d != 0);
ld = log(d)/log(2);
return ld;
}

void calcJunctSetRatios(struct junctSet *jsList, struct resultM *intenM)
/* Caluclate the ratio of junction to every other junction in 
   the set. */
{
char *outFile = optionVal("ratioFile", NULL);
boolean doLog = optionExists("log2Ratio");
struct junctSet *js = NULL;
int i = 0, j = 0, expIx = 0;
int numIx = 0, denomIx = 0;
FILE *out = NULL;
FILE *redundant = NULL;
double **mat = intenM->matrix;
int colCount = intenM->colCount;
double (*transform)(double d) = NULL;
struct hash *nIndex = intenM->nameIndex;
char buff[4096];


if(outFile == NULL)
    errAbort("Must specify a ratioFile to print ratios to.");

if(doLog)
    transform = log2;
else
    transform = identity;
safef(buff, sizeof(buff), "%s.redundant", outFile);
redundant = mustOpen(buff, "w");
out = mustOpen(outFile, "w");
/* Print the header. */
fprintf(out, "YORF\tNAME\t");
for(i = 0; i < colCount-1; i++)
    fprintf(out, "%s\t", intenM->colNames[i]);
fprintf(out, "%s\n", intenM->colNames[i]);
for(js = jsList; js != NULL; js = js->next) 
    {
    char **jPSets = js->junctPSets;
    char **djPSets = js->dupJunctPSets;
    int jCount = js->maxJunctCount;
    int dCount = js->junctDupCount;
    char *gsName = NULL;
    if(strstr(js->genePSets[0], "EX") && js->genePSetCount == 2)
	gsName = js->genePSets[1];
    else
	gsName = js->genePSets[0];
	    
    for(i = 0; i < jCount; i++)
	{
	numIx = hashIntValDefault(nIndex, jPSets[i], -1);
	if(numIx == -1)
	    errAbort("Can't find %s in hash.", jPSets[i]);

	/* Calc ratios for the rest of the probe sets. */
	for(j = i+1; j < jCount; j++)
	    {
	    denomIx = hashIntValDefault(nIndex, jPSets[j], -1);
	    if(numIx == -1)
		errAbort("Can't find %s in hash.", jPSets[j]);
	    fprintf(out, "%s;%s\t%s\t", jPSets[i], jPSets[j], gsName);
	    for(expIx = 0; expIx < colCount - 1; expIx++)
		{
		assert(pow(2,mat[denomIx][expIx]) != 0);
		fprintf(out, "%f\t", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
		}
	    fprintf(out, "%f\n", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
	    }
	/* Calc ratios for the dup probe sets. */
	if(dCount > 1)
	    fprintf(redundant, "%s", js->name);
	for(j = 0; j < dCount; j++)
	    {
	    denomIx = hashIntValDefault(nIndex, djPSets[j], -1);
	    if(numIx == -1)
		errAbort("Can't find %s in hash.", djPSets[j]);
	    fprintf(out, "%s;%s\t%s\t", jPSets[i], djPSets[j], gsName);
	    if(dCount > 1)
		fprintf(redundant, "\t%s;%s", jPSets[i], djPSets[j]);
	    for(expIx = 0; expIx < colCount - 1; expIx++)
		{
		assert(pow(2,mat[denomIx][expIx]) != 0);
		fprintf(out, "%f\t", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
		}
	    fprintf(out, "%f\n", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
	    }
	if(dCount > 1)
	    fprintf( redundant, "\n");
	}
    
    /* Get the ratios of the duplicate probes to eachother. */
    for(i = 0; i < dCount; i++)
	{
	numIx = hashIntValDefault(nIndex, djPSets[i], -1);
	if(numIx == -1)
	    errAbort("Can't find %s in hash.", djPSets[i]);

	/* Calc ratios for the rest of the dup probe sets. */
	for(j = i+1; j < dCount; j++)
	    {
	    denomIx = hashIntValDefault(nIndex, djPSets[j], -1);
	    if(numIx == -1)
		errAbort("Can't find %s in hash.", djPSets[j]);
	    fprintf(out, "%s;%s\t%s\t", djPSets[i], djPSets[j], gsName);
	    for(expIx = 0; expIx < colCount - 1; expIx++)
		{
		assert(pow(2,mat[denomIx][expIx]) != 0);
		fprintf(out, "%f\t", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
		}
	    fprintf(out, "%f\n", transform(pow(2,mat[numIx][expIx]) / pow(2,mat[denomIx][expIx])) );
	    }
	}
    }
carefulClose(&out);
carefulClose(&redundant);
}


void fillInJunctUsed(struct junctSet *jsList, struct resultM *intenM)
/* If it isn't already determined, figures out which junctions we're
   using. */
{
struct junctSet *js = NULL;
char *bestDup = NULL;
int i = 0;
for(js = jsList; js != NULL; js = js->next)
    {
    if(js->junctUsedCount != 0) /* If it has already been filled in forget about it. */
	continue;
    if(js->junctDupCount == 0)  /* Trivial if there are no redundant sets to choose from. */
	{
	js->junctUsedCount = js->maxJunctCount;
	AllocArray(js->junctUsed, js->junctUsedCount);
	for(i = 0; i < js->junctUsedCount; i++)
	    js->junctUsed[i] = cloneString(js->junctPSets[i]);
	}
    else /* Go find the redundant junction that correlates best with a gene probe set. */
	{
	js->junctUsedCount = js->maxJunctCount + 1;
	AllocArray(js->junctUsed, js->junctUsedCount);
	for(i = 0; i < js->junctUsedCount - 1; i++)
	    js->junctUsed[i] = cloneString(js->junctPSets[i]);
	bestDup = findBestDuplicate(js, intenM);
	if(bestDup != NULL)
	    js->junctUsed[i] = cloneString(bestDup);
	else
	    js->junctUsedCount--;
	}
    }
}
	
struct hash *hashBeds(char *fileName)
/* Hash all of the beds in a file. */
{
struct bed *bedList = NULL, *bed = NULL;
struct hash *bedHash = newHash(12);

bedList = bedLoadAll(fileName);
for(bed = bedList; bed != NULL; bed = bed->next)
  {
    hashAddUnique(bedHash, bed->name, bed);
  }
return bedHash;
}

int agxVertexByPos(struct altGraphX *agx, int position)
/* Return the vertex index by to position. */
{
  int *vPos = agx->vPositions;
  int vC = agx->vertexCount;
  int i = 0;
  for(i = 0; i < vC; i++)
    if(vPos[i] == position)
      return i;
  return -1;
}

int bedSpliceStart(const struct bed *bed)
/* Return the start of the bed splice site. */
{
assert(bed->blockSizes);
return bed->chromStart + bed->blockSizes[0];
}

int bedSpliceEnd(const struct bed *bed)
/* Return the end of the bed splice site. */
{
assert(bed->blockSizes);
return bed->chromStart + bed->chromStarts[1];
}

boolean connectToDownStreamSoft(struct altGraphX *ag, bool **em, int v)
/* Does this vertex (v) connect to a downstream vertex to 
   create an soft ended exon. */
{
int i = 0;
int vC = ag->vertexCount;
unsigned char *vT = ag->vTypes;
for(i = 0; i < vC; i++)
    {
    if(em[v][i] && vT[i] == ggSoftEnd && 
       getSpliceEdgeType(ag, altGraphXGetEdgeNum(ag, v, i)) == ggExon)
	return TRUE;
    }
return FALSE;
}

boolean connectToUpstreamSoft(struct altGraphX *ag, bool **em, int v)
/* Does this vertex (v) connect to a upstream vertex to 
   create an soft started exon. */
{
int i = 0;
int vC = ag->vertexCount;
unsigned char *vT = ag->vTypes;
for(i = 0; i < vC; i++)
    {
    if(em[i][v] && vT[i] == ggSoftStart && 
       getSpliceEdgeType(ag, altGraphXGetEdgeNum(ag, i, v)) == ggExon)
	return TRUE;
    }
return FALSE;
}

int translateStrand(int type, char strand)
/* Translate the type by strand. If the strand is negative
   both alt3Prime and alt5Prime are flipped. */
{
boolean isNeg = strand == '-';
if(type == alt3Prime && isNeg)
    type = alt5Prime;
else if(type == alt3PrimeSoft && isNeg)
    type = alt5PrimeSoft;
else if(type == alt5Prime && isNeg)
    type = alt3Prime;
else if(type == alt5PrimeSoft && isNeg)
    type = alt3PrimeSoft;
return type;
}

void outputJunctDna(char *bedName, struct altGraphX *ag, FILE *dnaOut)
/* Write out the dna for a given junction in the
   sense orientation. */
{
struct bed *bed = NULL;
bool **em = NULL;
unsigned char *vTypes = NULL;
int *vPos = NULL;
int start1 = -1, end1 = -1, start2 = -1, end2 = -1;
struct dnaSeq *upSeq1 = NULL, *downSeq1 = NULL, *exonSeq1 = NULL;
struct dnaSeq *upSeq2 = NULL, *downSeq2 = NULL, *exonSeq2 = NULL;
bed = hashMustFindVal(junctBedHash, bedName);
em = altGraphXCreateEdgeMatrix(ag);
end1 = agxVertexByPos(ag, bedSpliceStart(bed));
start2 = agxVertexByPos(ag, bedSpliceEnd(bed));

if(end1 == -1 || start2 == -1)
    {
    altGraphXFreeEdgeMatrix(&em, ag->vertexCount);
    noDna++;
    return;
    }
    
end2 = agxFindClosestDownstreamVertex(ag, em, start2);
start1 = agxFindClosestUpstreamVertex(ag, em, end1);
if(end2 == -1 || start1 == -1)
    {
    altGraphXFreeEdgeMatrix(&em, ag->vertexCount);
    noDna++;
    return;
    }
vPos = ag->vPositions;
upSeq1 = hChromSeq(bed->chrom, vPos[start1]-200, vPos[start1]);
exonSeq1 = hChromSeq(bed->chrom, vPos[start1], vPos[end1]);
downSeq1 = hChromSeq(bed->chrom, vPos[end1], vPos[end1]+200);
upSeq2 = hChromSeq(bed->chrom, vPos[start2]-200, vPos[start2]);
exonSeq2 = hChromSeq(bed->chrom, vPos[start2], vPos[end2]);
downSeq2 = hChromSeq(bed->chrom, vPos[end2], vPos[end2]+200);

if(bed->strand[0] == '-')
    {
    reverseComplement(upSeq1->dna, upSeq1->size);
    reverseComplement(exonSeq1->dna, exonSeq1->size);
    reverseComplement(downSeq1->dna, downSeq1->size);
    reverseComplement(upSeq2->dna, upSeq2->size);
    reverseComplement(exonSeq2->dna, exonSeq2->size);
    reverseComplement(downSeq2->dna, downSeq2->size);
    fprintf(dnaOut, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
	    bedName, downSeq2->dna, exonSeq2->dna, upSeq2->dna, 
	    downSeq1->dna, exonSeq1->dna, upSeq1->dna);
    }
else
    {
    fprintf(dnaOut, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
	    bedName, upSeq1->dna, exonSeq1->dna, upSeq2->dna, 
	    upSeq2->dna, exonSeq2->dna, downSeq2->dna);
    }
dnaSeqFree(&upSeq1);
dnaSeqFree(&exonSeq1);
dnaSeqFree(&downSeq1);
dnaSeqFree(&upSeq2);
dnaSeqFree(&exonSeq2);
dnaSeqFree(&downSeq2);
altGraphXFreeEdgeMatrix(&em, ag->vertexCount);
withDna++;
}

void outputDnaForJunctSet(struct junctSet *js, FILE *dnaOut)
/* For each junction in a junction set. Cut out the dna for the exon
   associated with it.  Format is junctName 3'intron exon '5intron
*/
{
bool **em = NULL;
int type = -1;
struct binElement *be = NULL, *beList = NULL, *beStrand = NULL;
unsigned char *vTypes = NULL;
struct altGraphX *ag = NULL, *agList = NULL;
int i = 0;
beList = chromKeeperFind(js->chrom, js->chromStart, js->chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    ag = be->val;
    if(ag->strand[0] == js->strand[0])
	slSafeAddHead(&agList, ag);
    }
slFreeList(&beList);

/* Can't do much with no graphs or too many graphs. */
if(agList == NULL || slCount(agList) != 1)
    return;
ag = agList;

/* Do the individual junctions. */
for(i = 0; i < js->maxJunctCount; i++)
    {
    outputJunctDna(js->junctPSets[i], ag, dnaOut);
    }
for(i = 0; i < js->junctDupCount; i++)
    {
    outputJunctDna(js->dupJunctPSets[i], ag, dnaOut);
    }

}
void outputDnaForAllJuctSets(struct junctSet *jsList)
/* Loop through and output dna for all of the junctions. */
{
char *db = optionVal("db", NULL);
char *dnaFile = optionVal("outputDna", NULL);
FILE *dnaOut = NULL;
struct junctSet *js = NULL;
if(db == NULL)
    errAbort("Must specify db when specifying outputDna.");
assert(dnaFile);
hSetDb(db);
dnaOut = mustOpen(dnaFile, "w");
for(js = jsList; js != NULL; js = js->next)
    {
    outputDnaForJunctSet(js, dnaOut);
    }
warn("%d with dna, %d without dna", withDna, noDna);
carefulClose(&dnaOut);
}

void outputCassetteBed(struct junctSet *js, struct bed *bedUp, struct bed *bedDown,
		   struct altGraphX *ag, bool **em, int start, int ve1, int ve2,
		   int altBpStart, int altBpEnd)
/* Write out a cassette bed to a file. */
{
struct bed bed; 
if(js->altExpressed && cassetteBedOut)
    {
    bed.chrom = bedUp->chrom;
    bed.chromStart = ag->vPositions[altBpStart];
    bed.chromEnd = ag->vPositions[altBpEnd];
    bed.name = js->name;
    bed.strand[0] = bedUp->strand[0];
    bed.score = altCassette;
    bedTabOutN(&bed,6,cassetteBedOut);
    }
}

int spliceTypeForJunctSet(struct junctSet *js)
/* Determine the alternative splicing type for a junction set.  This
 function is a little long as I have to convert into altGraphX and do
 some strand specific things. */
{
bool **em = NULL;
int type = -1;
struct binElement *be = NULL;
int ssPos[4];
int ssCount = 0;
struct bed *bedUp = NULL, *bedDown = NULL;
unsigned char *vTypes = NULL;
struct altGraphX *ag = NULL;

/* Can't have more than three introns and be a simple
   process. */
if(js->maxJunctCount + js->junctDupCount > 3)
    return -1;

be = chromKeeperFind(js->chrom, js->chromStart, js->chromEnd);
if(slCount(be) == 1)
    ag = be->val;
slFreeList(&be);
if(ag == NULL)
    return -1;

vTypes = ag->vTypes;
em = altGraphXCreateEdgeMatrix(ag);
/* Get the beds. If their are enough primar junctions
 use them. Otherwise use the duplicates. */
if(js->maxJunctCount >= 2)
    {
    bedUp = hashMustFindVal(junctBedHash, js->junctPSets[0]);
    bedDown = hashMustFindVal(junctBedHash, js->junctPSets[1]);
    }
else
    {
    bedUp = hashMustFindVal(junctBedHash, js->junctPSets[0]);
    bedDown = hashMustFindVal(junctBedHash, js->dupJunctPSets[0]);
    }

/* Sort the beds. */
if(bedUp->chromStart > bedDown->chromStart ||
   (bedDown->chromStart == bedDown->chromStart && 
    bedUp->chromEnd > bedDown->chromEnd))
    {
    struct bed *tmp = bedUp;
    bedUp = bedDown;
    bedDown = tmp;
    }

/* If same start type check for alternative 3' splice. */
if(bedUp->chromStart == bedDown->chromStart)
    {
    int start = -1, ve1 = -1, ve2 = -2;
    int altBpStart = 0, altBpEnd = 0, firstVertex = 0, lastVertex = 0;
    int isThreePrime = -1;
    start = agxVertexByPos(ag, bedSpliceStart(bedUp));
    ve1 = agxVertexByPos(ag, bedSpliceEnd(bedUp));
    ve2 = agxVertexByPos(ag, bedSpliceEnd(bedDown));
    isThreePrime = agxIsAlt3Prime(ag, em, start, ve1, ve2,
				  &altBpStart, &altBpEnd, &firstVertex, &lastVertex);
    if(isThreePrime)
	type = alt3Prime;
    else if(agxIsAlt3PrimeSoft(ag, em, start, ve1, ve2,
			       &altBpStart,  &altBpEnd, &firstVertex, &lastVertex))
	{
	type = alt3PrimeSoft;
	}
    else if(agxIsCassette(ag, em, start, ve1, ve2, 
			  &altBpStart,  &altBpEnd, &firstVertex, &lastVertex))
	{
	type = altCassette;
	js->cassette = TRUE;
	}
/*     outputAlt3Dna(js, bedUp, bedDown, ag, em, start, ve1, ve2, altBpStart, altBpEnd) */
    if(type == altCassette)
	outputCassetteBed(js, bedUp, bedDown, ag, em, start, ve1, ve2, altBpStart, altBpEnd);
    type = translateStrand(type, ag->strand[0]);
    }
/* If same end then check for an alternative 5' splice site. */
else if(bedUp->chromEnd == bedDown->chromEnd)
    {
    int start1 = -1, start2 = -1, ve1 = -1, ve2 = -2;
    int altBpStart = 0, altBpEnd = 0, firstVertex = 0, lastVertex = 0;
    int isFivePrime = -1;
    ve1 = agxVertexByPos(ag, bedSpliceStart(bedUp));
    ve2 = agxVertexByPos(ag, bedSpliceStart(bedDown));
    start1 = agxFindClosestUpstreamVertex(ag, em, ve1);
    start2 = agxFindClosestUpstreamVertex(ag, em, ve2);
    if(start1 == start2)
	{
	if(agxIsAlt5Prime(ag, em, start1, ve1, ve2,
			  &altBpStart, &altBpEnd, &firstVertex, &lastVertex))
	    type = alt5Prime;
	}
    else if(agxIsAlt5PrimeSoft(ag, em, start1, start2, ve1, ve2, 
			       &altBpStart, &altBpEnd, &firstVertex, &lastVertex)) 
        /* Check for alternative transcription starts. */
	{
	type = alt5PrimeSoft;
	}
    else  /* Could be a cassette. */
	{
	int start = -1;
	int end = agxVertexByPos(ag, bedSpliceEnd(bedDown));
	start = agxVertexByPos(ag, bedSpliceStart(bedUp));
	if(agxIsCassette(ag, em, start, start2, end,
			 &altBpStart, &altBpEnd, &firstVertex, &lastVertex)) 
	    {
	    type = altCassette;
	    js->cassette = TRUE;
	    }
	if(type == altCassette)
	    outputCassetteBed(js, bedUp, bedDown, ag, em, start, ve1, ve2, altBpStart, altBpEnd);
	}

    type = translateStrand(type, ag->strand[0]);
    }
if(js->cassette && type != altCassette)
    js->cassette == FALSE;
altGraphXFreeEdgeMatrix(&em, ag->vertexCount);
return type;
}

void countSpliceType(int type)
/* Record the counts of different alt-splice types. */
{
switch(type)
    {
    case alt3Prime:
	alt3Count++;
	break;
    case alt5Prime:
	alt5Count++;
	break;
    case alt3PrimeSoft:
	alt3SoftCount++;
	break;
    case alt5PrimeSoft:
	alt5SoftCount++;
	break;
    case altCassette:
	altCassetteCount++;
	break;
    default:
	otherCount++;
    }
}

    
void outputBedsForJunctSet(int type, struct junctSet *js)
/* Write out the beds for the junction set with the 
   type as the scores. */
{
struct bed *bed = NULL;
int i = 0;
for(i = 0; i < js->maxJunctCount; i++)
    {
    bed = hashFindVal(junctBedHash, js->junctPSets[i]);
    bed->score = type;
    bedTabOutN(bed, 12, bedJunctTypeOut);
    }
for(i = 0; i < js->junctDupCount; i++)
    {
    bed = hashFindVal(junctBedHash, js->dupJunctPSets[i]);
    bed->score = type;
    bedTabOutN(bed, 12, bedJunctTypeOut);
    }
}

void determineSpliceTypes(struct junctSet *jsList)
{
int type = -1;
struct junctSet *js = NULL;
for(js = jsList; js != NULL; js = js->next)
    {
      type = spliceTypeForJunctSet(js);
      if(type == -1)
	  type = altOther;
      js->spliceType = type;
      countSpliceType(type);
      outputBedsForJunctSet(type, js);
    }
}

void outputTissueSpecific(struct junctSet *jsList, struct resultM *probM, struct hash *bedHash)
/* Output alternatively spliced isoforms that are tissue specific. */
{
struct junctSet *js = NULL;
char *tissueSpecific = optionVal("tissueSpecific", NULL);
FILE *tsOut = NULL;
int tsCount = 0;
tissueExpThresh = optionInt("tissueExpThresh",1);
assert(tissueSpecific);
tsOut = mustOpen(tissueSpecific, "w");
for(js = jsList; js != NULL; js = js->next)
    {
    int junctIx = 0;
    if(js->altExpressed != TRUE)
	continue;
    for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	{
	boolean isSkip = FALSE;
	boolean isInclude = FALSE;
	int count = 0;
	int tissueIx = 0;
	int colIx = 0, rowIx = 0;
	if(js->spliceType == altCassette)
	    {
	    isSkip = (skipJsIx(js, bedHash) == junctIx);
	    isInclude = (includeJsIx(js, bedHash) == junctIx);
	    }
	for(colIx = 0; colIx < probM->colCount; colIx++)
	    {
	    if(junctionExpressed(js, colIx, junctIx) == TRUE)
		{

		tissueIx = colIx;
		count++;
		}
	    }
	if(count == 1)
	    {
	    int otherJunctIx = 0;
	    /* Require that at least one other junction is expressed in another tissue. */
	    for(otherJunctIx = 0; otherJunctIx < js->junctUsedCount; otherJunctIx++)
		{   
		int otherColIx = 0;
		if(otherJunctIx == junctIx)
		    continue;
		for(otherColIx = 0; otherColIx < probM->colCount; otherColIx++)
		    {
		    if(otherColIx == tissueIx)
			continue;
		    if(junctionExpressed(js, otherColIx, otherJunctIx)) 
			{
			fprintf(tsOut, "%s\t%s\t%d\t%d\t%d\n", js->junctUsed[junctIx], probM->colNames[tissueIx], 
				js->spliceType, isSkip, isInclude);
			tsCount++;
			otherJunctIx = js->junctUsedCount+1; /* To end the outer loop. */
			break; /* to end the inner loop. */
			}
		    }
		}
	    }
	}
    }
warn("%d junctions are tissue specific", tsCount);
carefulClose(&tsOut);
}

void outputBrainSpecific(struct junctSet *jsList, struct resultM *probM, struct hash *bedHash)
/* Output alternatively spliced isoforms that are brain specific. */
{
struct junctSet *js = NULL;
char *brainSpecific = optionVal("brainSpecific", NULL);
FILE *bsOut = NULL;
int bsCount = 0;
boolean *isBrain = NULL;
int i = 0, j = 0;
char *brainTissues[] = {"cerebellum","cerebral_hemisphere","cortex","medial_eminence","olfactory_bulb","pinealgland","thalamus"};
tissueExpThresh = optionInt("tissueExpThresh",1);
assert(brainSpecific);
AllocArray(isBrain, probM->colCount);
for(i = 0; i < probM->colCount; i++)
    {
    for(j = 0; j < ArraySize(brainTissues); j++) 
	{
	if(sameWord(brainTissues[j], probM->colNames[i])) 
	    {
	    isBrain[i] = TRUE;
	    break;
	    }
	}
    }

bsOut = mustOpen(brainSpecific, "w");
for(js = jsList; js != NULL; js = js->next)
    {
    int junctIx = 0;
/*     if(js->altExpressed != TRUE) */
/* 	continue; */
    for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	{
	boolean isSkip = FALSE;
	boolean isInclude = FALSE;
	int count = 0;
	int brainIx = 0;
	int colIx = 0, rowIx = 0;
	for(colIx = 0; colIx < probM->colCount; colIx++)
	    {
	    if((junctionExpressed(js, colIx, junctIx) == TRUE ||
		geneExpressed(js, colIx) == TRUE)
		&& isBrain[colIx])
		{
		brainIx = colIx;
		if(js->spliceType == altCassette)
		    {
		    isSkip = (skipJsIx(js, bedHash) == junctIx);
		    isInclude = (includeJsIx(js, bedHash) == junctIx);
		    }
		count++;
		}
	    else if(junctionExpressed(js, colIx, junctIx) == TRUE && !isBrain[colIx])
		{
		count = -1;
		break;
		}
	    }
	if(count >= tissueExpThresh)
	    {
	    int otherJunctIx = 0;
	    /* Require that at least one other junction is expressed in another non-brain tissue. */
	    for(otherJunctIx = 0; otherJunctIx < js->junctUsedCount; otherJunctIx++)
		{   
		int otherJunctExpCount = 0;
		int otherColIx = 0;
		if(otherJunctIx == junctIx)
		    continue;
		for(otherColIx = 0; otherColIx < probM->colCount; otherColIx++)
		    {
		    if(isBrain[otherColIx])
			continue;
		    if(junctionExpressed(js, otherColIx, otherJunctIx)) 
			{
			otherJunctExpCount++;
			if(otherJunctExpCount >= tissueExpThresh)
			    {
			    int includeIx = junctIx;
			    int before = 0;
			    if(js->spliceType == altCassette)
				includeIx = includeJsIx(js, bedHash);
			    
			    fprintf(bsOut, "%s\t%s\t%d\t%d\t%d\n", js->name, probM->colNames[brainIx], 
				    js->spliceType, isSkip, isInclude);
			    bsCount++;
			    otherJunctIx = js->junctUsedCount+1; /* To end the outer loop. */
			    break; /* to end the inner loop. */
			    }
			}
		    }
		}
	    }
	}
    }
freez(&isBrain);
warn("%d junctions are brain specific", bsCount);
carefulClose(&bsOut);
}

void altHtmlPages(char *junctFile, char *probFile, char *intensityFile, char *bedFile)
/* Do a top level summary. */
{
struct junctSet *jsList = NULL;
struct junctSet *js = NULL;
struct resultM *probM = NULL;
struct resultM *intenM = NULL;
int setCount = 0;
int junctIx = 0, i = 0;
FILE *htmlOut = NULL;
FILE *htmlFrame = NULL;
FILE *spreadSheet = NULL;
char *spreadSheetName = optionVal("spreadSheet", NULL);
char *browserName = "hgwdev-sugnet.cse";
char nameBuff[2048];
char *htmlPrefix = optionVal("htmlPrefix", "");
struct hash *bedHash = NULL;
char *db = "mm2";

assert(junctFile);
jsList = junctSetLoadAll(junctFile);
warn("Loaded %d records from %s", slCount(jsList), junctFile);
if(optionExists("junctPsMap"))
    {
    printJunctSetProbes(jsList);
    exit(0);
    }
assert(intensityFile);

intenM = readResultMatrix(intensityFile);
warn("Loaded %d rows and %d columns from %s", intenM->rowCount, intenM->colCount, intensityFile);
if(optionExists("doSjRatios"))
    {
    calcJunctSetRatios(jsList, intenM);
    exit(0);
    }

assert(probFile);
assert(bedFile);
probM = readResultMatrix(probFile);
warn("Loaded %d rows and %d columns from %s", probM->rowCount, probM->colCount, probFile);

fillInJunctUsed(jsList, intenM);
fillInProbs(jsList, probM);
fillInIntens(jsList, intenM);
bedHash = hashBeds(bedFile);
junctBedHash = bedHash;
calcExpressed(jsList, probM);

if(doJunctionTypes)
    {
    determineSpliceTypes(jsList);
    warn("%d altCassette, %d alt3, %d alt5, %d altTranStart %d altTranEnd, %d other",
	 altCassetteCount, alt3Count, alt5SoftCount, 
	 alt5Count, alt3SoftCount, otherCount);
    for(js = jsList; js != NULL; js = js->next)
	{
	if(js->expressed)
	    countExpSpliceType(js->spliceType);
	if(js->altExpressed)
	    countExpAltSpliceType(js->spliceType);
	}
    warn("%d altCassette, %d alt3, %d alt5, %d altTranStart %d altTranEnd, %d other expressed",
	 altCassetteExpCount, alt3ExpCount, alt5SoftExpCount, 
	 alt5ExpCount, alt3SoftExpCount, otherExpCount);
    warn("%d altCassette, %d alt3, %d alt5, %d altTranStart %d altTranEnd, %d other expressed alternatively",
	 altCassetteExpAltCount, alt3ExpAltCount, alt5SoftExpAltCount, 
	 alt5ExpAltCount, alt3SoftExpAltCount, otherExpAltCount);
    }
calcExonCorrelation(jsList, bedHash, intenM, probM);
warn("Calculating expression");

if(optionExists("tissueSpecific"))
    {
    outputTissueSpecific(jsList, probM, bedHash);
    }

if(optionExists("brainSpecific"))
    {
    outputBrainSpecific(jsList, probM, bedHash);
    }

if(optionExists("outputDna"))
    outputDnaForAllJuctSets(jsList);

/* Write out the lists. */
warn("Writing out links.");
safef(nameBuff, sizeof(nameBuff), "%s%s", htmlPrefix, "lists.html");
htmlOut = mustOpen(nameBuff, "w");
fprintf(htmlOut, "<html><body>\n");
outputLinks(&jsList, bedHash, probM, htmlOut);
fprintf(htmlOut, "</body></html>\n");
carefulClose(&htmlOut);

/* Loop through and output a spreadsheet compatible file
   with the names of junction sets. */
if(spreadSheetName != NULL)
    {
    spreadSheet = mustOpen(spreadSheetName, "w");
    for(js = jsList; js != NULL; js = js->next) 
	{
	if(js->altExpressed) 
	    {
	    fprintf(spreadSheet, "%s\t%s\t", js->hName,  js->name);
	    for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
		{
		fprintf(spreadSheet, "\t%s\t", js->junctUsed[junctIx]);
		for(i = 0; i < probM->colCount; i++)
		    {
		    if(js->junctProbs[junctIx][i] >= presThresh && 
		       geneExpressed(js, i))
			fprintf(spreadSheet, "%s,", probM->colNames[i]);
		    }
		}
	    fprintf(spreadSheet, "\n");
	    }
	}
    carefulClose(&spreadSheet);
    }


/* Write out the frames. */
safef(nameBuff, sizeof(nameBuff), "%s%s", htmlPrefix, "frame.html");
htmlFrame = mustOpen(nameBuff, "w");
fprintf(htmlFrame, "<html><head><title>Junction Sets</title></head>\n"
	"<frameset cols=\"30%,70%\">\n"
	"   <frame name=\"_list\" src=\"./%s%s\">\n"
	"   <frameset rows=\"50%,50%\">\n"
	"      <frame name=\"browser\" src=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">\n"
	"      <frame name=\"plots\" src=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">\n"
	"   </frameset>\n"
	"</frameset>\n"
	"</html>\n", htmlPrefix, "lists.html", 
	browserName, db, jsList->chrom, jsList->chromStart, jsList->chromEnd,
	browserName, db, jsList->chrom, jsList->chromStart, jsList->chromEnd);
}

void agxLoadChromKeeper(char *file)
     /* Load the agx's in the file into the chromKeeper. */
{
  char *db = optionVal("db", NULL);
  struct altGraphX *agx = NULL, *agxList = NULL;
  if(db == NULL)
    errAbort("Must specify a database when loading agxs.");
  chromKeeperInit(db);
  agxList = altGraphXLoadAll(file);
  for(agx = agxList; agx != NULL; agx = agx->next)
    {
      chromKeeperAdd(agx->tName, agx->tStart, agx->tEnd, agx);
    }
  doJunctionTypes = TRUE;
}

int main(int argc, char *argv[])
{
char *exonPsFile = NULL;
FILE *agxFile = NULL;
char *agxFileName = NULL;
char *cassetteBedOutName = NULL;

if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
presThresh = optionFloat("presThresh", .9);
absThresh = optionFloat("absThresh", .1);
agxFileName = optionVal("agxFile", NULL);

if(agxFileName != NULL)
    {
    char *spliceTypeFile = optionVal("spliceTypes", NULL);
    if(spliceTypeFile == NULL)
	errAbort("Must specify spliceTypes flag when specifying agxFile");
    bedJunctTypeOut = mustOpen(spliceTypeFile, "w");
    agxLoadChromKeeper(agxFileName);
    }
cassetteBedOutName = optionVal("cassetteBed", NULL);
if(cassetteBedOutName != NULL)
    {
    cassetteBedOut = mustOpen(cassetteBedOutName, "w");
    }

if((exonPsFile = optionVal("exonStats", NULL)) != NULL)
    {
    exonPsStats = mustOpen(exonPsFile, "w");
    fprintf(exonPsStats, 
	    "#name\tcorrelation\texonSjPercent\tsjExonPercent\t"
	    "exonAgree\texonDisagree\tsjAgree\tsjDisagree\tprobCorr\tskipCorr\tskipProbCorr\n");
    }
if(optionExists("strictDisagree"))
    disagreeThresh = absThresh;
else
    disagreeThresh = presThresh;
altHtmlPages(optionVal("junctFile", NULL), optionVal("probFile", NULL), 
	     optionVal("intensityFile", NULL), optionVal("bedFile", NULL));
carefulClose(&exonPsStats);
carefulClose(&bedJunctTypeOut);
carefulClose(&cassetteBedOut);
return 0;
}
