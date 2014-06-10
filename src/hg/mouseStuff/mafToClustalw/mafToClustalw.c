/* mafToClustalw - convert maf file to clustalw. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "maf.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToClustalw - convert maf file to clustalw\n"
  "usage:\n"
  "   mafToClustalw in.maf out.fa\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void mafAliToClustalw(struct mafAli *maf, FILE *of)
/* convert a MAF alignment to a clustalw */
{
struct mafComp *c;
for (c = maf->components ; c ; c = c->next )
    fprintf(of, "%-18s%s\n", c->src, c->text);
fprintf(of,"\n");
}

void mafToClustalw(char *inName, char *outName)
/* mafToClustalw - convert maf file to clustalw. */
{
struct mafFile *mf = mafOpen(inName);
FILE *faFh = mustOpen(outName, "w");
struct mafAli *maf;

fprintf(faFh, "CLUSTAL W  Clustalw-like format converted from MAF file\n\n");
while ((maf = mafNext(mf)) != NULL)
    {
    mafAliToClustalw(maf, faFh);
    mafAliFree(&maf);
    }
carefulClose(&faFh);
mafFileFree(&mf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
++argv;
--argc;
if (argc != 2)
    usage();
mafToClustalw(argv[0], argv[1]);
return 0;
}

