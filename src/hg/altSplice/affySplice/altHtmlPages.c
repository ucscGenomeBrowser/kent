/* altHtmlPages.c - Program to create a web based browser
   of altsplice predicitions. */

#include "common.h"
#include "bed.h"
#include "hash.h"
#include "options.h"
#include "chromKeeper.h"
#include "binRange.h"
#include "obscure.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
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
    double **junctProbs;        /* Matrix of junction results, rows are junctUsed. */ 
    double **geneProbs;         /* Matrix of gene set results, rows are genePSets. */
    boolean expressed;          /* TRUE if one form of this junction set is expressed. */
    boolean altExpressed;       /* TRUE if more than one form of this junction set is expressed. */
    double score;               /* Score of being expressed overall. */
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


static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"junctFile", OPTION_STRING},
    {"probFile", OPTION_STRING},
    {"hybeFile", OPTION_STRING},
    {"presThresh", OPTION_FLOAT},
    {"absThresh", OPTION_FLOAT},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File containing junctionSets from plotAndCountAltsMultGs.R log.",
    "File containing summarized probability of expression for each probe set.",
    "File giving names and ids for hybridizations.",
    "Optional threshold at which to call expressed.",
    "Optional threshold at which to call not expressed."
};

double presThresh = 0;   /* Probability above which we consider something expressed. */
double absThresh = 0;    /* Probability below which we consider something not expressed. */


void usage()
/** Print usage and quit. */
{
int i=0;
warn("altHtmlPages - Program to create a web based browser\n"
     "of altsplice predicitions.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   ");
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

struct junctSet *junctSetLoad(char *row[])
/* Load up a junct set. */
{
struct junctSet *js = NULL;
int index = 1;
int count = 0;
AllocVar(js);

js->chrom = cloneString(row[index++]);
js->chromStart = sqlUnsigned(row[index++]);
js->chromEnd = sqlUnsigned(row[index++]);
js->name = cloneString(row[index++]);
js->junctCount = js->maxJunctDupCount = sqlUnsigned(row[index++]);
js->strand[0] = row[index++][0];
js->genePSetCount = sqlUnsigned(row[index++]);
sqlStringDynamicArray(row[index++], &js->genePSets, &count);
js->hName = cloneString(row[index++]);
sqlStringDynamicArray(row[index++], &js->junctPSets, &count);
js->maxJunctCount = js->junctDupCount = sqlUnsigned(row[index++]);
sqlStringDynamicArray(row[index++], &js->dupJunctPSets, &count);
js->junctUsedCount = sqlUnsigned(row[index++]);
sqlStringDynamicArray(row[index++], &js->junctUsed, &count);
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
    el = junctSetLoad(row);
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
	corr = correlation(js->junctProbs[i], js->junctProbs[j], colCount);
	if(minCorr > corr)
	    minCorr = corr;
	}
    }
return minCorr;
}

boolean junctionExpressed(struct junctSet *js, int colIx, int junctIx)
/* Is the junction at junctIx expressed above the 
   threshold in colIx? */
{
boolean geneExp = FALSE;
boolean junctExp = FALSE;
int i = 0;
for(i = 0; i < js->genePSetCount; i++)
    {
    if(js->geneProbs[i][colIx] >= presThresh)
	geneExp = TRUE;
    }
if(geneExp && js->junctProbs[junctIx][colIx] >= presThresh)
    junctExp = TRUE;
return junctExp;
}


