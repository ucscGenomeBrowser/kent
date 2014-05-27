/* mafIndex - Index maf file based on a particular target. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafIndex - Index maf file based on a particular target\n"
  "usage:\n"
  "   mafIndex targetSeq in.maf out.maf.ix\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void mafIndex(char *target, char *in, char *out)
/* mafIndex - Index maf file based on a particular target. */
{
struct mafFile *mf = mafOpen(in);
FILE *f = mustOpen(out, "w");
struct mafAli *maf;
struct mafComp *mc;
off_t pos;

while ((maf = mafNextWithPos(mf, &pos)) != NULL)
    {
    mc = mafMayFindComponent(maf, target);
    if (mc != NULL)
	fprintf(f, "%d %d %llu\n", mc->start, mc->size, (unsigned long long)pos); 
    }
mafFileFree(&mf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mafIndex(argv[1], argv[2], argv[3]);
return 0;
}
