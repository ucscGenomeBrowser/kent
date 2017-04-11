/* faToTwoBit - Convert DNA from fasta to 2bit format. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "fa.h"
#include "twoBit.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToTwoBit - Convert DNA from fasta to 2bit format\n"
  "usage:\n"
  "   faToTwoBit in.fa [in2.fa in3.fa ...] out.2bit\n"
  "options:\n"
  "   -long          use 64-bit offsets for index.   Allow for twoBit to contain more than 4Gb of sequence. \n"
  "                  NOT COMPATIBLE WITH OLDER CODE.\n"
  "   -noMask        Ignore lower-case masking in fa file.\n"
  "   -stripVersion  Strip off version number after '.' for GenBank accessions.\n"
  "   -ignoreDups    Convert first sequence only if there are duplicate sequence\n"
  "                  names.  Use 'twoBitDup' to find duplicate sequences."
  );
}

boolean noMask = FALSE;
boolean stripVersion = FALSE;
boolean ignoreDups = FALSE;
boolean useLong = FALSE;

static struct optionSpec options[] = {
   {"noMask", OPTION_BOOLEAN},
   {"stripVersion", OPTION_BOOLEAN},
   {"ignoreDups", OPTION_BOOLEAN},
   {"long", OPTION_BOOLEAN},
   {NULL, 0},
};

static void unknownToN(char *s, int size)
/* Convert non ACGT characters to N. */
{
char c;
int i;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (ntChars[(int)c] == 0)
        {
	if (isupper(c))
	    s[i] = 'N';
	else
	    s[i] = 'n';
	}
    }
}

	    
void faToTwoBit(char *inFiles[], int inFileCount, char *outFile)
/* Convert inFiles in fasta format to outfile in 2 bit 
 * format. */
{
struct twoBit *twoBitList = NULL, *twoBit;
int i;
struct hash *uniqHash = newHash(18);
FILE *f;

for (i=0; i<inFileCount; ++i)
    {
    char *fileName = inFiles[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct dnaSeq seq;
    ZeroVar(&seq);
    while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
	if (seq.size == 0)
	    {
	    warn("Skipping item %s which has no sequence.\n",seq.name);
	    continue;
	    }
	    
        /* strip off version number */
        if (stripVersion)
            {
            char *sp = NULL;
            sp = strchr(seq.name,'.');
            if (sp != NULL)
                *sp = '\0';
            }

        if (hashLookup(uniqHash, seq.name))
            {
            if (!ignoreDups)
                errAbort("Duplicate sequence name %s", seq.name);
            else
                continue;
            }
	hashAdd(uniqHash, seq.name, NULL);
	if (noMask)
	    faToDna(seq.dna, seq.size);
	else
	    unknownToN(seq.dna, seq.size);
	twoBit = twoBitFromDnaSeq(&seq, !noMask);
	slAddHead(&twoBitList, twoBit);
	}
    lineFileClose(&lf);
    }
slReverse(&twoBitList);
f = mustOpen(outFile, "wb");
twoBitWriteHeaderExt(twoBitList, f, useLong);
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    twoBitWriteOne(twoBit, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
noMask = optionExists("noMask");
stripVersion = optionExists("stripVersion");
ignoreDups = optionExists("ignoreDups");
useLong = optionExists("long");
dnaUtilOpen();
faToTwoBit(argv+1, argc-2, argv[argc-1]);
return 0;
}
