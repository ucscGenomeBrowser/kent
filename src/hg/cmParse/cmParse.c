/* cmParse - parse clone map and create directory scaffold based on it, one
 * directory for each chromosome and one subdirectory for each contig.  Fill
 * in geno.lst, info and map files for each contig. */
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "obscure.h"
#include "linefile.h"
#include "portable.h"
#include "errabort.h"
#include "hash.h"
#include "cheapcgi.h"
#include "imreClone.h"

/* Command line options. */
boolean imreFormat = FALSE;
char *checkImre = NULL;

void usage()
/* Describe usage and exit. */
{
errAbort(
   "cmParse - turn clone map into more easily processed\n"
   "form\n"
   "usage:\n"
   "    cmParse input badclone.tab fin.lst gsDir ooDir\n"
   "options:\n"
   "    -imre - input is an Imre Vastrik@ebi format dir rather than WashU\n"
   "    -checkImre=dir - Cross check Imre formatted dir vs WashU\n");
}


char *wellMapped[] = 
/* Chromosomes that are well mapped - where we'll use the
 * map literally rather than redoing it. */
    { "Y", };
    
char *finChroms[] = 
/* Chromosomes that are finished - no need to assemble these. */
    { "20", "21", "22", };

FILE *errLog;

struct hash *badHash;   /* Hash for bad clones. */
struct hash *ntHash;    /* A hash with contigs of finished clones. */


struct cloneInfo
/* Information about one clone. */
    {
    struct cloneInfo *next;
    char *acc;
    char *dir;
    int phase;
    int pos;
    bool inMap;
    bool inFfa;
    struct ntInfo *nt;
    struct cmContig *contig;	/* Back pointer to contig. May be NULL. */
    };


struct cmChrom
/* Clone map info for one chromosome. */
    {
    struct cmChrom *next;
    char *name;
    struct cmContig *orderedList;
    struct cmContig *randomList;
    };

struct cloneLine
/* A line that refers to a clone. */
    {
    struct cloneLine *next;	/* next in list. */
    struct cloneInfo *clone;    /* Clone this refers to. */
    char *line;                 /* Line in file. */
    };

struct cmContig
/* Clone map info for one contig (possibly with gaps). */
    {
    struct cmContig *next;	 /* Next in list. */
    char *name;			 /* Name of contig. (Allocated in hash). */
    struct slName *lineList;     /* Lines that describe it in map file. */
    struct cloneLine *cloneList; /* Clones in map file. */
    struct ntInfo *ntList;       /* List of nt contigs in this fpc contig. */
    struct cmChrom *chrom;	 /* Back-pointer to chromosome this belongs to. */
    boolean isRandom;		 /* True if in 'random' part of map. */
    };

struct ntInfo
/* Info about one NT contig. */
    {
    struct ntInfo *next;  /* Next in list. */
    char *name;           /* Name (allocated in hash) */
    struct ntClonePos *cloneList;  /* List of clones. */
    char *ctg;		  /* Contig this is in. */
    };

struct ntClonePos
/* Position of a clone in NT contig. */
    {
    struct ntClonePos *next;	/* Next in list. */
    struct cloneInfo *clone;    /* Clone reference. */
    int ntPos;                  /* Position in bases. */
    int size;                   /* Size of clone. */
    int orientation;            /* +1 or -1 */
    };

struct cloneInfo *storeCloneInHash(char *acc, struct hash *cloneHash, 
	struct cloneInfo **pCloneList, struct cmContig *contig)
/* Put clone in hash if it isn't already.  Return cloneInfo on clone. */
{
struct hashEl *hel;
struct cloneInfo *ci;

if ((hel = hashLookup(cloneHash, acc)) != NULL)
    return hel->val;
AllocVar(ci);
hel = hashAdd(cloneHash, acc, ci);
ci->acc = hel->name;
slAddHead(pCloneList, ci);
ci->contig = contig;
return ci;
}


