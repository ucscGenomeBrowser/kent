/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "md5.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file");
}

/*
 * those are the standard RFC 1321 test vectors
 */

static char *msg[] =
{
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012" \
	"345678901234567890"
};

static char *val[] =
{
    "d41d8cd98f00b204e9800998ecf8427e",
    "0cc175b9c0f1b6a831c399e269772661",
    "900150983cd24fb0d6963f7d28e17f72",
    "f96b697d7cb7938d525a2f31aaf161d0",
    "c3fcd3d76192e4007dfb496cca67e13b",
    "d174ab98d277d9f5a5611c2c9f419d9f",
    "57edf4a22be3c955ac49da2e2107b67a"
};

void freen( char *fileName )
{
char *buf;
size_t size;
int i, j;
char output[33];
struct md5_context ctx;
unsigned char md5sum[16];

readInGulp(fileName, &buf, &size);
md5_starts(&ctx);
md5_update(&ctx, (uint8 *)buf, size);
md5_finish(&ctx, md5sum);
for( j = 0; j < 16; j++ )
    printf( "%02x", md5sum[j] );
printf("\n");
}



int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
