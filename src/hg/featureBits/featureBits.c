/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "bits.h"
#include "featureBits.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections. \n"
  "usage:\n"
  "   featureBits database chromosome table(s)\n"
  "This will return the number of bits in all the tables anded together"
  );
}

void featureBits(char *database, char *chrom, int tableCount, char *tables[])
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
struct sqlConnection *conn;
int chromSize, i, setSize;
Bits *acc = NULL;
Bits *bits = NULL;

hSetDb(database);
conn = hAllocConn();
chromSize = hChromSize(chrom);
acc = bitAlloc(chromSize);
bits = bitAlloc(chromSize);

fbOrTableBits(acc, tables[0], chrom, chromSize, conn);
for (i=1; i<tableCount; ++i)
    {
    bitClear(bits, chromSize);
    fbOrTableBits(bits, tables[i], chrom, chromSize, conn);
    bitAnd(acc, bits, chromSize);
    }
setSize = bitCountRange(acc, 0, chromSize);
printf("%d bases of %d (%4.2f%%) in intersection\n", setSize, chromSize, 100.0*setSize/chromSize);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
featureBits(argv[1], argv[2], argc-3, argv+3);
return 0;
}