boolean inContig(struct cmContig *contig, struct cloneInfo *clone)
/* Return TRUE if bac already in contig. */
{
struct cloneLine *cl;

for (cl = contig->cloneList; cl != NULL; cl = cl->next)
    {
    if (clone == cl->clone)
	{
	fprintf(errLog, "%s duplicate %s\n", contig->name, clone->acc);
	return TRUE;
	}
    }
return FALSE;
}

void parseNtWord(char *ntWord, char ntName[64], int *retStart, struct lineFile *lf)
/* Parse something like NT_000023_500 to NT_000023 and 500 */
{
char *s, *t;
int len;

s = strchr(ntWord, '_');
t = strrchr(ntWord, '_');
if (s == NULL || (s == t))
    errAbort("Badly formatted NT word %s line %d of %s", ntWord, lf->lineIx, lf->fileName);
len = t - ntWord;
if (len >= 64)
    len = 63;
memcpy(ntName, ntWord, len);
ntName[len] = 0;
t += 1;
if (!isdigit(t[0]))
    errAbort("Badly formatted NT word %s line %d of %s", ntWord, lf->lineIx, lf->fileName);
*retStart = atoi(t)-1;
}

boolean checkAccFormat(char *acc)
/* Check accession is in reasonable format. */
{
int accLen = strlen(acc);
if (startsWith("NG_", acc) && accLen == 9 && isdigit(acc[3]) && isdigit(acc[8]))
    return TRUE;
return !(accLen < 6 || accLen > 8 || !isupper(acc[0]) || !isdigit(acc[accLen-1]));
}

void readContigList(struct lineFile *in, struct cmChrom *chrom,
	struct cmContig **pContigList, bool isCommitChrom, bool isRandom,
	struct hash *cloneHash, struct cloneInfo **pCloneList, struct hash *ctgHash)
/* Read in contigs. */
{
boolean gotEnd = FALSE;
char *lastCtgName = "";
char *ctgName;
char *acc;
int lineSize;
char *line;
char *words[16];
int wordCount;
struct cmContig *contig = NULL;
struct cloneLine *cl;
struct cloneInfo *ci;
struct slName *cName;
char oline[512];
bool isCommitted;

while (lineFileNext(in, &line, &lineSize))
    {
    if (line[0] == '#') continue;
    if (lineSize >= sizeof(oline))
	errAbort("Line %d of %s too long (%d bytes)", in->lineIx, in->fileName, lineSize);
    memcpy(oline, line, lineSize+1);
    if (startsWith("end human", line))
	{
	gotEnd = TRUE;
	break;
	}
    else if (startsWith("start human", line))
	{
	gotEnd = TRUE;
	lineFileReuse(in);
	break;
	}
    if (isCommitChrom)
	continue;
    wordCount = chopLine(oline, words);
    if (words[0][0] == '@' || words[0][0] == '*')
	continue;	
    acc = words[0];
    if (hashLookup(badHash, acc))
        continue;
    isCommitted = sameWord(acc, "COMMITTED");
    if (!isCommitted && !(wordCount <= 11 && wordCount >= 9))
	{
        if ((wordCount == 8) && startsWith("ctg", words[3]) && 
            isdigit(words[4][0]) && isdigit(words[5][0]) )
            {
            int i;
            warn("Filling in missing field 6 line %d of %s", in->lineIx, in->fileName);
            for (i=10; i>=6; --i)
                words[i] = words[i-1];
            words[5] = "???";
            wordCount = 11;
            }
        else
		errAbort("%s\nExpecting 11 words line %d of %s", line, in->lineIx, in->fileName);
	}
    ctgName = words[3];
    if (!sameString(ctgName, lastCtgName))
	{
	AllocVar(contig);
	slAddTail(pContigList, contig);
	hashAddSaveName(ctgHash, ctgName, contig, &contig->name);
	lastCtgName = contig->name;
	contig->chrom = chrom;
	contig->isRandom = isRandom;
	}
    cName = newSlName(line);
    slAddTail(&contig->lineList, cName);
    if (!isCommitted )
	{
	int phase;
	int accLen;
	if (wordCount > 11 || wordCount < 9)
	    errAbort("Need 11 words line %d of %s\n", in->lineIx, in->fileName);
	if (!isdigit(words[4][0]) 
		&& (words[4][0] != '-' || !isdigit(words[4][1])))
	    errAbort("Expecting position in field 5 line %d of %s\n", in->lineIx, in->fileName);
	phase = atoi(words[7]);
	if (!isdigit(words[7][0]) || phase < 0 || phase > 3)
	    errAbort("Expecting phase in field 8 line %d of %s\n", in->lineIx, in->fileName);
	if (!startsWith("ctg", words[3]))
	    errAbort("Expecting contig field 4 line %d of %s\n", in->lineIx, in->fileName);
	if (!checkAccFormat(acc))
	    errAbort("Badly formed accession field 1 line %d of %s\n", in->lineIx, in->fileName);
	ci = storeCloneInHash(acc, cloneHash, pCloneList, contig);
	ci->inMap = TRUE;
	if (!inContig(contig, ci))
	    {
	    char *ntWord = words[8];
	    ci->pos = atoi(words[4]);
	    ci->phase = phase;
	    AllocVar(cl);
	    cl->clone = ci;
	    cl->line = cName->name;
	    slAddTail(&contig->cloneList, cl);
	    if (startsWith("NT_", ntWord))
	        {
		struct ntInfo *nt = NULL;
		struct ntClonePos *ntPos;
		char ntName[64];
		int ntStart;
		parseNtWord(ntWord, ntName, &ntStart, in);
                /* Here we want to warn the user if the NT contig occurs in
		 * more than one fingerprint contig.  Past the warning we want
		 * processing to continue normally. */
		nt = hashFindVal(ntHash, ntName);
                if (nt != NULL)
		    {
		    if (nt->ctg != contig->name)
			{
			warn("%s crosses between contigs %s and %s", nt->name, nt->ctg, contig->name);
                        nt = NULL;
			}
		    }
		if (nt == NULL)
		    {
		    struct hashEl *hel;
		    AllocVar(nt);
		    hel = hashAdd(ntHash, ntName, nt);
		    nt->name = hel->name;
		    nt->ctg = contig->name;
		    slAddTail(&contig->ntList, nt);
		    }
		AllocVar(ntPos);
		ntPos->clone = ci;
		ntPos->ntPos = ntStart;
		ntPos->size = atoi(words[6]);
		ntPos->orientation = 1;
		slAddTail(&nt->cloneList, ntPos);
		}
	    }
	}
    }
if (!gotEnd)
    warn("%s seems to be truncated\n", in->fileName);
}

