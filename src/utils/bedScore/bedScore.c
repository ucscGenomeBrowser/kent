/* bedScore - Add scores to a BED file or transform existing scores. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "sqlList.h"
#include "basicBed.h"
#include "hmmstats.h"
#include "dystring.h"
#include <values.h>

/* Usage definitions and defaults */
#define scoreEncode     "encode"
#define scoreReg        "reg"
#define scoreStd2       "std2"
#define scoreLod        "lod"
#define scoreAsinh      "asinh"

static char *method = scoreEncode;
static int col = 5;
static int minScore = 0;
static int doLog = 0;
static int maxScore = 1000;
static boolean uniform = FALSE;
static struct scorer *scorer = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedScore - Add scores to a BED file or transform existing scores\n"
  "usage:\n"
  "   bedScore input.bed output.bed\n"
  "       or\n"
  "   bedScore input1.bed input2.bed ... outDir\n"
  "options:\n"
  "   -col=N      - Column to use as input value (default %d)\n"
  "   -method=\n"
  "             encode - ENCODE pipeline (UCSC) (default)\n"
  "             reg - Regulatory clusters, maxscore at 1 std dev (UCSC) \n"
  "             std2 - score is maxed at 2 std devs \n"
  "             asinh - ENCODE Uniform TFBS (Steve Wilder, ENCODE AWG/EBI)\n"
  "   -minScore=N - Minimum score to assign (default %d). Not supported for 'reg' method\n"
  "   -uniform   - Calculate uniform normalization factor across all input files\n"
  "   -log  - Calculate based on log\n"
  "note:\n"
  "    If multiple files are specified, they must all be of same BED size\n.",
  /*
  "             lod - Phastcons log odds (Adam Siepel, Cornell)\n"
  */
  col, minScore);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"col", OPTION_INT},
   {"log", OPTION_BOOLEAN},
   {"method", OPTION_STRING},
   {"minScore", OPTION_INT},
   {"uniform", OPTION_BOOLEAN},
   {NULL, 0}
};

/* Two structs to enable capturing the BED in BED5ish fashion, while retaining comments and 
   empty lines. The data lines will be formatted tab-sep, so will lose original 
   whitespace sepration
*/

struct bed5
/* Browser extensible data with 5 fields (somehow not in basic bed!)*/
    {
    struct bed *next;  /* Next in singly linked list. */
    char *chrom;        /* Human chromosome or FPC contig */
    unsigned chromStart;        /* Start position in chromosome */
    unsigned chromEnd;  /* End position in chromosome */
    char *name; /* Name of item */

    /* The following items are not loaded by   the bedLoad routines. */
    int score; /* Score - 0-1000 */  /* Should be uint but there are still some ct users with neg values, .as DOES say uint */
    };

struct bedFileLine
/* A single line in a BED 5+ file.  Could be data, comment, or empty */
    {
    struct bedFileLine *next;
    struct bed5 *bed5;          // NULL if comment or empty line
    double inputVal;            // input value to transform to BED score, if data line
    char *text;                 // comment, empty string, or bedPlus portion of data line
    };

/* Functions to manage the bedFileLines */

boolean lineIsData(char *line)
/* Determine if line is blank or comment */
{
char *s = skipLeadingSpaces(line);
char c = s[0];
if (c != 0 && c != '#')
    return TRUE;
return FALSE;
}

