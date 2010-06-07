/* consForBed.c - Program to read in bed files and conservation files
   and output the conservation for each base in the bed files. */
#include "common.h"
#include "linefile.h"
#include "bed.h"
#include "options.h"
#include "obscure.h"
#include "memalloc.h"
#define FLOAT_CHEAT 10000.0 /* Trick for storing floats with limited percision in ints. */

void usage() 
/* Print usage and quit. */
{
errAbort("consForBed - Takes a bed file and a conservation file and outputs\n"
	 "conservation scores for each position in the bed file. Optionally\n"
	 "outputs a summary file which contains every conservation value seen\n"
	 "usage:\n   "
	 "consForBed -chrom=chr1 -chromSize=246127941 -consFile=phyloHmm.tab \\\n"
	 "     -bedFile=bedFile.bed -bedConsOut=bedCons.tab -summary=summaryOut\n");
}

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"chrom", OPTION_STRING},
    {"chromSize", OPTION_INT},
    {"consFile", OPTION_STRING},
    {"bedConsOut", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"summary", OPTION_STRING},
    {NULL, 0}
};

int *readInConservationVals(char *fileName)
/* Open up the file and read in the conservation scores.
   return an array indexed by base position with the conservation
   scores. Free with freez() */
{
struct lineFile *lf = NULL;
int *scores = NULL;
int chromSize = optionInt("chromSize", 0);
int i = 0;
char *words[2];
if(chromSize == 0)
    errAbort("Can't have chromSize set to 0.");
warn("Reading in conservation");
setMaxAlloc(sizeof(*scores)*chromSize+1);
AllocArray(scores, chromSize);

/* Make empty data be -1, a not possible score. */
for(i = 0; i < chromSize; i++)
    scores[i] = -1;

/* Open up our conservation file. */
if(sameString(fileName, "stdin"))
    lf = lineFileStdin(TRUE);
else
    lf = lineFileOpen(fileName, TRUE);

dotForUserInit( chromSize/10 > 1 ? chromSize/10 : 1);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    scores[atoi(words[0])] = round(atof(words[1]) * FLOAT_CHEAT);
    dotForUser();
    }
lineFileClose(&lf);
warn("Done");
return scores;
}

void outputBedConservation(struct bed *bed, int *probs, FILE *bedOut, FILE *summaryOut)
/* Output the conservation of each base in a bed. */
{
int i = 0;
fprintf(bedOut, "%s", bed->name);
if(bed->strand[0] == '+')
    {
    for(i = bed->chromStart; i < bed->chromEnd; i++)
	{
	fprintf(bedOut, "\t%.4f", probs[i] / FLOAT_CHEAT);
	if(summaryOut != NULL)
	    fprintf(summaryOut, "%.4f\n", probs[i]/ FLOAT_CHEAT);
	}
    }

else if(bed->strand[0] == '-')
    {
    for(i = bed->chromEnd - 1; i >= bed->chromStart; i--)
	{
	fprintf(bedOut, "\t%.4f", probs[i]/ FLOAT_CHEAT);
	if(summaryOut != NULL)
	    fprintf(summaryOut, "%.4f\n", probs[i]/FLOAT_CHEAT);
	}
    }
else
    errAbort("Don't recognize strand %s in bed %s.", bed->strand, bed->name);
fprintf(bedOut, "\n");
}

void consForBed() 
/* Open and read the bed file. Load consFile into an double 
   array for easy access and process. */
{
char *bedFileName = NULL;
char *chrom = NULL;
struct bed *bedList = NULL, *bed = NULL;
char *consFileName = NULL;
int *consProb = NULL;

char *consBedName = NULL;
FILE *consBedOut = NULL;
char *summaryBedName = NULL;
FILE *summaryBedOut = NULL;

/* Get the output file names. */
consBedName = optionVal("bedConsOut", NULL);
if(consBedName == NULL)
    errAbort("Must specify an output file for bed conservation.");

summaryBedName = optionVal("summary", NULL);

/* What chromosome are we on? */
chrom = optionVal("chrom", NULL);
if(chrom == NULL)
    errAbort("Must specify a chromosome.");

/* read in the beds. */
warn("Reading in beds.");
bedFileName = optionVal("bedFile", NULL);
if(bedFileName != NULL)
    bedList = bedLoadAll(bedFileName);
else
    errAbort("Must specify a bedFile.\n");

/* Read in the conservation scores. */
consFileName = optionVal("consFile", NULL);
if(consFileName != NULL)
    consProb = readInConservationVals(consFileName);
else
    errAbort("Must specify a conservation file.");

/* Open output files */
consBedOut = mustOpen(consBedName,"w");
if(summaryBedName != NULL)
    summaryBedOut = mustOpen(summaryBedName, "w");

/* Process each individual bed. */
warn("Writing out conservation for beds.");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(differentString(chrom, bed->chrom))
	continue;
    outputBedConservation(bed, consProb, consBedOut, summaryBedOut);
    }
warn("Cleaning up");
carefulClose(&consBedOut);
carefulClose(&summaryBedOut);
freez(&consProb);
warn("Done.");
}



int main(int argc, char* argv[])
/* Everybody's favorite function. */
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
consForBed();
return 0;
}
