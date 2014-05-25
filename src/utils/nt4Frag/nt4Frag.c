/* nt4Frag - Extract a piece of a .nt4 file to .fa format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "nt4.h"
#include "fa.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "nt4Frag - Extract a piece of a .nt4 file to .fa format\n"
  "usage:\n"
  "   nt4Frag file.nib start end strand out.fa\n");
}

void nt4Frag(char *fileName, int start, int end, char strand, char *faFile)
/* nt4Frag - Extract part of a .nt4 file as .fa. */
{
DNA *dna;
int size;
char seqName[512];

if (strand != '+' && strand != '-')
   {
   usage();
   }
if (start >= end)
   {
   usage();
   }
size = end - start;
dna = nt4LoadPart(fileName, start, size);
if (strand == '-')
    reverseComplement(dna, size);
sprintf(seqName, "%s:%d%c%d", fileName, start, strand, end);
faWrite(faFile, seqName, dna, size);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    {
    usage();
    }
if (!isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    {
    usage();
    }
nt4Frag(argv[1], atoi(argv[2]), atoi(argv[3]), argv[4][0], argv[5]);
return 0;
}
