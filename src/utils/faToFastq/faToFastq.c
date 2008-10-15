/* faToFastq - Convert fa to fastq format, just faking quality values.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"

static char const rcsid[] = "$Id: faToFastq.c,v 1.1 2008/10/15 23:52:15 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToFastq - Convert fa to fastq format, just faking quality values.\n"
  "usage:\n"
  "   faToFastq in.fa out.fastq\n"
  "options:\n"
  "   -qual=X quality letter to use.  Default is '<' which is good I think....\n"
  );
}

char qualLetter = '<';

static struct optionSpec options[] = {
   {"qual", OPTION_STRING},
   {NULL, 0},
};

void faToFastq(char *inFa, char *outFastq)
/* faToFastq - Convert fa to fastq format, just faking quality values.. */
{
struct lineFile *lf = lineFileOpen(inFa, TRUE);
FILE *f = mustOpen(outFastq, "w");
DNA *dna;
int size;
char *name;
while (faMixedSpeedReadNext(lf, &dna, &size, &name))
    {
    fprintf(f, "@%s\n", name);
    fprintf(f, "%s\n", dna);
    fprintf(f, "+\n");
    int i;
    for (i=0; i<size; ++i)
        fputc(qualLetter, f);
    fputc('\n', f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *qual = optionVal("qual", NULL);
if (qual != NULL)
    qualLetter = qual[0];
faToFastq(argv[1], argv[2]);
return 0;
}
