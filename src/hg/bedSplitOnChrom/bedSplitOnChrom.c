/* bedSplitOnChrom - Split bed into a directory with one file per chromosome.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "bed.h"


int maxChromCount = 256;
boolean nfCheck;    /* check for number of fields consistency */
boolean doStrand;   /* append strand to file name */
boolean doAppend;   /* append to rather than create files */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedSplitOnChrom - Split bed into a directory with one file per chromosome.\n"
  "usage:\n"
  "   bedSplitOnChrom inFile.bed outDir\n"
  "options:\n"
  "   -append   append to rather than create files\n"
  "   -strand   append strand to file name\n"
  "   -noCheck  do not check to see if number of fields is same in every record\n"
  "   -maxChromCount=N Maximum number of different chromosomes, default %d\n"
  , maxChromCount);
}

static struct optionSpec options[] = {
   {"append", OPTION_BOOLEAN},
   {"strand", OPTION_BOOLEAN},
   {"noCheck", OPTION_BOOLEAN},
   {"maxChromCount", OPTION_INT},
   {NULL, 0},
};

void bedSplitOnChrom(char *inFile, char *outDir)
/* bedSplitOnChrom - Split bed into a directory with one file per chromosome. */
{
/* Create output directory. */
makeDir(outDir);

/* Open file and figure out how many fields there are. */
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *row[100];
int numFields = lineFileChopNext(lf, row, ArraySize(row));
char lastChrom[2048];

lastChrom[0] = 0;

if (numFields == 0)
    return;	/* Empty file, nothing to do. */
if (numFields >= ArraySize(row))
    errAbort("Too many fields (%d) in bed file %s.  Max is %d", 
    	numFields, lf->fileName, (int)(ArraySize(row)-1));
if (numFields < 3 || !isdigit(row[1][0]) || !isdigit(row[2][0]))
    errAbort("%s does not appear to be a bed file.", lf->fileName);

/* Output as needed, creating a hash of open files. */
char path[PATH_LEN];
struct hash *fileHash = hashNew(8);
char buffer[4096];
FILE *f = NULL;

for (;;)
    {
    /* Look up file in hash, creating a new file if need be. */
    char *chrom = row[0];

    if (doStrand)
	{
	char *ptr = buffer;

	for(;*chrom; chrom++, ptr++)
	    *ptr = *chrom;

	*ptr++ = row[5][0];
	*ptr++ = 0;

	chrom = buffer;
	}

    if (differentString(chrom, lastChrom))
	{
	f = hashFindVal(fileHash, chrom);
	strcpy(lastChrom, chrom);
	verbose(2, "new chrom %s f %p\n", lastChrom,f);
	}

    if (f == NULL)
        {
	if (fileHash->elCount >= maxChromCount)
	    errAbort("%s is the %dth chromosome, which is too many. "
	             "Use maxChromCount option if need be.",
		     chrom, fileHash->elCount+1);
	safef(path, sizeof(path), "%s/%s.bed", outDir, chrom);
	f = mustOpen(path, doAppend ? "a" : "w");
	verbose(2, "opened %s f %p\n",path, f);
	hashAdd(fileHash, chrom, f);
	}

    /* Output line of bed file, starting with the three fields that are always there. */
    fprintf(f, "%s\t%s\t%s", row[0], row[1], row[2]);
    int i;
    for (i=3; i<numFields; ++i)
	fprintf(f, "\t%s", row[i]);
    fputc('\n', f);
    if (ferror(f))
	{
	safef(path, sizeof(path), "%s/%s.bed", outDir, chrom);
        errnoAbort("Couldn't write to %s.", path);
	}

    /* Fetch next line, breaking loop if it's not there, 
     * and maybe insuring that it has the usual number of fields. */
    int fieldsInLine = lineFileChopNext(lf, row, ArraySize(row));
    if (fieldsInLine == 0)
        break;
    if (nfCheck && (fieldsInLine != numFields))
	errAbort("First line in %s had %d fields, but line %d has %d fields.",
		lf->fileName, numFields, lf->lineIx, fieldsInLine);
    }

/* Careful close all output files to make sure last bytes really written. */
struct hashEl *hel;
for (hel = hashElListHash(fileHash); hel != NULL; hel = hel->next)
    {
    FILE *f = hel->val;
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxChromCount = optionInt("maxChromCount", maxChromCount);
if (argc != 3)
    usage();
nfCheck = !optionExists("noCheck");
doStrand = optionExists("strand");
doAppend = optionExists("append");
bedSplitOnChrom(argv[1], argv[2]);
return 0;
}
