/* ooLiftSpec -
 * This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on cloneMap. */
#include "common.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "portable.h"
#include "hash.h"
#include "chromInserts.h"


/* Variables that can be overridden from command line. */
char *goldName = NULL;

char *finChroms[] = 
/* Chromosomes that are finished - no need to assemble these. */
    { "20", "21", "22", "Y"};

void usage()
/* Explain usage and exit. */
{
errAbort(
 "ooLiftSpec - make a specification for lifting coordinates from\n"
 "contig to chromosome\n"
 "usage:\n"
 "   ooLiftSpec cloneMap inserts ooDir\n"
 "This will make lift directory containing ordered.lft and random.lft\n"
 "each chromosome subdir of ooDir based on contigs in cloneMap and\n"
 "large inserts in inserts file.\n"
 "options:\n"
 "   -goldName=gold.NN - use gold.NN instead of contig.agp\n"
 "   -imre  - cloneMap is Imre TPF/FPC directory format rather than Wash U\n"
 "   -imreMerge=dir - Merge WashU and TPF maps.  You need to also specify fpcChrom\n"
 "   -fpcChrom=M,N - List of chromosomes where the WashU map should predominate.\n"
 "   -chrom=N - List of chromosomes to output (default all)\n"
 );
}

struct contigInfo 
/* Info on one contig. */
    {
    struct contigInfo *next;	/* Next in list */
    char *name;			/* Name of contig. */
    int size;			/* Size of contig. */
    };

void writeListType(char *fileName, struct contigInfo *ciList, char *suffix)
/* Write file list... */
{
FILE *f = mustOpen(fileName, "w");
struct contigInfo *ci;

for (ci = ciList; ci != NULL; ci = ci->next)
    fprintf(f, "%s/%s%s\n", ci->name, ci->name, suffix);
fclose(f);
}

void writeList(char *fileName, struct contigInfo *ciList)
/* Write list file that contains all contigs. */
{
FILE *f = mustOpen(fileName, "w");
struct contigInfo *ci;

for (ci = ciList; ci != NULL; ci = ci->next)
    fprintf(f, "%s\n", ci->name);
fclose(f);
}

boolean filterChromOutput(char *name)
/* Return TRUE if we want to output this chromosome. */
{
struct slName *list;
boolean ok;

if ((list = cgiStringList("chrom")) == NULL)
    return TRUE;
ok = slNameInList(list, name);
slFreeList(&list);
return ok;
}

void writeLift(char *fileName, struct contigInfo *ciList, struct chromInserts *chromInserts,
	char *chromName, char *shortName)
/* Write list file that contains all contigs. */
{
if (filterChromOutput(shortName))
    {
    FILE *f = mustOpen(fileName, "w");
    struct contigInfo *ci, *prev = NULL;
    int offset = 0;
    int size = 0;
    struct bigInsert *bi;

    boolean slNameInList(struct slName *list, char *string);

    for (ci = ciList; ci != NULL; ci = ci->next)
	{
	size += chromInsertsGapSize(chromInserts, ci->name, ci == ciList);
	size += ci->size;
	}
    if (chromInserts != NULL && chromInserts->terminal != NULL)
	size += chromInserts->terminal->size;
    for (ci = ciList; ci != NULL; ci = ci->next)
	{
	offset += chromInsertsGapSize(chromInserts, ci->name, ci == ciList);
	fprintf(f, "%d\t%s/%s\t%d\t%s\t%d\n", offset, shortName, ci->name, ci->size, chromName, size);
	offset += ci->size;
	}
    fclose(f);
    }
}


int sizeFromAgp(char *fileName)
/* Return length of sequence from .agp file. */
{
char prevLine[512];
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[16];
boolean gotAny = FALSE;
int size;

if (!fileExists(fileName))
    return 0;
lf = lineFileOpen(fileName, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    gotAny = TRUE;
    if (lineSize >= sizeof(prevLine))
	errAbort("Line too long line %d of %s\n", lf->lineIx, lf->fileName);
    memcpy(prevLine, line, lineSize+1);
    }
if (!gotAny)
    return 0;
wordCount = chopLine(line, words);
if (wordCount < 8)
    errAbort("Expecting at least 8 words on last line of %s\n", lf->fileName);
size = atoi(words[2]);
lineFileClose(&lf);
return size;
}

