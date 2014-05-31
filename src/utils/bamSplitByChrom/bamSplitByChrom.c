/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome . */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome \n"
  "usage:\n"
  "   bamSplitByChrom input.bam\n"
  "options:\n"
  "   \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

samfile_t *samMustOpen(char *fileName, char *mode, void *extraHeader)
/* Open up samfile or die trying. */
{
samfile_t *sf = samopen(fileName, mode, extraHeader);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", fileName);
return sf;
}

char *concat(char *s1, char *s2)
/* A simple concatenate function. */
{
char *result = needMem(strlen(s1)+strlen(s2) +1);
strcpy(result,s1);
strcat(result,s2);
return result;
}

void openOutput(struct hash *hash, bam_header_t *head)
{
int i;
for ( i = 0; i < head->n_targets; ++i )
    {
    char *fileName =concat(head->target_name[i], ".bam");
    samfile_t *outBam = bamMustOpenLocal(fileName, "wb", head);
    hashAdd(hash, head->target_name[i], outBam);
    }
}

void closeOutput(struct hash *hash, bam_header_t *head)
{
int i;
for ( i = 0; i < head->n_targets; ++i )
    {
    samclose(hashFindVal(hash, head->target_name[i]));
    }
}

void writeOutput(samfile_t *input, struct hash *hash)
{
bam_header_t *head = input ->header;
bam1_t one;
ZeroVar(&one);
samfile_t *unmapped = bamMustOpenLocal("unmapped.bam", "wb", head);
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
	    samwrite(unmapped, &one);
	    }
    }
samclose(unmapped);    
}

void bamSplitByChrom(char *inBam)
{
struct hash *hash = hashNew(0);
samfile_t *input = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = input ->header;
openOutput(hash, head);
/* Open up file, loop through header, and make up a hash with chromosome names for keys,
 * and FILE * for values. */
writeOutput(input, hash);
/* Loop through each record of BAM file, looking up chromosome, getting file from hash,
 * and adding record to appropriate file */
closeOutput(hash, head);
samclose(input);
/* Loop through each files nad close it */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
bamSplitByChrom(argv[1]);
return 0;
}
