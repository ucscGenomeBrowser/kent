/* vcfFilter - Apply filters to VCF data.  Output goes to stdout. */
#include "common.h"
#include "bits.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vcf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vcfFilter - Apply filters to VCF data.  Output goes to stdout.\n"
  "usage:\n"
  "   vcfFilter [options] input.vcf\n"
  "options:\n"
  "   -excludeVcf=F   Discard variants at positions in VCF file F.\n"
  "                   (CHROM values must match input.vcf's CHROM values.)\n"
  "   -minAc=N        Discard alleles with fewer than N observations.\n"
  "                   Discard variants with no remaining alleles.\n"
  "                   Update AC in INFO column; if input has genotypes then\n"
  "                   update AN in INFO column and recode genotypes.\n"
  "                   Note: if input has genotypes, then incoming AC and AN\n"
  "                   are ignored, i.e. genotypes are trusted more than AC/AN.\n"
  "   -rename         Replace the ID value with a comma-separated list of\n"
  "                   <ref><pos><alt> names, one for each alt (after -minAc)\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    { "excludeVcf", OPTION_STRING },
    { "minAc", OPTION_INT },
    { "rename", OPTION_BOOLEAN },
    {NULL, 0},
};

// TODO: move this to vcf.h
#define VCF_MIN_COLUMNS 8

struct dyBits
/* Dynamically sized bit array */
    {
    Bits *bits;
    int size;
    };

static void dyBitsSet(struct dyBits *db, int ix, boolean value)
/* Set db->bits[ix] to value, reallocating if db->size is insufficient. */
{
if (ix >= db->size)
    {
    int newSize = db->size;
    while (ix >= newSize)
        newSize *= 2;
    db->bits = bitRealloc(db->bits, db->size, newSize);
    }
if (value)
    bitSetOne(db->bits, ix);
else
    bitClearOne(db->bits, ix);
}

static struct hash *getExcludeChromPos()
/* If -excludeVcf is given, parse the file and hash the {chrom, pos} pairs to exclude. */
{
struct hash *excludeChromPos = NULL;
char *excludeVcf = optionVal("excludeVcf", NULL);
if (excludeVcf)
    {
    struct lineFile *lf = lineFileOpen(excludeVcf, TRUE);
    excludeChromPos = hashNew(0);
    int excludeCount = 0;
    char *line;
    while (lineFileNext(lf, &line, NULL))
        {
        // Ignore header.
        if (line[0] != '#')
            {
            // Just use CHROM and POS columns.
            char *words[16];
            int wordCount = chopTabs(line, words);
            lineFileExpectAtLeast(lf, VCF_MIN_COLUMNS, wordCount);
            struct hashEl *hel = hashLookup(excludeChromPos, words[0]);
            struct dyBits *db = NULL;
            if (hel == NULL)
                {
                AllocVar(db);
                db->size = (32 * 1024);  // Sufficient for SARS-CoV-2
                db->bits = bitAlloc(db->size);
                hashAdd(excludeChromPos, words[0], db);
                }
            else
                db = hel->val;
            dyBitsSet(db, atol(words[1]), TRUE);
            excludeCount++;
            }
        }
    lineFileClose(&lf);
    verbose(2, "Read %d positions to exclude from %s\n", excludeCount, excludeVcf);
    }
return excludeChromPos;
}

static boolean chromPosAreExcluded(struct hash *excludeChromPos, char **words)
/* Return TRUE if the CHROM and POS columns are found in excludeChromPos.  Don't call on NULL. */
{
char *chrom = words[0];
int ix = atol(words[1]);
struct dyBits *db = hashFindVal(excludeChromPos, chrom);
if (db && ix < db->size)
    return bitReadOne(db->bits, ix);
return FALSE;
}

struct alCount
// Container for an allele and count so alleles can be sorted by count.
    {
    char *allele;
    int count;
    };

int alCountCmpDesc(const void *a, const void *b)
/* Compare counts of two alleles for sorting in descending order. */
{
const struct alCount *alA = (const struct alCount *)a;
const struct alCount *alB = (const struct alCount *)b;
return alB->count - alA->count;
}

static void printWithNewAlts(char **words, int wordCount, struct alCount *newAlts, int newAltCount,
                             int newAn, int *gtAlCodes, boolean rename)
