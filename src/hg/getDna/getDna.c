#include "common.h"
#include "obscure.h"
#include "dnaseq.h"
#include "fa.h"
#include "hgdb.h"

void usage()
{
errAbort(
    "getDna - Fetch dna in fasta format from a list of genBank\n"
    "accession numbers.\n"
    "usage:\n"
    "     getDna accession(s)\n"
    "where accession(s) is one or more accession numbers.\n"
    "     getDna @listFile\n"
    "where listFile is a white-space delimited list of accession\n"
    "numbers\n");
}

void doOne(char *acc)
/* Process one accession number. */
{
struct dnaSeq *seqList, *seq, *next;
int count;
char nameBuf[64];

seqList = hgdbGetSeq(acc);
count = slCount(seqList);
if (count == 1)
    {
    seq = seqList;
    faWriteNext(stdout, acc, seq->dna, seq->size);
    freeDnaSeq(&seq);
    }
else
    {
    int seqIx = 1;
    for (seq = seqList, seqIx=1; seq != NULL; seq = next, seqIx += 1)
	{
	next = seq->next;
	sprintf(nameBuf, "%s.%d", acc, seqIx);
	faWriteNext(stdout, nameBuf, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
}

int main(int argc, char *argv[])
{
int i;
char *name;

if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    {
    name = argv[i];
    if (name[0] == '@')
	{
	char **words;
	int wordCount;
	char *buf;
	int j;
	char *acc;
	readAllWords(name+1, &words, &wordCount, &buf);
	for (j = 0; j<wordCount; ++j)
	    {
	    doOne(words[j]);
	    }
	freeMem(words);
	freeMem(buf);
	}
    else
	{
	doOne(name);
	}
    }
}
