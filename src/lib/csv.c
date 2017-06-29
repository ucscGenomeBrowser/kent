/* csv - stuff to help process comma separated values.  Have to wrap quotes around
 * things with commas, and escape quotes with more quotes sometimes. */

#include "common.h"
#include "linefile.h"
#include "csv.h"

void csvEscapeAndAppend(struct dyString *dy, char *string)
/* Append escaped string to dy.  Will insert comma if dy is non-empty */
{
if (dy->stringSize != 0)
    dyStringAppendC(dy, ',');
if (strchr(string, ',') == NULL)
    dyStringAppend(dy, string);
else
    {
    dyStringAppendC(dy, '"');
    char c;
    while ((c = *string++) != 0)
	{
	if (c == '"')
	    dyStringAppendC(dy, c);
	dyStringAppendC(dy, c);
	}
    dyStringAppendC(dy, '"');
    }
}

char *csvEscapeToDyString(struct dyString *dy, char *string)
/* Wrap string in quotes if it has any commas.  Put result into dy, and return it as a 
 * string.   Anything already in quotes get double-quoted */
{
/* If there are no commas just output it */
dyStringClear(dy);
csvEscapeAndAppend(dy, string);
return dy->string;
}

void csvWriteVal(char *val, FILE *f)
/* Write val, which may have some quotes or commas in it, in a way to be compatable with
 * csv list representation */
{
/* If there are no commas just output it */
if (strchr(val, ',') == NULL)
    {
    fputs(val, f);
    return;
    }

/* Strip surrounding quotes if any */
val = trimSpaces(val);
int valLen = strlen(val);
if (valLen > 2 && val[0] == '"' && lastChar(val) == '"')
     {
     val[valLen-1] = 0;
     val += 1;
     }

/* Put quotes around it and output, escaping internal quotes with double quotes */
fputc('"', f);
char c;
while ((c = *val++) != 0)
    {
    if (c == '"')
	fputc('"', f);
    fputc(c, f);
    }
fputc('"', f);
}

char *csvParseNext(char **pos, struct dyString *scratch)
/* Return next value starting at pos, putting results into scratch and
 * returning scratch->string or NULL if no values left. Will update *pos
 * to after trailing comma if any. This will tolerate and ignore leading
 * and trailing white space.  
 *     Since an empty or all-white string will return NULL, if you
 * want empty strings to be a legitimate value then they have to be quoted
 * or followed by a comma. */
{
// Return NULL at end of string
char *s = skipLeadingSpaces(*pos);
if (isEmpty(s))
    return NULL;

// Clear scratch pad and get first character
dyStringClear(scratch);
char c = *s;

// If we start with a quote then fall into logic that goes to next quote,
// treating internal "" as a single " so that can have internal quotes
if (c == '"')
    {
    for (;;)
        {
	c = *(++s);
	if (c == 0)
	    errAbort("Isolated quote in csvParseNext %s", *pos);
	if (c == '"')  
	    {
	    ++s;
	    if (*s == c)  // Next char also a quote we convert the two quotes to one
		{
		dyStringAppendC(scratch, c);
		}
	    else
	        {
		// End of string.  Skip over white space until next comma
		s = skipLeadingSpaces(s);
		c = *s;
		if (c == ',')
		    {
		    ++s;  // skip over trailing comma.
		    break;
		    }
		if (c == 0)
		    break;
		else
		    errAbort("Unexpected text after quotes in csvParseNext %s", *pos);
		}
	    }
	else
	    dyStringAppendC(scratch, c);
	}
    }
else	// Did not start with a quote,  so we just copy until comma or end of string.
    {
    char lastC = 0;
    for (;;)
       {
       if (c == 0)
           break;
       if (c == ',')
           {
	   ++s;  // skip over trailing comma.
	   break;
	   }
	dyStringAppendC(scratch, c);
	lastC = c;
	c = *(++s);
	}
    if (isspace(lastC))
	{
        int erased = eraseTrailingSpaces(scratch->string);
	scratch->stringSize -= erased;
	}
    }

// Update position to start reading next one from and return scratchpad
*pos = s;
return scratch->string;
}

boolean csvNeedsParsing(char *s)
/* Return TRUE if s is something that needs parsing through the csv parser.  That
 * is it either starts with a quote or has a comma */
{
if (strchr(s, ','))
    return TRUE;
return *s == '"';
}

struct slName *csvParse(char *csv)
/* Return a list of parsed out csv values.  Do a slFreeList of this when done */
{
struct dyString *scratch = dyStringNew(0);
struct slName *list = NULL;
char *val;
while ((val = csvParseNext(&csv, scratch)) != NULL)
    slNameAddHead(&list, val);
dyStringFree(&scratch);
slReverse(&list);
return list;
}

