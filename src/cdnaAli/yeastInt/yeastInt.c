/* yeastint - look for yeast intron (or other organism introns
 * where don't want to be tied to the whole work database.)
 */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "nt4.h"
#include "localmem.h"
#include "cdnaAli.h"

struct nt4Seq **chromNt4;

void usage()
/* Explain usage and abort. */
{
errAbort(
    "yeastInt - look for yeast (or other non-worm) introns in refiAli good.txt\n"
    "alignment output file.  Usage:\n"
    "    yeastInt good.txt ntDir out.fa out.gff out.txt\n");
}

int cloneAliReverseStrand(struct ali *ali)
/* Returns TRUE (1)  if the alignment of the clone EST came from is on minus
 * strand, FALSE (0) otherwise. */
{
struct cdna *cdna = ali->cdna;
char *name = cdna->name;
int nameLen = strlen(name);
boolean isReverse = FALSE;

if (nameLen > 1 && name[nameLen-1] == '3' && name[nameLen-2] == '.')
    isReverse = TRUE;
if (ali->strand == '-')
    isReverse = !isReverse;
return (isReverse ? 1 : 0);
}

int consensusStrand(struct cdnaRef *refList)
/* Return most common strand in refList. */
{
struct cdnaRef *ref;
int counts[2];

counts[0] = counts[1] = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    counts[cloneAliReverseStrand(ref->ali)] += 1;
    }
return (counts[0] >= counts[1] ? 0 : 1);
}

char intronStrand(DNA *start, int size, boolean *retPerfect)
/* Returns '+', '-' or '.' for forward, reverse, and who knows. */
{
DNA a,b,y,z;
int fCount = 0, rCount = 0;

a = start[0];
b = start[1];
y = start[size-2];
z = start[size-1];

if (a == 'g')
    ++fCount;
if (b == 't')
    ++fCount;
if (y == 'a')
    ++fCount;
if (z == 'g')
    ++fCount;

if (a == 'c')
    ++rCount;
if (b == 't')
    ++rCount;
if (y == 'a')
    ++rCount;
if (z == 'c')
    ++rCount;
*retPerfect = (fCount >= 4 || rCount >= 4);
if (fCount >= 3 || rCount >= 3)
    return (fCount > rCount) ? '+' : '-';
else
    return '.';
}

void writeIntron(FILE *f, struct feature *intron, DNA *dna, int dnaSize, char strand, int startExtra, int intronSize, int insideIntronSize)
{
int i;
struct cdnaRef *ref;
fprintf(f, "%d   %c %s ",
    intron->cdnaCount, strand,
    intron->gene);
for (i=0; i<startExtra; ++i)
    fputc(dna[i], f);
fputc('^', f);
for (i=startExtra; i<= startExtra+insideIntronSize; i++)
    fputc(dna[i], f);
fputc('.', f);
fputc('.', f);
fputc('.', f);
for (i=startExtra + intronSize - insideIntronSize; i<startExtra+intronSize; ++i)
    fputc(dna[i], f);
fputc('^', f);
for (i=startExtra+intronSize; i<dnaSize; ++i)
    fputc(dna[i], f);
for (ref = intron->cdnaRefs; ref != NULL; ref = ref->next)
    fprintf(f, " %s", ref->ali->cdna->name);
fputc('\n', f);
}

void convertToRna(char *dna)
/* Convert from lower case DNA to upper case RNA */
{
char c;
while ((c = *dna) != 0)
    {
    c = toupper(c);
    if (c == 'T')
        c = 'U';
    *dna++ = c;
    }
}

void writeDnaText(FILE *f, char *dna, int dnaSize, int lineSize)
/* Write out DNA to a file. */
{
while (dnaSize > 0)
    {
    if (lineSize > dnaSize) lineSize = dnaSize;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    dnaSize -= lineSize;
    }
}