int calcExpressed(struct junctSet *jsList, struct resultM *probMat)
/* Loop through and calculate expression and score where score is correlation. */
{
struct junctSet *js = NULL;
int i = 0, j = 0, k = 0;
double minCor = 2; /* Correlation is always between -1 and 1, 2 is
		    * good max. */
double *X = NULL;
double *Y = NULL;
int elementCount;
int colCount = probMat->colCount;
int rowCount = probMat->rowCount;     
int colIx = 0, junctIx = 0;           /* Column and Junction indexes. */
int junctOneExp = 0, junctTwoExp = 0; /* What are the counts of
				       * expressed and alts. */

for(js = jsList; js != NULL; js = js->next)
    {
    int *expressed = NULL;
    int totalTissues = 0;
    int junctExpressed = 0;
    AllocArray(expressed, js->junctUsedCount);

    /* Loop through and see how many junctions
       are actually expressed and in how many tissues. */
    for(colIx = 0; colIx < colCount; colIx++)
	{
	boolean anyExpressed = FALSE;
	for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	    {
	    if(junctionExpressed(js, colIx, junctIx) == TRUE)
		{
		expressed[junctIx]++;
		anyExpressed = TRUE;
		}
	    }
	if(anyExpressed)
	    totalTissues++;
	}
    
    /* Set number expressed. */
    for(i = 0; i < js->junctUsedCount; i++)
	{
	if(expressed[i] > 0)
	    junctExpressed++;
	}

    /* If one tissue expressed set is expressed. */
    if(junctExpressed >= 1)
	{
	js->expressed = TRUE;
	junctOneExp++;
	}
    
    /* If two tissues are expressed set is alternative expressed. */
    if(junctExpressed >= 2)
	{
	js->altExpressed = TRUE;
	junctTwoExp++;
	}

    if(js->altExpressed == TRUE)
	js->score = calcMinCorrelation(js, expressed, totalTissues, colCount);
    else
	js->score = BIGNUM;
    freez(&expressed);
    }

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

void makePlotLinks(struct junctSet *js, struct dyString *buff)
{
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"./noAltTranStartJunctSet/altPlot/%s.%d-%d.%s.png\">[all]</a> ",
	       js->chrom, js->chromStart, js->chromEnd, js->hName);
dyStringPrintf(buff, "<a target\"plots\" href=\"./noAltTranStartJunctSet/altPlot.median/%s.%d-%d.%s.png\">[median]</a> ",
	       js->chrom, js->chromStart, js->chromEnd, js->hName);
}

boolean geneExpressed(struct junctSet *js, int colIx)
/* Are any of the gene sets expressed in this sample? */
{
int i = 0;
boolean expressed = FALSE;
for(i = 0; i < js->genePSetCount; i++)
    {
    if(js->geneProbs[i][colIx] >= presThresh)
	expressed = TRUE;
    }
return expressed;
}

void makeJunctExpressedTissues(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
for(i = 0; i < probM->colCount; i++)
    {
    if(js->junctProbs[junctIx][i] >= presThresh && 
       geneExpressed(js, i))
	dyStringPrintf(buff, "%s, ", probM->colNames[i]);
    }
}

void makeJunctExpressedLink(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.cse.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on\">", 
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
dyStringClear(buff);
for(i = 0; i < probM->colCount; i++)
    {
    if(js->junctProbs[junctIx][i] <= absThresh)
	dyStringPrintf(buff, "%s, ", probM->colNames[i]);
    }
}

void makeJunctMdbLink(struct junctSet *js, struct dyString *buff, int junctIx, struct resultM *probM)
{
int offSet = 100;
int i = 0;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"plots\" href=\"http://mdb1-sugnet.cse.ucsc.edu/cgi-bin/mdbSpliceGraph?mdbsg.calledSelf=on&coordString=%s:%c:%s:%d-%d&mdbsg.cs=%d&mdbsg.ce=%d&mdbsg.expName=%s&mdbsg.probeSummary=on\">", 
	       "mm2", js->strand[0], js->chrom, js->chromStart, js->chromEnd, 
	       js->chromStart - offSet, js->chromEnd + offSet, "AffyMouseSplice1-02-2004");
dyStringPrintf(buff, "%s", js->junctUsed[junctIx]);
dyStringPrintf(buff, "</a> ");
}

void makeGbCoordLink(struct junctSet *js, int chromStart, int chromEnd, struct dyString *buff)
/* Make a link to the genome browser with links highlighted. */
{
int junctIx = 0;
int offSet = 100;
dyStringClear(buff);
dyStringPrintf(buff, "<a target=\"browser\" href=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&splicesPFt=red&splicesPlt=or&splicesP_name=", js->chrom, chromStart - offSet, chromEnd + offSet);
for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
    dyStringPrintf(buff, "%s ", js->junctUsed[junctIx]);
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
    dyStringPrintf(buff, "<a target=\"browser\" href=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?position=%s&splicesPFt=red&splicesPlt=or&splicesP_name=", js->hName);
    for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	dyStringPrintf(buff, "%s ", js->junctUsed[junctIx]);
    dyStringPrintf(buff, "\">%s</a>", js->hName);
    }
