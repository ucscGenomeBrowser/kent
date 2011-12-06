/* newClones - Find new clones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "newClones - Find new clones\n"
  "usage:\n"
  "   newClones old.lst new.lst\n"
  );
}

void newClones(char *oldFile, char *newFile)
/* newClones - Find new clones. */
{
struct slName *oldList = readAllLines(oldFile);
struct slName *newList = readAllLines(newFile);
struct slName *el;
struct hash *hash = newHash(0);

for (el = oldList; el != NULL; el = el->next)
    {
    chopSuffix(el->name);
    hashAdd(hash, el->name, NULL);
    }
for (el = newList; el != NULL; el = el->next)
    {
    chopSuffix(el->name);
    if (!hashLookup(hash, el->name))
        printf("%s\n", el->name);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
newClones(argv[1], argv[2]);
return 0;
}
