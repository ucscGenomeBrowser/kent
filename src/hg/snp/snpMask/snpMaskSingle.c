/* snpMaskSingle - Print sequence using IUPAC ambiguous nucleotide codes for single base substitutions. */
#include "common.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: snpMaskSingle.c,v 1.1 2008/02/04 20:16:54 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"snpMaskSingle - print sequence using IUPAC ambiguous nucleotide codes for single base substitutions\n"
"usage:\n"
"    snpMaskSingle snpNNN.bed db.2bit out.fa [diffObserved.txt]\n"
"Given a snpNNN.bed (NNN >= 125) and the corresponding 2bit assembly\n"
"sequence, write a fasta file with an IUPAC ambiguous nucleotide code\n"
"representing the observed alleles at the position of each single-base\n"
"SNP. If optional arg [diffOberved.txt] is given, then differing observed\n"
"strings at the same location will be written to that file.\n"
"Note1: This assumes that snpNNN.bed is sorted by position -- results will\n"
"be incorrect if rows for different chroms are shuffled!\n"
"Note2: It is a good idea to filter the snpNNN.bed input to exclude SNPs\n"
"with exceptions that indicate problems, for example:\n"
"    \n"
"    awk '$5 ~ /^MultipleAlignments|ObservedWrongFormat|ObservedMismatch/ {print $4;}' \\\n"
"      /cluster/data/dbSNP/NNN/human/snpNNNExceptions.bed \\\n"
"      | sort -u \\\n"
"      > snpNNNExcludeRsIds.txt\n"
"\n"
"    grep -vFwf snpNNNExcludeRsIds.txt \\\n"
"      /cluster/data/dbSNP/NNN/$organism/snpNNN.bed \\\n"
"    | snpMaskSingle stdin /cluster/data/$db/$db.2bit stdout \\\n"
"    | faSplit byname stdin substitutions/\n"
);
}

/* As Heather suggested, libify the IUPAC nucleotide stuff in dnaUtil? */

/* This is a character array to be indexed by a number with one bit set for
 * each nucleotide found at that base -- it translates those base bitmaps
 * into IUPAC nucleotide characters. */
char *iupacNt =  /* #  = A C G T */
"?"              /* 0  = 0 0 0 0 (none) */
"T"              /* 1  = 0 0 0 1 (T) */
"G"              /* 2  = 0 0 1 0 (G) */
"K"              /* 3  = 0 0 1 1 (G/T) */
"C"              /* 4  = 0 1 0 0 (C) */
"Y"              /* 5  = 0 1 0 1 (C/T) */
"S"              /* 6  = 0 1 1 0 (C/G) */
"B"              /* 7  = 0 1 1 1 (C/G/T) */
"A"              /* 8  = 1 0 0 0 (A) */
"W"              /* 9  = 1 0 0 1 (A/T) */
"R"              /* 10 = 1 0 1 0 (A/G) */
"D"              /* 11 = 1 0 1 1 (A/G/T) */
"M"              /* 12 = 1 1 0 0 (A/C) */
"H"              /* 13 = 1 1 0 1 (A/C/T) */
"V"              /* 14 = 1 1 1 0 (A/C/G) */
"N"              /* 15 = 1 1 1 1 (A/C/G/T) */;

#define NT_BITOFFSET_A 3
#define NT_BITOFFSET_C 2
#define NT_BITOFFSET_G 1
#define NT_BITOFFSET_T 0

UBYTE iupacNtToBaseBits(char iupac)
/* Translate the given character into a base bitmap -- which is the offset
 * of the character in iupacNt (or 0 if the character is not IUPAC). */
{
iupac = toupper(iupac);
char *ptr = strchr(iupacNt, iupac);
if (ptr == NULL)
    return 0;
else
    return (UBYTE)(ptr - iupacNt);
}

char baseBitsToIupacNt(UBYTE baseBits)
/* Translate the given base bitmap into the corresponding IUPAC code. 
 * A bitmap of 0 results in the non-IUPAC character '?'. */
{
if (baseBits > 0xf)
    errAbort("baseBits must be at most 15 (0xf, 0b1111).");
return iupacNt[baseBits];
}

UBYTE baseBitsComplement(UBYTE baseBits)
/* Complement each base included in baseBits by swizzling the bit
 * positions of baseBits: A<->T, C<->G. */
{
if (baseBits > 0xf)
    errAbort("baseBits must be at most 15 (0xf).");
UBYTE baseBitsComp = 0;
baseBitsComp |= ((baseBits >> NT_BITOFFSET_A) & 1) << NT_BITOFFSET_T;
baseBitsComp |= ((baseBits >> NT_BITOFFSET_C) & 1) << NT_BITOFFSET_G;
baseBitsComp |= ((baseBits >> NT_BITOFFSET_T) & 1) << NT_BITOFFSET_A;
baseBitsComp |= ((baseBits >> NT_BITOFFSET_G) & 1) << NT_BITOFFSET_C;
return baseBitsComp;
}

char iupacNtComplement(char iupac)
/* Return the IUPAC nucleotide character representing the complements of
 * all bases represented by iupac -- but if iupac is not an iupac character,
 * leave it unchanged. */
{
UBYTE baseBits = iupacNtToBaseBits(iupac);
if (baseBits == 0)
    return iupac;
else
    return baseBitsToIupacNt(baseBitsComplement(baseBits));
}

boolean iupacNtIsAmbiguous(char iupac)
/* Return TRUE if iupac is not A, C, G or T. */
{
iupac = toupper(iupac);
return (iupac != 'A' && iupac != 'C' && iupac != 'G' && iupac != 'T');
}

