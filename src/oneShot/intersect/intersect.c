/* Intersect - gives list of genes that are in both lists. */
#include "common.h"
#include "hash.h"

int main(int argc, char *argv[])
{
struct hash *hash;
char line[512];
int wordCount;
char *words[1];
FILE *in, *out;

if (argc != 4)
    {
    errAbort("intersect - usage a.in b.in c.out");
    }
hash = newHash(0);
in = mustOpen(argv[1], "r");
while (fgets(line, sizeof(line), in))
    {
    wordCount = chopLine(line, words);
    if (wordCount > 0)
        {
        hashAdd(hash, words[0], NULL);
        }
    }
fclose(in);

in = mustOpen(argv[2], "r");
out = mustOpen(argv[3], "w");
while (fgets(line, sizeof(line), in))
    {
    wordCount = chopLine(line, words);
    if (wordCount > 0)
        {
        if (hashLookup(hash, words[0]))
            {
            fprintf(out, "%s\n", words[0]);
            }
        }
    }
return 0;
}
