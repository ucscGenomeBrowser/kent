/* txCdsOrfInfo - Given a sequence and a putative ORF, calculate some basic information on it.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "rangeTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsOrfInfo - Given a sequence and a putative ORF, calculate some basic information on it.\n"
  "usage:\n"
  "   txCdsOrfInfo in.cds in.fa out.ra\n"
  "where:\n"
  "   in.cds is a three column file: <sequence> <start> <end>\n"
  "   in.fa is a fasta file containing all sequence mentioned in in.cds\n"
  "   out.ra is a ra file with the info\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

/*
  "       upstreamSize - bases covered by ORFs upstream\n"
  "       upstreamKozakSize - bases covered by Kozak starting ORFs upstream\n"
*/

int findOrfEnd(struct dnaSeq *seq, int start)
/* Figure out end of orf that starts at start */
{
int lastPos = seq->size-3;
int i;
for (i=start+3; i<lastPos; i += 3)
    {
    if (isStopCodon(seq->dna+i))
        return i+3;
    }
return seq->size;
}

void outputOneRa(struct dnaSeq *seq, int start, int end, FILE *f)
/* Output one Ra record to file. */
{
fprintf(f, "orfName %s_%d_%d\n", seq->name, start, end);
fprintf(f, "txName %s\n", seq->name);
fprintf(f, "txSize %d\n", seq->size);
fprintf(f, "cdsStart %d\n", start);
fprintf(f, "cdsEnd %d\n", end);
fprintf(f, "cdsSize %d\n", end-start);
fprintf(f, "gotStart %d\n", startsWith("atg", seq->dna+start));
fprintf(f, "gotEnd %d\n", isStopCodon(seq->dna+end-3));
boolean gotKozak1 = FALSE;
if (start >= 3)
    {
    char c = seq->dna[start-3];
    gotKozak1 = (c == 'a' || c == 'g');
    }
fprintf(f, "gotKozak1 %d\n", gotKozak1);
boolean gotKozak2 = FALSE;
if (start+3 < seq->size)
    gotKozak2 = (seq->dna[start+3] == 'g');
fprintf(f, "gotKozak2 %d\n", gotKozak2);
fprintf(f, "gotKozak %d\n", gotKozak1 + gotKozak2);

/* Count up upstream ATG and Kozak */
struct rbTree *upAtgRanges = rangeTreeNew(), *upKozakRanges = rangeTreeNew();
int upAtg = 0, upKozak = 0;
int i;
for (i=0; i<start; ++i)
    {
    if (startsWith("atg", seq->dna + i))
        {
	int orfEnd = findOrfEnd(seq, i);
	if (orfEnd < start)
	    rangeTreeAdd(upAtgRanges, i, orfEnd);
	++upAtg;
	if (isKozak(seq->dna, seq->size, i))
	    {
	    ++upKozak;
	    if (orfEnd < start)
		rangeTreeAdd(upKozakRanges, i, orfEnd);
	    }
	}
    }
fprintf(f, "upstreamAtgCount %d\n", upAtg);
fprintf(f, "upstreamKozakCount %d\n", upKozak);
fprintf(f, "upstreamSize %d\n", rangeTreeOverlapSize(upAtgRanges, 0, start));
fprintf(f, "upstreamKozakSize %d\n", rangeTreeOverlapSize(upKozakRanges, 0, start));
fprintf(f, "\n");

/* Cluen up and go home. */
rangeTreeFree(&upAtgRanges);
rangeTreeFree(&upKozakRanges);
}

void txCdsOrfInfo(char *inCds, char *inFa, char *outInfo)
/* txCdsOrfInfo - Given a sequence and a putative ORF, calculate some basic information on it.. */
{
struct hash *dnaHash = faReadAllIntoHash(inFa, dnaLower);
struct lineFile *lf = lineFileOpen(inCds, TRUE);
FILE *f = mustOpen(outInfo, "w");
char *row[3];
while (lineFileRow(lf, row))
    {
    char *seqName = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    struct dnaSeq *seq = hashFindVal(dnaHash, seqName);
    if (seq == NULL)
        errAbort("%s is in %s but not %s", seqName, inCds, inFa);
    outputOneRa(seq, start, end, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
dnaUtilOpen();
txCdsOrfInfo(argv[1], argv[2], argv[3]);
return 0;
}
