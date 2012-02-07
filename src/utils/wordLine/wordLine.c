/* wordLine - chop up words by white space and output them with one
 * word to each line. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "tokenizer.h"


static struct optionSpec optionSpecs[] = {
    {"csym", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
 "wordLine - chop up words by white space and output them with one\n"
 "word to each line.\n"
 "usage:\n"
 "    wordLine inFile(s)\n"
 "Output will go to stdout."
 "Options:\n"
 "    -csym - Break up words based on C symbol rules rather than white space\n"
 );
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

void tokenLine(char *file)
/* tokenLine - chop up words by c-tokens and output one per line. */
{
struct tokenizer *tok = tokenizerNew(file);
char *s;

tok->leaveQuotes = TRUE;
tok->uncommentC = TRUE;
tok->uncommentShell = TRUE;
while ((s = tokenizerNext(tok)) != NULL)
    puts(s);
tokenizerFree(&tok);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    {
    if (optionExists("csym"))
	tokenLine(argv[i]);
    else
	wordLine(argv[i]);
    }
return 0;
}
