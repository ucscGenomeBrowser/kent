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
	case '\a':
	    fputs("\\a", f);
	    break;
	case '\b':
	    fputs("\\b", f);
	    break;
	case '\f':
	    fputs("\\f", f);
	    break;
	case '\n':
	    fputs("\\n", f);
	    break;
	case '\r':
	    fputs("\\r", f);
	    break;
	case '\t':
	    fputs("\\t", f);
	    break;
	case '\v':
	    fputs("\\t", f);
	    break;
	case '\?':
	    fputs("\\?", f);
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

