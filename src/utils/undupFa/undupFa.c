/* undupFa - remove duplicate records from FA file. */

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
  "undupFa - rename duplicate records in FA file\n"
  "usage\n"
  "   undupFa faFile(s)\n");
}

void undupFa(char *fileName)
/* undupFa - remove duplicate records from FA file. */
{
struct dnaSeq *seqList = faReadAllDna(fileName), *seq;
struct hash *uniq = newHash(0);
struct hashEl *hel;
int ix;
char newName[256];
char *name;

FILE *out = mustOpen(fileName, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    name = seq->name;
    if ((hel = hashLookup(uniq, name)) == NULL)
	hashAdd(uniq, name, NULL);
    else
	{
	ix = ptToInt(hel->val);
	hel->val = intToPt(ix+1);
	sprintf(newName, "%s_%c", name, 'a' + ix);
	printf("Converting %s number %d to %s\n", name, ix+1, newName);
	name = newName;
	hashAdd(uniq, newName, NULL);
	}
    faWriteNext(out, name, seq->dna, seq->size);
    }
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    undupFa(argv[i]);
return 0;
}
