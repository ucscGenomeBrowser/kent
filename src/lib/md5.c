/* md5 calculating functions and the like.  Just wrappers for md5sum program */

#include "common.h"
#include "hex.h"
#include "linefile.h"
#include "hash.h"
#include "pipeline.h"
#include "md5.h"

char *md5ToHex(unsigned char md5[16])
/* Convert binary representation of md5 to hex string. Do a freeMem on result when done. */
{
char hex[33];
char *h;
int i;
for (i = 0, h=hex; i < 16; ++i, h += 2)
    byteToHex( md5[i], h);  
hex[32] = 0;
return cloneString(hex);
}

void md5HexToMd5(char hexIn[32], unsigned char md5Out[16])
/* Convert hexadecimal representation of md5 back to binary */
{
char *pt = hexIn;
int i;
for (i=0; i<16; ++i)
     {
     md5Out[i] = hexToByte(pt);
     pt += 2;
     }
}

char *md5HexForFile(char *fileName)
/* Calculate md5 on file and return in hex format.  Use freeMem on result when done. */
{
/* Calculate md5 using pipeline to unix utility. */
char *cmd[] = {"md5sum", NULL};
struct pipeline *pl = pipelineOpen1(cmd, pipelineRead, fileName, NULL);
FILE *f = pipelineFile(pl);
char hex[33];
mustRead(f, hex, 32);
hex[32] = 0;
pipelineClose(&pl);
return cloneString(hex);
}

char *md5HexForBuf(char *buf, size_t bufSize)
/* Return md5 sum of buffer. Use freeMem on result when done. */
{
/* Calculate md5 using pipeline to unix utility. */
char *cmd[] = {"md5sum", NULL};
struct pipeline *pl = pipelineOpenMem1(cmd, pipelineRead, buf, bufSize, fileno(stderr));
FILE *f = pipelineFile(pl);
char hex[33];
mustRead(f, hex, 32);
hex[32] = 0;
pipelineClose(&pl);
return cloneString(hex);
}

char *md5HexForString(char *string)
/* Return md5 sum of zero-terminated string. Use freeMem on result when done. */
{
return md5HexForBuf(string, strlen(string));
}

void md5ForFile(char *fileName, unsigned char md5[16])
/* Return MD5 sum for file in md5 in binary rather than hex format. */
{
char *hex = md5HexForFile(fileName);
md5HexToMd5(hex, md5);
freeMem(hex);
}

struct hash *md5FileHash(char *fileName)
/* Read md5sum file and return a hash keyed by file names with md5sum values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(0);
while (lineFileRow(lf, row))
    hashAdd(hash, row[1], cloneString(row[0]));
lineFileClose(&lf);
return hash;
}
