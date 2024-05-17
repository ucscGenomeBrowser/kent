/* mmHashTest - Make a hash of strings, write an mmHash file, memory-map it and look up items. */

/* Copyright (C) 2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "mmHash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmHashTest - Make a hash of strings, write an mmHash file, memory-map it and look up items.\n"
  "usage:\n"
  "  mmHashTest in.txt out.mmh out.txt\n"
  "Each line of in.txt contains a key followed by a tab character followed by a value.\n"
  "in.txt is read into a hash which is converted to mmHash and written out to out.mmh.\n"
  "out.mmh is read back in as a memory-mapped file, items are looked up and written to out.txt.\n"
  "out.txt should contain the same contents as in.txt unless there are multiple lines with\n"
  "the same key; in that case, only the last line with the key will be included in out.txt.\n"
  );
}

static struct optionSpec options[] = {
    {NULL, 0},
};

static void makeRandomString(char *buf, int bufSize)
/* Fill buf with bufSize-1 random printable characters and 0-terminate. */
{
// Use as printable characters: ASCII 32-126
int i;
for (i = 0;  i < bufSize-1;  i++)
    buf[i] = 32 + (rand() % 95);
buf[bufSize-1] = 0;
}

void mmHashTest(char *inFileName, char *mmHashFileName, char *outFileName)
/* Read in items from a file and print the resulting clusters. */
{
FILE *f = mustOpen(outFileName, "w");
// Read inFile into hash
struct lineFile *lf = lineFileOpen(inFileName, TRUE);
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
hashToMmHashFile(hash, mmHashFileName);
freeHashAndVals(&hash);

// Get that file memory-mapped.
struct mmHash *mmh = mmHashFromFile(mmHashFileName);

// Just for fun, look up a long random name, with the expectation that there is no way it will
// ever appear in input, to make sure that when a key is not found, we return NULL instead of
// crashing or returning a value.
char longRandomString[512];
makeRandomString(longRandomString, sizeof longRandomString);
const char *shouldBeNull = mmHashFindVal(mmh, longRandomString);
if (shouldBeNull != NULL)
    errAbort("Error: I really did not expect to find the following line as a key in %s:\n%s\n"
             "-- but there it was, with a value of '%s'",
             inFileName, longRandomString, shouldBeNull);

// Look up and write out all the items.
struct slName *key;
for (key = keyList;  key != NULL;  key = key->next)
    {
    const char *val = mmHashFindVal(mmh, key->name);
    if (val == NULL)
        errAbort("Lookup of key '%s' failed.", key->name);
    fprintf(f, "%s\t%s\n", key->name, val);
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mmHashTest(argv[1], argv[2], argv[3]);
return 0;
}
