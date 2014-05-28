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

bool isUnderscore(char c)
/* checks if the character is an underscore */
{
if (c == '_')
    {
    return(TRUE);
    }
return(FALSE);
}

char * removeUnderscores(char * input)
/* Removes the Underscores fromo a string */
{
int i, j=0;
char *result = needMem(strlen(input)+1); 
for (i = 0; input[i] != '\0'; ++i)
   {
   if (!isUnderscore(input[i]))
       {
       result[j] = input[i];
       ++j;
       }
   }
return(result);
}

void bamSplitByChrom(char *inBam)
/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome . */
{
/* Open file and get header for it. */
samfile_t *input = samMustOpen(inBam, "rb", NULL);
bam_header_t *head = input->header;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
int i =0;
for (i=0; i<head->n_targets; ++i)
/* Loop through each chromosome. */   
    {
    //head->target_name[i] = removeUnderscores(head->target_name[i]);
    samfile_t *in = samMustOpen(inBam, "rb", NULL);
    char *outBam = head->target_name[i];  
    char *bam = ".bam";
    samfile_t *out = bamMustOpenLocal(concat(outBam,bam), "wb", head);
    /* Open an output bam file. */
    for (;;)
    /* Loop through the input bam file. */
        {
        if (samread(in, &one) < 0)
            {
	    break;
	    }
	if (head->target_name[one.core.tid]==outBam)
	//if (strcmp(head->target_name[one.core.tid],outBam)==0)
            /* Seems to be dropping certain chromosomes (with underscores in names) */
	    {
	    
	    printf("seems to be working");
            samwrite(out, &one);
            }
        }
    samclose(out);
    samclose(in);
    }
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
