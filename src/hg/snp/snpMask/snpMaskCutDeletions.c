/* snpMaskCutDeletions -- Print genomic sequence with deletion SNPs removed. */

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
"snpMaskCutDeletions -- Print genomic sequence with deletion SNPs removed.\n"
"usage:\n"
"    snpMaskCutDeletions snpNNN.bed db.2bit out.fa\n"
"Given a snpNNN.bed (NNN >= 125) and the corresponding 2bit assembly\n"
"sequence, write a fasta file of each assembly seq minus the sequence\n"
"from each deletion SNP.\n"
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
"    | snpMaskCutDeletions stdin /cluster/data/$db/$db.2bit stdout \\\n"
"    | faSplit byname stdin deletions/\n"
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

void snpMaskCutDeletions(char *snpBedFile, char *twoBitFile, char *outFile)
{
/* snpMaskCutDeletions -- Print genomic sequence with deletion SNPs removed. */
struct lineFile *lfSnp = lineFileOpen(snpBedFile, TRUE);
struct twoBitFile *tbfGenomic = twoBitOpen(twoBitFile);
FILE *outMasked = mustOpen(outFile, "w");
long long expectedTotalSize = twoBitTotalSize(tbfGenomic);
long long totalCutSnps = 0, totalCutBases = 0, totalSize = 0;
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
    char *observed = words[8];
    char *class = words[10];
    int weight = lineFileNeedFullNum(lfSnp, words, 16);
    if (chromEnd < chromStart+1 ||
	!sameString(class, "deletion") ||
	weight != 1 ||
	!isSimpleObserved(observed))
	continue;
    if (!seq || !sameString(seq->name, chrom))
	{
	if (seq)
	    {
	    /* Write the sequence from the last deletion point
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
    if (chromEnd <= seqPos)
	continue;
    if (chromStart > seqPos)
	/* Write sequence preceding this deletion: */
	writeSeqWithBreaks(outMasked, seq->dna+seqPos, chromStart - seqPos,
			   50);
    totalCutSnps++;
    /* Hop over the deleted genomic sequence: */
    totalCutBases += (chromEnd - max(chromStart, seqPos));
    seqPos = chromEnd;
    }
if (seq)
    {
    /* Write the sequence from the last deletion point through the
     * end of the chrom. */
    writeSeqWithBreaks(outMasked, seq->dna+seqPos, seq->size - seqPos, 50);
    totalSize += seq->size;
    dnaSeqFree(&seq);
    }
lineFileClose(&lfSnp);
twoBitClose(&tbfGenomic);
carefulClose(&outMasked);
verbose(1, "Cut %lld snps totaling %lld bases from %lld genomic bases\n",
	totalCutSnps, totalCutBases, totalSize);
if (totalSize != expectedTotalSize)
    verbose(0, "%s has %lld total bases, but the total number of bases "
	    "in sequences for which we masked snps is %lld "
	    "(difference is %lld)\n", twoBitFile, expectedTotalSize,
	    totalSize, (expectedTotalSize - totalSize));
}

int main(int argc, char *argv[])
/* Check args and call snpMaskCutDeletions. */
{
if (argc != 4)
    usage();
snpMaskCutDeletions(argv[1], argv[2], argv[3]);
return 0;
}