struct bedFileLine *bflNew(char *line, int lineNum)
/* Load a bedFileLine from a file line */
{
char *words[64]; 
int wordCount;
struct bedFileLine *bfl;
static int bedSize = 0;

AllocVar(bfl);
if (!lineIsData(line))
    {
    bfl->text = cloneString(line);
    // DEBUG
    // bflOut(bfl, stdout);
    // fflush(stdout);
    return bfl;
    }
verbose(3, "%s\n", line);
wordCount = chopByWhite(line, words, sizeof(words));
if (bedSize == 0)
    {
    verbose(2, "BED size is %d\n", wordCount);
    if (wordCount < 5 || wordCount < col)
        errAbort("Unexpected number of fields in BED file: %d", wordCount);
    bedSize = wordCount;
    }
if (wordCount != bedSize)
    errAbort("Unexpected number of fields in line #%d of BED %d file: %d", 
                    lineNum, bedSize, wordCount);
bfl->bed5 = (struct bed5 *)bedLoad5(words);
char *input = words[col-1];
if (stringIn(",", input))
    {
    // found comma-sep list, so sum values
    bfl->inputVal = sqlSumDoublesCommaSep(input);
    }
else
    bfl->inputVal = sqlDouble(input);
verbose(2, "%0.2f\n", bfl->inputVal);
if (doLog)
    bfl->inputVal = log10(bfl->inputVal + 1);
if (bedSize > 5)
    {
    // join (could be done quicker w/ custom code)
    bfl->text = 
        slNameListToString(
            slNameListFromStringArray(&words[5], bedSize-5), '\t');
    }
// DEBUG
// bflOut(bfl, stdout);
// fflush(stdout);
return bfl;
}

void bflFree(struct bedFileLine **bfl)
/* Free memory */
{
freeMem((*bfl)->text);
if ((*bfl)->bed5)
    {
    freeMem((*bfl)->bed5->name);
    freeMem((*bfl)->bed5->chrom);
    /* more to free from bed5 ? */
    }
freez(bfl);
}

boolean bflIsData(struct bedFileLine *bfl)
/* Determine if this is a BED line, or just fluff (empty or comment) */
{
return (bfl->bed5 != NULL);
}

void bflSetScore(struct bedFileLine *bfl, int score)
/* Update score in BED */
{
if (bflIsData(bfl))
    bfl->bed5->score = score;
}

void bflOut(struct bedFileLine *bfl, FILE *f)
/* Output a bed file line to opened file */
{
    if (bfl->bed5)
        {
        bedOutputN((struct bed *)bfl->bed5, 5, f, '\t', '\t');
        verbose(3, "inputVal = %f\t", bfl->inputVal);
        }
    if (bfl->text)
        fputs(bfl->text, f);
    fputs("\n", f);
}

/* Scoring methods */

struct scorer
/* Representation of scoring method and its local data */
    {
    char *name;         /* method identifer */
    void *info;         /* intermediate values needed by scorer */
    void (*inputValue)(struct scorer *scorer, double val);  
                        /* stash input value or otherwise process */
    void (*summarize)(struct scorer *scorer);    /* prep for rescoring after seeing all input */
    double (*outputValue)(struct scorer *scorer, double val);  /* rescore input value */
    };

/* encode method 

    This method was used the ENCODE pipeline (hgLoadBed -fillInScore), applied to signalValue
    of ENCODE datasets lacking browser scores.  It computes new score from the function:
    y = ax+b, where a is <score-range>/<input-range>.

    Author Larry Meyer (in hgLoadBed):

    float a = (1000-minScore) / (max - min);
    float b = 1000 - ((1000-minScore) * max) / (max - min);
    sqlSafef(query, sizeof(query),
        "update %s set score = round((%f * %s) + %f)",
            track, a, fillInScoreColumn, b);
*/


struct scoreEncodeInfo
    {
    double minVal, maxVal;
    double normFactor, normOffset;    // y = ax+b
    } scoreEncodeInfo = {MAXDOUBLE, 0, 0, 0};

void scoreEncodeIn(struct scorer *scorer, double inputVal)
/* Gather any info needed from each input value */
{
struct scoreEncodeInfo *info = scorer->info;
verbose(3, "inputVal=%f, oldMin=%f, oldMax=%f\t", inputVal, info->minVal, info->maxVal);
info->minVal = min(inputVal, info->minVal);
info->maxVal = max(inputVal, info->maxVal);
verbose(3, "min=%f, max=%f\n", info->minVal, info->maxVal);
}

