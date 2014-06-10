/* fragPart - get a part of a fragment. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "fa.h"
#include "hCommon.h"


char *faDirs[] = 
    {
    "/projects/cc/hg/gs.2/fin/fa",
    "/projects/cc/hg/gs.2/draft/fa",
    "/projects/cc/hg/gs.2/predraft/fa",
    };

int faDirCount = 3;

void usage()
/* Explain usage and exit. */
{
errAbort(
"fragPart - get part of a fragment's sequence\n"
"usage:\n"
"   fragPart acc.frag [strand] [start] [end]\n"
"examples:\n"
"   fragPart AC000001.4_3 + 100 200\n"
"This gets between bases 100 and 200 of fragment 4_3 in \n"
"AC000001.fa somewhere....\n");
}

void fragPart(char *frag, char *strand, int start, int end)
/* fragPart - get a part of a fragment. */
{
char fileName[512];
char fragPartName[256];
int fragSize;
int i;
boolean gotName = FALSE, gotFrag = FALSE;
DNA *dna;
int size;
char *name;
FILE *f;
boolean isRc = (strand[0] == '-');

for (i=0; i<faDirCount; ++i)
    {
    char *dir = faDirs[i];
    faRecNameToFaFileName(dir, frag, fileName);
    if (fileExists(fileName))
	{
	gotName = TRUE;
	break;
	}
    }
if (!gotName)
    errAbort("Couldn't locate %s", frag);

f = mustOpen(fileName, "r");
while (faFastReadNext(f, &dna, &size, &name))
    {
    if (sameString(name, frag))
        {
	gotFrag = TRUE;
	break;
	}
    }
if (!gotFrag)
    errAbort("There is no fragment %s in %s", frag, fileName);

if (start < 0)
    start = 0;
if (end > size)
    end = size;
fragSize = end-start;
if (end <= start)
    errAbort("No sequence left after clipping.  (Start %d, end %d).",
       start, end);
sprintf(fragPartName, "%s_%d_%d%s", frag, start, end,
   (isRc ? "_rc" : ""));
if (isRc)
    reverseComplement(dna+start, fragSize);
faWriteNext(stdout, fragPartName, dna+start, fragSize);
}

int main(int argc, char *argv[])
/* Process command line and exit. */
{
char *strand = "+";
int start = 0;
int end = 0x3fffffff;

if (argc < 2)
    usage();
if (argc >= 3)
    {
    strand = argv[2];
    if (!sameString(strand, "+") && !sameString(strand, "-"))
	errAbort("Strand must be + or -");
    }
if (argc >= 4)
    {
    if (!isdigit(argv[3][0]))
	errAbort("Expecting start number");
    start = atoi(argv[3]);
    }
if (argc >= 5)
    {
    if (!isdigit(argv[4][0]))
	errAbort("Expecting end number");
    end = atoi(argv[4]);
    }
dnaUtilOpen();
fragPart(argv[1], strand, start, end);
return 0;
}


