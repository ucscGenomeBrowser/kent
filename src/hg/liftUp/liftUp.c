/* liftUp - change coordinates of .psl, .agp, or .out file
 * to parent coordinate system. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "psl.h"
#include "rmskOut.h"
#include "chromInserts.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
 "liftUp - change coordinates of .psl, .agp, .gl, .out, .gff, .gtf .gdup or .bed files\n"
 "to parent coordinate system. \n"
 "usage:\n"
 "   liftUp [-type=.xxx] destFile liftSpec how sourceFile(s)\n"
 "The optional -type parameter tells what type of files to lift\n"
 "If omitted the type is inferred from the suffix of destFile\n"
 "Type is one of the suffixes described above.\n"
 "DestFile will contain the merged and lifted source files,\n"
 "with the coordinates translated as per liftSpec.  LiftSpec\n"
 "is tab-delimited with each line of the form:\n"
 "   offset oldName oldSize newName newSize\n"
 "The 'how' parameter controls what the program will do with\n"
 "items which are not in the liftSpec.  It must be one of:\n"
 "   carry - Items not in liftSpec are carried to dest without translation\n"
 "   drop  - Items not in liftSpec are silently dropped from dest\n"
 "   warn  - Items not in liftSpec are dropped.  A warning is issued\n"
 "If the destination is a .agp file then a 'large inserts' file\n"
 "also needs to be included in the command line:\n"
 "   liftUp dest.agp liftSpec how inserts sourceFile(s)\n"
 "This file describes where large inserts due to heterochromitin\n"
 "should be added.\n"
 );
}

boolean silentDrop;	/* True if should silently drop items not in liftSpec. */
boolean carryMissing;   /* True if should carry missing items untranslated. */
boolean pipeOut;	/* True if main output is stdout. */

struct liftSpec
/* How to lift coordinates. */
    {
    struct liftSpec *next;	/* Next in list. */
    char *oldName;		/* Name in source file. */
    int offset;			/* Offset to add. */
    char *newName;		/* Name in dest file. */
    int size;                   /* Size of new sequence. */
    };

struct liftSpec *readLifts(char *fileName)
/* Read in lift file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[16];
struct liftSpec *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *offs;
    lineFileExpectWords(lf, 5, wordCount);
    offs = words[0];
    if (!isdigit(offs[0]) && !(offs[0] == '-' && isdigit(offs[1])))
	errAbort("Expecting number in first field line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[4][0]))
	errAbort("Expecting number in fifth field line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(el);
    el->oldName = cloneString(words[1]);
    el->offset = atoi(offs);
    el->newName = cloneString(words[3]);
    el->size = atoi(words[4]);
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
if (!pipeOut) printf("Got %d lifts in %s\n", slCount(list), fileName);
if (list == NULL)
    errAbort("Empty liftSpec file %s", fileName);
return list;
}

char *rmChromPrefix(char *s)
/* Remove chromosome prefix if any. */
{
char *e = strchr(s, '/');
if (e != NULL)
    return e+1;
else
    return s;
}

void rmChromPart(struct liftSpec *list)
/* Turn chrom/ctg to just ctg . */
{
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    {
    el->oldName = rmChromPrefix(el->oldName);
    }
}

struct hash *hashLift(struct liftSpec *list)
/* Return a hash of the lift spec. */
{
struct hash *hash = newHash(0);
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    hashAdd(hash, el->oldName, el);
return hash;
}

struct liftSpec *findLift(struct hash *liftHash, char *oldName, struct lineFile *lf)
/* Find lift or  return NULL.  lf parameter is just for error reporting. */
{
struct hashEl *hel;
static int warnLeft = 10;	/* Only issue 10 warnings. */

if ((hel = hashLookup(liftHash, oldName)) == NULL)
    {
    if (!silentDrop && !carryMissing)
	{
	if (warnLeft > 0)
	    {
	    --warnLeft;
	    if (lf != NULL)
		warn("%s isn't in liftSpec file line %d of %s", oldName, lf->lineIx, lf->fileName);
	    else
		warn("%s isn't in liftSpec", oldName);
	    }
	}
    return NULL;
    }
return hel->val;
}


