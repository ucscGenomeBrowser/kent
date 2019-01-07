/* twoBitDup - check to see if a twobit file has any identical sequences in it. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"
#include "dnaseq.h"
#include "math.h"
#include "udc.h"
#include "md5.h"

// static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitDup - check to see if a twobit file has any identical sequences in it\n"
  "usage:\n"
  "   twoBitDup file.2bit\n"
  "options:\n"
  "  -keyList=file - file to write a key list, two columns: md5sum and sequenceName\n"
  "                   NOTE: use of keyList is very time expensive for 2bit files\n"
  "                   with a large number of sequences (> 5,000).  Better to\n"
  "                   use a cluster run with the doIdKeys.pl automation script.\n"
  "  -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "\nexample: twoBitDup -keyList=stdout db.2bit \\\n"
  "          | grep -v 'are identical' | sort > db.idKeys.txt"
  );
}

static char *keyList = NULL;

static struct optionSpec options[] = {
   {"keyList", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void twoBitDup(char *filename)
/* twoBitDup - check to see if a twobit file has any identical sequences in it. */
{
struct twoBitFile *tbf;

tbf = twoBitOpen(filename);
struct twoBitIndex *index;
int seqCount = slCount(tbf->indexList);
int hashSize = log2(seqCount) + 2;	 // +2 for luck
struct hash *seqHash = newHash(hashSize);
FILE *keyListFile = NULL;

verbose(2, "hash size is %d\n", hashSize);
if (keyList)
    {
    verbose(2, "writing key list to %s\n", keyList);
    keyListFile = mustOpen(keyList, "w");
    }

for (index = tbf->indexList; index != NULL; index = index->next)
    {
    verbose(2,"grabbing seq %s\n", index->name);
    int size;
    struct dnaSeq *seq = twoBitReadSeqFragExt(tbf, index->name,
	0, 0, FALSE, &size);
    struct hashEl *hel;
    if ((hel = hashLookup(seqHash, seq->dna)) != NULL)
	printf("%s and %s are identical\n", index->name, (char *)hel->val);
    else
	hel = hashAdd(seqHash, seq->dna, index->name);
    if (keyListFile) {
       char *md5Sum = md5HexForString(seq->dna);
       fprintf(keyListFile, "%s\t%s\n", md5Sum, index->name);
       freeMem(md5Sum);
    }
    freeDnaSeq(&seq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
keyList = optionVal("keyList", NULL);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
twoBitDup(argv[1]);
return 0;
}
