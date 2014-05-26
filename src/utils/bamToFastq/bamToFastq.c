/* bamToFastq - converts a BAM file to Fastq. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamToFastq - converts a BAM file to Fastq\n"
  "usage:\n"
  "   bamToFastq input.bam output.fastq\n"
  "options:\n"
  "   "
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct fastqSeq
/* Holds a single fastq sequence. */
    {
    struct fastqSeq *next; /* Next in sequence. */
    int size;       /* Size of the sequence. */
    char *header;   /* Sequence header, begins with '@' */
    char *del;      /* Fastq deliminator '+' */
    char *dna;      /* DNA sequence */
    unsigned char *quality;  /* DNA quality, in ASCII format, unsigned for bam functions */
    };

void fixQuality(struct fastqSeq *seq)
/* The bam quality reader returns a format that is not FASTQ. */
/* This function updates the bam quality to a fastq quality. */
{
int size = strlen(seq->dna);
int i = 0;
for (i=0; i < size; ++i)
    {
    seq->quality[i] += 33;
    }
seq->quality[size]='\0';
}

void fastqWriteNext(struct fastqSeq *input, FILE *f)
/* Writes a single fastq structure to the target file. */
{
    fprintf(f,"%s\n",input->header);
    fprintf(f,"%s\n",input->dna);
    fprintf(f,"%s\n",input->del);
    fprintf(f,"%s\n",input->quality);
}


void freeFastqSeq(struct fastqSeq **pInput)
/* Frees the memory allocated to a fastq structure. */
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

char *concat(char *s1, char *s2)
/* A simple concatenate function. */
{
char *result = needMem(strlen(s1)+strlen(s2) +1);
strcpy(result,s1);
strcat(result,s2);
return result;
}

samfile_t *samMustOpen(char *fileName, char *mode, void *extraHeader)
/* Open up samfile or die trying. */
{
samfile_t *sf = samopen(fileName, mode, extraHeader);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", fileName);
return sf;
}


void bamToFastq(char *inBam, char *outFastq)
/* bamToFastq - converts a BAM file to Fastq. */
{
samfile_t *in = samMustOpen(inBam, "rb", NULL);
/* Open up the BAM input  and a fastq sequence */
FILE *f = mustOpen(outFastq, "w");
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
struct fastqSeq seq = {};
for (;;)
    {
    if (samread(in, &one) < 0)
	{
	break;
	}
    seq.header = concat("@",bam1_qname(&one));
    seq.del = "+";  
    seq.dna = bamGetQuerySequence(&one, TRUE);
    seq.quality = bamGetQueryQuals(&one, TRUE);
    /* enter in the required fastqSeq values */
    fixQuality(&seq); 
    fastqWriteNext(&seq, f);
    /* print the fasqSeq to file */
    }
samclose(in);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bamToFastq(argv[1],argv[2]);
return 0;
}
