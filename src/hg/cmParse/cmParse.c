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

void usage()
/* Describe usage and exit. */
{
errAbort(
   "cmParse - turn clone map into more easily processed\n"
   "form\n"
   "usage:\n"
   "    cmParse input badclone.tab fin.lst gsDir ooDir\n");
}

char finDir[512];	/* Where finished sequence lives. */
char draftDir[512];	/* Where draft sequence lives. */
char predraftDir[512];	/* Where predraft sequence lives. */

char *wellMapped[] = 
/* Chromosomes that are well mapped - where we'll use the
 * map literally rather than redoing it. */
    { "Y", };
    
char *finChroms[] = 
/* Chromosomes that are finished - no need to assemble these. */
    { "20", "21", "22", };

FILE *errLog;


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
    };

struct hash *cloneHash;	/* A hash with clone accessions for key and cloneInfo's for value. */
struct hash *badHash;   /* Hash for bad clones. */
struct cloneInfo *cloneList;  /* List of all clones. */
struct hash *ntHash;    /* A hash with contigs of finished clones. */
struct hash *ctgHash;   /* A hash of fingerprint contigs. */

struct cloneInfo *storeCloneInHash(char *acc)
/* Put clone in hash if it isn't already.  Return cloneInfo on clone. */
{
struct hashEl *hel;
struct cloneInfo *ci;

if ((hel = hashLookup(cloneHash, acc)) != NULL)
    return hel->val;
AllocVar(ci);
hel = hashAdd(cloneHash, acc, ci);
ci->acc = hel->name;
slAddHead(&cloneList, ci);
return ci;
}


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
    struct cmContig *next;	/* Next in list. */
    char *name;			/* Name of contig. (Allocated in hash). */
    struct slName *lineList;    /* Lines that describe it in map file. */
    struct cloneLine *cloneList; /* Clones in map file. */
    struct ntInfo *ntList;       /* List of nt contigs in this fpc contig. */
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
return !(accLen < 6 || accLen > 8 || !isupper(acc[0]) || !isdigit(acc[accLen-1]));
}

void readContigList(struct lineFile *in, struct cmContig **pContigList, bool isCommitChrom)
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
struct cmContig *contig;
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
	struct hashEl *hel;
	AllocVar(contig);
	slAddTail(pContigList, contig);
        hel = hashAdd(ctgHash, ctgName, contig);
	contig->name = hel->name;
	lastCtgName = contig->name;
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
	ci = storeCloneInHash(acc);
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

void readNa(struct lineFile *in, struct cmContig **pContigList)
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
slAddTail(pContigList, contig);
contig->name = cloneString("ctg_na");
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
	ci = storeCloneInHash(acc);
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


struct cmChrom *cmParse(char *inName)
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
struct cmChrom *chromList = NULL, *chrom;

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
	readNa(in, pContigList);
    else
	readContigList(in, pContigList, sameString(chromName, "COMMIT"));
    }
slReverse(&chromList);
return chromList;
}

boolean wannaMakeDir(char *dir)
/* Make dir if possible.  Return FALSE if not. */
{
if (mkdir(dir, 0775) < 0)
    {
    warn("\nCouldn't make directory %s", dir);
    fprintf(errLog, "%s exists\n", dir);
    perror("");
    return FALSE;
    }
return TRUE;
}

void mustMakeDir(char *dir)
/* Make directory or die trying.  Error even if dir exists. */
{
if (!wannaMakeDir(dir))
    {
    noWarnAbort();
    }
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
	    if (wannaMakeDir(contig->name))
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

void faDirToCloneHash(char *dir)
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
	ci = storeCloneInHash(rootFile);
	ci->inFfa = TRUE;
	ci->dir = dir;
	}
    }
}

void listMissing(struct cloneInfo *ciList, struct hash *badHash)
/* List all clones that are in ffa but not map and vice versa. */
{
struct cloneInfo *ci;
int missingFromMapCount = 0;
int missingFromFfaCount = 0;

fprintf(errLog, "Clones missing from map:\n");
for (ci = ciList; ci != NULL; ci = ci->next)
    {
    if (ci->inFfa && !ci->inMap && !hashLookup(badHash, ci->acc))
	{
	fprintf(errLog, "%s\n", ci->acc);
	++missingFromMapCount;
	}
    }
fprintf(errLog, "Clones missing from ffa's:\n");
for (ci = ciList; ci != NULL; ci = ci->next)
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

void cmProcess(char *inName, char *badClones, char *finLst, char *gsDir, 
	char *outDir)
/* Read in file and create corresponding dir full of goodies. */
{
struct cmChrom *chromList;

cloneHash = newHash(0);
ntHash = newHash(0);
ctgHash = newHash(0);
badHash = newHash(11);

sprintf(finDir, "%s/fin/fa", gsDir);
sprintf(draftDir, "%s/draft/fa", gsDir);
sprintf(predraftDir, "%s/predraft/fa", gsDir);

if (chromList == NULL)
    errAbort("Nothing to do in %s\n", inName);
readBad(badHash, badClones, 0);
readBad(badHash, finLst, 0);
chromList = cmParse(inName);
printf("Reading predraft dir...\n");
faDirToCloneHash(predraftDir);
printf("Reading draft dir...\n");
faDirToCloneHash(draftDir);
printf("Reading fin dir...\n");
faDirToCloneHash(finDir);
makeCmDir(chromList, outDir);
slReverse(&cloneList);
listMissing(cloneList, badHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct cmChrom *chromList = NULL, *chrom;
if (argc != 6)
    usage();
errLog = mustOpen("err.log", "w");
cmProcess(argv[1], argv[2], argv[3], argv[4], argv[5]);
}