void readNa(struct lineFile *in, struct cmChrom *chrom,
	struct cmContig **pContigList, struct hash *cloneHash, 
	struct cloneInfo **pCloneList, struct hash *ctgHash)
/* Read "NA" contigs. */
{
boolean gotEnd = FALSE;
char *acc;
int lineSize;
char *line;
char *words[16];
int wordCount;
struct cmContig *contig;
struct cloneLine *cl;
struct cloneInfo *ci;
struct slName *cName;
char oline[512];
int pos = 0;

AllocVar(contig);
contig->chrom = chrom;
slAddTail(pContigList, contig);
contig->name = cloneString("ctg_na");
hashAdd(ctgHash, contig->name, contig);
while (lineFileNext(in, &line, &lineSize))
    {
    if (line[0] == '#') continue;
    if (lineSize >= sizeof(oline))
	errAbort("Line %d of %s too long (%d bytes)", in->lineIx, in->fileName, lineSize);
    memcpy(oline, line, lineSize+1);
    if (startsWith("end human", line))
	{
	gotEnd = TRUE;
	break;
	}
    wordCount = chopLine(oline, words);
    if (words[0][0] == '@' || words[0][0] == '*')
	continue;	
    if (wordCount != 5  && wordCount != 4)
	errAbort("Expecting 5 word line %d of %s", in->lineIx, in->fileName);
    acc = words[0];
    if (!hashLookup(badHash, acc))
	{
	cName = newSlName(line);
	slAddTail(&contig->lineList, cName);
	ci = storeCloneInHash(acc, cloneHash, pCloneList, contig);
	ci->inMap = TRUE;
	if (!inContig(contig, ci))
	    {
	    ci->pos = pos;
	    pos += 100;
	    ci->phase = atoi(words[wordCount-2]);
	    AllocVar(cl);
	    cl->clone = ci;
	    cl->line = cName->name;
	    slAddTail(&contig->cloneList, cl);
	    }
	}
    }
if (!gotEnd)
    warn("%s seems to be truncated\n", in->fileName);
}


