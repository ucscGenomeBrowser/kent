/* faFlyBaseToUcsc - Convert Flybase peptide fasta file to UCSC format. */

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
  "faFlyBaseToUcsc - Convert Flybase peptide fasta file to UCSC format\n"
  "usage:\n"
  "   faFlyBaseToUcsc in.faa out.faa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void faFlyBaseToUcsc(char *inName, char *outName)
/* faFlyBaseToUcsc - Convert Flybase peptide fasta file to UCSC format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line;
char *startPat = "FBgn";
char *endPat = "]";
int startSize = strlen(startPat);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	char *s = stringIn(startPat, line), *e;
	if (s == NULL)
	   errAbort("No %s line %d of %s", startPat, lf->lineIx, lf->fileName);
	e = stringIn(endPat, s + startSize);
	if (e == NULL)
	   errAbort("No %s line %d of %s", endPat, lf->lineIx, lf->fileName);
	*e = 0;
	fprintf(f, ">%s\n", s);
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
if (argc != 3)
    usage();
faFlyBaseToUcsc(argv[1], argv[2]);
return 0;
}
