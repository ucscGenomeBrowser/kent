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
    char *contig;	/* Contig this is in. */
    int mapPos;         /* Map position of clone. */
    int phase;          /* HTG Phase. */
    int ntPos;		/* Start position in NT contig. */
    };

struct ntContig
/* Info on one nt contig. */
    {
    struct ntContig *next;	/* Next in list. */
    char *name;			/* Name of contig. */
    struct dlList *cloneList;   /* References to clones used. */
    int cloneCount;             /* Number of clones. */
    char *majorityContig;		/* Contig with majority of clones. */
    bool usedSeq;		/* Set to true if saved out seq	already. */
    bool usedPos;               /* Set to true if saved out position already. */
    };

struct ctg
/* Information about a contig. */
    {
    struct ctg *next;	/* Next in list. */
    char *name;		/* Name (allocated in contigHash) */
    char *dir;		/* Directory this is in. */
    char *chrom;	/* Chromosome this is in. */
    struct clone *cloneList;	/* List of clones. */
    char infoHeader[128];	/* First line of info file. */
    };

int cloneCmpNtPos(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct clone *a = *((struct clone **)va);
const struct clone *b = *((struct clone **)vb);
return a->ntPos - b->ntPos;
}

struct clone *readInfo(char *fileName, char *retHeader, struct hash *cloneHash, char *contig)
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
    clone->contig = contig;
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

void readNt(char *fileName, struct hash *cloneHash, struct hash *ntHash)
/* Read NT contig file and fill in appropriate data structures in ntHash. */
{
struct ntContig *nt = NULL;
struct clone  *clone;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[5];
char *ntName;

while (lineFileRow(lf, words))
    {
    ntName = words[0];
    if ((nt = hashFindVal(ntHash, ntName)) == NULL)
        {
	AllocVar(nt);
	hashAddSaveName(ntHash, ntName, nt, &nt->name);
	nt->cloneList = newDlList();
	}
    if ((clone = hashFindVal(cloneHash, words[1])) != NULL)
	{
	clone->nt = nt;
	if (!dlValInList(nt->cloneList, clone))
	    {
	    dlAddValTail(nt->cloneList, clone);
	    nt->cloneCount += 1;
	    clone->ntPos = lineFileNeedNum(lf, words, 2);
	    }
	}
    }
lineFileClose(&lf);
}

char *ntDirName;
struct hash *ntHash;

struct hash *contigHash;	/* Hash of contigs - values are ctgs. */
struct ctg *ctgList = NULL;
struct hash *cloneHash;



void doContig(char *dir, char *chrom, char *contig)
/* Read in pre-NT versions of info, geno.lst, and also nt.noNt
 * and save in contig. */
{
struct clone *cloneList = NULL, *clone;
struct ntContig *nt;
char fileName[512];
struct ctg *ctg;
FILE *f = NULL;

/* Make sure contig is uniq, and save it in list/hash */
if (hashLookup(contigHash, contig))
    errAbort("Contig %s duplicated", contig);
AllocVar(ctg);
slAddHead(&ctgList, ctg);
hashAddSaveName(contigHash, contig, ctg, &ctg->name);
ctg->dir = cloneString(dir);
ctg->chrom = cloneString(chrom);

/* Read in input three files. */
sprintf(fileName, "%s/info.noNt", dir);
ctg->cloneList = cloneList = readInfo(fileName, ctg->infoHeader, cloneHash, ctg->name);
sprintf(fileName, "%s/geno.lst.noNt", dir);
findCloneSeqFiles(fileName, cloneList, cloneHash);
sprintf(fileName, "%s/nt.noNt", dir);
if (fileExists(fileName))
    readNt(fileName, cloneHash, ntHash);
}