struct cmChrom *wuParse(char *inName, struct hash *cloneHash, 
	struct cloneInfo **pCloneList, struct hash *ctgHash)
/* Parse wash U style clone map into common
 * intermediate format. */
{
struct lineFile *in = lineFileOpen(inName, TRUE);
int lineSize;
char *line;
char *words[16];
int wordCount;
char chromName[32];
char lastChromName[32];
boolean isOrdered;
char *s;
struct cmChrom *chromList = NULL, *chrom = NULL;

strcpy(lastChromName, "");
while (lineFileNext(in, &line, &lineSize))
    {
    struct cmContig **pContigList;

    if (line[0] == '#') continue;
    if (!startsWith("start human SUPERLINK", line))
	continue;
    wordCount = chopLine(line, words);
    if (wordCount != 5 && wordCount != 4)
	errAbort("Odd start line %d of %s\n", in->lineIx, in->fileName);
    if (words[wordCount-1][0] != '*')
	errAbort("Odd start line %d of %s\n", in->lineIx, in->fileName);
    s = strrchr(words[2], '.');
    if (s == NULL)
	errAbort("Couldn't find chromosome line %d of 5s\n", in->lineIx, in->fileName);
    s += 1;
    strncpy(chromName, s, sizeof(chromName));
    if (!sameString(chromName, lastChromName))
	{
	strcpy(lastChromName, chromName);
	printf("Reading %s\n", chromName);
	AllocVar(chrom);
	chrom->name = cloneString(chromName);
	slAddHead(&chromList, chrom);
	}
    isOrdered = sameWord(words[3], "ORDERED");
    pContigList = (isOrdered ? &chrom->orderedList : &chrom->randomList);
    if (*pContigList != NULL)
	errAbort("Duplicate chromosome %s %s",
		chromName, words[3]);
    if (sameString(chromName, "NA"))
	readNa(in, chrom, pContigList, cloneHash, pCloneList, ctgHash);
    else
	readContigList(in, chrom, pContigList, sameString(chromName, "COMMIT"), 
		!isOrdered, cloneHash, pCloneList, ctgHash);
    }
slReverse(&chromList);
return chromList;
}

void mustMakeDir(char *dir)
/* Make directory or die trying.  Error even if dir exists. */
{
if (!makeDir(dir))
    errAbort("%s already exists", dir);
}

void mustChangeDir(char *dir)
/* Change directory or die trying. */
{
if (chdir(dir) < 0)
    {
    warn("Couldn't change directory to %s", dir);
    perror("");
    noWarnAbort();
    }
}

char *mapType(char *chromName, boolean isOrdered)
/* Return map type for info file. */
{
if (stringArrayIx(chromName, wellMapped, ArraySize(wellMapped)) >= 0)
    return "PLACED";
else if (isOrdered)
    return "ORDERED";
else
    return "RANDOM";
}


void writeNtList(char *fileName, struct cmContig *contig)
/* Write out all NT contigs to file. */
{
FILE *f = mustOpen(fileName, "w");
struct ntInfo *nt;
struct ntClonePos *ntPos;

for (nt = contig->ntList; nt != NULL; nt = nt->next)
    {
    for (ntPos = nt->cloneList; ntPos != NULL; ntPos = ntPos->next)
        {
	fprintf(f, "%s\t%s\t%d\t%c\t%d\n", nt->name, ntPos->clone->acc,
	    ntPos->ntPos, (ntPos->orientation < 0 ? '-' : '+'),
	    ntPos->size);
	}
    fprintf(f, "\n");
    }
fclose(f);
}