boolean fillInSizes(struct contigInfo *ciList, char *ooDir, char *chrom)
/* Fill in size of contigs from .agp files. */
{
char fileName[512];
struct contigInfo *ci;
boolean gotSize = FALSE;

for (ci = ciList; ci != NULL; ci = ci->next)
    {
    if (goldName != NULL)
	sprintf(fileName, "%s/%s/%s/%s", ooDir, chrom, ci->name, goldName);
    else
	sprintf(fileName, "%s/%s/%s/%s.agp", ooDir, chrom, ci->name, ci->name);
    ci->size = sizeFromAgp(fileName);
    if (ci->size)
        gotSize = TRUE;
    }
return gotSize;
}

struct contigInfo *readContigs(struct lineFile *lf)
/* Read contigs up intil next "start human" or "end human" */
{
int lineSize, wordCount;
char *line, *words[16];
struct contigInfo *ciList = NULL, *ci;
struct hash *hash = newHash(0);
char *contig;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith("end human", line))
	break;
    if (startsWith("start human", line))
	{
	lineFileReuse(lf);
	break;
	}
    if (line[0] == '@' || line[0] == '*')
	continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (sameString("COMMITTED", words[0]))
        continue;
    if (wordCount < 6)
	errAbort("short line %d of %s\n", lf->lineIx, lf->fileName);
    contig = words[3];
    if (!hashLookup(hash, contig))
	{
	hashAdd(hash, contig, NULL);
	AllocVar(ci);
	ci->name = cloneString(contig);
	slAddHead(&ciList, ci);
	}
    }
freeHash(&hash);
slReverse(&ciList);
return ciList;
}

void warnUnmapped(char *ctg, char *chrom, struct hash *contigHash)
/* Warn if ctg is not in hash. */
{
char fin[256];
int i;

if (ctg == NULL)
    return;
for (i=0; i<ArraySize(finChroms); ++i)
    {
    sprintf(fin, "chr%s", finChroms[i]);
    if (sameString(fin, chrom))
        return;
    }
if (!hashLookup(contigHash, ctg))
    warn("Contig %s (chromosome %s) in inserts file but not map",
	ctg, chrom);
}

void outputLifts(char *ooDir, struct contigInfo *ciList,
    char *chromName, char *shortName, char *type, char *shortType,
    struct chromInserts *chromInserts)
/* Write out ciList in various forms to output files. */
{
char liftDir[512];
char fileName[512];

sprintf(liftDir, "%s/%s/lift", ooDir, shortName);
mkdir(liftDir, 0777);
sprintf(fileName, "%s/%s.lst", liftDir, type);
writeList(fileName, ciList);
sprintf(fileName, "%s/%s.lft", liftDir, type);
writeLift(fileName, ciList, chromInserts, chromName, shortName);
sprintf(fileName, "%s/%sOut.lst", liftDir, shortType);
writeListType(fileName, ciList, ".fa.out");
sprintf(fileName, "%s/%sAgp.lst", liftDir, shortType);
writeListType(fileName, ciList, ".agp");
}


void wuLiftSpec(char *mapName, struct hash *insertsHash, 
	char *ooDir, struct hash *contigHash, struct hash *useHash)
