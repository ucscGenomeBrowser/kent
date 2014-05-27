/* splitFa - split a big FA file into smaller ones. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Print usage instructions and exit. */
{
errAbort("splitFa - split a big FA file into smaller ones.\n"
	 "usage:\n"
	 "    splitFa source.fa maxSize destRoot\n"
	 "This will split source into files of maxSize or smaller.\n"
	 "The resulting files will be named destRoot01.fa destRoot02.fa\n"
	 "and so forth.\n");
}


int main(int argc, char *argv[])
{
char *sourceName, *destRootName;
int maxSize;
char destName[512];
char faName[512];
int destIx;
int size, start;
struct dnaSeq *seq;


if (argc != 4)
    usage();
sourceName = argv[1];
maxSize = atoi(argv[2]);
if (maxSize < 1)
    usage();
destRootName = argv[3];
printf("reading %s\n", sourceName);
seq = faReadDna(sourceName);
for (start = 0, destIx = 1; start < seq->size; start += size, ++destIx)
    {
    size = seq->size - start;
    if (size > maxSize)
	size = maxSize;
    sprintf(destName, "%s%02d.fa", destRootName, destIx);
    sprintf(faName, "%s.%d", seq->name, destIx);
    printf("writing %s\n", destName);
    faWrite(destName, faName, seq->dna+start, size);
    }
return 0;
}

