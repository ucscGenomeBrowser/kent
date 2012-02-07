/* checkYbr - Check NCBI assembly (aka Yellow Brick Road). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkYbr - Check NCBI assembly (aka Yellow Brick Road)\n"
  "usage:\n"
  "   checkYbr build.agp contig.fa seq_contig.md\n"
  "options:\n"
  "   -checkUs=ourDir - check that our NT*/NT*.fa under ourDir look right\n"
  );
}

struct contig
/* A contig of the assembly. */
    {
    struct contig *next;
    char *name;	/* Allocated in hash. */
    int size;   /* Size. */
    bool gotFa; /* Set to TRUE if is in FA file. */
    bool gotMd; /* Set to TRUE if is in MD file. */
    bool gotUs;	/* Set to TRUE if have this in our subdir ok. */
    };


struct contig *contigsFromAgp(char *fileName, struct hash *hash)
/* Build up a list of contigs looking at agp file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
struct contig *contigList = NULL, *contig;
while (lineFileChop(lf, row))
    {
    char *name = row[0];
    int s = lineFileNeedNum(lf, row, 1) - 1;
    int e = lineFileNeedNum(lf, row, 2);
    int size = e - s;
    if (size < 0)
        errAbort("Start before end line %d of %s", lf->lineIx, lf->fileName);
    if ((contig = hashFindVal(hash, name)) == NULL)
        {
	AllocVar(contig);
	hashAddSaveName(hash, name, contig, &contig->name);
	slAddHead(&contigList, contig);
	}
    if (s != contig->size)
	errAbort("Start doesn't match previous end line %d of %s", lf->lineIx, lf->fileName);
    contig->size = e;
    }
lineFileClose(&lf);
slReverse(&contigList);
return contigList;
}

int alphaCount(char *s)
/* Return count of alphabetical characters. */
{
int count = 0;
char c;
while ((c = *s++) != 0)
    {
    if (isalpha(c))
        ++count;
    }
return count;
}

boolean faSizeNext(struct lineFile *lf, char retLine[512], int *retSize)
/* Get > line and size for next fa record.  Return FALSE at end of file. */
{
char *line;
int size = 0;

/* Fetch first record . */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        return FALSE;
    if (line[0] == '>')
        break;
    }
strncpy(retLine, line, 512);

/* Loop around counting DNA-looking characters until next record. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    if (line[0] == '>')
	{
        lineFileReuse(lf);
	break;
	}
    size += alphaCount(line);
    }
*retSize = size;
return TRUE;
}


int flagFa(char *fileName, struct hash *hash)
/* Read through .fa file and make sure that contigs match up. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char comment[512];
int size, i;
struct contig *contig;
int problemCount= 0;
/* char *name; */
char name[16];
unsigned totSize = 0;
unsigned seqCount = 0;

while (faSizeNext(lf, comment, &size))
    {
    /* Name no longer last word in line - TSF */
    /* name = lastWordInLine(comment); */
    for (i = 0; i < 9; i++) 
	{
	name[i] = comment[i+5];
	}
    name[9] = '\0';
    totSize += size;
    ++seqCount;
    if (name == NULL || (!startsWith("NT_", name) && !startsWith("NG_", name)
	&& !startsWith("NC_", name)))
	{
	++problemCount;
        /* printf("%s has an entry which doesn't have NT_ as a last word\n", fileName); */
        printf("%s has an entry which doesn't have NT_ NG_ or NC_ as name of contig: '%s', at sequence %u\n", fileName, name, seqCount);
	}
    else
        {
	if ((contig = hashFindVal(hash, name)) == NULL)
	    {
	    ++problemCount;
	    printf("%s is in %s, but not agp file\n", name, fileName);
	    }
	else
	    {
	    if (contig->size != size)
	        {
		++problemCount;
		printf("%s is %d bases in %s, but %d bases in agp file\n", 
			name, size, fileName, contig->size);
		}
	    contig->gotFa = TRUE;
	    }
	}
    }
lineFileClose(&lf);
return problemCount;
}

int flagMd(char *fileName, struct hash *hash)
/* Parse through md file and make sure it's consistent with contigs in hash */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int problemCount = 0;
char *row[9], *name;
static char lastChrom[64];
int lastEnd = 0, s, e, size;
struct contig *contig;