void makeCmDir(struct cmChrom *chromList, char *outDir)
/* Make directory full of goodies corresponding to chromList. */
{
struct cmChrom *chrom;
struct cmContig *contig;
struct slName *line;
struct cloneInfo *ci;
struct cloneLine *cl;
int isOrdered;
FILE *info;
FILE *geno;
FILE *map;
char *s;
int i;

mustMakeDir(outDir);
mustChangeDir(outDir);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (sameString(chrom->name, "COMMIT"))
	continue;
    if (stringArrayIx(chrom->name, finChroms, ArraySize(finChroms)) >= 0)
	{
	printf("Skipping finished %s\n", chrom->name);
        continue;
	}
    printf("Writing chromosome %s", chrom->name);
    fflush(stdout);
    mustMakeDir(chrom->name);
    mustChangeDir(chrom->name);
    for (isOrdered = 1; isOrdered >= 0; --isOrdered)
	{
	contig = (isOrdered ? chrom->orderedList : chrom->randomList);
	for ( ; contig != NULL; contig = contig->next)
	    {
	    printf(".");
	    fflush(stdout);
	    if (makeDir(contig->name))
		{
		mustChangeDir(contig->name);
		info = mustOpen("info.noNt", "w");
		geno = mustOpen("geno.lst.noNt", "w");
		map = mustOpen("map", "w");
		if (contig->ntList != NULL)
		    writeNtList("nt.noNt", contig);
		fprintf(info, "%s %s\n", contig->name, 
		    mapType(chrom->name, isOrdered));

		for (line = contig->lineList; line != NULL; line = line->next)
		    fprintf(map, "%s\n", line->name);
		for (cl = contig->cloneList; cl != NULL; cl = cl->next)
		    {
		    ci = cl->clone;
		    fprintf(info, "%s %d %d\n", ci->acc, ci->pos, ci->phase);
		    if (ci->dir != NULL)
			fprintf(geno, "%s/%s.fa\n", ci->dir, ci->acc);
		    }
		fclose(info);
		fclose(geno);
		fclose(map);
		mustChangeDir("..");
		}
	    }
	}
    mustChangeDir("..");
    printf("\n");
    }
}

void faDirToCloneHash(char *dir, struct hash *cloneHash, struct cloneInfo **pCloneList)
/* Read in dir and store file names in hash keyed by
 * accession names. */
{
struct slName *list, *el;
struct cloneInfo *ci;

list = listDir(dir, "*.fa");
for (el = list; el != NULL; el = el->next)
    {
    char rootFile[256];
    char *s;
    strcpy(rootFile, el->name);
    chopSuffix(rootFile);
    if (!hashLookup(badHash, rootFile))
	{
	ci = storeCloneInHash(rootFile, cloneHash, pCloneList, NULL);
	ci->inFfa = TRUE;
	ci->dir = dir;
	}
    }
}

void listMissing(struct cloneInfo *cloneList, struct hash *badHash)
/* List all clones that are in ffa but not map and vice versa. */
{
struct cloneInfo *ci;
int missingFromMapCount = 0;
int missingFromFfaCount = 0;

fprintf(errLog, "Clones missing from map:\n");
for (ci = cloneList; ci != NULL; ci = ci->next)
    {
    if (ci->inFfa && !ci->inMap && !hashLookup(badHash, ci->acc))
	{
	fprintf(errLog, "%s\n", ci->acc);
	++missingFromMapCount;
	}
    }
fprintf(errLog, "Clones missing from ffa's:\n");
for (ci = cloneList; ci != NULL; ci = ci->next)
    {
    if ((!ci->inFfa) && ci->inMap && ci->phase > 0 && 
    	!hashLookup(badHash, ci->acc))
	{
	fprintf(errLog, "%s\n", ci->acc);
	++missingFromFfaCount;
	}
    }
fprintf(errLog, "%d clones missing from map, %d missing from ffas\n",
	missingFromMapCount, missingFromFfaCount);
fprintf(stdout, "%d clones missing from map, %d missing from ffas\n",
	missingFromMapCount, missingFromFfaCount);
}

