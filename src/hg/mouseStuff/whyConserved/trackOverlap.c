/* trackOverlap - Correlate a track with a series of tracks specified in specFile. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "jksql.h"
#include "bits.h"
#include "featureBits.h"

static char const rcsid[] = "$Id: trackOverlap.c,v 1.2 2008/09/03 19:20:40 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackOverlap- Overlap how much of a track is overlapped by\n"
  "   other tracks and vice versa. This is done by correlating\n"
  "   series of bitmap projections (i.e. featureBits multiple times).\n"
  "usage:\n"
  "   trackOverlap database chromosome track listFile\n"
  "\n"
  "Where listFile is a file with one featureBits specification per\n"
  "line. For example: 'intronEst:exon:10', see featureBits for\n"
  "more examples of syntax.\n"
//  "Use 'all' in the chromosome to cover the whole genome\n"
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

void trackOverlap(char *database, char *chrom, char *homologyTrack, char *specFile)
/* trackOverlap - Correlate a track with a series of tracks specified in specFile. */
{
struct lineFile *lf = NULL;
char *line = NULL;
struct sqlConnection *conn;
int chromSize;
Bits *homo = NULL;
Bits *bits = NULL;
Bits *once = NULL;

lf = lineFileOpen(specFile, TRUE);
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
	"track", "cov",  "track", "new",  "cum");
printf("%-21s %8s %8s %5s %6s  %6s %5s  %5s \n",
	"", "size", "size", 
	"geno", "track", "cov",  "cov",  "cov");
printf("-----------------------------------------------------------------------------\n");

/* Whittle awway at homology... */
explainSome(database, homo, once, bits, chrom, chromSize, conn, NULL, homologyTrack);
while(lineFileNextReal(lf, &line))
    {
    explainSome(database, homo, once, bits, chrom, chromSize, conn, line, NULL);
    }
lineFileClose(&lf);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
trackOverlap(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
