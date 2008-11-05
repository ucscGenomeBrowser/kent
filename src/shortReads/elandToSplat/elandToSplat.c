/* elandToSplat - Convert eland output to splat output.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: elandToSplat.c,v 1.1 2008/11/05 03:55:35 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elandToSplat - Convert eland output to splat output.\n"
  "usage:\n"
  "   elandToSplat in.vmf out.splat\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputMappings(char *readName, char *seq, char *mappings, FILE *f)
/* Parse out mappings, and output a splat line for each one. */
{
if (readName[0] == '>')
    readName += 1;
char *chrom = NULL;
char *s, *e;
for (s = mappings; s[0] != 0; s = e)
   {
   }
}

void elandToSplat(char *vmfIn, char *splatOut)
/* elandToSplat - Convert eland output to splat output.. */
{
struct lineFile *lf = lineFileOpen(vfmIn, TRUE);
FILE *f = mustOpen(splatOut, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *words[5];
    int wordCount = chopLine(line, words);
    if (wordCount != 3 && wordCount != 4)
       errAbort("Expecting 3 or 4 columns line %d of %s, got %d", lf->lineIx, lf->fileName, wordCount);
    if (wordCount == 4)
        {
	char *readName = words[0];
	char *seq = words[1];
	chr *mappings = words[2];
	outputMappings(words[0], words[1], words[2], f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
elandToSplat(argv[1], argv[2]);
return 0;
}
