/* mark0_1 - collect 0th and 1st order Markov models from a series of FA files */
#include "common.h"
#include "obscure.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "wormdna.h"

void usage()
/* Explain usage and exit. */
{
errAbort("mark0_1 collects and prints 0th and 1st order Markov models.\n"
         "usage:\n"
         "     mark0_1 few outFile file1.fa file2.fa file3.fa\n"
         "     mark0_1 many outFile fileWithListOfFaOnePerLine");
}

void addToMarks(DNA *dna, int dnaSize, int count0[5], int count1[5][5])
/* Add in counts from DNA into models */
{
int val, lastVal;
int i;

if (dnaSize < 1)
    return;
val = ntVal[dna[0]] + 1;
assert(val >= 0 && val <= 5);
count0[val] += 1;
for (i=1; i<dnaSize; ++i)
    {
    lastVal = val;
    val = ntVal[dna[i]] + 1;
    assert(val >= 0 && val <= 5);
    count0[val] += 1;
    count1[lastVal][val] += 1;
    }
}        

void collectMarkovs(char **faFiles, int faCount, char *outFileName)
/* Loop through each fa file and collect 1st and 2nd order Markov data
 * from it. */ 
{
int i,j;
char *faName;
FILE *out;
struct dnaSeq *seq;
int countMark0[5];
int countMark1[5][5];
static int ourOrder[5] = {A_BASE_VAL+1, C_BASE_VAL+1, G_BASE_VAL+1, T_BASE_VAL+1, 0};
static char ourNts[5] = "ACGTN";
int totalMark0 = 0, totalMark1 = 0;

/* Initialize with small psuedocounts. */
for (i=0; i<5; ++i)
    {
    countMark0[i] = 1;
    for (j=0; j<5; ++j)
        countMark1[i][j] = 1;
    }

/* Make frequencies histograms */
for (i=0; i<faCount; ++i)
    {
    faName = faFiles[i];
    printf("Processing %s\n", faName);
    seq = faReadDna(faName);
    addToMarks(seq->dna, seq->size, countMark0, countMark1);        
    }

/* Count up totals. */
for (i=0; i<5; ++i)
    {
    totalMark0 += countMark0[i];
    for (j=0; j<5; ++j)
        totalMark1 += countMark1[i][j];
    }

/* Write out results. */
out = mustOpen(outFileName, "w");

fprintf(out, "Markov 0 Model:\n      A     C     G     T     N\n");
fprintf(out, "    ");
for (i=0; i<5; ++i)
    fprintf(out, "%1.3f ", (double)countMark0[ourOrder[i]]/totalMark0);
fprintf(out, "\n\n");

fprintf(out, "Markov 1 Model:\n      A     C     G     T     N\n");
for (i=0; i<5; ++i)
    {
    int rowCount = 0;
    for (j=0; j<5; ++j)
        rowCount += countMark1[ourOrder[i]][j];
    fprintf(out, " %c  ", ourNts[i]); 
    for (j=0; j<5; ++j)
        fprintf(out, "%1.3f ", (double)countMark1[ourOrder[i]][ourOrder[j]]/rowCount);
    fprintf(out, "\n");
    }

fclose(out);
}

int main(int argc, char *argv[])
/* Execution starts here. */
{
char *command;
char *outFileName;

if (argc < 4)
    usage();
dnaUtilOpen();
command = argv[1];
outFileName = argv[2];
if (sameWord(command, "few"))
    collectMarkovs(argv+3, argc-3, outFileName);
else if (sameWord(command, "many"))
    {
    char **words;
    int wordCount;
    char *wordBuf;
    readAllWords(argv[3], &words, &wordCount, &wordBuf);
    collectMarkovs(words, wordCount, outFileName);
    freeMem(words);
    freeMem(wordBuf);
    }
else
    usage();
return 0;
}