void readBad(struct hash *badHash, char *fileName, int cloneWord)
/* Read bad clones into hash. */
{
char *words[8];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *acc;
int badCount = 0;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (wordCount < cloneWord+1)
	{
        errAbort("Expecting at least %d words line %d of %s", cloneWord+1,
		lf->lineIx, lf->fileName);
	}
    acc = words[cloneWord];
    chopSuffix(acc);
    if (!checkAccFormat(acc))
        errAbort("Badly formatted accession line %d of %s", lf->lineIx, lf->fileName);
    hashStore(badHash, acc);
    ++badCount;
    }
lineFileClose(&lf);
printf("Got %d clones to avoid from %s\n", badCount, fileName);
}

void readGsClones(char *gsDir, struct hash *cloneHash, struct cloneInfo **pCloneList)
/* Read in clones from freeze directory. */
{
static char *subDirs[3] = {"fin", "draft", "predraft"};
int i;
char *subDir, path[512];	

for (i=0; i<ArraySize(subDirs); ++i)
    {
    subDir = subDirs[i];
    sprintf(path, "%s/%s/fa", gsDir, subDir);
    printf("Reading %s ...\n", path);
    faDirToCloneHash(path, cloneHash, pCloneList);
    }
}

void ooDirsFromWuMap(char *inName, char *gsDir, char *outDir)
/* Read in St. Louis format file and create corresponding dir full of 
 * goodies. */
{
struct hash *cloneHash = newHash(0);
struct hash *ctgHash = newHash(0);
struct cloneInfo *cloneList = NULL;
struct cmChrom *chromList = wuParse(inName, cloneHash, &cloneList, ctgHash);
if (chromList == NULL)
    errAbort("Nothing to do in %s\n", inName);
readGsClones(gsDir, cloneHash, &cloneList);
slReverse(&cloneList);
makeCmDir(chromList, outDir);
listMissing(cloneList, badHash);
}

struct cmChrom *imParseOne(char *imreFile, struct hash *cloneHash, 
	struct cloneInfo **pCloneList, struct hash *ctgHash)
/* Parse a single tpf.txt file (Imre style) */
{
struct cmChrom *chrom = NULL;
struct cmContig *contig = NULL;
struct cloneLine *cl;
struct lineFile *lf = lineFileOpen(imreFile, TRUE);
char *words[16], *line;
char origLine[512];
int wordCount, lineSize;
struct slName *cName;
struct cloneInfo *ci;
static struct imreClone im;
char sFile[128];
char *chromName = NULL;

uglyf("Reading %s\n", imreFile);
splitPath(imreFile, NULL, sFile, NULL);
chromName = sFile;
if (startsWith("Chr", chromName))
    chromName += 3;
AllocVar(chrom);
chrom->name = cloneString(chromName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    if (lineSize >= sizeof(origLine))
        errAbort("Line %d too long in %s", lf->lineIx, lf->fileName);
    strcpy(origLine, line);
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (sameWord(words[0], "gap"))
        continue;
    if (wordCount < 5)
        errAbort("Expecting at least 5 words line %d of %s", 
		lf->lineIx, lf->fileName);
    imreCloneStaticLoad(words, &im);
    if (contig == NULL || !sameString(contig->name, im.imreContig))
        {
	AllocVar(contig);
	slAddHead(&chrom->orderedList, contig);
	hashAddSaveName(ctgHash, im.imreContig, contig, &contig->name);
	contig->chrom = chrom;
	}
    cName = newSlName(line);
    slAddTail(&contig->lineList, cName);
    ci = storeCloneInHash(im.accession, cloneHash, pCloneList, contig);
    ci->inMap = TRUE;
    AllocVar(cl);
    cl->clone = ci;
    cl->line = cName->name;
    slAddTail(&contig->cloneList, cl);
    }
slReverse(&chrom->orderedList);
return chrom;
}

struct cmChrom *imParse(char *imreDir, struct hash *cloneHash, 
	struct cloneInfo **pCloneList, struct hash *ctgHash)
