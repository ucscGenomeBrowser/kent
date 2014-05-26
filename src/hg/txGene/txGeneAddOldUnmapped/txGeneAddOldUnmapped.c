/* txGeneAddOldUnmapped - Add information about genes that didn't map from old assembly to oldToNew table.. */

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
  "txGeneAddOldUnmapped - Add information about genes that didn't map from old assembly to oldToNew table.\n"
  "usage:\n"
  "   txGeneAddOldUnmapped oldToNewMapped.tab old.unmapped oldToNew.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txGeneAddOldUnmapped(char *mappedFile, char *unmappedFile, char *outputFile)
/* txGeneAddOldUnmapped - Add information about genes that didn't map from old assembly to oldToNew table.. */
{
struct lineFile *mappedLf = lineFileOpen(mappedFile, TRUE);
struct lineFile *unmappedLf = lineFileOpen(unmappedFile, TRUE);
FILE *f = mustOpen(outputFile, "w");

/* Read through the mapped ones and save to output unchanged, but keep hash of all the accessions
 * we've seen to double check mapped/unmapped go together. */
struct hash *accHash = hashNew(0);
    {
    char *row[4];
    while (lineFileRowTab(mappedLf, row))
	{
	char *acc = row[1];
	hashAdd(accHash, acc, NULL);
	fprintf(f, "%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3]);
	}
    }

/* Read through bed file for unmapped and save data. */
    {
    char *row[12];
    while (lineFileRowTab(unmappedLf, row))
        {
	char *acc = row[3];
	if (hashLookup(accHash, acc))
	    errAbort("%s is in both %s and %s\n", acc, mappedFile, unmappedFile);
	fprintf(f, "\t%s\t\t%s\n", acc, "unmapped");
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txGeneAddOldUnmapped(argv[1], argv[2], argv[3]);
return 0;
}
