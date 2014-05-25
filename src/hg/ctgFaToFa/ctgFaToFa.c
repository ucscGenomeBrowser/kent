/* ctgFaToFa - Convert from one big file with all NT contigs to one contig per file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ctgFaToFa - Convert from one big file with all NT contigs to one contig per file.\n"
  "usage:\n"
  "   ctgFaToFa ctg.fa ctg_coords ntDir\n");
}

struct ntContig
/* ntContig structure. */
    {
    struct ntContig *next;
    char *name;		/* Allocated in hash */
    char *hsName;	/* Allocated in hash */
    int cloneCount;
    };

FILE *fakeFile = NULL;

char *nextFakeNtName(char *hsName, char *oldName)
/* Here is where you come to generate a fake
 * name for an NT contig. */
{
static int i = 0;
static char fakeName[64];

if (fakeFile == NULL)
    fakeFile = mustOpen("ctgFa.fakes", "w");
sprintf(fakeName, "NT_FAKE%d", ++i);
warn("Generating fake name %s for %s (old %s)", fakeName, hsName, oldName);
fprintf(fakeFile, "%s\t%s\t%s\n", fakeName, hsName, oldName);
return fakeName;
}

void ctgFaToFa(char *ctgFa, char *ctgCoords, char *ntDir)
/* ctgFaToFa - Convert from one big file with all NT contigs to one contig per file.. */
{
struct lineFile *lf;
char fileName[512], *line;
char *ntName, *hsName;
char *parts[6];
int lineSize, partCount;
struct hash *uniqHash = newHash(0);
FILE *f = NULL;
int dotMod = 0;
struct hash *ntHash = newHash(0);
struct hash *hsHash = newHash(0);
struct ntContig *nt;
char *words[8];

printf("Loading %s\n", ctgCoords);
lf = lineFileOpen(ctgCoords, TRUE);
while (lineFileRow(lf, words))
    {
    ntName = words[0];
    if ((nt = hashFindVal(ntHash, ntName)) != NULL)
        ++nt->cloneCount;
    else
        {
	AllocVar(nt);
	hashAddSaveName(ntHash, ntName, nt, &nt->name);
	hashAddSaveName(hsHash, words[1], nt, &nt->hsName);
	nt->cloneCount = 1;
	}
    }
lineFileClose(&lf);


lf = lineFileOpen(ctgFa, FALSE);
makeDir(ntDir);
while (lineFileNext(lf, &line, &lineSize))
    {
    if ((++dotMod&0x1ffff) == 0)
        {
	printf(".");
	fflush(stdout);
	}
    if (line[0] == '>')
        {
	carefulClose(&f);
	line[lineSize-1] = 0;
	partCount = chopByChar(line, '|',parts,ArraySize(parts));
	if (partCount < 3)
	    {
	    uglyf("partCount = %d\n", partCount);
	    errAbort("Expecting | separated header line %d of %s", lf->lineIx, lf->fileName); 
	    }
	ntName = parts[1];
	nt = hashFindVal(ntHash, ntName);
	hsName = parts[2];
	if (nt == NULL)
	    {
	    hsName = firstWordInLine(ntName);
	    nt = hashMustFindVal(hsHash, hsName);
	    ntName = nt->name;
	    }
	if (nt->cloneCount > 1)
	    {
	    if (!startsWith("Hs", hsName))
	        errAbort("Expecting %s to start with 'Hs' line %d of %s",
			hsName, lf->lineIx, lf->fileName);
	    if (hashLookup(uniqHash, ntName))
	        ntName = nextFakeNtName(hsName, ntName);
	    hashAddUnique(uniqHash, ntName, NULL);
	    if (!startsWith("NT_", ntName))
		errAbort("Expecting NT_ name line %d of %s", lf->lineIx, lf->fileName); 
	    sprintf(fileName, "%s/%s.fa", ntDir, ntName);
	    f = mustOpen(fileName, "w");
	    fprintf(f, ">%s.1_1\n", ntName);
	    }
	}
    else
        {
	if (f != NULL)
	    mustWrite(f, line, lineSize);
	}
    }
printf("\n");
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
ctgFaToFa(argv[1], argv[2], argv[3]);
return 0;
}
