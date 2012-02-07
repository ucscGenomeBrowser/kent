/* rle - byte oriented run length encoding. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "rle.h"


static int countSameAsStart(signed char *s, int max)
/* Count number of signed chars that are the same as first. */
{
signed char v = *s;
int i;
if (max > 127)
    max = 127;
for (i=1; i<max; ++i)
   if (s[i] != v)
       break;
return i;
}

int rleCompress(void *vIn, int inSize, signed char *out)
/* Compress in to out.  Out should be at least inSize * 1.5. 
 * Returns compressed size. */
{
signed char *in = vIn;
signed char *endIn = in + inSize;
signed char *s = in, *d = out;
signed char *uncStart = in;
int uncSize, sameCount;
int sizeLeft;

while ((sizeLeft = (endIn - s)) != 0)
    {
    sameCount = countSameAsStart(s, sizeLeft);
    uncSize = s - uncStart;
    if (sameCount >= 3)
        {
	int uncSize = s - uncStart;
	while (uncSize > 0)
	    {
	    int size = uncSize;
	    if (size > 127) size = 127;
	    *d++ = size;
	    memcpy(d, uncStart, size);
	    d += size;
	    uncSize -= size;
	    uncStart += size;
	    }
	*d++ = -sameCount;
	*d++ = *s;
	s += sameCount;
	uncStart = s;
	}
    else
        s += sameCount;
    }  
uncSize = s - uncStart;
while (uncSize > 0)
    {
    int size = uncSize;
    if (size > 127) size = 127;
    *d++ = size;
    memcpy(d, uncStart, size);
    d += size;
    uncSize -= size;
    uncStart += size;
    }
return d - out;
}

void rleUncompress(signed char *in, int inSize, void *vOut, int outSize)
/* Uncompress in to out. */
{
int count;
signed char *out = vOut;
signed char *endOut = out + outSize;
#ifndef NDEBUG
signed char *endIn = in + inSize;
#endif

while (out < endOut)
     {
     count = *in++;
     if (count > 0)
          {
	  memcpy(out, in, count);
	  in += count;
	  out += count;
	  }
    else
          {
	  count = -count;
	  memset(out, *in++, count);
	  out += count;
	  }
	  
    }
assert(out == endOut && in == endIn);
}

