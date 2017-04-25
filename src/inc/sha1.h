/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

#ifndef SHA1_H
#define SHA1_H

typedef struct {
    bits32 state[5];
    bits32 count[2];
    bits8  buffer[64];
} SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void SHA1_Init(SHA1_CTX* context);
void SHA1_Update(SHA1_CTX* context, const bits8* data, const size_t len);
void SHA1_Final(SHA1_CTX* context, bits8 digest[SHA1_DIGEST_SIZE]);

/* ============== Added by UCSC Genome Browser ============= */

char *sha1ToHex(unsigned char hash[20]);
/* Convert binary representation of sha1 to hex string. Do a freeMem on result when done. */

void sha1ForFile(char *fileName, unsigned char hash[20]);
/* Make sha1 hash from file */

char *sha1HexForFile(char *fileName);
/* Return Sha1 as Hex string */

void sha1ForBuf(char *buffer, size_t bufSize, unsigned char hash[20]);
/* Return sha1 hash of buffer. */

char *sha1HexForBuf(char *buf, size_t bufSize);
/* Return Sha1 as Hex string */

char *sha1HexForString(char *string);
/* Return sha sum of zero-terminated string. */

#endif

