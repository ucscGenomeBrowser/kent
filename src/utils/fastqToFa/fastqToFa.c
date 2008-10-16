/* fastqToFa - Convert from fastq to fasta format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: fastqToFa.c,v 1.1 2008/10/16 00:10:56 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastqToFa - Convert from fastq to fasta format.\n"
  "usage:\n"
  "   fastqToFa in.fastq out.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fastqToFa(char *inFastq, char *outFa)
/* fastqToFa - Convert from fastq to fasta format.. */
{
struct lineFile *lf = lineFileOpen(inFastq, TRUE);
FILE *f = mustOpen(outFa, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    line = skipLeadingSpaces(line);
    if (line[0] != '@')
        errAbort("%s doesn't seem to be fastq format.  Expecting '@' start of line %d, got %c.",
		lf->fileName, lf->lineIx, line[0]);
    fprintf(f, ">%s\n", line+1);
    if (!lineFileNextReal(lf, &line))
       lineFileUnexpectedEnd(lf); 
    int dnaSize = strlen(line);
    char c;
    while ((c = *line++) != 0)
        {
	switch (c)
	    {
	    case 'a':
	    case 'c':
	    case 't':
	    case 'g':
	    case 'n':
	    case 'A':
	    case 'C':
	    case 'G':
	    case 'T':
	    case 'N':
	        fputc(c, f);
		break;
	    default:
	        errAbort("Expecting DNA sequence, got '%c' line %d of %s", c, 
			lf->lineIx, lf->fileName);
		break;
	    }
	}
    fputc('\n', f);
    if (!lineFileNextReal(lf, &line))
       lineFileUnexpectedEnd(lf); 
    if (!sameString(line, "+"))
        errAbort("Expecting '+', got '%s' line %d of %s", line, lf->lineIx, lf->fileName);
    if (!lineFileNextReal(lf, &line))
       lineFileUnexpectedEnd(lf); 
    int qualitySize = strlen(line);
    if (qualitySize != dnaSize)
       errAbort("Size of quality line (%d) doesn't match size of DNA line (%d) line %d of %s\n",
           qualitySize, dnaSize, lf->lineIx, lf->fileName);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
fastqToFa(argv[1], argv[2]);
return 0;
}
