/* textOut - set up stdout to be HTTP text, file or compressed file. */

#ifndef TEXTOUT_H
#define TEXTOUT_H

#include "pipeline.h"

/* Supported compression types: */
#define textOutCompressNone "none"
#define textOutCompressBzip2 "bzip2"
#define textOutCompressCompress "compress"
#define textOutCompressGzip "gzip"
#define textOutCompressZip "zip"

struct pipeline *textOutInit(char *fileName, char *compressType);
/* Set up stdout to be HTTP text, file (if fileName is specified), or 
 * compressed file (if both fileName and a supported compressType are 
 * specified). 
 * Return NULL if no compression, otherwise a pipeline handle on which 
 * the calling process should wait (after flushing and closing stdout). */

#endif /* TEXTOUT_H */
