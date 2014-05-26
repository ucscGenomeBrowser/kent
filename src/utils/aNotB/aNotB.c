/* aNotB - List symbols that are in a but not b. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "aNotB - List symbols that are in a but not b\n"
  "usage:\n"
  "   aNotB aFile bFile outFile\n"
  "A symbol in this context is the first word in a line\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *hashFirstWord(char *fileName)
/* Hash first word in each line of file */
{
char *row[1];
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    hashStore(hash, row[0]);
    }
lineFileClose(&lf);
return hash;
}

void aNotB(char *aFile, char *bFile, char *outFile)
/* aNotB - List symbols that are in a but not b. */
{
struct hash *bHash = hashFirstWord(bFile);
struct lineFile *lf = lineFileOpen(aFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[1];
while (lineFileRow(lf, row))
    {
    if (!hashLookup(bHash, row[0]))
	fprintf(f, "%s\n", row[0]);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
aNotB(argv[1], argv[2], argv[3]);
return 0;
}