void scoreEncodeSummarize(struct scorer *scorer)
/* Compute temporary values needed after seeing all input values and before rescoring.
   For this method, these are coefficients needed for scaling function, f(x) = ax + b.
  */
{
struct scoreEncodeInfo *info = scorer->info;
double min = info->minVal;
double max = info->maxVal;
info->normFactor = (maxScore-minScore) / (max - min);
info->normOffset = maxScore - ((maxScore-minScore) * max) / (max - min);
verbose(2, "  min=%f, max=%f, normFactor=%f, normOffset=%f\n", 
                min, max, info->normFactor, info->normOffset);
}

double scoreEncodeOut(struct scorer *scorer, double inputVal)
{
struct scoreEncodeInfo *info = scorer->info;
return (round((info->normFactor * inputVal) + info->normOffset));
}

/* reg method 

   This method is used by the hg/regulate suite to score TFBS peaks (in narrowPeak format)
   based signalValue.  Input signal values are multiplied by a normalization factor 
   calculated as the ratio of the maximum score value (1000) to the signal value 
   at 1 standard deviation from the mean, with values exceeding 1000 capped at 1000. 
   This has the effect of distributing scores up to mean + 1std across the score range, 
   but assigning all above to the max score.

   TODO: support for minScore != 0 ? And verify support for maxScore.

   Author Jim Kent (in regClusterBedExpCfg.c and regClusterMakeTableOfTables)
*/

struct scoreStdInfo
    {
    int count;
    double minVal, maxVal;
    double sum, sumSquares;
    double normFactor;
    } scoreStdInfo = {0, MAXDOUBLE, 0, 0, 0, 0};

void scoreStdIn(struct scorer *scorer, double inputVal)
/* Gather any info needed from each input value */
{
struct scoreStdInfo *info = scorer->info;
verbose(3, "inputVal=%f, oldMin=%f, oldMax=%f, oldSum=%f, oldSq = %f\t", 
            inputVal, info->minVal, info->maxVal, info->sum, info->sumSquares);
info->count++;
info->minVal = min(inputVal, info->minVal);
info->maxVal = max(inputVal, info->maxVal);
info->sum += inputVal;
info->sumSquares += (inputVal * inputVal);
verbose(3, "min=%f, max=%f, sum=%f, squares=%f\n", 
            info->minVal, info->maxVal, info->sum, info->sumSquares);
}

void scoreStdSummarize(struct scorer *scorer)
/* Compute temporary values needed after seeing all input values and before rescoring.
   For this method, this is the normalization factor, derived from mean and std.
  */
{
struct scoreStdInfo *info = scorer->info;
double std = calcStdFromSums(info->sum, info->sumSquares, info->count);
double mean = info->sum/info->count;
double highEnd = mean + std;
if (sameString(method, "std2"))
    highEnd += std;
if (highEnd > info->maxVal) highEnd = info->maxVal;     // needed ?
info->normFactor = ((double)maxScore)/highEnd;
verbose(2, "min=%f, max=%f, count=%d, std=%f, mean=%f, norm=%g\n", 
            info->minVal, info->maxVal, info->count, std, mean, info->normFactor);
}

double scoreStdOut(struct scorer *scorer, double inputVal)
/* Adjust scores and output file */
{
struct scoreStdInfo *info = scorer->info;
return (info->normFactor * inputVal);
}

/* asinh method 

   This method was used by the ENCODE Analysis Working Group to assign scores to the
   January 2011 freeze of ENCODE TFBS, hosted at the ENCODE Integrative Analysis Hub.

   Author: Steve Wilder (in regClusterBedExpCfg.c and regClusterMakeTableOfTables)

   Input signal values are transformed using the inverse trigonometric function, asinh
   (almost linear for small positive values, tending to logarithmic for large values),
   and taking the lower and upper quartile as the extreme values.

   i.e if signal values is x, Q1 is lower quartile of signal distribution, 
   Q3 is upper quartile of signal distribution:

        score = minScore if x <= Q1
              = maxScore if x >= Q3
              = minScore + (maxScore-minScore) * (asinh(x) - asinh(Q1)) / (asinh(Q3) - asinh(Q1))
                     if Q1 < x < Q3
*/

