/* reTraceFixMaf - Add quality line and recompute chrom line in maf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "maf.h"
#include "qaSeq.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "reTraceFixMaf - Add quality line and recompute chrom line in maf\n"
  "usage:\n"
  "   reTraceFixMaf in.maf in.qa out.maf\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *makeQaHash(struct qaSeq *qaList)
{
struct hash *qaHash = newHash(20);
struct qaSeq *qas;
for (qas = qaList; qas != NULL; qas = qas->next)
    hashAdd(qaHash, qas->name, qas);
return qaHash;
}

void reTraceFixMaf(char *mafFileName, char *qaFileName, char *newMafFileName)
/* reTraceFixMaf - Add quality line and recompute chrom line in maf. */
{
struct mafFile *mFile = mafReadAll(mafFileName);
struct qaSeq *qaList = qaRead(qaFileName);
struct hash *qaHash = makeQaHash(qaList);
struct mafAli *ali;
for (ali = mFile->alignments; ali != NULL; ali = ali->next)
    {
    if (!ali->components || !ali->components->next)
	errAbort("Something's up with the maf.");
    else
	{
	struct mafComp *secondSrc = ali->components->next;
	struct qaSeq *qas = hashMustFindVal(qaHash, secondSrc->src);
	int i, offset;
	int length = strlen(secondSrc->text);
	if (secondSrc->strand == '-')
	    reverseBytes(qas->qa, qas->size);
	offset = secondSrc->start;
	AllocArray(secondSrc->quality, length+1);
	for (i = 0; i < length; i++)
	    {
	    if (secondSrc->text[i] == '-')
		secondSrc->quality[i] = '-';
	    else
		{
		int q = (int)qas->qa[offset++];
		char c = 'F';
		if ((q >= 0) && (q < 45))
		    {
		    q = q / 5;
		    c = '0' + q;
		    }
		else if ((q >= 45) && (q < 98))
		    c = '9';
		else if (q == 99)
		    c = '0';
		else
		    c = 'F';
		secondSrc->quality[i] = c;
		}
	    }
	secondSrc->quality[length] = '\0';
	if (secondSrc->strand == '-')
	    reverseBytes(qas->qa, qas->size);
	}
    }
mafWriteAll(mFile, newMafFileName);
hashFree(&qaHash);
qaSeqFreeList(&qaList);
mafFileFree(&mFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
reTraceFixMaf(argv[1], argv[2], argv[3]);
return 0;
}
