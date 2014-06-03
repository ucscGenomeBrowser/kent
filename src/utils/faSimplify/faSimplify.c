/* faSimplify - Simplify fasta record headers. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faSimplify - Simplify fasta record headers\n"
  "usage:\n"
  "   faSimplify in.fa startPat endPat out.fa\n"
  "This will write out the stuff between startPat and\n"
  "endPat\n"
  "options:\n"
  "   -prefix=XXX This will add XXX as a prefix\n"
  "   -suffix=XXX This will add YYY as a suffix\n"
  );
}

static struct optionSpec options[] = {
   {"prefix", OPTION_STRING},
   {"suffix", OPTION_STRING},
   {NULL, 0},
};

char *prefix = "";
char *suffix = "";

void faSimplify(char *inName, char *startPat, char *endPat, char *outName)
/* faFlyBaseToUcsc - Convert Flybase peptide fasta file to UCSC format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line;
int startSize = strlen(startPat);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	char *s = stringIn(startPat, line), *e;
	if (s == NULL)
	   errAbort("No %s line %d of %s", startPat, lf->lineIx, lf->fileName);
	s += startSize;
	e = stringIn(endPat, s);
	if (e == NULL)
	   errAbort("No %s line %d of %s", endPat, lf->lineIx, lf->fileName);
	*e = 0;
	fprintf(f, ">%s%s%s\n", prefix, s, suffix);
	}
    else
        {
	fprintf(f, "%s\n", line);
	}
    }
carefulClose(&f);
lineFileClose(&lf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
prefix = optionVal("prefix", prefix);
suffix = optionVal("suffix", suffix);
faSimplify(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
