#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "quotedP.h"


char *quotedPrintableEncode(char *input)
/* Use Quoted-Printable standard to encode a string. */
{
struct dyString *dy = dyStringNew(0);
size_t i=0,l=strlen(input);
int width = 0;
for (i=0; i < l; ++i)
    {
    char c = input[i];
    switch (c)
	{
	case '=':
	case '\t':
	case '\r':
	case '\n':
	case ' ':
	    dyStringAppendC(dy, '=');
	    dyStringPrintf(dy, "%2x", c);
	    width += 3;
	    break;
	default:
	    dyStringAppendC(dy, c);
	    ++width;
	}
    if (width > 72)
	{
	dyStringAppendC(dy, '=');
	dyStringAppendC(dy, '\n');
	width = 0;
	}
	
    }
/* add terminator to prevent extra newline */
if (lastChar(dy->string) != '=')  
    dyStringAppendC(dy, '=');
    
return dyStringCannibalize(&dy);
}

boolean quotedPCollapse(char *line)
/* Use Quoted-Printable standard to decode a string.
 * Return true if the line does not end in '='
 * which indicate continuation. */
{
size_t i=0,j=0,l=strlen(line);
boolean result = lastChar(line) != '=';
char c1 = ' ', c2 = ' ';
while(i < l)
    {
    if (line[i] == '=')
	{
	if (i > (l-3)) 
	    break;     /* not enough room left for whole char */
	++i;	    
	c1 = line[i++];
	c2 = line[i++];
	c1 = toupper(c1);
	c2 = toupper(c2);
	if (isdigit(c1))
	    c1 -= 48;
	else
	    c1 -= 55;
	if (isdigit(c2))
	    c2 -= 48;
	else
	    c2 -= 55;
	line[j++] = (c1 * 16) + c2;
	}
    else
	{
	line[j++] = line[i++];
	}
    }
line[j] = 0; /* terminate line */
return result;
}

char *quotedPrintableDecode(char *input)
/* Use Quoted-Printable standard to decode a string.  Return decoded
 * string which will be freeMem'd.  */
{
size_t inplen = strlen(input);
char *result = (char *)needMem(inplen+1);
size_t j=0;
char *line = NULL;
int size = 0;
int i = 0;
boolean newLine = FALSE;

struct lineFile *lf = lineFileOnString("", TRUE, cloneString(input));

while (lineFileNext(lf, &line, &size))
    {
    newLine = quotedPCollapse(line);
    size = strlen(line); 
    for (i = 0; i < size; )
	result[j++] = line[i++];
    if (newLine)
	result[j++] = '\n';
    }

lineFileClose(&lf);  /* frees cloned string */

result[j] = 0;  /* terminate text string */
     
return result;
}

