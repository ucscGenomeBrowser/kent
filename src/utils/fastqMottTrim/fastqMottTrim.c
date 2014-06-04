/* fastqMottTrim - mott. 
 * Applies Richard Mott's trimming algorithm to the 
 * three prime end of each sequence. 
 * Cuttoff is the lowest desired quality score in phred scores, 
 * Set by default to a 50%  chance of mismatch. 
 * Base corresponds to any additions to the ASCII quality scheme. 
 * Minlength specifies the minimum sequence length for the output. 
 * For paired and unpaired data. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "fq.h"

int clMinLength = 30;
boolean clIsIllumina = FALSE;
int clCutoff = 3;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastqMottTrim - Applies Mott's trimming algorithm to a fastq file.\n"
  "Trims the 3 prime end based on cumulative quality \n"
  "usage:\n"
  "   fastqMottTrim input.fq output.fq\n"
  "for paired end use; \n"
  "   fastqMottTrim pair1.fq pair2.fq output1.fq output2.fq \n"
  "options:\n"
  " -minLength = int The minimum length allowed for a trimmed sequence, the default is 30\n"
  " -isIllumina bool TRUE for illumina, FALSE for Sanger. Sanger is the default \n"
  " -cutoff = int The lowest desired phred score, the default is 3.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"minLength",OPTION_INT},
   {"isIllumina",OPTION_BOOLEAN},
   {"cutoff",OPTION_INT},
   {NULL,0},
};


boolean  mottTrim(struct fq *input, int minLength, boolean isIllumina, int cutoff)
/* Applies mott's trimming algorithm to the fastq input. 
 * Trims from the 3 prime end based on 
 * The sum of (quality score - cutoff value ). 
 * Returns true if the trimmed sequence is larger than the minimum sequence length. */
{
int base = 33;
int index = -1;
long minValue = 100000000;
long scoreValue = 0;
if(isIllumina)
    base = 64;
int len = strlen(input->dna);
int i = len - 1;
for(; i >= 0; --i)
    {
    int qualScore = input->quality[i] - base;
    scoreValue += (qualScore - cutoff);
    /* Convert the quality scores to their ascii values. 
     * Calculate the sum of the (quality score - cutoff value)'s. */
    if(scoreValue < minValue)
        {
        minValue = scoreValue;
        index = i;
        }
        /* Modify the fastq fields to be the trimmed length. */
    }
if(minValue <= cutoff)
    {
    input->dna[index] = '\0';
    input->quality[index] = '\0';
    }  
if(strlen(input->dna) >= minLength)
    {
    return(TRUE);
    }
return(FALSE);
}

void trimPairedFastqFiles(char *input, char *output,
            char *input2, char *output2, int minLength, boolean isIllumina, int cutoff )
/* Goes through fastq sequences in a fastq file; 
 * Parses, stores, mottTrims,  prints, then frees each fastq sequence. 
 * For paired data. */
{
FILE *f = mustOpen(output, "w");
FILE *f2 = mustOpen(output2, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
struct lineFile *lf2 = lineFileOpen(input2, TRUE);
struct fq *fq;
struct fq *fq2;
while ((fq = fqReadNext(lf)) != NULL && (fq2 = fqReadNext(lf2)) != NULL)
    {
    if( mottTrim(fq, minLength, isIllumina, cutoff) && mottTrim(fq2, minLength, isIllumina, cutoff))
        /* For paired data both reads must pass the minimum cutoff length.*/
	{
	fqWriteNext(fq, f);
        fqWriteNext(fq2, f2);
	}
    fqFree(&fq);
    fqFree(&fq2);
    }
carefulClose(&f);
carefulClose(&f2);
}



void trimSingleFastqFile(char *input, char *output, int minLength, boolean isIllumina, int cutoff )
/* Goes through fastq sequences in a fastq file; 
 * Parses, stores, mottTrims,  prints, then frees each fastq sequence. */
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
struct fq *fq;
while ((fq = fqReadNext(lf)) != NULL)
    {
    if( mottTrim(fq, minLength, isIllumina, cutoff))
        {
	fqWriteNext(fq, f);
        }
    fqFree(&fq);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clMinLength = optionInt("minLength", clMinLength);
clIsIllumina = optionExists("isIllumina");
clCutoff = optionInt("cutoff", clCutoff);

if(argc != 3 && argc != 5)
    {
    usage();
    }
if(argc == 3)
    {
    trimSingleFastqFile(argv[1], argv[2], clMinLength, clIsIllumina, clCutoff);
    }
    
if(argc == 5)
    {
    trimPairedFastqFiles(argv[1], argv[3], argv[2], argv[4], clMinLength, clIsIllumina, clCutoff);
    }
    
return 0;
}
