/* faends - get just the ends of some fa sequences. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

struct dnaSeq *readAllFa(char *fileName)
{
struct dnaSeq *list = NULL, *el;
FILE *f;
int seqCount = 0;

uglyf("Loading %s\n", fileName);
f = mustOpen(fileName, "r");
while ((el = faReadOneDnaSeq(f, NULL, TRUE)) != NULL)
    {
    if (++seqCount % 5000 == 0)
        uglyf("loaded sequence #%d\n", seqCount);
    slAddHead(&list, el);
    }
fclose(f);
slReverse(&list);
return list;
}

int main(int argc, char *argv[])
{
char *inName, *outName;
int chopSize;
int seqSize;
struct dnaSeq *seqList = NULL, *seq;
FILE *f;
DNA *dna;
int seqCount = 0;
boolean tail = TRUE;
char *headOrTail;

if (argc != 5 || !isdigit(argv[3][0]))
    {
    errAbort("faends - write just the end part of each sequence to a new fa file\n"
             "usage:\n"
             "    faends in.fa tail/head size out.fa");
    }
inName = argv[1];
headOrTail = argv[2];
chopSize = atoi(argv[3]);
outName = argv[4];
seqList = readAllFa(inName);
if (sameWord(headOrTail, "head"))
    tail = FALSE;
else if (sameWord(headOrTail, "tail"))
    tail = TRUE;
else
    errAbort("Expecting head or tail as second parameter, got %s", headOrTail);

printf("Read %d sequences from %s\n", slCount(seqList), inName); 
f = mustOpen(outName, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    seqSize = seq->size;
    dna = seq->dna;
    if (seqSize >= chopSize-3)
        {
        int chop = chopSize;
        if (chop > seqSize-3)
            chop = seqSize-3;
        if (tail)
            dna += seqSize - chop;
        else
            dna[chop] = 0;
        }
    assert(seq->name != NULL);
    assert(dna != NULL);
    fprintf(f, ">%s\n%s\n", seq->name, dna);
    }
fclose(f);
printf("Done writing %s %d bases of each sequence to %s\n", 
    (tail ? "last" : "first"), chopSize, outName);
return 0;
}