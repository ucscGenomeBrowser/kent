/* SHA-1 in C
By Steve Reid <sreid@sea-to-sky.net>
100% Public Domain
http://svn.ghostscript.com/jbig2dec/trunk/sha1.c
*/
#define RSHA1HANDSOFF    /* Copies data before messing with it. */

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "hex.h"
#include "errAbort.h"
#include "rSha1.h"

void RSHA1_Transform(bits32 state[5], const bits8 buffer[64]);

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
/* FIXME: can we do this in an endian-proof way? */
#ifdef WORDS_BIGENDIAN
#define blk0(i) block->l[i]
#else
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);


/* Hash a single 512-bit block. This is the core of the algorithm. */
void RSHA1_Transform(bits32 state[5], const bits8 buffer[64])
{
    bits32 a, b, c, d, e;
    typedef union {
        bits8 c[64];
        bits32 l[16];
    } CHAR64LONG16;
    CHAR64LONG16* block;

#ifdef RSHA1HANDSOFF
    static bits8 workspace[64];
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif

    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;

    /* Wipe variables */
    a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */
void RSHA1_Init(RSHA1_CTX* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */
void RSHA1_Update(RSHA1_CTX* context, const bits8* data, const size_t len)
{
    size_t i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        RSHA1_Transform(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            RSHA1_Transform(context->state, data + i);
        }
        j = 0;
    }
    else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);

}


/* Add padding and return the message digest. */
void RSHA1_Final(RSHA1_CTX* context, bits8 digest[RSHA1_DIGEST_SIZE])
{
    bits32 i;
    bits8  finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    RSHA1_Update(context, (bits8 *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        RSHA1_Update(context, (bits8 *)"\0", 1);
    }
    RSHA1_Update(context, finalcount, 8);  /* Should cause a RSHA1_Transform() */
    for (i = 0; i < RSHA1_DIGEST_SIZE; i++) {
        digest[i] = (bits8)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }

    /* Wipe variables */
    i = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(finalcount, 0, 8);	/* SWR */

#ifdef RSHA1HANDSOFF  /* make RSHA1Transform overwrite its own static vars */
    RSHA1_Transform(context->state, context->buffer);
#endif
}

/* ==================== Routines added by UCSC genome browser ========================= */


char *rSha1ToHex(unsigned char hash[20])
/* Convert binary representation of sha1 to hex string. Do a freeMem on result when done. */
{
int hexSize = 20*2;
char hex[hexSize+1];
char *h;
int i;
for (i = 0, h=hex; i < 20; ++i, h += 2)
    byteToHex( hash[i], h);  
hex[hexSize] = 0;
return cloneString(hex);
}


void rSha1ForFile(char *fileName, unsigned char hash[20])
/* Make sha1 hash from file */
{
FILE *fp = fopen (fileName, "r");
if (!fp) errAbort("missing file %s", fileName);
#define BS 4096 /* match coreutils */
RSHA1_CTX ctx;
RSHA1_Init(&ctx);
size_t nr;
unsigned char buf[BS];
while ((nr=fread(buf, 1, sizeof(buf), fp)))
    { 
    RSHA1_Update(&ctx, buf, nr);
    };
RSHA1_Final(&ctx, hash);
}

char *rSha1HexForFile(char *fileName)
/* Return Sha1 as Hex string */
{
unsigned char hash[20];
rSha1ForFile(fileName, hash);
return rSha1ToHex(hash);
}

void rSha1ForBuf(char *buffer, size_t bufSize, unsigned char hash[20])
/* Return sha1 hash of buffer. */
{
#define BS 4096 /* match coreutils */
RSHA1_CTX ctx;
RSHA1_Init(&ctx);
size_t remaining = bufSize;
while (remaining > 0)
    {
    int bufSize = BS;
    if (bufSize > remaining)
	bufSize = remaining;
    RSHA1_Update(&ctx, (unsigned char*)buffer, bufSize);
    buffer += bufSize;
    remaining -= bufSize;
    }
RSHA1_Final(&ctx, hash);
}

char *rSha1HexForBuf(char *buf, size_t bufSize)
/* Return Sha1 as Hex string */
{
unsigned char hash[20];
rSha1ForBuf(buf, bufSize, hash);
return rSha1ToHex(hash);
}

char *rSha1HexForString(char *string)
/* Return sha sum of zero-terminated string. */
{
return rSha1HexForBuf(string, strlen(string));
}

