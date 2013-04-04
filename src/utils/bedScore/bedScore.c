/* bedScore - Add scores to a BED file or transform existing scores. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "basicBed.h"
#include <values.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedScore - Add scores to a BED file or transform existing scores\n"
  "usage:\n"
  "   bedScore input.bed output.bed\n"
  "options:\n"
  "   -col=N      - Column to use as input value (default 5)\n"
  "   -minScore=N - Minimum score to assign (default 0)\n"
  "   -method=\n"
  "             encode - ENCODE pipeline (UCSC) (default)\n"
  "             reg - Regulatory clusters (UCSC) \n"
  "             lod - Phastcons log odds (Adam Siepel, Cornell)\n"
  "             asinh - ENCODE Uniform TFBS (Steve Wilder, ENCODE AWG/EBI)\n"
  );
}

/* Globals and defaults */
int minScore = 0;
int col = 5;
char *method = "encode";

/* Command line validation table. */
static struct optionSpec options[] = {
   {"col", OPTION_INT},
   {"method", OPTION_STRING},
   {"minScore", OPTION_INT},
   {NULL, 0}
};


/* Capture the BED in BED5ish fashion, but retain comments and empty lines
 * The data lines will be formatted tab-sep, so will lose original whitespace sep
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
verbose(2, "%s\n", line);
wordCount = chopByWhite(line, words, sizeof(words));
if (bedSize == 0)
    {
    verbose(2, "wordcount=%d\n", wordCount);
    if (wordCount < 5 || wordCount < col)
        errAbort("Unexpected number of fields in BED file: %d", wordCount);
    bedSize = wordCount;
    }
if (wordCount != bedSize)
    errAbort("Unexpected number of fields in line #%d of BED %d file: %d", 
                    lineNum, bedSize, wordCount);
bfl->bed5 = (struct bed5 *)bedLoad5(words);
bfl->inputVal = sqlDouble(words[col-1]);
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

/* Scoring methods */

/* Globals for scorers.  Will clean this up later */
static struct slDouble *inputVals = NULL;      // list of input values to transform
static double maxVal = 0, minVal = MAXDOUBLE;

void scorerAddInputValue(double inputVal)
/* Add input value to list, gathering stats as needed by scoring method */
{
// TODO

maxVal = max(inputVal, maxVal);
minVal = min(inputVal, minVal);

slAddHead(&inputVals, slDoubleNew(inputVal));
}

void scorerWriteBedFile(struct bedFileLine *bfls, FILE *f)
/* Output all lines in BED, with scores (re)calculated */
// TODO
{
struct bedFileLine *bfl;
float a = (1000-minScore) / (maxVal - minVal);
float b = 1000 - ((1000-minScore) * maxVal) / (maxVal - minVal);
verbose(2, "min=%f, max=%f, a=%f, b=%f\n", minVal, maxVal, a, b);
for (bfl = bfls; bfl != NULL; bfl = bfl->next)
    {
    if (bfl->bed5)
        {
        // encode method only as first cut
        bfl->bed5->score = round((a * bfl->inputVal) + b);
        verbose(2, "old score: %f, new score: %d\t", bfl->inputVal, bfl->bed5->score);
        }
    bflOut(bfl, f);
    }
}

/*
 * Here are code snippets and comments describing all 4 scoring methods 
 * (gleaned from various sources).  To be integrated (method=encode works as first case)
 */

#ifdef METHOD_encode
float a = (1000-minScore) / (max - min);
float b = 1000 - ((1000-minScore) * max) / (max - min);
safef(query, sizeof(query),
    "update %s set score = round((%f * %s) + %f)",
            track, a, fillInScoreColumn, b);
#endif

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

#ifdef METHOD_asinh
For ENCODE, I set sensible minimum and maximum values, transforming the signal value (seventh column) with the asinh function (almost linear for small positive values, tending to logarithmic for large values), and taking the lower and upper quartile as the extreme values.

 i.e if signal values is x, Q1 is lower quartile of signal distribution, Q3 is upper quartile of signal distribution

score = 370 if x <= Q1
           = 1000 if x>=Q3
            = 370 + 630 * (asinh(x) - asinh(Q1)) / (asinh(Q3) - asinh(Q1))   if Q1 < x < Q3
#endif

#ifdef METHOD_reg
double calcNormScoreFactor(char *fileName, int scoreCol)
/* Figure out what to multiply things by to get a nice browser score (0-1000) */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[scoreCol+1];
double sum = 0, sumSquares = 0;
int n = 0;
double minVal=0, maxVal=0;
int fieldCount;
while ((fieldCount = lineFileChop(lf, row)) != 0)
    {
    lineFileExpectAtLeast(lf, scoreCol+1, fieldCount);
    double x = sqlDouble(row[scoreCol]);
    if (n == 0)
        minVal = maxVal = x;
    if (x < minVal) minVal = x;
    if (x > maxVal) maxVal = x;
    sum += x;
    sumSquares += x*x;
    n += 1;

double std = calcStdFromSums(sum, sumSquares, n);
double mean = sum/n;
double highEnd = mean + std;
if (highEnd > maxVal) highEnd = maxVal;
return 1000.0/highEnd;
}
#endif

void bedScore(char *inFile, char *outFile)
/* bedScore - Add scores to a BED file. */
{
struct lineFile *in = lineFileOpen(inFile, TRUE);
FILE *out = mustOpen(outFile, "w");
char *line = NULL;
int lineNum = 0;
struct bedFileLine *bfl, *bedFileLines = NULL;   // list of lines in BED file

verbose(2, "Reading %s\n", in->fileName);
while (lineFileNext(in, &line, NULL))
    {
    // parse input file into a list of bedFileLines
    lineNum++;
    bfl = bflNew(line, lineNum);
    scorerAddInputValue(bfl->inputVal);
    slAddHead(&bedFileLines, bfl);
    }
// (re)score beds and write to output file
slReverse(&bedFileLines);
scorerWriteBedFile(bedFileLines, out);
carefulClose(&out);
lineFileClose(&in);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage("Wrong number of arguments");
col = optionInt("col", col);
minScore = optionInt("minScore", minScore);
method = optionVal("method", method);
bedScore(argv[1], argv[2]);
return 0;
}
