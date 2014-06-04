/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome.
 * Unmapped reads are written to the file 'unmapped.bam' if requested. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"

boolean clUnmapped = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome \n"
  "usage:\n"
  "   bamSplitByChrom input.bam\n"
  "options:\n"
  "  -unmapped Creates a file 'unmapped.bam' for the unmapped reads. \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"unmapped",OPTION_BOOLEAN},
   {NULL, 0},
};

void openOutput(struct hash *hash, bam_header_t *head)
/* Loops through the input bam's header, opening an output file
 * for each chromosome in the input file. */
{
int i;
for (i = 0; i < head->n_targets; ++i )
    {
    char *fileName = catTwoStrings(head->target_name[i], ".bam");
    samfile_t *outBam = bamMustOpenLocal(fileName, "wb", head);
    hashAdd(hash, head->target_name[i], outBam);
    }
}

void closeOutput(struct hash *hash, bam_header_t *head)
/* Loops through the output files and closes them. */
{
int i;
for (i = 0; i < head->n_targets; ++i )
    {
    samclose(hashFindVal(hash, head->target_name[i]));
    }
}

void writeOutput(samfile_t *input, struct hash *hash, boolean unmapped)
/* Reads through the input bam and writes each alignment to the correct output file.
 * Unmapped reads are written to 'unmapped.bam' */ 
{
bam_header_t *head = input ->header;
bam1_t one;
ZeroVar(&one);
samfile_t *unmap = bamMustOpenLocal("unmapped.bam", "wb", head);
if (!unmapped)
    /* Removes the 'unmapped.bam' file if it is not requested. */
    {
    remove("unmapped.bam");
    }
for (;;)
    {
    if (samread (input, &one) < 0)
        {
	break;
	}
	if (one.core.tid > 0)
	    {
            samwrite(hashFindVal(hash, head->target_name[one.core.tid]), &one);    
            }
        else 
	    {
	    if (unmapped)
	        {
	        samwrite(unmap, &one);
	        }
	    }
    }
samclose(unmap);
}

void bamSplitByChrom(char *inBam, boolean unmapped)
/* Splits the bam file into multiple bam files based on chromosome. */
{
struct hash *hash = hashNew(0);
samfile_t *input = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = input ->header;
/* Open the input bam. */
openOutput(hash, head);
/* Open the output bam files. */
writeOutput(input, hash, unmapped);
/* Write each alignment to the correct output file. */
closeOutput(hash, head);
/* Close the output files. */
samclose(input);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clUnmapped = optionExists("unmapped");
if (argc != 2)
    usage();
bamSplitByChrom(argv[1], clUnmapped);
return 0;
}
