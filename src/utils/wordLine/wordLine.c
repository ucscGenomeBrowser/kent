/* wordLine - chop up words by white space and output them with one
 * word to each line. */
#include "common.h"
#include "linefile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
 "wordLine - chop up words by white space and output them with one\n"
 "word to each line.\n"
 "usage:\n"
 "    wordLine inFile(s)\n"
 "Output will go to stdout.");
}

void wordLine(char *file)
/* wordLine - chop up words by white space and output them with one
 * word to each line. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
int lineSize, wordCount;
static char *line, *words[1024*16];
int i;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    for (i=0; i<wordCount; ++i)
	{
	puts(words[i]);
	}
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    wordLine(argv[i]);
}