UBYTE dnaToAggregateBaseBits(DNA *dna)
/* Build up a base bitmap representing all bases that appear in dna by
 * OR-ing the bits for each char of dna.  Non-IUPAC characters are
 * ignored (bits=0000, no-op in OR). */
{
int len = strlen(dna);
UBYTE baseBits = 0;
int i;
for (i = 0;  i < len;  i++)
    baseBits |= iupacNtToBaseBits(dna[i]);
return baseBits;
}

char *bitsToObserved(UBYTE bits)
/* Make a dbSNP-style observed string from bits. */
{
char observed[16];
observed[0] = '\0';

if ((bits >> NT_BITOFFSET_A) & 0x1)
    strcat(observed, "A");
if ((bits >> NT_BITOFFSET_C) & 0x1)
    {
    if (observed[0] != '\0')
	strcat(observed, "/");
    strcat(observed, "C");
    }
if ((bits >> NT_BITOFFSET_G) & 0x1)
    {
    if (observed[0] != '\0')
	strcat(observed, "/");
    strcat(observed, "G");
    }
if ((bits >> NT_BITOFFSET_T) & 0x1)
    {
    if (observed[0] != '\0')
	strcat(observed, "/");
    strcat(observed, "T");
    }
return cloneString(observed);
}

char mergeBaseAndObserved(char oldIupac, char *observed, char strand,
			  char *chrom, int chromStart, int chromEnd, FILE *diffObsOut)
/* Return an IUPAC base encompassing the old base and all of the bases
 * contained in observed.  Alert the developer if different SNPs at the
 * same base have different bases in observed. */
{
boolean isAmbig = iupacNtIsAmbiguous(oldIupac);
UBYTE oldBits = iupacNtToBaseBits(oldIupac);
UBYTE obsBits = dnaToAggregateBaseBits(observed);
if (strand == '-')
    obsBits = baseBitsComplement(obsBits);
if (isAmbig && obsBits != oldBits)
    {
    if (diffObsOut != NULL)
	fprintf(diffObsOut, "differing observed strings at %s|%d|%d: %s, %s\n",
		chrom, chromStart, chromEnd,
		bitsToObserved(oldBits), bitsToObserved(obsBits));
    else
	verbose(1, "differing observed strings at %s|%d|%d: %s, %s\n",
		chrom, chromStart, chromEnd,
		bitsToObserved(oldBits), bitsToObserved(obsBits));
    }
return baseBitsToIupacNt(oldBits | obsBits);
}


void snpMaskSingle(char *snpBedFile, char *twoBitFile, char *outFile, char *diffObsFile)
{
/* snpMaskSingle - Print sequence using IUPAC ambiguous nucleotide codes for single base substitutions. */
struct lineFile *lfSnp = lineFileOpen(snpBedFile, TRUE);
struct twoBitFile *tbfGenomic = twoBitOpen(twoBitFile);
FILE *outMasked = mustOpen(outFile, "w");
FILE *diffObsOut = (diffObsFile == NULL) ? NULL : mustOpen(diffObsFile, "w");
long long expectedTotalSize = twoBitTotalSize(tbfGenomic);
long long totalMaskedSnps = 0, totalMaskedBases = 0, totalSize = 0;
int prevMaskedChromStart = -1;
struct dnaSeq *seq = NULL;
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
    if (chromEnd != chromStart + 1 ||
	!sameString(class, "single") ||
	weight != 1)
	continue;
    if (!seq || !sameString(seq->name, chrom))
	{
	if (seq)
	    {
	    faWriteNext(outMasked, seq->name, seq->dna, seq->size);
	    totalSize += seq->size;
	    dnaSeqFree(&seq);
	    }
	seq = twoBitReadSeqFrag(tbfGenomic, chrom, 0, 0);
	}
    boolean inRep = islower(seq->dna[chromStart]);
    char oldCode = toupper(seq->dna[chromStart]);
    char newCode = mergeBaseAndObserved(oldCode, observed, strand,
					chrom, chromStart, chromEnd, diffObsOut);
    if (oldCode != newCode)
	{
	if (inRep)
	    newCode = tolower(newCode);
	seq->dna[chromStart] = newCode;
	totalMaskedSnps++;
	if (chromStart != prevMaskedChromStart)
	    totalMaskedBases++;
	prevMaskedChromStart = chromStart;
	}
    }
if (seq)
    {
    faWriteNext(outMasked, seq->name, seq->dna, seq->size);
    totalSize += seq->size;
    dnaSeqFree(&seq);
    }
lineFileClose(&lfSnp);
twoBitClose(&tbfGenomic);
carefulClose(&outMasked);
verbose(1, "Masked %lld snps in %lld out of %lld genomic bases\n",
	totalMaskedSnps, totalMaskedBases, totalSize);
if (totalSize != expectedTotalSize)
    verbose(0, "%s has %lld total bases, but the total number of bases "
	    "in sequences for which we masked snps is %lld "
	    "(difference is %lld)\n", twoBitFile, expectedTotalSize,
	    totalSize, (expectedTotalSize - totalSize));
}

int main(int argc, char *argv[])
/* Check args and call snpMaskSingle. */
{
if (argc != 4 && argc != 5)
    usage();
char *diffObsFile = (argc == 5) ? argv[4] : NULL;
snpMaskSingle(argv[1], argv[2], argv[3], diffObsFile);
return 0;
}