void skipLines(struct lineFile *lf, int count)
/* Skip a couple of lines. */
{
int i, lineSize;
char *line;

for (i=0; i<count; ++i)
    lineFileNext(lf, &line, &lineSize);
}

int numField(char **words, int field, int skip, struct lineFile *lf)
/* Read field from words as number.  lf parameter just for error reporting. */
{
char *s = words[field] + skip;
bool sign = FALSE;
int val;

if (s[0] == '-')
    {
    sign = TRUE;
    ++s;
    }
if (!isdigit(s[0]))
    errAbort("Expecting number field %d line %d of %s", field+1, lf->lineIx, lf->fileName);
val = atoi(s);
if (sign)
    val = -val;
return val;
}

void liftOut(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates in .out file. */
{
FILE *dest = mustOpen(destFile, "w");
char *source;
int i;
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[32];
char *s;
int begin, end, left;
char leftString[18];
char *id;
struct liftSpec *spec;
char *newName;

rmskOutWriteHead(dest);
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    if (!pipeOut) printf("Lifting %s\n", source);
    if (!fileExists(source))
	{
	warn("%s does not exist\n", source);
	continue;
	}
    lf = lineFileOpen(source, TRUE);
    if (!lineFileNext(lf, &line, &lineSize))
	{
	warn("%s is empty\n", source);
	lineFileClose(&lf);
	continue;
	}
    if (startsWith("There were no", line))
	{
	lineFileClose(&lf);
	continue;
	}
    skipLines(lf, 2);
    while (lineFileNext(lf, &line, &lineSize))
	{
	wordCount = chopLine(line, words);
	if (wordCount < 14 || wordCount > 15)
	    errAbort("Expecting 14 words line %d of %s", lf->lineIx, lf->fileName);
	if (wordCount == 15)
	    id = words[14];
	else
	    id = "";
	begin = numField(words, 5, 0, lf);
	end = numField(words, 6, 0, lf);
	s = words[7];
	if (s[0] != '(')
	    errAbort("Expecting parenthesized field 8 line %d of %s", lf->lineIx, lf->fileName);
	left = numField(words, 7, 1, lf);
	spec = findLift(liftHash, words[4], lf);
	if (spec == NULL) 
	    {
	    if (carryMissing)
	        newName = words[4];
	    else
		continue;
	    }
	else
	    {
	    begin += spec->offset;
	    end += spec->offset;
	    left = spec->size - end;
	    newName = spec->newName;
	    }
	sprintf(leftString, "(%d)", left);
	fprintf(dest, 
	  "%5s %5s %4s %4s  %-9s %7d %7d %9s %1s  %-14s %-19s %6s %4s %6s %6s\n",
	  words[0], words[1], words[2], words[3], newName,
	  begin, end, leftString,
	  words[8], words[9], words[10], words[11], words[12], words[13], id);
	}
    }
fclose(dest);
}

void liftPsl(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates in .psl file. */
{
FILE *dest = mustOpen(destFile, "w");
char *source;
int i,j;
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[32];
struct psl *psl;
unsigned *tStarts;
struct liftSpec *spec;
int offset;
int blockCount;
char *tName;

pslWriteHead(dest);
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    if (!fileExists(source))
	{
	warn("%s doesn't exist!", source);
	continue;
	}
    if (!pipeOut) printf("Lifting %s\n", source);
    lf = pslFileOpen(source);
    while ((psl = pslNext(lf)) != NULL)
	{
	tName = psl->tName;
	spec = findLift(liftHash, tName, lf);
	if (spec == NULL)
	    {
	    if (!carryMissing)
	        {
		pslFree(&psl);
		continue;
		}
	    }
	else
	    {
	    offset = spec->offset;
	    psl->tStart += offset;
	    psl->tEnd += offset;
	    blockCount = psl->blockCount;
	    tStarts = psl->tStarts;
	    if (psl->strand[1] == '-')
	        {
		for (j=0; j<blockCount; ++j)
		    {
		    int tr = psl->tSize - tStarts[j];
		    tr += offset;
		    tStarts[j] = spec->size - tr;
		    }
		}
	    else
	        {
		for (j=0; j<blockCount; ++j)
		    tStarts[j] += offset;
		}
	    psl->tSize = spec->size;
	    psl->tName = spec->newName;
	    }
	pslTabOut(psl, dest);
	psl->tName = tName;
	pslFree(&psl);
	}
    lineFileClose(&lf);
    }
fclose(dest);
}

