/* bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "psl.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.\n"
  "usage:\n"
  "   bamToPsl in.bam out.psl\n"
  "options:\n"
  "   -fasta=output.fa\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"fasta", OPTION_STRING},
   {NULL, 0},
};

void bamToPsl(char *inBam, char *outPsl, char *outFasta)
/* bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.. */
{
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = sam_hdr_read(in);
if (head == NULL)
    errAbort("Aborting ... bad BAM header in %s", inBam);

/* Open up psl output and write header. */
FILE *f = mustOpen(outPsl, "w");
pslWriteHead(f);

/* Optionally open up fasta output */
FILE *faF = NULL;
if (outFasta != NULL)
    faF = mustOpen(outFasta, "w");

bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
/* Write next sequence to fa file. */
for (;;)
    {
    if (sam_read1(in, head, &one) < 0)
	{
	break;
	}
    struct psl *psl = bamToPslUnscored(&one, head);
    if (psl != NULL)
         pslTabOut(psl, f);
    if (faF != NULL)
        {
	char *dna = bamGetQuerySequence(&one, TRUE);
	char *qName = bam1_qname(&one);
	faWriteNext(faF, qName, dna, strlen(dna));
	freez(&dna);
	}
    }

samclose(in);
carefulClose(&f);
carefulClose(&faF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fastaName = optionVal("fasta", NULL);
bamToPsl(argv[1], argv[2], fastaName);
return 0;
}