/* This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on WashU format clonemap. */
{
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[16];
char chromName[32];
char shortName[32];
boolean isOrdered;
char *chrom;
struct contigInfo *ciList, *ci;
char *shortType, *type, *suffix;
boolean gotSizes = FALSE;
struct chromInserts *chromInserts;

lf = lineFileOpen(mapName, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (!startsWith("start human SUPERLINK", line))
	continue;
    wordCount = chopLine(line, words);
    if (wordCount != 5 && wordCount != 4)
	errAbort("Odd start line %d of %s\n", lf->lineIx, lf->fileName);
    if (words[wordCount-1][0] != '*')
	errAbort("Odd start line %d of %s\n", lf->lineIx, lf->fileName);
    chrom = strrchr(words[2], '.');
    if (chrom == NULL)
	errAbort("Couldn't find chromosome line %d of 5s\n", lf->lineIx, lf->fileName);
    chrom += 1;
    if (stringArrayIx(chrom, finChroms, ArraySize(finChroms)) >= 0)
        continue;
    if (sameString(chrom, "COMMIT"))
	continue;
    isOrdered = sameWord(words[3], "ORDERED");
    if (isOrdered)
	{
	type = "ordered";
	shortType = "o";
	suffix = "";
	}
    else
	{
	type = "random";
	shortType = "r";
	suffix = "_random";
	}
    sprintf(chromName, "chr%s%s", chrom, suffix);
    printf("%s %s\n", chrom, chromName);
    strcpy(shortName, chrom);
    if (sameString(chrom, "NA"))
	{
	AllocVar(ciList);
	ciList->name = "ctg_na";
	chromInserts = NULL;
	}
    else
	{
	chromInserts = hashFindVal(insertsHash, chromName);
	if (chromInserts == NULL && !stringIn("_random", chromName))
	    warn("No inserts found for %s", chromName);
	ciList = readContigs(lf);
	}
    for (ci = ciList; ci != NULL; ci = ci->next)
	hashStore(contigHash, ci->name);
    gotSizes |= fillInSizes(ciList, ooDir, shortName);

    if (sameString(type, "random") || 
	    useHash == NULL || hashLookup(useHash, shortName))
	outputLifts(ooDir, ciList, chromName, shortName, 
	    type, shortType, chromInserts);
    }
if (!gotSizes)
    warn("All contigs size 0??? Maybe you need to run goldToAgp.");
lineFileClose(&lf);
}


boolean imreLiftOne(char *imreFile, char *chromName, char *shortName,
    struct hash *insertsHash, char *ooDir,
    struct hash *contigHash)
/* Handle a single imre-format chromosome file. Returns TRUE if
 * got any non-empty contigs. */
{
struct lineFile *lf = lineFileOpen(imreFile, TRUE);
char *line, *words[16];
int wordCount;
char *imreContig;
char contigName[256];
char lastContig[256];
struct contigInfo *ciList = NULL, *ci = NULL;
boolean gotSizes;

strcpy(lastContig, "");
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#' || line[0] == '?')
        continue;
    wordCount = chopTabs(line, words);
    if (wordCount == 0)
	continue;
    if (sameWord(words[0], "gap"))
        continue;
    if (wordCount < 5)
	errAbort("Expecting at least 5 words line %d of %s",
	    lf->lineIx, lf->fileName);
    imreContig = words[2];
    eraseWhiteSpace(imreContig);
    if (startsWith("ctg", imreContig))
        strcpy(contigName, imreContig);
    else
        sprintf(contigName, "ctg%s", imreContig);
    if (ci == NULL || !sameString(contigName, ci->name))
        {
	AllocVar(ci);
	ci->name = cloneString(contigName);
	slAddHead(&ciList, ci);
	hashAdd(contigHash, ci->name, NULL);
	}
    }
slReverse(&ciList);
lineFileClose(&lf);
gotSizes = fillInSizes(ciList, ooDir, shortName);
outputLifts(ooDir, ciList, chromName, shortName, 
    "ordered", "o", hashFindVal(insertsHash, chromName));
return gotSizes;
}

void imreLiftSpec(char *mapDir, struct hash *insertsHash, char *ooDir,
	struct hash *contigHash, struct hash *avoidHash)
