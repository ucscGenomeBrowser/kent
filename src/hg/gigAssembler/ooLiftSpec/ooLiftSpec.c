/* ooLiftSpec -
 * This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on cloneMap. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "chromInserts.h"

char *finChroms[] = 
/* Chromosomes that are finished - no need to assemble these. */
    { "20", "21", "22", };

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
 "large inserts in inserts file.\n");
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

void writeLift(char *fileName, struct contigInfo *ciList, struct chromInserts *chromInserts,
	char *chromName, char *shortName)
/* Write list file that contains all contigs. */
{
FILE *f = mustOpen(fileName, "w");
struct contigInfo *ci, *prev = NULL;
int offset = 0;
int size = 0;
struct bigInsert *bi;

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

void ooLiftSpec(char *mapName, char *inserts, char *ooDir)
/* This will make ordered.lft and random.lft 'lift specifications' in
 * each chromosome subdir of ooDir based on cloneMap. */
{
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[16];
char chromName[32];
char shortName[32];
boolean isOrdered;
char *chrom;
struct contigInfo *ciList, *ci;
char fileName[512];
char *shortType, *type, *suffix;
char liftDir[512];
boolean gotSizes = FALSE;
struct hash *insertsHash = newHash(8);
struct hash *contigHash = newHash(0);
struct chromInserts *chromInsertList = NULL, *chromInserts;

chromInsertList = chromInsertsRead(inserts, insertsHash);

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
	if (chromInserts == NULL)
	    warn("No inserts found for %s", chromName);
	ciList = readContigs(lf);
	}
    for (ci = ciList; ci != NULL; ci = ci->next)
	hashStore(contigHash, ci->name);
    gotSizes |= fillInSizes(ciList, ooDir, shortName);

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
if (!gotSizes)
    warn("All contigs size 0??? Maybe you need to run goldToAgp.", chrom);
lineFileClose(&lf);

/* Check that all contigs in insert file are actually used. */
for (chromInserts = chromInsertList; chromInserts != NULL; chromInserts = chromInserts->next)
    {
    struct bigInsert *bi;
    for (bi = chromInserts->insertList; bi != NULL; bi = bi->next)
        {
	if (bi->ctgBefore && !hashLookup(contigHash, bi->ctgBefore))
	    warn("Contig %s (chromosome %s) in inserts file but not map",
	    	bi->ctgBefore, chromInserts->chrom);
	if (bi->ctgAfter && !hashLookup(contigHash, bi->ctgAfter))
	    warn("Contig %s (chromosome %s) in inserts file but not map",
	    	bi->ctgAfter, chromInserts->chrom);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
ooLiftSpec(argv[1], argv[2], argv[3]);
}
