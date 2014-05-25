/* rmFaDup - remove duplicate records from FA file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "fa.h"
#include "hash.h"
#include "obscure.h"


void usage()
/* Print usage and exit. */
{
errAbort(
  "rmFaDup - remove duplicate records in FA file\n"
  "usage\n"
  "   rmFaDup oldName.fa newName.fa\n");
}

void rmFaDups(char *inFile, char *outFile)
/* undupFa - remove duplicate records from FA file. */
{
struct hash *uniq = newHash(0);
struct hashEl *hel;
char *name;
FILE *in = mustOpen(inFile, "r");
FILE *out = mustOpen(outFile, "w");
DNA *dna;
int size;


while (faFastReadNext(in, &dna, &size, &name))
    {
    if ((hel = hashLookup(uniq, name)) == NULL)
	{
	hashAdd(uniq, name, NULL);
	faWriteNext(out, name, dna, size);
	}
    else
	printf("removing duplicate %s\n", name);
    }
fclose(in);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
rmFaDups(argv[1], argv[2]);
return 0;
}
