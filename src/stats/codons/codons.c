/* Codons - Input is GenBank format.  Output is 0th and 1st order Markov
 * histogram of codon usage. */
#include "common.h"
#include "obscure.h"
#include "dnautil.h"

boolean coding = TRUE;
boolean reverse = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort("Codons - counts up codon frequencies in GenBank file\n"
         "usage:\n"
         "    codons many outFile.cod outFile.fa fileWithListOfFileNames\n"
         "    codons few outFile.cod outFile.fa file1.gb file2.gb ...\n");
}

struct faOut
/* Manage base at a time output to fa. */
{
FILE *f;
int lineSize;
int linePos;
};

struct faOut *faOutOpen(char *fileName, int lineSize)
/* Return a newly opened faOut */
{
struct faOut *fao;
AllocVar(fao);
fao->f = mustOpen(fileName, "w");
fao->lineSize = lineSize;
fao->linePos = 0;
return fao;
}

void faOutClose(struct faOut **pFaOut)
/* Close down an faOut */
{
struct faOut *fao = *pFaOut;
if (fao != NULL)
    {
    carefulClose(&fao->f);
    freez(pFaOut);
    }
}

static void faOutEndLine(struct faOut *fao)
/* Finish current line if non-empty. */
{
if (fao->linePos)
    {
    fputc('\n', fao->f);
    fao->linePos = 0;
    }
}

void faOutStartSeq(struct faOut *fao, char *name)
/* Start a new named sequence. */
{
faOutEndLine(fao);
fprintf(fao->f, ">%s\n", name);
fao->linePos = 0;
}

void faOutEndSeq(struct faOut *fao)
/* Finish up current sequence. */
{
faOutEndLine(fao);
}

void faOutBase(struct faOut *fao, DNA base)
/* Write one base to fa file. */
{
fputc(base, fao->f);
if (++fao->linePos >= fao->lineSize)
    {
    faOutEndLine(fao);
    }
}

static int frame = 0;
static int lastVal = -1;
int cdsStart, cdsEnd;
int baseIx;
int codonCount = 0;
int cdsCount = 0;
char *stopCodons[3] = { "taa", "tag", "tga"};

void initFrame()
/* Set up as if just starting coding region. */
{
frame = 0;
lastVal = -1;
baseIx = 0;
cdsStart = 0;
cdsEnd = 0x7fffffff;
}

void addBase(int freq[64], int mark[64][64], DNA base, struct faOut *fao)
/* Add a single base to codon model. */
{
static DNA codon[3];

if (baseIx >= cdsStart && baseIx < cdsEnd)
    {
    faOutBase(fao, base);
    codon[frame] = base;
    if ((frame += 1) == 3)
        {
        int val;
        if (reverse)
            reverseComplement(codon,3);
        val = codonVal(codon);
        if (val >= 0)
            {
            freq[val] += 1;
            if (lastVal >= 0)
                {
                if (reverse)
                    mark[val][lastVal] += 1;
                else
                    mark[lastVal][val] += 1;
                }
            }
        lastVal = val;
        frame = 0;
        ++codonCount;
        }
    }
++baseIx;
}

boolean hasSuffix(char *name, char *suffix)
/* Returns TRUE if name ends in suffix. */
{
int len = strlen(name) - strlen(suffix);
return sameWord(name+len, suffix);
}

boolean isGb(char *name)
/* Return true if file suffix indicates it's a genbank file. */
{
return hasSuffix(name, ".gb");
}

void addFa(char *fileName, int freq[64], int mark[64][64], struct faOut *fao)
/* Add in counts of codons from one FA file. */
{
FILE *f = mustOpen(fileName, "r");
char line[1024];
char *linePt;
int lineCount = 0;
char c;
static boolean firstSeq = FALSE;

initFrame();
while (fgets(line, sizeof(line), f))
    {
    if (++lineCount % 100000 == 0)
        printf("Processing line %d of %s\n", lineCount, fileName);
    if (line[0] == '>')
        {
        initFrame();
        faOutStartSeq(fao, line+1);
        }
    else
        {
        linePt = line;
        while ((c = *linePt++) != 0)
            {
            if (!isdigit(c) && !isspace(c))
                {
                addBase(freq, mark, c, fao);
                }
            }
        }
    }
fclose(f);
}