else
    dyStringPrintf(buff, "%s", js->hName);
}

void outputLinks(struct junctSet *jsList, struct resultM *probM, FILE *out)
/* Output a report for each junction set. */
{
struct junctSet *js = NULL;
int colIx = 0, junctIx = 0;
struct dyString *dy = newDyString(2048);
slSort(&jsList, junctSetScoreCmp);
fprintf(out, "<ol>\n");
for(js = jsList; js != NULL; js = js->next)
    {
    if(js->score <= 1)
	{
	boolean gray = FALSE;
	char *col = NULL;
	fprintf(out, "<li>");
	fprintf(out, "<table>");
	fprintf(out, "<tr><td bgcolor=\"#7896be\">");
	makeGbClusterLink(js, dy);
	fprintf(out, "<b>Id:</b> %s ", dy->string);
	makeGbLink(js, dy);
	fprintf(out, "<b>GeneName:</b> %s ", dy->string);
	makePlotLinks(js, dy);
	fprintf(out, "<b>Plots:</b> %s ", dy->string);
	fprintf(out, "<b>Score:</b> %.2f", js->score);
	fprintf(out, "</td></tr>\n<tr><td><table>\n");

	for(junctIx = 0; junctIx < js->junctUsedCount; junctIx++)
	    {
	    col = "#bebebe";
	    makeJunctMdbLink(js, dy, junctIx, probM);
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Junct:</b> %s</td></tr>", col, dy->string);
	    makeJunctExpressedTissues(js, dy, junctIx, probM);
	    col = "#ffffff";
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Exp:</b> %s </td></tr>",col, dy->string);
	    makeJunctNotExpressedTissues(js, dy, junctIx, probM);
	    fprintf(out, "<tr><td bgcolor=\"%s\"><b>Not Exp:</b> %s </td></tr>", col, dy->string);
	    }
	fprintf(out, "</table></td></tr>\n");
	fprintf(out, "</table>\n");
	}
    }
}

void altHtmlPages(char *junctFile, char *probFile)
/* Do a top level summary. */
{
struct junctSet *jsList = junctSetLoadAll(junctFile);
struct resultM *probM = NULL;
FILE *htmlOut = NULL;
FILE *htmlFrame = NULL;
char *browserName = "hgwdev-sugnet.cse";
char *db = "mm2";
warn("Loaded %d records from %s", slCount(jsList), junctFile);

probM = readResultMatrix(probFile);
warn("Loaded %d rows and %d columns from %s", probM->rowCount, probM->colCount, probFile);
fillInProbs(jsList, probM);
warn("Calculating expression");
calcExpressed(jsList, probM);

/* Write out the lists. */
warn("Writing out links.");
htmlOut = mustOpen("lists.html", "w");
fprintf(htmlOut, "<html><body>\n");
outputLinks(jsList, probM, htmlOut);
fprintf(htmlOut, "</body></html>\n");
carefulClose(&htmlOut);

/* Write out the frames. */
htmlFrame = mustOpen("frame.html", "w");
fprintf(htmlFrame, "<html><head><title>Junction Sets</title></head>\n"
	"<frameset cols=\"30%,70%\">\n"
	"   <frame name=\"_list\" src=\"./%s\">\n"
	"   <frameset rows=\"50%,50%\">\n"
	"      <frame name=\"browser\" src=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">\n"
	"      <frame name=\"plots\" src=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">\n"
	"   </frameset>\n"
	"</frameset>\n"
	"</html>\n", "lists.html", 
	browserName, db, jsList->chrom, jsList->chromStart, jsList->chromEnd,
	browserName, db, jsList->chrom, jsList->chromStart, jsList->chromEnd);
}

int main(int argc, char *argv[])
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
presThresh = optionFloat("presThresh", .9);
absThresh = optionFloat("absThresh", .1);
altHtmlPages(optionVal("junctFile", NULL), optionVal("probFile", NULL));
return 0;
}