/* If at least one alt allele is left after filtering, print out the variant with updated ALT,
 * INFO and (if applicable) genotypes; if rename then replace ID too. */
{
if (newAltCount > 0)
    {
    // Sort to put the most common alternate allele first.
    qsort(newAlts, newAltCount, sizeof(*newAlts), alCountCmpDesc);
    char *pos = words[1];
    char *ref = words[3];
    int i;
    printf("%s\t%s\t", words[0], pos);
    if (rename)
        {
        printf("%s%s%s", ref, pos, newAlts[0].allele);
        for (i = 1;  i < newAltCount;  i++)
            printf(",%s%s%s", ref, pos, newAlts[i].allele);
        }
    else
        printf("%s", words[2]);
    printf("\t%s\t%s", ref, newAlts[0].allele);
    for (i = 1;  i < newAltCount;  i++)
        printf(",%s", newAlts[i].allele);
    printf("\t%s\t%s\t", words[5], words[6]);
    printf("AC=%d", newAlts[0].count);
    for (i = 1;  i < newAltCount;  i++)
        printf(",%d", newAlts[i].count);
    if (newAn > 0)
        printf(";AN=%d", newAn);
    char *oldInfoWords[1024];
    int oldInfoWordCount = chopString(words[7], ";", oldInfoWords, ArraySize(oldInfoWords));
    for (i = 0;  i < oldInfoWordCount;  i++)
        {
        if (!startsWith("AC=", oldInfoWords[i]) &&
            (!startsWith("AN=", oldInfoWords[i]) || newAn <= 0))
            printf(";%s", oldInfoWords[i]);
        }
    if (wordCount > VCF_NUM_COLS_BEFORE_GENOTYPES && gtAlCodes)
        {
        printf("\tGT");
        for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < wordCount;  i++)
            {
            if (gtAlCodes[i] < 0)
                printf("\t.");
            else
                printf("\t%d", gtAlCodes[i]);
            }
        }
    printf("\n");
    }
}

static void filterMinAcFromGenotypes(char **words, int wordCount, int minAc, boolean rename)
/* Tally up counts of each alt allele from genotype columns.  Discard any allele with fewer than
 * minAc genotypes.  If no alleles remain, don't print out; otherwise, update the ALT column,
 * update AC and AN in the INFO column, and recode genotype columns. */
{
char *alts[1024];
int altCount = chopCommas(words[4], alts);
int altCounts[altCount];
memset(altCounts, 0, sizeof(altCounts));
int missingCount = 0;
int gtAlCodes[wordCount];
int i;
for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < wordCount;  i++)
    {
    if (strchr(words[i], '|') || strchr(words[i], '/'))
        errAbort("Sorry, only monoploid genotypes are supported (this was written for SARS-CoV-2 data).");
    if (words[i][0] == '.')
        {
        missingCount++;
        gtAlCodes[i] = -1;
        }
    else if (isdigit(words[i][0]))
        {
        int alIx = atol(words[i]);
        gtAlCodes[i] = alIx;
        if (alIx > 0)
            altCounts[alIx-1]++;
        }
    else
        errAbort("Unrecognized genotype value '%s' (%s\t%s\t%s\t%s\t%s",
                 words[i], words[0], words[1], words[2], words[3], words[4]);
    }
int newAltCount = 0;
struct alCount newAlts[altCount];
for (i = 0;  i < altCount;  i++)
    if (altCounts[i] >= minAc)
        {
        newAlts[newAltCount].allele = alts[i];
        newAlts[newAltCount].count = altCounts[i];
        newAltCount++;
        }
int newAn = wordCount - VCF_NUM_COLS_BEFORE_GENOTYPES - missingCount;
// Recode genotypes
int oldToNewIx[altCount+1];
for (i = 0;  i < altCount;  i++)
    {
    // Default to reference (ix = 0) for discarded alleles, because that is effectively what we
    // do when we discard the whole variant because no allele passes the minAc threshold.
    int newIx = 0;
    int j;
    for (j = 0;  j < newAltCount;  j++)
        if (sameString(newAlts[j].allele, alts[i]))
            {
            newIx = j+1;
            break;
            }
    int oldIx = i+1;
    oldToNewIx[oldIx] = newIx;
    }
for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < wordCount;  i++)
    if (gtAlCodes[i] > 0)
        gtAlCodes[i] = oldToNewIx[gtAlCodes[i]];
printWithNewAlts(words, wordCount, newAlts, newAltCount, newAn, gtAlCodes, rename);
}

