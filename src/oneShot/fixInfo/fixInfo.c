/* fixInfo - remove dupes from info file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"

char *skipWord(char *fw)
/* skips over current word to start of next. 
 * Error for this not to exist. */
{
char *s;
s = skipToSpaces(fw);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
s = skipLeadingSpaces(s);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
return s;
}

void fixFile(char *fileName)
{
char bakName[512];
struct lineFile *lf;
int lineSize;
char *line;
FILE *f;
int wordCount;
char *words[3];
int ix = 0;

sprintf(bakName, "%s.bak", fileName);
rename(fileName, bakName);
lf = lineFileOpen(bakName, TRUE);
f = mustOpen(fileName, "w");
lineFileNext(lf, &line, &lineSize);
fprintf(f, "%s\n", line);
while (lineFileNext(lf, &line, &lineSize))
    {
    ++ix;
    wordCount = chopLine(line, words);
    fprintf(f, " %s %d\n", words[0], ix*100);
    }
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
int i;
for (i=1; i<argc; ++i)
    fixFile(argv[i]);
}

