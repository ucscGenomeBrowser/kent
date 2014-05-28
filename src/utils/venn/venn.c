/* venn - Do venn diagram calculations. */

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
  "venn - Do venn diagram calculations\n"
  "usage:\n"
  "   venn aSize abSize bSize\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void venn(double a, double ab, double b)
/* venn - Do venn diagram calculations. */
{
double total = a + b - ab;
printf("A only  %4.1f%%\n", 100*(a-ab)/total);
printf("A and B %4.1f%%\n", 100*ab/total);
printf("B only  %4.1f%%\n", 100*(b-ab)/total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
venn(atof(argv[1]), atof(argv[2]), atof(argv[3]));
return 0;
}
