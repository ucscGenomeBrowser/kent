/* sanger22gtf - Convert Sanger chromosome 22 annotations to gtf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "sanger22gtf - Convert Sanger chromosome 22 annotations to gtf\n"
  "usage:\n"
  "   sanger22gtf in.gff out.gtf\n"
  );
}

void sanger22gtf(char *inName, char *outName)
/* sanger22gtf - Convert Sanger chromosome 22 annotations to gtf. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line, *word, *s;
int i;

while (lineFileNext(lf, &line, NULL))
    {
    /* Just pass through comments and blank lines. */
    s = skipLeadingSpaces(line);
    if (s[0] == '#' || s[0] == '0')
        {
	fprintf(f, "%s\n", line);
	continue;
	}
    
    for (i=0; i<8; ++i)
        {
	word = nextWord(&line);
	if (word == NULL)
	    errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
	fprintf(f, "%s\t", word);
	}

    s = skipLeadingSpaces(line);
    if (s[0] != 0)
        fprintf(f, "%s", line);
    fputc('\n', f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
sanger22gtf(argv[1], argv[2]);
return 0;
}
