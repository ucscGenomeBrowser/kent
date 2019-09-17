/* edwSamPairedEndStats - Calculate some paired end stats from a SAM file.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "hmmstats.h"

/* See http://samtools.sourceforge.net/SAMv1.pdf for description of SAM format. */

int maxInsert = 1000;
boolean showDiscordant = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSamPairedEndStats - Calculate some paired end stats from a SAM file.\n"
  "usage:\n"
  "   edwSamPairedEndStats in.sam out.ra\n"
  "options:\n"
  "   -maxInsert=N (default %d) - maximum distance between pairs\n"
  "   -showDiscordant - if true print (to stdout) info on discordant pairs\n"
  , maxInsert
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxInsert", OPTION_INT},
   {"showDiscordant", OPTION_BOOLEAN},
   {NULL, 0},
};

void edwSamPairedEndStats(char *inSam, char *outRa)
/* edwSamPairedEndStats - Calculate some paired end stats from a SAM file. */
{
struct lineFile *lf = lineFileOpen(inSam, TRUE);
char *line;
int readCount = 0;
int alignedCount = 0;
int pairedCount = 0;
double sum = 0;
double sumSquares = 0;
int distanceMin = 0, distanceMax = 0;
char lastReadName[256];
boolean lastCigarGood = FALSE;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '@')  // Skip header rows
        continue;
    ++readCount;

    /* Parse out fields we are interested in. */
    char *row[9];  // Only interested in first nine fields 
    int rowSize = chopLine(line, row);
    lineFileExpectAtLeast(lf, ArraySize(row), rowSize);
    char *readName = row[0];
    char *cigar = row[5];

    // We assume file is sorted so that read pairs are together.  From the first one in a pair we 
    // just figure out if the cigar is good and save the read name to insure our assumption about 
    // read pairs is right
    if ((readCount&1) == 1)  // First in pair
        {
	safef(lastReadName, sizeof(lastReadName), "%s", readName);
	lastCigarGood = (cigar[0] != '*');
	}
    else
	{
	if (!sameString(readName, lastReadName))
	    errAbort("error in %s: reads do not come in pairs, %s != %s on line %d", lf->fileName,
		readName, lastReadName, lf->lineIx);
	       
	if (lastCigarGood && cigar[0] != '*')   // Both sides align, yea!
	    {
	    ++alignedCount;
	    char *templateSizeString = row[8];
	    int templateSize = sqlSigned(templateSizeString);
	    int posSize = abs(templateSize);
	    if (posSize > 0 && posSize < maxInsert)  // Distance between pair is good
		{
		if (pairedCount == 0)
		    distanceMin = distanceMax = posSize;
		else
		    {
		    if (distanceMax < posSize) distanceMax = posSize;
		    if (distanceMin > posSize) distanceMin = posSize;
		    }
		pairedCount += 1;
		sum += posSize;
		sumSquares += posSize*posSize;
		}
	    else
		{
		if (showDiscordant)
		    printf("%s line %d of %s is discordant\n", readName, lf->lineIx, lf->fileName);
		}
	    }
	}
    }
lineFileClose(&lf);

double distanceMean = 0, distanceStd=0, concordance=0;

if (pairedCount != 0)
    {
    /* Calculate statistics. */
    distanceMean = sum/pairedCount;
    distanceStd = calcStdFromSums(sum, sumSquares, pairedCount);
    concordance = (double)pairedCount/(double)alignedCount;
    }


/* Write out results */
FILE *f = mustOpen(outRa, "w");
fprintf(f, "concordance %g\n", concordance);
fprintf(f, "distanceMean %g\n", distanceMean);
fprintf(f, "distanceStd %g\n", distanceStd);
fprintf(f, "distanceMin %d\n", distanceMin);
fprintf(f, "distanceMax %d\n", distanceMax);
carefulClose(&f);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxInsert = optionInt("maxInsert", maxInsert);
showDiscordant = optionExists("showDiscordant");
edwSamPairedEndStats(argv[1], argv[2]);
return 0;
}