/* This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on Imre format clonemap. */
{
struct fileInfo *fiList1 = listDirX(mapDir, "*", TRUE), *fi1;
struct fileInfo *fiList2 = NULL, *fi2;
char sDir[256], sFile[128];
char *shortName = NULL;
char chromName[128];
int len;
boolean gotSizes = FALSE;

for (fi1 = fiList1; fi1 != NULL; fi1 = fi1->next)
    {
    if (fi1->isDir)
        {
        struct fileInfo *fiList2 = listDirX(fi1->name, "t*.txt", TRUE);
        for (fi2 = fiList2; fi2 != NULL; fi2 = fi2->next)
	    {
	    /* Figure out chromosome long and short names from input file name. */
	    splitPath(fi2->name, sDir, NULL, NULL);
	    len = strlen(sDir);
	    if (sDir[len-1] == '/') sDir[len-1] = 0;
	    splitPath(sDir, NULL, sFile, NULL);
	    shortName = sFile;
	    if (startsWith("Chr", shortName))
		shortName += 3;
	    sprintf(chromName, "chr%s", shortName);

	    /* Call routine to parse file and produce lift. */
	    if (stringArrayIx(shortName, finChroms, ArraySize(finChroms)) < 0
	     && (avoidHash == NULL || hashLookup(avoidHash, shortName) == NULL))
		{
		gotSizes |= imreLiftOne(fi2->name, chromName, shortName, insertsHash, 
		    ooDir, contigHash);
		}
	    }
        slFreeList(&fiList2);
        }
    }
slFreeList(&fiList1);
if (!gotSizes)
    warn("All contigs size 0??? Maybe you need to run goldToAgp.");
}

struct hash *commaListToHash(char *commaList)
/* Convert a comma separated list to hash. */
{
char *dup = cloneString(commaList), *s;
char *words[128];
int wordCount, i;
struct hash *hash = newHash(5);

wordCount = chopCommas(dup, words);
for (i=0; i<wordCount; ++i)
    {
    s = words[i];
    if (s[0] != 0)
	hashAdd(hash, s, NULL);
    }
freeMem(dup);
return hash;
}

void mergeLiftSpec(char *imreDir, char *wuFile, char *fpcChrom, 
	struct hash *insertsHash, char *ooDir, struct hash *contigHash)
/* This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir map in imreDir for most chromosomes,
 * but in wuFile map for the fpcChrom chromosomes. */
{
struct hash *fpcHash = commaListToHash(fpcChrom);
imreLiftSpec(imreDir, insertsHash, ooDir, contigHash, fpcHash);
wuLiftSpec(wuFile, insertsHash, ooDir, contigHash, fpcHash);
}

void ooLiftSpec(char *mapName, char *inserts, char *ooDir)
/* This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on cloneMap. */
{
struct chromInserts *chromInsertList = NULL, *chromInserts;
struct hash *insertsHash = newHash(8);
struct hash *contigHash = newHash(0);

chromInsertList = chromInsertsRead(inserts, insertsHash);
if (cgiBoolean("imre"))
    imreLiftSpec(mapName, insertsHash, ooDir, contigHash, NULL);
else if (cgiVarExists("imreMerge"))
    {
    char *imreDir = cgiString("imreMerge");
    char *wuFile = mapName;
    char *fpcChrom = cgiOptionalString("fpcChrom");
    if (fpcChrom == NULL)
        errAbort("You need to specify fpcChrom with imreDir");
    mergeLiftSpec(imreDir, wuFile, fpcChrom, insertsHash, ooDir, contigHash);
    }
else
    wuLiftSpec(mapName, insertsHash, ooDir, contigHash, NULL);

/* Check that all contigs in insert file are actually used. */
for (chromInserts = chromInsertList; chromInserts != NULL; chromInserts = chromInserts->next)
    {
    struct bigInsert *bi;
    for (bi = chromInserts->insertList; bi != NULL; bi = bi->next)
        {
	warnUnmapped(bi->ctgBefore, chromInserts->chrom, contigHash);
	warnUnmapped(bi->ctgAfter, chromInserts->chrom, contigHash);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
goldName = cgiOptionalString("goldName");
ooLiftSpec(argv[1], argv[2], argv[3]);
return 0;
}
