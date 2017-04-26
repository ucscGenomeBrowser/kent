/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

/* putting prefix R (for Reid) in front of things to not collide with openssl version */

#ifndef RSHA1_H
#define RSHA1_H

typedef struct {
    bits32 state[5];
    bits32 count[2];
    bits8  buffer[64];
} RSHA1_CTX;

#define RSHA1_DIGEST_SIZE 20

void RSHA1_Init(RSHA1_CTX* context);
void RSHA1_Update(RSHA1_CTX* context, const bits8* data, const size_t len);
void RSHA1_Final(RSHA1_CTX* context, bits8 digest[RSHA1_DIGEST_SIZE]);

/* ============== Added by UCSC Genome Browser ============= */

char *rSha1ToHex(unsigned char hash[20]);
/* Convert binary representation of sha1 to hex string. Do a freeMem on result when done. */

void rSha1ForFile(char *fileName, unsigned char hash[20]);
/* Make sha1 hash from file */

char *rSha1HexForFile(char *fileName);
/* Return Sha1 as Hex string */

void rSha1ForBuf(char *buffer, size_t bufSize, unsigned char hash[20]);
/* Return sha1 hash of buffer. */

char *rSha1HexForBuf(char *buf, size_t bufSize);
/* Return Sha1 as Hex string */

char *rSha1HexForString(char *string);
/* Return sha sum of zero-terminated string. */

#endif

