/* ooNtInput - Arrange source of finished sequence in FPC contig dirs to come 
 * from NT contigs where possible. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dlist.h"
#include "portable.h"
#include "ooUtils.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooNtInput - Arrange source of finished sequence in FPC contig dirs to come from NT contigs where possible\n"
  "usage:\n"
  "   ooNtInput ooDir ntFaDir\n");
}

struct clone
/* Info on one clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Allocated in hash. */
    char *faFile;	/* File with sequence in it. */
    struct ntContig *nt;  /* Associated NT contig if any. */
    int mapPos;         /* Map position of clone. */
    int phase;          /* HTG Phase. */
    };

struct ntContig
/* Info on one nt contig. */
    {
    struct ntContig *next;	/* Next in list. */
    char *name;			/* Name of contig. */
    struct dlList *cloneList;   /* References to clones used. */
    int cloneCount;             /* Number of clones. */
    bool usedSeq;		/* Set to true if saved out seq	already. */
    bool usedPos;               /* Set to true if saved out position already. */
    };

struct clone *readInfo(char *fileName, char *retHeader, struct hash *cloneHash)
/* Read in info file and put in hash/list/header. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[3];
int lineSize;
struct clone *cloneList = NULL, *clone;

lineFileNeedNext(lf, &line, &lineSize);
strcpy(retHeader, line);
while (lineFileRow(lf, words))
    {
    AllocVar(clone);
    slAddHead(&cloneList, clone);
    hashAddSaveName(cloneHash, words[0], clone, &clone->name);
    clone->mapPos = lineFileNeedNum(lf, words, 1);
    clone->phase = lineFileNeedNum(lf, words, 2);
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

void findCloneSeqFiles(char *fileName, struct clone *cloneList, struct hash *cloneHash)
/* Read in file with list of sequences and place it on clones.
 * and set faFile field in clone. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char dir[256], base[128], ext[64];
char *words[1];
struct clone *clone;

/* Load in data from file and put on clone. */
while (lineFileRow(lf, words))
    {
    splitPath(words[0], dir, base, ext);
    clone = hashFindVal(cloneHash, base);
    if (clone == NULL)
        errAbort("%s is in %s but not hash\n", base, fileName);
    clone->faFile = cloneString(words[0]);
    }
lineFileClose(&lf);

/* Check all clones except for phase 0 do have sequence. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->phase != 0)
        {
	if (clone->faFile == NULL)
	    errAbort("%s is in info.noNt but not genoList.noNt", clone->name);
	}
    }
}

struct ntContig *readNt(char *fileName, struct hash *cloneHash, struct hash *ntHash)
/* Read NT contig file and fill in appropriate data structures. */
{
struct ntContig *ntList = NULL, *nt = NULL;
struct clone  *clone;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[5];
char lastNtName[128], *ntName;

strcpy(lastNtName, "");
while (lineFileRow(lf, words))
    {
    ntName = words[0];
    if (!sameString(lastNtName, ntName))
        {
	strcpy(lastNtName, ntName);
	AllocVar(nt);
	slAddHead(&ntList, nt);
	hashAddSaveName(ntHash, ntName, nt, &nt->name);
	nt->cloneList = newDlList();
	}
    if ((clone = hashFindVal(cloneHash, words[1])) != NULL)
	{
	clone->nt = nt;
	dlAddValTail(nt->cloneList, clone);
	nt->cloneCount += 1;
	}
    }
lineFileClose(&lf);
slReverse(&ntList);
return ntList;
}

char *ntDirName;

void doContig(char *dir, char *chrom, char *contig)
/* Make nt, info, and geno.lst files from their predecessors. */
{
struct hash *cloneHash = newHash(10);
struct clone *cloneList = NULL, *clone;
struct hash *ntHash = newHash(8);
struct ntContig *ntList = NULL, *nt;
char fileName[512];
char infoHeader[128];
FILE *f = NULL;

uglyf("%s %s %s\n", dir, chrom, contig);

/* Read in input three files. */
sprintf(fileName, "%s/info.noNt", dir);
cloneList = readInfo(fileName, infoHeader, cloneHash);
sprintf(fileName, "%s/geno.lst.noNt", dir);
findCloneSeqFiles(fileName, cloneList, cloneHash);
sprintf(fileName, "%s/nt.noNt", dir);
if (fileExists(fileName))
    ntList = readNt(fileName, cloneHash, ntHash);

/* Write out info file. */
sprintf(fileName, "%s/info", dir);
f = mustOpen(fileName, "w");
fprintf(f, "%s\n", infoHeader);
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if ((nt = clone->nt) != NULL && nt->cloneCount > 1)
        {
	if (!nt->usedPos)
	    {
	    nt->usedPos = TRUE;
	    fprintf(f, "%s %d 3\n", nt->name, clone->mapPos);
	    }
	}
    else
        {
	fprintf(f, "%s %d %d\n", clone->name, clone->mapPos, clone->phase);
	}
    }
carefulClose(&f);

/* Write out geno.lst file. */
sprintf(fileName, "%s/geno.lst", dir);
f = mustOpen(fileName, "w");
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if ((nt = clone->nt) != NULL && nt->cloneCount > 1)
        {
	if (!nt->usedSeq)
	    {
	    nt->usedSeq = TRUE;
	    sprintf(fileName, "%s/%s.fa", ntDirName, nt->name);
	    if (!fileExists(fileName))
	        errAbort("%s doesn't exist!", fileName);
	    fprintf(f, "%s\n", fileName);
	    }
	}
    else
        {
	if (clone->faFile != NULL)
	    fprintf(f, "%s\n", clone->faFile);
	}
    }
carefulClose(&f);
freeHash(&cloneHash);
freeHash(&ntHash);
for (nt = ntList; nt != NULL; nt = nt->next)
    {
    freeDlList(&nt->cloneList);
    }
for (clone = cloneList; clone != NULL; clone = clone->next)
    freez(&clone->faFile);
slFreeList(&ntList);
slFreeList(&cloneList);
}

void ooNtInput(char *ooDir, char *ntDir)
/* ooNtInput - Arrange source of finished sequence in FPC contig dirs to come from NT 
 * contigs where possible. */
{
ntDirName = ntDir;
ooToAllContigs(ooDir, doContig);
// doContig("/projects/hg/gs.5/oo.21/18/ctg25191", "18", "ctg25191");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooNtInput(argv[1], argv[2]);
return 0;
}
