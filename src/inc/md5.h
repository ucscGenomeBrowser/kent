#ifndef MD5_H
#define MD5_H

#define uint8  unsigned char
#define uint32 unsigned long int
#define uint64 unsigned long long int

struct md5_context
{
    uint64 total;
    uint32 state[4];
    uint8 buffer[64];
};

void md5_starts( struct md5_context *ctx );
void md5_update( struct md5_context *ctx, uint8 *input, uint32 length );
void md5_finish( struct md5_context *ctx, uint8 digest[16] );

#define MD5READBUFSIZE 256 * 1024

void md5ForFile(char * fileName, unsigned char md5[16]);
/* read f in buffer pieces and update md5 hash */

char *md5ToHex(unsigned char md5[16]);
/* return md5 as hex string */

char *md5HexForFile(char * fileName);
/* read f in buffer pieces and return hex string for md5sum */

char *md5HexForString(char *string);
/* Return hex string for md5sum of string. */

struct hash *md5FileHash(char *fileName);
/* Read md5sum file and return a hash keyed by file names with md5sum values. */

#endif /* MD5_H */