void addGb(char *fileName, int freq[64], int mark[64][64], struct faOut *faOut)
/* Add in counts of codons from one GenBank file. */
{
FILE *f = mustOpen(fileName, "r");
char line[1024];
char origLine[1024];
char locusName[128];
int lineCount = 0;
char *words[128];
int wordCount;
char *linePt;
boolean gotCds = FALSE;
boolean inDna = FALSE;
boolean gotMrna;

while (fgets(line, sizeof(line), f))
    {
    if (++lineCount % 100000 == 0)
        printf("Processing line %d of %s\n", lineCount, fileName);
    linePt = skipLeadingSpaces(line);
    if (!linePt[0])
        continue;
    if (startsWith("LOCUS", line))
        {
        initFrame();
        gotCds = FALSE;
        inDna = FALSE;
        wordCount = chopLine(line, words);
        if ((gotMrna =  sameString("mRNA", words[4])) == TRUE)
            {
            /* Truncate name if necessary and store it for later. */
            char *name = words[1];
            int len = strlen(name);
            if (len >= sizeof(locusName))
                name[sizeof(locusName)-1] = 0;
            strcpy(locusName, name);
            }
        }
    else if (startsWith("//", line))
        {
        gotMrna = FALSE;
        faOutEndSeq(faOut);
        }
    else if (gotMrna && startsWith("     CDS", line))
        {
        strcpy(origLine, line);
        wordCount = chopLine(linePt, words);
        if (wordCount == 2)
            {
            char *parts[2];
            int partCount;
            char c1, c2;
            partCount = chopString(words[1], ".", parts, ArraySize(parts));
            if (partCount == 2)
                {
                c1 = parts[0][0];
                c2 = parts[1][0];
                if (isdigit(c1) && isdigit(c2))
                    {
                    cdsStart = atoi(parts[0]) - 1;
                    cdsEnd = atoi(parts[1]);
                    gotCds = TRUE;
                    ++cdsCount;
                    }
                else if ((!isdigit(c1) && c1 != '<' && c1 != '>') ||
                         (!isdigit(c2) && c2 != '<' && c2 != '>') )
                    {
                    warn("Weird CDS line %d of %s, skipping:\n%s", lineCount, fileName, origLine);
                    }
                }
            else 
                {
                warn("Strange CDS line %d of %s, skipping:\n%s", lineCount, fileName, origLine);
                }
            }
        else
            {
            warn("Odd CDS line %d of %s, skipping:\n%s", lineCount, fileName, origLine);
            }
        }
    else if (gotCds && startsWith("ORIGIN", line))
        {
        inDna = TRUE;
        faOutStartSeq(faOut, locusName);
        }
    else if (inDna && isdigit(linePt[0]))
        {
        char c;
        while ((c = *linePt++) != 0)
            {
            if (!isspace(c) && !isdigit(c))
                {
                addBase(freq, mark, c, faOut);
                }
            }
        }
    }
fclose(f);
}

void collectCodons(char *gbNames[], int gbCount, char *outFile, char *faOutFile)
/* Collect statistics on codons from all files and print. */
{
FILE *out = mustOpen(outFile, "w");
char *fileName;
int i,j;
int freq[64];
int freqTotal;
int mark[64][64];
int markRowTotal[64];
int val;
struct faOut *fao = faOutOpen(faOutFile, 50);

/* Initialize with pseudocounts. */
for (i=0; i<64; ++i)
    {
    freq[i] = 0;
    for (j=0; j<64; ++j)
        mark[i][j] = 1;
    }
/* Add in real counts from each file. */
for (i = 0; i<gbCount; ++i)
    {
    fileName = gbNames[i];
    printf("processing %s\n", fileName);
    if (isGb(fileName))
        addGb(fileName, freq, mark, fao);
    else
        addFa(fileName, freq, mark, fao);
    }

/* Explicitly set stop codon counts to zero. (They should be low anyway.) */
if (coding)
    {
    for (i=0; i<ArraySize(stopCodons); ++i)
        {
        if (reverse)
            reverseComplement(stopCodons[i], 3);
        val = codonVal(stopCodons[i]);
        freq[val] = 0;
        for (j=0; j<64; ++j)
            {
            mark[j][val] = 0;
            }
        }
    }

/* Total up counts for 0th order model. */
freqTotal = 0;
for (i=0; i<64; ++i)
    {
    freqTotal += freq[i];
    }

/* Total up row counts for 1st order model. */
for (i=0; i<64; ++i)
    {
    markRowTotal[i] = 0;
    for (j=0; j<64; ++j)
        {
        markRowTotal[i] += mark[i][j];
        }
    }

/* Set up stop codon rows in mark to zero too for prettier display. */
if (coding)
    {
    for (i=0; i<ArraySize(stopCodons); ++i)
        {
        val = codonVal(stopCodons[i]);
        for (j=0; j<64; ++j)
            mark[val][j] = 0;
        }
    }

fprintf(out, "Tables based on %d codons in %d cDNAs\n\n", codonCount, cdsCount);

fprintf(out, "Markov 0\n");
/* Print labels on top. */
for (i=0; i<3; ++i)
    {
    fprintf(out, "     ");
    for (j=0; j<64; ++j)
        {
        DNA *codon = valToCodon(j);
        fprintf(out, "%c   ", codon[i]);
        }
    fprintf(out, "\n");
    }
/* Print counts in permill */
fprintf(out, "   ");
for (i=0; i<64; ++i)
    {
    int milli = round(10000.0 * freq[i] / freqTotal);
    fprintf(out, "%3d ", milli);
    }
fprintf(out, "\n\n");


fprintf(out, "Markov 1\n");
/* Print labels on top. */
for (i=0; i<3; ++i)
    {
    fprintf(out, "      ");
    for (j=0; j<64; ++j)
        {
        DNA *codon = valToCodon(j);
        fprintf(out, " %c   ", codon[i]);
        }
    fprintf(out, "\n");
    }
/* Print left labels and counts. */
for (i=0; i<64; ++i)
    {
    fprintf(out, "%s ", valToCodon(i) );
    for (j=0; j<64; ++j)
        {
        int milli = round(10000.0 * mark[i][j] / markRowTotal[i]);
        fprintf(out, "%4d ", milli);
        }
    fprintf(out, "\n");
    }
printf("\n");

fclose(out);
faOutClose(&fao);
}

int main(int argc, char *argv[])
{
char *command;
char *codOutFile;
char *faOutFile;

if (argc < 5)
    usage();
dnaUtilOpen();
command = argv[1];
codOutFile = argv[2];
faOutFile = argv[3];
if (sameString(command, "many"))
    {
    char **words;
    int wordCount;
    char *wordBuf;
    readAllWords(argv[4], &words, &wordCount, &wordBuf);
    collectCodons(words, wordCount, codOutFile, faOutFile);
    freeMem(words);
    freeMem(wordBuf);
    }
else if (sameString(command, "few"))
    {
    collectCodons(argv+4, argc-4, codOutFile, faOutFile);
    }
else
    usage();
return 0;
}