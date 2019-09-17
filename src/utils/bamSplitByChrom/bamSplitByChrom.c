/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome.
 * Unmapped reads are written to the file 'unmapped.bam' if requested. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "portable.h"
#include <dirent.h>

boolean clUnmapped = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome.\n"
  " The output bam files are put into a user specified directory\n"
  "usage:\n"
  "   bamSplitByChrom input.bam directory\n"
  "options:\n"
  "  -unmapped Creates a file 'unmapped.bam' for the unmapped reads. \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"unmapped",OPTION_BOOLEAN},
   {NULL, 0},
};

void closeOutput(struct hash *hash)
/* Loops through the output files and closes them. */
{
struct hashEl *hel, *helList = hashElListHash(hash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    samclose(hel->val);
    }
slFreeList(&helList);
}

void writeOutput(samfile_t *input,  struct hash *hash, boolean unmapped, char *dir)
/* Reads through the input bam and writes each alignment to the correct output file.
 * Unmapped reads are written to 'unmapped.bam' */ 
{
bam1_t one;
ZeroVar(&one);
if (makeDir(dir))
    {
    setCurrentDir(dir);
    }
// Creates a new directory and moves into it. 
bam_header_t *head = sam_hdr_read(input);
samfile_t *unmap = bamMustOpenLocal("unmapped.bam", "wb", head);
if (!unmapped)
    /* Removes the 'unmapped.bam' file if it is not requested. */
    {
    remove("unmapped.bam");
    }
for (;;)
    {
    if (sam_read1(input, head, &one) < 0)
        {
	break;
	}
    if (one.core.tid >= 0)
	{
	char *chrom = head->target_name[one.core.tid];
	samfile_t *out = hashFindVal(hash, chrom);
	if (out == NULL)
	    {
	    char *fileName = catTwoStrings(chrom, ".bam");
	    out = bamMustOpenLocal(fileName, "wb", head);
	    hashAdd(hash, chrom, out);
	    }
	sam_write1(hashFindVal(hash, head->target_name[one.core.tid]), head, &one);    
	}
    else 
	{
	if (unmapped)
	    {
	    sam_write1(unmap, head, &one);
	    }
	}
    }
samclose(unmap);
}

void bamSplitByChrom(char *inBam, char *dir, boolean unmapped)
/* Splits the bam file into multiple bam files based on chromosome. */
{
struct hash *hash = hashNew(0);
samfile_t *input = bamMustOpenLocal(inBam, "rb", NULL);
writeOutput(input, hash, unmapped, dir);
/* Open the output bam files in a new directory. */
/* Write each alignment to the correct output file. */
closeOutput(hash);
/* Loops through each output file and closes it */
samclose(input);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clUnmapped = optionExists("unmapped");
if (argc != 3)
    usage();
bamSplitByChrom(argv[1], argv[2], clUnmapped);
return 0;
}
