/* Handle escaping for XML files.  Deal with things like
 * &amp; and &quot. */

#include "common.h"
#include "hash.h"
#include "xmlEscape.h"

static char const rcsid[] = "$Id: xmlEscape.c,v 1.2 2008/09/17 17:56:38 kent Exp $";

struct hash *xmlEscapeSymHash()
/* Return hash of predefined xml character symbols to lookup. */
{
struct hash *symHash = newHash(6);
hashAdd(symHash, "lt", "<");
hashAdd(symHash, "gt", ">");
hashAdd(symHash, "amp", "&");
hashAdd(symHash, "apos", "'");
hashAdd(symHash, "quot", "\"");
return symHash;
}

void xmlEscapeBytesToFile(unsigned char *buffer, int len, FILE *f)
/* Write buffer of given length to file, escaping as need be. */
{
unsigned char c;
int i;
for (i=0; i<len; ++i)
    {
    c = buffer[i];
    if (isalnum(c))
        fputc(c, f);
    else
        {
	switch (c)
	    {
	    case '&':
	        fputs("&amp;", f);
		break;
	    case '\'':
	        fputs("&apos;", f);
		break;
	    case '"':
	        fputs("&quot;", f);
		break;
	    case '<':
	        fputs("&lt;", f);
		break;
	    case '>':
	        fputs("&gt;", f);
		break;
	    case ' ':
	    case '-':
	    case '\t':
	    case '\n':
	    case ',':
	    case '.':
	    case ';':
	    case ':':
	    case '(':
	    case ')':
	    case '[':
	    case ']':
	    case '#':
	    case '/':
	        fputc(c, f);
		break;
	    default:
	        fprintf(f, "&#%d;", c);
		break;
	    }
	}
    }
}

void xmlEscapeStringToFile(char *s, FILE *f)
/* Write escaped zero-terminated string to file. */
{
int len = strlen(s);
xmlEscapeBytesToFile((unsigned char *)s, len, f);
}
