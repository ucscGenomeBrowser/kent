/* twoBitToFa - Convert all or part of twoBit file to fasta. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"
#include "bPlusTree.h"
#include "basicBed.h"
#include "udc.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitToFa - Convert all or part of .2bit file to fasta\n"
  "usage:\n"
  "   twoBitToFa input.2bit output.fa\n"
  "options:\n"
  "   -seq=name       Restrict this to just one sequence.\n"
  "   -start=X        Start at given position in sequence (zero-based).\n"
  "   -end=X          End at given position in sequence (non-inclusive).\n"
  "   -seqList=file   File containing list of the desired sequence names \n"
  "                   in the format seqSpec[:start-end], e.g. chr1 or chr1:0-189\n"
  "                   where coordinates are half-open zero-based, i.e. [start,end).\n"
  "   -noMask         Convert sequence to all upper case.\n"
  "   -bpt=index.bpt  Use bpt index instead of built-in one.\n"
  "   -bed=input.bed  Grab sequences specified by input.bed. Will exclude introns.\n"
  "   -bedPos         With -bed, use chrom:start-end as the fasta ID in output.fa.\n"
  "   -udcDir=/dir/to/cache  Place to put cache for remote bigBed/bigWigs.\n"
  "\n"
  "Sequence and range may also be specified as part of the input\n"
  "file name using the syntax:\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name:start-end\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */
char *clSeqList = NULL; /* file containing list of seq names */
bool noMask = FALSE;  /* convert seq to upper case */
char *clBpt = NULL;	/* External index file. */
char *clBed = NULL;	/* Bed file that specifies bounds of sequences. */
bool clBedPos = FALSE;

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"seqList", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"noMask", OPTION_BOOLEAN},
   {"bpt", OPTION_STRING},
   {"bed", OPTION_STRING},
   {"bedPos", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void outputOne(struct twoBitFile *tbf, char *seqSpec, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbf, seqSpec, start, end);
if (noMask)
    toUpperN(seq->dna, seq->size);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

static void processAllSeqs(struct twoBitFile *tbf, FILE *outFile)
/* get all sequences in a file */
{
struct twoBitIndex *index;
for (index = tbf->indexList; index != NULL; index = index->next)
    outputOne(tbf, index->name, outFile, 0, 0);
}

static void processSeqSpecs(struct twoBitFile *tbf, struct twoBitSeqSpec *tbss,
                            FILE *outFile)
/* process list of twoBitSeqSpec objects */
{
struct twoBitSeqSpec *s;
for (s = tbss; s != NULL; s = s->next)
    outputOne(tbf, s->name, outFile, s->start, s->end);
}

struct dnaSeq *twoBitAndBedToSeq(struct twoBitFile *tbf, struct bed *bed)
/* Get sequence defined by bed.  Exclude introns. */
{
struct dnaSeq *seq;
if (bed->blockCount <= 1)
    {
    seq = twoBitReadSeqFrag(tbf, bed->chrom, bed->chromStart, bed->chromEnd);
    freeMem(seq->name);
    seq->name = cloneString(bed->name);
    }
else
    {
    int totalBlockSize = bedTotalBlockSize(bed);
    AllocVar(seq);
    seq->name = cloneString(bed->name);
    seq->dna = needMem(totalBlockSize+1);
    seq->size = totalBlockSize;
    int i;
    int seqOffset = 0;
    for (i=0; i<bed->blockCount; ++i)
        {
	int exonSize = bed->blockSizes[i];
	int exonStart = bed->chromStart + bed->chromStarts[i];
	struct dnaSeq *exon = twoBitReadSeqFrag(tbf, bed->chrom, exonStart, exonStart+exonSize);
	memcpy(seq->dna + seqOffset, exon->dna, exonSize);
	seqOffset += exonSize;
	dnaSeqFree(&exon);
	}
    }
if (bed->strand[0] == '-')
    reverseComplement(seq->dna, seq->size);
return seq;
}

static void processSeqsFromBed(struct twoBitFile *tbf, char *bedFileName, FILE *outFile)
/* Get sequences defined by beds.  Exclude introns. */
{
struct bed *bed, *bedList = bedLoadAll(bedFileName);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct dnaSeq *seq = twoBitAndBedToSeq(tbf, bed);
    char* seqName = NULL;
    if (clBedPos) 
        {
        char buf[1024];
        safef(buf, 1024, "%s:%d-%d", bed->chrom, bed->chromStart, bed->chromEnd);
        seqName = buf;
        }
    else
        seqName = seq->name;
    if (noMask)
        toUpperN(seq->dna, seq->size);
    faWriteNext(outFile, seqName, seq->dna, seq->size);
    dnaSeqFree(&seq);
    }
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
struct twoBitFile *tbf;
FILE *outFile = mustOpen(outName, "w");
struct twoBitSpec *tbs;

if (clSeq != NULL)
    {
    char seqSpec[2*PATH_LEN];
    if (clEnd > clStart)
        safef(seqSpec, sizeof(seqSpec), "%s:%s:%d-%d", inName, clSeq, clStart, clEnd);
    else
        safef(seqSpec, sizeof(seqSpec), "%s:%s", inName, clSeq);
    tbs = twoBitSpecNew(seqSpec);
    }
else if (clSeqList != NULL)
    tbs = twoBitSpecNewFile(inName, clSeqList);
else
    tbs = twoBitSpecNew(inName);

if (tbs == NULL)
    errAbort("%s is not a twoBit file", inName);

if (tbs->seqs != NULL && clBpt != NULL)
    tbf = twoBitOpenExternalBptIndex(tbs->fileName, clBpt);
else
    tbf = twoBitOpen(tbs->fileName);
if (clBed != NULL)
    {
    processSeqsFromBed(tbf, clBed, outFile);
    }
else
    {
    if (tbs->seqs == NULL)
	processAllSeqs(tbf, outFile);
    else
	processSeqSpecs(tbf, tbs->seqs, outFile);
    }
twoBitSpecFree(&tbs);
carefulClose(&outFile);
twoBitClose(&tbf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clSeq = optionVal("seq", clSeq);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clSeqList = optionVal("seqList", clSeqList);
clBpt = optionVal("bpt", clBpt);
clBed = optionVal("bed", clBed);
clBedPos = optionExists("bedPos");
noMask = optionExists("noMask");
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (clBedPos && !clBed) 
    errAbort("the -bedPos option requires the -bed option");
if (clBed != NULL)
    {
    if (clSeqList != NULL)
	errAbort("Can only have seqList or bed options, not both.");
    if (clSeq != NULL)
        errAbort("Can only have seq or bed options, not both.");
    }
if ((clStart > clEnd) && (clSeq == NULL))
    errAbort("must specify -seq with -start and -end");
if ((clSeq != NULL) && (clSeqList != NULL))
    errAbort("can't specify both -seq and -seqList");

dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
