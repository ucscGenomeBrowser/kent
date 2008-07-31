/* whyConserved - Try and analyse why a particular thing is conserved. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "jksql.h"
#include "bits.h"
#include "featureBits.h"

static char const rcsid[] = "$Id: whyConserved.c,v 1.3.338.1 2008/07/31 02:24:43 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "whyConserved - Try and analyse why a particular thing is conserved\n"
  "usage:\n"
  "   whyConserved database chromosome homologyTrack\n"
  "Use 'all' in the chromosome to cover the whole genome\n"
  );
}

void explainSome(char *database, Bits *homo, Bits *once, Bits *bits, char *chrom, int chromSize, 
	struct sqlConnection *conn, char *trackSpec, char *homologyTrack)
/* Explain some of homology. */
{
int trackSize = 0, homoSize = 0, andSize = 0, cumSize = 0, newSize = 0;

homoSize = bitCountRange(homo, 0, chromSize);
bitClear(bits, chromSize);
if (trackSpec != NULL)
    {
    fbOrTableBits(database, bits, trackSpec, chrom, chromSize, conn);
    trackSize = bitCountRange(bits, 0, chromSize);
    bitAnd(bits, homo, chromSize);
    andSize = bitCountRange(bits, 0, chromSize);
    bitAnd(bits, once, chromSize);
    newSize = bitCountRange(bits, 0, chromSize);
    bitNot(bits, chromSize);
    bitAnd(once, bits, chromSize);
    cumSize = homoSize - bitCountRange(once, 0, chromSize);
    }
else
    {
    trackSpec = homologyTrack;
    trackSize = andSize = homoSize;
    cumSize = newSize = 0;
    }

printf("%-21s %8d %8d %5.2f%% %6.2f%% %6.2f%% %5.2f%% %5.2f%%\n",
	trackSpec, trackSize, andSize, 
	100.0*trackSize/chromSize,
	100.0*andSize/trackSize,
	100.0*andSize/homoSize,
	100.0*newSize/homoSize,
	100.0*cumSize/homoSize);
}

#ifdef OLD
void whyConserved(char *database, char *chrom, char *homologyTrack)
/* whyConserved - Try and analyse why a particular thing is conserved. */
{
struct sqlConnection *conn;
int chromSize;
Bits *homo = NULL;
Bits *bits = NULL;
Bits *once = NULL;

hSetDb(database);
conn = hAllocConn();
chromSize = hChromSize(chrom);

homo = bitAlloc(chromSize);
bits = bitAlloc(chromSize);
once = bitAlloc(chromSize);

/* Get homology bitmap and set once mask to be the same. */
fbOrTableBits(homo, homologyTrack, chrom, chromSize, conn);
bitOr(once, homo, chromSize);

/* printHeader */
printf("%-21s %8s %8s %5s %6s  %6s %5s  %5s \n",
	"Track Specification", "track", "overlap", 
	"track", "mus",  "track", "new",  "cum");
printf("%-21s %8s %8s %5s %6s  %6s %5s  %5s \n",
	"", "size", "size", 
	"geno", "track", "mus",  "mus",  "mus");
printf("-----------------------------------------------------------------------------\n");

/* Whittle awway at homology... */
explainSome(homo, once, bits, chrom, chromSize, conn, NULL, homologyTrack);
explainSome(homo, once, bits, chrom, chromSize, conn, "simpleRepeat", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "rmsk", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "sanger22:CDS:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "refGene:CDS:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "sanger22:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "refGene:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "ensGene:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "rnaGene", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "mrna:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "intronEst:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "xenoMrna:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "xenoEst:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "genscan:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "genscanSubopt", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "psu:exon:10", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "sanger22:upstream:200", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "refGene:upstream:200", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "mrna:upstream:200", NULL);
explainSome(homo, once, bits, chrom, chromSize, conn, "est", NULL);
hFreeConn(&conn);
}
#endif /* OLD */

void whyConserved(char *database, char *chrom, char *homologyTrack)
/* whyConserved - Try and analyse why a particular thing is conserved. */
{
struct sqlConnection *conn;
int chromSize;
Bits *homo = NULL;
Bits *bits = NULL;
Bits *once = NULL;

conn = hAllocConn(database);
chromSize = hChromSize(database, chrom);

homo = bitAlloc(chromSize);
bits = bitAlloc(chromSize);
once = bitAlloc(chromSize);

/* Get homology bitmap and set once mask to be the same. */
fbOrTableBits(database, homo, homologyTrack, chrom, chromSize, conn);
bitOr(once, homo, chromSize);

/* printHeader */
printf("%-21s %8s %8s %5s %6s  %6s %5s  %5s \n",
	"Track Specification", "track", "overlap", 
	"track", "mus",  "track", "new",  "cum");
printf("%-21s %8s %8s %5s %6s  %6s %5s  %5s \n",
	"", "size", "size", 
	"geno", "track", "mus",  "mus",  "mus");
printf("-----------------------------------------------------------------------------\n");

/* Whittle awway at homology... */
explainSome(database, homo, once, bits, chrom, chromSize, conn, NULL, homologyTrack);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "sanger22:CDS", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "refGene:CDS", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "est", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "intronEst:exon", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "blatMouse:exon", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "xenoMrna:exon", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "xenoEst:exon", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "genscan:exon", NULL);
explainSome(database, homo, once, bits, chrom, chromSize, conn, "exoFish", NULL);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
whyConserved(argv[1], argv[2], argv[3]);
return 0;
}
