/* regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "hmmstats.h"
#include "errabort.h"


boolean encodeList = FALSE;
boolean tabList = FALSE;
boolean useTarget = FALSE;
char *cellLetter = NULL;
int scoreCol = 7;
boolean noLetter = FALSE;
boolean noLetterOk = FALSE;
boolean noNormalize = FALSE;

struct hash *cellLetterHash;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.\n"
  "usage:\n"
  "   regClusterBedExpCfg inputFileList output.cfg\n"
  "options:\n"
  "   -cellLetter=file.tab - two column file of form <letter> <name> for cell lines\n"
  "           If not present the first letter of the cell name will be used\n"
  "   -tabList - the inputFileList is in three or four column format of form:\n"
  "           <fileName> <cell> <factor-antibody> [factor-target]\n"
  "   -useTarget - cluster on target (must have 4 column tabList)\n"
  "   -encodeList - the inputFileList is of format you might get from cut and paste of\n"
  "        encode downloads page - tab separated with following columns:\n"
  "             <relDate> <fileName> <fileSize> <submitDate> <metadata>\n"
  "        where the metadata component is in the format:\n"
  "              this=that; that=two words; that=whatever\n"
  "        and the antibody and cell tags in the metadata are used\n"
  "   -scoreCol=N - The column (starting with 1) with score.  5 for bed, 7 for narrowPeak\n"
  "        default %d\n"
  "   -noLetter - just list cell types found in the inputFileList that lack a code in the cellLetter file\n"
  "   -noLetterOk - use first letter of cell name (lower-cased) if not found in cell letter file. Strips trailing + qualifiers in cell name before looking up in cell letter file.\n"
  "   -noNormalize - skip normalization, set norm value to 1 and scoreCol to 4 (score) in cfg file\n"
  , scoreCol
  );
}

static struct optionSpec options[] = {
   {"encodeList", OPTION_BOOLEAN},
   {"tabList", OPTION_BOOLEAN},
   {"useTarget", OPTION_BOOLEAN},
   {"cellLetter", OPTION_STRING},
   {"scoreCol", OPTION_INT},
   {"noLetter", OPTION_BOOLEAN},
   {"noLetterOk", OPTION_BOOLEAN},
   {"noNormalize", OPTION_BOOLEAN},
   {NULL, 0},
};

char *cellAbbrevDefault(char *cell, boolean toLower)
/* Return default abbreviation of cell-name */
{
static char buf[2];
buf[0] = (toLower ? tolower(cell[0]) : cell[0]);
buf[1] = 0;
return buf;
}

char *cellAbbreviation(char *cell)
/* Return abbreviated version of cell-name */
{
if (cellLetterHash == NULL)
    return cellAbbrevDefault(cell, FALSE);

if (noLetterOk)
    {
    // strip qualifiers (follow the '+' char)
    char *plus = stringIn("+", cell);
    if (plus)
        *plus = 0;
    }
char *val = hashFindVal(cellLetterHash, cell);
if (val != NULL)
    return val;

if (noLetterOk)
    return cellAbbrevDefault(cell, TRUE);

if (noLetter)
    uglyf("cell %s isn't in %s\n", cell, cellLetter);
else
    errAbort("cell %s isn't in %s\n", cell, cellLetter);
return NULL;
}

int commonPrefixSize(struct slName *list)
/* Return length of common prefix */
{
if (list == NULL)
    return 0;
int commonSize = strlen(list->name);
struct slName *el, *lastEl = list;
for (el = list->next; el != NULL; el = el->next)
    {
    int sameSize = countSame(el->name, lastEl->name);
    commonSize = min(sameSize, commonSize);
    lastEl = el;
    }
return commonSize;
}

int countSameAtEnd(char *a, char *b)
/* Count number of characters at end of strings that are same in each string. */
{
int count = 0;
char *aEnd = a + strlen(a);
char *bEnd = b + strlen(b);
while  (--aEnd >= a && --bEnd >= b)
    {
    if (*aEnd != *bEnd)
        break;
    ++count;
    }
return count;
}

int commonSuffixSize(struct slName *list)
/* Return length of common suffix */
{
if (list == NULL)
    return 0;
int commonSize = strlen(list->name);
struct slName *el, *lastEl = list;
for (el = list->next; el != NULL; el = el->next)
    {
    int sameSize = countSameAtEnd(el->name, lastEl->name);
    commonSize = min(sameSize, commonSize);
    lastEl = el;
    }
return commonSize;
}

void camelParseTwo(char *in, char **retA, char **retB)
/* Parse out CamelCased in into a and b.  */
{
char *s = in;
char *aStart = s;
char *bStart = NULL;
char c;
while ((c = *(++s)) != 0)
    {
    if (isupper(c))
        {
	bStart = s;
	break;
        }
    }
if (bStart == NULL)
   errAbort("Couldn't find start of second word in %s", in);
*retA = cloneStringZ(aStart, bStart - aStart);
*retB = cloneString(bStart);
}

double calcNormScoreFactor(char *fileName, int scoreCol)
/* Figure out what to multiply things by to get a nice browser score (0-1000) */
{
if (noNormalize)
    return 1.0;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[scoreCol+1];
double sum = 0, sumSquares = 0;
int n = 0;
double minVal=0, maxVal=0;
while (lineFileRow(lf, row))
    {
    double x = sqlDouble(row[scoreCol]);
    if (n == 0)
        minVal = maxVal = x;
    if (x < minVal) minVal = x;
    if (x > maxVal) maxVal = x;
    sum += x;
    sumSquares += x*x;
    n += 1;
    }
lineFileClose(&lf);
double std = calcStdFromSums(sum, sumSquares, n);
double mean = sum/n;
double highEnd = mean + std;
if (highEnd > maxVal) highEnd = maxVal;
return 1000.0/highEnd;
}

