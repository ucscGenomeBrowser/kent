/* cdwWrangleIsaacInput - Convert list of directories into input.txt assuming directories are output of Illumina Isaac pipeline.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWrangleIsaacInput - Convert list of directories into input.txt assuming directories are output of Illumina Isaac pipeline.\n"
  "usage:\n"
  "   cdwWrangleIsaacInput dir1 dir2 ... \n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwWrangleIsaacInput(int dirCount, char *dirArray[])
/* cdwWrangleIsaacInput - Convert list of directories into input.txt assuming directories are output of Illumina Isaac pipeline.. */
{
FILE *f = stdout;
int i;
for (i=0; i<dirCount; ++i)
    {
    char *dir = dirArray[i];
    struct fileInfo *file, *fileList = listDirX(dir, "*", FALSE);

    /* Make a pass through finding the input files for others */
    char *read1 = NULL, *read2=NULL, *bam = NULL;
    for (file = fileList; file != NULL; file = file->next)
        {
	if (endsWith(file->name, "_R1.fastq.gz"))
	    {
	    if (read1 != NULL)
	        errAbort("Duplicate read1 in %s, %s and %s\n", dir, read1, file->name);
	    read1 = file->name;
	    }
	else if (endsWith(file->name, "_R2.fastq.gz"))
	    {
	    if (read2 != NULL)
	        errAbort("Duplicate read1 in %s, %s and %s\n", dir, read2, file->name);
	    read2 = file->name;
	    }
	else if (endsWith(file->name, ".bam"))
	    {
	    if (bam != NULL)
	        errAbort("Duplicate read1 in %s, %s and %s\n", dir, bam, file->name);
	    bam = file->name;
	    }
	}
    if (read1 == NULL)
        errAbort("Missing _R1.fastq.gz in %s", dir);
    if (read2 == NULL)
        errAbort("Missing _R2.fastq.gz in %s", dir);
    if (bam == NULL)
        errAbort("Missing .bam in %s", dir);

    /* Make second pass creating inputs for everything but fastqs */
    for (file = fileList; file != NULL; file = file->next)
        {
	if (endsWith(file->name, ".fastq.gz"))
	    ;
	else if (endsWith(file->name, ".bam"))
	    fprintf(f, "%s/%s\tIsaac_alignment\t%s/%s\t%s/%s\n", 
		dir, file->name, dir, read1, dir, read2);
	else if (endsWith(file->name, ".bai"))
	    fprintf(f, "%s/%s\tsamtools_index\t%s/%s\n", dir, file->name, dir, bam);
	else if (endsWith(file->name, ".vcf"))
	    fprintf(f, "%s/%s\tIsaac_variant\t%s/%s\n", dir, file->name, dir, bam);
	else if (endsWith(file->name, ".pdf"))
	    fprintf(f, "%s/%s\tIsaac_variant\t%s/%s\n", dir, file->name, dir, bam);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
cdwWrangleIsaacInput(argc-1, argv+1);
return 0;
}
