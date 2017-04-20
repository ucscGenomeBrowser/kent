#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "obscure.h"
#include "gitSha1.h"

void manual_way(char *filename, unsigned char hash[20])
/* generate sha1 manually the hard way. */
{
FILE *fp = fopen (filename, "r");
if (!fp) errAbort("missing file %s", filename);

#define BS 4096 /* match coreutils */

blk_SHA_CTX ctx;
blk_SHA1_Init(&ctx);
size_t nr;
char buf[BS];
while ((nr=fread_unlocked(buf, 1, sizeof(buf), fp)))
    blk_SHA1_Update(&ctx, buf, nr);

blk_SHA1_Final(hash, &ctx);
}

char *string_way(char *filename)
/* generate sha1 via lib on single long string. */
{
char *buf = NULL;
size_t bufSize;
readInGulp(filename, &buf, &bufSize);
return sha1HexForBuf(buf, bufSize);
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
return sha1HexForBuf(buf2, bufSize2);
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

blk_SHA_CTX ctx;
blk_SHA1_Init(&ctx);

blk_SHA1_Update(&ctx, prefix, strlen(prefix)+1);

size_t nr;
char buf[BS];
while ((nr=fread_unlocked(buf, 1, sizeof(buf), fp)))
    blk_SHA1_Update(&ctx, buf, nr);

blk_SHA1_Final(hash, &ctx);
return sha1ToHex(hash);
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
return sha1HexForBuf(buf, bufSize);
}

int main(int argc, char** argv)
{
    if (argc != 2) 
	errAbort("must specfy file to use for sha1 has on commandline.");
    char* filename = argv[1];

    unsigned char hash[20];
    manual_way(filename, hash);   

    printf("Manual filename way:\n");
    int i;
    for (i=0; i<sizeof(hash); i++)
        printf("%02x",*(hash+i));
    printf("  %s\n\n", filename);

    printf("Library filename way:\n");
    char *hex = sha1HexForFile(filename);
    printf("%s  %s\n\n", hex, filename);

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

    return 0;
}


