/* rle - byte oriented run length encoding. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef RLE_H
#define RLE_H

int rleCompress(void *in, int inSize, signed char *out);
/* Compress in to out.  Out should be at least inSize * 1.5. 
 * Returns compressed size. */

void rleUncompress(signed char *in, int inSize, void *out, int outSize);
/* Uncompress in to out. */

#endif /* RLE_H */
