/* fullalte - Extracts full 3' end of intron based on a fragment.
 * Used to fill out alt-splicing intron before skipped exon data
 * set, which originally only had last 50 bp of intron. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

struct faSeq
    {
    struct faSeq *next;
    struct dnaSeq *seq;
    char *commentLine;
    };

struct faSeq *faReadAll(char *fileName)
/* Read all sequences in FA file. */
{
FILE *f = mustOpen(fileName, "r");
struct faSeq *list = NULL, *el;
struct dnaSeq *seq;
char *comment;

while (faReadNext(f, NULL, TRUE, &comment, &seq) )
    {
    AllocVar(el);
    el->seq = seq;
    el->commentLine = comment;
    slAddHead(&list, el);
    }
fclose(f);
slReverse(&list);
return list;
}

void writeLines50(FILE *f, DNA *dna)
/* Write DNA 50 bp to a line. */
{
int sizeLeft = strlen(dna);
int lineSize;
while (sizeLeft > 0)
    {
    lineSize = 50;
    if (lineSize > sizeLeft)
        lineSize = sizeLeft;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    sizeLeft -= lineSize;
    }
}

int main(int argc, char *argv[])
{
char *fragName, *fullName, *outName;
FILE *out;
struct faSeq *fragList, *fullList;
struct faSeq *fragEl, *fullEl;
struct dnaSeq *fragSeq, *fullSeq;

if (argc != 4)
    {
    errAbort("fullAltE - extracts full intron from fragments\n"
             "usage\n"
             "   fullAltE frags.fa introns.fa output.fa\n");
    }
dnaUtilOpen();
fragName = argv[1];
fullName = argv[2];
outName = argv[3];

printf("Loading %s\n", fragName);
fragList = faReadAll(fragName);
printf("Loading %s\n", fullName);
fullList = faReadAll(fullName);
printf("Creating %s\n", outName);
out = mustOpen(outName, "w");
for (fragEl = fragList; fragEl != NULL; fragEl = fragEl->next)
    {
    boolean foundMatch = FALSE;
    fragSeq = fragEl->seq;
    for (fullEl = fullList; fullEl != NULL; fullEl = fullEl->next)
        {
        fullSeq = fullEl->seq;
        if (strstr(fullSeq->dna, fragSeq->dna) != NULL)
            {
            fprintf(out, "%s", fragEl->commentLine);
            writeLines50(out, fullSeq->dna+5);
            foundMatch = TRUE;
            break;
            }
        }
    if (!foundMatch)
        warn("Couldn't find match for %s", fragSeq->name);
    }

fclose(out);
return 0;
}