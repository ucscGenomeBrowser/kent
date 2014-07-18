/* bamMerge -  Merges multiple bam files into a single bam file . */
/* The header data for the output comes from the first bam file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamMerge -  Merges multiple bam files into a single bam output file, merged.bam  \n"
  "usage:\n"
  "   bamMerge input1.bam input2.bam ... inputn.bam\n"
  "options:\n"
  "   \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void bamMerge(char *fileNames[], int files)
/* bamMerge -  Merges multiple bam files into a single bam file. */
{
samfile_t *chromHead = bamMustOpenLocal(fileNames[0], "rb", NULL);
bam_header_t *head = chromHead->header;
samfile_t *out = bamMustOpenLocal("merged.bam", "wb", head);
samclose(chromHead);
/* Opens the output and sets the header to be the same as the first input. */
int i;
for (i = 0; i < files; ++i)
/* Loop through all input files */
    {
    samfile_t *in = bamMustOpenLocal(fileNames[i], "rb", NULL);
    bam1_t one;
    ZeroVar(&one);	// This seems to be necessary!
    /* Open an input file */
    for (;;)
        {
        if (samread(in, &one) < 0)
            {
	    break;
	    }
        samwrite(out, &one);
        /* Copy the input to the output */
	}
    samclose(in);
    }
samclose(out);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();   
bamMerge(++argv,--argc);
return 0;

}
