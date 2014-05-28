/* tfbsConsSort - a utility to sort tfbsCons files before loading them. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tfbsCons.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tfbsConsSort - a utility to sort tfbsCons files before loading them\n"
  "usage:\n"
  "   tfbsConsSort *.bed > out.bed\n"
  "    or\n"
  "   cat *.bed | tfbsConsSort stdin > out.bed\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int sortFunc(const void *e1, const void *e2)
{
struct tfbsCons *tfbs1 = *(struct tfbsCons **)e1;
struct tfbsCons *tfbs2 = *(struct tfbsCons **)e2;
int dif;

dif = strcmp(tfbs1->chrom, tfbs2->chrom);
if (dif == 0)
    dif = tfbs1->chromStart - tfbs2->chromStart;
if (dif == 0)
    dif = strcmp(tfbs1->name, tfbs2->name);
if (dif == 0)
    dif = strcmp(tfbs1->species, tfbs2->species);
return dif;
}


int main(int argc, char *argv[])
/* Process command line. */
{
int fileNo;
struct tfbsCons *el, *masterList = NULL;

optionInit(&argc, argv, options);
if (argc < 2)
    usage();

for(fileNo = 1; fileNo < argc; fileNo++)
    masterList = slCat(tfbsConsLoadAllByChar(argv[fileNo], '\t'), masterList);

slSort(&masterList, sortFunc);

for(el = masterList; el ; el=el->next)
    tfbsConsTabOut(el, stdout);

return 0;
}
