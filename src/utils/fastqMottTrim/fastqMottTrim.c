/* fastqMottTrim - mott. */
/* Applies Richard Mott's trimming algorithm to the */
/* three prime end of each sequence. */
/* Cuttoff is the lowest desired quality score in phred scores, */
/* set by default to 1/100 chance of mismatch. */
/* Base corresponds to any additions to the ASCII quality scheme. */
/* Minlength specifies the minimum sequence length for the output. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"

int clMinLength = 20;
boolean clIsIllumina = FALSE;
int clCutoff = 20;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastqMottTrim - applies Mott's trimming algorithm to a fastq file\n"
  " trims the 3 prime end based on cumulative quality \n"
  "usage:\n"
  "   fastqMottTrim input.fq output.fq\n"
  "options:\n"
  " -minLength=N the minimum length allowed for a trimmed sequence default 20  \n"
  " -isIllumina TRUE for illumina, FALSE for Sanger \n"
  " -cutoff=N  the lowest desired phred score, default 30 \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"minLength",OPTION_INT},
   {"isIllumina",OPTION_BOOLEAN},
   {"cutoff",OPTION_INT},
   {NULL,0},
};

struct fastqSeq
/* holds a single fastq sequence */
    {
    struct fastqSeq *next;
    int size;       /* Size of the sequence. */
    char *header;   /* Sequence header, begins with '@' */
    char *del;      /* Fastq deliminator '+' */
    char *dna;      /* DNA sequence */
    char *quality;  /* DNA quality, in ASCII format */
    };

#ifdef OLD
struct fastqSeq  *fastqReadNext(struct lineFile *lf)
/*reads the next fastq sequence and returns a fastq struct */
{
struct fastqSeq *seq=needMem(sizeof(struct fastqSeq));
/* dyamically allocate memory */
int size;
lineFileNext(lf,&seq->header,&size);
lineFileNext(lf,&seq->dna,&size);
lineFileNext(lf,&seq->del,&size);
lineFileNext(lf,&seq->quality,&size);
return(seq);
}
#endif /* OLD */

char *nextLineIntoString(struct lineFile *lf)
/* Return next line cloned into persistent memory.  Abort if can't get next line. */
{
char *line;
lineFileNeedNext(lf, &line, NULL);
return cloneString(line);
}

struct fastqSeq  *fastqReadNext(struct lineFile *lf)
/* Reads the next fastq sequence and returns a fastq struct.
 * Returns NULL at end of file. */
{
char *line;
if (!lineFileNext(lf, &line, NULL))
    return NULL;   // Pass back end of file

struct fastqSeq *seq;
AllocVar(seq);
seq->header = cloneString(line);
seq->dna = nextLineIntoString(lf);
seq->del = nextLineIntoString(lf);
seq->quality = nextLineIntoString(lf);
seq->size = strlen(seq->dna);
if (seq->size != strlen(seq->quality))
    errAbort("dna and quality sizes don't match at record ending line %d of %s", lf->lineIx, lf->fileName);
return(seq);
}


void fastqWriteNext(struct fastqSeq *input, FILE *f)
/* a function for writing a single fastq struct to file */
{
    fprintf(f,"%s\n",input->header);
    fprintf(f,"%s\n",input->dna);
    fprintf(f,"%s\n",input->del);
    fprintf(f,"%s\n",input->quality);
}

void freeFastqSeq(struct fastqSeq **pInput)
/* frees the memory allocated to a fastq struct */
{
struct fastqSeq *input = *pInput;
if (input != NULL)
    {
    freeMem(input->header);
    freeMem(input->dna);
    freeMem(input->del);
    freeMem(input->quality);
    freez(pInput);
    }
}

boolean  mottTrim(struct fastqSeq *input, int minLength, boolean isIllumina, int cutoff)
/* applies mott's trimming algorithm to the fastq input*/
/* trims from the 3 prime end based on */
/* the sum of (quality score - cutoff value ) */
/* returns true if the trimmed sequence is larger than the minimum sequene length */
{
int base = 33;
int index = -1;
long minValue = 1000000;
long scoreValue = 0;
int qualScore;
if(isIllumina)
    base = 64;
int i = strlen(input->dna)-1;
/*determining the trim length */
for(; i >= 0; --i)
    {
/*convert the quality scores to their ascii values */
    qualScore = input->quality[i];
    qualScore = qualScore - base;
/*calculate the sum of the (quality score - cutoff value)'s*/
    scoreValue = scoreValue + (qualScore - cutoff);
    if(scoreValue < minValue)
        {
        minValue=scoreValue;
        ++index;
    }
/*modifying the fastq fields to be the trimmed length*/
}
if(minValue < cutoff)
    {
    input->dna[strlen(input->dna)-index] = '\0';
    input->quality[strlen(input->quality)-index] = '\0';
    }  
if(strlen(input->dna) > minLength)
    {
    return(TRUE);
    }
return(FALSE);



}

void parseFastq(char *input, char *output, int minLength, boolean isIllumina, int cutoff )
/* goes through fastq sequences in a fastq file*/
/* parses, stores, mottTrims,  prints, then frees*/
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
struct fastqSeq *fq;
while ((fq = fastqReadNext(lf)) != NULL)
    {
    if( mottTrim(fq, minLength, isIllumina, cutoff))
        {
	fastqWriteNext(fq, f);
        }
    freeFastqSeq(&fq);
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

if(argc != 3)
   {
   usage();
   }
parseFastq(argv[1], argv[2], clMinLength, clIsIllumina, clCutoff);
return 0;
}