void malformedAgp(struct lineFile *lf)
/* Report error in .agp. */
{
errAbort("Bad line %d in %s\n", lf->lineIx, lf->fileName);
}

void liftAgp(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates in .agp file. */
{
FILE *dest = mustOpen(destFile, "w");
char *source;
int i;
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[32];
char *s;
struct liftSpec *spec;
int start, end;
int ix = 0;
char newDir[256], newName[128], newExt[64];
struct bigInsert *bi;
struct chromInserts *chromInserts;
struct hash *insertHash = newHash(8);
struct hash *contigsHash = newHash(10);
boolean firstContig = TRUE;
char lastContig[256];
char *contig;
int lastEnd = 0;

if (sourceCount < 2)
    usage();

if (carryMissing)
    warn("'carry' doesn't work for .agp files, ignoring");

splitPath(destFile, newDir, newName, newExt);

/* Read in inserts file and process it. */
chromInsertsRead(sources[0], insertHash);
chromInserts = hashFindVal(insertHash, newName);

strcpy(lastContig, "");
for (i=1; i<sourceCount; ++i)
    {
    source = sources[i];
    if (!pipeOut) printf("Lifting %s\n", source);
    lf = lineFileMayOpen(source, TRUE);
    if (lf != NULL)
	{
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    /* Check for comments and just pass them through. */
	    s = skipLeadingSpaces(line);
	    if (s[0] == '#')
		{
		fprintf(dest, "%s\n", line);
		continue;
		}
	    /* Parse line, adjust offsets, write */
	    wordCount = chopLine(line, words);
	    if (wordCount != 8 && wordCount != 9)
		malformedAgp(lf);
	    contig = words[0];
	    if (!sameString(contig, lastContig))
	        {
		char *gapType = "contig";
		char *ctg = rmChromPrefix(contig);
		int gapSize = chromInsertsGapSize(chromInserts, 
			ctg, firstContig);
		if (hashLookup(contigsHash, contig))
		    errAbort("Contig repeated line %d of %s", lf->lineIx, lf->fileName);
		hashAdd(contigsHash, contig, NULL);
		if (gapSize != 0)
		    {
		    if ((bi = bigInsertBeforeContig(chromInserts, ctg)) != NULL)
		        {
			gapType = bi->type;
			}
		    fprintf(dest, "%s\t%d\t%d\t%d\tN\t%d\t%s\tno\n",
			newName, end+1, end+gapSize, ++ix, gapSize, gapType);
		    }
		firstContig = FALSE;
		strcpy(lastContig, contig);
		}
	    spec = findLift(liftHash, contig, lf);
	    start = numField(words, 1, 0, lf) + spec->offset;
	    end = numField(words, 2, 0, lf) + spec->offset;
	    if (end > lastEnd) lastEnd = end;
	    if (!sameString(newName, spec->newName))
	        errAbort("Mismatch in new name between %s and %s", newName, spec->newName);
	    fprintf(dest, "%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s",
		    newName, start, end, ++ix,
		    words[4], words[5], words[6], words[7]);
	    if (wordCount == 9)
		fprintf(dest, "\t%s", words[8]);
	    fputc('\n', dest);
	    }
	lineFileClose(&lf);
	}
    }
if (chromInserts != NULL)
    {
    if ((bi = chromInserts->terminal) != NULL)
        {
	fprintf(dest, "%s\t%d\t%d\t%d\tN\t%d\t%s\tno\n",
	    newName, lastEnd+1, lastEnd+bi->size, ++ix, bi->size, bi->type);
	}
    }
fclose(dest);
}

struct bedInfo
/* Info on a line of a bed file. */
    {
    struct bedInfo *next;	/* Next in list. */
    char *chrom;  /* Chromosome (not allocated here). */
    int start;    /* Start position. */
    int end;      /* End position. */
    char line[1]; /* Rest of line - allocated at run time. */
    };

int bedInfoCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedInfo *a = *((struct bedInfo **)va);
const struct bedInfo *b = *((struct bedInfo **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

int max3(int a, int b, int c)
/* Return max of a,b,c */
{
int m = max(a,b);
if (c > m) m = c;
return m;
}

void liftTabbed(char *destFile, struct hash *liftHash, 
   int sourceCount, char *sources[],
   int ctgWord, int startWord, int endWord, 
   boolean doubleLift, int ctgWord2, int startWord2, int endWord2)
/* Generic lift a tab-separated file with contig, start, and end fields. */
{
int minFieldCount = max3(startWord, endWord, ctgWord) + 1;
int wordCount, lineSize;
char *words[128], *line, *source;
struct lineFile *lf;
FILE *f = mustOpen(destFile, "w");
int i,j;
int start, end, start2, end2;
char *contig, *chrom = NULL, *chrom2 = NULL;
struct liftSpec *spec;
static char buf[1024*16];
char *s;
int len;
struct bedInfo *biList = NULL, *bi;
boolean anyHits = FALSE;

if (doubleLift)
   {
   int min2 = max3(ctgWord2, startWord2, endWord2);
   minFieldCount = max(minFieldCount, min2);
   }
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    lf = lineFileOpen(source, TRUE);
    if (!pipeOut) printf("Lifting %s\n", source);
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (line[0] == '#')
	    continue;
	wordCount = chopTabs(line, words);
	if (wordCount == 0)
	    continue;
	if (wordCount < minFieldCount)
	   errAbort("Expecting at least %d words line %d of %s", 
	   	minFieldCount, lf->lineIx, lf->fileName); 
	contig = words[ctgWord];
	contig = rmChromPrefix(contig);
	if (startWord >= 0)
	    start = lineFileNeedNum(lf, words, startWord);
	if (endWord >= 0)
	    end = lineFileNeedNum(lf, words, endWord);
	spec = findLift(liftHash, contig, lf);
	if (spec == NULL)
	    {
	    if (carryMissing)
		chrom = cloneString(contig);
	    else
		continue;
	    }
	else
	    {
	    chrom = spec->newName;
	    start += spec->offset;
	    end += spec->offset;
	    }
	if (doubleLift)
	    {
	    contig = words[ctgWord2];
	    start2 = lineFileNeedNum(lf, words, startWord2);
	    end2 = lineFileNeedNum(lf, words, endWord2);
	    spec = findLift(liftHash, contig, lf);
	    if (spec == NULL)
		{
		if (carryMissing)
		    chrom2 = cloneString(contig);
		else
		    errAbort("Couldn't find second contig in lift file at line %d of %s\n", lf->lineIx, lf->fileName);
		}
	    else
		{
		chrom2 = spec->newName;
		start2 += spec->offset;
		end2 += spec->offset;
		}
	    }
	anyHits = TRUE;
	s = buf;
	for (j=0; j<wordCount; ++j)
	    {
	    if (s + 128 >= buf + sizeof(buf))
	        errAbort("Line %d too long in %s", lf->lineIx, lf->fileName);
	    if (j != 0)
		*s++ = '\t';
	    if (j == ctgWord)
		s += sprintf(s, "%s", chrom);
	    else if (j == startWord)
	        s += sprintf(s, "%d", start);
	    else if (j == endWord)
	        s += sprintf(s, "%d", end);
	    else if (doubleLift && j == ctgWord2)
		s += sprintf(s, "%s", chrom2);
	    else if (doubleLift && j == startWord2)
	        s += sprintf(s, "%d", start2);
	    else if (doubleLift && j == endWord2)
	        s += sprintf(s, "%d", end2);
	    else
	        s += sprintf(s, "%s", words[j]);
	    }
	*s = 0;
	len = s-buf;
	bi = needMem(sizeof(*bi) + len);
	bi->chrom = chrom;
	bi->start = start;
	bi->end = end;
	memcpy(bi->line, buf, len);
	slAddHead(&biList, bi);
	}
    lineFileClose(&lf);
    }
slSort(&biList, bedInfoCmp);
for (bi = biList; bi != NULL; bi = bi->next)
    {
    fprintf(f, "%s\n", bi->line);
    }
fclose(f);
if (!anyHits)
   errAbort("No lines lifted!");
}

