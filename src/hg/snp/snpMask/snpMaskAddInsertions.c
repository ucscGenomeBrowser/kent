/* snpMaskAddInsertions -- Print genomic sequence plus insertion SNPs. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "dnaseq.h"
#include "twoBit.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"snpMaskAddInsertions -- Print genomic sequence plus insertion SNPs.\n"
"usage:\n"
"    snpMaskAddInsertions snpNNN.bed db.2bit out.fa\n"
"Given a snpNNN.bed (NNN >= 125) and the corresponding 2bit assembly\n"
"sequence, write a fasta file with added sequence from each insertion SNP.\n"
"Note1: snpNNN.bed input *must* be sorted by position.\n"
"Note2: It is a good idea to exclude SNPs with exceptions that indicate\n"
"problems, for example:\n"
"\n"
"    awk '$5 ~ /^MultipleAlignments|ObservedTooLong|ObservedWrongFormat|ObservedMismatch|MixedObserved$/ {print $4;}' \\\n"
"      /cluster/data/dbSNP/NNN/human/snpNNNExceptions.bed \\\n"
"      | sort -u \\\n"
"      > snpNNNExcludeRsIds.txt\n"
"\n"
"    grep -vFwf snpNNNExcludeRsIds.txt \\\n"
"      /cluster/data/dbSNP/NNN/$organism/snpNNN.bed \\\n"
"    | snpMaskAddInsertions stdin /cluster/data/$db/$db.2bit stdout \\\n"
"    | faSplit byname stdin insertions/\n"
);
}

boolean isSimpleObserved(char *observed)
/* Return TRUE if observed is "-/" followed by alpha-only. */
{
int i;
if (!startsWith("-/", observed))
    return FALSE;
for (i = 2;  i < strlen(observed); i++)
    if (!isalpha(observed[i]))
	return FALSE;
return TRUE;
}

void snpMaskAddInsertions(char *snpBedFile, char *twoBitFile, char *outFile)
{
/* snpMaskAddInsertions -- Print genomic sequence plus insertion SNPs. */
struct lineFile *lfSnp = lineFileOpen(snpBedFile, TRUE);
struct twoBitFile *tbfGenomic = twoBitOpen(twoBitFile);
FILE *outMasked = mustOpen(outFile, "w");
long long expectedTotalSize = twoBitTotalSize(tbfGenomic);
long long totalAddedSnps = 0, totalAddedBases = 0, totalSize = 0;
struct dnaSeq *seq = NULL;
int seqPos = 0;
char *words[17];
int wordCount;

while ((wordCount = lineFileChopTab(lfSnp, words)) > 0)
    {
    lineFileExpectWords(lfSnp, 17, wordCount);
    char *chrom = words[0];
    int chromStart = lineFileNeedFullNum(lfSnp, words, 1);
    int chromEnd = lineFileNeedFullNum(lfSnp, words, 2);
    char strand = words[5][0];
    char *observed = words[8];
    char *class = words[10];
    int weight = lineFileNeedFullNum(lfSnp, words, 16);
    if (chromEnd != chromStart ||
	chromStart == seqPos ||
	!sameString(class, "insertion") ||
	weight != 1 ||
	!isSimpleObserved(observed))
	continue;
    if (!seq || !sameString(seq->name, chrom))
	{
	if (seq)
	    {
	    /* Write the sequence from the last insertion point
	     * through the end of the chrom. */
	    writeSeqWithBreaks(outMasked, seq->dna+seqPos, seq->size - seqPos,
			       50);
	    totalSize += seq->size;
	    dnaSeqFree(&seq);
	    }
	seq = twoBitReadSeqFrag(tbfGenomic, chrom, 0, 0);
	seqPos = 0;
	fprintf(outMasked, ">%s\n", chrom);
	}
    int obsLen = strlen(observed+2);
    if (strand == '-')
	reverseComplement(observed+2, obsLen);
    /* Write sequence preceding this insertion, then the insertion: */
    writeSeqWithBreaks(outMasked, seq->dna+seqPos, chromStart - seqPos, 50);
    writeSeqWithBreaks(outMasked, observed+2, obsLen, 50);
    totalAddedSnps++;
    totalAddedBases += obsLen;
    seqPos = chromStart;
    }
if (seq)
    {
    /* Write the sequence from the last insertion point through the
     * end of the chrom. */
    writeSeqWithBreaks(outMasked, seq->dna+seqPos, seq->size - seqPos, 50);
    totalSize += seq->size;
    dnaSeqFree(&seq);
    }
lineFileClose(&lfSnp);
twoBitClose(&tbfGenomic);
carefulClose(&outMasked);
verbose(1, "Added %lld snps totaling %lld bases to %lld genomic bases\n",
	totalAddedSnps, totalAddedBases, totalSize);
if (totalSize != expectedTotalSize)
    verbose(0, "%s has %lld total bases, but the total number of bases "
	    "in sequences for which we masked snps is %lld "
	    "(difference is %lld)\n", twoBitFile, expectedTotalSize,
	    totalSize, (expectedTotalSize - totalSize));
}

int main(int argc, char *argv[])
/* Check args and call snpMaskAddInsertions. */
{
if (argc != 4)
    usage();
snpMaskAddInsertions(argv[1], argv[2], argv[3]);
return 0;
}