void writeIntrons(struct feature *intronList, char *gffName, char *txtName, char *faName)
{
FILE *gff = mustOpen(gffName, "w");
FILE *txt = mustOpen(txtName, "w");
FILE *fa = mustOpen(faName, "w");
struct feature *intron;
int intronSize;
int intronCount = 0;
int idealExtra = 5;
int insideIntronSize = 10;
int startExtra, endExtra;
int dnaStart, dnaEnd, dnaSize;
struct nt4Seq *chrom;
DNA *dna;
char strand;
boolean sureAboutStrand;
int fCount=0, rCount=0, qCount=0;
static char strandIxToC[2] = {'+', '-'};
char cloneStrand;
int consMismatchCount = 0;
int nonconsMismatchCount = 0;

printf("Writing introns gff\n");
fprintf(gff, "##gff-version 1\n");
fprintf(gff, "##source-version cDnaIntrons 1.0\n");

for (intron = intronList; intron != NULL; intron = intron->next)
    {
    /* Get the dna for this intron, and a little extra. */
    chrom = chromNt4[ixInStrings(intron->chrom, chromNames, chromCount)];
    dnaStart = intron->start - idealExtra;
    if (dnaStart < 0)
        dnaStart = 0;
    intronSize = intron->end - intron->start;
    startExtra = intron->start - dnaStart;
    dnaEnd = intron->end + idealExtra;
    if (dnaEnd > chrom->baseCount)
        dnaEnd = chrom->baseCount;
    endExtra = dnaEnd - intron->end;
    dnaSize = dnaEnd - dnaStart;
    dna = nt4Unpack(chrom, dnaStart, dnaSize);
    /* Figure out what strand it's on by match to consensus sequence. */
    strand = intronStrand(dna + startExtra, intronSize, &sureAboutStrand);
    cloneStrand = strandIxToC[consensusStrand(intron->cdnaRefs)];
    if (strand == '.') strand = cloneStrand;
    if (cloneStrand != strand)
        {
        //warn("Mixed signals on %s intron strand pos %s:%d-%d", 
        //    (sureAboutStrand ? "consensus" : "non-consensus"),
        //    intron->chrom, intron->start, intron->end);
        if (sureAboutStrand)
            consMismatchCount += 1;
        else
            nonconsMismatchCount += 1;
        }
    if (strand == '.') ++qCount;
    else if (strand == '+') ++fCount;
    else if (strand == '-') ++rCount;

    /* Write line to gff file. */
    fprintf(gff, "%s\t%s\tintron\t%d\t%d\t%d\t%c\t.\t%s\n",
        intron->chrom, intron->cdnaRefs->ali->cdna->name, intron->start + 1, intron->end, 
        intron->cdnaCount, strand, intron->gene);

    /* Write line to txt file, reverse complementing if necessary. */
    if (strand == '-')
        {
        int temp;
        reverseComplement(dna, dnaSize);
        temp = startExtra;
        startExtra = endExtra;
        endExtra = temp;
        }
    convertToRna(dna);
    writeIntron(txt, intron, dna, dnaSize, strand, startExtra, intronSize, insideIntronSize);

    /* Write to fa file. */
    fprintf(fa, ">%s_%d_%d%c\n", intron->chrom, intron->start+1, 
        intron->end, strand);
    writeDnaText(fa, dna + startExtra, intronSize, 50);
     
    freeMem(dna);
    ++intronCount;
    }
fclose(fa);
fclose(gff);
fclose(txt);
printf("Wrote %d introns: %d + %d - %d ?\n", intronCount,
    fCount, rCount, qCount);
printf("%d consensus mismatches, %d nonconsensus mismatches on strand\n",
    consMismatchCount, nonconsMismatchCount);
}



int main(int argc, char *argv[])
{
char *goodName;
char *ntDir;
char *outFa, *outGff, *outTxt;
long startTime, endTime;
struct cdna *cdnaList;
struct feature *intronList;

/* Check command line. */
if (argc != 6)
    usage();
goodName = argv[1];
ntDir = argv[2];
outFa = argv[3];
outGff = argv[4];
outTxt = argv[5];

cdnaAliInit();

startTime = clock1000();
loadGenome(ntDir, &chromNt4, &chromNames, &chromCount);
endTime = clock1000();
printf("Loaded %d genome files in %f seconds\n", chromCount, (endTime - startTime)*0.001);

startTime = clock1000();
cdnaList = loadCdna(goodName);
endTime = clock1000();
printf("Loaded %s in %f seconds\n", goodName, (endTime - startTime)*0.001);

intronList = makeIntrons(cdnaList);
writeIntrons(intronList, outGff, outTxt, outFa);

return 0;
}
