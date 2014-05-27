/* edwFixGbb2 - Second attempt to fix gtf/big bed problem.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixGbb2 - Second attempt to fix gtf/big bed problem.\n"
  "usage:\n"
  "   edwFixGbb2 dupeFile output\n"
  "where dupeFile is space separated in format <count> <md5>\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixGbb2(char *dupeFile, char *outFile)
/* edwFixGbb2 - Second attempt to fix gtf/big bed problem.. */
{
char *row[2];
struct lineFile *lf = lineFileOpen(dupeFile, FALSE);
FILE *f = mustOpen(outFile, "w");
while (lineFileRow(lf, row))
    {
    char *md5 = row[1];
    fprintf(f, "update edwFile set deprecated='Uninformative and duplicated because bigBed recapitulates entire input gene set.' where md5='%s' and edwFileName like '%%.gtf.bigBed';\n", md5);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixGbb2(argv[1], argv[2]);
return 0;
}
