/* tabToMmHash - Read in a tab-sep file, hash first column to string of remaining columns,
 * write mmHash file. */
#include "common.h"
#include "linefile.h"
#include "mmHash.h"
#include "options.h"

/* This file is copyright 2024 UCSC Genome Browser Authors, but license is hereby
 * granted for all use - public, private or commercial. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabToMmHash - Read in a tab-sep file, hash first column to string of remaining columns, write mmHash file\n"
  "usage:\n"
  "   tabToMmHash in.tab out.mmh\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void tabToMmHash(char *tabIn, char *mmhOut)
/* tabToMmHash - Read in a tab-sep file, hash first column to string of remaining columns,
 * write mmHash file. */
{
// Read inFile into hash
struct lineFile *lf = lineFileOpen(tabIn, TRUE);
struct hash *hash = hashNew(0);
struct slName *keyList = NULL;
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    {
    char *key = line;
    char *val = "";
    char *tab = strchr(line, '\t');
    if (tab != NULL)
        {
        *tab = '\0';
        val = tab + 1;
        }
    hashAdd(hash, key, cloneString(val));
    slNameAddHead(&keyList, key);
    }
lineFileClose(&lf);
slReverse(&keyList);

// Convert hash to mmHash file.
hashToMmHashFile(hash, mmhOut);
freeHashAndVals(&hash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tabToMmHash(argv[1], argv[2]);
return 0;
}
