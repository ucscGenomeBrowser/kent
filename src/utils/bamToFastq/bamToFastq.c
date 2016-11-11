/* bamToFastq - Converts a bam file to fastq format. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "fq.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamToFastq - Converts a bam file to fastq format.\n"
  "usage:\n"
  "   bamToFastq input.bam output.fastq\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void fixQuality(struct fq *seq)
/* The bam quality reader returns a format that is not fastq. */
/* This function updates the bam quality to a fastq quality. */
{
int size = strlen(seq->dna);
int i = 0;
for (i = 0; i < size; ++i)
    {
    seq->quality[i] += 33;
    }
seq->quality[size] = '\0';
}

void bamToFastq(char *inBam, char *outFastq)
/* bamToFastq - converts a bam file to Fastq. */
{
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = sam_hdr_read(in);
if (head == NULL)
    errAbort("Aborting ... bad BAM header in %s", inBam);

/* Open up the bam input  and a fastq sequence */
FILE *f = mustOpen(outFastq, "w");
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
struct fq seq = {};
for (;;)
    {
    if (sam_read1(in, head, &one) < 0)
	{
	break;
	}
    seq.header = catTwoStrings("@",bam1_qname(&one));
    seq.dna = bamGetQuerySequence(&one, TRUE);
    seq.quality = bamGetQueryQuals(&one, TRUE);
    /* Enter in the required fq values. */
    fixQuality(&seq); 
    fqWriteNext(&seq, f);
    /* Print the fq to file. */
    freez(&seq.header);
    freez(&seq.dna);
    freez(&seq.quality);
    }

samclose(in);
carefulClose(&f);
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
