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

#endif /* MD5_H */