static void filterMinAcFromInfo(char **words, int wordCount, int minAc, boolean rename)
/* Parse AC from INFO column; discard alleles with count < minAc.  If no alleles remain,
 * don't print out; otherwise, update the ALT column and AC in the INFO column. */
{
char *alts[1024];
char altCpy[strlen(words[4])+1];
safecpy(altCpy, sizeof altCpy, words[4]);
int altCount = chopCommas(altCpy, alts);
struct alCount newAlts[altCount];
int newAltCount = 0;
char infoCpy[strlen(words[7])+1];
safecpy(infoCpy, sizeof infoCpy, words[7]);
char *infoWords[1024];
int infoWordCount = chopString(infoCpy, ";", infoWords, ArraySize(infoWords));
int i;
for (i = 0;  i < infoWordCount;  i++)
    if (startsWith("AC=", infoWords[i]))
        {
        char *acStr = infoWords[i] + strlen("AC=");
        char *altCountWords[altCount+1];
        int altCountWordCount = chopCommas(acStr, altCountWords);
        if (altCountWordCount != altCount)
            errAbort("Inconsistent number of alts vs AC= counts: '%s' vs '%s'",
                     words[4], words[7]);
        int j;
        for (j = 0;  j < altCount;  j++)
            {
            int count = atol(altCountWords[j]);
            if (count >= minAc)
                {
                newAlts[newAltCount].allele = alts[j];
                newAlts[newAltCount].count = count;
                newAltCount++;
                }
            }
        break;
        }
printWithNewAlts(words, wordCount, newAlts, newAltCount, 0, NULL, rename);
}

static void maybeRename(struct dyString *dy, char **words, boolean rename)
/* If rename is TRUE then set ID (using dy) to comma-sep list of <ref><pos><alt> for each alt;
 * otherwise just copy exiting id into dy. */
{
dyStringClear(dy);
if (rename)
    {
    char *alts[1024];
    char altCpy[strlen(words[4])+1];
    safecpy(altCpy, sizeof altCpy, words[4]);
    int altCount = chopCommas(altCpy, alts);
    char *ref = words[3];
    char *pos = words[1];
    dyStringPrintf(dy, "%s%s%s", ref, pos, alts[0]);
    int i;
    for (i = 1;  i < altCount;  i++)
        dyStringPrintf(dy, ",%s%s%s", ref, pos, alts[i]);
    }
else
    {
    dyStringAppend(dy, words[2]);
    }
words[2] = dy->string;
}

static void printWords(char **words, int wordCount)
/* Print words, tab-separated, to stdout, followed by a newline. */
{
printf("%s", words[0]);
int i;
for (i = 1;  i < wordCount; i++)
    printf("\t%s", words[i]);
printf("\n");
}

void vcfFilter(char *vcfIn)
/* vcfFilter - Apply filters to VCF data.  Output goes to stdout. */
{
struct lineFile *lf = lineFileOpen(vcfIn, TRUE);
int minAc = optionInt("minAc", 0);
boolean rename = optionExists("rename");
struct hash *excludeChromPos = getExcludeChromPos();
struct dyString *dyScratch = dyStringNew(0);
int headerColCount = 0;
// VCF with >1M samples (for SARS-CoV-2) causes stack problems / SEGV if we declare words on stack,
// so allocate it once we know how many columns to expect:
char **words = NULL;
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("#CHROM", line))
        {
        headerColCount = chopString(line, "\t", NULL, 0);
        lineFileExpectAtLeast(lf, VCF_NUM_COLS_BEFORE_GENOTYPES+1, headerColCount);
        AllocArray(words, headerColCount+1);
        chopByChar(line, '\t', words, headerColCount+1);
        printWords(words, headerColCount);
        }
    else if (line[0] == '#')
        {
        printf("%s\n", line);
        }
    else
        {
        if (headerColCount < VCF_MIN_COLUMNS)
            errAbort("Error: missing a tab-separated #CHROM line in vcf header.");
        int wordCount = chopByChar(line, '\t', words, headerColCount+1);
        lineFileExpectWords(lf, headerColCount, wordCount);
        if (excludeChromPos && chromPosAreExcluded(excludeChromPos, words))
            continue;
        if (minAc > 0)
            {
            // Renaming happens (if specified) after filtering alleles by minAc,
            // while printing output, inside these functions:
            if (wordCount > VCF_NUM_COLS_BEFORE_GENOTYPES)
                filterMinAcFromGenotypes(words, wordCount, minAc, rename);
            else
                filterMinAcFromInfo(words, wordCount, minAc, rename);
            }
        else
            {
            maybeRename(dyScratch, words, rename);
            printWords(words, wordCount);
            }
        }
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
vcfFilter(argv[1]);
return 0;
}