void liftBed(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift Browser Extensible Data file.  This is a tab
 * separated file where first three fields are 
 * seq, start, end.  This also sorts the result. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 1, 2, FALSE, 0, 0, 0);
}

void liftGff(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates of a .gff or a .gtf file. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 3, 4, FALSE, 0, 0, 0);
}

void liftGdup(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates of a .gdup. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 1, 2, TRUE, 6, 7, 8);
}


char *contigInDir(char *name, char dirBuf[256])
/* Figure out which contig we're in by the file name. It should be the directory
 * before us. */
{
char root[128], ext[64]; 
char *s, *contig;
int len;

splitPath(name, dirBuf, root, ext);
len = strlen(dirBuf);
if (len == 0)
    errAbort("Need contig directory in file name to lift .gl files\n");
if (dirBuf[len-1] == '/')
    dirBuf[len-1] = 0;
if ((s = strrchr(dirBuf, '/')) != NULL)
    contig = s+1;
else
    contig = dirBuf;
return contig;
} 

void liftGl(char *destFile, struct hash *liftHash, int sourceCount, char *sources[]) 
/* Lift up coordinates in .gl file. */ 
{ 
char dirBuf[256], chromName[256];
int i; 
char *source; 
char *contig; 
FILE *dest = mustOpen(destFile, "w"); 
char *s;
struct lineFile *lf = NULL;
int lineSize, wordCount;
char *line, *words[32];
struct liftSpec *spec;
int offset;

if (carryMissing)
    warn("'carry' doesn't work for .gl files, ignoring");
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    if (!pipeOut) printf("Processing %s\n", source);
    contig = contigInDir(source, dirBuf);
    if (!startsWith("ctg", contig))
        {
	sprintf(chromName, "chr%s", contig);
	contig = chromName;
	}
    spec = findLift(liftHash, contig, lf);
    if (spec == NULL)
        continue;
    offset = spec->offset;
    lf = lineFileMayOpen(source, TRUE);
    if (lf == NULL)
        {
	warn("%s doesn't exist, skipping", source);
	continue;
	}
    while (lineFileNext(lf, &line, &lineSize))
	{
	int s, e;
	if ((wordCount = chopLine(line, words)) != 4)
	    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
	s = atoi(words[1]);
	e = atoi(words[2]);
	fprintf(dest, "%s\t%d\t%d\t%s\n", words[0], s+offset, e+offset, words[3]);
	}
    lineFileClose(&lf);
    }
}

void liftUp(char *destFile, char *liftFile, char *how, int sourceCount, char *sources[])
/* liftUp - change coordinates of .psl, .agp, or .out file
 * to parent coordinate system. */
{
struct liftSpec *lifts;
struct hash *liftHash;
char *source = sources[0];
char *destType = cgiUsualString("type", destFile);

if (sameWord(how, "carry"))
    carryMissing = TRUE;
else if (sameWord(how, "warn"))
    ;
else if (sameWord(how, "drop"))
    silentDrop = TRUE;
else
    usage();
pipeOut = sameString(destFile, "stdout");
lifts = readLifts(liftFile);

if (endsWith(destType, ".out"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftOut(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".psl") || endsWith(destType, ".pslx"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftPsl(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".agp"))
    {
    liftHash = hashLift(lifts);
    liftAgp(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gl"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftGl(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gff") || endsWith(destType, ".gtf"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftGff(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gdup"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftGdup(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".bed"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftBed(destFile, liftHash, sourceCount, sources);
    }
else if (strstr(destType, "gold"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts);
    liftAgp(destFile, liftHash, sourceCount, sources);
    }
else 
    {
    errAbort("Unknown file suffix for %s\n", destType);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 5)
    usage();
liftUp(argv[1], argv[2], argv[3], argc-4, argv+4);
}

