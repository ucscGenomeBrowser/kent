/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <netdb.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file/dir");
}

double numPieces = 32080159/100000;
double totalSize = 947469933/100000;

/* 30 per kb.
 */

void freen(char *host)
/* Test some hair-brained thing. */
{
int pieceSize;

#ifdef SOON 
for (pieceSize = 1; pieceSize <= 1000; ++pieceSize)
     {
     double x = pieceSize/1000.0;
     double logX = log(x);
     double nLogX = logX * (30-1);
     double enLogX = exp(nLogX);
     double y = 30 * enLogX;
     printf("%d %e %e %e %e %e\n", pieceSize, x, logX, nLogX, enLogX, y);
     }
#endif /* SOON  */
printf("exp(1) = %f, log(2.7) = %f, exp(log(1)) = %f\n", exp(1), log(2.7), exp(log(1)));

for (pieceSize = 1; pieceSize <= 1000; ++pieceSize)
     {
     double x = pieceSize/1000.0;
     printf("%d\t%f\n", pieceSize, 30 * pow(x, 29));
     }
}
// 30 3.000000e-02 -3.506558e+00 -1.016902e+02 6.863038e-45 2.058911e-43


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