struct scoreAsinhInfo
    {
    int count;
    double q1, q3;
    double asinhQ1, asinhQ3;
    double normFactor;            /* (maxScore-minScore)/(asinh(Q3) - asinh(Q1)) */
    struct slDouble *inputValues; /* save input values if needed by scorer 
                                        (e.g. to compute quartiles).
                                        This will be populated with input values  as BED is read */
    } scoreAsinhInfo = {0, 0, 0, 0, 0, 0, NULL};

void scoreAsinhIn(struct scorer *scorer, double inputVal)
/* Gather any info needed from each input value */
{
struct scoreAsinhInfo *info = scorer->info;
verbose(3, "inputVal=%f", inputVal);
info->count++;
slAddHead(&info->inputValues, slDoubleNew(inputVal));
}

void scoreAsinhSummarize(struct scorer *scorer)
/* Compute temporary values needed after seeing all input values and before rescoring.
   For this method, this is the normalization factor, derived from mean and std.
  */
{
struct scoreAsinhInfo *info = scorer->info;
double min, median, max;
slDoubleBoxWhiskerCalc(info->inputValues, &min, &info->q1, &median, &info->q3, &max);
info->asinhQ1 = asinh(info->q1);
info->asinhQ3 = asinh(info->q3);
verbose(2, "min=%f, q1=%f, median=%f, q3=%f, max=%f, count=%d, asinhQ1=%f, asinhQ3=%f", 
            min, info->q1, median, info->q3, max, info->count, info->asinhQ1, info->asinhQ3);
info->normFactor = (maxScore - minScore) / (info->asinhQ3 - info->asinhQ1);
verbose(2, ", normFactor=%f", info->normFactor);
}

double scoreAsinhOut(struct scorer *scorer, double inputVal)
/* Adjust scores and output file */
{
struct scoreAsinhInfo *info = scorer->info;
if (inputVal <= info->q1)
    return(minScore);
if (inputVal >= info->q3)
    return (maxScore);
return minScore + (info->normFactor * (asinh(inputVal) - info->asinhQ1));
}

/* Scorer initialization */

static struct scorer scorers[] = {
    {scoreEncode, &scoreEncodeInfo, &scoreEncodeIn, &scoreEncodeSummarize, &scoreEncodeOut},
    {scoreReg, &scoreStdInfo, &scoreStdIn, &scoreStdSummarize, &scoreStdOut},
    {scoreStd2, &scoreStdInfo, &scoreStdIn, &scoreStdSummarize, &scoreStdOut},
    {scoreAsinh, &scoreAsinhInfo, &scoreAsinhIn, &scoreAsinhSummarize, &scoreAsinhOut},
    {NULL, NULL, NULL, NULL, NULL}
};

struct scorer *scorerNew(const char *method)
/* Locate scorer by name from array of all */
{
struct scorer *pScorer;
for (pScorer = scorers; pScorer->name; pScorer++) 
    if (sameString(method, pScorer->name))
        return pScorer;
errAbort("scorer %s not found", method);
return NULL;
}

/*
 * Here are code snippets and comments describing all 4 scoring methods 
 * (gleaned from various sources).  To be integrated (method=encode works as first case)
 */

#ifdef METHOD_lod
# calculate coefficients a and b.
my $b = ( ((300 * log($max)) - (1000 * log($med))) /
          (log($max) - log($med)) );
my $a = (1000 - $b) / log($max);