while (lineFileRow(lf, row))
    {
    /* Get name and just continue if it's a special start or end thingie. */
    name = row[5];
    if (sameString(name, "start") || sameString(name, "end"))
        continue;

    /* Get start/end in half-open zero based coordinates, and check size is ok. */
    s = lineFileNeedNum(lf, row, 2) - 1;
    e = lineFileNeedNum(lf, row, 3);
    size = e - s;
    if (size < 0)
	{
        printf("negative sized contig line %d of %s\n", lf->lineIx, lf->fileName);
	++problemCount;
	}

    /* Check that the start of this fragment is after the end of the previous
     * fragment unless starting a new chromosome (in which case it should start at 0). */
    if (!sameString(lastChrom, row[1]))
	{
        lastEnd = 0;
	strncpy(lastChrom, row[1], sizeof(lastChrom));
	}
    if (lastEnd > s)
        {
	printf("Overlapping contigs line %d and %d of %s\n", lf->lineIx-1, lf->lineIx, lf->fileName);
	++problemCount;
	}
    lastEnd = e;
    
    
    /* Make sure this contig was in agp file and that it's the same size. */
    if ((contig = hashFindVal(hash, name)) == NULL)
        {
	printf("%s is in %s but not agp file\n", name, fileName);
	++problemCount;
	}
    else
        {
	if (contig->size != size)
	    {
	    printf("%s is %d bases in %s, but %d bases in agp file\n", 
		    name, size, fileName, contig->size);
	    ++problemCount;
	    }
	contig->gotMd = TRUE;
	}
    }
lineFileClose(&lf);
return problemCount;
}

int checkOurContig(char *contigDir, struct contig *contig)
/* Check files in contigDir. */
{
char fileName[512];
struct dnaSeq *seq;
int problemCount = 0;

/* Check FA file for size. */
sprintf(fileName, "%s/%s.fa", contigDir, contig->name);
uglyf("Checking %s %d\n", fileName, contig->size);
if (!fileExists(fileName))
   {
   printf("%s doesn't exist\n", fileName);
   return 1;
   }
seq = faReadAllDna(fileName);
if (seq == NULL)
   {
   printf("%s has no sequence\n", fileName);
   return 1;
   }
if (slCount(seq) != 1)
   {
   ++problemCount;
   printf("%s has more than one sequence\n", fileName);
   }
if (seq->size != contig->size)
   {
   ++problemCount;
   printf("%s is %d bases according to NCBI, but %d bases in %s",
       contig->name, contig->size, seq->size, fileName);
   }

freeDnaSeqList(&seq);
return problemCount;
}

void checkOurDir(char *ourDir, struct contig *contigList, struct hash *hash)
/* Check that our directories look ok. */
{
struct us
    {
    struct us *next;    /* Next in list */
    char *contig;	/* NT_XXXXXX or NG_XXXXXX */
    char *chrom;        /* 1, 2, 3, etc. */
    };
struct hash *ourHash = newHash(0);
struct us *usList = NULL, *us;
struct fileInfo *chromList = NULL, *chromFi, *ctgList = NULL, *ctgFi;
char chromDir[512], ctgDir[512];
struct contig *contig;
int problemCount = 0;

/* Build up a hash that says where each contig is. */
chromList = listDirX(ourDir, "*", FALSE);
for (chromFi = chromList; chromFi != NULL; chromFi = chromFi->next)
    {
    if (chromFi->isDir && strlen(chromFi->name) <= 2)
        {
	sprintf(chromDir, "%s/%s", ourDir, chromFi->name);
	ctgList = listDirX(chromDir, "N?_*", FALSE);
	for (ctgFi = ctgList; ctgFi != NULL; ctgFi = ctgFi->next)
	    {
	    if (ctgFi->isDir)
	        {
		AllocVar(us);
		slAddHead(&usList, us);
		us->contig = ctgFi->name;
		us->chrom = chromFi->name;
		hashAdd(ourHash, us->contig, us);
		}
	    }
	}
    }
printf("We have %d contigs\n", slCount(usList));

/* Check each contig. */
for (contig = contigList; contig != NULL; contig = contig->next)
    {
    if ((us = hashFindVal(ourHash, contig->name)) == NULL)
        {
	++problemCount;
	printf("%s is not in %s\n", contig->name, ourDir);
	}
    else
        {
	sprintf(ctgDir, "%s/%s/%s", ourDir, us->chrom, us->contig);
	problemCount += checkOurContig(ctgDir, contig);
	}
    }
freeHash(&ourHash);
}

void checkYbr(char *agpFile, char *faFile, char *mdFile)
/* checkYbr - Check NCBI assembly (aka Yellow Brick Road). */
{
struct hash *hash = newHash(0);
struct contig *contigList = NULL, *contig;
int problemCount = 0;

contigList = contigsFromAgp(agpFile, hash);
printf("Read %d contigs from %s\n", slCount(contigList), agpFile);
if (cgiVarExists("checkUs"))
    {
    printf("Checking our fa files for size\n");
    checkOurDir(cgiString("checkUs"), contigList, hash);
    }
else
    {
    problemCount += flagMd(mdFile, hash);
    printf("Verifying sequence sizes in %s\n", faFile);
    problemCount += flagFa(faFile, hash);
    for (contig = contigList; contig != NULL; contig = contig->next)
	{
	if (contig->gotFa && contig->gotMd)
	    continue;
	++problemCount;
	if (!contig->gotFa)
	    printf("No %s in %s\n", contig->name, faFile);
	if (!contig->gotMd)
	    printf("No %s in %s\n", contig->name, mdFile);
	}
    printf("%d problems detected\n", problemCount);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
checkYbr(argv[1], argv[2], argv[3]);
return 0;
}
