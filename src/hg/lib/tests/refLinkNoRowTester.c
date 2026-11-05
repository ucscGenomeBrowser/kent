/* refLinkNoRowTester - a refLink made for an accession with no row has no stray fields.
 *
 * doKnownGene in hgc builds a refLink on the stack when the accession has no refLink row, which
 * is every knownGene on hg38, hg19 and mm39.  Before #38317 it filled in three of the eight
 * fields and left the rest as whatever was on the stack.  Two of them were read: the OMIM link
 * came and went between requests to the same URL, and protAcc passed as a string segfaulted hgc
 * run from the command line.  refLinkInitNoRow now zeroes the struct first.
 *
 * Uninitialised memory is only sometimes non-zero, so the struct here is filled with a
 * non-zero byte first, which makes a missed field show every time.
 *
 * refs #38317 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "refLink.h"

static char *strField(char *s)
/* Print a string field, or that it is absent. */
{
return (s == NULL) ? "(absent)" : s;
}

int main(int argc, char *argv[])
{
struct refLink rl;
memset(&rl, 0xA5, sizeof rl);
refLinkInitNoRow(&rl, "ENST00000456328.2");
printf("a refLink for ENST00000456328.2, which has no refLink row\n");
printf("  name         %s\n", strField(rl.name));
printf("  mrnaAcc      %s\n", strField(rl.mrnaAcc));
printf("  product      %s\n", (rl.product == NULL) ? "(absent)" : "set -- FAIL");
printf("  protAcc      %s\n", (rl.protAcc == NULL) ? "(absent)" : "set -- FAIL");
printf("  geneId       %u\n", rl.geneId);
printf("  prodId       %u\n", rl.prodId);
printf("  locusLinkId  %u\n", rl.locusLinkId);
printf("  omimId       %u\n", rl.omimId);
printf("  next         %s\n", (rl.next == NULL) ? "(absent)" : "set -- FAIL");
return 0;
}
