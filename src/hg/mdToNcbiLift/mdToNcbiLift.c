/* mdToNcbiLift - Convert seq_contig.md file to ncbi.lft. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mdToNcbiLift - Convert seq_contig.md file to ncbi.lft\n"
  "usage:\n"
  "   mdToNcbiLift seq_contig.md ncbi.lft\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chromSize
/* Size info on chromosome. */
   {
   struct chrom *next;
   char *name;	/* Name allocated in hash. */
   int size;	/* ChromSize. */
   };

struct hash *chromSizeHash(char *fileName)
/* Return chromSize valued hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[9];
struct chromSize *cs;

while (lineFileRow(lf, row))
    {
    char *chrom = row[1];
    int end = lineFileNeedNum(lf, row, 3);
    cs = hashFindVal(hash, chrom);
    if (cs == NULL)
        {
	AllocVar(cs);
	hashAddSaveName(hash, chrom, cs, &cs->name);
	}
    if (end > cs->size)
        cs->size = end;
    }
lineFileClose(&lf);
return hash;
}

void mdToNcbiLift(char *inName, char *outName)
/* mdToNcbiLift - Convert seq_contig.md file to ncbi.lft. */
{
struct hash *hash = chromSizeHash(inName);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[9];

while (lineFileRow(lf, row))
    {
    int start = lineFileNeedNum(lf, row, 2) - 1;
    int end = lineFileNeedNum(lf, row, 3);
    char *chrom = row[1];
    char strand = row[4][0];
    struct chromSize *cs = hashMustFindVal(hash, chrom);
    fprintf(f, "%d\t", start);
    fprintf(f, "%s/%s\t", chrom, row[5]);
    fprintf(f, "%d\t", end - start);
    fprintf(f, "chr%s\t", chrom);
    fprintf(f, "%d\t", cs->size);
    if (strand != '-') strand = '+';
    fprintf(f, "%c\n", strand);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
mdToNcbiLift(argv[1], argv[2]);
return 0;
}