void makeConfigFromFileList(char *input, char *output)
/* makeConfigFromFileList - Create config file for hgBedsToBedExps from list of files.. */
{
FILE *f = mustOpen(output, "w");
struct slName *in, *inList = readAllLines(input);
int commonPrefix = commonPrefixSize(inList);
int commonSuffix = commonSuffixSize(inList);
for (in = inList; in != NULL; in = in->next)
    {
    char *s = in->name;
    int len = strlen(s);
    char *midString = cloneStringZ(s+commonPrefix, len - commonPrefix - commonSuffix);
    char *factor, *cell;
    camelParseTwo(midString, &cell, &factor);
    fprintf(f, "%s\t%s\t", factor, cell);
    fprintf(f, "%s\t", cellAbbreviation(cell));
    fprintf(f, "file\t%d\t", scoreCol-1);
    fprintf(f, "%g\t", calcNormScoreFactor(in->name, scoreCol-1));
    fprintf(f, "%s\n", in->name);
    }
carefulClose(&f);
}

void makeConfigFromTabList(char *input, char *output, boolean useTarget)
/* makeConfigFromFileList - Create config file for hgBedsToBedExps from list of file/cell/ab
     or file/cell/ab/target. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *row[4];
FILE *f = mustOpen(output, "w");

while (lineFileRow(lf, row))
    {
    char *fileName = row[0];
    char *cell = row[1];
    char *factor = row[2];
    verbose(3, "%s\n", fileName);
    if (useTarget)
        // 4 column input file -- output target cell+treatment+factor
        fprintf(f, "%s\t%s+%s\t", row[3], cell, factor);
    else
        // antibody cell+treatment
        fprintf(f, "%s\t%s", factor, cell);
    fprintf(f, "\t%s\t", cellAbbreviation(cell));
    fprintf(f, "file\t%d\t", scoreCol-1);
    fprintf(f, "%g\t", calcNormScoreFactor(fileName, scoreCol-1));
    fprintf(f, "%s\n", fileName);
    }
lineFileClose(&lf);
carefulClose(&f);
}

void makeConfigFromEncodeList(char *input, char *output)
/* create config file for hgBedsToBedExps from tab-separated file of format
 *         <relDate> <fileName> <fileSize> <submitDate> <metadata> */
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;

while (lineFileNextReal(lf, &line))
    {
    /* Parse out line into major components. */
    char *releaseDate = nextWord(&line);
    char *fileName = nextWord(&line);
    char *fileSize = nextWord(&line);
    char *submitDate = nextWord(&line);
    char *metadata = trimSpaces(line);
    if (isEmpty(metadata))
        errAbort("line %d of %s is truncated", lf->lineIx, lf->fileName);

    verbose(2, "releaseDate=%s; fileName=%s; fileSize=%s; submitDate=%s; %s\n", 
    	releaseDate, fileName, fileSize, submitDate, metadata);


    /* Loop through metadata looking for cell and antibody.  Metadata
     * is in format this=that; that=two words; that=whatever */
    char *cell = NULL, *antibody = NULL;
    for (;;)
        {
	/* Find terminating semicolon if any replace it with zero, and
	 * note position for next time around loop. */
	metadata = skipLeadingSpaces(metadata);
	if (isEmpty(metadata))
	    break;
	char *semi = strchr(metadata, ';');
	if (semi != NULL)
	   *semi++ = 0;

	/* Parse out name/value pair. */
	char *name = metadata;
	char *value = strchr(metadata, '=');
	if (value == NULL)
	   errAbort("Missing '=' in metadata after tag %s in line %d of %s", 
	   	name, lf->lineIx, lf->fileName);
	*value++ = 0;
	name = trimSpaces(name);
	value = trimSpaces(value);

	/* Look for our tags. */
	if (sameString(name, "cell"))
	    cell = value;
	else if (sameString(name, "antibody"))
	    antibody = value;

	metadata = semi;
	}
    if (cell == NULL) 
        errAbort("No cell in metadata line %d of %s", lf->lineIx, lf->fileName);
    if (antibody == NULL) 
        errAbort("No antibody in metadata line %d of %s", lf->lineIx, lf->fileName);

    fprintf(f, "%s\t%s\t", antibody, cell);
    fprintf(f, "%s\t", cellAbbreviation(cell));
    fprintf(f, "file\t%d\t", scoreCol-1);
    fprintf(f, "%g", calcNormScoreFactor(fileName, scoreCol-1));
    fprintf(f, "\t%s\n", fileName);
    }
carefulClose(&f);
}

void regClusterBedExpCfg(char *input, char *output)
/* regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.. */
{
if (cellLetter)
    cellLetterHash = hashTwoColumnFile(cellLetter);
if (encodeList)
    makeConfigFromEncodeList(input, output);
else if (tabList)
    makeConfigFromTabList(input, output, useTarget);
else
    makeConfigFromFileList(input, output);
}
 
int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encodeList = optionExists("encodeList");
tabList = optionExists("tabList");
useTarget = optionExists("useTarget");
cellLetter = optionVal("cellLetter", cellLetter);
scoreCol = optionInt("scoreCol", scoreCol);
noLetter = optionExists("noLetter");
noLetterOk = optionExists("noLetterOk");
noNormalize = optionExists("noNormalize");
if (noNormalize)
    // standard score column index added to config file
    scoreCol = 5;
else
    {
    verbose(2, "Normalizing score using column %d\n", scoreCol);
    }
regClusterBedExpCfg(argv[1], argv[2]);
return 0;
}