# Do the transform on the score column of saved input:
foreach my $wRef (@lines) {
  my @words = @{$wRef};
  if ($words[4] > 0) {
    $words[4] = int(($a * log($words[4])) + $b);
  }
#endif


void bedPreScoreFile(char *inFile)
/* Read input values and stash values needed for the method */
{
struct lineFile *in = lineFileOpen(inFile, TRUE);
char *line = NULL;
int lineNum = 0;
struct bedFileLine *bfl = NULL;

verbose(2, "Reading file '%s' into scorer '%s'\n", inFile, method);

// parse input file and let scorer have a look at each value
while (lineFileNext(in, &line, NULL))
    {
    lineNum++;
    bfl = bflNew(line, lineNum);
    if (bflIsData(bfl))
        scorer->inputValue(scorer, bfl->inputVal);
    bflFree(&bfl);
    }
verbose(2, "Found %d lines in BED file\n", lineNum);
lineFileClose(&in);
}

void bedScoreSummarize()
/* Compute normalize values after all input seen */
{
scorer->summarize(scorer);
}

void bedScoreFile(char *inFile, char *outFile)
/* Normalize scores and output */
{
struct lineFile *in = lineFileOpen(inFile, TRUE);
FILE *out = mustOpen(outFile, "w");
char *line = NULL;
struct bedFileLine *bfl = NULL;
int lineNum = 0;

verbose(2, "Writing file '%s'\n", outFile);

while (lineFileNext(in, &line, NULL))
    {
    lineNum++;
    bfl = bflNew(line, lineNum);
    if (bflIsData(bfl))
        {
        // data line, so transform it
        int score = scorer->outputValue(scorer, bfl->inputVal);
        score = max(minScore, score);
        score = min(maxScore, score);
        bflSetScore(bfl, score);
        verbose(3, "old score: %f, new score: %d\n", bfl->inputVal, score);
        }
    bflOut(bfl, out);
    }
carefulClose(&out);
lineFileClose(&in);
}

char *fileNameFromPath(char *path)
/* Strip directories from path and return file name */
{
char name[FILENAME_LEN];
char ext[FILEEXT_LEN];
char buf[FILENAME_LEN + FILEEXT_LEN + 1];

splitPath(path, NULL, name, ext);
safef(buf, sizeof(buf), "%s%s", &name[0], &ext[0]);
return (cloneString(buf));
}

void bedScore(int count, char *list[])
/* bedScore - Add scores to BED files. 
 *              If count == 2, file1 is input, file2 is output
 *              If count > 2, fileN is output dir, others are input files
 */
{
if (count == 2)
    {
    // single file
    char *inFile = list[0];
    char *outFile = list[1];
    bedPreScoreFile(inFile);
    bedScoreSummarize();
    bedScoreFile(inFile, outFile);
    return;
    }

// multiple files
char *outDir = list[count-1];
verbose(2, "Writing to output directory '%s'\n", outDir);
int i;
for (i = 0; i < count-1; i++)
    {
    char *filePath = list[i];
    if (uniform)
        {
        bedPreScoreFile(filePath);
        }
    else
        {
        bedPreScoreFile(filePath);
        bedScoreSummarize();
        char *fileName = fileNameFromPath(filePath);
        struct dyString *dy = dyStringCreate("%s/%s", outDir, fileName);
        bedScoreFile(filePath, dyStringCannibalize(&dy));
        }
    }
if (!uniform)
    return;

bedScoreSummarize();
for (i = 0; i < count-1; i++)
    {
    char *filePath = list[i];
    char *fileName = fileNameFromPath(filePath);
    struct dyString *dy = dyStringCreate("%s/%s", outDir, fileName);
    bedScoreFile(filePath, dyStringCannibalize(&dy));
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage("Too few arguments");
col = optionInt("col", col);
minScore = optionInt("minScore", minScore);
uniform = optionExists("uniform");
method = optionVal("method", method);
doLog = optionExists("log");
scorer = scorerNew(method);
bedScore(argc-1, &argv[1]);
return 0;
}
