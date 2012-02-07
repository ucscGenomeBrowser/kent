/* checkDupe - Check for dupes in HUGO names. */
#include "common.h"
#include "linefile.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkDupe - Check for dupes in HUGO names\n"
  "usage:\n"
  "   checkDupe file.tab\n");
}

void checkDupe(char *fileName)
/* checkDupe - Check for dupes in HUGO names. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[7];
struct hash *uniqHash = newHash(0);

while (lineFileRow(lf, words))
    {
    if (sameString(words[3], "hugo"))
        {
	hashAddUnique(uniqHash, words[6], NULL);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
checkDupe(argv[1]);
return 0;
}
