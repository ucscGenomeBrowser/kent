/* crTreeSearchBed - Search a crTree indexed bed file and print all items that overlap query.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "crTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "crTreeSearchBed - Search a crTree indexed bed file and print all items that overlap query.\n"
  "usage:\n"
  "   crTreeSearchBed file.bed index.cr chrom start end\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void crTreeSearchBed(char *bedFile, char *treeFile, char *chrom, bits32 start, bits32 end)
/* crTreeSearchBed - Search a crTree indexed bed file and print all items that overlap query.. */
{
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
struct crTreeFile *crf = crTreeFileOpen(treeFile);
struct fileOffsetSize *block, *blockList = crTreeFindOverlappingBlocks(crf, chrom, start, end);
verbose(2, "Got %d overlapping blocks\n", slCount(blockList));
for (block = blockList; block != NULL; block = block->next)
    {
    verbose(2, "block->offset %llu, block->size %llu\n", block->offset, block->size);
    lineFileSeek(lf, block->offset, SEEK_SET);
    bits64 sizeUsed = 0;
    while (sizeUsed < block->size)
        {
	char *line;
	int size;
	if (!lineFileNext(lf, &line, &size))
	    errAbort("Couldn't read %s\n", lf->fileName);
	char *parsedLine = cloneString(line);
	char *row[3];
	if (chopLine(parsedLine, row) != ArraySize(row))
	    errAbort("Badly formatted line of %s\n%s", lf->fileName, line);
	char *bedChrom = row[0];
	bits32 bedStart = sqlUnsigned(row[1]);
	bits32 bedEnd = sqlUnsigned(row[2]);
	if (sameString(bedChrom, chrom) && rangeIntersection(bedStart, bedEnd, start, end) > 0)
	    fprintf(stdout, "%s\n", line);
	freeMem(parsedLine);
	sizeUsed += size;
	}
    }
crTreeFileClose(&crf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
crTreeSearchBed(argv[1], argv[2], argv[3], sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
return 0;
}
