/* randString - generate a random string of specified length. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "options.h"

static char *charPool = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz";
static int poolSize = 0;

void usage()
/* Explain usage and exit. */
{
printf("sizeof(long int): %lu\n", (unsigned long)sizeof(long int));
printf("charPool: '%s'\n", charPool);
printf("poolSize: %d\n", poolSize);
errAbort(
  "randString - generate a random string of specified length\n"
  "usage:\n"
  "   randString <size> [count]\n"
  "options:\n"
  "   <size> - length of random string\n"
  "   [count] - optionally, this many strings"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

/* Characters to avoid in the 126 set of ASCII characters:
 * MySQL has special characters % and _ used for wild card searches
 * HTML URL references use $ & + , / : ; = ? and @
 * HTML code uses & < and >
 * the quote chars ' and " are used all over the place
 * space is inherently unsafe
 * marginally safe * . ( ) { } | \ ^ ~ [ ] `
 */

static void seedDrand()
/* seed the drand48 function with bytes from /dev/random */
{
long int seed = -1;
FILE *fd = mustOpen("/dev/random", "r");
fread((unsigned char *)&seed, sizeof(long int), 1, fd);
carefulClose(&fd);
printf("seed: 0x%0lx = %ld\n", seed, seed);
srand48(seed);
}

void randString(char *strLength)
/* randString - generate a random string of specified length. */
{
int N=sqlSigned(strLength);
int i;
for (i = 0; i < N; ++i)
    {
    int R = floor(poolSize * drand48());
    printf("%c", charPool[R]);
    }
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
poolSize = strlen(charPool);
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
seedDrand();
if ( argc > 2)
    {
    int count = sqlSigned(argv[2]);
    for ( ; count > 0; count--)
	randString(argv[1]);
    }
else
    randString(argv[1]);
return 0;
}
