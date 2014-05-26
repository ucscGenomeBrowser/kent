/* axtSomeRecords - Extract multiple axt records. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSomeRecords - Extract multiple axt records\n"
  "usage:\n"
  "   axtSomeRecords axtIn listOfQNames axtOut\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashLines(char *fileName)
/* Read all lines in file and put them in a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], NULL);
lineFileClose(&lf);
return hash;
}

void axtSomeRecords(char *axtIn, char *listName, char *axtOut)
/* axtSomeRecords - Extract multiple axt records. */
{
struct hash *hash = hashLines(listName);
struct lineFile *lf = lineFileOpen(axtIn, TRUE);
FILE *f = mustOpen(axtOut, "w");
struct axt *axt;

while ((axt = axtRead(lf) ) != NULL)
    {
    if (hashLookup(hash, axt->qName))
	axtWrite(axt, f);
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
axtSomeRecords(argv[1], argv[2], argv[3]);
return 0;
}
