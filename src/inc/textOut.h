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

/* Menu and values for passing to cgiMakeDropListFull: */
/* (not declaring the static char *[]'s here because those can lead to 
 * "variable defined but not used" warnings -- declare locally.) */
#define textOutCompressMenuContents { "plain text",\
				       "gzip compressed (.gz)",\
				       "bzip2 compressed (.bz2)",\
				       "LZW compressed (.Z)",\
				       "zip compressed (.zip)",\
				       NULL\
				     }

#define textOutCompressValuesContents { textOutCompressNone,\
					 textOutCompressGzip,\
					 textOutCompressBzip2,\
					 textOutCompressCompress,\
					 textOutCompressZip,\
					 NULL\
				       }


struct pipeline *textOutInit(char *fileName, char *compressType);
/* Set up stdout to be HTTP text, file (if fileName is specified), or 
 * compressed file (if both fileName and a supported compressType are 
 * specified). 
 * Return NULL if no compression, otherwise a pipeline handle on which 
 * textOutClose should be called when we're done writing stdout. */

void textOutClose(struct pipeline **pCompressPipeline);
/* Flush and close stdout, wait for the pipeline to finish, and then free 
 * the pipeline object. */

#endif /* TEXTOUT_H */
