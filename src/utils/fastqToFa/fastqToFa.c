/* fastqToFa - Convert from fastq to fasta format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: fastqToFa.c,v 1.2 2008/11/10 18:15:56 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastqToFa - Convert from fastq to fasta format.\n"
  "usage:\n"
  "   fastqToFa [options] in.fastq out.fa\n"
  "options:\n"
  "   -nameVerify='string' - for multi-line fastq files, 'string' must\n"
  "	match somewhere in the sequence names in order to correctly\n"
  "	identify the next sequence block (e.g.: -nameVerify='Supercontig_')\n"
  "   -qual=file.qual.fa - output quality scores to specifed file\n\t(default: quality scores are ignored)\n"
  "   -qualSizes=qual.sizes - write sizes file for the quality scores\n"
  "   -noErrors - warn only on problems, do not error out\n"
  "              (specify -verbose=3 to see warnings\n"
  "   -solexa - use Solexa/Illumina quality score algorithm\n\t(instead of Phread quality)\n"
  "   -verbose=2 - set warning level to get some stats output during processing"
  );
}

static char *qualFile = NULL;
static char *qualSizes = NULL;
static char *nameVerify = NULL;
static boolean solexa = FALSE;
static boolean noErrors = FALSE;

static struct optionSpec options[] = {
   {"qual", OPTION_STRING},
   {"qualSizes", OPTION_STRING},
   {"nameVerify", OPTION_STRING},
   {"solexa", OPTION_BOOLEAN},
   {"noErrors", OPTION_BOOLEAN},
   {NULL, 0},
};

void fastqToFa(char *inFastq, char *outFa)
/* fastqToFa - Convert from fastq to fasta format.. */
{
struct lineFile *lf = lineFileOpen(inFastq, TRUE);
FILE *f = mustOpen(outFa, "w");
char *line;
boolean inSequence = FALSE;
long long seqCount = 0;
long long totalSize = 0;
boolean readingOk = lineFileNextReal(lf, &line);
char seqNameNote[128];
FILE *qFH = NULL;
FILE *qSZ = NULL;

if (qualFile)
    qFH = mustOpen(qualFile, "w");
if (qualSizes)
    qSZ = mustOpen(qualSizes, "w");

// the first line has already been read upon entry here, and thus
// each time coming back to the top of this while() loop, the sequence
// name line has been read and is ready to process.

while (readingOk)
    {
    line = skipLeadingSpaces(line);
    if (line[0] != '@')
        errAbort("ERROR: %s doesn't seem to be fastq format.  "
		"Expecting '@' start of line %d, got %c.",
		lf->fileName, lf->lineIx, line[0]);
    if (nameVerify && (! stringIn(nameVerify, line+1)))
	errAbort("ERROR: expecting to find '%s' in sequence identifier\n\t"
	    "\tfound '%s' at line %d of %s\n",
		nameVerify, line+1, lf->lineIx, lf->fileName);
    ++seqCount;
    fprintf(f, ">%s\n", line+1);	// starting a sequence
    safef(seqNameNote, ArraySize(seqNameNote), "%s", line+1); // save name
    inSequence = TRUE;
    long long dnaSize = 0;	// counting the size of this one sequence
    while (inSequence)
	{
	readingOk = lineFileNextReal(lf, &line);
	if (!readingOk)
	   lineFileUnexpectedEnd(lf); 
	switch (line[0])	// check first character of line
	    {
	    case '+':		// seeing + enters quality scores
		inSequence = FALSE;
		break;
	    case '@':		// seeing next sequence already is an error
		errAbort("ERROR: expecting '+', got '@' at line %d of %s",
		    lf->lineIx, lf->fileName);
		break;
	    default:		// anything else is assumed to be sequence
		dnaSize += strlen(line);
		fprintf(f, "%s\n", line);
		break;
	    }
	}
    if (dnaSize < 1)
	errAbort("ERROR: expecting DNA sequence, got nothing at line %d of %s",
		lf->lineIx, lf->fileName);
    if (inSequence)	// I don't think this can actually happen here
	errAbort("ERROR: expecting '+', found end of file at line %d of %s",
	    lf->lineIx, lf->fileName);
    totalSize += dnaSize;
    long long qualitySize = 0;
    int retSize;
    readingOk = lineFileNext(lf, &line, &retSize);
    if (qFH)
	fprintf(qFH, ">%s\n", seqNameNote);
    boolean onNewline = TRUE;
    while (readingOk)
	{
	if ('@' == line[0])
	    {
	    if (!nameVerify)
		break;		// found next sequence
	    if (stringIn(nameVerify, line+1))
		break;		// found next sequence
	    }
	// anything else is considered to be quality score
	int lineLength = strlen(line);
	if (qFH)
	    {
	    int i;
	    int qualValue = ((int)(line[0])) - 33;
	    if (onNewline)
		fprintf(qFH, "%d", qualValue);	// first score only
	    else
		fprintf(qFH, " %d", qualValue);	// all subsequent scores
	    onNewline = FALSE;
	    qualitySize += 1;
	    for (i = 1; i < lineLength; ++i)
		{
		qualitySize += 1;
		int qualValue = ((int)(line[i])) - 33;
		if (solexa)
		    {
		    qualValue =
			10*log(1+pow(10,(((int)(line[i]))-64)/10.0))/log(10);
		    }
if (qualValue > 127)
    errAbort("ERROR: illegal quality score at character %d on "
	"line %d of %s\n\tfor sequence '%s'",
		i, lf->lineIx, lf->fileName, seqNameNote);
#define COUNTS_PER_LINE	26
		if (0 == qualitySize % COUNTS_PER_LINE)
		    fprintf(qFH, "\n%d", qualValue);
		else
		    fprintf(qFH, " %d", qualValue);  // next ones have space
		}
	    }
	    else
	    {
	    qualitySize += lineLength;
	    }
	readingOk = lineFileNext(lf, &line, &retSize);
	}
    if (qFH)	// the above loop *always* finishes needing a new line
	{
	fprintf(qFH, "\n");
	onNewline = TRUE;
    }
    if (qualitySize < 1)
	{
	if (noErrors)
	    verbose(3,"ERROR: expecting quality scores, got nothing at "
		"line %d of %s\n\tfor sequence '%s'",
		lf->lineIx, lf->fileName, seqNameNote);
	else
	    errAbort("ERROR: expecting quality scores, got nothing at "
		"line %d of %s\n\tfor sequence '%s'",
		lf->lineIx, lf->fileName, seqNameNote);
	}
    if (dnaSize != qualitySize)
	{
	if (noErrors)
	    verbose(3,"ERROR: number of quality scores %lld does not match "
		"sequence length %lld\n\tat line %d of %s\n"
		"\tfor sequence '%s'",
		qualitySize, dnaSize, lf->lineIx, lf->fileName, seqNameNote);
	else
	    errAbort("ERROR: number of quality scores %lld does not match "
		"sequence length %lld\n\tat line %d of %s"
		"\tfor sequence '%s'\n",
		qualitySize, dnaSize, lf->lineIx, lf->fileName, seqNameNote);
	}
    if (qSZ)
	{
	fprintf(qSZ, "%s\t%lld\n", seqNameNote, qualitySize);
	if (0 == seqCount % 1000)
	    fflush(qSZ);
	}
    }
carefulClose(&f);
if (qFH)
    carefulClose(&qFH);
if (qSZ)
    carefulClose(&qSZ);

verbose(2, "#\tprocessed %lld sequences with total length of %lld bases\n",
	seqCount, totalSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
qualFile = optionVal("qual", NULL);
qualSizes = optionVal("qualSizes", NULL);
nameVerify = optionVal("nameVerify", NULL);
solexa = optionExists("solexa");
noErrors = optionExists("noErrors");

if (nameVerify)
    verbose(2,"#\tverify sequence names contain the string '%s'\n", nameVerify);
else
    verbose(2,"#\tno name checks will be made on lines beginning with @\n");

if (qualSizes)
    verbose(2,"#\toutput sequence name and quality scores to '%s'\n",qualSizes);

if (qualFile)
    verbose(2,"#\toutput quality scores to '%s'\n", qualFile);
else
    {
    verbose(2,"#\tignore quality scores\n");
    if (solexa)
	errAbort("ERROR: specified solexa quality score algorithm,\n"
		"\tbut no quality file output");
    }
if (solexa)
    verbose(2,"#\tusing solexa quality score algorithm\n");
else
    verbose(2,"#\tusing default Phread quality score algorithm\n");
if (noErrors)
    verbose(2,"#\tissue warnings on errors and continue\n#\t(specify -verbose=3 to see warnings)\n");
else
    verbose(2,"#\tall errors will cause exit\n");

if (argc != 3)
    usage();

fastqToFa(argv[1], argv[2]);
return 0;
}