/* Parse tpf.txt files in imreDir. */
{
struct fileInfo *fiList1 = listDirX(imreDir, "*", TRUE), *fi1;
struct fileInfo *fiList2 = NULL, *fi2;
struct cmChrom *chromList = NULL, *chrom;

for (fi1 = fiList1; fi1 != NULL; fi1 = fi1->next)
    {
    if (fi1->isDir)
        {
	struct fileInfo *fiList2 = listDirX(fi1->name, "t*.txt", TRUE);
	for (fi2 = fiList2; fi2 != NULL; fi2 = fi2->next)
	    {
	    chrom = imParseOne(fi2->name, cloneHash, pCloneList, ctgHash);
	    slAddHead(&chromList, chrom);
	    }
	slFreeList(&fiList2);
	}
    }
slFreeList(&fiList1);
slReverse(&chromList);
return chromList;
}

struct chromMiss
/* Keep track of info on clones missing from chromosome. */
    {
    struct chromMiss *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    int count;			/* Count of missing ones. */
    };

void flagMissing(struct hash *hash, struct cloneInfo *list)
/* Report clones that are on list but not in hash */
{
struct cloneInfo *clone;
struct cmContig *contig;
char chromName[64];
struct hash *missHash = newHash(8);
struct chromMiss *missList = NULL, *miss;

int missCount = 0, count = 0;
printf("Missing clones\n");
for (clone = list; clone != NULL; clone = clone->next)
    {
    ++count;
    if (!hashLookup(hash, clone->acc))
        {
	contig = clone->contig;
	sprintf(chromName, "chr%s%s", contig->chrom->name,
		(contig->isRandom ? "_random" : ""));
	printf("%s\t%s\n", clone->acc, chromName);
	if (stringArrayIx(chromName, finChroms, ArraySize(finChroms)) < 0)
	    {
	    ++missCount;
	    miss = hashFindVal(missHash, chromName);
	    if (miss == NULL)
		{
		AllocVar(miss);
		hashAddSaveName(missHash, chromName, miss, &miss->name);
		slAddHead(&missList, miss);
		}
	    ++miss->count;
	    }
	}
    }
slReverse(&missList);
printf("Chromosome by chromosome misses:\n");
for (miss = missList; miss != NULL; miss = miss->next)
    printf("%-12s %3d\n", miss->name, miss->count);
printf("Total: %d of %d missing\n", missCount, count);
}


void checkImreVsWu(char *gsDir, char *imreDir, char *wuFile)
/* Check Imre Map vs. Wash U St. Louis Map. */
{
struct cmChrom *wuChromList = NULL, *imreChromList = NULL;
struct hash *wuCloneHash = newHash(0), *imreCloneHash = newHash(0);
struct cloneInfo *wuCloneList = NULL, *imreCloneList = NULL;
struct hash *wuCtgHash = newHash(0), *imreCtgHash = newHash(0);

wuChromList = wuParse(wuFile, wuCloneHash, &wuCloneList, wuCtgHash);
slReverse(&wuCloneList);
imreChromList = imParse(imreDir, imreCloneHash, &imreCloneList, imreCtgHash);
slReverse(&imreCloneList);
uglyf("Loaded %d imre chromosomes, %d wu chromosomes\n", 
	slCount(imreChromList), slCount(wuChromList));
uglyf("Loaded %d imre clones, %d wu clones\n",
	slCount(imreCloneList), slCount(wuCloneList));
flagMissing(imreCloneHash, wuCloneList);
}


void cmProcess(char *inName, char *badClones, char *finLst, char *gsDir, 
	char *outDir)
/* Read in file and create corresponding dir full of goodies. */
{
/* Load up hashes and lists from input files. */
ntHash = newHash(0);
badHash = newHash(11);
readBad(badHash, badClones, 0);
readBad(badHash, finLst, 0);

/* Pick routine to call based on command line parameters. */
if (checkImre != NULL)
    {
    checkImreVsWu(gsDir, checkImre, inName);
    }
else if (imreFormat)
    {
    uglyAbort("Not implemented");
    }
else
    {
    ooDirsFromWuMap(inName, gsDir, outDir);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct cmChrom *chromList = NULL, *chrom;
cgiSpoof(&argc, argv);
imreFormat = cgiBoolean("imre");
checkImre = cgiOptionalString("checkImre");
if (argc != 6)
    usage();
errLog = mustOpen("err.log", "w");
cmProcess(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}



