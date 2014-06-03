/* checkChain - check for duplicate ids. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "chain.h"
#include "hash.h"
#include "linefile.h"


static struct hash *idHash = NULL;
static struct hash *duplicateHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "checkChain - read all chains and report if duplicate ids\n"
    "usage:\n"
    "    checkChain chainFileName (compressed okay) outputFileName \n");
}

void checkIds(char *inputFileName, char *outputFileName)
/* report if duplicate ID found */
/* put all ids in idHash */
{
struct chain *chainEl;
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
FILE *outputFileHandle = NULL;
char idString[64];
char *idString2 = NULL;
struct hashEl *hel = NULL;
struct hashEl *hel2 = NULL;
int chainCount = 0;
int dupCount = 0;
struct hashCookie cookie;

idHash = newHash(0);
duplicateHash = newHash(0);
while ((chainEl = chainRead(lf)) != NULL)
    {
    chainCount++;
    safef(idString, sizeof(idString), "%d", chainEl->id);
    hel = hashLookup(idHash, idString);
    if (hel == NULL)
        hashAdd(idHash, cloneString(idString), NULL);
    else
        {
	hel2 = hashLookup(duplicateHash, idString);
	if (hel2 == NULL)
	    hashAdd(duplicateHash, cloneString(idString), NULL);
	}
    }
verbose(1, "chain count = %d\n", chainCount);
// freeHash(&idHash);

/* print contents of duplicateHash */
outputFileHandle = mustOpen(outputFileName, "w");
cookie = hashFirst(duplicateHash);
while ((idString2 = hashNextName(&cookie)) != NULL)
    {
    dupCount++;
    fprintf(outputFileHandle, "%s\n", idString2);
    }
verbose(1, "count of duplicate IDs = %d\n", dupCount);
carefulClose(&outputFileHandle);
// freeHash(&duplicateHash);
}

int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

checkIds(argv[1], argv[2]);
return 0;
}
