/* interactFixRanges - set chromStart and chromEnd to max extent of source and target regions */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "interact.h"

/* Command line switches. */
void usage()
/* Explain usage and exit. */
{
errAbort(
  "interactFixRanges - set chromStart and chromEnd of interact format file based on source and target regions\n"
  "usage:\n"
  "   interactFixRanges inFile outFile\n\n"
  );
}

static void interactFixRanges(char *inFile, char *outFile)
/* interactFixRanges - process file to extend ranges. */
{
verbose(1, "Reading %s\n", inFile);
struct interact *inter, *inters = interactLoadAllAndValidate(inFile);
FILE *f = mustOpen(outFile, "w");
for (inter = inters; inter; inter = inter->next)
    {
    interactFixRange(inter);
    interactOutputCustom(inter, f, '\t','\n'); 
    }
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
interactFixRanges(argv[1], argv[2]);
return 0;
}
