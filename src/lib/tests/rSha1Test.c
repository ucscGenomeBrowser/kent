#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "errAbort.h"
#include "obscure.h"
#include "portable.h"
#include "rSha1.h"

static char *test_data[] = {
    "abc",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "A million repetitions of 'a'"};
static char *test_results[] = {
    "A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D",
    "84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1",
    "34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F"};

/* ==== UCSC Genome Browser tests ========== */

void manual_way(char *filename, unsigned char hash[20])
/* generate sha1 manually the hard way. */
{
FILE *fp = fopen (filename, "r");
if (!fp) 
    {
    printf("missing file %s\n", filename);
    exit(1);
    }

#define BS 4096 /* match coreutils */

RSHA1_CTX ctx;
RSHA1_Init(&ctx);
size_t nr;
unsigned char buf[BS];
while ((nr=fread(buf, 1, sizeof(buf), fp)))
    RSHA1_Update(&ctx, buf, nr);

RSHA1_Final(&ctx, hash);
}

char *string_way(char *filename)
/* generate sha1 via lib on single long string. */
{
char *buf = NULL;
size_t bufSize;
readInGulp(filename, &buf, &bufSize);
return rSha1HexForBuf(buf, bufSize);
}

char *git_way(char *filename)
/* generate sha1 using git blob prefix.
 * Note that if you run this from the commandline, you get the git result
 * which of course includes its own blob prefix automatically:
     git hash-object some-file-name
 */
{
char *buf = NULL;
size_t bufSize;
readInGulp(filename, &buf, &bufSize);
char prefix[1024];
safef(prefix, sizeof prefix, "blob %llu", (unsigned long long)bufSize);
int pfxLen = strlen(prefix)+1;
size_t bufSize2 = pfxLen+bufSize;
char *buf2 = needLargeMem(bufSize2);
memmove(buf2,prefix,strlen(prefix)+1);
memmove(buf2+strlen(prefix)+1,buf,bufSize);
return rSha1HexForBuf(buf2, bufSize2);
}

char *git_way2(char *filename)
/* generate sha1 manually with git blob prefix the hard way. */
{
FILE *fp = fopen (filename, "r");
if (!fp) errAbort("missing file %s", filename);

#define BS 4096 /* match coreutils */
unsigned char hash[20];

off_t fs = fileSize(filename);
char prefix[1024];
safef(prefix, sizeof prefix, "blob %llu", (unsigned long long)fs);

RSHA1_CTX ctx;
RSHA1_Init(&ctx);

RSHA1_Update(&ctx, (unsigned char *)prefix, strlen(prefix)+1);

size_t nr;
unsigned char buf[BS];
while ((nr=fread(buf, 1, sizeof(buf), fp)))
    RSHA1_Update(&ctx, buf, nr);

RSHA1_Final(&ctx, hash);
return rSha1ToHex(hash);
}

void git_way3(char *filename)
/* generate sha1 manually the hard way. */
{
char cmd[1024];
safef(cmd, sizeof cmd, "git hash-object %s", filename);
printf("executing system command [%s]\n", cmd);
system(cmd);
}

char *string_empty()
/* generate sha1 via lib on empty input. */
{
char *buf = NULL;
size_t bufSize = 0;
return rSha1HexForBuf(buf, bufSize);
}


void digest_to_hex(const bits8 digest[RSHA1_DIGEST_SIZE], char *output)
{
    int i,j;
    char *c = output;

    for (i = 0; i < RSHA1_DIGEST_SIZE/4; i++) {
        for (j = 0; j < 4; j++) {
            sprintf(c,"%02X", digest[i*4+j]);
            c += 2;
        }
        sprintf(c, " ");
        c += 1;
    }
    *(c - 1) = '\0';
}

int main(int argc, char** argv)
{
    int k;
    RSHA1_CTX context;
    bits8 digest[20];
    char output[80];

    fprintf(stdout, "verifying SHA-1 implementation... ");

    for (k = 0; k < 2; k++){
        RSHA1_Init(&context);
        RSHA1_Update(&context, (bits8*)test_data[k], strlen(test_data[k]));
        RSHA1_Final(&context, digest);
	digest_to_hex(digest, output);

        if (strcmp(output, test_results[k])) {
            fprintf(stdout, "FAIL\n");
            fprintf(stderr,"* hash of \"%s\" incorrect:\n", test_data[k]);
            fprintf(stderr,"\t%s returned\n", output);
            fprintf(stderr,"\t%s is correct\n", test_results[k]);
            return (1);
        }
    }
    /* million 'a' vector we feed separately */
    RSHA1_Init(&context);
    for (k = 0; k < 1000000; k++)
        RSHA1_Update(&context, (bits8*)"a", 1);
    RSHA1_Final(&context, digest);
    digest_to_hex(digest, output);
    if (strcmp(output, test_results[2])) {
        fprintf(stdout, "FAIL\n");
        fprintf(stderr,"* hash of \"%s\" incorrect:\n", test_data[2]);
        fprintf(stderr,"\t%s returned\n", output);
        fprintf(stderr,"\t%s is correct\n", test_results[2]);
        return (1);
    }

    /* success */
    fprintf(stdout, "ok\n");


    if (argc < 2)
	{
	printf("must specify filename on commandline.\n");
	exit(1);
	}

    char *filename = argv[1];

    printf("Library filename way:\n");
    long thisTime = 0, lastTime = 0;
    lastTime = clock1();
    char *hex = rSha1HexForFile(filename);
    thisTime = clock1();
    printf("%s  %s elapsed time in seconds: %ld\n\n", hex, filename, (thisTime - lastTime));

    printf("Manual filename way:\n");
    unsigned char hash[20];
    manual_way(filename, hash);   
    int i;
    for (i=0; i<sizeof(hash); i++)
        printf("%02x",*(hash+i));
    printf("  %s\n\n", filename);

    printf("Library string way:\n");
    hex = string_way(filename);
    printf("%s  %s\n\n", hex, filename);

    printf("GIT way:\n");
    hex = git_way(filename);
    printf("%s with git blob prefix  %s\n\n", hex, filename);

    printf("GIT way2:\n");
    hex = git_way2(filename);
    printf("%s with git way2 blob prefix  %s\n\n", hex, filename);

    git_way3(filename);
    printf("\n");

    printf("Library string empty:\n");
    hex = string_empty();
    printf("%s  %s\n\n", hex, "empty-string");


    return(0);
}

