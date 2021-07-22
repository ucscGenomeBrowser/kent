/* vcfRenameAndPrune - Rename or remove samples from VCF with genotypes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "vcf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vcfRenameAndPrune - Rename samples in VCF; if new name not found, remove sample.\n"
  "usage:\n"
  "   vcfRenameAndPrune vcfIn.vcf[.gz] renaming.txt vcfOut.vcf\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  "renaming.txt has two whitespace-separated columns: old name (must uniquely match\n"
  "some sample named in #CHROM header line) and new name.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void vcfRenameAndPrune(char *vcfInFile, char *renamingFile, char *vcfOutFile)
/* vcfRenameAndPrune - Rename or remove samples from VCF with genotypes. */
{
struct hash *renaming = hashTwoColumnFile(renamingFile);
struct lineFile *lf = lineFileOpen(vcfInFile, TRUE);
FILE *outF = mustOpen(vcfOutFile, "w");
int headerColCount = 0;
int keeperCountMax = hashNumEntries(renaming);
int *keeperColumns;
AllocArray(keeperColumns, keeperCountMax);
int keeperCount = 0;
int keeperIx = 0;
// VCF with >1M samples (for SARS-CoV-2) causes stack problems / SEGV if we declare words on stack,
// so allocate it once we know how many columns to expect:
char **words = NULL;
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("#CHROM", line))
        {
        // Parse & replace sample names, build array of genotype columns that we're keeping
        headerColCount = chopString(line, "\t", NULL, 0);
        lineFileExpectAtLeast(lf, VCF_NUM_COLS_BEFORE_GENOTYPES+1, headerColCount);
        AllocArray(words, headerColCount+1);
        chopByChar(line, '\t', words, headerColCount+1);
        fputs(words[0], outF);
        int i;
        for (i = 1;  i < VCF_NUM_COLS_BEFORE_GENOTYPES;  i++)
            fprintf(outF, "\t%s", words[i]);
        for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < headerColCount;  i++)
            {
            char *newName = hashFindVal(renaming, words[i]);
            if (newName)
                {
                fprintf(outF, "\t%s", newName);
                if (keeperIx >= keeperCountMax)
                    lineFileAbort(lf, "Too many matching names in #CHROM line -- "
                                  "duplicate values in input? "
                                  "(%d values in renaming, too many matching names at column %d",
                                  keeperCountMax, i);
                keeperColumns[keeperIx++] = i;
                }
            }
        keeperCount = keeperIx;
        fputc('\n', outF);
        verbose(2, "Found %d keepers (out of %d genotype columns and %d entries in %s)\n",
                keeperCount, headerColCount - VCF_NUM_COLS_BEFORE_GENOTYPES, keeperCountMax,
                renamingFile);
        }
    else if (line[0] == '#')
        {
        // Pass through other header lines
        fputs(line, outF);
        fputc('\n', outF);
        }
    else
        {
        // Data line: print out only the genotype columns that we're keeping
        if (headerColCount == 0)
            lineFileAbort(lf, "Missing #CHROM header line -- can't rename.");
        int wordCount = chopByChar(line, '\t', words, headerColCount+1);
        lineFileExpectWords(lf, headerColCount, wordCount);
        // Recompute the counts of reference and alternate alleles in genotypes that we're keeping.
        // Keep only the alternate alleles that have a nonzero count.
        // Discard a row if there are no alternate alleles with nonzero count.
        char altCopy[strlen(words[4])+1];
        safecpy(altCopy, sizeof altCopy, words[4]);
        int altCount = chopString(altCopy, ",", NULL, 0);
        char *alts[altCount];
        chopCommas(altCopy, alts);
        int newAltCount = 0;
        char *newAlts[altCount];
        int newAltCounts[altCount];
        memset(newAltCounts, 0, sizeof newAltCounts);
        int altIxOldToNew[altCount];
        int i;
        for (i = 0;  i < altCount;  i++)
            altIxOldToNew[i] = -1;
        int totalCalls = keeperCount;
        for (i = 0;  i < keeperCount;  i++)
            {
            char *gt = words[keeperColumns[i]];
            if (sameString(gt, "."))
                totalCalls--;
            else if (!isAllDigits(gt))
                lineFileAbort(lf, "Expected genotype to be '.' or a number, but got '%s' "
                              "in keeperColumns[%d] = column %d",
                              gt, i, keeperColumns[i]);
            else
                {
                int alIx = atoi(gt);
                if (alIx > 0)
                    {
                    // Alternate allele; if this is the first time we've seen this one, add it to
                    // newAlts.
                    int oldAltIx = alIx - 1;
                    int newAltIx = altIxOldToNew[oldAltIx];
                    if (newAltIx < 0)
                        {
                        newAltIx = newAltCount++;
                        newAlts[newAltIx] = alts[oldAltIx];
                        newAltCounts[newAltIx] = 1;
                        altIxOldToNew[oldAltIx] = newAltIx;
                        }
                    else
                        newAltCounts[newAltIx]++;
                    // Update gt, i.e. words[keeperColumns[i]], with the new allele index.
                    int newAlIx = newAltIx + 1;
                    char newGt[16];
                    safef(newGt, sizeof newGt, "%d", newAlIx);
                    if (strlen(newGt) <= strlen(gt))
                        safecpy(gt, strlen(gt)+1, newGt);
                    else
                        // Extremely rare: single-digit ix to double-digit ix.  Leak a little mem.
                        words[keeperColumns[i]] = cloneString(newGt);
                    }
                }
            }
        if (newAltCount > 0)
            {
            // Write out line with updated alts, info, genotype columns
            fprintf(outF, "%s\t%s\t%s\t%s\t%s", words[0], words[1], words[2], words[3],
                    newAlts[0]);
            for (i = 1;  i < newAltCount;  i++)
                fprintf(outF, ",%s", newAlts[i]);
            fprintf(outF, "\t%s\t%s\tAC=%d", words[5], words[6], newAltCounts[0]);
            for (i = 1;  i < newAltCount;  i++)
                fprintf(outF, ",%d", newAltCounts[i]);
            fprintf(outF, ";AN=%d\tGT", totalCalls);
            for (i = 0;  i < keeperCount;  i++)
                fprintf(outF, "\t%s", words[keeperColumns[i]]);
            fputc('\n', outF);
            }
        }
    }
lineFileClose(&lf);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
vcfRenameAndPrune(argv[1], argv[2], argv[3]);
return 0;
}
