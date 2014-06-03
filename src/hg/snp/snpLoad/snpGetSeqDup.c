/* snpGetSeqDup - log duplicates in fasta files. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "linefile.h"


static struct hash *snpHash = NULL;
static struct hash *uniqHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpGetSeqDup - log duplicates in fasta files.\n"
  "usage:\n"
  "  snpGetSeqDup inputFilename \n");
}


void readInput(char *inputFileName)
{
struct lineFile *lf;
char *line;
char *row[2];
struct hashEl *hel = NULL;

lf = lineFileOpen(inputFileName, TRUE);

while (lineFileNext(lf, &line, NULL))
    {
    chopString(line, "\t", row, ArraySize(row));
    hel = hashLookup(uniqHash, row[0]);
    if (!hel)
        hashAdd(uniqHash, cloneString(row[0]), NULL);
    hashAdd(snpHash, cloneString(row[0]), cloneString(row[1]));
    }
lineFileClose(&lf);
}


void doLog()
{
FILE *logFileHandle = mustOpen("snpGetSeqDup.log", "w");
struct hashCookie cookie = hashFirst(uniqHash);
char *rsId = NULL;
int count = 0;
struct hashEl *hel = NULL;
char *fileName = NULL;
struct dyString *dy = newDyString(1024);

while ((rsId = hashNextName(&cookie)) != NULL)
    {
    count = 0;
    for (hel = hashLookup(snpHash, rsId); hel != NULL; hel = hashLookupNext(hel))
	count++;
    if (count == 1) continue;
    for (hel = hashLookup(snpHash, rsId); hel != NULL; hel = hashLookupNext(hel))
        {
	fileName = (char *)hel->val;
	dyStringAppend(dy, fileName);
	dyStringAppend(dy, " ");
	}
    fprintf(logFileHandle, "%s\t%s\n", rsId, dy->string);
    dyStringClear(dy);
    }

carefulClose(&logFileHandle);
}


int main(int argc, char *argv[])
{
if (argc != 2)
    usage();

snpHash = newHash(16);
uniqHash = newHash(16);
readInput(argv[1]);
doLog();

return 0;
}
