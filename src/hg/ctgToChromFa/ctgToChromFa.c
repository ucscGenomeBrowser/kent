/* ctgToChromFa - convert contig level fa files to chromosome level. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "chromInserts.h"
#include "errAbort.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ctgToChromFa - convert contig level fa files to chromosome level\n"
  "usage:\n"
  "   ctgToChromFa chromName inserts chromDir ordered.lst outFile\n"
  "options:\n"
  "   spacing=number  - set spacing between contigs to number (default 200000)\n"
  "   lift=file.lft - set spacing between contigs from lift file. \n"
  "   -missOk - Warns rather than aborts on missing sequence\n"
  );
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

int maxLineSize = 50;
int linePos = 0;

void charOut(FILE *f, char c)
/* Write char out to file, breaking at lines as needed. */
{
fputc(c, f);
if (++linePos >= maxLineSize)
    {
    fputc('\n', f);
    linePos = 0;
    if (ferror(f))
	{
	perror("Error writing fa file\n");
	errAbort("\n");
	}
    }
}

void addN(FILE *f, int count)
/* Write count N's to fa file. */
{
int i;
for (i=0; i<count; ++i)
    charOut(f, 'N');
}

int addFa(FILE *f, char *ctgFaName)
/* Append contents of FA file. Return the number of bases written. */
{
struct lineFile *lf = lineFileOpen(ctgFaName, TRUE);
int lineSize;
char *line, c;
int recordCount = 0;
int baseCount = 0;
int i;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	++recordCount;
	if (recordCount > 1)
	   warn("More than one record in %s\n", ctgFaName);
	}
    else
        {
	for (i=0; i<lineSize; ++i)
	    {
	    c = line[i];
	    if (isalpha(c))
		{
	        charOut(f, c);
		baseCount++;
		}
	    }
	}
    }
lineFileClose(&lf);
return(baseCount);
}

struct lift
/* Info on where contig is in chromosome, read and computed from
 * a .lft file. */
    {
    struct lift *next;	/* Next in list. */
    char *contig;	/* Allocated in hash, doesn't include chromosome. */
    int start;		/* Start of contig in chromosome. */
    int size;		/* Size of contig. */
    int nBefore;	/* Number of N's to insert before. */
    int chromSize;      /* Chromosome size. */
    };

void ctgToChromFa(char *chromName, char *insertFile, char *chromDir, 
	char *orderLst, char *outName, struct hash *liftHash)
/* ctgToChromFa - convert contig level fa files to chromosome level. */
{
struct hash *uniq = newHash(0);
struct bigInsert *bi;
struct chromInserts *chromInserts;
struct hash *insertHash = newHash(9);
struct lineFile *lf = lineFileOpen(orderLst, TRUE);
FILE *f = mustOpen(outName, "w");
char ctgFaName[512];
char *words[2];
int liftChromSize = 0;
int actualChromSize = 0;
boolean isFirst = TRUE;

chromInsertsRead(insertFile, insertHash);
chromInserts = hashFindVal(insertHash, chromName);
fprintf(f, ">%s\n", chromName);
while (lineFileNextRow(lf, words, 1))
    {
    char *contig = words[0];
    int nSize;
    
    if (liftHash != NULL)
        {
	struct lift *lift = hashMustFindVal(liftHash, contig);
	nSize = lift->nBefore;
	liftChromSize = lift->chromSize;
	}
    else
        nSize = chromInsertsGapSize(chromInserts, rmChromPrefix(contig), isFirst);
    hashAddUnique(uniq, contig, NULL);
    addN(f, nSize);
    actualChromSize += nSize;
    isFirst = FALSE;
    sprintf(ctgFaName, "%s/%s/%s.fa", chromDir, contig, contig);
    if (fileExists(ctgFaName))
        {
	actualChromSize += addFa(f, ctgFaName);
	}
    else
        {
	warn("%s does not exist\n", ctgFaName);
	if (!cgiVarExists("missOk"))
	    noWarnAbort();
	}
    }
lineFileClose(&lf);
if (chromInserts != NULL)
    if  ((bi = chromInserts->terminal) != NULL)
        {
	addN(f, bi->size);
	actualChromSize += bi->size;
	}
if (liftHash != NULL)
    {
    if (actualChromSize > liftChromSize)
	errAbort("Error: chromosome size from lift file is %d, but actual fa size is %d.  Possible inconsistency between lift and inserts?",
		 liftChromSize, actualChromSize);
    else if (actualChromSize < liftChromSize)
	addN(f, (liftChromSize - actualChromSize));
    }
if (linePos != 0)
   fputc('\n', f);
fclose(f);
}

struct hash *readLift(char *fileName)
/* Read a lift file into hash */
{
int lastEnd = 0;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct lift *lift;
struct hash *hash = newHash(0);
char *s;

while (lineFileRow(lf, row))
    {
    AllocVar(lift);
    s = strchr(row[1], '/');
    if (s == NULL)
        errAbort("Missing chromosome in chrom/contig field line %d of %s", 
	    lf->lineIx, lf->fileName);
    s += 1;
    hashAddSaveName(hash, s, lift, &lift->contig);
    lift->start = lineFileNeedNum(lf, row, 0);
    lift->size = lineFileNeedNum(lf, row, 2);
    lift->nBefore = lift->start - lastEnd;
    lift->chromSize = lineFileNeedNum(lf, row, 4);
    lastEnd = lift->start + lift->size;
    }
return hash;
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *liftHash = NULL;
cgiSpoof(&argc, argv);
if (argc != 6)
    usage();
if (cgiVarExists("spacing"))
    chromInsertsSetDefaultGapSize(cgiInt("spacing"));
if (cgiVarExists("lift"))
    liftHash = readLift(cgiString("lift"));
ctgToChromFa(argv[1], argv[2], argv[3], argv[4], argv[5], liftHash);
return 0;
}
