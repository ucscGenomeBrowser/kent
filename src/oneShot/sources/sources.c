#include "common.h"
#include "hash.h"

int main(int argc, char *argv[])
{
FILE *in = stdin;
FILE *out = stdout;
char origLine[1024];
char line[1024];
char *words[256];
int wordCount;
struct hash *hash;
int wordIx;
char *word;

if (argc != 2 || !isdigit(argv[1][0]))
    errAbort("Usage: %s wordIx", argv[0]);
	
wordIx = atoi(argv[1]);
hash = newHash(14);
while (fgets(line, sizeof(line), in))
    {
    strcpy(origLine, line);
    wordCount = chopLine(line, words);
    if (wordCount < 1 || words[0][0] == '#')
	continue;
    if (wordCount >= wordIx)
	{
	word = words[wordIx-1];
	if (!hashLookup(hash, word))
	    {
	    fprintf(out, "%s", origLine);
	    hashAdd(hash, word, NULL);
	    }
	}
    }
}
