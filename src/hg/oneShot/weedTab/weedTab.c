/* weedTab - Weed out entries from a tab file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "weedTab - Weed out entries from a tab file\n"
  "usage:\n"
  "   weedTab in.tab weeds.fa out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashFaNames(char *fileName)
/* Read in a .fa file and store the names in a hash. */
{
struct hash *hash = newHash(20);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
int count = 0;
while (lineFileNext(lf, &line, NULL))
    {
    if (*line == '>')
        {
	line += 1;
	word = nextWord(&line);
	if (word != NULL)
	    {
	    hashAdd(hash, word, NULL);
	    ++count;
	    }
	}
    }
printf("%d seqs in %s\n", count, fileName);
lineFileClose(&lf);
return hash;
}

void weedTab(char *inFile, char *weedFile, char *outFile)
/* weedTab - Weed out entries from a tab file. */
{
struct hash *weedHash = hashFaNames(weedFile);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int lineSize, wordCount;
char *line, dupe[1024], *words[2];
FILE *f = mustOpen(outFile, "w");
int allCount = 0, writeCount = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    ++allCount;
    if (lineSize >= sizeof(dupe))
        errAbort("line %d of %s too long, %d chars", 
		lf->lineIx, lf->fileName, lineSize);
    strcpy(dupe, line);
    wordCount = chopLine(line, words);
    if (wordCount < 2)
        errAbort("Need at least 2 words line %d of %s", 
		lf->lineIx, lf->fileName);
    if (!hashLookup(weedHash, words[1]))
	{
	++writeCount;
        fprintf(f, "%s\n", dupe);
	}
    }
printf("Wrote %d of %d lines to %s\n", writeCount, allCount, outFile);
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
weedTab(argv[1], argv[2], argv[3]);
return 0;
}
