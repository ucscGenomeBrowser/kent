/* checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent\n"
  "usage:\n"
  "   checkNt ctg_ng.agp ctg_coords ctg.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct nt
    {
    char *next;	/* Next in list. */
    char *name;	/* Name - allocated in hash. */
    int agpSize;	/* Size in AGP file. */
    int faSize;	/* Size in fa file. */	
    boolean inCoor;  /* True if in ctg_coords file. */
    };

struct nt *findNt(struct hash *ntHash, char *name)
/* Find NT in hash or create it if it doesn't exist yet. */
{
struct nt *nt = hashFindVal(ntHash, name);
if (nt == NULL)
    {
    AllocVar(nt);
    hashAddSaveName(ntHash, name, nt, &nt->name);
    }
return nt;
}

void addAgpSize(struct hash *ntHash, char *fileName)
/* Read an agp file and store size of each NT contig. */
{
char *row[5];
int wordCount;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((wordCount = lineFileChop(lf, row)) > 0)
    {
    lineFileExpectWords(lf, ArraySize(row), wordCount);
    if (row[4][0] != 'N' && row[4][0] != 'U')
        {
	int end = lineFileNeedNum(lf, row, 2);
	struct nt *nt = findNt(ntHash, row[0]);
	if (nt->agpSize < end) nt->agpSize = end;
	}
    }
lineFileClose(&lf);
}

void addNtSize(struct hash *ntHash, char *fileName)
/* Add size of NT contig from fa file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dnaSeq seq;
char *words[3];
ZeroVar(&seq);

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    struct nt *nt;
    int wordCount;
    wordCount = chopString(seq.name, "|", words, ArraySize(words));
    if (wordCount != 3)
        errAbort("Unknown format for NT_ names line %d of %s", lf->lineIx, lf->fileName);
    nt = findNt(ntHash, words[1]);
    if (nt->faSize != 0)
        printf("%s size %d and %d\n", nt->name, nt->faSize, seq.size);
    nt->faSize = seq.size;
    printf(".");
    fflush(stdout);
    }
printf("\n");
lineFileClose(&lf);
}

void addInCoor(struct hash *ntHash, char *fileName)
/* Check if NT's are present in ctg_coords file. */
{
char *row[8];
int wordCount;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct nt *nt;
while ((wordCount = lineFileChop(lf, row)) > 0)
    {
    lineFileExpectWords(lf, ArraySize(row), wordCount);
    nt = findNt(ntHash, row[0]);
    nt->inCoor = TRUE;
    }
lineFileClose(&lf);
}

void checkNt(char *agpFile, char *coordsFile, char *faFile)
/* checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent. */
{
struct hash *ntHash = newHash(0);
struct hashEl *hel;
struct nt *nt;
int badCount = 0;
addAgpSize(ntHash, agpFile);
addInCoor(ntHash, coordsFile);
addNtSize(ntHash, faFile);
for (hel = hashElListHash(ntHash); hel != NULL; hel = hel->next)
    {
    nt = hel->val;
    if (nt->agpSize != nt->faSize)
	{
        printf("%s is %d in %s and %d in %s\n", nt->name, nt->agpSize, agpFile, nt->faSize, faFile);
	badCount += 1;
	}
    else if (nt->inCoor == 0)
	{
        printf("%s is in %s but not %s\n", nt->name, agpFile, coordsFile);
	badCount += 1;
	}
    }
if (badCount)
    errAbort("%d errors total", badCount);
else
    printf("No problems detected\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
checkNt(argv[1], argv[2], argv[3]);
return 0;
}
