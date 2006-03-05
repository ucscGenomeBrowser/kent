/* util - utility functions.  Handy stuff that's relatively
 * independent of other things. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "pfCompile.h"

void printEscapedString(FILE *f, char *s)
/* Print string in such a way that C can use it. */
{
char c;
fputc('"', f);
while ((c = *s++) != 0)
    {
    switch (c)
        {
	case '\n':
	    fputs("\\n", f);
	    break;
	case '\r':
	    fputs("\\r", f);
	    break;
	case '\\':
	    fputs("\\\\", f);
	    break;
	case '"':
	    fputs("\\\"", f);
	    break;
	default:
	    fputc(c, f);
	    break;
	}
    }
fputc('"', f);
}

char *replaceSuffix(char *path, char *oldSuffix, char *newSuffix)
/* Return a string that's a copy of path, but with old suffix
 * replaced by new suffix. */
{
int pathLen = strlen(path);
int oldSuffLen = strlen(oldSuffix);
int newSuffLen = strlen(newSuffix);
int headLen = pathLen - oldSuffLen;
int newLen = headLen + newSuffLen;
char *result = needMem(newLen+1);
memcpy(result, path, headLen);
memcpy(result+headLen, newSuffix, newSuffLen);
return result;
}