void voteOnNt(struct ntContig *nt)
/* Assign nt->majority on one NT. */
{
struct ctgCounter
    {
    struct ctgCounter *next;	/* Next in list. */
    char *name;			/* Name (not allocated here). */
    int count;			/* Usage count. */
    } *ccList = NULL, *cc;
struct hash *ccHash = newHash(8);	
struct dlNode *node;
struct clone *clone;
char *bestContig = NULL;
int bestCount = 0;

/* Count up how many times each contig is used. */
for (node = nt->cloneList->head; !dlEnd(node); node = node->next)
    {
    clone = node->val;
    if ((cc = hashFindVal(ccHash, clone->contig)) == NULL)
        {
	AllocVar(cc);
	cc->name = clone->contig;
	slAddHead(&ccList, cc);
	hashAdd(ccHash, cc->name, cc);
	}
    cc->count += 1;
    }

/* Find most popular contig and assign it to majority. */
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    if (cc->count > bestCount)
        {
	bestCount = cc->count;
	bestContig = cc->name;
	}
    }
nt->majorityContig = bestContig;

if (slCount(ccList) != 1) 
    {
    printf("%d contigs claim part of %s, winner is %s\n", slCount(ccList), nt->name, nt->majorityContig);
    for (cc = ccList; cc != NULL; cc = cc->next)
        printf("  %s %d\n", cc->name, cc->count);
    }
freeHash(&ccHash);
slFreeList(&ccList);
}

void sortAndVote()
/* Sort cloneLists by ntPos and Assign nt->majorityContig to all NTs. */
{
struct hashEl *ntList = hashElListHash(ntHash), *hel;

for (hel = ntList; hel != NULL; hel = hel->next)
    {
    struct ntContig *nt = hel->val;
    voteOnNt(nt);
    dlSort(nt->cloneList, cloneCmpNtPos);
    }
}

int ntFlipTendency(struct ntContig *nt)
/* Return tendency of NT to flip.   This is made from map position
 * differences of clones. */
{
struct dlNode *node;
struct clone *clone, *lastClone = NULL;
int acc = 0;

for (node = nt->cloneList->head; !dlEnd(node); node = node->next)
    {
    clone = node->val;
    if (sameString(clone->contig, nt->majorityContig))
        {
	if (lastClone != NULL)
	    {
	    acc += clone->mapPos - lastClone->mapPos;
	    }
	lastClone = clone;
	}
    }
return acc;
}

void updateContig(struct ctg *ctg)
/* Update files in contig. */
{
char fileName[512];
FILE *f;
struct ntContig *nt;
struct clone *clone;

/* Write out info file. */
sprintf(fileName, "%s/info", ctg->dir);
f = mustOpen(fileName, "w");
fprintf(f, "%s\n", ctg->infoHeader);
for (clone = ctg->cloneList; clone != NULL; clone = clone->next)
    {
    if ((nt = clone->nt) != NULL && nt->cloneCount > 1)
        {
	if (sameString(clone->contig, nt->majorityContig) && !nt->usedPos)
	    {
	    nt->usedPos = TRUE;
	    fprintf(f, "%s %d 3 %d\n", nt->name, clone->mapPos, ntFlipTendency(nt));
	    }
	}
    else
        {
	fprintf(f, "%s %d %d 0\n", clone->name, clone->mapPos, clone->phase);
	}
    }
carefulClose(&f);

/* Write out geno.lst file. */
sprintf(fileName, "%s/geno.lst", ctg->dir);
f = mustOpen(fileName, "w");
for (clone = ctg->cloneList; clone != NULL; clone = clone->next)
    {
    if ((nt = clone->nt) != NULL && nt->cloneCount > 1)
        {
	if (sameString(clone->contig, nt->majorityContig) && !nt->usedSeq)
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
}

void ooNtInput(char *ooDir, char *ntDir)
/* ooNtInput - Arrange source of finished sequence in FPC contig dirs to come from NT 
 * contigs where possible. */
{
struct ctg *ctg;
ntDirName = ntDir;
ntHash = newHash(0);
contigHash = newHash(9);
cloneHash = newHash(15);
printf("Reading contigs from %s\n", ooDir);
ooToAllContigs(ooDir, doContig);
sortAndVote();
uglyf("Writing NT modified files\n");
slReverse(ctgList);
for (ctg = ctgList; ctg != NULL; ctg = ctg->next)
    updateContig(ctg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooNtInput(argv[1], argv[2]);
return 0;
}
