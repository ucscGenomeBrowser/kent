/* addMhcClones - Add info on missing MHC files to Greg's stuff.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "addMhcClones - Add info on missing MHC files to Greg's stuff.\n"
  "usage:\n"
  "   addMhcClones ~/gs/otherIn/mhc/clones ~/gs\n"
  "options:\n");
}

void addToSequenceInf(struct dnaSeq *seqList, char *fileName)
/* Append to sequence.inf file. */
{
FILE *f = mustOpen(fileName, "a");
struct dnaSeq *seq;
char cloneVer[256];

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    strcpy(cloneVer, seq->name);
    chopSuffixAt(cloneVer, '_');
    fprintf(f, "%s\t-\t%d\t3\t-\t6\t-\tSC\t-\n", cloneVer, seq->size);
    }
fclose(f);
}

void addToTrans(struct dnaSeq *seqList, char *fileName)
/* Append info to trans file. */
{
FILE *f = mustOpen(fileName, "a");
struct dnaSeq *seq;
char ncbiName[256];
char cloneVer[256];

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    strcpy(ncbiName, seq->name);
    subChar(ncbiName, '_', '~');
    strcpy(cloneVer, seq->name);
    chopSuffixAt(cloneVer, '_');
    fprintf(f, "%s\t%s (%s:1..%d)\n", seq->name, ncbiName, cloneVer, seq->size);
    }
fclose(f);
}

void addToFinf(struct dnaSeq *seqList, char *fileName)
/* Append info to finf file. */
{
FILE *f = mustOpen(fileName, "a");
struct dnaSeq *seq;
char ncbiName[256];
char cloneVer[256];

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    strcpy(ncbiName, seq->name);
    subChar(ncbiName, '_', '~');
    strcpy(cloneVer, seq->name);
    chopSuffixAt(cloneVer, '_');
    fprintf(f, "%s\t%s\t1\t%d\t1\t1,1/1\tw\n", ncbiName, cloneVer, seq->size);
    }
fclose(f);
}



void addMhcClones(char *cloneDir, char *gsDir)
/* addMhcClones - Add info on missing MHC files to Greg's stuff.. */
{
struct fileInfo *fileList = NULL, *fileEl;
struct dnaSeq *seqList = NULL, *seq;
char path[512];

fileList = listDirX(cloneDir, "*.fa", TRUE);
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    seq = faReadAllDna(fileEl->name);
    if (slCount(seq) != 1)
        errAbort("%s has %d sequences, can only handle one.", fileEl->name, slCount(seq));
    slAddHead(&seqList, seq);
    }
slReverse(&seqList);
printf("Read %d sequences\n", slCount(seqList));

sprintf(path, "%s/fin/trans", gsDir);
addToTrans(seqList, path);

sprintf(path, "%s/ffa/sequence.inf", gsDir);
addToSequenceInf(seqList, path);

sprintf(path, "%s/ffa/finished.finf", gsDir);
addToFinf(seqList, path);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
addMhcClones(argv[1], argv[2]);
return 0;
}
