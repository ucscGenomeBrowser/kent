/* faToMaf - Convert fa multiple alignment format to maf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "maf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToMaf - Convert fa multiple alignment format to maf\n"
  "usage:\n"
  "   faToMaf in.multiple.fa out.maf\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct dnaSeq *faReadWithDash(FILE *f)
/* Read in a fasta sequence that might include dashes. */
{
int c;
struct dnaSeq *seq;
struct dyString *dy;
char *line, *name;

/* Get first > */
for (;;)
    {
    if ((c = fgetc(f)) == -1)
	return NULL;
    if (c == '>')
        break;
    }
/* Get line into dy and then first word into name*/
dy = dyStringNew(0);
for (;;)
    {
    if ((c = fgetc(f)) == -1)
        errAbort("Unexpected end of file");
    if (c == '\n')
        break;
    dyStringAppendC(dy, c);
    }
line = dy->string;
name = cloneString(nextWord(&line));

/* Get up until next '>' into dy */
dyStringClear(dy);
for (;;)
    {
    if ((c = fgetc(f)) == -1)
	break;
    if (isdigit(c) || isspace(c))
        continue;
    if (c == '>')
	{
        ungetc(c, f);
	break;
	}
    dyStringAppendC(dy, c);
    }

/* Create seq. */
AllocVar(seq);
seq->name = name;
seq->dna = cloneString(dy->string);
seq->size = dy->stringSize;
verbose(2, "seq %s, %d bases\n", seq->name, seq->size);
dyStringFree(&dy);
return seq;
}

struct dnaSeq *readMultiFa(char *fileName)
/* Read in multiple-species fasta file into a list of seqs. */
{
FILE *f = mustOpen(fileName, "r");
struct dnaSeq *list = NULL, *seq;

while ((seq = faReadWithDash(f)) != NULL)
    {
    slAddHead(&list, seq);
    }
slReverse(&list);
carefulClose(&f);
return list;
}


struct mafComp *mafCompFakeFromAliSym(struct dnaSeq *seq)
/* Fake up a mafComp from a dnaSeq with dashes. */
{
struct mafComp *mc;
AllocVar(mc);
mc->src = cloneString(seq->name);
mc->srcSize = mc->size = seq->size - countChars(seq->dna, '-');
mc->strand = '+';
mc->start = 0;
mc->text = cloneString(seq->dna);
return mc;
}

struct mafAli *mafFromSeqList(struct dnaSeq *seqList, char *fileName)
/* Create mafAli from seqs. */
{
struct dnaSeq *seq;
struct mafComp *mc;
struct mafAli *maf;
int textSize;

/* Check that input looks reasonable. */
if (seqList == NULL)
    errAbort("No sequences in %s!", fileName);
textSize = seqList->size;
for (seq = seqList->next; seq != NULL; seq = seq->next)
    {
    if (textSize != seq->size)
        errAbort("Not all sequences in %s are the same size", fileName);
    }

/* Construct mafAli. */
AllocVar(maf);
maf->textSize = textSize;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    mc = mafCompFakeFromAliSym(seq);
    slAddHead(&maf->components, mc);
    }
slReverse(&maf->components);
return maf;
}

void faToMaf(char *inFa, char *outMaf)
/* faToMaf - Convert fa multiple alignment format to maf. */
{
struct dnaSeq *seqList = readMultiFa(inFa);
struct mafAli *maf = mafFromSeqList(seqList, inFa);
FILE *f = mustOpen(outMaf, "w");
mafWriteStart(f, NULL);
mafWrite(f, maf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
faToMaf(argv[1], argv[2]);
return 0;
}
