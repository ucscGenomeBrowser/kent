/* gff3Tester - tester for GFF3 objects */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "gff3.h"

static void usage()
/* Print usage message */
{
errAbort(
   "gff3Tester - tester for GFF3 objects\n"
   "usage:\n"
   "   gff3Tester gff3In gff3Out\n"
   "\n");
}
static void gff3Tester(char *gff3InFile, char *gff3OutFile)
/* tester for GFF3 objects */
{
struct gff3File *g3f = gff3FileOpen(gff3InFile, -1, 0, NULL);
gff3FileWrite(g3f, gff3OutFile);
gff3FileFree(&g3f);
}

int main(int argc, char *argv[])
{
if (argc != 3)
    usage();
gff3Tester(argv[1], argv[2]);
return 0;
}


