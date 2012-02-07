/* checkableBorf - Convert borfBig orf-finder output to checkable form. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkableBorf - Convert borfBig orf-finder output to checkable form\n"
  "usage:\n"
  "   checkableBorf file.borf file.cds output.check\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct mrnaInfo
/* Cds position and related stuff - just for testing. */
    {
    struct mrnaInfo *next;
    char *name;		/* Allocated in hash */
    int size;		/* Size of mRNA */
    int cdsStart;	/* Start of coding. */
    int cdsEnd;		/* End of coding. */
    };

struct hash *readMrnaInfo(char *fileName)
/* Read in file into a hash of mrnaInfos. */
{
struct hash *hash = newHash(16);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[4];
while (lineFileRow(lf, row))
    {
    struct mrnaInfo *m;
    AllocVar(m);
    hashAddSaveName(hash, row[0], m, &m->name);
    m->size = lineFileNeedNum(lf, row, 1);
    m->cdsStart = lineFileNeedNum(lf, row, 2);
    m->cdsEnd = lineFileNeedNum(lf, row, 3);
    }
return hash;
}


void checkableBorf(char *borfFile, char *cdsFile, char *output)
/* checkableBorf - Convert borfBig orf-finder output to checkable form. */
{
struct hash *cdsHash = readMrnaInfo(cdsFile);
struct lineFile *lf = lineFileOpen(borfFile, TRUE);
char *row[6];
struct mrnaInfo *mi;
FILE *f = mustOpen(output, "w");

while (lineFileRow(lf, row))
    {
    mi = hashMustFindVal(cdsHash, row[0]);
    fprintf(f, "%s\t%d\t%d\t%d\t%s\t%s\n", mi->name, mi->size, 
    	mi->cdsStart, mi->cdsEnd, row[4], row[5]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
checkableBorf(argv[1], argv[2], argv[3]);
return 0;
}
