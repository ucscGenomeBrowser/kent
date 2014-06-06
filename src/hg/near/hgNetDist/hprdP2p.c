/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* hprdP2p - Create hprd.p2p tab file by reading HPRD FLAT_FILES.
 *   It is assumed that the HPRD flat files have been decompressed in
 *   a directory like /cluster/data/hg18/p2p/hprd/FLAT_FILES/ 
 *   The data has this structure:

[hgwdev:FLAT_FILES> head BINARY_PROTEIN_PROTEIN_INTERACTIONS.txt
SMAD4   02995   NP_005350.1     SMAD9   04484   BAA21128.1      yeast 2-hybrid  16189514,15231748

col 2 col 5 dist 1.0

[hgwdev:FLAT_FILES> head PROTEIN_COMPLEXES.txt
COM_1   00011   ASCL1   NP_004307.2     in vivo 8900141,8948587
COM_1   00918   TCF3    NP_003191.1     in vivo 8900141,8948587
COM_1   02809   MEF2C   NP_002388.2     in vivo 8900141,8948587

All pairs from col 2 for same col 1 with dist 1.5

 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hprdP2p - Create hprd.p2p tab file using HPRD flat file for input to hgNetDist\n"
  "usage:\n"
  "   hprdP2p hprdBinaryPPI hprdComplexes hprd.p2p\n"
  );

  /*
  "options:\n"
  "   -xxx=XXX\n"

        hprdP2p -verbose=2 \
                /cluster/data/hg18/p2p/hprd/FLAT_FILES/BINARY_PROTEIN_PROTEIN_INTERACTIONS.txt \
                /cluster/data/hg18/p2p/hprd/FLAT_FILES/PROTEIN_COMPLEXES.txt \
                hprd.p2p
  */
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void iterateComplex(char *ids[100], int max, FILE *f, char *complexId)
/* Iterate through all pairs, assign dist 1.5 
 * to indicate not necessarily a direct interaction
 * between pairs in the complex */
{
int i=0,j=0;
if (max==1)
    verbose(2,"weird complex:%s has max==1, only 1 member\n",complexId);
for(i=0;i<max;++i)
    for(j=i+1;j<max;++j)
	fprintf(f,"%s\t%s\t1.5\n",ids[i],ids[j]);
}

void hprdP2p(char *hprdBinaryPPI, char *hprdComplexes, char *outTab)
/* hprdP2p - Create hprd.p2p tab file from HPRD flat files for use with hgNetDist. */
{
FILE *f = mustOpen(outTab, "w");
char *row[8];
char *row2[6];
char *ids[100];
struct lineFile *lf = lineFileOpen(hprdBinaryPPI, TRUE);
while (lineFileRowTab(lf, row))
    {
    char *hprdId1 = row[1];
    char *hprdId2 = row[4];
    fprintf(f,"%s\t%s\t1.0\n",hprdId1,hprdId2);
    }
lineFileClose(&lf);

lf = lineFileOpen(hprdComplexes, TRUE);
char *lastComplex = "";
int i = 0;
while (lineFileRowTab(lf, row2))
    {
    char *complexId = row2[0];
    char *hprdId = row2[1];
    if (sameString(hprdId,"None"))
	continue;
    if (!sameString(complexId,lastComplex))
	{
    	iterateComplex(ids, i, f, lastComplex);
	i = 0;
	lastComplex = complexId;
	}
    ids[i++] = cloneString(hprdId);
    }
iterateComplex(ids, i, f, lastComplex);
lineFileClose(&lf);

carefulClose(&f);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hprdP2p(argv[1],argv[2],argv[3]);
return 0;
}
